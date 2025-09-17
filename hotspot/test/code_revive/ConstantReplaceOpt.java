/*
 * Copyright (C) 2022, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
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
 */
/*
 * @test
 * @summary Test constant replace opt record
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/TestConstant.java
 * @run main/othervm ConstantReplaceOpt
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class ConstantReplaceOpt {
    public static void main(String[] args) throws Exception {
        String[] expect_outputs = {
            "OptRecord insert: ConstantReplace at klass: TestConstant field offset: 120",
            "OptRecord insert: ConstantReplace at klass: TestConstant field offset: 124",
            "OptRecord insert: ConstantReplace at klass: TestConstant field offset: 104",
            "OptRecord insert: ConstantReplace at klass: TestConstant field offset: 112",
            "OptRecord insert: ConstantReplace at klass: TestConstant field offset: 128",
        };
        String[] empty = {};
        Test(expect_outputs, empty,
             "-DintConst=2",
             "-DfloatConst=145.234",
             "-DdoubleConst=3.14",
             "-DlongConst=1234567",
             "-DboolConst=true",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestConstant.func1,TestConstant.func2,TestConstant.func3,TestConstant.func4,TestConstant.func5",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "TestConstant");

        // all type match
        String[] expect_outputs_all_match = {
            "Check opt records negative, score is -1",
            "revive success: TestConstant.func1()I",
            "revive success: TestConstant.func2()F",
            "revive success: TestConstant.func3()D",
            "revive success: TestConstant.func4()J",
            "revive success: TestConstant.func5()Z",
        };
        Test(expect_outputs_all_match, empty,
             "-DintConst=2",
             "-DfloatConst=145.234",
             "-DdoubleConst=3.14",
             "-DlongConst=1234567",
             "-DboolConst=true",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestConstant.func1,TestConstant.func2,TestConstant.func3,TestConstant.func4,TestConstant.func5",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestConstant");

        // one type not match
        String[] expect_outputs_one_not_match = {
            "Check opt records negative, score is -1",
            "revive success: TestConstant.func1()I",
            "revive success: TestConstant.func2()F",
            "revive fail: No usable or valid aot code version, TestConstant.func3()D",
            "revive success: TestConstant.func4()J",
            "revive success: TestConstant.func5()Z",
        };
        Test(expect_outputs_one_not_match, empty,
             "-DintConst=2",
             "-DfloatConst=145.234",
             "-DdoubleConst=3.145",
             "-DlongConst=1234567",
             "-DboolConst=true",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestConstant.func1,TestConstant.func2,TestConstant.func3,TestConstant.func4,TestConstant.func5",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestConstant");

        // two type not match
        String[] expect_outputs_two_not_match = {
            "Check opt records negative, score is -1",
            "revive success: TestConstant.func1()I",
            "revive success: TestConstant.func2()F",
            "revive fail: No usable or valid aot code version, TestConstant.func3()D",
            "revive success: TestConstant.func4()J",
            "revive fail: No usable or valid aot code version, TestConstant.func5()Z",
        };
        Test(expect_outputs_two_not_match, empty,
             "-DintConst=2",
             "-DfloatConst=145.234",
             "-DdoubleConst=3.145",
             "-DlongConst=1234567",
             "-DboolConst=false",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestConstant.func1,TestConstant.func2,TestConstant.func3,TestConstant.func4,TestConstant.func5",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestConstant");

        // all type not match
        String[] expect_outputs_all_not_match = {
            "revive fail: No usable or valid aot code version, TestConstant.func1()I",
            "revive fail: No usable or valid aot code version, TestConstant.func2()F",
            "revive fail: No usable or valid aot code version, TestConstant.func3()D",
            "revive fail: No usable or valid aot code version, TestConstant.func4()J",
            "revive fail: No usable or valid aot code version, TestConstant.func5()Z",
        };
        String[] unexpect_outputs_all_not_match = {
            "Check opt records negative, score is -1",
        };
        Test(expect_outputs_all_not_match, unexpect_outputs_all_not_match,
             "-DintConst=23",
             "-DfloatConst=145.2345",
             "-DdoubleConst=3.145",
             "-DlongConst=12345678",
             "-DboolConst=false",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestConstant.func1,TestConstant.func2,TestConstant.func3,TestConstant.func4,TestConstant.func5",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestConstant");

        // save with option "disable_constant_opt"
        String[] expect_opt_outputs = {
            "Emit method TestConstant.func1()I success",
            "Emit method TestConstant.func2()F success",
            "Emit method TestConstant.func3()D success",
            "Emit method TestConstant.func4()J success",
            "Emit method TestConstant.func5()Z success",
        };
        Test(expect_opt_outputs, empty,
             "-DintConst=2",
             "-DfloatConst=145.234",
             "-DdoubleConst=3.14",
             "-DlongConst=1234567",
             "-DboolConst=true",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestConstant.func1,TestConstant.func2,TestConstant.func3,TestConstant.func4,TestConstant.func5",
             "-XX:CodeReviveOptions=save,file=1.csa,log=save=trace,disable_constant_opt",
             "TestConstant");

        // same constant, match with disable_constant_opt
        String[] expect_opt_outputs_same_cons_match = {
            "revive success: TestConstant.func1()I",
            "revive success: TestConstant.func2()F",
            "revive success: TestConstant.func3()D",
            "revive success: TestConstant.func4()J",
            "revive success: TestConstant.func5()Z",
        };
        String[] unexpect_opt_outputs = {
            "Check opt records negative, score is -1",
        };
        Test(expect_opt_outputs_same_cons_match, unexpect_opt_outputs,
             "-DintConst=2",
             "-DfloatConst=145.234",
             "-DdoubleConst=3.14",
             "-DlongConst=1234567",
             "-DboolConst=true",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestConstant.func1,TestConstant.func2,TestConstant.func3,TestConstant.func4,TestConstant.func5",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace,disable_constant_opt",
             "TestConstant");

        // different constant, but match with disable_constant_opt
        String[] expect_opt_outputs_diff_cons_match = {
            "revive success: TestConstant.func1()I",
            "revive success: TestConstant.func2()F",
            "revive success: TestConstant.func3()D",
            "revive success: TestConstant.func4()J",
            "revive success: TestConstant.func5()Z",
        };
        Test(expect_opt_outputs_diff_cons_match, unexpect_opt_outputs,
             "-DintConst=23",
             "-DfloatConst=145.2345",
             "-DdoubleConst=3.145",
             "-DlongConst=12345678",
             "-DboolConst=false",
             "-XX:-TieredCompilation",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestConstant.func1,TestConstant.func2,TestConstant.func3,TestConstant.func4,TestConstant.func5",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace,disable_constant_opt",
             "TestConstant");
    }

    static void Test(String[] expect_outputs, String[] unexpect_outputs, String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
        for (String s : unexpect_outputs) {
            output.shouldNotContain(s);
        }
    }
}
