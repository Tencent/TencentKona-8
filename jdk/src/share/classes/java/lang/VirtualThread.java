/*
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package java.lang;

import java.security.AccessControlContext;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.ProtectionDomain;
import java.util.Arrays;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.ForkJoinPool.ForkJoinWorkerThreadFactory;
import java.util.concurrent.ForkJoinWorkerThread;
import java.util.concurrent.Future;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.ForkJoinTask;

import sun.misc.Unsafe;
import sun.nio.ch.Interruptible;
import sun.security.action.GetPropertyAction;
import static java.util.concurrent.TimeUnit.NANOSECONDS;

/**
 * A thread that is scheduled by the Java virtual machine rather than the operating
 * system.
 */

class VirtualThread extends Thread {
    private static final Unsafe unsafe = Unsafe.getUnsafe();
    private static final ContinuationScope VTHREAD_SCOPE = new ContinuationScope("VirtualThreads");
    private static final Executor DEFAULT_SCHEDULER = defaultScheduler();
    private static final ScheduledExecutorService UNPARKER = delayedTaskScheduler();
    private static final int TRACE_PINNING_MODE = tracePinningMode();

    private static final long STATE_OFFSET;
    private static final long PARK_PERMIT_OFFSET;
    private static final int PARK_PERMIT_TRUE = 1;
    private static final int PARK_PERMIT_FALSE = 0;
    static {
        try {
            STATE_OFFSET = unsafe.objectFieldOffset(VirtualThread.class.getDeclaredField("state"));
            PARK_PERMIT_OFFSET = unsafe.objectFieldOffset(VirtualThread.class.getDeclaredField("parkPermit"));
        } catch (Exception e) {
            throw new InternalError(e);
        }
    }

    // scheduler and continuation
    private final Executor scheduler;
    private final Continuation cont;
    private final Runnable runContinuation;

    // carrier thread when mounted
    private volatile Thread carrierThread;

    // virtual thread state
    private static final int ST_NEW      = 0;
    private static final int ST_STARTED  = 1;
    private static final int ST_RUNNABLE = 2;
    private static final int ST_RUNNING  = 3;
    private static final int ST_PARKING  = 4;
    private static final int ST_PARKED   = 5;
    private static final int ST_PINNED   = 6;
    private static final int ST_WALKINGSTACK = 51;  // Thread.getStackTrace
    private static final int ST_TERMINATED   = 99;
    private volatile int state;

    // park/unpark and await support
    private final ReentrantLock lock = new ReentrantLock();
    private Condition parking;            // created lazily
    private Condition termination;        // created lazily
    private volatile int parkPermit;      // 0 for false and 1 for true
    private volatile boolean interrupted;

    /**
     * Creates a new {@code VirtualThread} to run the given task with the given scheduler.
     *
     * @param scheduler the scheduler
     * @param name thread name
     * @param characteristics characteristics
     * @param task the task to execute
     */
    VirtualThread(Executor scheduler, String name, int characteristics, Runnable task) {
        super(name == null ? "<unnamed>" : name);

        Objects.requireNonNull(task);

        Runnable target = () -> {
            Throwable exc = null;
            try {
                task.run();
            } catch (Throwable e) {
                exc = e;
            } finally {
                if (exc != null) {
                    dispatchUncaughtException(exc);
                }
            }
        };

        this.scheduler = (scheduler != null) ? scheduler : DEFAULT_SCHEDULER;
        this.cont = new Continuation(VTHREAD_SCOPE, target) {
            @Override
            protected void onPinned(Continuation.Pinned reason) {
                if (TRACE_PINNING_MODE > 0) {
                    // switch to carrier thread as the printing may park
                    Thread thread = Thread.currentCarrierThread();
                    VirtualThread vthread = thread.getVirtualThread();
                    thread.setVirtualThread(null);
                    try {
                        boolean printAll = (TRACE_PINNING_MODE == 1);
                        PinnedThreadPrinter.printStackTrace(printAll);
                    } finally {
                        thread.setVirtualThread(vthread);
                    }
                }
                yieldFailed();
            }
        };

        // TBD create ForkJoinTask to avoid wrapping
        this.runContinuation = this::runContinuation;
    }

