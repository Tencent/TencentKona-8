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
 * @summary classpath mismatch between dump time and execution time
 * @library /testlibrary
 * @compile test-classes/C2.java
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @run main/othervm WrongClasspath
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import java.io.File;

public class WrongClasspath {

    public static void main(String[] args) throws Exception {
        String appJar = ClassFileInstaller.writeJar("jar.jar", "pkg/C2");
        String jar2 = ClassFileInstaller.writeJar("jar2.jar", "Dummy", "TestInlineDummy");

        // build The app
        String[] appClass = new String[] {"TestInlineDummy"};

        // dump aot
        OutputAnalyzer output = TestCommon.testDump(appJar + ":" + jar2, appClass, "log=save=trace",
                                                    "-XX:-TieredCompilation",
                                                    "-Xcomp",
                                                    "-XX:-BackgroundCompilation",
                                                    "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        output = TestCommon.testRunWithAOT(jar2, appClass, "log=restore=fail",
                                           "-XX:-TieredCompilation",
                                           "-Xcomp",
                                           "-XX:-BackgroundCompilation",
                                           "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldContain("Run time APP classpath is shorter than the one at dump time");
        output.shouldContain("Actual = ");
        output.shouldContain("Expected = ");
        output.shouldHaveExitValue(0);

        output = TestCommon.testRunWithAOT(jar2 + ":" + appJar, appClass, "log=restore=fail",
                                           "-XX:-TieredCompilation",
                                           "-Xcomp",
                                           "-XX:-BackgroundCompilation",
                                           "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldContain("APP classpath mismatch");
        output.shouldContain("Actual = ");
        output.shouldContain("Expected = ");
        output.shouldHaveExitValue(0);

        output = TestCommon.testRunWithAOT(jar2, appClass, "disable_validate_check,log=restore=fail",
                                           "-XX:-TieredCompilation",
                                           "-Xcomp",
                                           "-XX:-BackgroundCompilation",
                                           "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldNotContain("Run time APP classpath is shorter than the one at dump time");
        output.shouldHaveExitValue(0);
  }
}
