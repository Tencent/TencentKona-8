/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/**
 * @test
 * @summary Basic test for JFR jdk.VirtualThreadXXX events.
 * @run testng/othervm -XX:-YieldWithMonitor JfrEvents
 */

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.time.Duration;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.lang.management.ManagementFactory;

import jdk.jfr.EventType;
import jdk.jfr.Recording;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;

import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class JfrEvents {

    /**
     * Test jdk.VirtualThreadStart and jdk.VirtualThreadEnd events.
     */
    @Test
    public void testVirtualThreadStartAndEnd() throws Exception {
        try (Recording recording = new Recording()) {
            recording.enable("jdk.VirtualThreadStart");
            recording.enable("jdk.VirtualThreadEnd");

            // execute 100 tasks, each in their own virtual thread
            recording.start();
            ThreadFactory factory = Thread.ofVirtual().factory();
            ExecutorService executor = Executors.newFixedThreadPool(100, factory);
            try {
                for (int i = 0; i < 100; i++) {
                    executor.submit(() -> { });
                }
            } finally {
                executor.shutdown();
                executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
                // carrier thread (trigger after termination) might run slower than main thread
                // give time for carrier thread end events to be recorded
                Thread.sleep(2000);
                recording.stop();
            }

            Map<String, Integer> events = sumEvents(recording);
            System.out.println(events);

            int startCount = events.getOrDefault("jdk.VirtualThreadStart", 0);
            int endCount = events.getOrDefault("jdk.VirtualThreadEnd", 0);
            assertTrue(startCount == 100);
            assertTrue(endCount == 100);
        }
    }

    /**
     * Test jdk.VirtualThreadPinned event.
     */
    @Test
    public void testVirtualThreadPinned() throws Exception {
        try (Recording recording = new Recording()) {
            recording.enable("jdk.VirtualThreadPinned")
                     .withThreshold(Duration.ofMillis(500));

            // execute task in a virtual thread, carrier thread is pinned 3 times.
            recording.start();
            ThreadFactory factory = Thread.ofVirtual().factory();
            ExecutorService executor = Executors.newFixedThreadPool(1, factory);
            try {
                executor.submit(() -> {
                    Object lock = new Object();
                    synchronized (lock) {
                        // pinned, duration < 500ms
                        LockSupport.parkNanos(1);

                        // pinned, duration > 500ms
                        long nanos = Duration.ofSeconds(1).toNanos();
                        LockSupport.parkNanos(nanos);
                        LockSupport.parkNanos(nanos);
                    }
                });
            } finally {
                executor.shutdown();
                executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
                recording.stop();
            }

            Map<String, Integer> events = sumEvents(recording);
            System.out.println(events);

            // should have two pinned events recorded
            int pinnedCount = events.getOrDefault("jdk.VirtualThreadPinned", 0);
            assertTrue(pinnedCount == 2);
        }
    }

    /**
     * Test jdk.VirtualThreadSubmitFailed event.
     */
    @Test
    public void testVirtualThreadSubmitFailed() throws Exception {
        try (Recording recording = new Recording()) {
            recording.enable("jdk.VirtualThreadSubmitFailed");

            recording.start();
            ExecutorService pool = Executors.newFixedThreadPool(1);
            try {
                Executor scheduler = task -> pool.execute(task);
                ThreadFactory factory = Thread.ofVirtual().scheduler(scheduler).factory();

                // start a thread
                Thread thread = factory.newThread(LockSupport::park);
                thread.start();

                // give time for thread to park
                thread.join(500);
                assertTrue(thread.isAlive());

                // shutdown scheduler
                pool.shutdown();

                // unpark, the submit should fail
                try {
                    LockSupport.unpark(thread);
                    assertTrue(false);
                } catch (RejectedExecutionException expected) {
                }

                // start another thread, it should fail and an event should be recorded
                try {
                    factory.newThread(LockSupport::park).start();
                    throw new RuntimeException("RejectedExecutionException expected");
                } catch (RejectedExecutionException expected) {
                }
            } finally {
                pool.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
                recording.stop();
            }

            Map<String, Integer> events = sumEvents(recording);
            System.out.println(events);

            int count = events.getOrDefault("jdk.VirtualThreadSubmitFailed", 0);
            assertTrue(count == 2);
        }
    }

    /**
     * Read the events from the recording and return a map of event name to count.
     */
    private static Map<String, Integer> sumEvents(Recording recording) throws IOException {
        Path recordingFile = recordingFile(recording);
        List<RecordedEvent> events = RecordingFile.readAllEvents(recordingFile);
        return events.stream()
                .map(RecordedEvent::getEventType)
                .collect(Collectors.groupingBy(EventType::getName,
                                               Collectors.summingInt(x -> 1)));
    }

    /**
     * Return the file path to the recording file.
     */
    private static Path recordingFile(Recording recording) throws IOException {
        Path recordingFile = recording.getDestination();
        if (recordingFile == null) {
            File directory = new File(".");
            String pid = ManagementFactory.getRuntimeMXBean().getName().split("@")[0];
            recordingFile = new File(directory.getAbsolutePath(), "recording-" + recording.getId() + "-pid" + pid + ".jfr").toPath();
            recording.dump(recordingFile);
        }
        return recordingFile;
    }
}
