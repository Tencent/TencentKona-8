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

/**
 * @test
 * @library /lib/testlibrary
 * @run main/othervm DeadLockTest 1 1
 * @run main/othervm DeadLockTest 1 0
 * @run main/othervm DeadLockTest 0 1
 * @run main/othervm DeadLockTest 0 0
 * @summary detect dead lock is detected in VT, with argumetns VT is convert to thread by
 * adding option -Djdk.internal.VirtualThread=off
 */
import java.util.concurrent.CountDownLatch;
import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.locks.ReentrantLock;
import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;

/*
 * Deadlock cases include:
 * 1. VT holds monitor and wait blocker (pin)
 *    1.1 other VT/Thread holds blocker and wait monitor
 * 2. VT holds monitor1 and wait monitor2 (block in carrier)
 *    2.1 other VT/thread holds monitor2 and wait monitor1
 * 3. VT holds blocker and wait monitor (block in carrier)
 *    3.1  other VT/thread holds monitor and wait blocker
 * 4. VT holds blocker1 and wait blocker2 (yield)
 *    4.1 other VT/thread holds blocker2 and wait for block1
 */
public class DeadLockTest {
    // check 10 times with 1s interval
    public static void CheckDeadLock(ThreadMXBean mbean) {
        for (int i = 0; i < 10 ; i++) {
            long[] threadIds = mbean.findDeadlockedThreads();
            if (threadIds != null) {
                System.out.println("Deadlock detected!");
                ThreadInfo[] threadInfos = mbean.getThreadInfo(threadIds);
                for (int j = 0; j < threadInfos.length; j++) {
                    ThreadInfo threadInfo = threadInfos[j];
                    if (threadInfo == null) {
                        System.out.println("ThreadID: " + threadIds[j] + ", has no thread info");
                        continue;
                    }
                    StackTraceElement[] stackTrace = threadInfo.getStackTrace();
                    System.err.println(threadInfo.toString().trim());
                    for (StackTraceElement ste : stackTrace) {
                        System.err.println("\t" + ste.toString().trim());
                    }
                }
                System.exit(0);
            }
            try {
                Thread.sleep(1000);
            } catch (Exception e) {
            }
        }
        System.exit(0);
    }

    // 1. VT holds monitor and wait blocker (pin)
    // 3. VT holds blocker and wait monitor (block in carrier)
    public static class DeadLockTest1 {
        private static final ThreadMXBean mbean = ManagementFactory.getThreadMXBean();
        private static volatile boolean thread1Locked = false;
        private static volatile boolean thread2Locked = false;
        public static void main(String[] args) throws Exception {
            // args[0] = 0, thread1 is VT
            // args[0] = 1, thread1 is thread
            // args[1] = 0, thread2 is VT
            // args[1] = 1, thread2 is thread
            boolean thread1VT =  args[0].equals("0");
            boolean thread2VT =  args[1].equals("0");
            CountDownLatch waitSignal = new CountDownLatch(2);
            ExecutorService executor = Executors.newWorkStealingPool(4);
            Object monitor = new Object();
            ReentrantLock blocker = new ReentrantLock();
            Runnable r1 = new Runnable() {
                public void run() {
                    synchronized(monitor) {
                        thread1Locked = true;
                        System.out.println(Thread.currentThread().getName() + " first locked");
                        while (thread2Locked == false) {
                            try {
                                Thread.sleep(20);
                            } catch (Exception e) {

                            }
                        }
                        waitSignal.countDown();
                        blocker.lock();
                        System.out.println(Thread.currentThread().getName() + " second locked");
                    }
                }
            };

            Runnable r2 = new Runnable() {
                public void run() {
                    blocker.lock();
                    System.out.println(Thread.currentThread().getName() + " first locked");
                    thread2Locked = true;
                    while (thread1Locked == false) {
                        try {
                            Thread.sleep(20);
                        } catch (Exception e) {

                        }
                    }
                    waitSignal.countDown();
                    synchronized (monitor) {
                        System.out.println(Thread.currentThread().getName() + " second locked");
                    }
                }
            };
            Thread thread1;
            Thread thread2;
            if (thread1VT) {
                thread1 = Thread.ofVirtual().scheduler(executor).name("vt1").unstarted(r1);
            } else {
                thread1 = new Thread(r1);
            }
            if (thread2VT) {
                thread2 = Thread.ofVirtual().scheduler(executor).name("vt2").unstarted(r2);
            } else {
                thread2 = new Thread(r2);
            }
            thread1.start();
            thread2.start();
            waitSignal.await();
            Thread.sleep(1000);
            CheckDeadLock(mbean);
        }
    }

