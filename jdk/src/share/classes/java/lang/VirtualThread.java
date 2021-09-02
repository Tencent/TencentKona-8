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

import java.lang.invoke.MethodHandles;
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
import java.util.concurrent.ThreadPoolExecutor;

import sun.misc.InnocuousThread;
import sun.misc.Unsafe;
import sun.nio.ch.Interruptible;
import sun.security.action.GetPropertyAction;
import sun.security.util.SecurityConstants;

import static java.util.concurrent.TimeUnit.NANOSECONDS;

/**
 * A thread that is scheduled by the Java virtual machine rather than the operating
 * system.
 */

class VirtualThread extends Thread {
    private static final Executor DEFAULT_SCHEDULER = createDefaultScheduler();
    private static final ScheduledExecutorService UNPARKER = delayedTaskScheduler();
    private static final int TRACE_PINNING_MODE = tracePinningMode();
    private static final Unsafe UNSAFE = Unsafe.getUnsafe();

    // scope used for the continuations
    static final ContinuationScope VTHREAD_SCOPE = new ContinuationScope("VirtualThreads");

    private static final long STATE;
    private static final long PARK_PERMIT;
    private static final long CARRIER_THREAD;
    private static final long LOCK;
    static {
        try {
            STATE = UNSAFE.objectFieldOffset(VirtualThread.class.getDeclaredField("state"));
            PARK_PERMIT = UNSAFE.objectFieldOffset(VirtualThread.class.getDeclaredField("parkPermit"));
            CARRIER_THREAD = UNSAFE.objectFieldOffset(VirtualThread.class.getDeclaredField("carrierThread"));
            LOCK = UNSAFE.objectFieldOffset(VirtualThread.class.getDeclaredField("lock"));
        } catch (Exception e) {
            throw new InternalError(e);
        }
    }

    // scheduler and continuation
    private final Executor scheduler;
    private final Continuation cont;
    private final Runnable runContinuation;
    private final Runnable vtTarget;

    // virtual thread state
    private static final int NEW      = 0;
    private static final int STARTED  = 1;
    private static final int RUNNABLE = 2;     // runnable-unmounted
    private static final int RUNNING  = 3;     // runnable-mounted
    private static final int PARKING  = 4;
    private static final int PARKED   = 5;     // unmounted
    private static final int PINNED   = 6;     // mounted
    private static final int TERMINATED = 99;  // final state

    // can be suspended from scheduling when parked
    private static final int SUSPENDED = 1 << 8;
    private static final int PARKED_SUSPENDED = (PARKED | SUSPENDED);

    private static final int PARK_PERMIT_TRUE = 1;
    private static final int PARK_PERMIT_FALSE = 0;

    private volatile int state;

    // parking permit
    private volatile int parkPermit;

    // carrier thread when mounted
    private volatile Thread carrierThread;

    private volatile boolean interrupted;

    // lock/condition used when waiting (join or pinned)
    private volatile ReentrantLock lock;   // created lazily
    private Condition condition;           // created lazily while holding lock
    private final boolean isThreadPoolExecutor;
    private final boolean schedulerHookParkCarrier;

   /**
     * Add a user defined callback function when continuation try to yield
     * while it is pinned.
     *
     * @param Continuation.PinnedAction the user defined interface.
     */
    public void setPinnedAction(Continuation.PinnedAction pinnedAction) {
        this.pinnedAction = pinnedAction;
    }

    private Continuation.PinnedAction pinnedAction;

    private class VTContinuation extends Continuation {
        VTContinuation(ContinuationScope scope, Runnable target) {
            super(scope, target);
        }
        @Override
        protected void onPinned(Continuation.Pinned reason) {
            if (TRACE_PINNING_MODE > 0) {
                // switch to carrier thread as the printing may park
                Thread carrier = Thread.currentCarrierThread();
                VirtualThread vthread = carrier.getVirtualThread();
                carrier.setVirtualThread(null);
                try {
                    PinnedThreadPrinter.printStackTrace();
                } finally {
                    carrier.setVirtualThread(vthread);
                }
            }

            if (pinnedAction != null) {
                pinnedAction.run(reason);
            }

            if (state() == PARKING) {
                parkCarrierThread();
            }
            assert state() == RUNNING;
        }
    }

