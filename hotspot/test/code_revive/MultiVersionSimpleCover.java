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
 * @summary generate multi-version and select
 * @library /testlibrary
 * @compile test-classes/TestMultiVersionSelection.java
 * @run main/othervm MultiVersionSimpleCover
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class MultiVersionSimpleCover {
    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 20; i++) {
            Test("-XX:CompileCommand=compileonly,TestMultiVersionSelection.foo",
                 "-XX:CompileCommand=compileonly,TestMultiVersionSelection.bar",
                 "-XX:-BackgroundCompilation",
                 "-XX:CodeReviveOptions=save,disable_check_dir,file=r_%p.csa,log=opt=trace",
                 "TestMultiVersionSelection",
                 String.valueOf(i));
        }
        Test("-XX:CompileCommand=compileonly,TestMultiVersionSelection.bar",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=r_%p.csa,log=opt=trace",
             "TestMultiVersionSelection");
        String result = Test("-XX:CodeReviveOptions=merge,disable_check_dir,log=merge=info,file=a.csa,input_files=./");
        String[] lines = result.split("\n");
        int v15_count = 0;
        int v16_count = 0;
        int merged_version = 0;
        for (String line : lines) {
            if (line.indexOf("version 15:") != -1) {
                v15_count++;
            }
            if (line.indexOf("version 16:") != -1) {
                v16_count++;
            }
            if (line.indexOf("cover and merge") != -1) {
                merged_version++;
            }
        }
        System.out.println("v15 count = " + v15_count + ", v16 count = " + v16_count + ", merged = " + merged_version);
        Asserts.assertEQ(v15_count, 4);
        Asserts.assertEQ(v16_count, 2);
        Asserts.assertEQ(merged_version, 4);
    }

    static String Test(String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        return output.getOutput();
    }
}
