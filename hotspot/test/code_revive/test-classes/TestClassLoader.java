/*
 * Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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
import java.io.DataInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;

class TestClassLoader extends ClassLoader {
    private String className;
    public TestClassLoader(String className, ClassLoader parent) {
        super(parent);
        this.className = className;
    }
    @Override
    public Class<?> loadClass(String name) throws ClassNotFoundException {
        if (name.equals(className)) {
            return getClass(name);
        } else {
            return super.loadClass(name);
        }
    }

    public static ClassLoader loadByCustomLoader(String className, String methodName,
                                                 ClassLoader parentLoader) throws Exception {
        TestClassLoader loader = new TestClassLoader(className, parentLoader);
        Class<?> clazz = loader.loadClass(className);
        clazz.getMethod(methodName).invoke(null);
        return loader;
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
