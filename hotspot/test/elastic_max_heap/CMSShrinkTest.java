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
 * @test CMSShrinkTest
 * @summary Test whether exp_EMH_size can take effect when CMS shrink
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @compile test_classes/ShrinkGCTestBasic.java
 * @run main/othervm CMSShrinkTest
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

public class CMSShrinkTest extends TestBase {
    public static void main(String[] args) throws Exception {
        String architecture = System.getProperty("os.arch");
        String[] contains1 = {
            "CardGeneration: shrink according to ElasticMaxHeap",
            "DefNewGeneration: shrink according to ElasticMaxHeap",
            "GC.elastic_max_heap (2097152K->102400K)(4194304K)",
            "GC.elastic_max_heap success"
        };
        if (architecture.equals("aarch64")) {
            contains1 = new String[] {
                "CardGeneration: shrink according to ElasticMaxHeap",
                "DefNewGeneration: shrink according to ElasticMaxHeap",
                "GC.elastic_max_heap (2097152K->131072K)(4194304K)",
                "GC.elastic_max_heap success"
            };
        }
        Test("-XX:+UseConcMarkSweepGC", "100M", contains1, null);
    }

    private static void Test(String heap_type, String new_size, String[] contains, String[] not_contains) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(heap_type,
                                                                  "-Dtest.jdk=" + System.getProperty("test.jdk"),
                                                                  "-XX:+ElasticMaxHeap",
                                                                  "-XX:+TraceElasticMaxHeap",
                                                                  "-Xms50M",
                                                                  "-Xmx2G",
                                                                  "-XX:ElasticMaxHeapSize=4G",
                                                                  "ShrinkGCTestBasic",
                                                                  new_size,
                                                                  "false");
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
