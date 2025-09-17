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
 * @summary test klass initialization
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/TestWrongKlassState.java
 * @run main/othervm TestKlassInit
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;
import java.io.FileWriter;

public class TestKlassInit {
    public static void main(String[] args) throws Exception {
        // Save
        String[] empty = {};
        Test(empty,
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,*.bar",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=save,file=1.csa",
             "TestWrongKlassState");

        Test(empty,
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,*.bar",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=save,file=2.csa",
             "TestWrongKlassState", "1");

        // Merge
        FileWriter fw = new FileWriter("input_file.list");
        fw.write("1.csa\n2.csa");
        fw.close();

        Test(empty,
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,*.bar",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=merge,file=merged.csa,input_list_file=input_file.list,log=merge=info",
             "-version");

        // Restore
        Test(empty,
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,*.bar",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=restore,file=merged.csa,log=restore=info",
             "TestWrongKlassState", "1", "-1");
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
