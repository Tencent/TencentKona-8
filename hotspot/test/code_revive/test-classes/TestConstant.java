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
public class TestConstant {
    final static int intConst = Integer.parseInt(System.getProperty("intConst"));
    final static float floatConst = Float.parseFloat(System.getProperty("floatConst"));
    final static double doubleConst = Double.parseDouble(System.getProperty("doubleConst"));
    final static long longConst = Long.parseLong(System.getProperty("longConst"));
    final static boolean boolConst = Boolean.parseBoolean(System.getProperty("boolConst"));

    public static void main(String[] args) throws Exception {
        for(int i = 0; i < 100000; i++) {
            func1();
            func2();
            func3();
            func4();
            func5();
        }
    }
    public static int func1() {
        return intConst;
    }
    public static float func2() {
        return floatConst;
    }
    public static double func3() {
        return doubleConst;
    }
    public static long func4() {
        return longConst;
    }
    public static boolean func5() {
        return boolConst;
    }
}

