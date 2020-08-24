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
 * @run testng SimpleContinuation
 * @summary Basic test for continuation, test create/run/yield/resume/stop
 */

import static org.testng.Assert.*;
import org.testng.annotations.Test;
public class SimpleContinuation {
    static long count = 0;
    static ContinuationScope scope = new ContinuationScope("test");
    @Test
    public static void test() {
        foo();
        System.out.println("finish first");
        assertEquals(count, 2);
        foo();
        System.out.println("finish second");
        assertEquals(count, 4);
    }

    static void foo() {
        Runnable target = new Runnable() {
            public void run() {
                System.out.println("before yield");
                count++;
                Continuation.yield(scope);
                System.out.println("resume yield");
                count++;
            }
        };
        Continuation cont = new Continuation(scope, target);
        cont.run();
        cont.run();
    }
}
