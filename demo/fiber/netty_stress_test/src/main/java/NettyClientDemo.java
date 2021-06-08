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

public class NettyClientDemo {
    private static int threadCount;
    private static int requestCount;
    private static int statsInterval;
    private static boolean useFiber;

    public static void main(String[] args) throws Exception {

        threadCount = Integer.parseInt(args[0]);
        requestCount = Integer.parseInt(args[1]);
        useFiber = Boolean.parseBoolean(args[2]);
        statsInterval = requestCount / 10;

        System.out.println("options: thread " + threadCount + " requestCount " + requestCount + " use fiber " + useFiber);

        testSyncCall();
        System.exit(0);
    }

    static void testSyncCall() throws Exception {
        CountDownLatch startSignal = new CountDownLatch(1);
        CountDownLatch doneSignal = new CountDownLatch(requestCount);
        AtomicLong count = new AtomicLong();
        AtomicLong statsTimes = new AtomicLong();

        NettyClient client = new NettyClient();

        ThreadFactory factory;
        if (useFiber == false) {
            factory = Thread.ofPlatform().factory();
        } else {
            factory = Thread.ofVirtual().name("Test-", 0).factory();
        }
        ExecutorService e = Executors.newFixedThreadPool(threadCount, factory);

        Runnable r = new Runnable() {
            @Override
            public void run() {
                try {
                    startSignal.await();
                    String result = client.SyncCall("hello server");

                    long val = count.addAndGet(1);
                    if ((val % statsInterval) == 0) {
                        long time = System.currentTimeMillis();
                        long prev = statsTimes.getAndSet(time);
                        System.out.println("interval " + val + " throughput " + statsInterval/((time - prev)/1000.0));
                    }
                    //System.out.println("[client]receive from server: " + result + " count is " + val);
                    doneSignal.countDown();
                } catch (Exception e) {

                }
            }
        };

        for (int i = 0; i < requestCount; i++) {
            e.execute(r);
        }

        long before = System.currentTimeMillis();
        statsTimes.set(before);
        startSignal.countDown();
        doneSignal.await();

        long after = System.currentTimeMillis();
        long duration = (after - before);
        System.out.println("finish " + count.get() + " time " + duration + "ms throughput " + (count.get()/(duration/1000.0)));

        e.shutdown();
        e.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
        client.close();
    }
}
