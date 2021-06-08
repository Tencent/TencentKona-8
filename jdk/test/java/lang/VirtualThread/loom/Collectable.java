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

/**
 * @test
 * @run testng Collectable
 * @summary Test that virtual threads are GC'ed
 */

import java.lang.ref.WeakReference;
import java.util.concurrent.locks.LockSupport;

import org.testng.annotations.Test;
import static org.testng.Assert.*;

@Test
public class Collectable {

    // ensure that an unstarted virtual thread can be GC"ed
    public void testGC1() {
        Thread thread = Thread.ofVirtual().unstarted(() -> { });
        WeakReference<Thread> ref = new WeakReference<>(thread);
        thread = null;
        waitUntilCleared(ref);
    }

    // ensure that a parked virtual thread can be GC'ed
    // Coroutine is root and VT on stck can not be collected
    // Loom keep objec on heap and no stack roots points to VT
    /*public void testGC2() {
        Thread thread = Thread.startVirtualThread(LockSupport::park);
        WeakReference<Thread> ref = new WeakReference<>(thread);
        thread = null;
        waitUntilCleared(ref);
    }*/

    // ensure that a terminated virtual thread can be GC'ed
    public void testGC3() throws Exception {
        Thread thread = Thread.startVirtualThread(() -> { });
        thread.join();
        WeakReference<Thread> ref = new WeakReference<>(thread);
        thread = null;
        waitUntilCleared(ref);
    }

    private static void waitUntilCleared(WeakReference<?> ref) {
        while (ref.get() != null) {
            System.gc();
            try {
                Thread.sleep(50);
            } catch (InterruptedException ignore) { }
        }
    }
}
