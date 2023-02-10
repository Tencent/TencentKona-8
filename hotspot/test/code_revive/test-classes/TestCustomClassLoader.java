/*
 * Copyright (C) 2020, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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
        TestClassLoader loader = new TestClassLoader(TestCustomClassLoader.class.getClassLoader());
        Class<?> clazz = loader.loadClass("TestBasic");
        clazz.getMethod("invoke_foos").invoke(null);
    }
}

class TestClassLoader extends ClassLoader {
    public TestClassLoader(ClassLoader parent) {
        super(parent);
    }
    @Override
    public Class<?> loadClass(String name) throws ClassNotFoundException {
        if (name.equals("TestBasic")) {
            System.out.println(name);
            return getClass(name);
        } else {
            return super.loadClass(name);
        }
    }
    private Class<?> getClass(String name) throws ClassNotFoundException {
        String file = name.replace('.', File.separatorChar) + ".class";
        try {
            byte[] byteArr = loadClassData(file);
            Class<?> c = defineClass(name, byteArr, 0, byteArr.length);
            resolveClass(c);
            return c;
        } catch (IOException e) {
            return null;
        }
    }

    private byte[] loadClassData(String file) throws IOException {
        InputStream stream = getClass().getClassLoader().getResourceAsStream(file);
        int size = stream.available();
        byte buff[] = new byte[size];
        DataInputStream in = new DataInputStream(stream);
        in.readFully(buff);
        in.close();
        return buff;
    }
}