    /**
     * Creates a new {@code VirtualThread} to run the given task with the given scheduler.
     *
     * @param scheduler the scheduler
     * @param name thread name
     * @param characteristics characteristics
     * @param task the task to execute
     */
    VirtualThread(Executor scheduler, String name, int characteristics, Runnable task) {
        super(null, name == null ? "<unnamed>" : name, characteristics);

        Objects.requireNonNull(task);
        Runnable target = () -> {
            try {
                task.run();
            } catch (Throwable exc) {
                dispatchUncaughtException(exc);
            }
        };

        if (scheduler == null) {
            Thread parent = Thread.currentThread();
            if (parent.isVirtual()) {
                scheduler = ((VirtualThread)parent).scheduler;
            } else {
                scheduler = DEFAULT_SCHEDULER;
            }
        }
        this.scheduler = scheduler;
        this.vtTarget = task;
        this.isThreadPoolExecutor = (scheduler == null) ? false : (scheduler instanceof ThreadPoolExecutor);
        this.schedulerHookParkCarrier = (scheduler == null) ? false : (scheduler instanceof VTParkCarrierAction);
        this.cont = new VTContinuation(VTHREAD_SCOPE, target);
        this.runContinuation = /*(scheduler != null)
                ? new Runner(this)
                :*/ this::runContinuation;
    }

    /**
     * Schedules this {@code VirtualThread} to execute.
     *
     * @throws IllegalThreadStateException if the thread has already been started
     * @throws RejectedExecutionException if the scheduler cannot accept a task
     */
    @Override
    public void start() {
        if (!compareAndSetState(NEW, STARTED)) {
            throw new IllegalThreadStateException("Already started");
        }
        try {
            ScheduleContinuation();
        } catch (RejectedExecutionException ree) {
            // assume executor has been shutdown
            afterTerminate(false);
            throw ree;
        }
    }

