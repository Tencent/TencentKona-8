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
 * @summary Test multiversion
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/MultiVersion.java
 * @run main/othervm TestMultiVersion
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;
import java.io.FileWriter;

public class TestMultiVersion {
    public static void main(String[] args) throws Exception {
        // Save
        String[] expect_outputs_save1 = {
            "300000",
        };
        String[] empty = {};
        Test(expect_outputs_save1,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=MultiVersion.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa",
             "MultiVersion");

        String[] expect_outputs_save2 = {
            "600000",
        };
        Test(expect_outputs_save2,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=MultiVersion.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=2.csa",
             "MultiVersion",
             "1", "1", "1");

        String[] expect_outputs_save3 = {
            "900000",
        };
        Test(expect_outputs_save3,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=MultiVersion.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=3.csa",
             "MultiVersion",
             "-1", "-1", "-1");

        // Merge
        String[] expect_outputs_merge = {
            "method MultiVersion.foo(I)I, count 3",
        };

        Test(expect_outputs_merge,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=MultiVersion.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=merge,file=merged.csa,input_files=.,log=merge=info",
             "-version");

        String[] expect_outputs_save4 = {
            "600000",
        };
        Test(expect_outputs_save4,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=MultiVersion.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=4.csa",
             "MultiVersion",
             "0", "1", "-1");

        String[] expect_outputs_merge2 = {
            "method MultiVersion.foo(I)I, count 4",
        };

        FileWriter fw = new FileWriter("input_file.list");
        fw.write("1.csa\n2.csa\n3.csa\n4.csa");
        fw.close();

        Test(expect_outputs_merge2,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=MultiVersion.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=merge,file=merged2.csa,input_list_file=input_file.list,log=merge=info",
             "-version");

        String[] expect_outputs_restore = {
             "Unusable version 0 of method MultiVersion.foo(I)I",
             "of method MultiVersion.foo(I)I is set unusable for failed on opt check.",
             "All versions are unusable, AOT for method MultiVersion.foo(I)I is disabled.",
        };
        Test(expect_outputs_restore,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=MultiVersion.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=merged.csa,log=restore=info",
             "MultiVersion",
             "1", "-1");

        // First Policy
        String output1 = TestAndGetOutput("-XX:-TieredCompilation",
             "-XX:CompileOnly=MultiVersion.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=merged2.csa,log=restore=info,revive_policy=first",
             "MultiVersion",
             "1", "1", "1");
        int count1 = CountString(output1, 4, "Candidate Version ");
        if (count1 != 1) {
            throw new RuntimeException("FirstPolicy Test failed.");
        }

        // Random Policy
        String output2;
        int versionMask = 0;
        boolean right = false;
        for (int i = 0; i < 16; i++) {
             output2 = TestAndGetOutput("-XX:-TieredCompilation",
                 "-XX:CompileOnly=MultiVersion.foo",
                 "-XX:-BackgroundCompilation",
                 "-XX:CodeReviveOptions=restore,file=merged2.csa,log=restore=info,revive_policy=random",
                 "MultiVersion",
                 "1", "1", "1");
             // expect two candidate versions and both versions should be covered in 16 runs
             // following test is checking more than 1 bit in versionMask is set (1 bit means 1 version selected).
             // The fail rate is 1/2^16 with real random selection.
             versionMask |= 1 << GetSelectedVersion(output2, 4);
             if ((versionMask & (versionMask - 1)) != 0) {
                 right = true;
                 break;
             }
        }
        if (!right) {
            throw new RuntimeException("RandomPolicy Test failed");
        }

        // Appoint Policy
        String output3;
        for (int i = 0; i < 4; i++) {
             output3 = TestAndGetOutput("-XX:-TieredCompilation",
                 "-XX:CompileOnly=MultiVersion.foo",
                 "-XX:-BackgroundCompilation",
                 "-XX:CodeReviveOptions=restore,file=merged2.csa,log=restore=info,revive_policy=appoint=" + i,
                 "MultiVersion",
                 "1", "1", "1");
             int ver = GetSelectedVersion(output3, 4);
             if (ver == i) {
                 continue;
             }
             if (output3.contains("Candidate Version: " + i + " of method MultiVersion.foo(I)I.")) {
                 throw new RuntimeException("AppointPolicy Test failed.");
             }
        }
    }

    static int GetSelectedVersion(String output, int num) {
        for (int i = 0; i < num; i++) {
            if (output.contains("Select version " + i + " for method MultiVersion.foo(I)I.")) {
                return i;
            }
        }
        return -1;
    }

    static int CountString(String output, int num, String str) {
        int count = 0;
        for (int i = 0; i < num; i++) {
            if (output.contains(str + i)) {
                count++;
            }
        }
        return count;
    }

    static String TestAndGetOutput(String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        return output.getStdout();
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
