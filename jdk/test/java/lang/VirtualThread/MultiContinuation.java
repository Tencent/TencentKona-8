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
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.atomic.*;
public class MultiContinuation {
    static long count = 0;
    static ContinuationScope scope = new ContinuationScope("test");
    public static void main(String args[]) throws Exception {
        foo();
        System.out.println("finish foo 1");
        bar();
        System.out.println("finish bar 1");
        qux();
        System.out.println("finish qux 1");
        baz();
        System.out.println("finish baz 1");
        quux();
        System.out.println("finish quux 1");
    }

    static void foo() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("Continuation first run in " + Thread.currentThread().getName());
                Continuation.yield(scope);
                System.out.println("Continuation second run in " + Thread.currentThread().getName());
            }
        };
        Continuation[] conts = new Continuation[10];
        for (int i = 0; i < 10; i++) {
			conts[i] = new Continuation(scope, target);;
        }
		for (int i = 0; i < 10; i++) {
			conts[i].run();
		}
        for (int i = 0; i < 10; i++) {
            conts[i].run();
        }
    }

    static void bar() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("Continuation first run in " + Thread.currentThread().getName());
                Continuation.yield(scope);
                System.out.println("Continuation second run in " + Thread.currentThread().getName());
				Continuation.yield(scope);
                System.out.println("Continuation third run in " + Thread.currentThread().getName());
            }
        };
        Continuation[] conts = new Continuation[10];
        for (int i = 0; i < 10; i++) {
            conts[i] = new Continuation(scope, target);;
        }
        for (int i = 0; i < 10; i++) {
            conts[i].run();
        }
        Thread thread = new Thread(){
            public void run() {
		        for (int i = 0; i < 10; i++) {
        	    	conts[i].run();
        		}
            }
        };
        thread.start();
        thread.join();
        for (int i = 0; i < 10; i++) {
            conts[i].run();
        }
        //cont.run();
    }
    // create in other thread and run in main
    static void qux() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("Continuation first run in " + Thread.currentThread().getName());
                Continuation.yield(scope);
                System.out.println("Continuation second run in " + Thread.currentThread().getName());
                Continuation.yield(scope);
                System.out.println("Continuation third run in " + Thread.currentThread().getName());
            }
        };
        Continuation[] conts = new Continuation[10];
        for (int i = 0; i < 10; i++) {
            conts[i] = new Continuation(scope, target);;
        }
        Thread thread = new Thread(){
            public void run() {
                for (int i = 0; i < 10; i++) {
                    conts[i].run();
                }
				try {
					Thread.sleep(2000);
				} catch (Exception e) {
				}
                for (int i = 0; i < 10; i++) {
                    conts[i].run();
                }

            }
        };
        thread.start();
		Thread.sleep(1000);
        for (int i = 0; i < 10; i++) {
            conts[i].run();
        }
		thread.join();
    }

    // racing processing multiple continuations in threads
    static void baz() throws Exception {
	    Runnable target = new Runnable() {
            public void run() {
                System.out.println("Continuation first run in " + Thread.currentThread().getName());
                Continuation.yield(scope);
                System.out.println("Continuation second run in " + Thread.currentThread().getName());
                Continuation.yield(scope);
                System.out.println("Continuation third run in " + Thread.currentThread().getName());
            }
        };
		LinkedBlockingDeque<Continuation> queue = new LinkedBlockingDeque();
	    for (int i = 0; i < 10; i++) {
			queue.offer(new Continuation(scope, target));
		}
		AtomicInteger ai = new AtomicInteger(0);

		Runnable r = new Runnable() {
            public void run() {
                try {
                    while (true) {
                        Continuation c = queue.take();
                        c.run();
                        if (c.isDone() == false) {
                            queue.offer(c);
                        } else {
							System.out.println("done");
							int res = ai.incrementAndGet();
							if(res == 10) {
								return;
							}
						}
                    }
                } catch(InterruptedException e) {
                }
            }
		}; 

        Thread thread = new Thread(r);
        thread.start();
		thread.join();
    }


    static void quux() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                //System.out.println("Continuation first run in " + Thread.currentThread().getName());
                Continuation.yield(scope);
                //System.out.println("Continuation second run in " + Thread.currentThread().getName());
                Continuation.yield(scope);
                //System.out.println("Continuation third run in " + Thread.currentThread().getName());
            }
        };
        LinkedBlockingDeque<Continuation> queue = new LinkedBlockingDeque();
        for (int i = 0; i < 1000; i++) {
            queue.offer(new Continuation(scope, target));
        }
        AtomicInteger ai = new AtomicInteger(0);

        Thread[] threads = new Thread[10];
        Runnable r = new Runnable() {
            public void run() {
                try {
                    while (true) {
                        Continuation c = queue.take();
                        c.run();
                        if (c.isDone() == false) {
                            queue.offer(c);
                        } else {
                            //System.out.println("done");
                            int res = ai.incrementAndGet();
                            if(res == 1000) {
								for (int i = 0; i < 10; i++) {
									if (threads[i] == Thread.currentThread()) {
										continue;
									}
									threads[i].interrupt();
								}
                                return;
                            }
                        }
                    }
                } catch(InterruptedException e) {
                }
            }
        };
        

        for (int i = 0; i < 10; i++) {
            threads[i] = new Thread(r);
        }

		for (int i = 0; i < 10; i++) {
            threads[i].start();
        }

        for (int i = 0; i < 10; i++) {
            threads[i].join(); 
        }
    }

}
