/*
 * Copyright (c) 2003, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 * @test
 * @summary Basic test for continuation, test create/run/yield/resume/stop
 */
import java.util.concurrent.*;

public class SimpleVT {
    static long count = 0;
    public static void main(String args[]) throws Exception {
        foo();
        System.out.println("finish foo 1");
        FixedThreadPoolSimple();
        System.out.println("finish FixedThreadPoolSimple 1");
		singleThreadParkTest();
		System.out.println("finish singleThreadParkTest 1");
        multipleThreadParkTest();
        System.out.println("finish multipleThreadParkTest 1");
		multipleThreadPoolParkTest();
		System.out.println("finish multipleThreadPoolParkTest 1");
    }

    static void foo() throws Exception {
		ExecutorService executor = Executors.newSingleThreadExecutor();
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("before yield");
                Continuation.yield();
                System.out.println("resume yield");
            }
        };
		VirtualThread vt = new VirtualThread(executor, "foo_thread", 0, target);
		vt.start();
		vt.join();
		//Thread.sleep(1000);
		executor.shutdown();
    }

    static void FixedThreadPoolSimple() throws Exception {
        ExecutorService executor = Executors.newFixedThreadPool(4);
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("before yield");
                Continuation.yield();
                System.out.println("resume yield");
            }
        };
		VirtualThread[] vts = new VirtualThread[40];
		for (int i = 0; i < 40; i++) {
			vts[i] = new VirtualThread(executor, "FixedThreadPoolSimple_" + i, 0, target);
		}
		for (int i = 0; i < 40; i++) {
			vts[i].start();
		}
          
		for (int i = 0; i < 40; i++) {
			vts[i].join();
		}
        executor.shutdown();
    }

	static void singleThreadParkTest() throws Exception {
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("before park " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
                VirtualThread.park();
                System.out.println("after park " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
            }
        };
        VirtualThread vt = new VirtualThread(executor, "park_thread", 0, target);
        vt.start();
		System.out.println("after start " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
		Thread.sleep(500);
		System.out.println("before unpark " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
		vt.unpark();
		System.out.println("after unpark " + Thread.currentThread().getName() + " " + Thread.currentCarrierThread().getName());
        vt.join();
		 System.out.println("after join");
        //Thread.sleep(1000);
        executor.shutdown();
    }

    /*
     * Park and unpark in multiple virtual threads
     */
	static void multipleThreadParkTest() throws Exception {
        ExecutorService executor = Executors.newSingleThreadExecutor();
		VirtualThread[] vts = new VirtualThread[10];
        Runnable target = new Runnable() {
            public void run() {
				System.out.println(Thread.currentThread().getName() + " before park");
				int myIndex = -1;
				for (int i = 0; i < 10; i++) {
					if (vts[i] == Thread.currentThread()) {
						myIndex = i;
						break;
					}
				}
				// myIndex from 0 to 9
				if (myIndex > 0) {
					vts[myIndex - 1].unpark();
				}
                VirtualThread.park();
				System.out.println(Thread.currentThread().getName() + " after park");
            }
        };
		for (int i = 0; i < 10; i++) {
			vts[i] = new VirtualThread(executor, "vt" + i, 0, target);
		}
		for (int i = 0; i < 10; i++) {
            vts[i].start();
        }
        vts[9].unpark();
		for (int i = 0; i < 10; i++) {
            vts[i].join();
        }
        executor.shutdown();
    }

    static void multipleThreadPoolParkTest() throws Exception {
        ExecutorService executor = Executors.newFixedThreadPool(8);
        VirtualThread[] vts = new VirtualThread[100];
        Runnable target = new Runnable() {
            public void run() {
                System.out.println(Thread.currentThread().getName() + " before park");
                int myIndex = -1;
                for (int i = 0; i < 100; i++) {
                    if (vts[i] == Thread.currentThread()) {
                        myIndex = i;
                        break;
                    }
                }
                // myIndex from 0 to 9
                if (myIndex > 0) {
                    vts[myIndex - 1].unpark();
                } else {
					vts[99].unpark();
				}
                VirtualThread.park();
                System.out.println(Thread.currentThread().getName() + " after park");
            }
        };
        for (int i = 0; i < 100; i++) {
            vts[i] = new VirtualThread(executor, "vt" + i, 0, target);
        }
        for (int i = 0; i < 100; i++) {
            vts[i].start();
        }
        for (int i = 0; i < 100; i++) {
            vts[i].join();
        }
        executor.shutdown();
    }

}
