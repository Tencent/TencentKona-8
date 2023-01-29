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
 * @summary check whether there is some class reading from directory
 * @library /testlibrary
 * @compile test-classes/Parent.java
 * @compile test-classes/Child1.java
 * @compile test-classes/Child2.java
 * @compile test-classes/GrandChild1.java
 * @compile test-classes/ProfiledConfig.java
 * @compile test-classes/TestProfiledReceiver.java
 * @run main/othervm ClassInDirCheck
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.text.SimpleDateFormat;
import java.io.File;
import java.util.Date;

public class ClassInDirCheck {
    private static final SimpleDateFormat timeStampFormat =
        new SimpleDateFormat("HH'h'mm'm'ss's'SSS");
    public static void main(String[] args) throws Exception {
        String appJar = ClassFileInstaller.writeJar("jar.jar", "Parent", "Child1", "Child2");
        String filePath = ClassInDirCheck.class.getResource("/").getPath();
        // build The app
        String[] appClass = new String[] {"TestProfiledReceiver"};
        String[] expect_outputs = {
            "OptRecord insert: ProfiledReceiver at method=TestInstanceOf.foo(Ljava/lang/Object;)Z bci=1",
        };


        // dump aot
        OutputAnalyzer output = Test("-cp",
                                     appJar + ":" + filePath,
                                     "-XX:CodeReviveOptions=save,file=classInDirCheck.csa,log=save=trace",
                                     "-XX:-TieredCompilation",
                                     "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                                     "-XX:CompileCommand=inline,*.hi",
                                     "-XX:CompileCommand=compileonly,*.hi",
                                     "-XX:CompileCommand=inline,java.lang.Object::getClass",
                                     "TestProfiledReceiver");

        System.out.println(output.getOutput());
        output.shouldContain("Load class from directory");
        output.shouldHaveExitValue(0);

        // merge with class path
        output = Test("-cp", appJar + ":" + filePath,
                      "-XX:CodeReviveOptions=merge,input_files=classInDirCheck.csa,file=classInDirCheck_merge.csa,log=merge=trace",
                      "-version");

        System.out.println(output.getOutput());
        output.shouldContain("The class path can't have directory at merge time");
        output.shouldNotContain("APP classpath mismatch");
        output.shouldHaveExitValue(1);

        // merge with class path
        output = Test("-cp", appJar,
                      "-XX:CodeReviveOptions=merge,input_files=classInDirCheck.csa,file=classInDirCheck_merge.csa,log=merge=trace",
                      "-version");

        System.out.println(output.getOutput());
        output.shouldNotContain("The class path can't have directory at merge time");
        output.shouldNotContain("APP classpath mismatch");
        output.shouldHaveExitValue(0);

        // load with aot
        output = Test("-cp", appJar + ":" + filePath,
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck_merge.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        System.out.println(output.getOutput());
        output.shouldContain("revive success: Child1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);

        // load with aot
        output = Test("-cp", filePath + ":" + appJar,
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck_merge.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        System.out.println(output.getOutput());
        output.shouldContain("Load class from directory:");
        output.shouldNotContain("revive success: Child1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);
    }

    static OutputAnalyzer Test(String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        return output;
    }
}
