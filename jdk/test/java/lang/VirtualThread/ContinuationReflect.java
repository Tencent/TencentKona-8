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
 * @run testng ContinuationReflect
 * @summary Basic test for continuation reflect
 */
import org.testng.annotations.Test;
import static org.testng.Assert.*;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

public class ContinuationReflect {
    static ContinuationScope scope = new ContinuationScope("scope");
    private static boolean pinned = false;

    @Test
    public static void test_construct() {
         pinned = false;
         Continuation cont = new MyContinuation(scope, () -> {
            try {
                Class clz = Test1.class;
                System.out.println(clz);
                Constructor constructor = clz.getConstructor();
                Test1 t = (Test1)constructor.newInstance();
                t.foo();
            } catch (Exception e) {
                e.printStackTrace();
                System.exit(-1);
            }
        });
        cont.run();
        cont.run();
        assertEquals(pinned, false);
    }

    @Test
    public static void test_method() {
         pinned = false;
         Continuation cont = new MyContinuation(scope, () -> {
            try {
                Class clz = Test1.class;
                System.out.println(clz);
                Method m = clz.getDeclaredMethod("yieldCall");
                m.invoke(null);
            } catch (Exception e) {
                e.printStackTrace();
                System.exit(-1);
            }
        });
        cont.run();
        cont.run();
        assertEquals(pinned, false);
    }

    private static class MyContinuation extends Continuation {
        MyContinuation(ContinuationScope scope, Runnable r) {
            super(scope, r);
        }

        @Override
        protected void onPinned(Continuation.Pinned reason) {
            pinned = true;
            System.out.println(reason);
        }
    }

    private static class Test1 {
        public Test1() {
            System.out.println("Test construct");
            Continuation.yield(scope);
            System.out.println("Test construct back");
        }

        void foo() {
            System.out.println("foo");
        }

        static void yieldCall() {
            System.out.println("Test yieldCall");
            Continuation.yield(scope);
            System.out.println("Test yieldCall back");
        }
    }
}
