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

/*
 *  @test
 *  @library /lib /
 *  @build sun.hotspot.WhiteBox
 *  @build ClassFileInstaller
 *  @run driver ClassFileInstaller sun.hotspot.WhiteBox
 *                                 sun.hotspot.WhiteBox$WhiteBoxPermission
 *  @run testng/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+PrintCompilation -XX:+VerifyCoroutineStateOnYield VTDeoptimizeMonitor
 *  @summary Testing move monitor into _monitor_chunk of thread
 */
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import sun.misc.VirtualThreads;
import sun.hotspot.WhiteBox;
import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class VTDeoptimizeMonitor {
    public static void entrance() {
        int b = 0;
        for (int a = 0; a < 5; a++) {
            b++;
        }
    }

    @Test
    public static void DeoptimizeAndMonitor() throws Exception {
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Runnable target = new Runnable() {
            public void run() {
                WhiteBox WB = WhiteBox.getWhiteBox();
                Object lock = new Object();
                synchronized (lock) {
                    for (int i = 0; i < 100000; i++ ) {
                        entrance();
                        if ((i % 50000) == 0) {
                            System.out.println("hello deoptimized");
                            WB.deoptimizeAll();
                        }
                    }
                }
            }
        };
        Thread vt = Thread.ofVirtual().scheduler(executor).name("test_thread").unstarted(target);
        vt.start();
        vt.join();

        executor.shutdown();
    }
}
