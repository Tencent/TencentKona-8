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
public class TestInlineOnlyChild {

    public static void main(String[] args) throws Exception {
        int result = 0;
        Base instances[] = new Base[args.length];
        for (int i = 0; i < args.length; i++) {
            int child = Integer.parseInt(args[i]);
	    if (child == 1) {
                instances[i] = new Child1();
	    } else if (child == 2) {
            } else if (child == 3) {
            }
        }
        for (int i = 0; i < 100000; i++) {
            result += test(instances, i);
        }
        System.out.println(result);
    }

    static int test(Base instances[], int i) {
        int sum = 0;
        for (Base b : instances) {
            sum += b.foo(i);
        }
        return sum;
    }
}

abstract class Base {
    public abstract int foo(int i);
}

class Child1 extends Base {
    public int foo(int i) {
        return (i + 1) * (i - 2) * (i / 2) * 2 + i * i * i;
    }
}
