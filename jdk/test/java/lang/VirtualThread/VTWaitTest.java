/*
 * Copyright (C) 2020, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

/*
 * @test
 * @run testng VTWaitTest
 * @summary Virtual Thread wait test
 *   Conditional wait\CountDownLatch
 */
import java.util.concurrent.*;
import java.util.concurrent.locks.*;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class VTWaitTest {
    static int val = 0;
    @Test
    public static void test() throws Exception {
        SingleReentrantLockTest();
        System.out.println("finish SingleReentrantLockTest");
        SingleConditionTest();
        System.out.println("finish SingleConditionTest");
        MultiVThreadLockRace();
        System.out.println("finish MultiVThreadLockRace");
        MultiVTsAndThreadsLockRace();
        System.out.println("finish MultiVTsAndThreadsLockRace");
        MultiThreadCondRace();
        System.out.println("finish MultiThreadCondRace");
    }

    // 1. Init val = 0, Lock in main thread and sleep
    // 2. Execute in SingleThreadExecutor, Lock in VT1 cause yield
    // 3. Continue run VT2 and check value is still 0, update value to 1
    // 3. Unlock in main thread and unpark VT1, in VT1 check value is 1 update value to 2
    // 4. In main wait VTs join and value is 2
    static void SingleReentrantLockTest() throws Exception {
        val = 0;
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Lock lock = new ReentrantLock();

        Thread vt1 = Thread.ofVirtual().scheduler(executor).name("vt-1").unstarted(() -> {
            lock.lock();
            assertEquals(val, 1);
            val++;
            lock.unlock();
        });

        Thread vt2 = Thread.ofVirtual().scheduler(executor).name("vt-2").unstarted(() -> {
            assertEquals(val, 0);
            val++;
        });

        lock.lock();
        vt1.start();
        Thread.sleep(100);

        vt2.start();
        vt2.join();

        lock.unlock();
        vt1.join();

        assertEquals(val, 2);
        executor.shutdown();
    }

    // ReentrantLock test use Lock support
    // 1. Init val = 0, sleep 1s in main
    // 2. Execute in SingleThreadExecutor, lock.lock in VT1 and cond.wait, this should yield
    // 3. Continue run VT2 and check value is still 0, update value to 1
    // 3. Lock and Condition notify in main thread this will unpark VT1, in VT1 check value is 1 update value to 2
    // 4. In main wait VTs join and value is 2
    static volatile boolean enter_lock = false;
    static void SingleConditionTest() throws Exception {
        val = 0;
        enter_lock = false;
        Lock lock = new ReentrantLock();
        Condition cond = lock.newCondition();

        ExecutorService executor = Executors.newSingleThreadExecutor();
        Thread vt1 = Thread.ofVirtual().scheduler(executor).name("vt-1").unstarted(() -> {
            lock.lock();
            enter_lock = true;
            try {
                cond.await();
            } catch (InterruptedException e) {
                e.printStackTrace();
            } finally {
                lock.unlock();
            }
            assertEquals(val, 1);
            val++;
        });

        Thread vt2 = Thread.ofVirtual().scheduler(executor).name("vt-2").unstarted(() -> {
            assertEquals(val, 0);
            val++;
        });

        vt1.start();
        vt2.start();

        while (enter_lock == false) {
            Thread.sleep(100);
        }
        vt2.join();

        lock.lock();
        cond.signal();
        lock.unlock();

        vt1.join();

        assertEquals(val, 2);
        executor.shutdown();
    }

    // Complex lock1, start multiple VTs and race lock
    static void MultiVThreadLockRace() throws Exception {
        val = 0;
        ExecutorService executor = Executors.newFixedThreadPool(4);
        Lock lock = new ReentrantLock();
        Condition cond = lock.newCondition();

        Runnable target = new Runnable() {
            public void run() {
                lock.lock();
                System.out.println("before yield " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                Thread.yield();
                System.out.println("resume yield " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                val++;
                lock.unlock();
            }
        };

        Thread[] vts = new Thread[40];
        ThreadFactory f = Thread.ofVirtual().scheduler(executor).name("MultiVThreadLockRace_", 0).factory();
        for (int i = 0; i < 40; i++) {
            vts[i] = f.newThread(target);
        }
        for (int i = 0; i < 40; i++) {
            vts[i].start();
        }

        for (int i = 0; i < 40; i++) {
            vts[i].join();
        }

        assertEquals(val, 40);
        executor.shutdown();
    }

    // complex lock2, start multiple VTs and Java Thread, race lock
    static void MultiVTsAndThreadsLockRace() throws Exception {
        val = 0;
        Lock lock = new ReentrantLock();
        ExecutorService executor = Executors.newFixedThreadPool(4);

        Runnable target = new Runnable() {
            public void run() {
                lock.lock();
                System.out.println("before yield " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                Thread.yield();
                System.out.println("resume yield " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                val++;
                lock.unlock();
            }
        };

        Runnable thread_target = new Runnable() {
            public void run() {
                lock.lock();
                System.out.println("in thread : " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                val++;
                lock.unlock();
            }
        };

        Thread[] vts = new Thread[40];
        Thread[] threads = new Thread[40];
        ThreadFactory vf = Thread.ofVirtual().scheduler(executor).name("vt_", 0).factory();
        ThreadFactory tf = Thread.ofVirtual().name("thread_", 0).factory();
        for (int i = 0; i < 40; i++) {
            vts[i] = vf.newThread(target);
            threads[i] = tf.newThread(thread_target);
        }

        for (int i = 0; i < 40; i++) {
            vts[i].start();
            threads[i].start();
        }

        for (int i = 0; i < 40; i++) {
            vts[i].join();
            threads[i].join();
        }
        assertEquals(val, 80);
        executor.shutdown();
    }

    static CountDownLatch doneSignal;
    // complex condition, multiple VTs condition wait/notify, find some cases
    static void MultiThreadCondRace() throws Exception {
        doneSignal = new CountDownLatch(100);
        val = 0;
        Lock lock = new ReentrantLock();
        Condition cond = lock.newCondition();
        ExecutorService executor = Executors.newFixedThreadPool(4);

        Runnable target = new Runnable() {
            public void run() {
                lock.lock();
                try {
                    doneSignal.countDown();
                    cond.await();
                    val++;
                } catch (InterruptedException e) {
                    e.printStackTrace();
                } finally {
                    lock.unlock();
                }
                System.out.println("[condition] : " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
            }
        };

        Thread[] vts = new Thread[100];
        ThreadFactory f = Thread.ofVirtual().scheduler(executor).name("vt_", 0).factory();
        for (int i = 0; i < 100; i++) {
            vts[i] = f.newThread(target);
        }
        for (int i = 0; i < 100; i++) {
            vts[i].start();
        }

        doneSignal.await();

        for (int i = 0; i < 100; i++) {
            lock.lock();
            cond.signal();
            lock.unlock();
        }

        for (int i = 0; i < 100; i++) {
            vts[i].join();
        }

        assertEquals(val, 100);
        executor.shutdown();
    }
}
