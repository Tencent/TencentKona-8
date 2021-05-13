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
    }
}

