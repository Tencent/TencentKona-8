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
 * @summary Test whether load const is in the relocation when revive is on
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/LoadConstant.java
 * @run main/othervm ValidateCheckTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class ValidateCheckTest {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] {"LoadConstant"};

        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:CodeReviveOptions=save,log=save=trace,file=LoadConstantTest.csa",
            "-XX:-TieredCompilation",
            "-XX:-BackgroundCompilation",
            "-Xmx1g",
            "-XX:CompileCommand=compileonly,LoadConstant.foo",
            appClass[0]);


        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        // should not contain emit oop fail
        output.shouldNotContain("Emit fail for oop");
        output.shouldContain("Emit method LoadConstant.foo()Ljava/lang/Object; success");
        output.shouldHaveExitValue(0);

        // restore aot with different mx
        pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:CodeReviveOptions=restore,file=LoadConstantTest.csa,log=restore=trace",
            "-XX:-TieredCompilation",
            "-XX:-BackgroundCompilation",
            "-Xmx4g",
            "-XX:CompileCommand=compileonly,LoadConstant.foo",
            appClass[0]);

        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("Different setting of CompressedOops");
        output.shouldContain("Fail to load CSA file LoadConstantTest.csa");
        output.shouldHaveExitValue(0);

        // restore aot with different GC
        pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:CodeReviveOptions=restore,file=LoadConstantTest.csa,log=restore=trace",
            "-XX:-TieredCompilation",
            "-XX:-BackgroundCompilation",
            "-XX:+UseConcMarkSweepGC",
            "-XX:CompileCommand=compileonly,LoadConstant.foo",
            appClass[0]);

        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("Different value of UseParallelGC");
        output.shouldContain("Fail to load CSA file LoadConstantTest.csa");
        output.shouldHaveExitValue(0);

    }
}
