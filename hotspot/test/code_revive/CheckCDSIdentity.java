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
 * @summary check the identity from shared archive
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/ParentC.java
 * @compile test-classes/ChildC1.java
 * @compile test-classes/ChildC2.java
 * @compile test-classes/GrandChildC1.java
 * @compile test-classes/ProfiledConfigC.java
 * @compile test-classes/TestProfiledReceiverC.java
 * @run main/othervm CheckCDSIdentity
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

public class CheckCDSIdentity {
    public static void main(String[] args) throws Exception {
        String wildcard = System.getProperty("user.dir") + "/*";
        String appJar1 = ClassFileInstaller.writeJar("recevier.jar", "ParentC", "ChildC1", "ChildC2", "GrandChildC1", "ProfiledConfigC");
        String appJar2 = ClassFileInstaller.writeJar("config.jar", "TestProfiledReceiverC");
        String filePath = CheckCDSIdentity.class.getResource("/").getPath();
        // build The app
        String[] appClass = new String[] {"TestProfiledReceiverC"};

        // dump class list
        OutputAnalyzer output = Test("-cp",
                                     appJar1 + ":" + appJar2,
                                     "-XX:+UseAppCDS",
                                     "-XX:+RelaxCheckForAppCDS",
                                     "-Xshare:off",
                                     "-XX:DumpLoadedClassList=check_cds.list", 
                                     "TestProfiledReceiverC");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // dump shared archive with appJar1
        output = Test("-cp",
                      appJar1,
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-Xshare:dump",
                      "-XX:SharedClassListFile=check_cds.list",
                      "-XX:SharedArchiveFile=receiver.jsa",
                      "-version");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // dump shared archive with appJar1+2
        output = Test("-cp",
                      appJar1 + ":" + appJar2,
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-Xshare:dump",
                      "-XX:SharedClassListFile=check_cds.list",
                      "-XX:SharedArchiveFile=config.jsa",
                      "-version");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // save aot with shared archive receiver.jsa 
        output = Test("-cp",
                       appJar1 + ":" + appJar2,
                       "-XX:+UseAppCDS",
                       "-XX:+RelaxCheckForAppCDS",
                       "-Xshare:on",
                       "-XX:SharedArchiveFile=receiver.jsa", 
                       "-XX:CodeReviveOptions=save,file=cdsCheckIdentity.csa,log=save=trace",
                       "-XX:-TieredCompilation",
                       "-XX:CompileCommand=compileonly,TestProfiledReceiverC::foo",
                       "-XX:CompileCommand=inline,*.hi",
                       "-XX:CompileCommand=compileonly,*.hi",
                       "-XX:CompileCommand=inline,java.lang.Object::getClass",
                       "TestProfiledReceiverC");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // load with aot and shared archive receiver.jsa
        output = Test("-cp", appJar1 + ":" + appJar2,
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-Xshare:on",  
                      "-XX:SharedArchiveFile=receiver.jsa",
                      "-XX:CodeReviveOptions=restore,file=cdsCheckIdentity.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiverC::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiverC");

        // revive success with the same aot and cds file
        System.out.println(output.getOutput());
        output.shouldContain("revive success: ChildC1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);


        // load with aot and another cds.
        output = Test("-cp", appJar1 + ":" + appJar2,
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-Xshare:on",  
                      "-XX:SharedArchiveFile=config.jsa",
                      "-XX:CodeReviveOptions=restore,file=cdsCheckIdentity.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiverC::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiverC");

        // Expect: load aot successfully, and fail to revive method 
        System.out.println(output.getOutput());
        output.shouldNotContain("revive success: ChildC1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);

        // save aot with shared archive config.jsa 
        output = Test("-cp",
                       appJar1 + ":" + appJar2,
                       "-XX:+UseAppCDS",
                       "-XX:+RelaxCheckForAppCDS",
                       "-Xshare:on",
                       "-XX:SharedArchiveFile=config.jsa", 
                       "-XX:CodeReviveOptions=save,file=cdsCheckIdentity_config.csa,log=save=trace",
                       "-XX:-TieredCompilation",
                       "-XX:CompileCommand=compileonly,TestProfiledReceiverC::foo",
                       "-XX:CompileCommand=inline,*.hi",
                       "-XX:CompileCommand=compileonly,*.hi",
                       "-XX:CompileCommand=inline,java.lang.Object::getClass",
                       "TestProfiledReceiverC");

        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        // merge the aot files
        output = Test("-cp", appJar1 + ":" + appJar2,
                      "-XX:CodeReviveOptions=merge,input_files=.,file=cdsCheckIdentity_merge.csa,log=merge=trace",
                      "-version");
        System.out.println(output.getOutput());
                      
        // load with merged aot and shared archive receiver.jsa
        output = Test("-cp", appJar1 + ":" + appJar2,
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-Xshare:on",  
                      "-XX:SharedArchiveFile=receiver.jsa",
                      "-XX:CodeReviveOptions=restore,file=cdsCheckIdentity_merge.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiverC::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiverC");

        // revive success
        System.out.println(output.getOutput());
        output.shouldContain("revive success: ChildC1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);


        // load with merged aot and shared archive config.jsa
        output = Test("-cp", appJar1 + ":" + appJar2,
                      "-XX:+UseAppCDS",
                      "-XX:+RelaxCheckForAppCDS",
                      "-Xshare:on",  
                      "-XX:SharedArchiveFile=config.jsa",
                      "-XX:CodeReviveOptions=restore,file=cdsCheckIdentity_merge.csa,log=restore=trace",
                      "-XX:-TieredCompilation",
                      "-XX:CompileCommand=compileonly,TestProfiledReceiverC::foo",
                      "-XX:CompileCommand=inline,*.hi",
                      "-XX:CompileCommand=compileonly,*.hi",
                      "-XX:CompileCommand=inline,java.lang.Object::getClass",
                      "TestProfiledReceiverC");

        // Expect: load aot successfully, and revive successfully.
        System.out.println(output.getOutput());
        output.shouldContain("revive success: ChildC1.hi()Ljava/lang/String");
        output.shouldHaveExitValue(0);
    }

    static OutputAnalyzer Test(String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        return output;
    }
}
