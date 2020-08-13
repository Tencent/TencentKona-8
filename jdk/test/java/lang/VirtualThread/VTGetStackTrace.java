/*
 * Copyright (c) 2003, Oracle and/or its affiliates. All rights reserved.
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
 * @run testng VTGetStackTrace
 * @summary Test getStackTrace for virtual thread
 */
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import sun.misc.VirtualThreads;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

public class VTGetStackTrace {
    static volatile boolean releaseFlag = false;

    public static void level2_mounted() throws Exception {
        while (true) {
            if (releaseFlag) {
                break;
            }
        }

        assertEquals(getStackTraceLevelNum(Thread.currentThread().getStackTrace()), 2);
    }

    public static void level2_unmounted() throws Exception {
        VirtualThreads.park();
    }

    public static void level1(boolean is_park) throws Exception {
        if (is_park) {
            level2_unmounted();
        } else {
            level2_mounted();
        }
    }

    public static int getStackTraceLevelNum(StackTraceElement[] stackTraceElements) {
        int result = 0;
        for (int i = 0; i < stackTraceElements.length; i++) {
            if ("level1".equals(stackTraceElements[i].getMethodName()) ||
                "level2_mounted".equals(stackTraceElements[i].getMethodName()) ||
                "level2_unmounted".equals(stackTraceElements[i].getMethodName()))
                result++;
        }

        return result;
    }

    @Test
    public static void test() throws Exception {
        GetMountedVT();
        System.out.println("GetMountedVT finished");
        GetUnmountedVT();
        System.out.println("GetUnmountedVT finished");
    }

    public static void GetMountedVT() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                try {
                    level1(false);
                } catch (Exception e) {
                }
            }
        };

        releaseFlag = false;
        ExecutorService executor = Executors.newSingleThreadExecutor();
        ThreadFactory f = Thread.builder().virtual(executor).name("vt_", 0).factory();
        Thread vt = f.newThread(target);

        vt.start();
        Thread.sleep(100);

        assertEquals(getStackTraceLevelNum(vt.getStackTrace()), 2);

        releaseFlag = true;
        vt.join();

        executor.shutdown();
    }

    public static void GetUnmountedVT() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                try {
                    level1(true);
                } catch (Exception e) {
                }
            }
        };

        ExecutorService executor = Executors.newSingleThreadExecutor();
        ThreadFactory f = Thread.builder().virtual(executor).name("vt_", 0).factory();
        Thread vt = f.newThread(target);

        vt.start();
        Thread.sleep(100);

        assertEquals(getStackTraceLevelNum(vt.getStackTrace()), 2);
        VirtualThreads.unpark(vt);
        vt.join();

        executor.shutdown();
    }
}
