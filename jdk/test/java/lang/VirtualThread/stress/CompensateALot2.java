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
 * @run testng CompensateALot2
 * @summary Stress test Compensate of ForkJoinPool
 */

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.LockSupport;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class CompensateALot2 {
    /*
     * 1. vt_locked hold synchronized lock and wait, at the same time,
     *    create a new worker thread of ForkJoinPool.
     * 2. vt0~vt199 hold synchronized lock and wait.(if vt0 do not create
     *    a new worker thread before block, there is no carrier thread
     *    to run vt1, main thread will spin to check state of vt1 forever)
     * 3. Main thread invoke notifyAll to wake up vt.
     * 4. All vt finish normally.
     * 5. Wait thread number of ForkJoinPool back to zero.
     */
    public static void syncWaitCompensate() throws Exception {
        ExecutorService e = Executors.newWorkStealingPool(1);
        ThreadFactory tf = Thread.ofVirtual().scheduler(e).name("vt", 0).factory();

        Object o = new Object();
        Runnable target = new Runnable() {
            @Override
            public void run() {
                synchronized (o) {
                    System.out.println("get lock " + Thread.currentThread());
                    try {
                        o.wait();
                    }
                    catch (InterruptedException e) {
                    }
                }
            }
        };

        assertEquals(((ForkJoinPool)e).getPoolSize(), 0);

        Thread vt_locked = Thread.ofVirtual().scheduler(e).name("vt_locked").start(target);
        while (vt_locked.getState() != Thread.State.WAITING) {
            System.out.println("waiting vt_locked to WAITING");
            Thread.sleep(5);
        }

        while (((ForkJoinPool)e).getPoolSize() < 2) {
            System.out.println("waiting pool size to 2");
            Thread.sleep(10);
        }
        assertEquals(((ForkJoinPool)e).getPoolSize(), 2);

        int count = 5;
        Thread[] vts = new Thread[count];
        for (int i = 0; i < count; i++) {
            vts[i] = Thread.ofVirtual().scheduler(e).name("vt", i).start(target);
        }

        /* Test ForkJoinWorkerThread exit while no task */
        while (((ForkJoinPool)e).getPoolSize() < count + 1) {
            System.out.println("waiting pool size " + ((ForkJoinPool)e).getPoolSize());
            Thread.sleep(10);
        }
        assertTrue(((ForkJoinPool)e).getPoolSize() >= count + 1);

        synchronized (o) {
            o.notifyAll();
        }

        vt_locked.join();
        for (int i = 0; i < count; i++) {
            vts[i].join();
        }

        e.shutdown();
        e.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
    }
}
