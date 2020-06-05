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
import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class PinTest {
    static long count = 0;
    public static void main(String args[]) throws Exception {
        critical_section_test_continuation();
        System.out.println("finish critical_section_test_continuation 1");
        monitor_test_continuation();
        System.out.println("finish monitor_test_continuation 1");
        monitor_custom_test_continuation();
        System.out.println("finish monitor_custom_test_continuation 1");
    }

    // continuation set in critical section and yield
    static void critical_section_test_continuation() throws Exception {
        ContinuationScope scope = new ContinuationScope("scope");
        Continuation cont = new Continuation(scope, () -> {
            System.out.println("critical_section_test_continuation start");
            Continuation.pin();
            try {
                Continuation.yield();
            } catch (IllegalStateException e) {
                System.out.println("critical_section_test_continuation is pinned expect true: " + Continuation.isPinned());
                assertEquals(Continuation.isPinned(), true);
                System.out.println(e.toString());
            } 

            Continuation.unpin();
            System.out.println("critical_section_test_continuation is pinned expect false: " + Continuation.isPinned());
            assertEquals(Continuation.isPinned(), false);
            boolean result = Continuation.yield();
            System.out.println(result);
        });
        cont.run();
        cont.run();
    }

    static void monitor_test_continuation() throws Exception {
        Object o = new Object();
        ContinuationScope scope = new ContinuationScope("scope");
        Continuation cont = new Continuation(scope, () -> {
            System.out.println("monitor_test_continuation start");
            synchronized(o) {
                try {
                    System.out.println("critical_section_test_continuation is pinned expect true: " + Continuation.isPinned());
                    assertEquals(Continuation.isPinned(), true);
                    Continuation.yield();
                } catch (IllegalStateException e) {
                    System.out.println(e.toString());
                }
            }

            System.out.println("critical_section_test_continuation is pinned expect false: " + Continuation.isPinned());
            assertEquals(Continuation.isPinned(), false);
            boolean result = Continuation.yield();
            System.out.println(result);
        });
        cont.run();
        cont.run();
    }

    static void monitor_custom_test_continuation() throws Exception {
        Object o = new Object();
        ContinuationScope scope = new ContinuationScope("scope");
        Continuation cont = new Continuation(scope, () -> {
            System.out.println("monitor_custom_test_continuation start");
            synchronized(o) {
                boolean result = Continuation.yield();
                System.out.println("yield result : " + result);
            }
            boolean result = Continuation.yield();
            System.out.println("yield result : " + result);
        }) {
               protected void onPinned(Pinned reason) {
                   System.out.println("Pinned and block here");
                   System.out.println("critical_section_test_continuation is pinned expect true: " + Continuation.isPinned());
                   assertEquals(Continuation.isPinned(), true);
               }
        };
        System.out.println("critical_section_test_continuation is pinned expect false: " + Continuation.isPinned());
        assertEquals(Continuation.isPinned(), false);
        while (!cont.isDone()) {
            cont.run();
        }
    }

    // jni frame test
}
