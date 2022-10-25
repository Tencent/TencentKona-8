/*
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
