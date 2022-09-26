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
 * @summary check whether there is some class reading from directory
 * @library /testlibrary
 * @compile test-classes/Parent.java
 * @compile test-classes/Child1.java
 * @compile test-classes/Child2.java
 * @compile test-classes/GrandChild1.java
 * @compile test-classes/ProfiledConfig.java
 * @compile test-classes/TestProfiledReceiver.java
 * @run main/othervm WildCardDirSupport
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.text.SimpleDateFormat;
import java.io.File;
import java.util.Date;

public class WildCardDirSupport {
    private static final SimpleDateFormat timeStampFormat =
        new SimpleDateFormat("HH'h'mm'm'ss's'SSS");
    public static void main(String[] args) throws Exception {
        String wildcard = System.getProperty("user.dir") + "/*";
        String appJar1 = ClassFileInstaller.writeJar("parent.jar", "Parent");
        String appJar2 = ClassFileInstaller.writeJar("child1.jar", "Child1");
        String appJar3 = ClassFileInstaller.writeJar("child2.jar", "Child2");
        String filePath = ClassInDirCheck.class.getResource("/").getPath();
        // build The app
        String[] appClass = new String[] {"TestProfiledReceiver"};


        // dump aot with wildcard in classpath
        OutputAnalyzer output = Test("-cp",
                                     wildcard + ":" + filePath, 
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

        // merge with wildcard in classpath, but not specified in CodeReviveOptions
        output = Test("-cp", wildcard,
                      "-XX:CodeReviveOptions=merge,input_files=classInDirCheck.csa,file=classInDirCheck_merge.csa,log=merge=trace",
                      "-version");

        // classpath check pass
        System.out.println(output.getOutput());
        output.shouldNotContain("child1.jar is changed");
        output.shouldHaveExitValue(0);


        try {
            Thread.sleep(1000);
        } catch(Exception e) {
        }

        // change timestamp of the jar file child1.jar
        File fileToChange = new File(appJar2);
        Date filetime = new Date(fileToChange.lastModified());
        System.out.println(filetime.toString());     
        fileToChange.setLastModified(System.currentTimeMillis());
        filetime = new Date(fileToChange.lastModified());
        System.out.println(filetime.toString());

        // load with aot after the jar file child1.jar is changed.
        output = Test("-cp", wildcard + ":" + filePath,
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck_merge.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // Expect: the change of child1.jar is checked.
        System.out.println(output.getOutput());
        output.shouldContain("child1.jar is changed");
        output.shouldContain("Fail to load CSA file classInDirCheck_merge.csa");
        output.shouldHaveExitValue(0);


        // merge with class path after the timestamp of jar file is changed.
        output = Test("-cp", wildcard,
                      "-XX:CodeReviveOptions=merge,input_files=classInDirCheck.csa,file=classInDirCheck_merge.csa,log=merge=trace",
                      "-version");

        // Expect: the change of child1.jar is checked.
        System.out.println(output.getOutput());
        output.shouldContain("child1.jar is changed");
        output.shouldHaveExitValue(1);

        // merge with wildcard in class path and specified the classpath in CodeRevive options
        output = Test("-cp", wildcard,
                      "-XX:CodeReviveOptions=merge,input_files=classInDirCheck.csa,file=classInDirCheck_merge.csa,log=merge=trace,wildcard_classpath=" + wildcard,
                      "-version");

        // Expect: the timestamp check is ignored, then pass the classpath check 
        System.out.println(output.getOutput());
        output.shouldNotContain("APP classpath mismatch");
        output.shouldHaveExitValue(0);

        // load with aot with the above merged csa, the order and timestamp of some jar files will be ignored.
        output = Test("-cp", wildcard + ":" + filePath,
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck_merge.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // Expect: load aot successfully, and revive successfully.
        System.out.println(output.getOutput());
        output.shouldContain("revive success: Child1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);

        // dump aot: load class from directory
        output = Test("-cp",
                      wildcard + ":" + filePath, 
                      "-XX:CodeReviveOptions=save,file=classInDirCheck.csa,log=save=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // Expect: report the message: load class from directory
        System.out.println(output.getOutput());
        output.shouldContain("Load class from directory");
        output.shouldHaveExitValue(0);


        // merge 1: without CodeReviveOption wildcard_classpath
        output = Test("-cp", wildcard,
                      "-XX:CodeReviveOptions=merge,input_files=classInDirCheck.csa,file=classInDirCheck_merge1.csa,log=merge=trace",
                      "-version");

        // merge 2: with CodeReviveOption wildcard_classpath
        output = Test("-cp", wildcard,
                      "-XX:CodeReviveOptions=merge,input_files=classInDirCheck.csa,file=classInDirCheck_merge2.csa,log=merge=trace,wildcard_classpath=" + wildcard,
                      "-version");

        // try to change the order in directory
        ClassFileInstaller.writeJar("child1.jar", "Child1");
        String child3 = ClassFileInstaller.writeJar("child3.jar", "Child2", "Child1");
        ClassFileInstaller.writeJar("parent.jar", "Parent");
        ClassFileInstaller.writeJar("child2.jar", "Child2");
 
        File file3 = new File(child3);
        File file4 = new File("child3.war");
        file3.renameTo(file4);

        // load with aot from merge 2 
        output = Test("-cp", wildcard + ":" + filePath,
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck_merge2.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // Expect: load aot successfully
        System.out.println(output.getOutput());
        output.shouldContain("revive success: Child1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);


        //1. load with aot from merge2, and change the oder in the directory
        output = Test("-cp", "child1.jar:parent.jar:child2.jar" + ":" + filePath,
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck_merge2.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // Expect: load aot successfully
        System.out.println(output.getOutput());
        output.shouldContain("revive success: Child1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);

        //2. load with aot from merge2, and use the different oder in the directory
        output = Test("-cp", "parent.jar:child2.jar:child1.jar" + ":" + filePath,
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck_merge2.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // Expect: load aot successfully
        System.out.println(output.getOutput());
        output.shouldContain("revive success: Child1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);

        //3. load with aot from merge2, and use the different oder in the directory
        output = Test("-cp", "child2.jar:child1.jar:parent.jar" + ":" + filePath,
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck_merge2.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // Expect: load aot successfully
        System.out.println(output.getOutput());
        output.shouldContain("revive success: Child1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);


    }

    static OutputAnalyzer Test(String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        return output;
    }
}