    /**
     * Runs or continues execution of the continuation on the current thread.
     */
    private void runContinuation() {
        // the carrier thread should be a kernel thread
        if (Thread.currentThread().isVirtual()) {
            throw new InternalError("Carrier is virtual");
        }

        // set state to RUNNING
        int initialState = state();
        if (initialState == STARTED && compareAndSetState(STARTED, RUNNING)) {
            // first run
        } else if (initialState == RUNNABLE && compareAndSetState(RUNNABLE, RUNNING)) {
            // consume parking permit
            setParkPermit(PARK_PERMIT_FALSE);
        } else {
            throw new IllegalStateException();
        }

        boolean firstRun = (initialState == STARTED);
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
     * The task to execute when using a custom scheduler.
     */
    /*private static class Runner implements VirtualThreadTask {
        private final VirtualThread vthread;
        private static final VarHandle ATTACHMENT;
        static {
            try {
                MethodHandles.Lookup l = MethodHandles.lookup();
                ATTACHMENT = l.findVarHandle(Runner.class, "attachment", Object.class);
            } catch (Exception e) {
                throw new InternalError(e);
            }
        }
        private volatile Object attachment;
        Runner(VirtualThread vthread) {
            this.vthread = vthread;
        }
        @Override
        public void run() {
            vthread.runContinuation();
        }
        @Override
        public Thread thread() {
            return vthread;
        }
        @Override
        public Object attach(Object ob) {
            return ATTACHMENT.getAndSet(this, ob);
        }
        @Override
        public Object attachment() {
            return attachment;
        }
    }*/

    /**
     * Mounts this virtual thread. This method must be invoked before the continuation
     * is run or continued. It binds the virtual thread to the current carrier thread.
     */
    private void mount(boolean firstRun) {
        Thread carrier = Thread.currentCarrierThread();
        //assert carrierThread == null && thread.getVirtualThread() == null;

        // sets the carrier thread
        UNSAFE.putObjectVolatile(this, CARRIER_THREAD, carrier);

        // set thread field so Thread.currentThread() returns the VirtualThread object
        carrier.setVirtualThread(this);

        // sync up carrier thread interrupt status if needed
        if (interrupted) {
            carrier.setInterrupt();
        } else if (carrier.isInterrupted()) {
            synchronized (getBlockerLock()) {
                if (!interrupted) {
                    carrier.clearInterrupt();
                }
            }
        }

        /*if (firstRun && notifyJvmtiEvents) {
            notifyStarted(carrier, this);
            notifyMount(carrier, this);
        }*/
    }

    /**
     * Unmounts this virtual thread. This method must be invoked after the continuation
     * yields or terminates. It unbinds this virtual thread from the carrier thread.
     */
    private void unmount() {
        Thread carrier = Thread.currentCarrierThread();
        //assert carrierThread == thread & thread.getVirtualThread() == this;

        /*if (notifyJvmtiEvents) {
            notifyUnmount(carrier, this);
        }*/

        // drop connection between this virtual thread and the carrier thread
        carrier.setVirtualThread(null);
        synchronized (getBlockerLock()) {   // synchronize with interrupt
            UNSAFE.putObjectVolatile(this, CARRIER_THREAD, null);
        }

        // clear carrier thread interrupt status before exit
        carrier.clearInterrupt();
    }

    /**
     * Invoked after yielding. If parking it sets the state to PARKED. If
     * unparked while parking then will attempt to submit the task to continue.
     * If yield (Thread.yield) then submits the task to continue.
     */
    private void afterYield() {
        int s = state();
        if (s == PARKING) {
            setState(PARKED);
            // may have been unparked while parking
            if (parkPermit == PARK_PERMIT_TRUE && compareAndSetState(PARKED, RUNNABLE)) {
                ScheduleContinuation();
            }
        } else if (s == RUNNING) {
            // Thread.yield, submit task to continue
            setState(RUNNABLE);
            ScheduleContinuation();
        } else {
            throw new InternalError();
        }
    }

    /**
     * Invokes when the virtual thread terminates to set the state to TERMINATED
     * and notify anyone waiting for the virtual thread to terminate.
     *
     * @param notifyAgents true to notify JVMTI agents
     */
    private void afterTerminate(boolean notifyAgents) {
        assert state == STARTED || state == RUNNING;
        setState(TERMINATED);   // final state

        // notify JVMTI agents
        /*if (notifyAgents && notifyJvmtiEvents) {
            Thread thread = Thread.currentCarrierThread();
            notifyTerminated(thread, this);
        }*/

        // notify anyone waiting for this virtual thread to terminate
        ReentrantLock lock = this.lock;
        if (lock != null) {
            lock.lock();
            try {
                getCondition().signalAll();
            } finally {
                lock.unlock();
            }
        }
    }

    /**
     * Invoked by onPinned when an attempt to park fails because of a synchronized
     * or native frame on the continuation stack. The state is changed to PINNED
     * and the carrier thread parks until the virtual thread is unparked.
     */
    private void parkCarrierThread() {
        boolean awaitInterrupted = false;

        // switch to carrier thread
        Thread carrier = Thread.currentCarrierThread();
        carrier.setVirtualThread(null);
        final ReentrantLock lock = getLock();
        lock.lock();
        try {
            assert state == PARKING;
            setState(PINNED);

            if (parkPermit == PARK_PERMIT_FALSE) {
                // wait to be unparked or interrupted
                if (schedulerHookParkCarrier) {
                    ((VTParkCarrierAction)scheduler).beforePark();
                }
                getCondition().await();
            }
        } catch (InterruptedException e) {
            awaitInterrupted = true;
        } finally {
            lock.unlock();
            // after unlock avoid deadlock if other thread is trying
            // unpark current VT
            if (schedulerHookParkCarrier) {
                ((VTParkCarrierAction)scheduler).afterPark();
            }
            // continue running on the carrier thread
            assert state == PINNED;
            setState(RUNNING);

            // consume parking permit
            setParkPermit(PARK_PERMIT_FALSE);

            // switch back to virtual thread
            carrier.setVirtualThread(this);
        }

        // restore interrupt status
        if (awaitInterrupted)
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
            throw new InternalError("not a virtual thread");
        vthread.tryPark();
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
        Thread thread = Thread.currentThread();
        if (!thread.isVirtual())
            throw new InternalError("not a virtual thread");
        ((VirtualThread) thread).tryPark(nanos);
    }

    /**
     * Try to park. If already been unparked (parking permit available) or the
     * interrupt status is set then this method completes immediately without
     * yielding.
     */
    private void tryPark() {
        assert Thread.currentThread() == this && state == RUNNING;

        // continue if parking permit available or interrupted
        if (getAndSetParkPermit(PARK_PERMIT_FALSE) == PARK_PERMIT_TRUE || interrupted) {
            return;
        }

        setState(PARKING);

        boolean yielded = Continuation.yield(VTHREAD_SCOPE);

        // continued
        assert Thread.currentThread() == this && state == RUNNING;

        // notify JVMTI mount event here so that stack is available to agents
        /*if (yielded && notifyJvmtiEvents) {
            notifyMount(Thread.currentCarrierThread(), this);
        }*/
    }

    private void tryPark(long nanos) {
        // continue if parking permit available or interrupted
        if (getAndSetParkPermit(PARK_PERMIT_FALSE) == PARK_PERMIT_TRUE || interrupted) {
            return;
        }

        if (nanos > 0) {
            Future<?> unparker = scheduleUnpark(nanos);
            try {
                setState(PARKING);
                Continuation.yield(VTHREAD_SCOPE);
            } finally {
                if (state() != RUNNING) {
                    setState(RUNNING);
                }
                cancel(unparker);
            }
        } else {
            // consume permit when not parking
            tryYield();
            setParkPermit(PARK_PERMIT_FALSE);
        }
    }

    /**
     * Schedules this thread to be unparked after the given delay.
     */
    private Future<?> scheduleUnpark(long nanos) {
        //assert Thread.currentThread() == this;
        Thread carrier = this.carrierThread;
        // need to switch to carrier thread to avoid nested parking
        carrier.setVirtualThread(null);
        try {
            return UNPARKER.schedule(this::unpark, nanos, NANOSECONDS);
        } finally {
            carrier.setVirtualThread(this);
        }
    }

    /**
     * Cancels a task if it has not completed.
     */
    private void cancel(Future<?> future) {
        //assert Thread.currentThread() == this;
        if (!future.isDone()) {
            Thread carrier = this.carrierThread;
            // need to switch to carrier thread to avoid nested parking
            carrier.setVirtualThread(null);
            try {
                future.cancel(false);
            } finally {
                carrier.setVirtualThread(this);
            }
        }
    }

    /**
     * Re-enables this virtual thread for scheduling. If the virtual thread was
     * {@link #park() parked} then it will be unblocked, otherwise its next call
     * to {@code park} or {@linkplain #parkNanos(long) parkNanos} is guaranteed
     * not to block.
     *
     * @throws RejectedExecutionException if the scheduler cannot accept a task
     */
    void unpark() {
        if (getAndSetParkPermit(PARK_PERMIT_TRUE) == PARK_PERMIT_FALSE && Thread.currentThread() != this) {
            int s = state();
            if (s == PARKED && compareAndSetState(PARKED, RUNNABLE)) {
                ScheduleContinuation();
            } else if (s == PINNED) {
                // signal pinned thread so that it continues
                final ReentrantLock lock = getLock();
                lock.lock();
                try {
                    if (state() == PINNED) {
                        getCondition().signalAll();
                    }
                } finally {
                    lock.unlock();
                }
            }
        }
    }

    /**
     * Returns true if parking.
     */
    boolean isParking() {
        assert Thread.currentThread() == this;
        return state == PARKING;
    }

    /**
     * Attempts to yield. A no-op if the continuation is pinned.
     */
    void tryYield() {
        assert Thread.currentThread() == this && state == RUNNING;
        boolean yielded = Continuation.yield(VTHREAD_SCOPE);
        /*if (yielded && notifyJvmtiEvents) {
            notifyMount(Thread.currentCarrierThread(), this);
        }*/
        assert Thread.currentThread() == this && state == RUNNING;
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
        int s = state();
        if (s == NEW)
            throw new IllegalStateException("Not started");
        if (s == TERMINATED)
            return true;

        // timed or untimed wait for the thread to terminate
        final ReentrantLock lock = getLock();
        lock.lock();
        try {
            final Condition condition = getCondition();
            if (nanos == 0) {
                while (state() != TERMINATED) {
                    condition.await();
                }
                return true;
            } else {
                long startNanos = System.nanoTime();
                long remainingNanos = nanos;
                while (remainingNanos > 0 && state() != TERMINATED) {
                    condition.await(remainingNanos, NANOSECONDS);
                    remainingNanos = nanos - (System.nanoTime() - startNanos);
                }
                return (state() == TERMINATED);
            }
        } finally {
            lock.unlock();
        }
    }

    Runnable target() {
        return vtTarget;
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

                // interrupt carrier thread
                Thread carrier = carrierThread;
                if (carrier != null) carrier.setInterrupt();
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

    @Override
    boolean getAndClearInterrupt() {
        assert Thread.currentThread() == this;
        synchronized (getBlockerLock()) {
            boolean oldValue = interrupted;
            if (oldValue)
                interrupted = false;
            Thread carrier = carrierThread;
            if (carrier != null) carrier.clearInterrupt();
            return oldValue;
        }
    }

    /**
     * Sleep the current thread for the given sleep time (in nanoseconds)
     *
     * @implNote This implementation parks the thread for the given sleeping time
     * and will therefore be observed in PARKED state during the sleep. Parking
     * will consume the parking permit so this method makes available the parking
     * permit after the sleep. This will observed as spurious, but benign, wakeup
     * when the thread subsequently attempts to park.
     *
     * @throws InterruptedException if interrupted while sleeping
     */
    void sleepNanos(long nanos) throws InterruptedException {
        assert Thread.currentThread() == this;
        if (nanos >= 0) {
            if (getAndClearInterrupt())
                throw new InterruptedException();
            if (nanos == 0) {
                tryYield();
            } else {
                // park for the sleep time
                try {
                    long remainingNanos = nanos;
                    long startNanos = System.nanoTime();
                    while (remainingNanos > 0) {
                        parkNanos(remainingNanos);
                        if (getAndClearInterrupt()) {
                            throw new InterruptedException();
                        }
                        remainingNanos = nanos - (System.nanoTime() - startNanos);
                    }
                } finally {
                    // may have been unparked while sleeping
                    setParkPermit(PARK_PERMIT_TRUE);
                }
            }
        }
    }

    @Override
    public Thread.State getState() {
        switch (state()) {
            case NEW:
                return Thread.State.NEW;
            case STARTED:
            case RUNNABLE:
                // runnable, not mounted
                return Thread.State.RUNNABLE;
            case RUNNING:
                // if mounted then return state of carrier thread
                synchronized (getBlockerLock()) {
                    Thread carrierThread = this.carrierThread;
                    if (carrierThread != null) {
                        return carrierThread.getState();
                    }
                }
                // runnable, mounted
                return Thread.State.RUNNABLE;
            case PARKING:
                // runnable, mounted, not yet waiting
                return Thread.State.RUNNABLE;
            case PARKED:
            case PARKED_SUSPENDED:
            case PINNED:
                return Thread.State.WAITING;
            case TERMINATED:
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
        Thread carrier = carrierThread;
        if (carrier != null) {
            sb.append(carrier.getName());
            ThreadGroup g = carrier.getThreadGroup();
            if (g != null) {
                sb.append(",");
                sb.append(g.getName());
            }
        } else {
            if (state() == TERMINATED) {
                sb.append("<terminated>");
            } else {
                sb.append("<no carrier thread>");
            }
        }
        sb.append("]");
        return sb.toString();
    }

    /**
     * Returns the lock object, creating it if needed.
     */
    private ReentrantLock getLock() {
        ReentrantLock lock = this.lock;
        if (lock == null) {
            lock = new ReentrantLock();
            if (!UNSAFE.compareAndSwapObject(this, LOCK, null, lock)) {
                lock = this.lock;
            }
        }
        return lock;
    }

    /**
     * Returns the condition object for signalling, creating it if needed.
     */
    private Condition getCondition() {
        assert getLock().isHeldByCurrentThread();
        Condition condition = this.condition;
        if (condition == null) {
            this.condition = condition = lock.newCondition();
        }
        return condition;
    }

    // -- stack trace support --

    // private static final StackWalker STACK_WALKER = StackWalker.getInstance(VTHREAD_SCOPE);
    private static final StackTraceElement[] EMPTY_STACK = new StackTraceElement[0];

    @Override
    public StackTraceElement[] getStackTrace() {
        if (Thread.currentThread() == this) {
            return virtualThreadStackTrace((new Exception()).getStackTrace());
        } else {
            SecurityManager security = System.getSecurityManager();
            if (security != null) {
                security.checkPermission(SecurityConstants.GET_STACK_TRACE_PERMISSION);
            }

            // target virtual thread may be mounted or unmounted
            StackTraceElement[] stackTrace;
            do {
                Thread carrier = carrierThread;
                if (carrier != null) {
                    // mounted
                    stackTrace = tryGetStackTrace(carrier);
                } else {
                    // not mounted
                    stackTrace = tryGetStackTrace();
                }
                if (stackTrace == null) {
                    Thread.yield();
                }
            } while (stackTrace == null);
            return stackTrace;
        }
    }

    /**
     * Returns the stack trace for this virtual thread if it mounted on the given carrier
     * thread. If the virtual thread parks or is re-scheduled to another thread then
     * null is returned.
     */
    private StackTraceElement[] tryGetStackTrace(Thread carrier) {
        assert carrier != Thread.currentCarrierThread();

        StackTraceElement[] stackTrace = null;
        carrier.suspend();
        try {
            // get stack trace if virtual thread is still mounted on the suspended
            // carrier thread. Skip if the virtual thread is parking as the
            // continuation frames may or may not be on the thread stack.
            // FIXME: Thread.yield may also need special handling.
            if (carrierThread == carrier && state() != PARKING) {
                stackTrace = carrier.getStackTrace();
            }
        } finally {
            carrier.resume();
        }

        if (stackTrace != null) {
            return virtualThreadStackTrace(stackTrace);
        } else {
            return null;
        }
    }

    /**
     * Returns the stack trace for this virtual thread if it newly created,
     * parked, or terminated. Returns null if the thread is in other states.
     */
    private StackTraceElement[] tryGetStackTrace() {
        if (compareAndSetState(PARKED, PARKED_SUSPENDED)) {
            try {
                return virtualThreadStackTrace(cont.getStackTrace());
            } finally {
                assert state == PARKED_SUSPENDED;
                setState(PARKED);

                // may have been unparked while suspended
                if (parkPermit == PARK_PERMIT_TRUE && compareAndSetState(PARKED, RUNNABLE)) {
                    try {
                        ScheduleContinuation();
                    } catch (RejectedExecutionException ignore) { }
                }
            }
        } else {
            int state = state();
            if (state == NEW || state == TERMINATED) {
                return EMPTY_STACK;
            } else {
                return null;
            }
        }
    }

    /**
     * Returns the stack trace for the virtual thread. Returns the empty stack trace
     * if the runContinuation frame is not found.
     */
    static StackTraceElement[] virtualThreadStackTrace(StackTraceElement[] stackTrace) {
        int runMethod = findRunContinuation(stackTrace);
        if (runMethod >= 0) {
            // skip Continuation.run and Continuation.enterSpecial frames
            return Arrays.copyOf(stackTrace, runMethod);
        } else {
            return EMPTY_STACK;
        }
    }

    /**
     * Returns the stack trace for the carrier thread.
     */
    static StackTraceElement[] carrierThreadStackTrace(StackTraceElement[] stackTrace) {
        /*int runMethod = findRunContinuation(stackTrace);
        if (runMethod >= 0) {
            return Arrays.copyOfRange(stackTrace, runMethod, stackTrace.length);
        } else {
            return stackTrace;
        }*/
        return null;
    }

    /**
     * Returns index of the VirtualThread.runContinuation frame or -1 if not found.
     */
    private static int findRunContinuation(StackTraceElement[] stackTrace) {
        int index = 0;
        while (index < stackTrace.length) {
            StackTraceElement e = stackTrace[index];
            if ("java.lang.VirtualThread".equals(e.getClassName())
                    && "lambda$new$0".equals(e.getMethodName())) {
                return index;
            } else {
                index++;
            }
        }
        return -1;
    }

    // -- wrappers for VarHandle methods --

    private int state() {
        return state;  // volatile read
    }

    private void setState(int newValue) {
        state = newValue;  // volatile write
    }

    private void setReleaseState(int newValue) {
        UNSAFE.putIntVolatile(this, STATE, newValue);
    }

    private boolean compareAndSetState(int expectedValue, int newValue) {
        return UNSAFE.compareAndSwapInt(this, STATE, expectedValue, newValue);
    }

    private void setParkPermit(int newValue) {
        if (parkPermit != newValue) {
            parkPermit = newValue;
        }
    }

    private int getAndSetParkPermit(int newValue) {
        if (parkPermit != newValue) {
            return UNSAFE.getAndSetInt(this, PARK_PERMIT, newValue);
        } else {
            return newValue;
        }
    }

    // -- JVM TI support --

    /*private static volatile boolean notifyJvmtiEvents;  // set by VM
    private static native void notifyStarted(Thread carrierThread, VirtualThread vthread);
    private static native void notifyTerminated(Thread carrierThread, VirtualThread vthread);
    private static native void notifyMount(Thread carrierThread, VirtualThread vthread);
    private static native void notifyUnmount(Thread carrierThread, VirtualThread vthread);
    private static native void registerNatives();
    static {
        registerNatives();
    }*/


    static Executor defaultScheduler() {
        return DEFAULT_SCHEDULER;
    }
    /**
     * Creates the default scheduler as ForkJoinPool.
     */
    private static Executor createDefaultScheduler() {
        ForkJoinWorkerThreadFactory factory = pool -> {
            PrivilegedAction<ForkJoinWorkerThread> pa = () -> new CarrierThread(pool);
            return AccessController.doPrivileged(pa);
        };
        PrivilegedAction<Executor> pa = () -> {
            int parallelism;
            String propValue = System.getProperty("jdk.defaultScheduler.parallelism");
            if (propValue != null) {
                parallelism = Integer.parseInt(propValue);
            } else {
                parallelism = Runtime.getRuntime().availableProcessors();
            }
            Thread.UncaughtExceptionHandler ueh = (t, e) -> { };
            // use FIFO as default
            propValue = System.getProperty("jdk.defaultScheduler.lifo");
            boolean asyncMode = (propValue == null) || propValue.equalsIgnoreCase("false");
            return new ForkJoinPool(parallelism, factory, ueh, asyncMode);
        };
        return AccessController.doPrivileged(pa);
    }

    /**
     * A thread in the ForkJoinPool created by the default scheduler.
     */
    private static class CarrierThread extends ForkJoinWorkerThread {
        private static final ThreadGroup CARRIER_THREADGROUP = carrierThreadGroup();
        private static final AccessControlContext INNOCUOUS_ACC = innocuousACC();

        private static final Unsafe UNSAFE;
        private static final long CONTEXTCLASSLOADER;
        private static final long INHERITABLETHREADLOCALS;
        private static final long INHERITEDACCESSCONTROLCONTEXT;

        CarrierThread(ForkJoinPool pool) {
            super(CARRIER_THREADGROUP, pool);
            UNSAFE.putObject(this, CONTEXTCLASSLOADER, ClassLoader.getSystemClassLoader());
            UNSAFE.putObject(this, INHERITABLETHREADLOCALS, null);
            UNSAFE.putObject(this, INHERITEDACCESSCONTROLCONTEXT, INNOCUOUS_ACC);
        }

        @Override
        public void setUncaughtExceptionHandler(UncaughtExceptionHandler ueh) { }

        @Override
        public void setContextClassLoader(ClassLoader cl) {
            throw new SecurityException("setContextClassLoader");
        }

        /**
         * The thread group for the carrier threads.
         */
        private static final ThreadGroup carrierThreadGroup() {
            return AccessController.doPrivileged(new PrivilegedAction<ThreadGroup>() {
                public ThreadGroup run() {
                    ThreadGroup group = Thread.currentCarrierThread().getThreadGroup();
                    for (ThreadGroup p; (p = group.getParent()) != null; )
                        group = p;
                    return new ThreadGroup(group, "CarrierThreads");
                }
            });
        }

        /**
         * Return an AccessControlContext that doesn't support any permissions.
         */
        private static AccessControlContext innocuousACC() {
            return new AccessControlContext(new ProtectionDomain[] {
                    new ProtectionDomain(null, null)
            });
        }

        static {
            try {
                UNSAFE = Unsafe.getUnsafe();
                CONTEXTCLASSLOADER = UNSAFE.objectFieldOffset(Thread.class.getDeclaredField(
                    "contextClassLoader"));
                INHERITABLETHREADLOCALS = UNSAFE.objectFieldOffset(Thread.class.getDeclaredField(
                    "inheritableThreadLocals"));
                INHERITEDACCESSCONTROLCONTEXT = UNSAFE.objectFieldOffset(Thread.class.getDeclaredField(
                    "inheritedAccessControlContext"));
            } catch (Exception e) {
                throw new InternalError(e);
            }
        }
    }

    /**
     * Creates the ScheduledThreadPoolExecutor used to schedule unparking.
     */
    private static ScheduledExecutorService delayedTaskScheduler() {
        ScheduledThreadPoolExecutor stpe = (ScheduledThreadPoolExecutor)
            Executors.newScheduledThreadPool(1, task -> {
                Thread thread = InnocuousThread.newSystemThread("VirtualThread-unparker", task);
                thread.setDaemon(true);
                return thread;
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
        static synchronized void printStackTrace() {
            assert !(Thread.currentThread() instanceof VirtualThread);

            System.out.println(Thread.currentThread());
            StackTraceElement[] ste = virtualThreadStackTrace((new Exception()).getStackTrace());
            boolean afterYield = false;

            for (int i = 0; i < ste.length; i++) {
                if (afterYield) {
                    System.out.format("    %s%n", ste[i]);
                } else if ("java.lang.Continuation".equals(ste[i].getClassName())
                           && "yield".equals(ste[i].getMethodName())) {
                    afterYield = true;
                }
            }
        }
    }

    /**
     * Reads the value of the jdk.tracePinning property to determine if stack
     * traces should be printed when a carrier thread is pinned when a virtual thread
     * attempts to park.
     */
    private static int tracePinningMode() {
        String propValue = GetPropertyAction.privilegedGetProperty("jdk.tracePinnedThreads");
        if (propValue != null) {
            if (propValue.length() == 0 || "full".equalsIgnoreCase(propValue))
                return 1;
            if ("short".equalsIgnoreCase(propValue))
                return 2;
        }
        return 0;
    }

    /**
     * Only used in package
     */
    Continuation Cont() {
        return cont;
    }

    private final void ScheduleContinuation() {
        VirtualThread vt = null;
        Thread carrier = null;
        if (isThreadPoolExecutor) {
            carrier = Thread.currentCarrierThread();
            vt = carrier.getVirtualThread();
            if (vt != null) {
                carrier.setVirtualThread(null);
            }
        }
        scheduler.execute(runContinuation);
        if (vt != null) {
            carrier.setVirtualThread(vt);
        }
    }
}
