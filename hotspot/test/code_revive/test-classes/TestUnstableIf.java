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

public class TestUnstableIf {
    public static void main(String[] args) {
        boolean test = true;
        if (args.length > 0) {
            test = false;
        }

        if (args.length == 2) {
            for (int i = 0; i < 3300; i++) {
                foo(!test);
            }
        }

        for (int i = 0; i < 1000000; i++) {
            foo(test);
        }
    }

    public static String foo(boolean test) {
        if (test) {
            return "taken";
        } else {
            return "untaken";
        }
    }
}
