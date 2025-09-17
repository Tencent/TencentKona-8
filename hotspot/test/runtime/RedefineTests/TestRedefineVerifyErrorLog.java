/*
 * Copyright (C) 2023, 2024, Tencent. All rights reserved.
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
 * @summary Test print more detail error message
 * @library /testlibrary
 * @build RedefineClassHelper
 * @run main RedefineClassHelper
 * @run main/othervm TestRedefineVerifyErrorLog
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.InMemoryJavaCompiler;

class TestRedefineVerifyErrorLog_B {
    static boolean b;
    int faa() { System.out.println("orig"); return 1; }
}

public class TestRedefineVerifyErrorLog {
     public static void main(String[] args) throws Exception  {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-javaagent:redefineagent.jar",
                                                                  "-XX:TraceRedefineClasses=2",
                                                                  "RedefineVerifyErrorLog");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("expected VerifyError");
        output.shouldContain("VerifyError Bad type on operand stack");
     }
}
class RedefineVerifyErrorLog {

    static final String DEST = System.getProperty("test.classes");
    static String newB =
        "class TestRedefineVerifyErrorLog_B { " +
        " static boolean b; " +
        "  int faa() { System.out.println(\"redefine\"); b = true; return 2; } " +
        "}";

    public static void main(String[] args) throws Exception  {
        Class<?> B = Class.forName("TestRedefineVerifyErrorLog_B");

        try {
            byte[] classBytes = InMemoryJavaCompiler.compile("TestRedefineVerifyErrorLog_B", newB);
            // break classBytes
            // find pattern B6 00 04 04 B3
            // replace last 04 (iconst_1) with 0xe (dconst_0)
            for (int i = 0; i < classBytes.length; i++) {
                int cur = Byte.toUnsignedInt(classBytes[i]);
                if (cur == 0xb6 && (i + 4 < classBytes.length)) {
                    int cur_3 = Byte.toUnsignedInt(classBytes[i+3]);
                    int cur_4 = Byte.toUnsignedInt(classBytes[i+4]);
                    if (cur_3 == 0x4 && cur_4 == 0xb3) {
                        System.out.println("found");
                        classBytes[i+3] = 0xe;
                    }
                }
            }
            RedefineClassHelper.redefineClass(B, classBytes);
        } catch (VerifyError e) {
            System.out.println("expected VerifyError");
        }
    }
}