    /**
     * Schedules this {@code VirtualThread} to execute.
     *
     * @throws IllegalThreadStateException if the thread has already been started
     * @throws RejectedExecutionException if the scheduler cannot accept a task
     */
    @Override
    public void start() {
        if (!stateCompareAndSet(ST_NEW, ST_STARTED)) {
            throw new IllegalThreadStateException("Already started");
        }
        boolean scheduled = false;
        try {
            scheduler.execute(runContinuation);
            scheduled = true;
        } finally {
            if (!scheduled) {
                afterTerminate(false);
            }
        }
    }

    /**
     * Runs or continues execution of the continuation on the current thread.
     */
    private void runContinuation() {
        // the carrier thread should be a kernel thread
        if (Thread.currentThread().isVirtual()) {
            if (stateGet() == ST_STARTED) {
                afterTerminate(false);
            } else {
                // nothing to do
            }
            return;
        }

        // set state to ST_RUNNING
        int initialState = stateGet();
        assert initialState == ST_STARTED || initialState == ST_RUNNABLE;
        stateGetAndSet(ST_RUNNING);
        if (initialState == ST_RUNNABLE) {
            // consume parking permit
            parkPermitGetAndSet(PARK_PERMIT_FALSE);
        }

        boolean firstRun = (initialState == ST_STARTED);
        mount(firstRun);
        try {
            cont.run();
        } finally {
            unmount();
            if (cont.isDone()) {
                afterTerminate(true);
            } else {
                afterYield();
            }
        }
    }

    /**
     * Mounts this virtual thread. This method must be invoked before the continuation
     * is run or continued. It binds the virtual thread to the current carrier thread.
     */
    private void mount(boolean firstRun) {
        Thread thread = Thread.currentCarrierThread();

        // sets the carrier thread and forward interrupt status if needed
        carrierThread = thread;
        if (interrupted) {
            thread.setInterrupt();
        }

        // set thread field so Thread.currentThread() returns the VirtualThread object
        assert thread.getVirtualThread() == null;
        thread.setVirtualThread(this);

        if (firstRun && notifyJvmtiEvents) {
            //notifyStarted(thread, this);
            //notifyMount(thread, this);
        }
    }

    /**
     * Unmounts this virtual thread. This method must be invoked after the continuation
     * yields or terminates. It unbinds this virtual thread from the carrier thread.
     */
    private void unmount() {
        Thread thread = Thread.currentCarrierThread();

        if (notifyJvmtiEvents) {
            //notifyUnmount(thread, this);
        }

        // drop connection between this virtual thread and the carrier thread
        thread.setVirtualThread(null);
        synchronized (getBlockerLock()) {   // synchronize with interrupt
            carrierThread = null;
        }
    }

    /**
     * Invoke after yielding. If parking, sets the state to ST_PARKED and notifies
     * anyone waiting for the virtual thread to park.
     */
    private void afterYield() {
        int s = stateGet();
        if (s == ST_PARKING) {
            // signal anyone waiting for this virtual thread to park
            stateGetAndSet(ST_PARKED);
            signalParking();
        } else if (s == ST_RUNNING) {
            // Thread.yield, submit task to continue
            stateGetAndSet(ST_RUNNABLE);
            scheduler.execute(runContinuation);   // TBD if REE is thrown
        } else {
            throw new InternalError();
        }
    }

    /**
     * Invokes when the virtual thread terminates to set the state to ST_TERMINATED
     * and notify anyone waiting for the virtual thread to terminate.
     *
     * @param notifyAgents true to notify JVMTI agents
     */
    private void afterTerminate(boolean notifyAgents) {
        int oldState = stateGetAndSet(ST_TERMINATED);
        assert oldState == ST_STARTED || oldState == ST_RUNNING;

        // notify JVMTI agents
        if (notifyAgents && notifyJvmtiEvents) {
            Thread thread = Thread.currentCarrierThread();
            //notifyTerminated(thread, this);
        }

        // notify anyone waiting for this virtual thread to terminate
        signalTermination();
    }

