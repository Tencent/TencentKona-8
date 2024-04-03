/*
 * Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
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
 * @summary SharedArchiveConsistency with NMT
 * @library /testlibrary
 * @compile test-classes/Hello.java
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions MismatchWithNMT
 */
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.Utils;
import java.io.IOException;
import java.io.File;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.TimeUnit;

public class MismatchWithNMT {
    // dump with hello.jsa, then
    // read the jsa file
    //   1) run normal
    //   2) change timestamp of jar file
    public static void main(String... args) throws Exception {
        // must call to get offset info first!!!
        Path currentRelativePath = Paths.get("");
        String currentDir = currentRelativePath.toAbsolutePath().toString();
        System.out.println("Current relative path is: " + currentDir);
        // get jar file
        String jarFile = JarBuilder.getOrCreateHelloJar();

        // dump (appcds.jsa created)
        TestCommon.testDump(jarFile, null);

        // test, should pass
        System.out.println("1. Normal, should pass \n");
        String[] execArgs = {"-XX:NativeMemoryTracking=summary", "-XX:+RelaxCheckForAppCDS", "-cp", jarFile, "Hello"};

        OutputAnalyzer output = TestCommon.execCommon(execArgs);

        TestCommon.checkExecReturn(output, 0, true, "Hello World");

        // Wait for a while before changing the timestamp of hello.jar,
        // then the timestamp difference can be more observiable.
        TimeUnit.SECONDS.sleep(1);

        // modify the timestamp of hello.jar
        try {
            Runtime run = Runtime.getRuntime();
            Process p = run.exec("touch " + jarFile);
            p.waitFor();
        } catch (IOException e) {
            e.printStackTrace();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        System.out.println("\n2. timestamp mismatch\n");
        output = TestCommon.execCommon(execArgs);
        output.shouldContain("UseSharedSpaces: A jar file is not the one used while building");
        output.shouldHaveExitValue(0);
    }
}
