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
 * @run testng ParkTimeout 
 * @summary test virtual thread park timeout 
 */
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import sun.misc.VirtualThreads;
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
        Thread vt = Thread.builder().virtual().name("vt").task(target).build();
        vt.start();
		Thread.sleep(10);
		VirtualThreads.unpark(vt);
        vt.join();
    }
}
