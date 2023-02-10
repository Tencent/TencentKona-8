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
 * @run testng ParkTimeout
 * @run testng/othervm -Djdk.internal.VirtualThread=off ParkTimeout
 * @summary test virtual thread park timeout
 */
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.LockSupport;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
public class ParkTimeout {
    static long finished_vt_count = 0;
    static final long nanosPerSecond = 1000L * 1000L * 1000L;

    // park two times, first timeout and second is not timeout
    @Test
    public static void SingleVTPark() throws Exception {
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("first park");
				long beforePark = System.currentTimeMillis();
				LockSupport.parkNanos(nanosPerSecond * 1);
				long afterPark = System.currentTimeMillis();
				//System.out.println(afterPark - beforePark);
				assertEquals((afterPark - beforePark) < 100, true);
				LockSupport.parkNanos(nanosPerSecond * 1);
				long afterSecondPark = System.currentTimeMillis();
                //System.out.println("second park " + (afterSecondPark - afterPark));
				assertEquals((afterSecondPark - afterPark) > 1000, true);
				assertEquals((afterSecondPark - afterPark) < 1500, true);
            }
        };
        Thread vt = Thread.ofVirtual().name("vt").unstarted(target);
        vt.start();
		Thread.sleep(10);
		LockSupport.unpark(vt);
        vt.join();
    }
}
