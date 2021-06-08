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
 * @run main/othervm VTExecutorTest 10 100 100 false
 * @run main/othervm VTExecutorTest 100 1000 1000 false
 * @run main/othervm VTExecutorTest 1000 10000 5000 false
 * @run main/othervm VTExecutorTest 10 100 100 true
 * @run main/othervm VTExecutorTest 100 1000 1000 true
 * @run main/othervm VTExecutorTest 1000 10000 5000 true
 * @summary stress scheduling test
 */
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class VTExecutorTest {
    private static ExecutorService carrier_executor;
    private static int realThreadCount;
    private static int requestCount;
    private static int interval;
	private static boolean useDefaultScheduler = false;

    public static void main(String[] args) throws Exception {
        realThreadCount = Integer.parseInt(args[0]);
        requestCount = Integer.parseInt(args[1]);
        interval = Integer.parseInt(args[2]);
        useDefaultScheduler = Boolean.parseBoolean(args[3]);
		if (useDefaultScheduler == true) {
			carrier_executor = null;
		} else {
			carrier_executor = Executors.newFixedThreadPool(Runtime.getRuntime().availableProcessors());
		}
        for (int i = 1; i < 100; i+=20) {
            System.out.println("sleep " + i + "ms");
            testSchedule(i);
        }
		if (carrier_executor != null) {
            carrier_executor.shutdown();
        }
    }

    private static void testSchedule(int sleepMilli) throws Exception {
        AtomicInteger count = new AtomicInteger();
        ThreadFactory f = Thread.ofVirtual().factory();
        ExecutorService e = Executors.newFixedThreadPool(realThreadCount, f);
        CountDownLatch startSignal = new CountDownLatch(1);
        CountDownLatch doneSignal = new CountDownLatch(requestCount);
        Runnable r = new Runnable() {
            @Override
            public void run() {
                try {
                    startSignal.await();
                    Thread.sleep(sleepMilli);
                    int val = count.addAndGet(1);
                    if ((val % interval) == 0) {
                        System.out.println(val);
                    }
                    doneSignal.countDown();
                } catch (InterruptedException e) {
                }
            }
        };
        long start = System.currentTimeMillis();
        for (int i = 0; i < requestCount; i++) {
            e.execute(r);
        }
        long now = System.currentTimeMillis();
        System.out.println("finish submit " + (now - start));
        startSignal.countDown();
        doneSignal.await();
        System.out.println("finish " + count.get() + " time " + (System.currentTimeMillis()- now));
        if (count.get() != requestCount) {
           throw new Exception("Not equal");
        }
        now = System.currentTimeMillis();
        e.shutdown();
        e.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
        System.out.println("finish shutdown " + (System.currentTimeMillis() - now));
    }

    /*static class FiberThreadFactory implements ThreadFactory {
        private final AtomicInteger threadNumber = new AtomicInteger(1);
        private final String namePrefix = "vt-";
        FiberThreadFactory() {
        }

        public Thread newThread(Runnable r) {
            int index = threadNumber.getAndIncrement();
            Thread t = new VirtualThread(carrier_executor, namePrefix + index, 0, r);
            if (t.isDaemon())
                t.setDaemon(false);
            if (t.getPriority() != Thread.NORM_PRIORITY)
                t.setPriority(Thread.NORM_PRIORITY);
            return t;
        }
    }*/
}
