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
 * @run testng ContinuationYieldToError
 * @summary Basic test for continuation yieldTo error
 */
import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class ContinuationYieldToError {
    static ContinuationScope scope = new ContinuationScope("scope");
    public static int value = 0;
    public static void foo() {
        System.out.println("foo");
        value++;
    }

    // invoke Continuation yieldTo in Continuation started with run
    static boolean findEx = false;
    @Test
    public static void test_yieldToBetweenConts() {
        value = 0;
        Continuation cont1 = new Continuation(scope, () -> {
           System.out.println("enter cont1");
           foo();
           Continuation.yield(scope);
           foo();
           System.out.println("exit cont1");
        });
        Continuation cont2 = new Continuation(scope, () -> {
           System.out.println("enter cont2");
           foo();
           System.out.println(cont1);
           try {
              Continuation.yieldTo(cont1);
           } catch (IllegalStateException e) {
			  System.out.println(e);
              findEx = true;
           }
           foo();
           System.out.println("exit cont2");
        });
        cont1.run();
        cont2.run();
        assertEquals(findEx, true);
        cont1.run();
    }
}
