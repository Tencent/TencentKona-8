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
 * @summary Support for field klass relocation
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/TestHashTableGet.java
 * @run main/othervm FieldKlassRelocTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

public class FieldKlassRelocTest {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] {"TestHashTableGet"};

        String csa_file = "aot-fieldKlassRelocTest.csa";
        TestCommon.setCurrentArchiveName(csa_file);

        // dump aot
        OutputAnalyzer output = TestCommon.testDump(appClass, "perf,log=save=trace",
            "-XX:-TieredCompilation",
            "-Xcomp",
            "-XX:CompileCommand=compileonly,java/lang/invoke/MethodHandleImpl*.*",
            "-XX:-BackgroundCompilation");

        System.out.println(output.getOutput());
        output.shouldNotContain("Emit fail"); // no emit fail for in method trace
        // emit method java.lang.invoke.MethodHandleImpl$1.<init>([Ljava/lang/Object;)V success
        output.shouldContain("AOT save Information");
        output.shouldHaveExitValue(0);

        // restore aot
        output = TestCommon.testRunWithAOT(appClass, "perf,log=restore=trace",
            "-XX:-TieredCompilation",
            "-Xcomp",
            "-XX:+CITime",
            "-XX:CompileCommand=compileonly,java/lang/invoke/MethodHandleImpl*.*",
            "-XX:-BackgroundCompilation");

        System.out.println(output.getOutput());
        output.shouldContain("Restore Method time");
        output.shouldContain("Create nmethod time");
        output.shouldHaveExitValue(0);

        output = TestCommon.testRunWithAOT(appClass, "perf,log=restore=trace",
            "-XX:-TieredCompilation",
            "-Xcomp",
            "-XX:+CITime",
            "-XX:CompileCommand=compileonly,java/lang/invoke/MethodHandleImpl*.*",
            "-XX:-BackgroundCompilation");

        System.out.println(output.getOutput());
        output.shouldContain("Restore Method time");
        output.shouldHaveExitValue(0);

        // merge csa file
        output = TestCommon.testMergeCSA(null, "input_files=" + csa_file + ",log=merge=info", "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldContain("Add candidate code java/lang/invoke/MethodHandleImpl$2.<init>()V");
        output.shouldHaveExitValue(0);
    }
}
