/*
 *
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
 * @run testng/othervm YieldWithSync
 * @summary test virtual thread yield while hold a synchronized lock
 */
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.LockSupport;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class YieldWithSync {
    static long vt_tag = 0;
    static long finish_thread_count = 0;

    volatile static boolean spin_flag1 = true;
    volatile static boolean spin_flag2 = true;

    @Test
    // 1. vt hold ObjectMonitor and park
    // 2. thread try to get ObjectMonitor(cause ObjectMonitor inflate)
    // 3. unpark vt, vt release ObjectMonitor
    // 4. wake up thread;
    public static void ObjectMonitorTest1() throws Exception {
        vt_tag = 0;
        finish_thread_count = 0;

        Object o = new Object();
        ExecutorService executor = Executors.newFixedThreadPool(1);

        Runnable vt_target = new Runnable() {
            public void run() {
                synchronized (o) {
                    vt_tag++;
                    LockSupport.park();
                    vt_tag++;
                }
            }
        };

        Runnable t_target = new Runnable() {
            public void run() {
                synchronized (o) {
                    System.out.println("hello VtHoldLock");
                    finish_thread_count++;
                }
            }
        };

        Thread vt = Thread.ofVirtual().scheduler(executor).name("vt").start(vt_target);
        while (vt.getState() != Thread.State.WAITING && vt.getState() != Thread.State.TIMED_WAITING) {
            System.out.println("vt.getState is " + vt.getState());
            Thread.sleep(100);
        }
        assertEquals(vt_tag, 1);

        Thread t = new Thread(t_target, "thread-for-inflate-monitor");
        t.start();
        Thread.sleep(1000);
        assertEquals(vt_tag, 1);
        assertEquals(finish_thread_count, 0);

        LockSupport.unpark(vt);
        t.join();
        vt.join();
        assertEquals(vt_tag, 2);
        assertEquals(finish_thread_count, 1);

        executor.shutdown();
        executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
    }

    @Test
    // different vt run on same carrier, and can not get same ObjectMonitor
    // 1. vt1 hold ObjectMonitor, vt2 inflate ObjectMonitor
    // 2. vt2 and vt3 run on same carrier thread, and vt3 can not get ObjectMonitor
    public static void ObjectMonitorTest2() throws Exception {
        vt_tag = 0;
        Object o = new YieldWithSync();
        spin_flag1 = true;
        spin_flag2 = true;

        ExecutorService executor = Executors.newFixedThreadPool(2);

        Runnable target1 = new Runnable() {
            public void run() {
                synchronized (o) {
                    vt_tag++;
                    while (spin_flag1);
                }

                while (spin_flag2);
            }
        };

        Runnable target2 = new Runnable() {
            public void run() {
                synchronized (o) {
                    vt_tag++;
                    LockSupport.park();
                }
            }
        };

        ThreadFactory vf = Thread.ofVirtual().scheduler(executor).name("vt_", 0).factory();
        Thread vt1 = vf.newThread(target1);
        vt1.start();
        while (vt_tag != 1) {
            Thread.sleep(100);
        }

        Thread vt2 = vf.newThread(target2);
        vt2.start();
        spin_flag1 = false;

        while (vt_tag != 2) {
            Thread.sleep(100);
        }

        Thread vt3 = vf.newThread(target2);
        vt3.start();
        Thread.sleep(1000);
        assertEquals(vt_tag, 2);

        LockSupport.unpark(vt2);
        LockSupport.unpark(vt3);
        spin_flag2 = false;

        vt1.join();
        vt2.join();
        vt3.join();
        assertEquals(vt_tag, 3);

        executor.shutdown();
        executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
    }

    @Test
    // single vt alloc same ObjectMonitor(inflated) twice;
    public static void ObjectMonitorTest3() throws Exception {
        vt_tag = 0;
        Object o = new YieldWithSync();

        Runnable vt_target = new Runnable() {
            public void run() {
                synchronized (o) {
                    vt_tag++;
                    LockSupport.park();
                    synchronized (o) {
                        vt_tag++;
                    }
                }
            }
        };

        Runnable target = new Runnable() {
            public void run() {
                synchronized (o) {
                    System.out.println("hello thread");
                }
            }
        };
        Thread vt = Thread.ofVirtual().name("vt").start(vt_target);
        while (vt_tag != 1) {
            Thread.sleep(100);
        }
        Thread t = new Thread(target, "thread-for-inflate-monitor");
        t.start();
        Thread.sleep(1000);

        LockSupport.unpark(vt);
        vt.join();
        t.join();

        assertEquals(vt_tag, 2);
    }

    @Test
    // same vt run on different carrier, and reenter ObjectMonitor
    public static void ObjectMonitorTest4() throws Exception {
        vt_tag = 0;
        Object o = new YieldWithSync();
        spin_flag1 = true;
        spin_flag2 = true;
        ExecutorService executor = Executors.newFixedThreadPool(2);

        Runnable target1 = new Runnable() {
            public void run() {
                while (spin_flag1);
            }
        };

        Runnable target2 = new Runnable() {
            public void run() {
                synchronized (o) {
                    vt_tag++;
                    LockSupport.park();
                    synchronized (o) {
                        vt_tag++;
                    }
                }
            }
        };

        Runnable target3 = new Runnable() {
            public void run() {
                synchronized (o) {
                    while (spin_flag2);
                }
            }
        };

        ThreadFactory vf = Thread.ofVirtual().scheduler(executor).name("vt_", 0).factory();
        Thread vt1 = vf.newThread(target1);
        vt1.start();

        Thread vt2 = vf.newThread(target2);
        vt2.start();
        while (vt_tag != 1) {
            Thread.sleep(100);
        }

        Thread vt3 = vf.newThread(target3);
        vt3.start();
        Thread.sleep(1000);

        spin_flag1 = false;
        vt1.join();

        LockSupport.unpark(vt2);
        vt2.join();
        assertEquals(vt_tag, 2);

        spin_flag2 = false;
        vt3.join();

        executor.shutdown();
        executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
    }
}
