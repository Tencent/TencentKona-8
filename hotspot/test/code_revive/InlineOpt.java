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
 * @summary Test inline opt
 * @library /testlibrary
 * @compile test-classes/TestInlineDirect.java
 * @compile test-classes/TestInlineVirtual.java
 * @compile test-classes/TestInlineOnlyChild.java
 * @run main/othervm InlineOpt
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class InlineOpt {
    public static void main(String[] args) throws Exception {
        String[] expect_outputs_direct_save = {
            "OptRecord insert: Inline at method=TestInlineDirect.test(I)I bci=1 callee: FinalClass.foo(I)I",
        };
        String[] empty = {};
        Test(expect_outputs_direct_save, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "TestInlineDirect");

        String[] expect_outputs_direct_restore = {
            "OptRecord check: Inline at method=TestInlineDirect.test(I)I bci=1 callee: FinalClass.foo(I)I",
            "Check opt records negative",
        };
        Test(expect_outputs_direct_restore, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestInlineDirect");

        // test only child, profile is still virtual call profile
        String[] expect_outputs_onlychild_save = {
            "OptRecord insert: Inline at method=TestInlineOnlyChild.test([LBase;I)I bci=28 callee: Child1.foo(I)I",
        };
        Test(expect_outputs_onlychild_save, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "TestInlineOnlyChild", "1", "1", "1");

        String[] expect_outputs_onlychild_restore = {
            "OptRecord check: Inline at method=TestInlineOnlyChild.test([LBase;I)I bci=28 callee: Child1.foo(I)I",
            "Check opt records negative",
        };

        Test(expect_outputs_onlychild_restore, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestInlineOnlyChild", "1");

        // test virtual calll inline benifit could be diff
        // inline one hot virtual method
        String[] expect_outputs_one_virtual = {
            "OptRecord insert: DeVirtual at method=TestInlineVirtual.test([LBase;I)I bci=36 morphism=1  klass1: Child1  default: trap",
            "OptRecord insert: Inline at method=TestInlineVirtual.test([LBase;I)I bci=36 callee: Child1.foo(I)I",
        };
        Test(expect_outputs_one_virtual, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "TestInlineVirtual", "1");


        String[] expect_outputs_one_virtual_match = {
            "OptRecord check: DeVirtual at method=TestInlineVirtual.test([LBase;I)I bci=36 morphism=1  klass1: Child1  default: trap",
            "OptRecord check: Inline at method=TestInlineVirtual.test([LBase;I)I bci=36 callee: Child1.foo(I)I",
            "Check opt records negative"
        };
        Test(expect_outputs_one_virtual_match, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info,opt=trace",
             "TestInlineVirtual", "1");

         // inline major
         String[] expect_outputs_one_major_virtual = {
             "OptRecord insert: DeVirtual at method=TestInlineVirtual.test([LBase;I)I bci=36 morphism=0  klass1: Child1  default: virtual call",
             "OptRecord insert: Inline at method=TestInlineVirtual.test([LBase;I)I bci=36 callee: Child1.foo(I)I",
         };

         Test(expect_outputs_one_major_virtual, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=opt=trace",
             "-XX:TypeProfileMajorReceiverPercent=60",
             "TestInlineVirtual",
             "1", "1", "1", "1", "1", "1", "1", "1", "1", "2", "3");

         Test(empty, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=opt=trace",
             "-XX:TypeProfileMajorReceiverPercent=60",
             "TestInlineVirtual",
             "1");

         Test(empty, empty,
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=opt=trace",
             "-XX:TypeProfileMajorReceiverPercent=60",
             "TestInlineVirtual",
             "2");
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
