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
 * @test OptionsCheck
 * @key gc
 * @summary test invalid options combinations with elastic max heap
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @run main/othervm OptionsCheck
 */
public class OptionsCheck extends TestBase {
    public static void main(String[] args) throws Exception {
        String[] key_output = {
            "can not be combined with",
        };
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-Xmn200M", "-version");
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-XX:MaxNewSize=300M", "-version");
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-XX:OldSize=1G", "-version");
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-XX:+UseAdaptiveGCBoundary", "-version");
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-XX:-UseAdaptiveSizePolicy", "-version");
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-XX:+UseParNewGC", "-version");
        String[] contains1 = {
            "-XX:ElasticMaxHeapSize should be used together with -XX:+ElasticMaxHeap"
        };
        LaunchAndCheck(contains1, null, "-XX:ElasticMaxHeapSize=100M", "-version");
        String[] contains2 = {
            "-XX:ElasticMaxHeapSize should be used together with -Xmx/-XX:MaxHeapSize"
        };
        LaunchAndCheck(contains2, null, "-XX:+ElasticMaxHeap", "-XX:ElasticMaxHeapSize=100M", "-version");
        String[] contains3 = {
            "ElasticMaxHeapSize should be larger than MaxHeapSize"
        };
        LaunchAndCheck(contains3, null, "-XX:+ElasticMaxHeap", "-XX:ElasticMaxHeapSize=1G", "-Xmx2G", "-version");
    }

    public static void LaunchAndCheck(String[] contains, String[] not_contains, String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        if (contains != null) {
            for (String s : contains) {
                output.shouldContain(s);
            }
        }
        if (not_contains != null) {
            for (String s : contains) {
                output.shouldNotContain(s);
            }
        }
    }
}
