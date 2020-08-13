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
 * @run testng PinTest
 * @summary Basic test for continuation, test create/run/yield/resume/stop
 */
import java.util.concurrent.*;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class PinTest {
    static long count = 0;

    // continuation set in critical section and yield
    @Test
    public static void critical_section_test_continuation() throws Exception {
        ContinuationScope scope = new ContinuationScope("scope");
        Continuation cont = new Continuation(scope, () -> {
            System.out.println("critical_section_test_continuation start");
            Continuation.pin();
            try {
                Continuation.yield(scope);
            } catch (IllegalStateException e) {
                System.out.println("critical_section_test_continuation is pinned expect true: " + Continuation.isPinned(scope));
                assertEquals(Continuation.isPinned(scope), true);
                System.out.println(e.toString());
            } 

            Continuation.unpin();
            System.out.println("critical_section_test_continuation is pinned expect false: " + Continuation.isPinned(scope));
            assertEquals(Continuation.isPinned(scope), false);
            boolean result = Continuation.yield(scope);
            System.out.println(result);
        });
        cont.run();
        cont.run();
    }

    @Test
    public static void monitor_test_continuation() throws Exception {
        Object o = new Object();
        ContinuationScope scope = new ContinuationScope("scope");
        Continuation cont = new Continuation(scope, () -> {
            System.out.println("monitor_test_continuation start");
            synchronized(o) {
                try {
                    System.out.println("critical_section_test_continuation is pinned expect true: " + Continuation.isPinned(scope));
                    assertEquals(Continuation.isPinned(scope), true);
                    Continuation.yield(scope);
                } catch (IllegalStateException e) {
                    System.out.println(e.toString());
                }
            }

            System.out.println("critical_section_test_continuation is pinned expect false: " + Continuation.isPinned(scope));
            assertEquals(Continuation.isPinned(scope), false);
            boolean result = Continuation.yield(scope);
            System.out.println(result);
        });
        cont.run();
        cont.run();
    }

    @Test
    public static void monitor_custom_test_continuation() throws Exception {
        Object o = new Object();
        ContinuationScope scope = new ContinuationScope("scope");
        Continuation cont = new Continuation(scope, () -> {
            System.out.println("monitor_custom_test_continuation start");
            synchronized(o) {
                boolean result = Continuation.yield(scope);
                System.out.println("yield result : " + result);
            }
            boolean result = Continuation.yield(scope);
            System.out.println("yield result : " + result);
        }) {
               protected void onPinned(Pinned reason) {
                   System.out.println("Pinned and block here");
                   System.out.println("critical_section_test_continuation is pinned expect true: " + Continuation.isPinned(scope));
                   assertEquals(Continuation.isPinned(scope), true);
               }
        };
        while (!cont.isDone()) {
            cont.run();
        }
    }

    // jni frame test
}