    // 2. VT holds monitor1 and wait monitor2 (block in carrier)
    public static class DeadLockTest2 {
        private static final ThreadMXBean mbean = ManagementFactory.getThreadMXBean();
        private static volatile boolean thread1Locked = false;
        private static volatile boolean thread2Locked = false;
        public static void main(String[] args) throws Exception {
            // args[0] = 0, other is VT
            // args[0] = 1, other is thread
            System.out.println("args[0]: " + args[0]);
            boolean thread2VT =  args[0].equals("0");
            ExecutorService executor = Executors.newWorkStealingPool(4);
            Object monitor1 = new Object();
            Object monitor2 = new Object();
            CountDownLatch waitSignal = new CountDownLatch(2);
            Runnable r1 = new Runnable() {
                public void run() {
                    synchronized(monitor1) {
                        thread1Locked = true;
                        System.out.println(Thread.currentThread().getName() + " first locked");
                        while (thread2Locked == false) {
                            try {
                                Thread.sleep(20);
                            } catch (Exception e) {

                            }
                        }
                        waitSignal.countDown();
                        synchronized(monitor2) {
                            System.out.println(Thread.currentThread().getName() + " second locked");
                        }
                    }
                }
            };

            Runnable r2 = new Runnable() {
                public void run() {
                    synchronized(monitor2) {
                        System.out.println(Thread.currentThread().getName() + " first locked");
                        thread2Locked = true;
                        while (thread1Locked == false) {
                            try {
                                Thread.sleep(20);
                            } catch (Exception e) {

                            }
                        }
                        waitSignal.countDown();
                        synchronized (monitor1) {
                            System.out.println(Thread.currentThread().getName() + " second locked");
                        }
                    }
                }
            };
            Thread thread1 = Thread.ofVirtual().scheduler(executor).name("vt1").unstarted(r1);
            Thread thread2;
            if (thread2VT) {
                thread2 = Thread.ofVirtual().scheduler(executor).name("vt2").unstarted(r2);
            } else {
                thread2 = new Thread(r2);
            }
            thread1.start();
            thread2.start();
            waitSignal.await();
            Thread.sleep(1000);
            CheckDeadLock(mbean);
        }
    }

    // 4. VT holds blocker1 and wait blocker2 (yield)
    public static class DeadLockTest3 {
        private static final ThreadMXBean mbean = ManagementFactory.getThreadMXBean();
        private static volatile boolean thread1Locked = false;
        private static volatile boolean thread2Locked = false;
        public static void main(String[] args) throws Exception {
            // args[0] = 0, other is VT
            // args[0] = 1, other is thread
            boolean thread2VT =  args[0].equals("0");
            ExecutorService executor = Executors.newWorkStealingPool(4);
            ReentrantLock blocker1 = new ReentrantLock();
            ReentrantLock blocker2 = new ReentrantLock();
            CountDownLatch waitSignal = new CountDownLatch(2);
            Runnable r1 = new Runnable() {
                public void run() {
                    blocker1.lock();
                    thread1Locked = true;
                    System.out.println(Thread.currentThread().getName() + " first locked");
                    while (thread2Locked == false) {
                        try {
                            Thread.sleep(20);
                        } catch (Exception e) {
                        }
                    }
                    waitSignal.countDown();
                    blocker2.lock();
                    System.out.println(Thread.currentThread().getName() + " second locked");
                }
            };

            Runnable r2 = new Runnable() {
                public void run() {
                    blocker2.lock();
                    thread2Locked = true;
                    System.out.println(Thread.currentThread().getName() + " first locked");
                    while (thread1Locked == false) {
                        try {
                            Thread.sleep(20);
                        } catch (Exception e) {
                        }
                    }
                    waitSignal.countDown();
                    blocker1.lock();
                    System.out.println(Thread.currentThread().getName() + " second locked");
                }
            };
            Thread thread1 = Thread.ofVirtual().scheduler(executor).name("vt1").unstarted(r1);
            Thread thread2;
            if (thread2VT) {
                thread2 = Thread.ofVirtual().scheduler(executor).name("vt2").unstarted(r2);
            } else {
                thread2 = new Thread(r2);
            }
            thread1.start();
            thread2.start();
            waitSignal.await();
            Thread.sleep(1000);
            CheckDeadLock(mbean);
        }
    }


    static void runTest(String... cmds) throws Throwable {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(cmds);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        int exitValue = output.getExitValue();
        if (exitValue != 0) {
            //expecting a zero value
            throw new Error("Expected to get zero exit value");
        }
        output.shouldContain("Deadlock detected!");
    }

    public static void main(String[] args) throws Throwable {
        String vm_arg1 = "-Djdk.internal.VirtualThread=on";
        if (Integer.parseInt(args[0]) == 1) {
            System.out.println("disable virtual thread");
            vm_arg1 = "-Djdk.internal.VirtualThread=off";
        }

        String vm_arg2 = "-XX:+YieldWithMonitor";
        if (Integer.parseInt(args[1]) == 1) {
            System.out.println("disable yield with monitor");
            vm_arg2 = "-XX:-YieldWithMonitor";
        }

        runTest(vm_arg1, vm_arg2, DeadLockTest1.class.getName(), "1", "1");
        runTest(vm_arg1, vm_arg2, DeadLockTest1.class.getName(), "0", "0");
        runTest(vm_arg1, vm_arg2, DeadLockTest1.class.getName(), "0", "1");
        runTest(vm_arg1, vm_arg2, DeadLockTest1.class.getName(), "1", "0");

        runTest(vm_arg1, vm_arg2,  DeadLockTest2.class.getName(), "0");
        runTest(vm_arg1, vm_arg2,  DeadLockTest2.class.getName(), "1");

        runTest(vm_arg1, vm_arg2, DeadLockTest3.class.getName(), "0");
        runTest(vm_arg1, vm_arg2, DeadLockTest3.class.getName(), "1");
    }
}
