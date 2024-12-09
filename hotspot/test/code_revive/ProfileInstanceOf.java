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
 * @summary Test profiled receiver for instanceof
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/TestInstanceOf.java
 * @run main/othervm ProfileInstanceOf
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class ProfileInstanceOf {
    public static void main(String[] args) throws Exception {
        String[] expect_outputs = {
            "OptRecord insert: ProfiledReceiver at method=TestInstanceOf.foo(Ljava/lang/Object;)Z bci=1",
        };
        Test(expect_outputs,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInstanceOf.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "TestInstanceOf");

        // all match
        String[] expect_outputs_one_all_match = {
            "OptRecord check: ProfiledReceiver at method=TestInstanceOf.foo(Ljava/lang/Object;)Z bci=1",
            "klass: TestInstanceOf$ChildA",
            "Check opt records negative",
        };
        Test(expect_outputs_one_all_match,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInstanceOf.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestInstanceOf");

        // trap: not match
        String[] expect_outputs_one_trap_not_match = {
            "OptRecord check: ProfiledReceiver at method=TestInstanceOf.foo(Ljava/lang/Object;)Z bci=1",
            "klass: TestInstanceOf$ChildA",
            "Invalid neg count in opt profiled receiver",
        };
        Test(expect_outputs_one_trap_not_match,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInstanceOf.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestInstanceOf",
             "1");

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
