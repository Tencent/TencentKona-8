/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
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
 * @run testng ThreadExecutorTest
 * @summary Basic tests for Executors.newThreadExecutor
 */

import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.Phaser;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.stream.Collectors;

import org.testng.annotations.Test;
import static org.testng.Assert.*;

@Test
public class ThreadExecutorTest {

    /**
     * Test that a thread is created for each task.
     */
    public void testThreadPerTask() throws Exception {
        final int NUM_TASKS = 1000;
        AtomicInteger threadCount = new AtomicInteger();

        ThreadFactory factory1 = Thread.builder().virtual().factory();
        ThreadFactory factory2 = task -> {
            threadCount.addAndGet(1);
            return factory1.newThread(task);
        };

        ArrayList<Future<?>> results = new ArrayList<Future<?>>();
        //ExecutorService executor = Executors.newThreadExecutor(factory2);
        ExecutorService executor = Executors.newCachedThreadPool(factory2);
        /*try (executor)*/ {
            for (int i=0; i<NUM_TASKS; i++) {
                Future<?> result = executor.submit(() -> {
                    Thread.sleep(1000);
                    return null;
                });
                results.add(result);
            }
        }
        for (Future<?> result : results) {
            assertTrue(result.get() == null);
        }
        assertTrue(threadCount.get() == NUM_TASKS);
        executor.shutdown();
        executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
        assertTrue(executor.isTerminated());
    }

