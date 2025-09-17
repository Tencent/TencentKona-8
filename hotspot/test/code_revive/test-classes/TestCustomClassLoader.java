/*
 * Copyright (C) 2020, 2023, Tencent. All rights reserved.
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

import java.io.DataInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
public class TestCustomClassLoader {
    public static void main(String[] args) throws Exception {
	ClassLoader loader1 = TestClassLoader.loadByCustomLoader("TestBasic", "invokeFoos", TestCustomClassLoader.class.getClassLoader());
        ClassLoader loader2 = TestClassLoader.loadByCustomLoader("TestBasic", "invokeFoos", TestCustomClassLoader.class.getClassLoader());
	ClassLoader loader3 = TestClassLoader.loadByCustomLoader("TestBasic", "invokeFoos", loader2);
	ClassLoader loader4 = TestClassLoader.loadByCustomLoader("TestBasic", "invokeFoos", loader2);
    }
}
