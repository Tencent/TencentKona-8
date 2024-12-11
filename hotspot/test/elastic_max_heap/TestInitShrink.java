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
 * @test TestInitShrink
 * @key gc
 * @summary test init shrink for ElasticMaxHeapSize with unaligned arguments
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @run main/othervm -Xms50M -Xmx2147483647 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483648 -XX:+UseParallelGC TestInitShrink
 * @run main/othervm -Xms50M -Xmx2147483648 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483649 -XX:+UseParallelGC TestInitShrink
 * @run main/othervm -Xms50M -Xmx2147483647 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483649 -XX:+UseParallelGC TestInitShrink
 * @run main/othervm -Xms200M -Xmx200M -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=400M -XX:+UseParallelGC TestInitShrink
 * @run main/othervm -Xms50M -Xmx2147483647 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483648 -XX:+UseG1GC TestInitShrink
 * @run main/othervm -Xms50M -Xmx2147483648 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483649 -XX:+UseG1GC TestInitShrink
 * @run main/othervm -Xms50M -Xmx2147483647 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483649 -XX:+UseG1GC TestInitShrink
 * @run main/othervm -Xms200M -Xmx200M -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=400M -XX:+UseG1GC TestInitShrink
 * @run main/othervm -Xms50M -Xmx2147483647 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483648 -XX:+UseConcMarkSweepGC TestInitShrink
 * @run main/othervm -Xms50M -Xmx2147483648 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483649 -XX:+UseConcMarkSweepGC TestInitShrink
 * @run main/othervm -Xms50M -Xmx2147483647 -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=2147483649 -XX:+UseConcMarkSweepGC TestInitShrink
 * @run main/othervm -Xms200M -Xmx200M -XX:+ElasticMaxHeap -XX:ElasticMaxHeapSize=400M -XX:+UseConcMarkSweepGC TestInitShrink
 */
public class TestInitShrink {
    public static void main(String[] args) throws Exception {
        int pid = ProcessTools.getProcessId();
        /*
         * given the unaligned Xmx and ElasticMaxHeapSize parameters,
         * get maxMemory as expected
         * test whether jvm can shrink to the initial Xmx at start up
         */
        Runtime r = Runtime.getRuntime();
        // GC rarely happens between these call, should align with size
        long max = r.maxMemory();
        System.out.println("max heap size at start up: " + max);
        Asserts.assertLTE(max, 2147483648L);
    }
}
