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
 * @run main/othervm TracePinStack
 * @summary Print stack trace of current virtual thread when it fail to yield.
 */

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.LockSupport;
import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;

public class TracePinStack {
    public static class TestParkWithMonitor {
        public static void main(String[] args) throws Throwable {
            ExecutorService executor = Executors.newSingleThreadExecutor();

            Runnable target = new Runnable() {
                public void run() {
                    synchronized(this) {
                        LockSupport.parkNanos(1000);
                    }
                }
            };

            Thread vt = Thread.ofVirtual().scheduler(executor).name("vt-0").unstarted(target);
            vt.start();
            vt.join();
            executor.shutdown();
        }
    }

    public static void main(String[] args) throws Throwable {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Djdk.tracePinnedThreads=full", "-XX:-YieldWithMonitor", TestParkWithMonitor.class.getName());
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        int exitValue = output.getExitValue();
        System.out.println(output.getOutput());
        if (exitValue != 0) {
            //expecting a zero value
            throw new Error("Expected to get zero exit value");
        }

        output.shouldContain("sun.misc.VirtualThreads.park");
        output.shouldContain("TracePinStack$TestParkWithMonitor$1.run");

        System.out.println("PASSED");
    }
}
