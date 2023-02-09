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

public class TestInlineVirtual {

    public static void main(String[] args) throws Exception {
        int result = 0;
        Base instances[] = new Base[args.length];
        for (int i = 0; i < args.length; i++) {
            int child = Integer.parseInt(args[i]);
	    if (child == 1) {
                instances[i] = new Child1();
	    } else if (child == 2) {
                instances[i] = new Child2();
            } else if (child == 3) {
                instances[i] = new Child3();
            }
        }
        new Child1();
        new Child2();
        new Child3();
        for (int i = 0; i < 100000; i++) {
            result += test(instances, i);
        }
        System.out.println(result);
    }

    static int test(Base instances[], int i) {
        int sum = 0;
        new Child2();
        for (Base b : instances) {
            sum += b.foo(i);
        }
        return sum;
    }
}

class Base {
    int foo(int i) {
         return i + 2;
    }
}

class Child1 extends Base {
    int foo(int i) {
        new Child1();
        new Child2();
        new Child3();
        return i * i * i * 2;
    }
}

class Child2 extends Base {
    int foo(int i) {
        return i * 3;
    }
}

class Child3 extends Base {
    int foo(int i) {
        return i * 4;
    }
}
