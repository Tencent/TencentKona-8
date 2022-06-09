/*
 *
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

/**
 * @test
 * @library /lib/testlibrary
 * @run main/othervm StackOverflowErrorTest
 * @summary Check whether the StackOverflowError can be throw normally
 */

import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;

public class StackOverflowErrorTest {
    public static class StackOverflowErrorExampleFiber {
        public static void recursivePrint(int num) {
            if (num != 0)
                recursivePrint(++num);
        }

        public static void main(String[] args) throws Exception {
            Runnable r1 = new Runnable() {
                public void run() {
                    StackOverflowErrorExampleFiber.recursivePrint(1);
                }
            };
            Thread thread = Thread.ofVirtual().name("vt1").start(r1);
            thread.join();
        }
    }

    public static void main(String[] args) throws Throwable {
        // Start a thread associated with a coroutine to produce StackOverflowError
        runTest("-Xint", StackOverflowErrorExampleFiber.class.getName());
        runTest("-Xcomp", "-XX:TieredStopAtLevel=1", StackOverflowErrorExampleFiber.class.getName());
        runTest("-Xcomp", "-XX:TieredStopAtLevel=3", StackOverflowErrorExampleFiber.class.getName());
    }

    static void runTest(String... cmds) throws Throwable {
       ProcessBuilder simplePb = ProcessTools.createJavaProcessBuilder(cmds);
       OutputAnalyzer output = new OutputAnalyzer(simplePb.start());
       output.shouldContain("StackOverflowError");
       output.shouldNotContain("fatal error");
       output.shouldNotContain("crash");
    }
}
