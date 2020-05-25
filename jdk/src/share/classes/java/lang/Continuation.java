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
import java.dyn.*;

/**
 * Continuation implemenation use JKU coroutine runtime.
 * Continuation main interface is run/yield/try force yield.
 * Run method start/continue this continuation. Static method yield give up
 * running current continuation. Try force yield will try to yield continuation
 * at next safepoint.
 */
public class Continuation extends CoroutineBase {
    private static final Unsafe unsafe = Unsafe.getUnsafe();

    private static final boolean TRACE = sun.misc.VM.getSavedProperty("java.lang.Continuation.trace") != null;
    private static final boolean DEBUG = TRACE | sun.misc.VM.getSavedProperty("java.lang.Continuation.debug") != null;
    private static final long mountedOffset;

    static {
        try {
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
     *
     * Continuation scope is used for faster yield to caller continuation.
     * For example:
     * Continuation parent(parent_scope), child(child_scope)
     * When block operations happend, developer can choose yield child, if parent can continue.
     * If parent cannot continue while child continuation is blocking, direclty yield both child and parent.
     */
    private final ContinuationScope scope;
    private Continuation parent; // null for native stack
    private Continuation child; // non-null when we're yielded in a child continuation
    private CoroutineBase caller;  // could be thread coroutine or parent continuation

    private boolean done;
    private volatile int mounted = 0;  // 1 means true

    private short cs; // critical section semaphore
    private Object[] scopedCache;   // similar continuation level thread local, but no weak reference, not used now

    protected final void runTarget() {
        try {
            target.run();
        } finally {
            done = true;
        }
    }
    /**
     * Construct a Continuation
     * @param scope TBD
     * @param target TBD
     */
    public Continuation(ContinuationScope scope, Runnable target) {
        this(scope, -1, target);
    }
    
    /**
     * TBD
     * @param scope TBD
     * @param target TBD
     * @param stackSize in bytes
     */
    public Continuation(ContinuationScope scope, int stackSize, Runnable target) {
        super();
        this.scope = scope;
        this.target = target;
        currentCarrierThread().getCoroutineSupport().addContinuation("cont" ,this, stackSize);
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
        Continuation cont = currentCarrierThread().getContinuation();
        while (cont != null && cont.scope != scope) {
            cont = cont.parent;
        }
        return cont;
    }

    public static Continuation getCurrentContinuation() {
        Continuation cont = currentCarrierThread().getContinuation();
        return cont;
    }
    
    public CoroutineBase getCaller() {
        return caller;
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
        if (!compareAndSetMounted(0, 1)) // why atomic? Avoid invoke run in two thread at same time?
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
     * start/continue execute a continuation
     * 1. no parent continuation, switch from thread coroutine to continuation
     * 2. has parent and old parent is null, set parent and switch from parent to this
     * 3. TODO: previously yield to parent, need siwtch to inner most child
     *    Example: ContA invoke ContB invoke ContC
     *             Yield ContC to ContA, ContB -> child->ContC
     *             when invoke ContB.run, need switch to ContC
     *             when invoke Contc.run, abort
     */
    public final void run() {
        while (true) {
            if (TRACE) System.out.println("\n++++++++++++++++++++++++++++++");

            mount();

            if (done)
                throw new IllegalStateException("Continuation terminated");
            
            assert caller == null : "Caller coroutine is not null when run";
            
            Thread t = currentCarrierThread();
            if (parent != null) {
                if (parent != t.getContinuation())
                    throw new IllegalStateException();
            } else {
                parent = t.getContinuation();
            }
            CoroutineSupport support = currentCarrierThread().getCoroutineSupport();
            caller = parent;
            if (caller == null) {
                caller = support.getCurrent();
            }
            assert caller != null : "Caller coroutine is null before run";
            t.setContinuation(this);
            try {
                if (TRACE) System.out.println("Continuation run before call");
                support.continuationSwtich(caller, this);
                if (TRACE) System.out.println("Continuation run after call");
            } catch (Throwable e) {
                done = true;
                e.printStackTrace();
            } finally {
                fence();
                assert t == currentCarrierThread() : "thread change";
                caller = null;
                t.setContinuation(this.parent);
                unmount();
                return;
            }
        }
    }
    
    // yield from current caller to target
    // if target is null, yield to thread coroutine
    private final boolean yield0(CoroutineBase target) {
        CoroutineSupport support = currentCarrierThread().getCoroutineSupport();
        assert target != null : "target is null in Continuation.yield0";
        caller = null;
        // System.out.println("before yield");
        support.continuationSwtich(this, target);
        // System.out.println("after yield");
        return true;
    }

    /**
     * TBD
     * 
     * @param scope The {@link ContinuationScope} to yield
     * @return {@code true} for success; {@code false} for failure
     * @throws IllegalStateException if not currently in the given {@code scope},
     */
    public static boolean yield() {
        Thread t = currentCarrierThread();
        Continuation cont = t.getContinuation();
        return cont.yield0(cont.caller);
    }

    /*
     * hierachy yield support
     * 1. find continuation match scope, if not found throw exception
     * 2. setup parent child relationship used in resume
     * 3. siwtch to target
     */
    public static boolean yield(ContinuationScope scope) {
        Continuation cont = currentCarrierThread().getContinuation();
        Continuation c;
        for (c = cont; c != null && c.scope != scope; c = c.parent)
            ;
        if (c == null)
            throw new IllegalStateException("Not in scope " + scope);
        
        assert cont == c : "nested scope NYI";
        /*
        Continuation cur = cont;
        while (cur != c) {
            Continuation parent = cur.parent;
            assert parent.child == null : "should not have child";
            parent.child = cur;
            cur = parent;
        }*/
        return cont.yield0(c.caller);
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

    private boolean fence() {
        unsafe.storeFence();
        return true;
    }

    private boolean compareAndSetMounted(int expectedValue, int newValue) {
       boolean res = unsafe.compareAndSwapInt(this, mountedOffset, expectedValue, newValue);
       return res;
     }

    private void setMounted(int newValue) {
        mounted = newValue;
    }

    private String id() {
        return Integer.toHexString(System.identityHashCode(this)) + " [" + currentCarrierThread().getId() + "]";
    }
}
