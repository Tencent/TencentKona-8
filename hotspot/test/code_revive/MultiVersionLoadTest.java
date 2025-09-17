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
 * @summary generate merge and load all versions
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/TestDevirtual.java
 * @run main/othervm MultiVersionLoadTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class MultiVersionLoadTest {
    public static void main(String[] args) throws Exception {
        String[] expect_outputs = {
            "morphism=1  klass1: Child1  default: trap",
        };
        Test(expect_outputs,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "TestDevirtual",
             "1");

        String[] expect_outputs_2 = {
            "morphism=1  klass1: Child2  default: trap",
        };
        Test(expect_outputs_2,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=2.csa,log=opt=trace",
             "TestDevirtual",
             "2");


         String[] expect_merge = {
            "method TestDevirtual.test([LBase;I)I, count 2",
         };
         Test(expect_merge,
             "-XX:CodeReviveOptions=merge,log=merge=info,file=a.csa,input_files=./");

         String[] expect_print = {
            "morphism=1  klass1: Child1  default: trap",
            "morphism=1  klass1: Child2  default: trap",
            "TestDevirtual.test([LBase;I)I version 0",
            "TestDevirtual.test([LBase;I)I version 1",
            "evol_method TestDevirtual.test([LBase;I)I",
            "evol_method Child1.foo(I)I",
            "evol_method Child2.foo(I)I",
         };
         Test(expect_print,
             "-XX:CodeReviveOptions=print_opt,file=a.csa", "-XX:+UnlockDiagnosticVMOptions");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        String str = output.getOutput();
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
    }
}