    /**
     * Invoked by onPinned when the continuation cannot yield due to a synchronized
     * or native frame on the continuation stack. If the virtual thread is parking
     * then its state is changed to ST_PINNED and carrier thread parks.
     */
    private void yieldFailed() {
        if (stateGet() == ST_RUNNING) {
            // nothing to do
            return;
        }

        // switch to carrier thread
        Thread thread = Thread.currentCarrierThread();
        thread.setVirtualThread(null);

        boolean parkInterrupted = false;
        lock.lock();
        try {
            if (!stateCompareAndSet(ST_PARKING, ST_PINNED))
                throw new InternalError();

            Condition parking = parkingCondition();

            // signal anyone waiting for this virtual thread to park
            parking.signalAll();

            // and wait to be unparked (may be interrupted)
            parkingCondition().await();

        } catch (InterruptedException e) {
            parkInterrupted = true;
        } finally {
            lock.unlock();

            // continue running on the carrier thread
            if (!stateCompareAndSet(ST_PINNED, ST_RUNNING))
                throw new InternalError();

            // consume parking permit
            parkPermitGetAndSet(PARK_PERMIT_FALSE);

            // switch back to virtual thread
            thread.setVirtualThread(this);
        }

        // restore interrupt status
        if (parkInterrupted)
            Thread.currentThread().interrupt();
    }

    /**
     * Disables the current virtual thread for scheduling purposes.
     *
     * <p> If this virtual thread has already been {@link #unpark() unparked} then the
     * parking permit is consumed and this method completes immediately;
     * otherwise the current virtual thread is disabled for scheduling purposes and lies
     * dormant until it is {@linkplain #unpark() unparked} or the thread is
     * {@link Thread#interrupt() interrupted}.
     *
     * @throws IllegalCallerException if not called from a virtual thread
     */
    static void park() {
        VirtualThread vthread = Thread.currentCarrierThread().getVirtualThread();
        if (vthread == null)
            throw new Error("not a virtual thread");
        vthread.maybePark();
    }

    /**
     * Disables the current virtual thread for scheduling purposes for up to the
     * given waiting time.
     *
     * <p> If this virtual thread has already been {@link #unpark() unparked} then the
     * parking permit is consumed and this method completes immediately;
     * otherwise if the time to wait is greater than zero then the current virtual thread
     * is disabled for scheduling purposes and lies dormant until it is {@link
     * #unpark unparked}, the waiting time elapses or the thread is
     * {@linkplain Thread#interrupt() interrupted}.
     *
     * @param nanos the maximum number of nanoseconds to wait.
     *
     * @throws IllegalCallerException if not called from a virtual thread
     */
    static void parkNanos(long nanos) {
        Thread thread = Thread.currentCarrierThread();
        VirtualThread vthread = thread.getVirtualThread();
        if (vthread == null)
            throw new Error("not a virtual thread");
        if (nanos > 0) {
            // switch to carrier thread when submitting to avoid parking here
            thread.setVirtualThread(null);
            Future<?> unparker;
            try {
                unparker = UNPARKER.schedule(vthread::unpark, nanos, NANOSECONDS);
            } finally {
                thread.setVirtualThread(vthread);
            }
            // now park
            try {
                vthread.maybePark();
            } finally {
                unparker.cancel(false);
            }
        } else {
            // consume permit when not parking
            vthread.tryYield();
            vthread.parkPermitGetAndSet(PARK_PERMIT_FALSE);
        }
    }

