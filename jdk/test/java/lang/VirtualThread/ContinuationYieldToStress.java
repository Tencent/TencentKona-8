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
 * @run testng ContinuationYieldToStress 
 * @summary Continuation yield to stress test 
 */
import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class ContinuationYieldToStress {
    static ContinuationScope scope = new ContinuationScope("scope");
    
    // invoke massive continuaton yieldTo in same thread
    // yieldTo Between two continuations multiple times
    static class MyCont extends Continuation {
        int index;
        MyCont(Runnable r, int i) {
            super(scope, r);
            index = i;
        }
    }
    @Test
    public static void test_yieldToBetweenConts1() {
        final int count = 1000;
        final int yieldCount = 10;
        Continuation[] conts = new Continuation[count];
        Runnable r = () -> {
           MyCont self = (MyCont)Continuation.getCurrentContinuation(scope);
           int yieldToIndex = (self.index + 1) % count;
           System.out.println(self.index + " Enter cont");
           for (int i = 0; i < yieldCount; i++) {
              Continuation.yieldTo(conts[yieldToIndex]);
              //System.out.println(self.index + " Enter resume " + i);
           }
           System.out.println(self.index + " exit cont");
        };
        for (int i = 0; i < count; i++) {
            conts[i] = new MyCont(r, i);
        }
        for (int i = 0; i < count; i++) {
             Continuation.yieldTo(conts[i]);
        }
    }
}
