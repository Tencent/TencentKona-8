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
 * @summary Test whether un-cached method can be found
 * @library /testlibrary
 * @compile test-classes/TestHashTableGet.java
 * @run main/othervm UncachedMethodRelocTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class UncachedMethodRelocTest {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] {"TestHashTableGet"};

        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:-TieredCompilation",
            "-Xcomp",
            "-XX:CompileCommand=compileonly,java.util.Hashtable::get",
            "-XX:CodeReviveOptions=save,log=save=trace,file=TestHashTableGet.csa",
            "-XX:-BackgroundCompilation",
            appClass[0]);


        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("Emit method java/util/Hashtable.get(Ljava/lang/Object;)Ljava/lang/Object; success");
        output.shouldHaveExitValue(0);

        // restore aot
        pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:-TieredCompilation",
            "-Xcomp",
            "-XX:CompileCommand=compileonly,java.util.Hashtable::get",
            "-XX:CodeReviveOptions=restore,log=restore=trace,file=TestHashTableGet.csa",
            "-XX:-BackgroundCompilation",
            "-XX:+UnlockDiagnosticVMOptions",
            "-XX:+PrintNMethods",
            appClass[0]);

        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("Compiled method (c2 aot)");
        output.shouldContain("java.util.Hashtable::get (69 bytes)");
        output.shouldHaveExitValue(0);

    }
}
