/*
 *
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

public class Test {
    public static void main(String args[]) throws Exception {
        TestRedefine.print();
        SubTestRedefine.print();
        SubSubTestRedefine.print();
        TestRedefine.test();
        TestRedefine.printInterface();

        while(RedefineInterface.looping());

        TestRedefine.print();
        SubTestRedefine.print();
        SubSubTestRedefine.print();
        TestRedefine.test();
        TestRedefine.printInterface();
    }
}

