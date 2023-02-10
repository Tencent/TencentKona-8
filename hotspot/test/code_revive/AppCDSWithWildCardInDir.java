/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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
 * @summary AppCDS running with WilCard classpath
 * @library /testlibrary
 * @compile test-classes/Parent.java
 * @compile test-classes/Child1.java
 * @compile test-classes/Child2.java
 * @compile test-classes/GrandChild1.java
 * @compile test-classes/ProfiledConfig.java
 * @compile test-classes/TestProfiledReceiver.java
 * @run main/othervm AppCDSWithWildCardInDir
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.text.SimpleDateFormat;
import java.io.File;
import java.util.Date;

public class AppCDSWithWildCardInDir {
    private static final SimpleDateFormat timeStampFormat =
        new SimpleDateFormat("HH'h'mm'm'ss's'SSS");
    public static void main(String[] args) throws Exception {
        String wildcard = System.getProperty("user.dir") + "/*";
        String workdir = System.getProperty("user.dir") + "/";
        String appJar1 = ClassFileInstaller.writeJar("parent.jar", "Parent", "GrandChild1");
        String appJar2 = ClassFileInstaller.writeJar("child1.jar", "Child1", "ProfiledConfig");
        String appJar3 = ClassFileInstaller.writeJar("child2.jar", "Child2", "TestProfiledReceiver");
        // build The app
        String[] appClass = new String[] {"TestProfiledReceiver"};


        // dump class list
        OutputAnalyzer output = Test("-cp",
                                     wildcard + ":.",
                                     "-Xshare:off",
                                     "-XX:+UseAppCDS",
                                     "-XX:+RelaxCheckForAppCDS",
                                     "-XX:DumpLoadedClassList=appCDSWithWildCardDir.classlist",
                                     "TestProfiledReceiver");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // generate jsa with wildcard in classpath, but not specified in -XX:RecordClasspathForCDSDump
        output = Test("-cp", wildcard + ":.",
                      "-Xshare:dump",
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-XX:SharedClassListFile=appCDSWithWildCardDir.classlist",
                      "-XX:SharedArchiveFile=appCDSWithWildCardDir.jsa",
                      "-version");

        // classpath check pass
        System.out.println(output.getOutput());
        //output.shouldNotContain("child1.jar is changed");
        output.shouldHaveExitValue(0);


        // generate jsa with wildcard in classpath, and specified in -XX:RecordClasspathForCDSDump
        output = Test("-cp", wildcard + ":.",
                      "-Xshare:dump","-XX:+UseAppCDS","-XX:+RelaxCheckForAppCDS",
                      "-XX:SharedClassListFile=appCDSWithWildCardDir.classlist",
                      "-XX:SharedArchiveFile=appCDSWithWildCardDir2.jsa",
                      "-XX:RecordClasspathForCDSDump=" + wildcard,
                      "-version");

        // classpath check pass
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);



        try {
            Thread.sleep(2000);
        } catch(Exception e) {

        }

        // change timestamp of the jar file child1.jar
        File fileToChange = new File(appJar2);
        Date filetime = new Date(fileToChange.lastModified());
        System.out.println(filetime.toString());
        fileToChange.setLastModified(System.currentTimeMillis());
        filetime = new Date(fileToChange.lastModified());
        System.out.println(filetime.toString());

        // load with jsa after the jar file child1.jar is changed.
        output = Test("-cp", wildcard + ":.",
                      "-Xshare:on","-XX:+UseAppCDS","-XX:+RelaxCheckForAppCDS",
                      "-XX:SharedArchiveFile=appCDSWithWildCardDir.jsa",
                      "TestProfiledReceiver");

        // Expect: the change of child1.jar is checked.
        System.out.println(output.getOutput());
        output.shouldContain("UseSharedSpaces: A jar file is not the one used while building the shared archive file");
        output.shouldHaveExitValue(0);


        // load with jsa with wildcard after the jar file child1.jar is changed.
        output = Test("-cp", wildcard + ":.",
                      "-Xshare:on","-XX:+UseAppCDS","-XX:+RelaxCheckForAppCDS",
                      "-XX:SharedArchiveFile=appCDSWithWildCardDir2.jsa",
                      "-XX:+TraceClassLoading",
                      "TestProfiledReceiver");

        // Expect: the change of child1.jar is checked.
        System.out.println(output.getOutput());
        output.shouldNotContain("UseSharedSpaces: A jar file is not the one used while building the shared archive file");
        output.shouldContain("Loaded ProfiledConfig from shared objects file by");
        output.shouldHaveExitValue(0);

        //1. load with jsa with wildcard, and change the oder in the directory
        output = Test("-cp", workdir + "child1.jar:" + workdir + "parent.jar:" + workdir + "child2.jar:.",
                      "-Xshare:on","-XX:+UseAppCDS","-XX:+RelaxCheckForAppCDS",
                      "-XX:SharedArchiveFile=appCDSWithWildCardDir2.jsa",
                      "-XX:+TraceClassLoading",
                      "TestProfiledReceiver");

        // Expect: load jsa successfully
        System.out.println(output.getOutput());
        output.shouldNotContain("UseSharedSpaces: shared class paths mismatch");
        output.shouldContain("Loaded ProfiledConfig from shared objects file by");
        output.shouldHaveExitValue(0);

        //2. load with aot from merge2, and use the different oder in the directory
        output = Test("-cp", workdir + "parent.jar:" + workdir + "child2.jar:" + workdir + "child1.jar:.",
                      "-Xshare:on","-XX:+UseAppCDS","-XX:+RelaxCheckForAppCDS",
                      "-XX:SharedArchiveFile=appCDSWithWildCardDir2.jsa",
                      "-XX:+TraceClassLoading",
                      "TestProfiledReceiver");

        // Expect: load jsa successfully
        System.out.println(output.getOutput());
        output.shouldNotContain("UseSharedSpaces: shared class paths mismatch");
        output.shouldContain("Loaded ProfiledConfig from shared objects file by");
        output.shouldHaveExitValue(0);

    }

    static OutputAnalyzer Test(String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        return output;
    }
}
