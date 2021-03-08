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
 * @run testng SingleThreadExecutorTest 
 * @summary Basic test for VT before/after park interface 
 */

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.AbstractExecutorService;
import java.util.concurrent.ConcurrentLinkedDeque;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

import static org.testng.Assert.*;
import org.testng.annotations.Test;
public class SingleThreadExecutorTest {
    @Test
    public static void test() throws Exception {
        ExecutorService e = new SingleThreadExecutor();
        ThreadFactory tf = Thread.builder().virtual(e).name("vt", 0).factory();
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
                System.out.println("enter vt0 " + Thread.currentCarrierThread());
                lock.lock();
                System.out.println("vt0 get lock " +  Thread.currentCarrierThread());
                lock.unlock();
            }
        };
        Runnable vt1_r = new Runnable() {
            @Override
            public void run() {
                System.out.println("enter vt1 " +  Thread.currentCarrierThread());
                synchronized (this) {
                    lock.lock();
                }
                System.out.println("vt1 get lock " +  Thread.currentCarrierThread());
                lock.unlock();
            }
        };
        Thread vt0 = Thread.builder().virtual(e).task(vt0_r).name("vt0").build();
        Thread vt1 = Thread.builder().virtual(e).task(vt1_r).name("vt1").build();
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
        System.out.println("finish vt0");
        vt1.join();
        System.out.println("finish vt1");
        e.shutdown();
    }
}

/*
 * only run with single thread executor
 * 1. at same time only one runnable task running
 * 2. If ExcludeWorkerThread is blocked, create new ExcludeWorkerThread continue running
 * 3. If ExcludeWorkerThread is un blocked, terminate/caching current running thread and notify unblocked thread
 **/
class SingleThreadExecutor extends AbstractExecutorService implements VTParkCarrierAction {
    ConcurrentLinkedDeque<Runnable> tasks = new ConcurrentLinkedDeque<>();
    Thread currentThread = null;
    //AtomicInteger pendingCount = new AtomicInteger(0);
    int pendingCount = 0;
    private volatile boolean is_shutdown = false;
    private volatile boolean is_shutdown_imm = false;
    private volatile boolean terminated = false;
    private volatile ReentrantLock lock = new ReentrantLock();
    private Condition condition = lock.newCondition();
    @Override
    public void shutdown() {
        is_shutdown = true;
    }

    @Override
    public List<Runnable> shutdownNow() {
        is_shutdown = true;
        is_shutdown_imm = true;
        // fetch tasks in list
        ArrayList list = new ArrayList<Runnable>();
        do {
            Runnable r = tasks.poll();
            if (r == null) {
                break;
            }
            list.add(r);
        } while(true);
        return list;
    }

    @Override
    public boolean isShutdown() {
        return is_shutdown;
    }

    @Override
    public boolean isTerminated() {
        return terminated;
    }

    @Override
    public boolean awaitTermination(long timeout, TimeUnit unit) throws InterruptedException {
        long nanos = unit.toNanos(timeout);
        lock.lock();
        try {
            for (;;) {
                if (terminated) {
                    return true;
                }
                if (nanos <= 0)
                    return false;
                nanos = condition.awaitNanos(nanos);
            }
        } finally {
            lock.unlock();
        }
    }

    @Override
    public void execute(Runnable command) {
        if (is_shutdown == false) {
            tasks.offer(command);
        }
    }

    public void beforePark() {
        if (!lock.isHeldByCurrentThread()) {
            throw new Error("Pin without lock");
        }
        if (currentThread != Thread.currentThread()) {
            throw new Error("un expect thread");
        }
        pendingCount++;
        currentThread = new ExcludeWorkerThread();
        currentThread.start();
        lock.unlock();
    }

    public void afterPark() {
        lock.lock();
        currentThread = Thread.currentThread();
        pendingCount--;
    }

    SingleThreadExecutor() {
        currentThread = new ExcludeWorkerThread();
        currentThread.start();
    }

    // only one thread in SingleThreadExecutor can running simultaneously
    //
    class ExcludeWorkerThread extends Thread {
        @Override
        public void run() {
            try {
                lock.lock();
                while (is_shutdown_imm == false) {
                    if (lock.hasQueuedThreads()) {
                        break;
                    }
                    Runnable r = tasks.poll();
                    if (r == null) {
                        if (is_shutdown) {
							break;
                        }
                        Thread.yield();
                    }
                    else {
                        r.run();
                    }
                }
            } finally {
                if (pendingCount == 0) {
                    terminated = true;
                }
                System.out.println("terminate " + Thread.currentThread());
                lock.unlock();
            }
        }
    }
}

