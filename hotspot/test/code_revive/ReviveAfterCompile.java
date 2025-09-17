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

/*
 * @test
 * @summary Test if global symbol relocation info is found as expect
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/TestBasic.java
 * @compile test-classes/TestLeafDependency.java
 * @run main/othervm ReviveAfterCompile
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class ReviveAfterCompile {
    public static void main(String[] args) throws Exception {
        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa",
             "-XX:-TieredCompilation",
             "-Xcomp",
             "-XX:+UseG1GC",
             "TestBasic");

        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa",
             "-XX:-TieredCompilation",
             "-Xcomp",
             "-XX:+UseConcMarkSweepGC",
             "TestBasic");

        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa",
             "-XX:-TieredCompilation",
             "-Xcomp",
             "TestBasic");

        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa,log=oop=trace",
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,TestBasic.foo",
             "TestBasic");

        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa",
             "-XX:-TieredCompilation",
             "-Xcomp",
             "TestBasic");

        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa",
             "-XX:-TieredCompilation",
             "-Xcomp",
             "-XX:CompileCommand=compileonly,TestBasic.foo",
             "TestBasic");

        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa",
             "-XX:-TieredCompilation",
             "-XX:+PrintCompilation",
             "-Xcomp",
             "TestBasic");

        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa",
             "-XX:-TieredCompilation",
             "-Xcomp",
             "-XX:CompileCommand=compileonly,*.bar",
             "TestLeafDependency");

        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa,log=method=trace",
             "-XX:-TieredCompilation",
             "-Xcomp",
             "-XX:CompileCommand=reviveonly,java.lang.AbstractStringBuilder::append",
             "TestBasic");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        command[0] = command[0].replace("Options=save", "Options=restore");
        pb = ProcessTools.createJavaProcessBuilder(true, command);
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
    }
}
