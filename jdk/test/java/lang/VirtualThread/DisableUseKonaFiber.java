/*
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates 
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
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
