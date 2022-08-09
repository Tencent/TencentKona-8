/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
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

import java.lang.Thread.Builder;
import java.lang.Thread.Builder.OfPlatform;
import java.lang.Thread.Builder.OfVirtual;
import java.lang.Thread.UncaughtExceptionHandler;
import java.lang.invoke.MethodHandles;
import java.util.Objects;
import java.util.concurrent.Executor;
import java.util.concurrent.ThreadFactory;
import sun.misc.Unsafe;
import sun.security.action.GetPropertyAction;

/**
 * Defines static methods to create platform and virtual thread builders.
 */
class ThreadBuilders {

    /**
     * Base implementation of ThreadBuilder.
     */
    private static abstract
    class BaseThreadBuilder<T extends Builder> implements Builder {
        private String name;
        private long counter;
        private int characteristics;
        private UncaughtExceptionHandler uhe;

        String name() {
            return name;
        }

        long counter() {
            return counter;
        }

        int characteristics() {
            return characteristics;
        }

        UncaughtExceptionHandler uncaughtExceptionHandler() {
            return uhe;
        }

        String nextThreadName() {
            if (name != null && counter >= 0) {
                return name + (counter++);
            } else {
                return name;
            }
        }

        @Override
        @SuppressWarnings("unchecked")
        public T name(String name) {
            this.name = Objects.requireNonNull(name);
            this.counter = -1;
            return (T) this;
        }

        @Override
        @SuppressWarnings("unchecked")
        public T name(String prefix, long start) {
            Objects.requireNonNull(prefix);
            if (start < 0)
                throw new IllegalArgumentException("'start' is negative");
            this.name = prefix;
            this.counter = start;
            return (T) this;
        }

        @Override
        @SuppressWarnings("unchecked")
        public T allowSetThreadLocals(boolean allow) {
            if (allow) {
                characteristics &= ~Thread.NO_THREAD_LOCALS;
            } else {
                characteristics |= Thread.NO_THREAD_LOCALS;
            }
            return (T) this;
        }

        @Override
        @SuppressWarnings("unchecked")
        public T inheritInheritableThreadLocals(boolean inherit) {
            if (inherit) {
                characteristics &= ~Thread.NO_INHERIT_THREAD_LOCALS;
            } else {
                characteristics |= Thread.NO_INHERIT_THREAD_LOCALS;
            }
            return (T) this;
        }

        /*@Override
        @SuppressWarnings("unchecked")
        public T inheritInheritableScopeLocals(boolean inherit) {
            if (inherit) {
                characteristics &= ~Thread.NO_INHERIT_SCOPE_LOCALS;
            } else {
                characteristics |= Thread.NO_INHERIT_SCOPE_LOCALS;
            }
            return (T) this;
        }*/

        @Override
        @SuppressWarnings("unchecked")
        public T uncaughtExceptionHandler(UncaughtExceptionHandler ueh) {
            this.uhe = Objects.requireNonNull(ueh);
            return (T) this;
        }
    }

    /**
     * ThreadBuilder.OfPlatform implementation.
     */
    static class PlatformThreadBuilder
            extends BaseThreadBuilder<OfPlatform> implements OfPlatform {
        private ThreadGroup group;
        private boolean daemon;
        private boolean daemonChanged;
        private int priority;
        private long stackSize;

        @Override
        String nextThreadName() {
            String name = super.nextThreadName();
            return (name != null) ? name : Thread.nextThreadName();
        }

        @Override
        public OfPlatform group(ThreadGroup group) {
            this.group = Objects.requireNonNull(group);
            return this;
        }

        @Override
        public OfPlatform daemon(boolean on) {
            daemon = on;
            daemonChanged = true;
            return this;
        }

        @Override
        public OfPlatform priority(int priority) {
            if (priority < Thread.MIN_PRIORITY || priority > Thread.MAX_PRIORITY)
                throw new IllegalArgumentException();
            this.priority = priority;
            return this;
        }

        @Override
        public OfPlatform stackSize(long stackSize) {
            if (stackSize < 0L)
                throw new IllegalArgumentException();
            this.stackSize = stackSize;
            return this;
        }

        @Override
        public Thread unstarted(Runnable task) {
            /* Not support create thread with characteristics now */
            assert characteristics() == 0;

            Objects.requireNonNull(task);
            String name = nextThreadName();
            Thread thread = new Thread(group, task, name, stackSize);
            //thread.initializeThreadLocal(characteristics());
            if (daemonChanged)
                thread.setDaemon(daemon);
            if (priority != 0)
                thread.setPriority(priority);
            UncaughtExceptionHandler uhe = uncaughtExceptionHandler();
            if (uhe != null)
                thread.uncaughtExceptionHandler(uhe);
            return thread;
        }

        @Override
        public ThreadFactory factory() {
            return new PlatformThreadFactory(group, name(), counter(), characteristics(),
                    daemonChanged, daemon, priority, stackSize, uncaughtExceptionHandler());
        }

    }

