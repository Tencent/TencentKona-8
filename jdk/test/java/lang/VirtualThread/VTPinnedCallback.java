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
 * @test
 * @run testng/othervm -XX:-YieldWithMonitor VTPinnedCallback
 * @summary Basic test for add a callback of Continuation when Continuation is pinned.
 */
import java.util.concurrent.*;
import java.util.concurrent.locks.*;
import sun.misc.VirtualThreads;
import org.testng.annotations.Test;
import static org.testng.Assert.*;


public class VTPinnedCallback {
    static long count = 0;

    @Test
    public static void register_pinned_callback() throws Throwable {
        count = 0;

        Object o = new Object();
        Thread vt = Thread.ofVirtual().unstarted(()->{
            synchronized(o) {
                Thread.yield(); //pinned, call PinnedAction
            }
        });

        VirtualThreads.setPinnedAction(vt, new Continuation.PinnedAction() {
            public void run(Continuation.Pinned reason) {
                System.out.println("PinnedAction is called : " + reason);
                count++;
            }
        });

        vt.start();
        vt.join();

        assertEquals(count, 1);
    }
}
