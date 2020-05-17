/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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
import sun.misc.Unsafe;
import java.dyn.AsymCoroutine;
import java.dyn.AsymRunnable;
/**
 * StackFul Continuation implementation
 */
public class Continuation {
    private static final Unsafe unsafe = Unsafe.getUnsafe();

    private static final boolean TRACE = sun.misc.VM.getSavedProperty("java.lang.Continuation.trace") != null;
    private static final boolean DEBUG = TRACE | sun.misc.VM.getSavedProperty("java.lang.Continuation.debug") != null;
	private static final long mountedOffset;

    static {
        try {
            //registerNatives();
            mountedOffset = unsafe.objectFieldOffset(Continuation.class.getDeclaredField("mounted"));
        } catch (Exception e) {
            throw new InternalError(e);
        }
    }
    /** Reason for pinning */
    public enum Pinned { 
        /** Native frame on stack */ NATIVE,
        /** Monitor held */          MONITOR,
        /** In critical section */   CRITICAL_SECTION
    }

    private static Pinned pinnedReason(int reason) {
        switch (reason) {
            case 1: return Pinned.CRITICAL_SECTION;
            case 2: return Pinned.NATIVE;
            case 3: return Pinned.MONITOR;
            default:
                throw new AssertionError("Unknown pinned reason: " + reason);
        }
    }

    private static Thread currentCarrierThread() {
        return Thread.currentCarrierThread();
    }

    private Runnable target;

    /* While the native JVM code is aware that every continuation has a scope, it is, for the most part,
     * oblivious to the continuation hierarchy. The only time this hierarchy is traversed in native code
     * is when a hierarchy of continuations is mounted on the native stack.
     */
    private final ContinuationScope scope;
    private final long runtimeContinuation;

    private Continuation parent; // null for native stack
    private boolean done;
    private volatile int mounted = 0;  // 1 means true

    private short cs; // critical section semaphore
    private Object[] scopedCache;

    // temp asym
    AsymCoroutine<Void, Void> coro;

    /**
     * Construct a Continuation
     * @param scope TBD
     * @param target TBD
     */
    public Continuation(ContinuationScope scope, Runnable target) {
        this.scope = scope;
        this.target = target;
        runtimeContinuation = 0;
        coro = new AsymCoroutine<Void, Void>("empty") { 
            protected Void run(Void value) {
                try {
                  if (TRACE) System.out.println("Coro run start"); 
                  target.run();
                } finally {
                  done = true;
                  if (TRACE) System.out.println("Coro run finish");
                }
                return null;
            }
        };
    }

    /**
     * TBD
     * @param scope TBD
     * @param target TBD
     * @param stackSize in bytes
     */
    public Continuation(ContinuationScope scope, int stackSize, Runnable target) {
        this(scope, target);
    }

    @Override
    public String toString() {
        return super.toString() + " scope: " + scope;
    }

    ContinuationScope getScope() {
        return scope;
    }

    Continuation getParent() {
        return parent;
    }

    /**
     * TBD
     * @param scope TBD
     * @return TBD
     */
    public static Continuation getCurrentContinuation(ContinuationScope scope) {
        // not suppose to use in first step
        return null;
        /*Continuation cont = currentCarrierThread().getContinuation();
        while (cont != null && cont.scope != scope)
            cont = cont.parent;
        return cont;*/
    }

    public static Continuation getCurrentContinuation() {
        Continuation cont = currentCarrierThread().getContinuation();
        return cont;
    }

    /**
     * TBD
     * @return TBD
     * @throws IllegalStateException if the continuation is mounted
     */
    public StackTraceElement[] getStackTrace() {
        return null;
    }

    private void mount() {
        if (!compareAndSetMounted(0, 1))
            throw new IllegalStateException("Mounted!!!!");
        Thread.setScopedCache(scopedCache);
    }

    private void unmount() {
        assert mounted == 1: "not mounted yet";
        scopedCache = Thread.scopedCache();
        Thread.setScopedCache(null);
        setMounted(0);
    }
    