    /**
     * ThreadBuilder.OfVirtual implementation.
     */
    static class VirtualThreadBuilder
            extends BaseThreadBuilder<OfVirtual> implements OfVirtual {
        private Executor scheduler;

        static final boolean ENABLE_VIRTUAL_THREAD = enableVirtualThread();
        static final boolean ENABLE_VT_SOCKET = enableVTSocket();

        private static boolean enableVirtualThread() {
            String propValue = GetPropertyAction.privilegedGetProperty("jdk.internal.VirtualThread");
            if (propValue != null) {
                if (propValue.length() == 0)
                    return true;
                if ("off".equalsIgnoreCase(propValue))
                    return false;
            }
            return true;
        }

        private static boolean enableVTSocket() {
            if (!GetPropertyAction.privilegedGetProperty("os.name").startsWith("Linux")) {
                return false;
            }
            String propValue = GetPropertyAction.privilegedGetProperty("jdk.internal.VTSocket");
            if (propValue != null) {
                if (propValue.length() == 0)
                    return true;
                if ("off".equalsIgnoreCase(propValue))
                    return false;
            }
            return true;
        }

        @Override
        public OfVirtual scheduler(Executor scheduler) {
            if (scheduler == null) {
                this.scheduler = VirtualThread.defaultScheduler();
            } else {
                this.scheduler = scheduler;
                if (!ENABLE_VIRTUAL_THREAD) {
                    System.out.println("[warning]: execute() can not be overwritten when disable virtual thread");
                }
            }
            return this;
        }

        @Override
        public Thread unstarted(Runnable task) {
            Objects.requireNonNull(task);

            Thread thread;
            if (ENABLE_VIRTUAL_THREAD)
                thread = new VirtualThread(scheduler, nextThreadName(), characteristics(), task);
            else
                thread = new Thread(task, nextThreadName(), characteristics());

            UncaughtExceptionHandler uhe = uncaughtExceptionHandler();
            if (uhe != null)
                thread.uncaughtExceptionHandler(uhe);
            return thread;
        }

        @Override
        public ThreadFactory factory() {
            return new VirtualThreadFactory(scheduler, name(), counter(), characteristics(),
                    uncaughtExceptionHandler());
        }
    }

    /**
     * Base ThreadFactory implementation.
     */
    private static abstract class BaseThreadFactory implements ThreadFactory {
        private static final long COUNT_OFFSET;
        private static final Unsafe unsafe = Unsafe.getUnsafe();
        static {
            try {
                Unsafe unsafe = Unsafe.getUnsafe();
                COUNT_OFFSET = unsafe.objectFieldOffset(BaseThreadFactory.class.getDeclaredField("count"));
            } catch (Exception e) {
                throw new InternalError(e);
            }
        }
        private final String name;
        private final int characteristics;
        private final UncaughtExceptionHandler uhe;

        private final boolean hasCounter;
        private volatile long count;

        BaseThreadFactory(String name,
                          long start,
                          int characteristics,
                          UncaughtExceptionHandler uhe)  {
            this.name = name;
            if (name != null && start >= 0) {
                this.hasCounter = true;
                this.count = start;
            } else {
                this.hasCounter = false;
            }
            this.characteristics = characteristics;
            this.uhe = uhe;
        }

        int characteristics() {
            return characteristics;
        }

        UncaughtExceptionHandler uncaughtExceptionHandler() {
            return uhe;
        }

        String nextThreadName() {
            if (hasCounter) {
                return name + (long)unsafe.getAndAddInt(this, COUNT_OFFSET, 1);
            } else {
                return name;
            }
        }
    }

    /**
     * ThreadFactory for platform threads.
     */
    private static class PlatformThreadFactory extends BaseThreadFactory {
        private final ThreadGroup group;
        private final boolean daemonChanged;
        private final boolean daemon;
        private final int priority;
        private final long stackSize;

        PlatformThreadFactory(ThreadGroup group,
                              String name,
                              long start,
                              int characteristics,
                              boolean daemonChanged,
                              boolean daemon,
                              int priority,
                              long stackSize,
                              UncaughtExceptionHandler uhe) {
            super(name, start, characteristics, uhe);
            this.group = group;
            this.daemonChanged = daemonChanged;
            this.daemon = daemon;
            this.priority = priority;
            this.stackSize = stackSize;
        }

        @Override
        String nextThreadName() {
            String name = super.nextThreadName();
            return (name != null) ? name : Thread.nextThreadName();
        }

        @Override
        public Thread newThread(Runnable task) {
            Objects.requireNonNull(task);
            if (characteristics() != 0) {
                throw new IllegalStateException("create thread with characteristics");
            }

            String name = nextThreadName();
            Thread thread = new Thread(group, task, name, stackSize);
            if (daemonChanged)
                thread.setDaemon(daemon);
            if (priority != 0)
                thread.setPriority(priority);
            UncaughtExceptionHandler uhe = uncaughtExceptionHandler();
            if (uhe != null)
                thread.uncaughtExceptionHandler(uhe);
            return thread;
        }
    }

    /**
     * ThreadFactory for virtual threads.
     */
    private static class VirtualThreadFactory extends BaseThreadFactory {
        private final Executor scheduler;

        VirtualThreadFactory(Executor scheduler,
                             String name,
                             long start,
                             int characteristics,
                             UncaughtExceptionHandler uhe) {
            super(name, start, characteristics, uhe);
            this.scheduler = scheduler;
        }

        @Override
        public Thread newThread(Runnable task) {
            Objects.requireNonNull(task);
            String name = nextThreadName();
            Thread thread;
            if (VirtualThreadBuilder.ENABLE_VIRTUAL_THREAD)
                thread = new VirtualThread(scheduler, name, characteristics(), task);
            else
                thread = new Thread(task, name, characteristics());
            UncaughtExceptionHandler uhe = uncaughtExceptionHandler();
            if (uhe != null)
                thread.uncaughtExceptionHandler(uhe);
            return thread;
        }
    }
}
