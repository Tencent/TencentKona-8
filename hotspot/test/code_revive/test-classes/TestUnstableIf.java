/*
 *
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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
