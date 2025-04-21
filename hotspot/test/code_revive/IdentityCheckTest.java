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
 * @summary Test the check for the change of klass identity
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/Parent2.java
 * @compile test-classes/Child3.java
 * @compile test-classes/GrandChild3.java
 * @compile test-classes/ProfiledConfig2.java
 * @compile test-classes/TestProfiledReceiver2.java
 * @run main/othervm IdentityCheckTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.io.File;
import java.io.FileWriter;

public class IdentityCheckTest {
    public static void main(String[] args) throws Exception {
        String[] save_outputs = {"unresolved GrandChild3 class"};
        Test(save_outputs,
             "-XX:CodeReviveOptions=save,log=archive=trace,file=identitycheck.csa",
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,TestProfiledReceiver2::foo",
             "-XX:CompileCommand=inline,*.hi",
             "-XX:CompileCommand=compileonly,*.hi",
             "-XX:CompileCommand=inline,java.lang.Object::getClass",
             "TestProfiledReceiver2");

        String[] success_outputs = {"revive success: TestProfiledReceiver2.foo",
                                    "revive success: Child3.hi()Ljava/lang/String"}; 
        Test(success_outputs,
             "-XX:CodeReviveOptions=restore,log=restore=trace,file=identitycheck.csa",
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,TestProfiledReceiver2::foo",
             "-XX:CompileCommand=inline,*.hi",
             "-XX:CompileCommand=compileonly,*.hi",
             "-XX:CompileCommand=inline,java.lang.Object::getClass",
             "TestProfiledReceiver2");

        // modify class GrandChild3
        compileClass();
        String[] failure_outputs = {"Identity for klass GrandChild3 is different", 
                                    "revive fail: No usable or valid aot code version, TestProfiledReceiver2.foo"};
        Test(failure_outputs,
             "-XX:CodeReviveOptions=restore,log=restore=trace,file=identitycheck.csa",
             "-XX:-TieredCompilation",
             "-XX:CompileCommand=compileonly,TestProfiledReceiver2::foo",
             "-XX:CompileCommand=inline,*.hi",
             "-XX:CompileCommand=compileonly,*.hi",
             "-XX:CompileCommand=inline,java.lang.Object::getClass",
             "TestProfiledReceiver2");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
    }

    private static String generateClass() {
        StringBuilder sb = new StringBuilder();
        sb.append("public class GrandChild3 extends Child3 {");
        sb.append("    static String g;");
        sb.append("}");
        return sb.toString();
    }

    private static void compileClass() throws Exception {
        String code;
        code = generateClass();

        File file = new File("GrandChild3.java");
        FileWriter fw = new FileWriter(file);
        fw.write(code);
        fw.close();

        String[] cmd = { System.getProperty("test.jdk") + "/bin/javac", "-d", System.getProperty("test.classes"), 
                         "-cp", System.getProperty("test.classes") + ":.", "GrandChild3.java" };
        Process process = Runtime.getRuntime().exec(cmd);
        if (process.waitFor() != 0) {
             throw new RuntimeException("Error in compile");
        }
    }
}
