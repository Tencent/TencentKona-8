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
 * @summary generate null indexed dependency
 * @library /testlibrary
 * @compile test-classes/TestUniqConcreteMethod.java
 * @run main/othervm NullDepCompare
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class NullDepCompare {
    public static void main(String[] args) throws Exception {
        String[] null_output = new String[0];
        Test(null_output,
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,disable_check_dir,file=1.csa,log=opt=trace",
             "TestUniqConcreteMethod");

        Test(null_output,
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=2.csa,log=opt=trace",
             "TestUniqConcreteMethod");

        String[] expect_merge = {
           "TestUniqConcreteMethod.foo(LTestUniqConcreteMethod$B1;)I, count 2",
        };
        Test(expect_merge,
            "-XX:CodeReviveOptions=merge,disable_check_dir,log=merge=info,file=a.csa,input_files=./");

        String[] expect_outputs = {
            "unique_concrete_method null TestUniqConcreteMethod$B1.func()I",
        };
        Test(expect_outputs, "-XX:CodeReviveOptions=print_opt,file=a.csa", "-XX:+UnlockDiagnosticVMOptions");
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
