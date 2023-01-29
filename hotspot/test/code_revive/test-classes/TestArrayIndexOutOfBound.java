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

public class TestArrayIndexOutOfBound {
    public static int LEN = 1024 * 1024;
    public static String[] arr = new String[LEN];
    public static void main(String[] args) throws Exception {
        boolean doThrow = false;
        if (args.length != 0) {
            doThrow = true;
        }

        for (int i = 0; i < LEN; i++) {
            foo(i);
        }

        if (!doThrow) return;

        for (int i = 0; i < LEN; i++) {
            try {
                foo(LEN + i);
            } catch (Exception e) {}
        }
    }

    public static String foo(int i) throws Exception {
        return arr[i];
    }
}
