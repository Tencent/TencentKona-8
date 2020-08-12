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
 *  @test
 *  @run testng GlobalMarkTest
 *  @summary Test an object is pointed by the stack of continuation, and it will not collect by GC
 *           when the continuation is not running.
 */

import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class GlobalMarkTest {
    static long count = 0;
    static ContinuationScope scope = new ContinuationScope("test");
    public static void main(String args[]) throws Exception {
        foo();
        System.out.println("finish foo");
        bar();
        System.out.println("finish bar");
    }

    static void foo() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                String tmp_str = new String("string in continuation");
                ReferenceQueue<String> queue = new ReferenceQueue<>();
                WeakReference<String> str_weak_ref = new WeakReference<>(tmp_str, queue);
                Continuation.yield(scope);

                Reference<? extends String> str_ref = queue.poll();
                assertEquals(str_ref, null);
                System.out.println("tmp_str : " + tmp_str + ", is not collected by GC");
                tmp_str = null;
                Continuation.yield(scope);

                str_ref = queue.poll();
                System.out.println("str_ref : " + str_ref);
                assertNotEquals(str_ref, null);
            }
        };

        Thread thread = new Thread(){
            public void run() {
                Continuation cont = new Continuation(scope, target);
                System.out.println("Continuation create in " + Thread.currentThread().getName());
                cont.run();

                System.gc();
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    System.out.println(e.toString());
                }
                cont.run();

                System.gc();
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    System.out.println(e.toString());
                }
                cont.run();
            }
        };

        thread.start();
        thread.join();
    }

    static void bar() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                String tmp_str = new String("string in continuation");
                ReferenceQueue<String> queue = new ReferenceQueue<>();
                WeakReference<String> str_weak_ref = new WeakReference<>(tmp_str, queue);
                Continuation.yield(scope);

                Reference<? extends String> str_ref = queue.poll();
                assertEquals(str_ref, null);
                System.out.println("tmp_str : " + tmp_str + ", is not collected by GC");
                tmp_str = null;
                Continuation.yield(scope);

                str_ref = queue.poll();
                System.out.println("str_ref : " + str_ref);
                assertNotEquals(str_ref, null);
            }
        };

        Continuation cont = new Continuation(scope, target);
        System.out.println("Continuation create in " + Thread.currentThread().getName());

        Runnable r = new Runnable() {
            public void run() {
                cont.run();
            }
        };
        Thread thread1 = new Thread(r);
        Thread thread2 = new Thread(r);

        thread1.start();
        thread1.join();

        System.gc();
        Thread.sleep(100);
        cont.run();

        System.gc();
        Thread.sleep(100);
        thread2.start();
        thread2.join();
    }
}
