/*
 * Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
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
 * @summary SharedArchiveConsistency
 * @library /testlibrary
 * @compile test-classes/Hello.java
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions SharedArchiveConsistency
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

public class SharedArchiveConsistency {
    public static File jsa;        // will be updated during test
    public static File orgJsaFile; // kept the original file not touched.

    public static String[] matchMessages = {
        "Unable to use shared archive",
        "An error has occurred while processing the shared archive file.",
        "Checksum verification failed.",
        "The shared archive file has been truncated."
    };

    public static void writeData(FileChannel fc, long offset, ByteBuffer bb) throws Exception {
        fc.position(offset);
        fc.write(bb);
    }

    public static FileChannel getFileChannel(File jsaFile) throws Exception {
        List<StandardOpenOption> arry = new ArrayList<StandardOpenOption>();
        arry.add(READ);
        arry.add(WRITE);
        return FileChannel.open(jsaFile.toPath(), new HashSet<StandardOpenOption>(arry));
    }

    public static int getFileHeaderSize(FileChannel fc) throws Exception {
        return 256;
    }

    private static long getRandomBetween(long start, long end) throws Exception {
        if (start > end) {
            throw new IllegalArgumentException("start must be less than end");
        }
        Random aRandom = new Random(100);
        int d = aRandom.nextInt((int)(end - start));
        if (d < 1) {
            d = 1;
        }
        return start + d;
    } 

    public static void modifyJsaHeader(File jsaFile) throws Exception {
        FileChannel fc = getFileChannel(jsaFile);
        // screw up header info
        byte[] buf = new byte[getFileHeaderSize(fc)];
        ByteBuffer bbuf = ByteBuffer.wrap(buf);
        writeData(fc, 0L, bbuf);
        if (fc.isOpen()) {
            fc.close();
        }
    }

    public static void copyFile(File from, File to) throws Exception {
        if (to.exists()) {
            if(!to.delete()) {
                throw new IOException("Could not delete file " + to);
            }
        }
        to.createNewFile();
        setReadWritePermission(to);
        Files.copy(from.toPath(), to.toPath(), REPLACE_EXISTING);
    }

    // Copy file with bytes deleted or inserted
    // del -- true, deleted, false, inserted
    public static void copyFile(File from, File to, boolean del) throws Exception {
        try (
            FileChannel inputChannel = new FileInputStream(from).getChannel();
            FileChannel outputChannel = new FileOutputStream(to).getChannel()
        ) {
            long size = inputChannel.size();
            int init_size = getFileHeaderSize(inputChannel);
            outputChannel.transferFrom(inputChannel, 0, init_size);
            int n = (int)getRandomBetween(0, 1024);
            if (del) {
                System.out.println("Delete " + n + " bytes at data start section");
                inputChannel.position(init_size + n);
                outputChannel.transferFrom(inputChannel, init_size, size - init_size - n);
            } else {
                System.out.println("Insert " + n + " bytes at data start section");
                outputChannel.position(init_size);
                outputChannel.write(ByteBuffer.wrap(new byte[n]));
                outputChannel.transferFrom(inputChannel, init_size + n , size - init_size);
            }
        }
    }

    public static void restoreJsaFile() throws Exception {
        Files.copy(orgJsaFile.toPath(), jsa.toPath(), REPLACE_EXISTING);
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
    //   3) keep header correct but modify content
    //   4) update both header and content, test
    //   5) delete bytes in data begining
    //   6) insert bytes in data begining
    //   7) randomly corrupt data in four areas: RO, RW. MISC DATA, MISC CODE
    public static void main(String... args) throws Exception {
        // must call to get offset info first!!!
        Path currentRelativePath = Paths.get("");
        String currentDir = currentRelativePath.toAbsolutePath().toString();
        System.out.println("Current relative path is: " + currentDir);
        // get jar file
        String jarFile = JarBuilder.getOrCreateHelloJar();

        // dump (appcds.jsa created)
        TestCommon.testDump(jarFile, null);

        // test, should pass
        System.out.println("1. Normal, should pass but may fail\n");
        String[] execArgs = {"-XX:+PrintSharedSpaces", "-cp", jarFile, "Hello"};

        OutputAnalyzer output = TestCommon.execCommon(execArgs);

        try {
            TestCommon.checkExecReturn(output, 0, true, "Hello World");
        } catch (Exception e) {
            TestCommon.checkExecReturn(output, 1, true, matchMessages[0]);
        }

        // get current archive name
        jsa = new File(TestCommon.getCurrentArchiveName());
        if (!jsa.exists()) {
            throw new IOException(jsa + " does not exist!");
        }

        setReadWritePermission(jsa);

        // save as original untouched
        orgJsaFile = new File(new File(currentDir), "appcds.jsa.bak");
        copyFile(jsa, orgJsaFile);

        // modify jsa header, test should fail
        System.out.println("\n2. Corrupt header, should fail\n");
        modifyJsaHeader(jsa);
        output = TestCommon.execCommon(execArgs);
        output.shouldContain("The shared archive file has a bad magic number");
    }
}