    /**
     * Park or complete immediately.
     *
     * <p> If this virtual thread has already been unparked or its interrupt status is
     * set then this method completes immediately; otherwise it yields.
     */
    private void maybePark() {
        assert Thread.currentCarrierThread().getVirtualThread() == this;

        // prepare to park; important to do this before consuming the parking permit
        if (!stateCompareAndSet(ST_RUNNING, ST_PARKING))
            throw new InternalError();

        // consume permit if available, and continue rather than park
        if (parkPermitGetAndSet(PARK_PERMIT_FALSE) || interrupted) {
            if (!stateCompareAndSet(ST_PARKING, ST_RUNNING))
                throw new InternalError();

            // signal anyone waiting for this virtual thread to park
            signalParking();
            return;
        }
        Continuation.yield(VTHREAD_SCOPE);

        // continued
        assert stateGet() == ST_RUNNING;

        // notify JVMTI mount event here so that stack is available to agents
        if (notifyJvmtiEvents) {
            //notifyMount(Thread.currentCarrierThread(), this);
        }
    }

    /**
     * Re-enables this virtual thread for scheduling. If the virtual thread was
     * {@link #park() parked} then it will be unblocked, otherwise its next call
     * to {@code park} or {@linkplain #parkNanos(long) parkNanos} is guaranteed
     * not to block.
     *
     * @throws RejectedExecutionException if the scheduler cannot accept a task
     * @return this virtual thread
     */
    VirtualThread unpark() {
        if (!parkPermitGetAndSet(PARK_PERMIT_TRUE) && Thread.currentThread() != this) {
            int s = waitIfParking();
            // avoid scheduler.execute park current virtual thread, this might cause deadlock
            // park virtual thread on scheduler blockedLinkedQueue's lock
            // carrier thread might all block this lock and if unpark is first happens on
            // this virtual thread, its cause dead lock
            // clear virtual thread make this block in carrier thread only
            Thread thread = Thread.currentCarrierThread();
            VirtualThread vthread = thread.getVirtualThread();
            if (vthread != null) thread.setVirtualThread(null);
            try {
                if (s == ST_PARKED && stateCompareAndSet(ST_PARKED, ST_RUNNABLE)) {
                    boolean scheduled = false;
                    try {
                        //if (tryForkLocal() == false) {
                            scheduler.execute(runContinuation);
                        //}
                        scheduled = true;
                    } finally {
                        if (!scheduled) {
                            stateCompareAndSet(ST_RUNNABLE, ST_PARKED);
                        }
                    }
                }
            } finally {
                if (vthread != null) thread.setVirtualThread(vthread);
            }
        }
        return this;
    }

    /**
     * Returns true if parking.
     */
    boolean isParking() {
        assert Thread.currentCarrierThread().getVirtualThread() == this;
        return state == ST_PARKING;
    }

    /**
     * If this virtual thread is parking then wait for it to exit the ST_PARKING state.
     * If the virtual thread is pinned then signal it to continue on the original carrier
     * thread.
     *
     * @return the virtual thread state
     */
    private int waitIfParking() {
        int s;
        int spins = 0;
        while (((s = stateGet()) == ST_PARKING) && (spins < 32)) {
            //Thread.onSpinWait();
            spins++;
        }
        if (s == ST_PARKING || s == ST_PINNED) {
            boolean parkInterrupted = false;
            Thread thread = Thread.currentCarrierThread();
            VirtualThread vthread = thread.getVirtualThread();
            if (vthread != null) thread.setVirtualThread(null);
            lock.lock();
            try {
                while ((s = stateGet()) == ST_PARKING) {
                    try {
                        parkingCondition().await();
                    } catch (InterruptedException e) {
                        parkInterrupted = true;
                    }
                }
                if (s == ST_PINNED) {
                    // signal so that execution continues on original thread
                    parkingCondition().signalAll();
                }
            } finally {
                lock.unlock();
                if (vthread != null) thread.setVirtualThread(vthread);
            }
            if (parkInterrupted)
                Thread.currentThread().interrupt();
        }
        return s;
    }

