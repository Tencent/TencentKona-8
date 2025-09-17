/*
 * Copyright (C) 2022, 2023, Tencent. All rights reserved.
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

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import sun.misc.Unsafe;

public class TestInterfaceKlassReviveAndRestore {
    public static interface Callable {
        public int normalField = 1;
        public static int f = 0;
        public void call();
    }

    public static interface DefaultInterface {
        default public void func() {
            System.out.println("default interface function");
        }
        public static void bar() {
            System.out.println("static interface function");
        }
    }

    public static interface DefaultInterface1 {
        public static void bar() {
            System.out.println("static interface function");
        }
    }

    public static class RealCall implements Callable {
        public void call() {
            System.out.println("real call: " + normalField);
        }
    }

    public static class RealDefaultInterface implements DefaultInterface {
    }

    public static class RealDefaultInterface1 implements DefaultInterface1 {
    }

    public static Class foo(int i) {
        if (i == 1) return DefaultInterface.class;
        if (i == 2) return DefaultInterface1.class;
        return Callable.class;
    }

    public static void main(String[] args) throws Exception {
        Class dummy = Callable.class;
        int param = 0;
        int input = 0;
        if (args.length > 0) {
           input = Integer.valueOf(args[0]);
        }

        if (input == 1) {
            RealCall rc = new RealCall();
        }

        if (input == 2) {
            RealCall rc = new RealCall();
            rc.call();
        }

        if (input == 3) {
            RealCall rc = new RealCall();
            Field field = Callable.class.getDeclaredField("f");
            field.setAccessible(true);
            Field modifiersField = Field.class.getDeclaredField("modifiers");
            modifiersField.setAccessible(true);
            modifiersField.setInt(field, field.getModifiers() & ~Modifier.FINAL);
            field.set(null, 1);
        }

        if (input == 4) {
            param = 1;
        }

        if (input == 5) {
            param = 1;
            RealDefaultInterface rdi = new RealDefaultInterface();
        }

        if (input == 6) {
            param = 2;
            DefaultInterface1.bar();
        }

        if (input == 7) {
            param = 2;
            RealDefaultInterface1 rdi = new RealDefaultInterface1();
        }

        if (input == 8) {
            param = 2;
            Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
            Field field = unsafeClass.getDeclaredField("theUnsafe");
            field.setAccessible(true);
            Unsafe unsafe = (Unsafe) field.get(null);
            unsafe.ensureClassInitialized(DefaultInterface1.class);
        }

        for (int i = 0; i < 1000000; i++) {
            foo(param);
        }
    }
}
