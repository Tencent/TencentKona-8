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
 * @summary csa with different fingerprint should be merged with different container.
 * @library /testlibrary
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @compile test-classes/LoadConstant.java
 * @run main/othervm DifferentFingerprint
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class DifferentFingerprint {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] { "TestInlineDummy" };
        // dump aot
        TestCommon.setCurrentArchiveName("mx1g.csa");
        OutputAnalyzer output = TestCommon.testDump(appClass, "log=save=fail",
            "-Xcomp",
            "-Xmx1g",
            "-XX:-BackgroundCompilation");
        output.shouldHaveExitValue(0);

        appClass = new String[] { "LoadConstant" };
        // dump aot
        TestCommon.setCurrentArchiveName("mx4g.csa");
        output = TestCommon.testDump(appClass, "log=save=trace",
            "-XX:-TieredCompilation",
            "-Xmx4g",
            "-XX:-BackgroundCompilation",
            "-XX:CompileCommand=compileonly,LoadConstant.foo");
        output.shouldContain("Emit method LoadConstant.foo()Ljava/lang/Object; success");
        output.shouldHaveExitValue(0);

        // dump aot with g1
        TestCommon.setCurrentArchiveName("g1.csa");
        output = TestCommon.testDump(appClass, "log=save=trace",
            "-XX:-TieredCompilation",
            "-Xmx4g",
            "-XX:+UseG1GC",
            "-XX:-BackgroundCompilation",
            "-XX:CompileCommand=compileonly,LoadConstant.foo");
        output.shouldContain("Emit method LoadConstant.foo()Ljava/lang/Object; success");
        output.shouldHaveExitValue(0);

        // dump aot with cms
        TestCommon.setCurrentArchiveName("cms.csa");
        output = TestCommon.testDump(appClass, "log=save=trace",
            "-XX:-TieredCompilation",
            "-Xmx4g",
            "-XX:+UseConcMarkSweepGC",
            "-XX:-BackgroundCompilation",
            "-XX:CompileCommand=compileonly,LoadConstant.foo");
        output.shouldContain("Emit method LoadConstant.foo()Ljava/lang/Object; success");
        output.shouldHaveExitValue(0);

        // dump aot with g1 + some cms options
        TestCommon.setCurrentArchiveName("g1_with_cms.csa");
        output = TestCommon.testDump(appClass, "log=save=trace",
            "-XX:-TieredCompilation",
            "-Xmx4g",
            "-XX:+UseG1GC",
            "-XX:+CMSIgnoreYoungGenPerWorker",
            "-XX:-BackgroundCompilation",
            "-XX:CompileCommand=compileonly,LoadConstant.foo");
        output.shouldContain("Emit method LoadConstant.foo()Ljava/lang/Object; success");
        output.shouldHaveExitValue(0);

        // dump aot with g1 + ContendedPaddingWidth
        TestCommon.setCurrentArchiveName("g1.csa");
        output = TestCommon.testDump(appClass, "log=save=trace",
            "-XX:-TieredCompilation",
            "-Xmx4g",
            "-XX:+UseG1GC",
            "-XX:-BackgroundCompilation",
            "-XX:ContendedPaddingWidth=8192",
            "-XX:CompileCommand=compileonly,LoadConstant.foo");
        output.shouldContain("Emit method LoadConstant.foo()Ljava/lang/Object; success");
        output.shouldHaveExitValue(0);


        TestCommon.setCurrentArchiveName("merged.csa");
        output = TestCommon.mergeCSA(null, "input_files=.,log=merge=trace", "-version");
        System.out.println(output.getOutput());
        output.shouldContain("Total valid csa file number");
        output.shouldContain("Total container number        : 5");
        output.shouldHaveExitValue(0);

        // run merged csa file
        appClass = new String[] { "LoadConstant" };
        output = TestCommon.runWithArchive(appClass, "log=restore=info",
            "-XX:-TieredCompilation",
            "-Xmx4g",
            "-XX:CompileCommand=compileonly,LoadConstant.foo");
        System.out.println(output.getOutput());
        output.shouldContain("Compiled method (c2 aot)");
        output.shouldContain("LoadConstant::foo");
        output.shouldHaveExitValue(0);

        TestCommon.setCurrentArchiveName("merged.csa");
        output = TestCommon.mergeCSA(null, "input_files=.,log=merge=trace,coverage=90", "-version");
        System.out.println(output.getOutput());
        output.shouldContain("Total valid csa file number");
        output.shouldContain("Total container number        : 4");
        output.shouldHaveExitValue(0);

        TestCommon.setCurrentArchiveName("merged.csa");
        output = TestCommon.mergeCSA(null, "input_files=.,log=merge=trace,max_file_size=10M", "-version");
        System.out.println(output.getOutput());
        output.shouldContain("Total valid csa file number");
        output.shouldContain("Total container number        : 5");
        output.shouldHaveExitValue(0);

        TestCommon.setCurrentArchiveName("merged.csa");
        output = TestCommon.mergeCSA(null, "input_files=.,log=merge=trace,max_file_size=10K", "-version");
        System.out.println(output.getOutput());
        output.shouldContain("Incorrect CodeReviveOptions:max_file_size no ending with M or m");
        output.shouldHaveExitValue(0);
    }
}
