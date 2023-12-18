/*
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
 * @summary Print the context information of java.lang.IncompatibleClassChangeError: Implementing class
 * @library /testlibrary
 * @compile test_classes/Interface1.java test_classes/Interface2.java test_classes/DependentClass.java test_classes/LoadDependentClass.java
 * @run main ParseInterfaceTest
 */

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

public class ParseInterfaceTest {
    /*
    * test steps:
    *   1. DependentClass depends on Interface1 and Interface2
    *   2. load DependentClass
    *       Interface2 is "interface", success
    *   3. modify "interface Interface2" to "class Interface2" and recompile
    *   4. load DependentClass again
    *       Interface2 is "class", throw java.lang.IncompatibleClassChangeError
    */
    public static void main(String[] args) throws Exception{
        // Interface2 is "interface", should be successful
        String[] contains1 = {
            "Succeed to load DependentClass!"
        };
        Test(contains1, null);

        // modify "interface Interface2" to "class Interface2" and recompile
        ModifyInterface2(false);
        RecompileInterface2();

        // recover, make sure the initial state of Interface2 is "interface"
        ModifyInterface2(true);

        // Interface2 is "class", should throw java.lang.IncompatibleClassChangeError
        String[] contains2 = {
            "Exception in thread \"main\" java.lang.IncompatibleClassChangeError: class DependentClass can not implement Interface2, because it is not an interface"
        };
        String[] not_contains2 = {
            "Succeed to load DependentClass!"
        };
        Test(contains2, not_contains2);
    }

    private static void Test(String[] contains, String[] not_contains) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("LoadDependentClass");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        CheckOutput(output, contains, not_contains);
    }

    private static void ModifyInterface2(boolean is_interface) throws Exception{
        String modifiedCode = "\n" +
                              "public class Interface2 {\n" +
                              "    void method2() {\n" +
                              "    }\n" +
                              "}\n";
        if (is_interface) {
            modifiedCode = "\n" +
                           "public interface Interface2 {\n" +
                           "    void method2();\n" +
                           "}\n";;
        }
        String filePath = System.getProperty("test.src") + "/test_classes/Interface2.java";
        BufferedReader reader = new BufferedReader(new FileReader(filePath));
        StringBuilder license = new StringBuilder();
        StringBuilder code = new StringBuilder();
        String line;
        boolean isLicense = true;
        while ((line = reader.readLine()) != null) {
            if (isLicense) {
                license.append(line).append("\n");
                if (line.contains(" */")) {
                    isLicense = false;
                }
            } else {
                code.append(line).append("\n");
            }
        }
        reader.close();

        BufferedWriter writer = new BufferedWriter(new FileWriter(filePath));
        writer.write(license.toString());
        writer.write(modifiedCode);
        writer.close();

        System.out.println("Succeed to modify Interface2.java");
    }

    private static void RecompileInterface2() throws Exception{
        String filePath = System.getProperty("test.src") + "/test_classes/Interface2.java";
        String testPath = System.getProperty("test.classes");
        String command = System.getProperty("test.jdk") + "/bin/javac -d " + testPath + " " + filePath;
        System.out.println("command = " + command);

        Process process = Runtime.getRuntime().exec(command);
        int exitCode = process.waitFor();
        if (exitCode == 0) {
            System.out.println("Succeed to recompile Interface2.java");
        } else {
            System.out.println("Failed to recompile Interface2.java");
        }
    }

    public static void CheckOutput(OutputAnalyzer output, String[] contains, String[] not_contains) throws Exception {
        System.out.println(output.getOutput());
        if (contains != null) {
            for (String s : contains) {
                output.shouldContain(s);
            }
        }
        if (not_contains != null) {
            for (String s : not_contains) {
                output.shouldNotContain(s);
            }
        }
    }
}
