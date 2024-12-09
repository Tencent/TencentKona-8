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
 * @summary Method with call site dependency shouldn't be dumped
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/CallSiteTest.java
 * @run main/othervm CallSiteDependencyTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class CallSiteDependencyTest {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] {"CallSiteTest"};

        // dump aot
        OutputAnalyzer output = TestCommon.testDump(appClass, "log=save=trace", "-XX:-TieredCompilation");
        output.shouldContain("CallSiteTest.bar");
        // following message shows fail to resolve oop fro java/lang/invoke/BoundMethodHandle$Species_LL
        // now emit can fail and skip revive these method
        // emit fail for oop 0x7abe4b300, java/lang/invoke/DirectMethodHandle
        output.shouldContain("Emit fail for oop");
        //output.shouldNotContain("exactInvoker");     // Should not dump method with callsite dependency
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());

        output = TestCommon.testDump(appClass, "log=save=trace,fatal_on_fail",
            "-XX:-TieredCompilation", "-XX:+UnlockDiagnosticVMOptions");

        System.out.println(output.getOutput());
        output.shouldContain("Emit fail for oop");
        output.shouldContain("fatal error: fatal_on_fail: Emit/Revive");
    }
}
