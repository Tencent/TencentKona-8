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

public class MultiVersionCase {
    static String s;
    public static void main(String[] args) {
        int v = 0;
        if (args.length == 1) {
            v = Integer.parseInt(args[0]);
       }
        for (int i = 0; i < 1000000; i++) {
            foo(v);
        }
    }

    static void foo(int v) {
        if (v == 0) {
            s = "v0";
        }
        if (v == 1) {
            s = "v1";
        }
        if (v == 2) {
            s = "v2";
        }
        if (v == 3) {
            s = "v3";
        }
        if (v == 4) {
            s = "v4";
        }
        if (v == 5) {
            s = "v5";
        }
        if (v == 6) {
            s = "v6";
        }
        if (v == 7) {
            s = "v7";
        }
        if (v == 8) {
            s = "v8";
        }
        if (v == 9) {
            s = "v9";
        }
        if (v == 10) {
            s = "v10";
        }
        if (v == 11) {
            s = "v11";
        }
        if (v == 12) {
            s = "v12";
        }
        if (v == 13) {
            s = "v13";
        }
        if (v == 14) {
            s = "v14";
        }
        if (v == 15) {
            s = "v15";
        }
        if (v == 16) {
            s = "v16";
        }
        if (v == 17) {
            s = "v17";
        }
    }
}