    /**
     * TBD
     */
    public final void run() {
        while (true) {
            if (TRACE) System.out.println("\n++++++++++++++++++++++++++++++");

            mount();

            if (done)
                throw new IllegalStateException("Continuation terminated");

            Thread t = currentCarrierThread();
            if (parent != null) {
                if (parent != t.getContinuation())
                    throw new IllegalStateException();
            } else {
                parent = t.getContinuation();
            }
            t.setContinuation(this);
            try {
                if (TRACE) System.out.println("Continuation run before call");
                coro.call(); 
                if (TRACE) System.out.println("Continuation run after call");
            } catch (Throwable e) {
              done = true;
              e.printStackTrace();
            } finally {
                fence();
                assert t == currentCarrierThread() : "thread change";
                currentCarrierThread().setContinuation(this.parent);
                unmount();
                return;
            }
        }
    }

    /**
     * TBD
     * 
     * @param scope The {@link ContinuationScope} to yield
     * @return {@code true} for success; {@code false} for failure
     * @throws IllegalStateException if not currently in the given {@code scope},
     */
    public static boolean yield(/*ContinuationScope scope*/) {
        // if yield failed, call onPinned
        // if yield success, call onContinue
        Thread t = currentCarrierThread();
        Continuation cont = t.getContinuation();
        System.out.println("before yield");
        cont.coro.ret();
        System.out.println("resume yield");
        return false;
    }

    public static boolean yield(ContinuationScope scope) {
        // if yield failed, call onPinned
        // if yield success, call onContinue
        return yield();
    }
    private void onPinned0(int reason) {
        if (TRACE) System.out.println("PINNED " + this + " reason: " + reason);
        onPinned(pinnedReason(reason));
    }

    /**
     * TBD
     * @param reason TBD
     */
    protected void onPinned(Pinned reason) {
        if (DEBUG)
            System.out.println("PINNED! " + reason);
        throw new IllegalStateException("Pinned: " + reason);
    }

    /**
     * TBD
     */
    protected void onContinue() {
        if (TRACE)
            System.out.println("On continue");
    }

    /**
     * TBD
     * @return TBD
     */
    public boolean isDone() {
        return done;
    }

    /**
     * Pins the current continuation (enters a critical section).
     * This increments an internal semaphore that, when greater than 0, pins the continuation.
     */
    public static void pin() {
        Continuation cont = currentCarrierThread().getContinuation();
        if (cont != null) {
            assert cont.cs >= 0;
            if (cont.cs == Short.MAX_VALUE)
                throw new IllegalStateException("Too many pins");
            cont.cs++;
        }
    }

    /**
     * Unpins the current continuation (exits a critical section).
     * This decrements an internal semaphore that, when equal 0, unpins the current continuation
     * if pinne with {@link #pin()}.
     */
    public static void unpin() {
        Continuation cont = currentCarrierThread().getContinuation();
        if (cont != null) {
            assert cont.cs >= 0;
            if (cont.cs == 0)
                throw new IllegalStateException("Not pinned");
            cont.cs--;
        }
    }

    /**
     * Tests whether the given scope is pinned. 
     * This method is slow.
     * 
     * @param scope the continuation scope
     * @return {@code} true if we're in the give scope and are pinned; {@code false otherwise}
     */
    public static boolean isPinned(ContinuationScope scope) {
        int res = isPinned0(scope);
        return res != 0;
    }

    static private native int isPinned0(ContinuationScope scope);

    private void clean() {
        // if (!isStackEmpty())
        //     clean0();
    }

    private boolean fence() {
        unsafe.storeFence(); // needed to prevent certain transformations by the compiler
        return true;
    }

    private boolean compareAndSetMounted(int expectedValue, int newValue) {
       boolean res = unsafe.compareAndSwapInt(this, mountedOffset, expectedValue, newValue);
    //    System.out.println("-- compareAndSetMounted:  ex: " + expectedValue + " -> " + newValue + " " + res + " " + id());
       return res;
     }

    private void setMounted(int newValue) {
        // System.out.println("-- setMounted:  " + newValue + " " + id());
        mounted = newValue;
        // MOUNTED.setVolatile(this, newValue);
    }

    private String id() {
        return Integer.toHexString(System.identityHashCode(this)) + " [" + currentCarrierThread().getId() + "]";
    }

    // native methods
    // private static native void registerNatives();
    // private static native void swith(Continuation source, Continuation target);
}
