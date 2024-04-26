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

/*
 * @test ShrinkGCTest
 * @summary Test if shrink can be done without full gc
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @compile test_classes/ShrinkGCTestBasic.java
 * @run main/othervm ShrinkGCTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

public class ShrinkGCTest extends TestBase {
    public static void main(String[] args) throws Exception {
        String architecture = System.getProperty("os.arch");
        // ParallelGC
        String[] contains1 = {
            "GC.elastic_max_heap (4194304K->3145728K)(4194304K)",
            "GC.elastic_max_heap success"
        };
        String[] not_contains1 = {
            "PS_ElasticMaxHeapOp heap after Full GC"
        };
        Test("-XX:+UseParallelGC", "3G", "false", contains1, not_contains1);
        String[] contains2 = {
            "PS_ElasticMaxHeapOp heap after Full GC",
            "GC.elastic_max_heap (4194304K->512000K)(4194304K)",
            "GC.elastic_max_heap success"
        };
        if (architecture.equals("aarch64")) {
            contains2 = new String[] {
                "PS_ElasticMaxHeapOp heap after Full GC",
                "GC.elastic_max_heap (4194304K->524288K)(4194304K)",
                "GC.elastic_max_heap success"
            };
        }
        Test("-XX:+UseParallelGC", "500M", "false", contains2, null);

        // CMS
        String[] not_contains3 = {
            "Gen_ElasticMaxHeapOp heap after Full GC"
        };
        Test("-XX:+UseConcMarkSweepGC", "3G", "false", contains1, not_contains3);
        String[] contains4 = {
            "Gen_ElasticMaxHeapOp heap after Full GC",
            "GC.elastic_max_heap (4194304K->512000K)(4194304K)",
            "GC.elastic_max_heap success"
        };
        if (architecture.equals("aarch64")) {
            contains4 = new String[] {
                "Gen_ElasticMaxHeapOp heap after Full GC",
                "GC.elastic_max_heap (4194304K->524288K)(4194304K)",
                "GC.elastic_max_heap success"
            };
        }
        Test("-XX:+UseConcMarkSweepGC", "500M", "false", contains4, null);

        // G1GC
        String[] not_contains5 = {
            "G1_ElasticMaxHeapOp heap after Young GC",
            "G1_ElasticMaxHeapOp heap after Full GC"
        };
        Test("-XX:+UseG1GC", "3G", "false", contains1, not_contains5);
        String[] contains6 = {
            "G1_ElasticMaxHeapOp heap after Young GC",
            "G1_ElasticMaxHeapOp heap after Full GC",
            "GC.elastic_max_heap (4194304K->51200K)(4194304K)",
            "GC.elastic_max_heap success"
        };
        if (architecture.equals("aarch64")) {
            contains6 = new String[] {
                "G1_ElasticMaxHeapOp heap after Young GC",
                "G1_ElasticMaxHeapOp heap after Full GC",
                "GC.elastic_max_heap (4194304K->65536K)(4194304K)",
                "GC.elastic_max_heap success"
            };
        }
        Test("-XX:+UseG1GC", "50M", "false", contains6, null);
        String[] contains7 = {
            "G1_ElasticMaxHeapOp heap after Young GC",
            "GC.elastic_max_heap (4194304K->51200K)(4194304K)",
            "GC.elastic_max_heap success"
        };
        if (architecture.equals("aarch64")) {
            contains7 = new String[] {
                "G1_ElasticMaxHeapOp heap after Young GC",
                "GC.elastic_max_heap (4194304K->65536K)(4194304K)",
                "GC.elastic_max_heap success"
            };
        }
        String[] not_contains7 = {
            "G1_ElasticMaxHeapOp heap after Full GC"
        };
        // trigger Young GC only may fail due to the uncertainty of environment
        // may not trigger gc or trigger Young GC and Full GC
        Test("-XX:+UseG1GC", "50M", "true", contains7, not_contains7);
    }

    private static void Test(String heap_type, String new_size, String young_gen_larger, String[] contains, String[] not_contains) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(heap_type,
                                                                  "-Dtest.jdk=" + System.getProperty("test.jdk"),
                                                                  "-XX:+ElasticMaxHeap",
                                                                  "-XX:+TraceElasticMaxHeap",
                                                                  "-Xms50M",
                                                                  "-Xmx4G",
                                                                  "ShrinkGCTestBasic",
                                                                  new_size,
                                                                  young_gen_larger);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        CheckOutput(output, contains, not_contains);
    }

    public static void CheckOutput(OutputAnalyzer output, String[] contains, String[] not_contains) throws Exception {
        System.out.println(output.getOutput());
        if (contains != null) {
            for (String s : contains) {
                output.shouldContain(s);
            }
        }
        if (not_contains != null) {
            for (String s : not_contains) {
                output.shouldNotContain(s);
            }
        }
    }
}
