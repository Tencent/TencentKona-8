/*
 * Copyright (C) 2023, 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

import java.lang.management.*;
import java.util.*;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.JDKToolFinder;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

/**
 * @test MemoryFreeTest
 * @key gc
 * @summary test MemoryPool MemoryUsage returns correct max size
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseParallelGC MemoryPoolTest
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseG1GC MemoryPoolTest
 * @run main/othervm -Xms50M -Xmx2G -XX:+ElasticMaxHeap -XX:+UseConcMarkSweepGC MemoryPoolTest
 */
public class MemoryPoolTest extends TestBase {
    static Object[] root_array;
    static final long M = 1024L * 1024L;
    static long edenMaxSize;
    static long survivorMaxSize;
    static long oldGenMaxSize;
    public static void main(String[] args) throws Exception {
        String architecture = System.getProperty("os.arch");
        int pid = ProcessTools.getProcessId();
        /*
         * Steps: start with 2G heap
         * 1. start and allocate about 1G object
         * 2. get MemoryPool and usage as expected
         * 3. launch jcmd resize to 100M and expect success
         * 4. get MemoryPool and usage as expected
         */
        alloc_and_free(1024L * 1024L * 1024L);
        MemoryMXBean mem = ManagementFactory.getMemoryMXBean();
        MemoryUsage usage = mem.getHeapMemoryUsage();
        long max = usage.getMax() / M;
        long committed = usage.getCommitted() / M;
        long used = usage.getUsed() / M;
        System.out.println("After alloc -- Heap Max: " + max +
                           "M, Committed: " + committed +
                           "M, Used: " + used + "M");
        Asserts.assertGT(max, 1024L);
        Asserts.assertGTE(max, committed);
        Asserts.assertGTE(committed, used);

        // check eden, survivor and old memory pool after alloc
        get_memory_info();
        System.out.println("After alloc -- Eden Max: " + edenMaxSize +
                           "M, Survivor Max: " + survivorMaxSize +
                           "M, Old Max: " + oldGenMaxSize + "M");
        long orig_old = oldGenMaxSize;
        long orig_eden = edenMaxSize;
        Asserts.assertGT(edenMaxSize + survivorMaxSize + oldGenMaxSize, 1024L);
        Asserts.assertGTE(max, oldGenMaxSize);
        Asserts.assertGT(oldGenMaxSize, edenMaxSize);
        Asserts.assertGT(oldGenMaxSize, survivorMaxSize);

        root_array = null; // release

        // shrink to 500M should be fine for any GC
        String[] contains1 = {
            "GC.elastic_max_heap (2097152K->102400K)(2097152K)",
            "GC.elastic_max_heap success"
        };
        if (architecture.equals("aarch64")) {
            contains1 = new String[] {
                "GC.elastic_max_heap (2097152K->131072K)(2097152K)",
                "GC.elastic_max_heap success"
            };
        }
        resizeAndCheck(pid, "100M", contains1, null);
        mem = ManagementFactory.getMemoryMXBean();
        usage = mem.getHeapMemoryUsage();
        max = usage.getMax() / M;
        committed = usage.getCommitted() / M;
        used = usage.getUsed() / M;
        System.out.println("After resize -- Heap Max: " + max +
                           "M, Committed: " + committed +
                           "M, Used: " + used + "M");
        long target_size = 100L;
        if (architecture.equals("aarch64")) {
            target_size = 128L;
        }
        Asserts.assertLTE(max, target_size);
        Asserts.assertGTE(max, committed);
        Asserts.assertGTE(committed, used);

        // check eden, survivor and old memory pool after resize
        get_memory_info();
        System.out.println("After resize -- Eden Max: " + edenMaxSize +
                           "M, Survivor Max: " + survivorMaxSize +
                           "M, Old Max: " + oldGenMaxSize + "M");
        Asserts.assertLTE(edenMaxSize + survivorMaxSize + oldGenMaxSize, target_size);
        Asserts.assertLT(oldGenMaxSize, orig_old);
        Asserts.assertLTE(edenMaxSize, orig_eden);
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

    static void get_memory_info() {
        List<MemoryPoolMXBean> memoryPoolMXBeans = ManagementFactory.getMemoryPoolMXBeans();
        for (MemoryPoolMXBean memoryPoolMXBean : memoryPoolMXBeans) {
            String name = memoryPoolMXBean.getName();
            MemoryUsage usage = memoryPoolMXBean.getUsage();
            long max_size = usage.getMax() / M;
            if (name.contains("Eden")) {
                edenMaxSize = max_size;
            } else if (name.contains("Survivor")) {
                survivorMaxSize = max_size;
            } else if (name.contains("Old")) {
                oldGenMaxSize = max_size;
            }
        }
    }
}
