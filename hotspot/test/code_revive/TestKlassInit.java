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
 * @summary test klass initialization
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
             "-XX:CodeReviveOptions=merge,disable_check_dir,file=merged.csa,input_list_file=input_file.list,log=merge=info",
             "-version");

        // Restore
        Test(empty,
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,*.bar",
             "-XX:CompileCommand=compileonly,*.foo",
             "-XX:CodeReviveOptions=restore,disable_check_dir,file=merged.csa,log=restore=info",
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
