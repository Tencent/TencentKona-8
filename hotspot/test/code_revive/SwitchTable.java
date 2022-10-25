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
 * @summary Test if internal word relocation is found as expect
 * @library /testlibrary
 * @compile test-classes/TestSwitchTable.java
 * @run main/othervm SwitchTable
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class SwitchTable {
    public static void main(String[] args) throws Exception {
        String[] expect_outputs = {
            "internal word offset 0",
        };
        String[] expect_restore_outputs = {
            "Compiled method (c2 aot)",
            "TestSwitchTable::foo",
        };
        Test(expect_outputs,
             "-XX:CodeReviveOptions=save,log=archive=trace,file=1.csa",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileCommand=compileonly,TestSwitchTable.foo",
             "TestSwitchTable");

        Test(expect_restore_outputs,
             "-XX:CodeReviveOptions=restore,log=archive=trace,file=1.csa",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:+UnlockDiagnosticVMOptions",
             "-XX:+PrintNMethods",
             "-XX:CompileCommand=compileonly,TestSwitchTable.foo",
             "TestSwitchTable");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        String str = output.getOutput();
        int occur = str.split("internal word offset", -1).length - 1;
        Asserts.assertEQ(occur, 11);;
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
    }
}
