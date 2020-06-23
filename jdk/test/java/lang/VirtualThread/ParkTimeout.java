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
 * @run testng ParkTimeout 
 * @summary test virtual thread park timeout 
 */
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import sun.misc.VirtualThreads;
import static org.testng.Assert.*;

public class ParkTimeout {
    static long finished_vt_count = 0;
    static final long nanosPerSecond = 1000L * 1000L * 1000L;
    public static void main(String args[]) throws Exception {
        SingleVTPark();
        System.out.println("finish SingleVTTimeOut");
    }

    // park two times, first timeout and second is not timeout
    static void SingleVTPark() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("first park");
				long beforePark = System.currentTimeMillis();
				VirtualThreads.park(nanosPerSecond * 1);
				long afterPark = System.currentTimeMillis();
				//System.out.println(afterPark - beforePark);
				assertEquals((afterPark - beforePark) < 100, true);
				VirtualThreads.park(nanosPerSecond * 1);
				long afterSecondPark = System.currentTimeMillis();
                //System.out.println("second park " + (afterSecondPark - afterPark));
				assertEquals((afterSecondPark - afterPark) > 1000, true);
				assertEquals((afterSecondPark - afterPark) < 1500, true);
            }
        };
        VirtualThread vt = new VirtualThread(null, "vt", 0, target);
        vt.start();
		Thread.sleep(10);
		VirtualThreads.unpark(vt);
        vt.join();
    }
}
