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

public class TestInlineDirect {

    public static void main(String[] args) throws Exception {
        int result = 0;
        for (int i = 0; i < 100000; i++) {
            result += test(i);
        }
        System.out.println(result);
    }

    static int test(int i) {
       return FinalClass.foo(i);
    }
}

final class FinalClass {
    static int foo(int i) {
        if (i % 3 == 1)
          return i * i * i * 2;
        if (i % 3 == 2)
          return i * (i + 1) * (i + 2) * 2;
        return i * (i - 2) * (i - 1) * 2;
    }
}
