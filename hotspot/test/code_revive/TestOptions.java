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
 * @summary Test CodeReviveOptions and CodeReviveOptionsFile behavior
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @run main/othervm TestOptions
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.io.FileWriter;
import java.io.IOException;

public class TestOptions {
    public static void main(String[] args) throws Exception {
        String[] exp = {"CodeReviveOptions and CodeReviveOptionsFile can not set at same time, ignore both"};
        WriteFile("save,file=1.csa", "1.cf");
        Test(exp, 0,
             "-XX:CodeReviveOptions=save,file=1.csa,log=method=trace",
             "-XX:CodeReviveOptionsFile=./1.cf",
             "-version");

        String[] exp1 = {"2.cf can not open"};
        Test(exp1, 0,
             "-XX:CodeReviveOptionsFile=./2.cf",
             "-version");

        WriteFile("save,file=1.csa,toooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooolong", "2.cf");
        String[] exp2 = {"CodeReviveOptionsFile content exceeds limit 1024"};
        Test(exp2, 0,
             "-XX:CodeReviveOptionsFile=./2.cf",
             "-version");

        // invalid option should disble CodeRevive
        // wrong log option will disable save and restore
        String[] exp3 = {"Incorrect CodeReviveOptions: starting from \"log\""};
        Test(exp3, 0,
             "-XX:-TieredCompilation",
             "-Xcomp",
             "-XX:CodeReviveOptions=save,file=a.csa,log",
             "-version");
    }

    static void Test(String[] expect_outputs, int expect_exit_code, String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(expect_exit_code);
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
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
