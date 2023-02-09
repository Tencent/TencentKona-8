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

public class TestSwitchTable {
    static String s;
    public static void main(String[] args) {
        invoke_foos();
    }

    public static void invoke_foos() {
        for (int i = 0; i < 1000000; i++) {
            foo(i);
        }
    }

    static int foo(int len) {
        int result = 0;
        switch (len % 10) {
            case 0:
            case 2:
                result = len * len;
                break;
            case 1:
                result = 10 * len;
                break;
            case 3:
            case 4:
                result = 33 + len;
                break;
            case 5:
            case 6:
                result = len / 3;
                break;
            case 7:
            case 8:
                result = len * len *len;
                break;
            case 9:
                result = (len + 1) + (len + 1);
                break;
        }
        return result * len;
    }
}
