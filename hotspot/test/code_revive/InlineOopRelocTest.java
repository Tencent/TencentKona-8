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
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @run main/othervm InlineOopRelocTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class InlineOopRelocTest {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] {"TestInlineDummy"};

        // dump aot
        OutputAnalyzer output = TestCommon.testDump(appClass, "log=archive=trace",
                                                    "-XX:-TieredCompilation",
                                                    "-Xcomp",
                                                    "-XX:-BackgroundCompilation",
                                                    "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        output = TestCommon.testRunWithAOT(appClass, "log=restore=warning",
                                                    "-XX:-TieredCompilation",
                                                    "-Xcomp",
                                                    "-XX:-BackgroundCompilation",
                                                    "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldContain("Fail to find klass with name Dummy");
        output.shouldHaveExitValue(0);
        String[] expectedMethod = new String[] {"TestInlineDummy::foo", "Dummy::bar"};
        TestCommon.containAOTMethod(output, expectedMethod);
    }
}
