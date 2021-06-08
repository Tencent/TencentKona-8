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

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

/*
 * test scheduling perfromance with yield
 * input arguments: threadCount requestCount useFiber={true|false}
 * measures:
 * 1. threads startup time
 * 2. threads execution scheduling time
 * 3. threads close time
 */
public class VTyield {
    private static int threadCount;
    private static int requestCount;
    private static int perThreadYield;
    private static int statsInterval;
    private static int sleepms;
    private static boolean useFiber;
    public static void main(String[] args) throws Exception {
        threadCount = Integer.parseInt(args[0]);
        requestCount = Integer.parseInt(args[1]);
        useFiber = Boolean.parseBoolean(args[2]);
        statsInterval = requestCount/10;
        perThreadYield = requestCount/threadCount;
        System.out.println("options: thread " + threadCount + " requestCount " + requestCount + " perthread yield " + perThreadYield + " use fiber " + useFiber);
        testSchedule();
        System.exit(0);
    }

    private static void testSchedule() throws Exception {
        AtomicLong count = new AtomicLong();
        AtomicLong statsTimes = new AtomicLong();
        ExecutorService e;
        if (useFiber) {
            ThreadFactory f = Thread.ofVirtual().factory();
            e = Executors.newFixedThreadPool(threadCount, f);
        } else {
            e = Executors.newFixedThreadPool(threadCount);
        }
        CountDownLatch startSignal = new CountDownLatch(1);
        CountDownLatch doneSignal = new CountDownLatch(threadCount);
        Runnable r = new Runnable() {
            @Override
            public void run() {
                try {
                    startSignal.await();
                    for (int i = 0; i < perThreadYield; i++) {
                        Thread.yield();
                        long val = count.addAndGet(1);
                        if ((val % statsInterval) == 0) {
                            long time = System.currentTimeMillis();
                            long prev = statsTimes.getAndSet(time);
                            System.out.println("interval " + val + " throughput " + String.format("%.2f", statsInterval/((time - prev)/1000.0)));
                        }
                    }
                    doneSignal.countDown();
                } catch (InterruptedException e) {
                }
            }
        };
        long start = System.currentTimeMillis();
        for (int i = 0; i < threadCount; i++) {
            e.execute(r);
        }
        long afterStart = System.currentTimeMillis();
        System.out.println("finish start thread " + (afterStart - start) + "ms");
        statsTimes.set(afterStart);
        startSignal.countDown();
        doneSignal.await();
        long afterExecution = System.currentTimeMillis();
        long duration = (afterExecution - afterStart);
        System.out.println("finish " + count.get() + " time " + duration + "ms throughput " + (count.get()/(duration/1000.0)));
        e.shutdown();
        e.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
        System.out.println("finish shutdown " + (System.currentTimeMillis() - afterExecution) + "ms");
    }
}
