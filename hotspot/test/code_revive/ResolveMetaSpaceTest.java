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
 * @summary Resolve meta in MetaSpace for each csa
 * @library /testlibrary
 * @compile test-classes/C2.java
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @run main/othervm ResolveMetaSpaceTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import java.text.SimpleDateFormat;
import java.io.File;
import java.util.Date;

public class ResolveMetaSpaceTest {
    private static final SimpleDateFormat timeStampFormat =
        new SimpleDateFormat("HH'h'mm'm'ss's'SSS");

    public static void main(String[] args) throws Exception {
        String appJar = ClassFileInstaller.writeJar("jar.jar", "pkg/C2");
        String jar2 = ClassFileInstaller.writeJar("jar2.jar", "Dummy", "TestInlineDummy");

        // build The app
        String[] appClass = new String[] {"TestInlineDummy"};

        String csa_file = "resolveMetaSpaceTest_" + timeStampFormat.format(new Date()) + ".csa";
        TestCommon.setCurrentArchiveName(csa_file);

        // dump aot
        OutputAnalyzer output = TestCommon.testDump(appJar + ":" + jar2, appClass, "log=archive=trace",
                                                    "-XX:-TieredCompilation",
                                                    "-Xcomp",
                                                    "-XX:-BackgroundCompilation",
                                                    "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // class path isn't changed. all the meta should be resolved.
        String merged_csa_file = "resolveMetaSpaceTest_" + timeStampFormat.format(new Date()) + "_merged.csa";
        TestCommon.setCurrentArchiveName(merged_csa_file);
        output = TestCommon.testMergeCSA(appJar + ":" + jar2, "input_files=" + csa_file + ",log=merge=info", "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldContain("resovled TestInlineDummy.foo()Ljava/lang/String");
        output.shouldHaveExitValue(0);

        // the jar file is missing, the meta shouldn't be resolved.
        output = TestCommon.testMergeCSA(appJar, "input_files=" + csa_file + ",log=merge=info", "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldContain("unresolved TestInlineDummy.foo()Ljava/lang/String");
        output.shouldHaveExitValue(0);
  }
}
