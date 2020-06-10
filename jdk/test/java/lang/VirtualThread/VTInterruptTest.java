/*
 * Copyright (c) 2003, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @run testng VTInterruptTest
 * @summary Virtual Thread interrupt test
 *   Conditional wait\CountDownLatch 
 */
import java.util.concurrent.*;
import java.util.concurrent.locks.*;
import static org.testng.Assert.*;

public class VTInterruptTest {
    static int val = 0;
    public static void main(String args[]) throws Exception {
        SingleSleepInterrupt();
        System.out.println("finish SingleSleepInterrupt");
        multipleSleepInterrupt();
        System.out.println("finish multipleSleepInterrupt");
        lockInterruptiblyTest();
        System.out.println("finish lockInterruptiblyTest");
        lockIgnoreInterruptTest();
        System.out.println("finish lockIgnoreInterruptTest");
        conditionInterruptTest();
        System.out.println("finish conditionInterruptTest");
    }

    // interrupt sleep, can catch interrupt exception
    static void SingleSleepInterrupt() throws Exception {
        val = 0;

        ExecutorService executor = Executors.newSingleThreadExecutor();
        VirtualThread vt = new VirtualThread(executor, "vt", 0, () -> {
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                val++;
            }
        });

        vt.start();
        Thread.sleep(100);
        vt.interrupt();
        vt.join();

        assertEquals(val, 1);
        executor.shutdown();
    }

    // run multiple VT on singleThreadExecutor
    // interrupt part threads will not affect others
    static void multipleSleepInterrupt() throws Exception {
        val = 0;
        ExecutorService executor = Executors.newSingleThreadExecutor();

        Runnable target = new Runnable() {
            public void run() {
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    val++;
                }
                val++;
            }
        };

        VirtualThread[] vts = new VirtualThread[40];
        for (int i = 0; i < 40; i++) {
            vts[i] = new VirtualThread(executor, "multipleSleepInterrupt_" + i, 0, target);
        }
        for (int i = 0; i < 40; i++) {
            vts[i].start();
        }

        Thread.sleep(100);
        for (int i = 0; i < 40; i += 2) {
            vts[i].interrupt();
        }

        for (int i = 0; i < 40; i++) {
            vts[i].join();
        }

        assertEquals(val, 60);
        executor.shutdown();
    }

    // lock lockInterruptibly and interrupt, catch expected exception
    static void lockInterruptiblyTest() throws Exception {
        val = 0;
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Lock lock = new ReentrantLock();

        Runnable target = new Runnable() {
            public void run() {
                try {
                    lock.lockInterruptibly();
                } catch (InterruptedException e) {
                    System.out.println("catch InterruptedException");
                    val++;
                }

                System.out.println("vt finish");
            }
        };

        VirtualThread vt = new VirtualThread(executor, "vt-0", 0, target);

        lock.lock();
        vt.start();
        Thread.sleep(100);
        vt.interrupt();

        vt.join();

        assertEquals(val, 1);
        executor.shutdown();
    }

    // lock.lock cat not interrupt, exception should not throw
    static void lockIgnoreInterruptTest() throws Exception {
        val = 0;
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Lock lock = new ReentrantLock();

        Runnable target = new Runnable() {
            public void run() {
                try {
                    lock.lock();
                } catch (Exception e) {
                    System.out.println("catch Exception");
                    val++;
                } finally {
                    lock.unlock();
                }

                System.out.println("vt finish");
            }
        };

        VirtualThread vt = new VirtualThread(executor, "vt-0", 0, target);

        lock.lock();
        vt.start();
        Thread.sleep(100);
        vt.interrupt();
        Thread.sleep(100);

        lock.unlock();
        assertEquals(val, 0);
        executor.shutdown();
    }
 
    // condition wait and interrupt
    static void conditionInterruptTest() throws Exception {
        val = 0;
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Lock lock = new ReentrantLock();
        Condition cond = lock.newCondition();

        Runnable target = new Runnable() {
            public void run() {
                lock.lock();
                try {
                    cond.await();
                } catch (InterruptedException e) {
                    System.out.println("catch InterruptedException");
                    val++;
                } finally {
                    lock.unlock();
                }

                System.out.println("vt finish");
            }
        };

        VirtualThread vt = new VirtualThread(executor, "vt-0", 0, target);

        vt.start();
        Thread.sleep(100);

        vt.interrupt();
        Thread.sleep(100);

        assertEquals(val, 1);
        executor.shutdown();
    }
}

