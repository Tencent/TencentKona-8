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
 * @summary Test whether ArrayIndexOutOfBoundsException is initialized during restore
 * @library /testlibrary
 * @compile test-classes/TestArrayIndexOutOfBound.java
 * @run main/othervm TestAIOOB
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class TestAIOOB {
    public static void main(String[] args) throws Exception {
        String[] expect_outputs_direct_save = {
            "Emit method TestArrayIndexOutOfBound.foo(I)Ljava/lang/String; success",
        };
        Test(expect_outputs_direct_save,
             "-XX:CodeReviveOptions=save,file=1.csa,log=save=trace",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestArrayIndexOutOfBound.foo",
             "-XX:-TieredCompilation",
             "TestArrayIndexOutOfBound", "1");

        String[] expect_outputs_direct_restore = {
            "revive success: TestArrayIndexOutOfBound.foo(I)Ljava/lang/String",
        };
        Test(expect_outputs_direct_restore,
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=trace",
             "-XX:-BackgroundCompilation",
             "-XX:CompileOnly=TestArrayIndexOutOfBound.foo",
             "-XX:-TieredCompilation",
             "TestArrayIndexOutOfBound");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
    }
}
