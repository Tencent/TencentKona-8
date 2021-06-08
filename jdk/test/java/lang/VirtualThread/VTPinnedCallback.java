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
 * @run testng VTPinnedCallback
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