    /**
     * Attempts to yield. A no-op if the continuation is pinned.
     */
    void tryYield() {
        assert Thread.currentCarrierThread().getVirtualThread() == this;
        Continuation.yield(VTHREAD_SCOPE);
        assert state == ST_RUNNING;

    }
    /**
     * Waits up to {@code nanos} nanoseconds for this virtual thread to terminate.
     * A timeout of {@code 0} means to wait forever.
     *
     * @throws IllegalArgumentException if nanos is negative
     * @throws IllegalStateException if not started
     * @throws InterruptedException if interrupted while waiting
     * @return true if the thread has terminated
     */
    boolean joinNanos(long nanos) throws InterruptedException {
        if (nanos < 0)
            throw new IllegalArgumentException();

        int s = stateGet();
        if (s == ST_TERMINATED)
            return true;
        if (s == ST_NEW)
            throw new IllegalStateException("Not started");

        lock.lock();
        try {
            if (stateGet() == ST_TERMINATED)
                return true;

            // wait
            if (nanos == 0) {
                terminationCondition().await();
            } else {
                terminationCondition().await(nanos, NANOSECONDS);
            }
        } finally {
            lock.unlock();
        }
        return (stateGet() == ST_TERMINATED);
    }

    @Override
    public void interrupt() {
        if (Thread.currentThread() != this) {
            checkAccess();
            synchronized (getBlockerLock()) {
                interrupted = true;
                Interruptible b = getBlocker();
                if (b != null) {
                    b.interrupt(this);
                }

                // interrupt carrier thread if not null
                // this means this virtual thread is attached on its carrier thread now.
                Thread t = carrierThread;
                if (t != null) t.setInterrupt();
            }
        } else {
            interrupted = true;
            carrierThread.setInterrupt();
        }
        unpark();
    }

    @Override
    public boolean isInterrupted() {
        return interrupted;
    }

    boolean getAndClearInterrupt() {
        assert Thread.currentCarrierThread().getVirtualThread() == this;
        synchronized (getBlockerLock()) {
            boolean oldValue = interrupted;
            if (oldValue)
                interrupted = false;
            Thread t = carrierThread;
            //if (t != null) t.clearInterrupt(); only for win32 in loom
            return oldValue;
        }
    }

    /**
     * Sleep the current thread for the given sleep time (in nanoseconds)
     * @throws InterruptedException if interrupted while sleeping
     */
    void sleepNanos(long nanos) throws InterruptedException {
        assert Thread.currentCarrierThread().getVirtualThread() == this;
        if (nanos >= 0) {
            if (getAndClearInterrupt())
                throw new InterruptedException();
            if (nanos == 0) {
                tryYield();
            } else {
                long remainingNanos = nanos;
                long startNanos = System.nanoTime();
                while (remainingNanos > 0) {
                    parkNanos(remainingNanos);
                    if (getAndClearInterrupt()) {
                        throw new InterruptedException();
                    }
                    remainingNanos = nanos - (System.nanoTime() - startNanos);
                }
            }
        }
    }

    @Override
    public Thread.State getState() {
        switch (stateGet()) {
            case ST_NEW:
                return Thread.State.NEW;
            case ST_STARTED:
            case ST_RUNNABLE:
                // runnable, not mounted
                return Thread.State.RUNNABLE;
            case ST_RUNNING:
                // if mounted then return state of carrier thread
                synchronized (getBlockerLock()) {
                    Thread carrierThread = this.carrierThread;
                    if (carrierThread != null) {
                        return carrierThread.getState();
                    }
                }
                // runnable, mounted
                return Thread.State.RUNNABLE;
            case ST_PARKING:
                // runnable, mount, not yet waiting
                return Thread.State.RUNNABLE;
            case ST_PARKED:
            case ST_PINNED:
            case ST_WALKINGSTACK:
                return Thread.State.WAITING;
            case ST_TERMINATED:
                return Thread.State.TERMINATED;
            default:
                throw new InternalError();
        }
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder("VirtualThread[");
        String name = getName();
        if (name.length() > 0) {
            sb.append(name);
        } else {
            sb.append("@");
            sb.append(Integer.toHexString(hashCode()));
        }
        sb.append(",");
        Thread t = carrierThread;
        if (t != null) {
            sb.append(t.getName());
            ThreadGroup g = t.getThreadGroup();
            if (g != null) {
                sb.append(",");
                sb.append(g.getName());
            }
        } else {
            if (stateGet() == ST_TERMINATED) {
                sb.append("<terminated>");
            } else {
                sb.append("<no carrier thread>");
            }
        }
        sb.append("]");
        return sb.toString();
    }

