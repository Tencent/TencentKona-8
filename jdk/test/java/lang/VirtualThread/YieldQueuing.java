/*
 * Copyright (c) 2022, 2023, Oracle and/or its affiliates. All rights reserved.
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
 * @run testng/othervm YieldQueuing
 * @summary Test Thread.yield submits the virtual thread task to the expected queue
 */

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.LockSupport;

import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class YieldQueuing {

    /**
     * Test Thread.yield submits the task for the current virtual thread to a scheduler
     * submission queue when there are no tasks in the local queue.
     */
    @Test
    public void testYieldWithEmptyLocalQueue() throws Exception {
        List<String> list = new CopyOnWriteArrayList<String>();

        AtomicBoolean threadsStarted = new AtomicBoolean();

        Executor executor = Executors.newSingleThreadExecutor();

        Thread threadA = Thread.ofVirtual().scheduler(executor).unstarted(() -> {
            // pin thread until task for B is in submission queue
            while (!threadsStarted.get()) {
                // Just an optimization hint and could be safely
                // removed till we implement it
                // Thread.onSpinWait();
            }

            list.add("A");
            Thread.yield();      // push task for A to submission queue, B should run
            list.add("A");
        });

        Thread threadB = Thread.ofVirtual().scheduler(executor).unstarted(() -> {
            list.add("B");
        });

        // push tasks for A and B to submission queue
        threadA.start();
        threadB.start();

        // release A
        threadsStarted.set(true);

        // wait for result
        threadA.join();
        threadB.join();
        assertEquals(list, Arrays.asList("A", "B", "A"));
    }

    /**
     * Test Thread.yield submits the task for the current virtual thread to the local
     * queue when there are tasks in the local queue.
     */
    @Test
    public void testYieldWithNonEmptyLocalQueue() throws Exception {
        List<String> list = new CopyOnWriteArrayList<String>();

        AtomicBoolean threadsStarted = new AtomicBoolean();

        Executor executor = Executors.newSingleThreadExecutor();

        Thread threadA = Thread.ofVirtual().scheduler(executor).unstarted(() -> {
            // pin thread until tasks for B and C are in submission queue
            while (!threadsStarted.get()) {
                // An optimization hint, not implemented yet
                // Thread.onSpinWait();
            }

            list.add("A");
            LockSupport.park();   // B should run
            list.add("A");
        });

        Thread threadB = Thread.ofVirtual().scheduler(executor).unstarted(() -> {
            list.add("B");
            LockSupport.unpark(threadA);  // push task for A to local queue
            Thread.yield();               // push task for B to local queue, A should run
            list.add("B");
        });

        Thread threadC = Thread.ofVirtual().scheduler(executor).unstarted(() -> {
            list.add("C");
        });

        // push tasks for A, B and C to submission queue
        threadA.start();
        threadB.start();
        threadC.start();

        // release A
        threadsStarted.set(true);

        // wait for result
        threadA.join();
        threadB.join();
        threadC.join();
        assertEquals(list, Arrays.asList("A", "B", "C", "A", "B"));
    }
}
