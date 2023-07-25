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

/*
 * @test
 * @summary Test jmap -heap result
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @compile test_classes/NotActiveHeap.java
 * @run main/othervm JmapHeapTest
 */

import java.lang.reflect.Field;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.JDKToolFinder;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.Asserts;

public class JmapHeapTest extends TestBase {
    public static void main(String[] args) throws Exception {
        test("-XX:+UseParallelGC");
        test("-XX:+UseG1GC");
        test("-XX:+UseConcMarkSweepGC");
    }

    private static void test(String heap_type) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true,
                                                                  heap_type,
                                                                  "-XX:+ElasticMaxHeap",
                                                                  "-Xms100M",
                                                                  "-Xmx600M",
                                                                  "-XX:ElasticMaxHeapSize=2G",
                                                                  "NotActiveHeap");
        Process p = pb.start();
        try {
            long pid = getPidOfProcess(p);
            System.out.println(pid);

            // init heap info
            String[] contains1 = {
                "MaxHeapSize              = 2147483648 (2048.0MB)",
                "CurrentElasticHeapSize   = 629145600 (600.0MB)"
            };
            jmapAndCheck(pid, contains1);

            // shrink to 500M should be fine for any GC
            String[] contains2 = {
                "GC.elastic_max_heap success",
                "GC.elastic_max_heap (614400K->512000K)(2097152K)",
            };
            resizeAndCheck(pid, "500M", contains2, null);
            // check heap info after shrink
            String[] contains3 = {
                "MaxHeapSize              = 2147483648 (2048.0MB)",
                "CurrentElasticHeapSize   = 524288000 (500.0MB)"
            };
            jmapAndCheck(pid, contains3);

            // expand to 1G should be fine for any GC
            String[] contains4 = {
                "GC.elastic_max_heap success",
                "GC.elastic_max_heap (512000K->1048576K)(2097152K)",
            };
            resizeAndCheck(pid, "1G", contains4, null);
            // check heap info after expand
            String[] contains5 = {
                "MaxHeapSize              = 2147483648 (2048.0MB)",
                "CurrentElasticHeapSize   = 1073741824 (1024.0MB)"
            };
            jmapAndCheck(pid, contains5);
        } finally {
            p.destroy();
        }
    }

    static void jmapAndCheck(long pid, String[] contains) throws Exception{
        ProcessBuilder pb = new ProcessBuilder();
        pb.command(new String[] { JDKToolFinder.getJDKTool("jmap"), "-heap", Long.toString(pid)});
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        if (contains != null) {
            for (String s : contains) {
                output.shouldContain(s);
            }
        }
    }
}
