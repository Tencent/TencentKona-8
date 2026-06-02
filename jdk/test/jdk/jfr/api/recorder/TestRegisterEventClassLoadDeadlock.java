/*
 * Copyright (C) 2026, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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
 */
package jdk.jfr.api.recorder;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.management.ManagementFactory;
import java.lang.management.MonitorInfo;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Duration;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import jdk.jfr.Event;
import jdk.jfr.FlightRecorder;
import jdk.jfr.Recording;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;
import jdk.jfr.internal.JVM;

/**
 * @test TestRegisterEventClassLoadDeadlock
 * @bug 8368670
 * @summary Registering an event while another thread loads (and eagerly
 *          instruments) a class it depends on must not deadlock.
 *
 * @key jfr
 *
 * @run main/othervm -XX:FlightRecorderOptions=retransform=true
 *      jdk.jfr.api.recorder.TestRegisterEventClassLoadDeadlock
 */
public class TestRegisterEventClassLoadDeadlock {

    private static final String PREFIX = TestRegisterEventClassLoadDeadlock.class.getName() + "$";
    private static final String X_EVENT = PREFIX + "XEvent";
    private static final String A_EVENT = PREFIX + "AEvent";
    private static final long TIMEOUT_MS = 30_000;
    private static final int A_VALUE = 123;
    private static final int V_VALUE = 456;

    // Event class that is loaded by thread B while it holds its class loading lock.
    public static class XEvent extends Event {
        public int v;
    }

    // Event class registered by thread A. Resolving its field type needs to load XEvent.
    public static class AEvent extends Event {
        public XEvent x;
        public int a;
    }

    // Parallel capable loader, so the class loading lock is per class name.
    // The event classes are defined here, not in the parent, so that loading
    // them goes through eager instrumentation.
    static class TestLoader extends ClassLoader {
        static {
            registerAsParallelCapable();
        }

        final CountDownLatch xLoading = new CountDownLatch(1);
        volatile Thread registerThread;

        TestLoader() {
            super(TestRegisterEventClassLoadDeadlock.class.getClassLoader());
        }

        @Override
        protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
            if (!name.startsWith(PREFIX)) {
                return super.loadClass(name, resolve);
            }
            synchronized (getClassLoadingLock(name)) {
                Class<?> c = findLoadedClass(name);
                if (c == null) {
                    c = findClass(name);
                }
                if (resolve) {
                    resolveClass(c);
                }
                return c;
            }
        }

        @Override
        protected Class<?> findClass(String name) throws ClassNotFoundException {
            byte[] b = readClassBytes(name);
            if (name.equals(X_EVENT)) {
                // We hold the class loading lock of XEvent. Wait until the
                // registering thread is blocked on that lock while holding the
                // JFR metadata lock, then define the class.
                xLoading.countDown();
                awaitRegistrarBlockedOnUs(registerThread);
            }
            return defineClass(name, b, 0, b.length);
        }

