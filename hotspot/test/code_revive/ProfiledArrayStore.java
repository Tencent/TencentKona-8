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
 * @summary Test profiled receiver for array store
 * @library /testlibrary
 * @compile test-classes/TestArrayStore.java
 * @run main/othervm ProfiledArrayStore
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class ProfiledArrayStore {
    public static void main(String[] args) throws Exception {
        String[] expect_outputs = {
            "OptRecord insert: ProfiledReceiver at method=TestArrayStore.foo([LTestArrayStore$A;LTestArrayStore$A;)V bci=3",
        };
        Test(expect_outputs,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestArrayStore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "TestArrayStore");

        // all match
        String[] expect_outputs_one_all_match = {
            "OptRecord check: ProfiledReceiver at method=TestArrayStore.foo([LTestArrayStore$A;LTestArrayStore$A;)V bci=3",
            "klass: TestArrayStore$ChildA",
            "Check opt records negative",
        };
        Test(expect_outputs_one_all_match,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestArrayStore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestArrayStore");

        // trap: not match
        String[] expect_outputs_one_trap_not_match = {
            "OptRecord check: ProfiledReceiver at method=TestArrayStore.foo([LTestArrayStore$A;LTestArrayStore$A;)V bci=3",
            "klass: TestArrayStore$ChildA",
            "Different klass in opt profiled receiver",
        };
        Test(expect_outputs_one_trap_not_match,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestArrayStore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestArrayStore",
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
