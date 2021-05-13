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
 * @run testng SimpleVT
 * @summary Basic test for virtual thread, test create/run/yield/resume/stop
 */
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import sun.misc.VirtualThreads;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class SimpleVT {
    static long finished_vt_count = 0;

    @Test
    public static void foo() throws Exception {
        finished_vt_count = 0;
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("before yield");
                Thread.yield();
                System.out.println("resume yield");
                finished_vt_count++;
            }
        };
        Thread vt = Thread.builder().virtual(executor).name("foo_thread").task(target).build();
        vt.start();
        vt.join();
        //Thread.sleep(1000);
        executor.shutdown();
        assertEquals(finished_vt_count, 1);
    }

    @Test
    public static void FixedThreadPoolSimple() throws Exception {
        ExecutorService executor = Executors.newFixedThreadPool(4);
        AtomicInteger ai = new AtomicInteger(0);

        Runnable target = new Runnable() {
            public void run() {
                System.out.println("before yield " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                Thread.yield();
                System.out.println("resume yield " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                ai.incrementAndGet();
            }
        };

        Thread[] vts = new Thread[40];
        ThreadFactory f = Thread.builder().virtual(executor).name("FixedThreadPoolSimple_", 0).factory();
        for (int i = 0; i < 40; i++) {
            vts[i] = f.newThread(target);
        }
        for (int i = 0; i < 40; i++) {
            vts[i].start();
        }

        for (int i = 0; i < 40; i++) {
            vts[i].join();
        }
        executor.shutdown();
        assertEquals(ai.get(), 40);
    }

    @Test
    public static void singleThreadParkTest() throws Exception {
        final Thread kernel = Thread.currentThread();
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("before park " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                assertEquals(Thread.currentThread().getName(), "park_thread");
                assertNotEquals(Thread.currentCarrierThread(), kernel);
                VirtualThreads.park();
                System.out.println("after park " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                assertEquals(Thread.currentThread().getName(), "park_thread");
                assertNotEquals(Thread.currentCarrierThread(), kernel);
            }
        };
        Thread vt = Thread.builder().virtual(executor).name("park_thread").task(target).build();
        vt.start();

        System.out.println("after start " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
        assertEquals(Thread.currentThread(), kernel);
        assertEquals(Thread.currentCarrierThread(), kernel);

        Thread.sleep(500);

        System.out.println("before unpark " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
        assertEquals(Thread.currentThread(), kernel);
        assertEquals(Thread.currentCarrierThread(), kernel);

        VirtualThreads.unpark(vt);

        System.out.println("after unpark " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
        assertEquals(Thread.currentThread(), kernel);
        assertEquals(Thread.currentCarrierThread(), kernel);

        vt.join();
        System.out.println("after join");
        //Thread.sleep(1000);
        executor.shutdown();
    }

    /*
     * Park and unpark in multiple virtual threads
     */
    @Test
    public static void multipleThreadParkTest() throws Exception {
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Thread[] vts = new Thread[10];
        Runnable target = new Runnable() {
            public void run() {
                System.out.println(Thread.currentThread().getName() + " before park");
                int myIndex = -1;
                for (int i = 0; i < 10; i++) {
                    if (vts[i] == Thread.currentThread()) {
                        myIndex = i;
                        break;
                    }
                }

                assertEquals(Thread.currentThread().getName(), "vt" + myIndex);
                // myIndex from 0 to 9
                if (myIndex > 0) {
					VirtualThreads.unpark(vts[myIndex - 1]);
                }
                VirtualThreads.park();
                System.out.println(Thread.currentThread().getName() + " after park");
                assertEquals(Thread.currentThread().getName(), "vt" + myIndex);
            }
        };
        ThreadFactory f = Thread.builder().virtual(executor).name("vt", 0).factory();
        for (int i = 0; i < 10; i++) {
            vts[i] = f.newThread(target);
        }
        for (int i = 0; i < 10; i++) {
            vts[i].start();
        }
        VirtualThreads.unpark(vts[9]);
        for (int i = 0; i < 10; i++) {
            vts[i].join();
        }
        executor.shutdown();
    }

    @Test
    public static void multipleThreadPoolParkTest() throws Exception {
        ExecutorService executor = Executors.newFixedThreadPool(8);
        Thread[] vts = new Thread[100];
        Runnable target = new Runnable() {
            public void run() {
                System.out.println(Thread.currentThread().getName() + " before park");
                int myIndex = -1;
                for (int i = 0; i < 100; i++) {
                    if (vts[i] == Thread.currentThread()) {
                        myIndex = i;
                        break;
                    }
                }

                assertEquals(Thread.currentThread().getName(), "vt" + myIndex);
                // myIndex from 0 to 99
                if (myIndex > 0) {
                    VirtualThreads.unpark(vts[myIndex - 1]);
                } else {
                    VirtualThreads.unpark(vts[99]);
                }
                VirtualThreads.park();
                System.out.println(Thread.currentThread().getName() + " after park");
                assertEquals(Thread.currentThread().getName(), "vt" + myIndex);
            }
        };
        ThreadFactory f = Thread.builder().virtual(executor).name("vt", 0).factory();
        for (int i = 0; i < 100; i++) {
            vts[i] = f.newThread(target);
        }
        for (int i = 0; i < 100; i++) {
            vts[i].start();
        }
        for (int i = 0; i < 100; i++) {
            vts[i].join();
        }
        executor.shutdown();
    }

}