    /**
     * Signal the Condition object for parking
     */
    private void signalParking() {
        Thread t = Thread.currentCarrierThread();
        boolean inVirtualThread = t.getVirtualThread() != null;
        if (inVirtualThread) t.setVirtualThread(null);
        lock.lock();
        try {
            Condition parking = this.parking;
            if (parking != null) {
                parking.signalAll();
            }
        } finally {
            lock.unlock();
            if (inVirtualThread) t.setVirtualThread(this);
        }
    }

    /**
     * Signal the Condition object for termination
     */
    private void signalTermination() {
        lock.lock();
        try {
            Condition termination = this.termination;
            if (termination != null) {
                termination.signalAll();
            }
        } finally {
            lock.unlock();
        }
    }

    /**
     * Returns the Condition object for parking, creating it if needed.
     */
    private Condition parkingCondition() {
        assert lock.isHeldByCurrentThread();
        Condition parking = this.parking;
        if (parking == null) {
            this.parking = parking = lock.newCondition();
        }
        return parking;
    }

    /**
     * Returns the Condition object for termination, creating it if needed.
     */
    private Condition terminationCondition() {
        assert lock.isHeldByCurrentThread();
        Condition termination = this.termination;
        if (termination == null) {
            this.termination = termination = lock.newCondition();
        }
        return termination;
    }

    // -- stack trace support --

    private static final StackTraceElement[] EMPTY_STACK = new StackTraceElement[0];

    @Override
    public StackTraceElement[] getStackTrace() {
        /*if (Thread.currentCarrierThread().getVirtualThread() == this) {
        } else {
            // target virtual thread may be mounted or unmounted
            StackTraceElement[] stackTrace;
            do {
                Thread carrier = carrierThread;
                if (carrier != null) {
                    // mounted
                    // stackTrace = tryGetStackTrace(carrier);
                } else {
                    // not mounted
                    // stackTrace = tryGetStackTrace();
                }
                if (stackTrace == null) {
                    // Thread.onSpinWait();
                }
            } while (stackTrace == null);
            return stackTrace;
        }*/
        return null;
    }

    // -- wrappers for VarHandle methods --

    private int stateGet() {
        return unsafe.getInt(this, STATE_OFFSET);
    }

    private int stateGetAndSet(int newValue) {
        return unsafe.getAndSetInt(this, STATE_OFFSET, newValue);
    }

    private boolean stateCompareAndSet(int expectedValue, int newValue) {
        boolean res = unsafe.compareAndSwapInt(this, STATE_OFFSET, expectedValue, newValue);
        return res;
    }

    private boolean parkPermitGetAndSet(int newValue) {
        return unsafe.getAndSetInt(this, PARK_PERMIT_OFFSET, newValue) == PARK_PERMIT_TRUE;
    }

    // -- JVM TI support --

    private static volatile boolean notifyJvmtiEvents;  // set by VM
    /*private static native void notifyStarted(Thread carrierThread, VirtualThread vthread);
    private static native void notifyTerminated(Thread carrierThread, VirtualThread vthread);
    private static native void notifyMount(Thread carrierThread, VirtualThread vthread);
    private static native void notifyUnmount(Thread carrierThread, VirtualThread vthread);
    private static native void registerNatives();*/
    static {
        //registerNatives();
    }

