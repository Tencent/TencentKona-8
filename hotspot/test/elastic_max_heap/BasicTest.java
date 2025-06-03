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
 * @test
 * @summary Test Basic Elastic Max Heap resize
 * @requires (os.family == "linux") & ((os.arch == "amd64") | (os.arch == "aarch64") | (os.arch == "aarch64" | (os.arch == "loongarch64")))
 * @library /testlibrary
 * @compile test_classes/NotActiveHeap.java
 * @run main/othervm BasicTest
 */

import java.lang.reflect.Field;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.JDKToolFinder;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class BasicTest extends TestBase {
    public static void main(String[] args) throws Exception {
        test("-XX:+UseParallelGC");
        test("-XX:+UseConcMarkSweepGC");
        test("-XX:+UseG1GC");
        // EMH uses PS by default when no GC option is specified in command line
        test("-XX:ActiveProcessorCount=1");
    }

    private static void test(String heap_type_or_process_count) throws Exception {
        String architecture = System.getProperty("os.arch");
        // Xms = 100M - 1B, Xmx = 600M - 1B, ElasticMaxHeapSize = 1G - 1B
        // unaligned arguments should be fine
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, heap_type_or_process_count, "-XX:+ElasticMaxHeap", "-Xms104857599", "-Xmx629145599", "-XX:ElasticMaxHeapSize=1073741823", "NotActiveHeap");
        Process p = pb.start();
        try {
            long pid = getPidOfProcess(p);
            System.out.println(pid);

            // shrink to 500M should be fine for any GC
            String[] contains1 = {
                "GC.elastic_max_heap success",
                "GC.elastic_max_heap (614400K->512000K)(1048576K)",
            };
            if (architecture.equals("aarch64")) {
                contains1 = new String[] {
                    "GC.elastic_max_heap success",
                    "GC.elastic_max_heap (622592K->524288K)(1048576K)",
                };
            } else if (architecture.equals("loongarch64")) {
                contains1 = new String[] {
                    "GC.elastic_max_heap success",
                    "GC.elastic_max_heap (614400K->516096K)(1048576K)",
                };
            }
            resizeAndCheck(pid, "500M", contains1, null);

            // expand to 800M should be fine for any GC
            String[] contains2 = {
                "GC.elastic_max_heap success",
                "GC.elastic_max_heap (512000K->819200K)(1048576K)",
            };
            if (architecture.equals("aarch64")) {
                contains2 = new String[] {
                    "GC.elastic_max_heap success",
                    "GC.elastic_max_heap (524288K->819200K)(1048576K)",
                };
            } else if (architecture.equals("loongarch64")) {
                contains2 = new String[] {
                    "GC.elastic_max_heap success",
                    "GC.elastic_max_heap (516096K->819200K)(1048576K)",
                };
	    }
            resizeAndCheck(pid, "800M", contains2, null);

            // expand to 2G should fail
            String[] contains3 = {
                "GC.elastic_max_heap fail",
                "2097152K exceeds maximum limit 1048576K",
            };
            resizeAndCheck(pid, "2G", contains3, null);

            // epxand to 1G should be fine
            String[] contains4 = {
                "GC.elastic_max_heap success",
                "GC.elastic_max_heap (819200K->1048576K)(1048576K)",
            };
            resizeAndCheck(pid, "1G", contains4, null);

            // shrink to 300M should be fine
            // unaligned arguments should be fine, new_size = 300M -1B
            String[] contains5 = {
                "GC.elastic_max_heap success",
                "GC.elastic_max_heap (1048576K->307200K)(1048576K)",
            };
            if (architecture.equals("aarch64")) {
                contains5 = new String[] {
                    "GC.elastic_max_heap success",
                    "GC.elastic_max_heap (1048576K->327680K)(1048576K)",
                };
            } else if (architecture.equals("loongarch64")) {
                contains5 = new String[] {
                    "GC.elastic_max_heap success",
                    "GC.elastic_max_heap (1048576K->311296K)(1048576K)",
                };
	    }
            resizeAndCheck(pid, "314572799", contains5, null);
        } finally {
            p.destroy();
        }
    }
}
