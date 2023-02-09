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
 * @library /lib/testlibrary
 * @run main/othervm VTPrintLockInfo
 * @summary Print lock info of vt when vt is not running and holds an ObjectMonitor.
 */

import java.io.IOException;
import java.util.concurrent.*;
import java.util.concurrent.locks.*;
import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;
import jdk.testlibrary.JDKToolFinder;
import java.lang.reflect.Field;

public class VTPrintLockInfo {
    static CountDownLatch doneSignal = new CountDownLatch(1);
    public static class YieldWithMonitor {
        public static void main(String... args) throws Exception {
            CountDownLatch startSignal = new CountDownLatch(1);
            ReentrantLock lock = new ReentrantLock();

            Runnable vt0_r = new Runnable() {
                @Override
                public void run() {
                    try {
                        startSignal.await();
                        synchronized (this) {
                            lock.lock();
                        }
                        lock.unlock();
                    } catch (Exception e) {
                    }
                }
            };

            lock.lock();
            startSignal.countDown();
            Thread vt0 = Thread.ofVirtual().name("vt0").start(vt0_r);

            doneSignal.await();
            lock.unlock();
            vt0.join();
        }
    }

    public static long getPidOfProcess(Process p) {
        long pid = -1;
        try {
            if (p.getClass().getName().equals("java.lang.UNIXProcess")) {
                Field f = p.getClass().getDeclaredField("pid");
                f.setAccessible(true);
                pid = f.getLong(p);
                f.setAccessible(false);
            }
        } catch (Exception e) {
            pid = -1;
        }
        return pid;
    }

    public static void main(String[] args) throws Throwable {
        ProcessBuilder simplePb = ProcessTools.createJavaProcessBuilder(VTPrintLockInfo.YieldWithMonitor.class.getName());
        long simplePid = getPidOfProcess(simplePb.start());
        ProcessBuilder outputPb = new ProcessBuilder();
        outputPb.command(new String[]{JDKToolFinder.getJDKTool("jstack"), String.valueOf(simplePid)});
        OutputAnalyzer output = new OutputAnalyzer(outputPb.start());
        System.out.println(output.getOutput());
        /* output is:
          Coroutine: 0x7f55a4000bf0    VirtualThread => name: vt0, state PARKED
          at java.lang.Continuation.yield0(Continuation.java:251)
          - parking to wait for  <0x00000007ae2640a8> (a java.util.concurrent.locks.ReentrantLock$NonfairSync)
          at java.lang.Continuation.yield(Continuation.java:298)
          at java.lang.VirtualThread.tryPark(VirtualThread.java:523)
          at java.lang.VirtualThread.park(VirtualThread.java:483)
          at java.lang.System$2.parkVirtualThread(System.java:1320)
          at sun.misc.VirtualThreads.park(VirtualThreads.java:55)
          at java.util.concurrent.locks.LockSupport.park(LockSupport.java:183)
          at java.util.concurrent.locks.AbstractQueuedSynchronizer.parkAndCheckInterrupt(AbstractQueuedSynchronizer.java:859)
          at java.util.concurrent.locks.AbstractQueuedSynchronizer.acquireQueued(AbstractQueuedSynchronizer.java:893)
          at java.util.concurrent.locks.AbstractQueuedSynchronizer.acquire(AbstractQueuedSynchronizer.java:1222)
          at java.util.concurrent.locks.ReentrantLock$NonfairSync.lock(ReentrantLock.java:209)
          at java.util.concurrent.locks.ReentrantLock.lock(ReentrantLock.java:285)
          at VTPrintLockInfo$YieldWithMonitor$1.run(VTPrintLockInfo.java:51)
          - locked <0x00000007ae265de0> (a VTPrintLockInfo$YieldWithMonitor$1)
          at java.lang.VirtualThread.lambda$new$0(VirtualThread.java:184)
          at java.lang.VirtualThread$$Lambda$5/1747585824.run(Unknown Source)
          at java.lang.Continuation.start(Continuation.java:111)
        */
        output.shouldContain("- locked");
        output.shouldContain("- parking to wait for");
        doneSignal.countDown();
    }
}
