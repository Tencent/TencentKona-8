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
 * @summary check whether the options of CDS affect the fingerprint check of AOT
 * @library /testlibrary
 * @compile test-classes/Parent.java
 * @compile test-classes/Child1.java
 * @compile test-classes/Child2.java
 * @compile test-classes/GrandChild1.java
 * @compile test-classes/ProfiledConfig.java
 * @compile test-classes/TestProfiledReceiver.java
 * @run main/othervm FilterCDSOption
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

public class FilterCDSOption {
    public static void main(String[] args) throws Exception {
        String wildcard = System.getProperty("user.dir") + "/*";
        String appJar1 = ClassFileInstaller.writeJar("filter_cds.jar", "Parent", "Child1", "Child2", "GrandChild1", "ProfiledConfig", "TestProfiledReceiver");
        String filePath = FilterCDSOption.class.getResource("/").getPath();
        // build The app
        String[] appClass = new String[] {"TestProfiledReceiver"};

        // dump class list
        OutputAnalyzer output = Test("-cp",
                                     appJar1,
                                     "-XX:+UseAppCDS",
                                     "-XX:+RelaxCheckForAppCDS",
                                     "-Xshare:off",
                                     "-XX:DumpLoadedClassList=filter_cds.list", 
                                     "TestProfiledReceiver");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // dump shared archive with appJar1
        output = Test("-cp",
                      appJar1,
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-Xshare:dump",
                      "-XX:SharedClassListFile=filter_cds.list",
                      "-XX:SharedArchiveFile=receiver.jsa",
                      "-version");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // save aot with shared archive and disable verify for shared archive
        output = Test("-cp",
                       appJar1,
                       "-XX:+UseAppCDS",
                       "-XX:+RelaxCheckForAppCDS",
                       "-XX:-VerifySharedSpaces",
                       "-Xshare:on",
                       "-XX:SharedArchiveFile=receiver.jsa", 
                       "-XX:CodeReviveOptions=save,file=classInDirCheck.csa,log=save=trace",
                       "-XX:-TieredCompilation",
                       "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                       "-XX:CompileCommand=inline,*.hi",
                       "-XX:CompileCommand=compileonly,*.hi",
                       "-XX:CompileCommand=inline,java.lang.Object::getClass",
                       "TestProfiledReceiver");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // load with aot and shared archive and enable verify for shared archive
        output = Test("-cp", appJar1,
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-XX:+VerifySharedSpaces",
                      "-Xshare:on",  
                      "-XX:SharedArchiveFile=receiver.jsa",
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // revive success
        System.out.println(output.getOutput());
        output.shouldContain("revive success: Child1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);


        // load with aot and cds without -XX:+RelaxCheckForAppCDS
        output = Test("-cp", appJar1,
                      "-XX:+UseAppCDS",
                      "-Xshare:on",  
                      "-XX:SharedArchiveFile=receiver.jsa",
                      "-XX:CodeReviveOptions=restore,file=classInDirCheck.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiver::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiver");

        // Expect: load aot successfully, revive method successfully
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
