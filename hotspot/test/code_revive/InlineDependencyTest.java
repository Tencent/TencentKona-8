/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

/*
 * @test
 * @summary Test whether inline method is in dependency array when revive is on
 * @library /testlibrary
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @run main/othervm InlineDependencyTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class InlineDependencyTest {
    public static boolean isDebugVM() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        if (output.firstMatch("debug") != null) {
            return true;
        } else {
            return false;
        }
    }

    public static void main(String[] args) throws Exception {
        if (!isDebugVM()) {
            return;
        }

        // build The app
        String[] appClass = new String[] {"TestInlineDummy"};

        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:CodeReviveOptions=save,file=TestInlineDummy.csa,log=save=info",
            "-XX:-TieredCompilation",
            "-XX:-BackgroundCompilation",
            "-Xcomp",
            "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar",
            "-XX:ReviveOnly=Dummy.bar",
            "-XX:+PrintDependencies",
            appClass[0]);


        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("\'foo\' \'()Ljava/lang/String;\' in \'TestInlineDummy\'\nDependency of type evol_method");
        output.shouldContain("\'bar\' \'()Ljava/lang/String;\' in \'Dummy");
        output.shouldHaveExitValue(0);

        // restore aot
        pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:CodeReviveOptions=restore,file=TestInlineDummy.csa,log=restore=info",
            "-XX:-TieredCompilation",
            "-XX:-BackgroundCompilation",
            "-Xcomp",
            "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar",
            "-XX:ReviveOnly=Dummy.bar",
            "-XX:+PrintDependencies",
            "-XX:+UnlockDiagnosticVMOptions",
            "-XX:+PrintNMethods",
            appClass[0]);

        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("Compiled method (c2 aot)");
        output.shouldContain("Dummy::bar");
        output.shouldHaveExitValue(0);
    }
}
