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
public class TestLeafDependency {
    static abstract class Dummy {
        public Object p;

        public Object getObj() {
            return new Object();
        }

        public synchronized Object bar() {
            if (p == null) {
                p = getObj();
            }
            return p;
        }
    }

    static class Child extends Dummy {
    }

    static class GrandChild extends Child {
    }

    public static void main(String[] args) {
        if (args.length != 0) {
            Dummy t1 = new GrandChild();
        }
        Dummy t = new Child();

        for (int i = 0; i < 500000; i++) {
            t.bar();
        }
    }
}
