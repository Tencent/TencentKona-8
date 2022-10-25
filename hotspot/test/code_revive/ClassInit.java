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
 * @summary Test class init state when revive
 * @library /testlibrary
 * @compile test-classes/TestClassOnlyLoaded.java
 * @run main/othervm ClassInit
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class ClassInit {
    public static void main(String[] args) throws Exception {
        // generate csa, class is initialized
        String[] save_expect_outputs1 = {
            "TestClassOnlyLoaded$A status 4",
        };
        Test(save_expect_outputs1,
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=save,file=1.csa,log=save=info",
             "TestClassOnlyLoaded",
             "1");

        // generate csa, class is only loaded not initialized
        String[] save_expect_outputs2 = {
            "TestClassOnlyLoaded$A status 1",
        };
        Test(save_expect_outputs2,
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=save,file=2.csa,log=save=info",
             "TestClassOnlyLoaded");

        // restore, class is loaded but require initialized
        String[] expect_outputs_fail = {
            "Unexpected state loaded(1) for klass TestClassOnlyLoaded$A, which should be at least fully_initialized(4)",
            "revive fail: No usable or valid aot code version, TestClassOnlyLoaded.foo(Ljava/lang/Object;)Z"
        };
        Test(expect_outputs_fail,
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info",
             "TestClassOnlyLoaded");

        // restore, class is loaded and require is loaded
        String[] expect_outputs_both_loaded = {
            "revive success: TestClassOnlyLoaded.foo(Ljava/lang/Object;)Z",
        };
        Test(expect_outputs_both_loaded,
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=restore,file=2.csa,log=restore=trace",
             "TestClassOnlyLoaded");

        // restore, class is initialized and require is loaded
        Test(expect_outputs_both_loaded,
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=restore,file=2.csa,log=restore=trace",
             "TestClassOnlyLoaded",
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
