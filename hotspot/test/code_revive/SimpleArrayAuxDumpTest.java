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
 * @summary Test whether array kls is supported
 * @library /testlibrary
 * @compile test-classes/TestArrayKlsReloc.java
 * @run main/othervm SimpleArrayAuxDumpTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class SimpleArrayAuxDumpTest {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] {"TestArrayKlsReloc"};

        // dump aot
        OutputAnalyzer output = TestCommon.testDump(appClass, "log=save=trace",
                                                    "-XX:-TieredCompilation",
                                                    "-XX:-BackgroundCompilation");

        System.out.println(output.getOutput());
        output.shouldContain("Emit method TestArrayKlsReloc.foo3()[[I success");
        output.shouldContain("Emit method TestArrayKlsReloc.foo2()[I success");
        output.shouldContain("Emit method TestArrayKlsReloc.foo1()[[Ljava/util/Hashtable; success");
        output.shouldContain("Emit method TestArrayKlsReloc.foo()[Ljava/util/Hashtable; success");
        output.shouldHaveExitValue(0);

        // restore aot
        output = TestCommon.testRunWithAOT(appClass, null,
                                           "-XX:-TieredCompilation",
                                           "-XX:-BackgroundCompilation");
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        String[] expectedMethod = new String[] {"TestArrayKlsReloc::foo",
                                                "TestArrayKlsReloc::foo1",
                                                "TestArrayKlsReloc::foo2",
                                                "TestArrayKlsReloc::foo3"};
        TestCommon.containAOTMethod(output, expectedMethod);
    }
}
