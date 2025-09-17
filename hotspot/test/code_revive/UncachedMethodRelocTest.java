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
 * @summary Test whether un-cached method can be found
 * @requires (os.family == "linux") & (os.arch == "amd64")
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
