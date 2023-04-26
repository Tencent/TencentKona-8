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

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.JDKToolFinder;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

/**
 * @test RuntimeMemoryTest
 * @key gc
 * @summary test java.lang.Runtime max memory and total memory
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseParallelGC RuntimeMemoryTest
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseG1GC RuntimeMemoryTest
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseConcMarkSweepGC RuntimeMemoryTest
 */
public class RuntimeMemoryTest extends TestBase {
    static Object[] root_array;
    static final long M = 1024L * 1024L;
    public static void main(String[] args) throws Exception {
        int pid = ProcessTools.getProcessId();
        /*
         * Steps: start with 2G heap
         * 1. start and allocate about 1G object
         * 2. get totalMemory/maxMemory/freeMemory as expected
         * 3. launch jcmd resize to 100M and expect success
         * 4. get totalMemory/maxMemory/freeMemory as expected
         */
        Runtime r = Runtime.getRuntime();
        // GC rarely happens between these call, should align with size
        long max = r.maxMemory() / M;
        long total = r.totalMemory() / M;
        long free = r.freeMemory() / M;
        System.out.println("Before alloc -- Max: " + max + "M, Total: " + total + "M, Free: " + free + "M");
        alloc_and_free(1024L * 1024L * 1024L);

        max = r.maxMemory() / M;
        total = r.totalMemory() / M;
        free = r.freeMemory() / M;
        root_array = null; // release
        System.out.println("After alloc -- Max: " + max + "M, Total: " + total + "M, Free: " + free + "M");
        Asserts.assertGT(max, 1024L);
        Asserts.assertGTE(max, total);
        Asserts.assertGT(max, free);

        // shrink to 500M should be fine for any GC
        String[] contains1 = {
            "GC.elastic_max_heap (2097152K->102400K)(2097152K)",
            "GC.elastic_max_heap success"
        };
        resizeAndCheck(pid, "100M", contains1, null);
        max = r.maxMemory() / M;
        total = r.totalMemory() / M;
        free = r.freeMemory() / M;
        System.out.println("After resize -- Max: " + max + "M, Total: " + total + "M, Free: " + free + "M");
        Asserts.assertLT(max, 101L);
        Asserts.assertGTE(max, total);
        Asserts.assertGT(max, free);
    }

    static void alloc_and_free(long size) {
        // suppose compressed
        // each int array size is 8(MarkOop + len) + 4 * len
        // each object is 1k int[254]
        int root_len = (int)(size / 1024L);
        root_array = new Object[root_len];
        for (int i = 0; i < root_len; i++) {
            root_array[i] = new int[254];
        }
    }
}
