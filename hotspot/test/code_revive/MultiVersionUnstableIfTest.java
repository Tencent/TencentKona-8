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
 * @summary generate multi-version and select
 * @library /testlibrary
 * @compile test-classes/MultiVersionCase.java
 * @run main/othervm MultiVersionUnstableIfTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class MultiVersionUnstableIfTest {
    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 20; i++) {
            Test("-XX:CompileCommand=compileonly,MultiVersionCase.foo",
                 "-XX:-BackgroundCompilation",
                 "-XX:CodeReviveOptions=save,file=r_%p.csa,log=opt=trace",
                 "MultiVersionCase",
                 String.valueOf(i));
        }
        String result = Test("-XX:CodeReviveOptions=merge,log=merge=info,file=a.csa,input_files=./");
        String[] lines = result.split("\n");
        int v15_count = 0;
        int v16_count = 0;
        for (String line : lines) {
            if (line.indexOf("version 15:") != -1) {
                v15_count++;
            }
            if (line.indexOf("version 16:") != -1) {
                v16_count++;
            }
        }
        System.out.println("v15 count = " + v15_count + ", v16 count = " + v16_count);
        Asserts.assertEQ(v15_count, 2);
        Asserts.assertEQ(v16_count, 1);
    }

    static String Test(String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        return output.getOutput();
    }
}
