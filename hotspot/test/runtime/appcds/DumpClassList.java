/*
 * Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

/*
 * @test
 * @summary DumpLoadedClassList should exclude generated classes, classes in bootclasspath/a
 * @library /testlibrary
 * @compile test-classes/ArrayListTest.java
 * @run main DumpClassList
 */

import com.oracle.java.testlibrary.InMemoryJavaCompiler;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

import java.util.ArrayList;
import java.util.List;
import java.io.*;

public class DumpClassList {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] {"ArrayListTest"};
        String classList = "app.list";

        String[] expectedClasses =  new String[] {"boot/append/Foo", "ArrayListTest"};

        JarBuilder.build("app", appClass[0]);
        String appJar = TestCommon.getTestJar("app.jar");

        // build bootclasspath/a
        String source2 = "package boot.append; "                 +
                        "public class Foo { "                    +
                        "    static { "                          +
                        "        System.out.println(\"Foo\"); "  +
                        "    } "                                 +
                        "}";

        ClassFileInstaller.writeClassToDisk("boot/append/Foo",
             InMemoryJavaCompiler.compile("boot.append.Foo", source2),
             System.getProperty("test.classes"));

        String appendJar = JarBuilder.build("bootappend", "boot/append/Foo");

        // dump class list
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
            true,
            "-XX:+UseAppCDS",
            "-Xshare:off",
            "-XX:DumpLoadedClassList=" + classList,
            "-Xbootclasspath/a:" + appendJar,
            "-cp",
            appJar,
            appClass[0]);
        OutputAnalyzer output = TestCommon.executeAndLog(pb, "dumpClassList");
        TestCommon.checkExecReturn(output, 0, true,
                                   "hello world");

        List<String> dumpedClasses = toClassNames(classList);

        for (String clazz : expectedClasses) {
            if (!dumpedClasses.contains(clazz)) {
                throw new RuntimeException(clazz + " missing in " +
                                           classList);
            }
        }
    }

    public static List<String> toClassNames(String filename) throws IOException {
        ArrayList<String> classes = new ArrayList<>();
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(filename)))) {
            for (; ; ) {
                String line = br.readLine();
                if (line == null) {
                    break;
                }
                classes.add(line);
            }
        }
        return classes;
    }
}
