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
 * @summary Test whether metadata index is correctly updated during merge.
 * @library /testlibrary
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @compile test-classes/LoadConstant.java
 * @run main/othervm UpdateMetaIndex
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;


public class UpdateMetaIndex {
    public static void main(String[] args) throws Exception {
        // build The app
        String[] appClass = new String[] { "TestInlineDummy" };
        // dump aot
        TestCommon.setCurrentArchiveName("1.csa");
        OutputAnalyzer output = TestCommon.testDump(appClass, "log=archive=trace",
            "-Xcomp",
            "-XX:-BackgroundCompilation");
        output.shouldHaveExitValue(0);

        appClass = new String[] { "LoadConstant" };
        // dump aot
        TestCommon.setCurrentArchiveName("2.csa");
        output = TestCommon.testDump(appClass, "log=archive=trace",
            "-Xcomp",
            "-XX:-BackgroundCompilation");
        output.shouldHaveExitValue(0);

        TestCommon.setCurrentArchiveName("merged.csa");
        output = TestCommon.mergeCSA(null, "input_files=.,log=merge=trace", "-Xcomp", "-version");
        output.shouldHaveExitValue(0);

        // run merged csa file
        appClass = new String[] { "TestInlineDummy", "LoadConstant" };
        output = TestCommon.runWithArchive(appClass, "log=merge=trace", "-Xcomp");
        output.shouldHaveExitValue(0);

        // print merged csa file
        output = TestCommon.runWithArchive(appClass, "print_opt");
        output.shouldHaveExitValue(0);
    }
}
