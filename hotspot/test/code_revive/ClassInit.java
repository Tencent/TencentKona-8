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
 * @summary Test class init state when revive
 * @requires (os.family == "linux") & (os.arch == "amd64")
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
