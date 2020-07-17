/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

public class TestNativePin {
    static {
        System.loadLibrary("TestNativePin");
    }
    private static ContinuationScope scope = new ContinuationScope("scope");

    native static void invokeJava();

    static void foo() {
      Continuation.yield(scope);
    }

    static public void main(String[] args) {
        TestPinNative();
        TestPinNativeMultiple();
    }

    private static boolean hasPinned = false;
    public static void TestPinNative() {
        Continuation cont = new Continuation(scope, () -> {
           System.out.println("enter cont");
           invokeJava();
           System.out.println("exit cont");
        }) {
           @Override
            protected void onPinned(Continuation.Pinned reason) {
                System.out.println("Pin " + reason);
                hasPinned = (reason == Continuation.Pinned.NATIVE);
            }
        };
        cont.run();
        System.out.println("hasPinned " + hasPinned);
        if (hasPinned == false) {
            throw new RuntimeException("fail TestPinNative");
        }
    }

    private static int count = 0;
    public static void TestPinNativeMultiple() {
        Continuation cont = new Continuation(scope, () -> {
           System.out.println("enter cont");
           invokeJava();
           invokeJava();
           Continuation.yield(scope);
           invokeJava();
           Continuation.yield(scope);
           System.out.println("exit cont");
        }) {
           @Override
            protected void onPinned(Continuation.Pinned reason) {
                System.out.println("Pin " + reason);
                count++;
            }
        };
        cont.run();
        System.out.println("count " + count);
        cont.run();
        System.out.println("count " + count);
        cont.run();
        System.out.println("count " + count);
        if (count != 3) {
            throw new RuntimeException("fail TestPinNativeMultiple");
        }
    }
}
