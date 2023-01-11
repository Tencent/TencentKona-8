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
 * @summary Test if dep check fail as expected
 * @library /testlibrary
 * @compile test-classes/TestLeafDependency.java
 * @run main/othervm TestDependencyFail
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.io.FileWriter;
import java.io.IOException;

public class TestDependencyFail {
    public static void main(String[] args) throws Exception {
        Test(null,
             "-XX:CodeReviveOptions=save,file=1.csa",
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,*.bar",
             "TestLeafDependency");

        String[] exp = {"validate_aot_method_dependencies fail"};
        Test(exp,
             "-XX:CodeReviveOptions=restore,file=1.csa,log=method=trace",
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,*.bar",
             "TestLeafDependency",
             "0");

        WriteFile("restore,file=1.csa,log=method=trace", "1.cf");
        Test(exp,
             "-XX:CodeReviveOptionsFile=./1.cf",
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,*.bar",
             "TestLeafDependency",
             "0");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
    }

    static void WriteFile(String content, String file_name) {
      try (FileWriter fw = new FileWriter(file_name)) {
          fw.write(content);
      } catch (IOException e) {
        System.out.println(e);
        e.printStackTrace();
      }
    }
}
