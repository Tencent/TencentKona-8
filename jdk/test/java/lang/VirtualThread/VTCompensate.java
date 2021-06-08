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
 * @run testng VTCompensate
 * @summary Basic test for ForkJoinPool to compensate when all threads are pinned.
 */

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.locks.ReentrantLock;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class VTCompensate {
    @Test
    public static void test() throws Exception {
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
}
