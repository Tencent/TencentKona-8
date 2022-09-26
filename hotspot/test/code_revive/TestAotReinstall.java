/*
 *
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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
/*
 * @test
 * @summary Test whether deopt method is installed again and again
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
