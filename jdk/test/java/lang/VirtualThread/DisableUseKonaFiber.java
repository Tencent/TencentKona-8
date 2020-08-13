/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 * @test
 * @library /lib/testlibrary
 * @build DisableUseKonaFiber
 * @summary Use interface of KonaFiber when disable KonaFiber will crash
 *          and show the crash reason to customs.
 * @run main/othervm DisableUseKonaFiber
 */

import java.util.concurrent.*;
import sun.misc.VirtualThreads;
import jdk.testlibrary.OutputAnalyzer;
import java.io.File;
import jdk.testlibrary.ProcessTools;

public class DisableUseKonaFiber {
    public static class DisableUseKonaFiberOP {
        public static void main(String[] args) throws Throwable {
            ExecutorService executor = Executors.newSingleThreadExecutor();
            Runnable target = new Runnable() {
                public void run() {
                    System.out.println("before yield");
                    Thread.yield();
                    System.out.println("resume yield");
                }
            };
            Thread vt = Thread.builder().virtual(executor).name("foo_thread").task(target).build();
            vt.start();
            vt.join();
            executor.shutdown();
        }
    }

    public static void main(String[] args) throws Throwable {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-XX:-UseKonaFiber", DisableUseKonaFiberOP.class.getName());
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        int exitValue = output.getExitValue();
        if (0 == exitValue) {
            //expecting a non zero value
            throw new Error("Expected to get non zero exit value");
        }

        output.shouldContain("UseKonaFiber is off");
        // extract hs-err file
        String hs_err_file = output.firstMatch("# *(\\S*hs_err_pid\\d+\\.log)", 1);
        if (hs_err_file == null) {
            throw new Error("Did not find hs-err file in output.\n");
        }

        /*
         * Check if hs_err files exist or not
         */
        File f = new File(hs_err_file);
        if (!f.exists()) {
            throw new Error("hs-err file missing at "+ f.getAbsolutePath() + ".\n");
        }

        System.out.println("PASSED");
    }
}