    //Tests that newVirtualThreadExecutor creates virtual threads
    public void testNewVirtualThreadExecutor() throws Exception {
        final int NUM_TASKS = 10;
        AtomicInteger virtualThreadCount = new AtomicInteger();
        ThreadFactory factory1 = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newCachedThreadPool(factory1);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            for (int i=0; i<NUM_TASKS; i++) {
                executor.submit(() -> {
                    if (Thread.currentThread().isVirtual()) {
                        virtualThreadCount.addAndGet(1);
                    }
                });
            }
        }
        executor.shutdown();
        executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
        assertTrue(virtualThreadCount.get() == NUM_TASKS);
    }

    //Test that shutdownNow stops executing tasks.
    public void testShutdownNow() {
        ThreadFactory factory = Thread.builder().virtual().daemon(true).factory();
        ExecutorService executor = Executors.newCachedThreadPool(factory);
        Future<?> result;
        try {
            result = executor.submit(() -> {
                Thread.sleep(Duration.ofDays(1).toMillis());
                return null;
            });
        } finally {
            executor.shutdownNow();
        }
        Throwable e = expectThrows(ExecutionException.class, result::get);
        assertTrue(e.getCause() instanceof InterruptedException);
    }

    //Test submit when the Executor is shutdown but not terminated.
    public void testSubmitAfterShutdown() {
        Phaser barrier = new Phaser(2);
        ThreadFactory factory = Thread.builder().virtual().daemon(true).factory();
        ExecutorService executor = Executors.newCachedThreadPool(factory);
        try {
            // submit task to prevent executor from terminating
            executor.submit(barrier::arriveAndAwaitAdvance);
            executor.shutdown();
            assertTrue(executor.isShutdown() && !executor.isTerminated());
            assertThrows(RejectedExecutionException.class, () -> executor.submit(() -> {}));
        } finally {
            // allow task to complete
            barrier.arriveAndAwaitAdvance();
        }
    }
    // Test submit when the Executor is terminated.
    public void testSubmitAfterTermination() {
        ThreadFactory factory = Thread.builder().virtual().daemon(true).factory();
        ExecutorService executor = Executors.newSingleThreadExecutor(factory);
        executor.shutdown();
        assertTrue(executor.isShutdown() && executor.isTerminated());
        assertThrows(RejectedExecutionException.class, () -> executor.submit(() -> {}));
    }
    //Test invokeAny where all tasks complete normally.
    public void testInvokeAnyCompleteNormally1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newSingleThreadExecutor(factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> "bar";
            List<Callable<String>> taskList = new ArrayList<>();
            taskList.add(task1);
            taskList.add(task2);
            String result = executor.invokeAny(taskList);
            assertTrue("foo".equals(result) || "bar".equals(result));
        }
    }

    // Test invokeAny where all tasks complete normally.
    public void testInvokeAnyCompleteNormally2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newSingleThreadExecutor(factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofMinutes(1).toMillis());
                return "bar";
            };
            List<Callable<String>> taskList = new ArrayList<>();
            taskList.add(task1);
            taskList.add(task2);
            String result = executor.invokeAny(taskList);
            assertTrue("foo".equals(result));
        }
    }

    // Test invokeAny where all tasks complete with exception.
    public void testInvokeAnyCompleteExceptionally1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            class FooException extends Exception { }
            Callable<String> task1 = () -> { throw new FooException(); };
            Callable<String> task2 = () -> { throw new FooException(); };
            try {
                List<Callable<String>> taskList = new ArrayList<>();
                taskList.add(task1);
                taskList.add(task2);
                executor.invokeAny(taskList);
                assertTrue(false);
            } catch (ExecutionException e) {
                Throwable cause = e.getCause();
                assertTrue(cause instanceof FooException);
            }
        }
    }

    // Test invokeAny where all tasks complete with exception.
    public void testInvokeAnyCompleteExceptionally2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            class FooException extends Exception { }
            Callable<String> task1 = () -> { throw new FooException(); };
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofSeconds(2).toMillis());
                throw new FooException();
            };
            try {
                List<Callable<String>> taskList = new ArrayList<>();
                taskList.add(task1);
                taskList.add(task2);
                executor.invokeAny(taskList);
                assertTrue(false);
            } catch (ExecutionException e) {
                Throwable cause = e.getCause();
                assertTrue(cause instanceof FooException);
            }
        }
    }

    // Test invokeAny where some, not all, tasks complete normally.
    public void testInvokeAnySomeCompleteNormally1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            class FooException extends Exception { }
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> { throw new FooException(); };
            String result = executor.invokeAny(Arrays.asList(task1, task2));
            assertTrue("foo".equals(result));
        }
    }
    // Test invokeAny where some, not all, tasks complete normally.
    public void testInvokeAnySomeCompleteNormally2() throws Exception {
       ThreadFactory factory = Thread.builder().virtual().factory();
       ExecutorService executor = Executors.newFixedThreadPool(2, factory);
       //try (var executor = Executors.newVirtualThreadExecutor())
       {
            class FooException extends Exception { }
            Callable<String> task1 = () -> {
                Thread.sleep(Duration.ofSeconds(2).toMillis());
                return "foo";
            };
            Callable<String> task2 = () -> { throw new FooException(); };
            String result = executor.invokeAny(Arrays.asList(task1, task2));
            assertTrue("foo".equals(result));
        }
    }

    // Test invokeAny where all tasks complete normally before timeout expires.
    public void testInvokeAnyWithTimeout1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> "bar";
            String result = executor.invokeAny(Arrays.asList(task1, task2), 1, TimeUnit.MINUTES);
            assertTrue("foo".equals(result) || "bar".equals(result));
        }
    }

    // Test invokeAny where timeout expires before any task completes.
    @Test(expectedExceptions = { TimeoutException.class })
    public void testInvokeAnyWithTimeout2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            Callable<String> task1 = () -> {
                Thread.sleep(Duration.ofMinutes(1).toMillis());
                return "foo";
            };
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofMinutes(2).toMillis());
                return "bar";
            };
            executor.invokeAny(Arrays.asList(task1, task2), 2, TimeUnit.SECONDS);
        }
    }

    // Test invokeAny where timeout expires after some tasks have completed
    @Test(expectedExceptions = { TimeoutException.class })
    public void testInvokeAnyWithTimeout3() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            class FooException extends Exception { }
            Callable<String> task1 = () -> { throw new FooException(); };
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofMinutes(2).toMillis());
                return "bar";
            };
            executor.invokeAny(Arrays.asList(task1, task2), 2, TimeUnit.SECONDS);
        }
    }

    // Test invokeAny cancels remaining tasks
    public void testInvokeAnyCancelRemaining() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            DelayedResult<String> task1 = new DelayedResult("foo", Duration.ofMillis(50));
            DelayedResult<String> task2 = new DelayedResult("bar", Duration.ofMinutes(1));
            String result = executor.invokeAny(Arrays.asList(task1, task2));
            assertTrue("foo".equals(result) && task1.isDone());
            while (!task2.isDone()) {
                Thread.sleep(Duration.ofMillis(100).toMillis());
            }
            assertTrue(task2.exception() instanceof InterruptedException);
        }
    }

    // Test invokeAny with interrupt status set.
    public void testInvokeAnyInterrupt1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        /*try (var executor = Executors.newVirtualThreadExecutor())*/ {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> "bar";
            Thread.currentThread().interrupt();
            try {
                executor.invokeAny(Arrays.asList(task1, task2));
                assertTrue(false);
            } catch (InterruptedException expected) {
                assertFalse(Thread.currentThread().isInterrupted());
            } finally {
                Thread.interrupted(); // clear interrupt
            }
        }
    }

    // Test interrupt with thread blocked in invokeAny.
    public void testInvokeAnyInterrupt2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> {
                Thread.sleep(Duration.ofMinutes(1).toMillis());
                return "foo";
            };
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofMinutes(2).toMillis());
                return "bar";
            };
            ScheduledInterrupter.schedule(Thread.currentThread(), 2000);
            try {
                executor.invokeAny(Arrays.asList(task1, task2));
                assertTrue(false);
            } catch (InterruptedException expected) {
                assertFalse(Thread.currentThread().isInterrupted());
            } finally {
                Thread.interrupted(); // clear interrupt
            }
        }
    }

    // Test invokeAny after ExecutorService has been shutdown.
    @Test(expectedExceptions = { RejectedExecutionException.class })
    public void testInvokeAnyAfterShutdown() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        executor.shutdown();

        Callable<String> task1 = () -> "foo";
        Callable<String> task2 = () -> "bar";
        executor.invokeAny(Arrays.asList(task1, task2));
    }

    // Test invokeAny with empty collection.
    @Test(expectedExceptions = { IllegalArgumentException.class })
    public void testInvokeAnyEmpty1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            executor.invokeAny(Arrays.asList());
        }
    }

    // Test invokeAny with empty collection.
    @Test(expectedExceptions = { IllegalArgumentException.class })
    public void testInvokeAnyEmpty2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            executor.invokeAny(Arrays.asList(), 1, TimeUnit.MINUTES);
        }
    }

    // Test invokeAny with null.
    @Test(expectedExceptions = { NullPointerException.class })
    public void testInvokeAnyNull1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            executor.invokeAny(null);
        }
    }

    // Test invokeAny with null element
    @Test(expectedExceptions = { NullPointerException.class })
    public void testInvokeAnyNull2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            List<Callable<String>> list = new ArrayList<>();
            list.add(() -> "foo");
            list.add(null);
            executor.invokeAny(null);
        }
    }

    // Test invokeAll where all tasks complete normally.
    public void testInvokeAll1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofSeconds(1).toMillis());
                return "bar";
            };

            List<Future<String>> list = executor.invokeAll(Arrays.asList(task1, task2));

            // list should have two elements, both should be done
            assertTrue(list.size() == 2);
            boolean notDone = list.stream().anyMatch(r -> !r.isDone());
            assertFalse(notDone);

            // check results
            //List<String> results = list.stream().map(Future::join).collect(Collectors.toList());
            List<String> results =  Arrays.asList(list.get(0).get(), list.get(1).get());
            assertEquals(results, Arrays.asList("foo", "bar"));
        }
    }

    // Test invokeAll where all tasks complete with exception.
    public void testInvokeAll2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            class FooException extends Exception { }
            class BarException extends Exception { }
            Callable<String> task1 = () -> { throw new FooException(); };
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofSeconds(1).toMillis());
                throw new BarException();
            };

            List<Future<String>> list = executor.invokeAll(Arrays.asList(task1, task2));

            // list should have two elements, both should be done
            assertTrue(list.size() == 2);
            boolean notDone = list.stream().anyMatch(r -> !r.isDone());
            assertFalse(notDone);

            // check results
            Throwable e1 = expectThrows(ExecutionException.class, () -> list.get(0).get());
            assertTrue(e1.getCause() instanceof FooException);
            Throwable e2 = expectThrows(ExecutionException.class, () -> list.get(1).get());
            assertTrue(e2.getCause() instanceof BarException);
        }
    }

    // Test invokeAll where all tasks complete normally before the timeout expires.
    public void testInvokeAll3() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofSeconds(1).toMillis());
                return "bar";
            };

            List<Future<String>> list = executor.invokeAll(Arrays.asList(task1, task2), 1, TimeUnit.DAYS);

            // list should have two elements, both should be done
            assertTrue(list.size() == 2);
            boolean notDone = list.stream().anyMatch(r -> !r.isDone());
            assertFalse(notDone);

            // check results
            //List<String> results = list.stream().map(Future::join).collect(Collectors.toList());
            List<String> results =  Arrays.asList(list.get(0).get(), list.get(1).get());
            assertEquals(results, Arrays.asList("foo", "bar"));
        }
    }

    // Test invokeAll where some tasks do not complete before the timeout expires.
    public void testInvokeAll4() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            AtomicReference<Exception> exc = new AtomicReference<>();
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> {
                try {
                    Thread.sleep(Duration.ofDays(1).toMillis());
                    return "bar";
                } catch (Exception e) {
                    exc.set(e);
                    throw e;
                }
            };

            List<Future<String>> list = executor.invokeAll(Arrays.asList(task1, task2), 3, TimeUnit.SECONDS);

            // list should have two elements, both should be done
            assertTrue(list.size() == 2);
            boolean notDone = list.stream().anyMatch(r -> !r.isDone());
            assertFalse(notDone);

            // check results
            assertEquals(list.get(0).get(), "foo");
            assertTrue(list.get(1).isCancelled());

            // task2 should be interrupted
            Exception e;
            while ((e = exc.get()) == null) {
                Thread.sleep(50);
            }
            assertTrue(e instanceof InterruptedException);
        }
    }

    // Test invokeAll with cancelOnException=true, last task should be cancelled
    // There is no cancelOnException method in JDK8
    // default <T> List<Future<T>> invokeAll(Collection<? extends Callable<T>> tasks,
    //                                      boolean cancelOnException)
    /*public void testInvokeAll5() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(3, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            class BarException extends Exception { }
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofSeconds(3).toMillis());
                throw new BarException();
            };
            Callable<String> task3 = () -> {
                Thread.sleep(Duration.ofDays(1).toMillis());
                return "baz";
            };

            List<Future<String>> list = executor.invokeAll(Arrays.asList(task1, task2, task3), true);

            // list should have three elements, all should be done
            assertTrue(list.size() == 3);
            boolean notDone = list.stream().anyMatch(r -> !r.isDone());
            assertFalse(notDone);

            // task1 should have a result
            String s = list.get(0).get();
            assertTrue("foo".equals(s));

            // tasl2 should have failed with an exception
            Throwable e2 = expectThrows(ExecutionException.class, () -> list.get(1).get());
            assertTrue(e2.getCause() instanceof BarException);

            // task3 should be cancelled
            expectThrows(CancellationException.class, () -> list.get(2).get());
        }
    }

    // Test invokeAll with cancelOnException=true, first task should be cancelled
    public void testInvokeAll6() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(3, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            class BarException extends Exception { }
            Callable<String> task1 = () -> {
                Thread.sleep(Duration.ofDays(1).toMillis());
                return "foo";
            };
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofSeconds(3).toMillis());
                throw new BarException();
            };
            Callable<String> task3 = () -> "baz";

            List<Future<String>> list = executor.invokeAll(Arrays.asList(task1, task2, task3), true);

            // list should have three elements, all should be done
            assertTrue(list.size() == 3);
            boolean notDone = list.stream().anyMatch(r -> !r.isDone());
            assertFalse(notDone);

            // task1 should be cancelled
            expectThrows(CancellationException.class, () -> list.get(0).get());

            // tasl2 should have failed with an exception
            Throwable e2 = expectThrows(ExecutionException.class, () -> list.get(1).get());
            assertTrue(e2.getCause() instanceof BarException);

            // task3 should have a result
            String s = list.get(2).get();
            assertTrue("baz".equals(s));
        }
    }*/

    // Test invokeAll with interrupt status set.
    public void testInvokeAllInterrupt1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofMinutes(1).toMillis());
                return "bar";
            };

            Thread.currentThread().interrupt();
            try {
                executor.invokeAll(Arrays.asList(task1, task2));
                assertTrue(false);
            } catch (InterruptedException expected) {
                assertFalse(Thread.currentThread().isInterrupted());
            } finally {
                Thread.interrupted(); // clear interrupt
            }
        }
    }

    public void testInvokeAllInterrupt2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofMinutes(1).toMillis());
                return "bar";
            };

            Thread.currentThread().interrupt();
            try {
                executor.invokeAll(Arrays.asList(task1, task2), 1, TimeUnit.SECONDS);
                assertTrue(false);
            } catch (InterruptedException expected) {
                assertFalse(Thread.currentThread().isInterrupted());
            } finally {
                Thread.interrupted(); // clear interrupt
            }
        }
    }

    // no cacelationOnException method
    /* public void testInvokeAllInterrupt3() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> "foo";
            Callable<String> task2 = () -> {
                Thread.sleep(Duration.ofMinutes(1).toMillis());
                return "bar";
            };

            Thread.currentThread().interrupt();
            try {
                executor.invokeAll(Arrays.asList(task1, task2), true);
                assertTrue(false);
            } catch (InterruptedException expected) {
                assertFalse(Thread.currentThread().isInterrupted());
            } finally {
                Thread.interrupted(); // clear interrupt
            }
        }
    }*/

    // Test interrupt with thread blocked in invokeAll
    public void testInvokeAllInterrupt4() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> "foo";
            DelayedResult<String> task2 = new DelayedResult("bar", Duration.ofMinutes(1));
            ScheduledInterrupter.schedule(Thread.currentThread(), 2000);
            try {
                executor.invokeAll(Arrays.asList(task1, task2));
                assertTrue(false);
            } catch (InterruptedException expected) {
                assertFalse(Thread.currentThread().isInterrupted());

                // task2 should have been interrupted
                while (!task2.isDone()) {
                    Thread.sleep(Duration.ofMillis(100).toMillis());
                }
                assertTrue(task2.exception() instanceof InterruptedException);
            } finally {
                Thread.interrupted(); // clear interrupt
            }
        }
    }

    // Test interrupt with thread blocked in invokeAll with timeout
    public void testInvokeAllInterrupt5() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> "foo";
            DelayedResult<String> task2 = new DelayedResult("bar", Duration.ofMinutes(1));
            ScheduledInterrupter.schedule(Thread.currentThread(), 2000);
            try {
                executor.invokeAll(Arrays.asList(task1, task2), 1, TimeUnit.DAYS);
                assertTrue(false);
            } catch (InterruptedException expected) {
                assertFalse(Thread.currentThread().isInterrupted());

                // task2 should have been interrupted
                while (!task2.isDone()) {
                    Thread.sleep(Duration.ofMillis(100).toMillis());
                }
                assertTrue(task2.exception() instanceof InterruptedException);
            } finally {
                Thread.interrupted(); // clear interrupt
            }
        }
    }

    // Test interrupt with thread blocked in invokeAll cancelOnException=true
    /*public void testInvokeAllInterrupt6() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task1 = () -> "foo";
            DelayedResult<String> task2 = new DelayedResult("bar", Duration.ofMinutes(1));
            ScheduledInterrupter.schedule(Thread.currentThread(), 2000);
            try {
                executor.invokeAll(Arrays.asList(task1, task2), true);
                assertTrue(false);
            } catch (InterruptedException expected) {
                assertFalse(Thread.currentThread().isInterrupted());

                // task2 should have been interrupted
                while (!task2.isDone()) {
                    Thread.sleep(Duration.ofMillis(100).toMillis());
                }
                assertTrue(task2.exception() instanceof InterruptedException);
            } finally {
                Thread.interrupted(); // clear interrupt
            }
        }
    }*/

    // Test invokeAll after ExecutorService has been shutdown.
    @Test(expectedExceptions = { RejectedExecutionException.class })
    public void testInvokeAllAfterShutdown1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        executor.shutdown();

        Callable<String> task1 = () -> "foo";
        Callable<String> task2 = () -> "bar";
        executor.invokeAll(Arrays.asList(task1, task2));
    }

    @Test(expectedExceptions = { RejectedExecutionException.class })
    public void testInvokeAllAfterShutdown2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        executor.shutdown();

        Callable<String> task1 = () -> "foo";
        Callable<String> task2 = () -> "bar";
        executor.invokeAll(Arrays.asList(task1, task2), 1, TimeUnit.SECONDS);
    }

    /*@Test(expectedExceptions = { RejectedExecutionException.class })
    public void testInvokeAllAfterShutdown3() throws Exception {
        var executor = Executors.newVirtualThreadExecutor();
        executor.shutdown();

        Callable<String> task1 = () -> "foo";
        Callable<String> task2 = () -> "bar";
        executor.invokeAll(Arrays.asList(task1, task2), true);
    }*/

    // Test invokeAll with empty collection.
    public void testInvokeAllEmpty1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            List<Future<Object>> list = executor.invokeAll(Arrays.asList());
            assertTrue(list.size() == 0);
        }
    }

    public void testInvokeAllEmpty2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            List<Future<Object>> list = executor.invokeAll(Arrays.asList(), 1, TimeUnit.SECONDS);
            assertTrue(list.size() == 0);
        }
    }

    /*public void testInvokeAllEmpty3() throws Exception {
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            List<Future<Object>> list = executor.invokeAll(Arrays.asList(), true);
            assertTrue(list.size() == 0);
        }
    }*/

    @Test(expectedExceptions = { NullPointerException.class })
    public void testInvokeAllNull1() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            executor.invokeAll(null);
        }
    }

    @Test(expectedExceptions = { NullPointerException.class })
    public void testInvokeAllNull2() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            List<Callable<String>> tasks = new ArrayList<>();
            tasks.add(() -> "foo");
            tasks.add(null);
            executor.invokeAll(tasks);
        }
    }

    @Test(expectedExceptions = { NullPointerException.class })
    public void testInvokeAllNull3() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            executor.invokeAll(null, 1, TimeUnit.SECONDS);
        }
    }

    @Test(expectedExceptions = { NullPointerException.class })
    public void testInvokeAllNull4() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            Callable<String> task = () -> "foo";
            executor.invokeAll(Arrays.asList(task), 1, null);
        }
    }

    @Test(expectedExceptions = { NullPointerException.class })
    public void testInvokeAllNull5() throws Exception {
        ThreadFactory factory = Thread.builder().virtual().factory();
        ExecutorService executor = Executors.newFixedThreadPool(2, factory);
        // try (var executor = Executors.newVirtualThreadExecutor()) {
        {
            List<Callable<String>> tasks = new ArrayList<>();
            tasks.add(() -> "foo");
            tasks.add(null);
            executor.invokeAll(tasks, 1, TimeUnit.SECONDS);
        }
    }

    // -- supporting classes --

    static class DelayedResult<T> implements Callable<T> {
        final T result;
        final Duration delay;
        volatile boolean done;
        volatile Exception exception;
        DelayedResult(T result, Duration delay) {
            this.result = result;
            this.delay = delay;
        }
        public T call() throws Exception {
            try {
                Thread.sleep(delay.toMillis());
                return result;
            } catch (Exception e) {
                this.exception = e;
                throw e;
            } finally {
                done = true;
            }
        }
        boolean isDone() {
            return done;
        }
        Exception exception() {
            return exception;
        }
    }

    static class ScheduledInterrupter implements Runnable {
        private final Thread thread;
        private final long delay;

        ScheduledInterrupter(Thread thread, long delay) {
            this.thread = thread;
            this.delay = delay;
        }

        @Override
        public void run() {
            try {
                Thread.sleep(delay);
                thread.interrupt();
            } catch (Exception e) { }
        }

        static void schedule(Thread thread, long delay) {
            new Thread(new ScheduledInterrupter(thread, delay)).start();
        }
    }
}
