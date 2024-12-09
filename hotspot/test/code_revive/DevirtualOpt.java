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
 * @summary Test devirtual opt record
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/TestDevirtual.java
 * @run main/othervm DevirtualOpt
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class DevirtualOpt {
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

        // all match with one klass
        String[] expect_outputs_one_all_match = {
            "morphism=1  klass1: Child1  default: trap",
            "Check opt records negative",
            "DeVirtual at method=TestDevirtual.test",
        };
        Test(expect_outputs_one_all_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestDevirtual",
             "1");

        // trap: not match
        String[] expect_outputs_one_trap_not_match = {
            "morphism=1  klass1: Child1  default: trap",
            "OptRecordDeVirtual: fail_with_trap klass not hit",
            "DeVirtual at method=TestDevirtual.test",
        };
        Test(expect_outputs_one_trap_not_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestDevirtual",
             "2", "3");

        // trap: one match
        String[] expect_outputs_one_trap_one_match = {
            "morphism=1  klass1: Child1  default: trap",
            "check_aot_method_opt_records: fail_with_trap",
            "DeVirtual at method=TestDevirtual.test",
        };
        Test(expect_outputs_one_trap_not_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestDevirtual",
             "1", "1", "1", "1","1", "1","1", "1","1", "1","1", "1", "1", "1","1", "1","1", "1","1", "1", "2");

        // generate csa with 2 devirtul target
        String[] expect_outputs_two = {
            "morphism=2  klass1: Child1  klass2: Child2  default: trap",
        };
        Test(expect_outputs_two,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "TestDevirtual",
             "1", "2");

         // single match
        String[] expect_outputs_two_match = {
            "morphism=2  klass1: Child1  klass2: Child2  default: trap",
            "Check opt records negative",
            "DeVirtual at method=TestDevirtual.test",
        };
        Test(expect_outputs_two_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestDevirtual",
             "1");

        // two match
        Test(expect_outputs_two_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestDevirtual",
             "1", "2", "1");

         // trap no match
         String[] expect_outputs_two_trap = {
            "morphism=2  klass1: Child1  klass2: Child2  default: trap",
            "OptRecordDeVirtual: fail_with_trap klass not hit",
            "DeVirtual at method=TestDevirtual.test",
         };
         Test(expect_outputs_two_trap,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestDevirtual",
             "3");
         // trap one match
         Test(expect_outputs_two_trap,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestDevirtual",
             "2", "3");

         // generate devirtual without trap
         String[] expect_outputs_virtual = {
            "morphism=0  klass1: Child1  default: virtual call",
         };
         Test(expect_outputs_virtual,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "-XX:TypeProfileMajorReceiverPercent=60",
             "TestDevirtual",
             "1", "1", "1", "1", "1", "1", "1", "1", "1", "2", "3");

         // one match
         String[] expect_outputs_virtual_match = {
            "morphism=0  klass1: Child1  default: virtual call",
            "Check opt records negative",
         };
         Test(expect_outputs_virtual_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "-XX:TypeProfileMajorReceiverPercent=60",
             "TestDevirtual",
             "1");

         // major match
         Test(expect_outputs_virtual_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "-XX:TypeProfileMajorReceiverPercent=60",
             "TestDevirtual",
             "1", "1", "2");

         // major not match
         String[] expect_outputs_virtual_no_match = {
            "morphism=0  klass1: Child1  default: virtual call",
            "Check opt records positive",
         };
         Test(expect_outputs_virtual_no_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "-XX:TypeProfileMajorReceiverPercent=60",
             "TestDevirtual",
             "1", "2", "2", "2", "2", "2", "2", "2", "2", "2", "2");

        // no match
         Test(expect_outputs_virtual_no_match,
             "-XX:CompileCommand=compileonly,TestDevirtual.*",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CompileCommand=exclude,*.main",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "-XX:TypeProfileMajorReceiverPercent=60",
             "TestDevirtual",
             "2", "2", "3");
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
