/*
 * Copyright (C) 2020, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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
 * @run testng ContinuationStackTrace
 * @summary Basic test for continuation get stack trace
 */
import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class ContinuationStackTrace {
    public static int value = 0;
    public static void foo() {
        System.out.println("foo");
        value++;
    }

    @Test
    public static void test_yield() {
        ContinuationScope scope = new ContinuationScope("scope");
        value = 0;
        Continuation cont = new Continuation(scope, () -> {
           System.out.println("enter cont");
           foo();
           Continuation.yield(scope);
           foo();
           System.out.println("exit cont");
        });
        assertEquals(value, 0);
        System.out.println(value);
        StackTraceElement[] stacks = cont.getStackTrace();
        assert(stacks.length == 0);
        cont.run();
        stacks = cont.getStackTrace();
        for (int i = 0; i < stacks.length; i++) {
            System.out.println(stacks[i] + " " + i);
        }
        assert(stacks.length == 5);
        System.out.println(value);
        assertEquals(value, 1);
        value += 2;
        cont.run();
        stacks = cont.getStackTrace();
        assert(stacks.length == 0);
        System.out.println(value);
        assertEquals(value, 4);
    }
}
