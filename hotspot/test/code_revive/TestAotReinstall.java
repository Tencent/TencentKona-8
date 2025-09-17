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
 * @summary Test whether deopt method is installed again and again
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/AotCodeDeopt.java
 * @run main/othervm TestAotReinstall
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class TestAotReinstall {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] {"AotCodeDeopt"};

        // dump aot
        OutputAnalyzer output = TestCommon.testDump(appClass, "log=method=trace",
                                                    "-XX:-TieredCompilation",
                                                    "-XX:CompileCommand=compileonly,*.foo",
                                                    "-XX:-BackgroundCompilation",
                                                    "-XX:-Inline");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        output = TestCommon.testRunWithAOT(appClass, "log=method=trace",
                                                    "-DenableDeopt=yes",
                                                    "-XX:-TieredCompilation",
                                                    "-XX:-BackgroundCompilation",
                                                    "-XX:+PrintCompilation",
                                                    "-XX:CompileCommand=compileonly,*.foo",
                                                    "-XX:-Inline");
        System.out.println(output.getOutput());
        output.shouldNotContain("3             AotCodeDeopt::foo");
        output.shouldHaveExitValue(0);
    }
}