    /**
     * Creates the default scheduler as ForkJoinPool.
     */
    private static Executor defaultScheduler() {
        ForkJoinWorkerThreadFactory factory = pool -> {
            PrivilegedAction<ForkJoinWorkerThread> pa = () -> new CarrierThread(pool);
            return AccessController.doPrivileged(pa);
        };
        PrivilegedAction<Executor> pa = () -> {
            int parallelism;
            String s = System.getProperty("jdk.defaultScheduler.parallelism");
            if (s != null) {
                parallelism = Integer.parseInt(s);
            } else {
                parallelism = Runtime.getRuntime().availableProcessors();
            }
            Thread.UncaughtExceptionHandler ueh = (t, e) -> { };
            // use FIFO as default
            s = System.getProperty("jdk.defaultScheduler.lifo");
            boolean asyncMode = (s == null) || s.equalsIgnoreCase("false");
            return new ForkJoinPool(parallelism, factory, ueh, asyncMode);
        };
        return AccessController.doPrivileged(pa);
    }

    /**
     * A thread in the ForkJoinPool created by the default scheduler.
     */
    private static class CarrierThread extends ForkJoinWorkerThread {
        CarrierThread(ForkJoinPool pool) {
            super(pool);
        }

        @Override
        public void setUncaughtExceptionHandler(UncaughtExceptionHandler ueh) { }

        @Override
        public void setContextClassLoader(ClassLoader cl) {
            throw new SecurityException("setContextClassLoader");
        }
    }

    /**
     * Creates the ScheduledThreadPoolExecutor used to schedule unparking.
     */
    private static ScheduledExecutorService delayedTaskScheduler() {
        ScheduledThreadPoolExecutor stpe = (ScheduledThreadPoolExecutor)
            Executors.newScheduledThreadPool(1, new ThreadFactory() {
                public Thread newThread(Runnable r) {
                    return AccessController.doPrivileged(new PrivilegedAction<Thread>() {
                        public Thread run() {
                            Thread t = new Thread(r);
                            t.setName("VirtualThreadUnparker");
                            t.setDaemon(true);
                            return t;
                        }
                    });
                }
            });
        stpe.setRemoveOnCancelPolicy(true);
        return stpe;
    }

    /**
     * Helper class to print the virtual thread stack trace when a carrier thread is
     * pinned.
     */
    private static class PinnedThreadPrinter {
        /**
         * Prints a stack trace of the current virtual thread to the standard output stream.
         * This method is synchronized to reduce interference in the output.
         * @param printAll true to print all stack frames, false to only print the
         *        frames that are native or holding a monitor
         */
        static synchronized void printStackTrace(boolean printAll) {
        }
    }

    /**
     * Reads the value of the jdk.tracePinning property to determine if stack
     * traces should be printed when a carrier thread is pinned when a virtual thread
     * attempts to park.
     */
    private static int tracePinningMode() {
        String value = GetPropertyAction.privilegedGetProperty("jdk.tracePinnedThreads");
        if (value != null) {
            if (value.length() == 0 || "full".equalsIgnoreCase(value))
                return 1;
            if ("short".equalsIgnoreCase(value))
                return 2;
        }
        return 0;
    }

    /* There could be fastpath to add task in same thread's work queue
     * If current thread is ForkJoinWorkerThread and shcueler is same with
     * ForkJoinWorkerThread's scheudler, append runnable task directory in
     * current ForkJoinWorkerThread's local work queue
     * This can be used in unpark for fast processing
     */
    private boolean tryForkLocal() {
        Thread t;
        if ((t = Thread.currentCarrierThread()) instanceof ForkJoinWorkerThread) {
            ForkJoinWorkerThread ft = (ForkJoinWorkerThread)t;
            if (ft.getPool() == scheduler) {
                DefaultTask task = new DefaultTask(runContinuation);
                task.fork();
                return true;
            }
        }
        return false;
    }

    static final class DefaultTask extends ForkJoinTask<Void> {
        final Runnable runnable;
        DefaultTask(Runnable runnable) {
            if (runnable == null) {
                throw new NullPointerException();
            }
            this.runnable = runnable;
        }
        public final Void getRawResult() { return null; }
        public final void setRawResult(Void v) { }
        public final boolean exec() {
            runnable.run();
            return true;
        }
    }
}
