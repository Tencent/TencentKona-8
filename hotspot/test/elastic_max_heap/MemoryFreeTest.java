/*
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.management.ManagementFactory;
import java.util.ArrayList;
import java.util.Random;
import java.util.regex.Pattern;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.JDKToolFinder;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

/**
 * @test MemoryFreeTest
 * @key gc
 * @summary test if elaistc max heap free physical memory accordingly
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseParallelGC MemoryFreeTest
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseG1GC MemoryFreeTest
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseConcMarkSweepGC MemoryFreeTest
 */
public class MemoryFreeTest extends TestBase {
    static Object[] root_array;
    public static void main(String[] args) throws Exception {
        int pid = ProcessTools.getProcessId();
        /*
         * Steps: start with 2G heap
         * 1. start and allocate about 1G object
         * 2. get RSS and above 1G
         * 3. launch jcmd resize to 100M and expect success
         * 4. get RSS again, should less than 350M
         */
        long rss = getLinuxPidVmRSS(pid);
        System.out.println("RSS before alloc " + rss + "K");
        alloc_and_free(1024L * 1024L * 1024L);
        rss = getLinuxPidVmRSS(pid); // rss result is K
        root_array = null; // release
        System.out.println("RSS after alloc " + rss + "K");
        Asserts.assertGT(rss, 1024L * 1024L);

        // shrink to 500M should be fine for any GC
        String[] contains1 = {
            "GC.elastic_max_heap (2097152K->102400K)(2097152K)",
            "GC.elastic_max_heap success"
        };
        resizeAndCheck(pid, "100M", contains1, null);
        rss = getLinuxPidVmRSS(pid); // rss result is K
        System.out.println("RSS after resize " + rss + "K");
        Asserts.assertLT(rss, 350L * 1024L);
    }

    static void alloc_and_free(long size) {
        // suppose compressed
        // each int array size is 8(MarkOop + len) + 4 * len
        // each object is 1k int[254]
        int root_len = (int)(size / 1024L);
        System.out.println(root_len);
        root_array = new Object[root_len];
        for (int i = 0; i < root_len; i++) {
            root_array[i] = new int[254];
        }
    }

    private static long getLinuxPidVmRSS(long pid) throws Exception {
        Pattern pattern = Pattern.compile("[^0-9]");
        String command = "cat /proc/" + pid + "/status";
        long res = 0;
        Process process = Runtime.getRuntime().exec(command);
        BufferedReader input = new BufferedReader(new InputStreamReader(process.getInputStream()));
        String line = "";
        while ((line = input.readLine()) != null) {
            if (line.startsWith("VmRSS", 0)) {
                res = Long.parseLong(pattern.matcher(line).replaceAll(""));
                break;
            }
        }
        input.close();
        return res;
    }
}
