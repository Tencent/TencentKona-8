/*
 * Copyright (C) 2022, 2023, Tencent. All rights reserved.
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

import java.util.Hashtable;

public class TestArrayKlsReloc {
    public static Hashtable[] foo() {
        return new Hashtable[10];
    }

    public static Hashtable[][] foo1() {
        return new Hashtable[10][];
    }

    public static int[] foo2() {
        return new int[10];
    }

    public static int[][] foo3() {
        return new int[10][];
    }

    public static void main(String[] args) {
        for (int i = 0; i < 10000; i++) {
            foo();
            foo1();
            foo2();
            foo3();
        }
    }
}
