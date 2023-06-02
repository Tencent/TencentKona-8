/*
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

/**
 * @test
 * @run testng CompensateALot
 * @summary Stress test Compensate of ForkJoinPool
 */

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.LockSupport;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class CompensateALot {
    volatile static boolean locked = false;

    @Test
    /*
     * 1. vt_locked hold synchronized lock and park.
     * 2. vt0~vt199 try to alloc synchronized lock and create a new worker thread
     *    of ForkJoinPool before block(if not, there is no carrier thread to
     *    run vt again, this test case will hang and result error).
     * 3. All vt finish normally.
     * 4. Wait thread number of ForkJoinPool back to zero.
     */
    public static void allocSyncFailCompensate() throws Exception {
        ExecutorService e = Executors.newWorkStealingPool(1);

        Runnable target = new Runnable() {
            @Override
            public void run() {
                System.out.println("before lock " + Thread.currentThread());
                synchronized (this) {
                    locked = true;
                    LockSupport.park();
                    System.out.println("after park " + Thread.currentThread());
                }
            }
        };

        assertEquals(((ForkJoinPool)e).getPoolSize(), 0);

        Thread vt_locked = Thread.ofVirtual().scheduler(e).name("vt_locked").start(target);
        while (locked != true) {
            Thread.sleep(5);
        }

        assertEquals(((ForkJoinPool)e).getPoolSize(), 1);
        Thread[] vts = new Thread[200];
        for (int i = 0; i < 200; i++) {
            System.out.println("launching vt" + i);
            vts[i] = Thread.ofVirtual().scheduler(e).name("vt", i).start(target);
        }

        for (int i = 0; i < 200; i++) {
            while (vts[i].getState() != Thread.State.BLOCKED) {
                Thread.sleep(3);
            }
            System.out.println("vt" + i + " in block status");
        }

        while (((ForkJoinPool)e).getPoolSize() < 200) {
            System.out.println("1: pool size is " + ((ForkJoinPool)e).getPoolSize());
            Thread.sleep(10);
        }
        assertTrue(((ForkJoinPool)e).getPoolSize() >= 200);

        LockSupport.unpark(vt_locked);
        for (int i = 0; i < 200; i++) {
            LockSupport.unpark(vts[i]);
        }

        vt_locked.join();
        for (int i = 0; i < 200; i++) {
            vts[i].join();
        }

        while (((ForkJoinPool)e).getPoolSize() != 0) {
            System.out.println("pool size is " + ((ForkJoinPool)e).getPoolSize());
            Thread.sleep(10);
        }

        e.shutdown();
        e.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
    }
}
