/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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
 * @summary Handling of directories in -cp is based on the classlist
 * @library /testlibrary
 * @compile test-classes/Hello.java
 * @run main/othervm DirClasspathTest
 */

import com.oracle.java.testlibrary.Platform;
import com.oracle.java.testlibrary.OutputAnalyzer;
import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

public class DirClasspathTest {
    private static final int MAX_PATH = 260;

    public static void main(String[] args) throws Exception {
        File dir = new File(System.getProperty("user.dir"));
        File emptydir = new File(dir, "emptydir");
        emptydir.mkdir();

        /////////////////////////////////////////////////////////////////
        // The classlist only contains boot class in following test cases
        /////////////////////////////////////////////////////////////////
        String bootClassList[] = {"java/lang/Object"};

        // Empty dir in -cp: should be OK
        OutputAnalyzer output;
        output = TestCommon.dump(emptydir.getPath(), bootClassList);
        TestCommon.checkDump(output);

        // Long path to empty dir in -cp: should be OK
        Path classDir = Paths.get(System.getProperty("test.classes"));
        Path destDir = classDir;
        int subDirLen = MAX_PATH - classDir.toString().length() - 2;
        if (subDirLen > 0) {
            char[] chars = new char[subDirLen];
            Arrays.fill(chars, 'x');
            String subPath = new String(chars);
            destDir = Paths.get(System.getProperty("test.classes"), subPath);
        }
        File longDir = destDir.toFile();
        longDir.mkdir();
        File subDir = new File(longDir, "subdir");
        subDir.mkdir();
        output = TestCommon.dump(subDir.getPath(), bootClassList);
        TestCommon.checkDump(output);

        // Non-empty dir in -cp: should be OK
        // <dir> is not empty because it has at least one subdirectory, i.e., <emptydir>
        output = TestCommon.dump(dir.getPath(), bootClassList);
        TestCommon.checkDump(output);

        // Long path to non-empty dir in -cp: should be OK
        // <dir> is not empty because it has at least one subdirectory, i.e., <emptydir>
        output = TestCommon.dump(longDir.getPath(), bootClassList);
        TestCommon.checkDump(output);

        /////////////////////////////////////////////////////////////////
        // The classlist contains non-boot class in following test cases
        /////////////////////////////////////////////////////////////////
        String appClassList[] = {"java/lang/Object", "com/sun/tools/javac/Main"};

        // Non-empty dir in -cp: should be OK (as long as no classes were loaded from there)
        output = TestCommon.dump(dir.getPath(), appClassList);
        TestCommon.checkDump(output);

        // Long path to non-empty dir in -cp: should be OK (as long as no classes were loaded from there)
        output = TestCommon.dump(longDir.getPath(), appClassList);
        TestCommon.checkDump(output);

        /////////////////////////////////////////////////////////////////
        // Loading an app class from a directory
        /////////////////////////////////////////////////////////////////
        String appClassList2[] = {"Hello", "java/lang/Object", "com/sun/tools/javac/Main"};
        // Non-empty dir in -cp: should report error if a class is loaded from it
        output = TestCommon.dump(classDir.toString(), appClassList2);
	output.shouldNotHaveExitValue(0);
        output.shouldContain("Cannot have non-empty directory in paths");

        // Long path to non-empty dir in -cp: should report error if a class is loaded from it
        File srcClass = new File(classDir.toFile(), "Hello.class");
        File destClass = new File(longDir, "Hello.class");
        try {
            Files.copy(srcClass.toPath(), destClass.toPath());
        } catch (Exception e) {
        }
        output = TestCommon.dump(longDir.getPath(), appClassList2);
    	output.shouldNotHaveExitValue(0);
        output.shouldContain("Cannot have non-empty directory in paths");
    }
}

