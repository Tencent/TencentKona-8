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

    @Test
    public static void foo() throws Exception {
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

    @Test
    public static void bar() throws Exception {
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
