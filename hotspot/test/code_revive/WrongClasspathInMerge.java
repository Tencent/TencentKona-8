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
 * @summary classpath mismatch between dump time and merge time
 * @library /testlibrary
 * @compile test-classes/C2.java
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @run main/othervm WrongClasspathInMerge
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import java.text.SimpleDateFormat;
import java.io.File;
import java.util.Date;

public class WrongClasspathInMerge {
    private static final SimpleDateFormat timeStampFormat =
        new SimpleDateFormat("HH'h'mm'm'ss's'SSS");
    public static void main(String[] args) throws Exception {
        String appJar = ClassFileInstaller.writeJar("jar.jar", "pkg/C2");
        String jar2 = ClassFileInstaller.writeJar("jar2.jar", "Dummy", "TestInlineDummy");

        // build The app
        String[] appClass = new String[] {"TestInlineDummy"};

        String csa_file = "wrongClasspathInMerge_" + timeStampFormat.format(new Date()) + ".csa";
        TestCommon.setCurrentArchiveName(csa_file);

        // dump aot
        // dump with none_canonical_jar2_path  //mnt/d/codes/jdks/kona8_dev/JTwork/scratch/0/jar2.jar
        // next test load with canonical_jar2_path should pass
        String separator = System.getProperty("file.separator");
        String none_canonical_jar2_path = jar2.replaceFirst(separator, separator + separator);
        OutputAnalyzer output = TestCommon.testDump(appJar + ":" + none_canonical_jar2_path, appClass, "log=archive=trace",
                                                    "-XX:-TieredCompilation",
                                                    "-Xcomp",
                                                    "-XX:-BackgroundCompilation",
                                                    "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // class path isn't changed. classpath mismatch shouldn't be reported.
        String merged_csa_file = "wrongClasspathInMerge_" + timeStampFormat.format(new Date()) + "_merged.csa";
        TestCommon.setCurrentArchiveName(merged_csa_file);
        output = TestCommon.testMergeCSA(appJar + ":" + jar2, "input_files=" + csa_file + ",log=merge=fail", "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldNotContain("APP classpath mismatch");
        output.shouldHaveExitValue(0);

        // class path isn't changed but not canonicalized. classpath mismatch shouldn't be reported.
        //String merged_csa_file = "wrongClasspathInMerge_" + timeStampFormat.format(new Date()) + "_merged.csa";
        TestCommon.setCurrentArchiveName(merged_csa_file);
        // //mnt/d/codes/jdks/kona8_dev/JTwork/scratch/0/jar2.jar
        output = TestCommon.testMergeCSA(appJar + ":" + none_canonical_jar2_path, "input_files=" + csa_file + ",log=merge=fail", "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldNotContain("APP classpath mismatch");
        output.shouldHaveExitValue(0);

        // the order of jar files is changed.
        output = TestCommon.testMergeCSA(jar2 + ":" + appJar, "input_files=" + csa_file + ",log=merge=fail", "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldContain("APP classpath mismatch");
        output.shouldContain("No compatible CSA file after fingerprint check");
        output.shouldHaveExitValue(1);

        // the class path is included in the class path of csa
        output = TestCommon.testMergeCSA(appJar, "input_files=" + csa_file + ",log=merge=fail", "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldNotContain("APP classpath mismatch");
        output.shouldHaveExitValue(0);

        // the class path isn't included in the class path of csa
        output = TestCommon.testMergeCSA(appJar + ":" + jar2 + ":" + jar2,
                                         "input_files=" + csa_file + ",log=merge=fail",
                                         "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldContain("Merge time APP classpath is longer than the path in csa file");
        output.shouldContain("Actual = ");
        output.shouldContain("Expected = ");
        output.shouldContain("No compatible CSA file after fingerprint check");
        output.shouldHaveExitValue(1);

        // class path isn't changed, but the jar file is changed.
        String jar2_2 = ClassFileInstaller.writeJar("jar2.jar", "Dummy", "TestInlineDummy", "pkg/C2");
        output = TestCommon.testMergeCSA(appJar + ":" + jar2_2, "input_files=" + csa_file + ",log=merge=fail", "-XX:-TieredCompilation");
        System.out.println(output.getOutput());
        output.shouldContain("Fail to validate file");
        output.shouldContain("APP classpath mismatch");
        output.shouldContain("Actual = ");
        output.shouldContain("Expected = ");
        output.shouldContain("No compatible CSA file after fingerprint check");
        output.shouldHaveExitValue(1);
  }
}
