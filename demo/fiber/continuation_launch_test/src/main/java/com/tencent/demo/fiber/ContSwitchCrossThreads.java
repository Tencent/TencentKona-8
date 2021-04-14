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

package com.tencent.demo.fiber;

import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.runner.*;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.*;
@BenchmarkMode(Mode.Throughput)
@Warmup(iterations = 2, time = 10, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 5, time = 10, timeUnit = TimeUnit.SECONDS)
@OutputTimeUnit(TimeUnit.SECONDS)
@Fork(1)
@Threads(1)
@State(Scope.Benchmark)

/*
 * Test continuation switch between threads 
 */
public class ContSwitchCrossThreads {
    private static ContinuationScope scope = new ContinuationScope("scope");
    private static Continuation[] conts = new Continuation[1000];
    private static Thread[] threads = new Thread[2];
    private static int runIndex = 0;
    private static final ReentrantLock lock1 = new ReentrantLock();
    private static final ReentrantLock lock2 = new ReentrantLock();
    private static final Condition cond1 = lock1.newCondition();
    private static final Condition cond2 = lock2.newCondition();
    private volatile static CountDownLatch doneSignal;
    @Param({"1", "2", "3", "4", "5"})
    public int iter;
    public static void main() throws Exception {
        Options opt = new OptionsBuilder()
                .include(ContSwitchCrossThreads.class.getSimpleName())
                .build();

        new Runner(opt).run();
    }

    @GenerateMicroBenchmark
    public void testCrossSwitch() throws Exception {
        // thread 0 run and thread 1 run
        // notify cond1
        // thread 1 run and notify cond2
        // thread 2 run and notify cond
        lock1.lock();
        lock2.lock();
        doneSignal = new CountDownLatch(1);
        try {
            cond1.signal();
        } finally {
            lock1.unlock();
        }
        doneSignal.await();
        doneSignal = new CountDownLatch(1);
        try {
            cond2.signal();
        } finally {
            lock2.unlock();
        }
        doneSignal.await();
    }

    @Setup
    public void prepare() {
         // prepare 100 continuations
         for (int i = 0; i < 1000; i++) {
             conts[i] = new Continuation(scope, () -> {
                 while(true) {
                     Continuation.yield(scope);
                 }
             });
         }
         // start two threads running
         threads[0] = new Thread(() -> {
             lock1.lock();
             while (true) {
                try {
                    cond1.await();
                    for (int j = 0; j < 1000; j++) {
                        conts[j].run();
                    }
                } catch(Throwable t) {
                } finally {
                    doneSignal.countDown();
                }
            }
         });
         threads[1] = new Thread(() -> {
             lock2.lock();
             while (true) {
                try {
                    cond2.await();
                    for (int j = 0; j < 1000; j++) {
                        conts[j].run();
                    }
                } catch(Throwable t) {
                } finally {
                    doneSignal.countDown();
                }
            }
         });
         threads[0].start();
         threads[1].start();
         try {
           Thread.sleep(1000);
         } catch(Exception e) {
         }
    }

    @TearDown
    public void shutdown() throws Exception {
    }
}
