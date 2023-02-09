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

public class TestArrayStore {
    static interface A {
        public void bar();
    }

    static class ChildA implements A {
        public void bar() {
            System.out.println("bar");
        }
    }

    static class ChildB implements A {
        public void bar() {
            System.out.println("bar1");
        }
    }

    public static void main(String[] args) {
        A[] ary = new A[1000];
        A a = null;
        ChildA ca = new ChildA();
        ChildB cb = new ChildB();
        if (args.length == 0) {
            a = ca;
        } else if (args.length == 1) {
            a = cb;
        }

        for (int i = 0; i < 10000000; i++) {
            foo(ary, a);
        }
    }

    public static void foo(A[] ary, A o) {
        ary[0] = o;
    }
}
