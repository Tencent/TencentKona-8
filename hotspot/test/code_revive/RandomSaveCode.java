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
 * @summary Test -XX:CodeReviveOptions:percent=xx
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @run main/othervm RandomSaveCode
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;
import java.util.List;

public class RandomSaveCode {
    public static void main(String[] args) throws Exception {
        String[] empty = {};
        // -XX:CodeReviveOptions:percent out of range, format wrong
        Test(empty,
             0,
             "-XX:CodeReviveOptions=save,file=1.csa,percent=234",
             "-version");

        Test(empty,
             0,
             "-XX:CodeReviveOptions=save,file=1.csa,percent=0",
             "-version");

        Test(empty,
             0,
             "-XX:CodeReviveOptions=save,file=1.csa,percent=ab",
             "-version");

        Test(empty,
             0,
             "-XX:CodeReviveOptions=save,file=1.csa,percent=03",
             "-version");

        Test(empty,
             0,
             "-XX:CodeReviveOptions=save,file=1.csa,percent=10",
             "-version");

        // -XX:CodeReviveOptions:percent=20 run 30 times, expect is 6, must be in range [1, 15] (100 runs test)
        int save_count = 0;
        for (int i = 0; i < 30; i++) {
             OutputAnalyzer output = TestAndGetOutput(
                 "-XX:CodeReviveOptions=save,file=1.csa,percent=20,log=archive=trace",
                 "-version");
             List<String> l = output.asLines();
             for (String s : l) {
                 if (s.contains("Fail to save CSA file")) {
                     save_count++;
                     break;
                 }
             }
        }
        System.out.println("save_count " + save_count);
        Asserts.assertGTE(save_count, 1);
        Asserts.assertLTE(save_count, 15);
    }

    static OutputAnalyzer TestAndGetOutput(String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        return output;
    }

    static void Test(String[] expect_outputs, int expect_value, String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(expect_value);
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
    }
}
