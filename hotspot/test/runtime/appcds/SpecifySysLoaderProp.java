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
 * @summary If -Djava.system.class.loader=xxx is specified in command-line, disable archived non-system classes
 * @library /testlibrary
 * @compile test-classes/TestClassLoader.java
 * @compile test-classes/ReportMyLoader.java
 * @compile test-classes/TrySwitchMyLoader.java
 * @run main/othervm SpecifySysLoaderProp
 */

import java.io.*;
import com.oracle.java.testlibrary.OutputAnalyzer;

public class SpecifySysLoaderProp {

  public static void main(String[] args) throws Exception {
    JarBuilder.build("sysloader", "TestClassLoader", "ReportMyLoader", "TrySwitchMyLoader");

    String jarFileName = "sysloader.jar";
    String appJar = TestCommon.getTestJar(jarFileName);
    TestCommon.testDump(appJar, TestCommon.list("ReportMyLoader"));
    String warning = "VM warning: Archived non-system classes are disabled because the java.system.class.loader property is specified";


    // (0) Baseline. Do not specify -Djava.system.class.loader
    //     The test class should be loaded from archive
    TestCommon.run(
        "-verbose:class",
        "-cp", appJar,
        "ReportMyLoader")
      .assertNormalExit("Loaded ReportMyLoader from shared objects file",
                        "ReportMyLoader's loader = sun.misc.Launcher$AppClassLoader@");

    // (1) Try to execute the archive with -Djava.system.class.loader=no.such.Klass,
    //     it should fail
    TestCommon.run(
        "-cp", appJar,
        "-Djava.system.class.loader=no.such.Klass",
        "ReportMyLoader")
      .assertAbnormalExit(output -> {
          output.shouldContain(warning);
          output.shouldContain("ClassNotFoundException: no.such.Klass");
        });

    // (2) Try to execute the archive with -Djava.system.class.loader=TestClassLoader,
    //     it should run, but archived non-system classes should be disabled
    TestCommon.run(
        "-verbose:class",
        "-cp", appJar,
        "-Djava.system.class.loader=TestClassLoader",
        "ReportMyLoader")
      .assertNormalExit("ReportMyLoader's loader = sun.misc.Launcher$AppClassLoader@", //<-this is still printed because TestClassLoader simply delegates to Launcher$AppLoader, but ...
             "TestClassLoader.called = true", //<-but this proves that TestClassLoader was indeed called.
             "TestClassLoader: loadClass(\"ReportMyLoader\",") //<- this also proves that TestClassLoader was indeed called.
      .assertNormalExit(output -> {
        output.shouldMatch("Loaded TestClassLoader from file");
        output.shouldMatch("Loaded ReportMyLoader from file");
        });

    // (3) Try to change the java.system.class.loader programmatically after
    //     the app's main method is executed. This should have no effect in terms of
    //     changing or switching the actual system class loader that's already in use.
    TestCommon.run(
        "-verbose:class",
        "-cp", appJar,
        "TrySwitchMyLoader")
      .assertNormalExit("Loaded ReportMyLoader from shared objects file",
             "TrySwitchMyLoader's loader = sun.misc.Launcher$AppClassLoader@",
             "ReportMyLoader's loader = sun.misc.Launcher$AppClassLoader@",
             "TestClassLoader.called = false");
  }
}
