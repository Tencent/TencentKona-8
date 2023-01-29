/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

public class TestInstanceOf {
    static class A {
        public int a;
    }

    static class ChildA extends A {
        public int b;
    }

    static class B {
        public int b;
    }

    public static void main(String[] args) {
        Object a = null;
        ChildA ca = new ChildA();
        B b = new B();
        if (args.length == 0) {
            a = ca;
        } else if (args.length == 1) {
            a = b;
        }

        for (int i = 0; i < 10000000; i++) {
            foo(a);
        }
    }

    public static boolean foo(Object o) {
        return o instanceof A;
    }
}
