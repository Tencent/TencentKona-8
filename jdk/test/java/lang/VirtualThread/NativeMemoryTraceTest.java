/*
 * Copyright (C) 2020, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

/**
 * @test
 * @library /lib/testlibrary
 * @run main/othervm -XX:NativeMemoryTracking=detail NativeMemoryTraceTest
 * @summary Print the information of the native memory including Coroutine
 */

import java.io.IOException;
import java.util.concurrent.*;
import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;
import jdk.testlibrary.JDKToolFinder;
import java.lang.reflect.Field;

public class NativeMemoryTraceTest {
    static final CountDownLatch unblock_vt_latch = new CountDownLatch(1);
    static final CountDownLatch unblock_main_thread_latch = new CountDownLatch(1);

    public static void main(String[] args) throws Throwable {
        // start virtual thread
        Runnable r1 = new Runnable() {
            public void run() {
                try {
                    unblock_main_thread_latch.countDown();
                    System.out.println("notify main");
                    unblock_vt_latch.await();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        };
        Thread thread1 = Thread.ofVirtual().name("vt1").start(r1);
        // wait virtual start and block
        unblock_main_thread_latch.await();
        int pid = ProcessTools.getProcessId();
        // Start a thread, using 'jcmd' to observe the summary of nmt
        ProcessBuilder outputPb = new ProcessBuilder();
        outputPb.command(new String[]{JDKToolFinder.getJDKTool("jcmd"), String.valueOf(pid), "VM.native_memory", "summary"});
        OutputAnalyzer output = new OutputAnalyzer(outputPb.start());
	    System.out.println(output.getOutput());
        output.shouldContain("CoroutineStack");
        output.shouldContain("Coroutine");
        unblock_vt_latch.countDown();
        thread1.join();
    }
}
