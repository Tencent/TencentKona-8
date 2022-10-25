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
 * @summary Test if global symbol relocation info is found as expect
 * @library /testlibrary
 * @compile test-classes/Parent.java
 * @compile test-classes/Child1.java
 * @compile test-classes/Child2.java
 * @compile test-classes/GrandChild1.java
 * @compile test-classes/ProfiledConfig.java
 * @compile test-classes/TestProfiledReceiver.java
 * @run main/othervm ProfileMetaOopTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class ProfileMetaOopTest {
    public static void main(String[] args) throws Exception {
        Test(null,
             "-XX:CodeReviveOptions=save,log=archive=trace,file=1.csa",
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
             "-XX:CompileCommand=inline,*.hi",
             "-XX:CompileCommand=compileonly,*.hi",
             "-XX:CompileCommand=inline,java.lang.Object::getClass",
             "TestProfiledReceiver");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("klass by name and classloader");
        output.shouldContain("mirror by name and classloader");
        output.shouldContain("method by name and classloader");
        output.shouldContain("kls = GrandChild1");
        output.shouldHaveExitValue(0);
    }
}
