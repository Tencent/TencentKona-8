/*
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
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
import org.testng.annotations.Test;

public class VTInterruptTest {
    static int val = 0;
    @Test
    public static void test() throws Exception {
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
        Thread vt = Thread.ofVirtual().scheduler(executor).name("vt").unstarted(() -> {
            try {
                while (true) {
                    Thread.sleep(1000);
                }
            } catch (InterruptedException e) {
                val++;
            }
        });

        vt.start();
        while (vt.getState() != Thread.State.WAITING) {
            Thread.sleep(100);
        }
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

        Runnable target1 = new Runnable() {
            public void run() {
                try {
                    while (true) {
                        Thread.sleep(1000);
                    }
                } catch (InterruptedException e) {
                    val++;
                }
                val++;
            }
        };

        Thread[] vts = new Thread[40];
        ThreadFactory f = Thread.ofVirtual().scheduler(executor).name("multipleSleepInterrupt_", 0).factory();
        for (int i = 0; i < 40; i+=2) {
            vts[i] = f.newThread(target1);
            vts[i+1] = f.newThread(target);
        }
        for (int i = 0; i < 40; i++) {
            vts[i].start();
        }

        for (int i = 0; i < 40; i += 2) {
            while (vts[i].getState() != Thread.State.WAITING) {
                Thread.sleep(100);
            }
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

        Thread vt = Thread.ofVirtual().scheduler(executor).name("vt-0").unstarted(target);
        lock.lock();
        vt.start();
        while (vt.getState() != Thread.State.WAITING) {
            Thread.sleep(100);
        }
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

        Thread vt = Thread.ofVirtual().scheduler(executor).name("vt-0").unstarted(target);

        lock.lock();
        vt.start();
        while (vt.getState() != Thread.State.WAITING) {
            Thread.sleep(100);
        }

        vt.interrupt();
        lock.unlock();

        vt.join();

        assertEquals(val, 0);
        executor.shutdown();
    }

    // condition wait and interrupt
    static volatile boolean enter_lock = false;
    static void conditionInterruptTest() throws Exception {
        val = 0;
        enter_lock = false;
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Lock lock = new ReentrantLock();
        Condition cond = lock.newCondition();

        Runnable target = new Runnable() {
            public void run() {
                lock.lock();
                enter_lock = true;
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

        Thread vt = Thread.ofVirtual().scheduler(executor).name("vt-0").unstarted(target); 

        vt.start();
        while (enter_lock == false) {
            Thread.sleep(100);
        }

        vt.interrupt();
        vt.join();

        assertEquals(val, 1);
        executor.shutdown();
    }
}

