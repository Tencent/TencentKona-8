/*
 * Copyright (C) 2023, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
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
 */

public class TestRedefine implements RedefineInterface {
    public static void main(String args[]) {
        test();
    }

    public static void print() {
        System.out.println("Original TestRedefine.print().");
    }

    public static void printInterface() {
        new TestRedefine().interfacePrint();
    }

    public static void test() {
        int total = 0;
        for (int i = 0; i < 20000; i++) {
            total += add(i);
        }
        System.out.println(total);
    }

    private static int innerAdd(int sum, int x) {
        return sum + x;
    }

    private static int add(int idx) {
        int sum = 0;
        for (int i = 0; i < idx; i++) {
            sum = innerAdd(sum, i);
        }
        return sum;
    }
}

class SubTestRedefine extends TestRedefine {
    public static void print() {
        System.out.println("SubTestRedefine.print");
    }
}

class SubSubTestRedefine extends SubTestRedefine {
    public static void print() {
        System.out.println("SubSubTestRedefine.print");
    }
}
