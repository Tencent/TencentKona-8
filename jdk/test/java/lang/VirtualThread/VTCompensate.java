/*
 * Copyright (C) 2021, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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
 * @run testng/othervm -XX:-YieldWithMonitor VTCompensate
 * @summary Basic test for ForkJoinPool to compensate when all threads are pinned.
 */

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.locks.ReentrantLock;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class VTCompensate {
    public static void pinCompensate(boolean needSleep) throws Exception {
        ExecutorService e = Executors.newWorkStealingPool(1);
        //Executors.newSingleThreadExecutor()
        ThreadFactory tf = Thread.ofVirtual().scheduler(e).name("vt", 0).factory();
        // VT0, VT1, main
        // 1. main acquire lock
        // 2. VT0 start, acquire lock and park
        // 3. VT1 start, acquire lock and pin
        // 4. main release lock and unpack VT0
        // 5. dead in VT1 and VT0, no VT can run
        ReentrantLock lock = new ReentrantLock();
        Runnable vt0_r = new Runnable() {
            @Override
            public void run() {
                System.out.println("enter vt0");
                lock.lock();
                System.out.println("vt0 get lock " + Thread.currentThread() + " " + Thread.currentCarrierThread());
                lock.unlock();
            }
        };
        Runnable vt1_r = new Runnable() {
            @Override
            public void run() {
                System.out.println("enter vt1");
                synchronized (this) {
                    lock.lock();
                }
                System.out.println("vt1 get lock " + Thread.currentThread() + " " + Thread.currentCarrierThread());
                lock.unlock();
            }
        };
        Thread vt0 = Thread.ofVirtual().scheduler(e).name("vt0").unstarted(vt0_r);
        Thread vt1 = Thread.ofVirtual().scheduler(e).name("vt1").unstarted(vt1_r);
        lock.lock();
        System.out.println("main lock");
        vt0.start();
        vt1.start();
        // wait vt1 pin and unlock
        while (true) {
            if (vt1.getState() == Thread.State.WAITING) {
                break;
            }
        }
        if (needSleep) {
            System.out.println("test compensated thread exit");
            while (((ForkJoinPool)e).getPoolSize() != 1) {
                Thread.sleep(1000);
            }
        }
        System.out.println("pool is " + (ForkJoinPool)e);

        lock.unlock();
        System.out.println("main release");
        vt0.join();
        assertEquals(vt0.getState(),  Thread.State.TERMINATED);
        System.out.println("finish vt0");
        vt1.join();
        assertEquals(vt1.getState(),  Thread.State.TERMINATED);
        System.out.println("finish vt1");
        e.shutdown();
    }

    @Test
    public static void testPinCompensate() throws Exception {
        // 1. needSleep is false to ensure compensate when pinned
        // 2. needSleep is true, compensate thread which is create on pin
        //    will exit. And a new thread will create when re-submit vt0.
        pinCompensate(false);
        pinCompensate(true);
    }
}
