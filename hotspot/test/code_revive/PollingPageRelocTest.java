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
 * @summary Test whether useless polling relocation is generated
 * @library /testlibrary
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @run main/othervm PollingPageRelocTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class PollingPageRelocTest {
    public static boolean isDebugVM() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        if (output.firstMatch("debug") != null) {
            return true;
        } else {
            return false;
        }
    }

    public static void main(String[] args) throws Exception {
        if (!isDebugVM()) {
            return;
        }

        // build The app
        String[] appClass = new String[] {"TestInlineDummy"};

        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:CodeReviveOptions=save,file=TestInlineDummy.csa",
            "-XX:-TieredCompilation",
            "-Xcomp",
            "-XX:+ForceUnreachable",
            appClass[0]);


        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
    }
}