        private static byte[] readClassBytes(String name) throws ClassNotFoundException {
            String path = name.replace('.', '/') + ".class";
            try (InputStream in = TestLoader.class.getClassLoader().getResourceAsStream(path)) {
                if (in == null) {
                    throw new ClassNotFoundException(name);
                }
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                }
                return out.toByteArray();
            } catch (IOException e) {
                throw new ClassNotFoundException(name, e);
            }
        }

        // Wait until the registering thread (a) is in the state that makes the
        // deadlock possible: it holds the JFR metadata lock and is blocked on a
        // monitor owned by the current thread (b), i.e. XEvent's class loading
        // lock. If that state is not reached in time, the test fails explicitly,
        // since otherwise it could pass without exercising the scenario.
        private static void awaitRegistrarBlockedOnUs(Thread a) {
            ThreadMXBean mx = ManagementFactory.getThreadMXBean();
            long b = Thread.currentThread().getId();
            long end = System.currentTimeMillis() + TIMEOUT_MS;
            String last = "no thread info";
            while (System.currentTimeMillis() < end) {
                ThreadInfo ti = mx.getThreadInfo(new long[] { a.getId() }, true, false)[0];
                if (ti != null) {
                    boolean blocked = ti.getThreadState() == Thread.State.BLOCKED;
                    boolean ownedByUs = ti.getLockOwnerId() == b;
                    boolean holdsMetadata = holdsMetadataLock(ti);
                    if (blocked && ownedByUs && holdsMetadata) {
                        return;
                    }
                    last = "state=" + ti.getThreadState() + ", lock=" + ti.getLockName()
                            + ", lockOwnerId=" + ti.getLockOwnerId() + " (expected " + b + ")"
                            + ", holdsMetadataLock=" + holdsMetadata;
                }
                try {
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    fail("Interrupted while waiting for " + a.getName());
                }
            }
            fail("Timeout waiting for " + a.getName() + " to hold the JFR metadata lock and block on "
                    + "the class loading lock of " + X_EVENT + ": " + last);
        }

        private static boolean holdsMetadataLock(ThreadInfo ti) {
            for (MonitorInfo mi : ti.getLockedMonitors()) {
                if (mi.getClassName().equals("jdk.jfr.internal.MetadataRepository")) {
                    return true;
                }
            }
            return false;
        }
    }

    // The JFR shutdown hook needs the metadata lock too, so a normal exit would
    // hang in the deadlock case. Report the failure and halt instead.
    private static void fail(String message) {
        System.err.println("TEST FAILED: " + message);
        System.err.flush();
        Runtime.getRuntime().halt(1);
    }

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        // Eager instrumentation of event classes happens only while recording.
        // Enable the events before they are loaded, so that the initial
        // instrumentation is the one being tested, not a later retransform.
        Recording recording = new Recording();
        recording.enable(A_EVENT).withThreshold(Duration.ZERO);
        recording.enable(X_EVENT).withThreshold(Duration.ZERO);
        recording.start();

        // With retransforms disabled the JVM forces instrumentation of event
        // classes on load, which bypasses MetadataRepository.isEnabled() and
        // the code path under test. Fail instead of passing without coverage.
        if (!JVM.getJVM().getAllowedToDoEventRetransforms()) {
            recording.close();
            throw new RuntimeException("Event retransforms are not allowed, "
                    + "the code path under test is not exercised");
        }

        TestLoader loader = new TestLoader();
        Class<? extends Event> aEvent =
                (Class<? extends Event>) Class.forName(A_EVENT, false, loader);

        // Thread B: loads XEvent, holding XEvent's class loading lock. When the class
        // is defined, JFR is called back (JVMUpcalls.bytesForEagerInstrumentation),
        // which needs the metadata lock.
        final Throwable[] errB = new Throwable[1];
        Thread b = new Thread(() -> {
            try {
                loader.loadClass(X_EVENT);
            } catch (Throwable t) {
                errB[0] = t;
            }
        }, "TestLoadXEvent");
        b.setDaemon(true);

        // Thread A: registers AEvent, holding the metadata lock. Reading the
        // fields of AEvent needs to load XEvent, which needs XEvent's class
        // loading lock.
        final Throwable[] errA = new Throwable[1];
        Thread a = new Thread(() -> {
            try {
                FlightRecorder.register(aEvent);
            } catch (Throwable t) {
                errA[0] = t;
            }
        }, "TestRegisterAEvent");
        a.setDaemon(true);

        loader.registerThread = a;
        b.start();
        if (!loader.xLoading.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Timeout waiting for XEvent to start loading");
        }
        a.start();

        ThreadMXBean mx = ManagementFactory.getThreadMXBean();
        long end = System.currentTimeMillis() + TIMEOUT_MS;
        while (System.currentTimeMillis() < end && (a.isAlive() || b.isAlive())) {
            long[] ids = mx.findDeadlockedThreads();
            if (ids != null) {
                StringBuilder sb = new StringBuilder("Deadlock detected:\n");
                for (ThreadInfo ti : mx.getThreadInfo(ids)) {
                    sb.append("  ").append(ti.getThreadName())
                      .append(" waits for ").append(ti.getLockName())
                      .append(" held by ").append(ti.getLockOwnerName()).append('\n');
                }
                fail(sb.toString());
            }
            Thread.sleep(50);
        }
        if (a.isAlive() || b.isAlive()) {
            fail("Threads did not finish, A alive=" + a.isAlive()
                    + ", B alive=" + b.isAlive());
        }
        if (errA[0] != null) {
            throw new Exception("Registering AEvent failed", errA[0]);
        }
        if (errB[0] != null) {
            throw new Exception("Loading XEvent failed", errB[0]);
        }

        // The deadlock is gone, verify that the events still work.
        // Use the classes defined by the test loader, the parent loader
        // has another set of classes.
        Class<?> xEvent = loader.loadClass(X_EVENT);
        Event ae = (Event) aEvent.getDeclaredConstructor().newInstance();
        aEvent.getField("a").setInt(ae, A_VALUE);
        Event xe = (Event) xEvent.getDeclaredConstructor().newInstance();
        xEvent.getField("v").setInt(xe, V_VALUE);
        ae.commit();
        xe.commit();
        recording.stop();

        Path dump = Paths.get("dump.jfr");
        recording.dump(dump);
        recording.close();
        verifyEvents(dump);
        Files.deleteIfExists(dump);
    }

    private static void verifyEvents(Path file) throws IOException {
        int aCount = 0;
        int xCount = 0;
        List<RecordedEvent> events = RecordingFile.readAllEvents(file);
        for (RecordedEvent e : events) {
            String name = e.getEventType().getName();
            if (name.equals(A_EVENT)) {
                aCount++;
                if (e.getInt("a") != A_VALUE) {
                    throw new RuntimeException("Wrong value for AEvent.a: " + e.getInt("a"));
                }
            } else if (name.equals(X_EVENT)) {
                xCount++;
                if (e.getInt("v") != V_VALUE) {
                    throw new RuntimeException("Wrong value for XEvent.v: " + e.getInt("v"));
                }
            }
        }
        if (aCount != 1 || xCount != 1) {
            throw new RuntimeException("Expected one event of each kind, AEvent=" + aCount
                    + ", XEvent=" + xCount);
        }
    }
}