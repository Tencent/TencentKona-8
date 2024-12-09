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
 * @summary Test state of interface
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/TestInterfaceKlassReviveAndRestore.java
 * @run main/othervm InterfaceStateSave
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class InterfaceStateSave {
    public static void main(String[] args) throws Exception {
        String[] o0 = {
            "mirror by name and classloader : tag =9, kls = TestInterfaceKlassReviveAndRestore$Callable, loader = 2, meta_index = 3, init_status 1",
        };
        Test(o0,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore");

        String[] o1 = {
            "mirror by name and classloader : tag =9, kls = TestInterfaceKlassReviveAndRestore$Callable, loader = 2, meta_index = 3, init_status 2",
        };
        Test(o1,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore", "1");

        Test(o1,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore", "2");

        String[] o2 = {
            "mirror by name and classloader : tag =9, kls = TestInterfaceKlassReviveAndRestore$Callable, loader = 2, meta_index = 3, init_status 4",
        };
        Test(o2,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore", "3");

        String[] o3 = {
            "mirror by name and classloader : tag =9, kls = TestInterfaceKlassReviveAndRestore$DefaultInterface, loader = 2, meta_index = 4, init_status 1",
        };
        Test(o3,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore", "4");

        String[] o4 = {
            "mirror by name and classloader : tag =9, kls = TestInterfaceKlassReviveAndRestore$DefaultInterface, loader = 2, meta_index = 4, init_status 4",
        };
        Test(o4,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore", "5");

        String[] o5 = {
            "mirror by name and classloader : tag =9, kls = TestInterfaceKlassReviveAndRestore$DefaultInterface1, loader = 2, meta_index = 4, init_status 4",
        };
        Test(o5,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore", "6");

        String[] o6 = {
            "mirror by name and classloader : tag =9, kls = TestInterfaceKlassReviveAndRestore$DefaultInterface1, loader = 2, meta_index = 4, init_status 2",
        };
        Test(o6,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore", "7");

        String[] o7 = {
            "mirror by name and classloader : tag =9, kls = TestInterfaceKlassReviveAndRestore$DefaultInterface1, loader = 2, meta_index = 4, init_status 4",
        };
        Test(o7,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=save,file=1.csa,log=archive=trace",
             "TestInterfaceKlassReviveAndRestore", "8");

        // all match
        String[] expect_outputs_one_all_match = {
            "Revive interface klass TestInterfaceKlassReviveAndRestore$DefaultInterface1 at state linked(2)",
            "OK for TestInterfaceKlassReviveAndRestore.foo(I)Ljava/lang/Class; v0 on revive preprocess",
        };
        Test(expect_outputs_one_all_match,
             "-XX:-TieredCompilation",
             "-XX:CompileOnly=TestInterfaceKlassReviveAndRestore.foo",
             "-XX:-BackgroundCompilation",
             "-XX:CodeReviveOptions=restore,file=1.csa,log=restore=info",
             "TestInterfaceKlassReviveAndRestore", "7");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        String str = output.getOutput();
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
    }
}
