/*
 * Copyright (C) 2022, 2023, Tencent. All rights reserved.
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
 * @summary csa file is incomplete
 * @requires (os.family == "linux") & (os.arch == "amd64")
 * @library /testlibrary
 * @compile test-classes/Dummy.java
 * @compile test-classes/TestInlineDummy.java
 * @run main/othervm FileInCompleteTest
 */
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.Utils;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import static java.nio.file.StandardCopyOption.REPLACE_EXISTING;
import java.nio.file.StandardOpenOption;
import static java.nio.file.StandardOpenOption.READ;
import static java.nio.file.StandardOpenOption.WRITE;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Random;

public class FileInCompleteTest {
    public static File csa;        // will be updated during test

    public static void shortenCsaFile(File csaFile) throws Exception {
        FileInputStream fis = new FileInputStream(csaFile);
        byte[] buf = new byte[(int)csaFile.length()];
        fis.read(buf);
        fis.close();
        writeBytesToFile(buf, csaFile);
    }

    private static void writeBytesToFile(byte[] bFile, File csaFile) {
        FileOutputStream fileOuputStream = null;
        try {
            int size = (int)csaFile.length() - 200;
            fileOuputStream = new FileOutputStream(csaFile);
            fileOuputStream.write(bFile, 0, size);
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (fileOuputStream != null) {
                try {
                    fileOuputStream.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public static void setReadWritePermission(File file) throws Exception {
        if (!file.canRead()) {
            if (!file.setReadable(true)) {
                throw new IOException("Cannot modify file " + file + " as readable");
            }
        }
        if (!file.canWrite()) {
            if (!file.setWritable(true)) {
                throw new IOException("Cannot modify file " + file + " as writable");
            }
        }
    }

    // dump with hello.jsa, then
    // read the jsa file
    //   1) run normal
    //   2) modify header
    public static void main(String... args) throws Exception {
        // build The app
        String[] appClass = new String[] {"TestInlineDummy"};

        // dump aot
        OutputAnalyzer output = TestCommon.testDump(appClass, "log=archive=trace",
                                                    "-XX:-TieredCompilation",
                                                    "-Xcomp",
                                                    "-XX:-BackgroundCompilation",
                                                    "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);

        output = TestCommon.testRunWithAOT(appClass, "log=restore=fail",
                                                     "-XX:-TieredCompilation",
                                                     "-Xcomp",
                                                     "-XX:-BackgroundCompilation",
                                                     "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        String[] expectedMethod = new String[] {"TestInlineDummy::foo", "Dummy::bar"};
        TestCommon.containAOTMethod(output, expectedMethod);

        // get current archive name
        csa = new File(TestCommon.getCurrentArchiveName());
        if (!csa.exists()) {
            throw new IOException(csa + " does not exist!");
        }

        setReadWritePermission(csa);

        // adjust csa file size, test should fail
        System.out.println("\n2. , should fail\n");
        shortenCsaFile(csa);

        output = TestCommon.testRunWithAOT(appClass, "log=restore=fail",
                                                     "-XX:-TieredCompilation",
                                                     "-Xcomp",
                                                     "-XX:-BackgroundCompilation",
                                                     "-XX:CompileOnly=TestInlineDummy.foo,Dummy.bar");
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(0);
        output.shouldContain("Incomplete archive file");
    }
}
