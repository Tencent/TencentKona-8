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
package com.tencent.crypto.provider.Cipher.SM4;
/*
 * utility class
 */

public class TestUtility {

    private static final String DIGITS = "0123456789abcdef";

    private TestUtility() {

    }

    public static String hexDump(byte[] bytes) {

        StringBuilder buf = new StringBuilder(bytes.length * 2);
        int i;

        buf.append("    "); // four spaces
        for (i = 0; i < bytes.length; i++) {
            buf.append(DIGITS.charAt(bytes[i] >> 4 & 0x0f));
            buf.append(DIGITS.charAt(bytes[i] & 0x0f));
            if ((i + 1) % 32 == 0) {
                if (i + 1 != bytes.length) {
                    buf.append("\n    "); // line after four words
                }
            } else if ((i + 1) % 4 == 0) {
                buf.append(' '); // space between words
            }
        }
        return buf.toString();
    }

    public static String hexDump(byte[] bytes, int index) {
        StringBuilder buf = new StringBuilder(bytes.length * 2);
        int i;

        buf.append("    "); // four spaces
        buf.append(DIGITS.charAt(bytes[index] >> 4 & 0x0f));
        buf.append(DIGITS.charAt(bytes[index] & 0x0f));
        return buf.toString();
    }

    public static boolean equalsBlock(byte[] b1, byte[] b2) {

        if (b1.length != b2.length) {
            return false;
        }

        for (int i = 0; i < b1.length; i++) {
            if (b1[i] != b2[i]) {
                return false;
            }
        }

        return true;
    }

    public static boolean equalsBlock(byte[] b1, byte[] b2, int len) {

        for (int i = 0; i < len; i++) {
            if (b1[i] != b2[i]) {
                return false;
            }
        }

        return true;
    }

}
