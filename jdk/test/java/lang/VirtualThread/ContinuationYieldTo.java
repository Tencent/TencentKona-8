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
 * @run testng ContinuationYieldTo 
 * @summary Basic test for continuation yieldTo api, test start/yieldTo/resume/stop
 */
import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class ContinuationYieldTo {
    static ContinuationScope scope = new ContinuationScope("scope");
    public static int value = 0;
    public static void foo() {
        System.out.println("foo");
        value++;
    }

    // invoke Continuation from thread with yieldTo
    @Test
    public static void test_thread_yieldTo_cont() {
        value = 0;
        Continuation cont = new Continuation(scope, () -> {
           foo();
        });
        Continuation.yieldTo(cont);
        assertEquals(value, 1);
        System.out.println(value);
    }

    // yieldTo with thread
    @Test
    public static void test_yieldToThread() {
        value = 0;
        Continuation cont = new Continuation(scope, () -> {
           System.out.println("enter cont");
           foo();
           Continuation.yield(scope);
           foo();
           System.out.println("exit cont");
        });
        assertEquals(value, 0);
        System.out.println(value);
        Continuation.yieldTo(cont);
        System.out.println(value);
        assertEquals(value, 1);
        value += 2;
        Continuation.yieldTo(cont);
        System.out.println(value);
        assertEquals(value, 4);
    }

    // yieldTo Between two continuations
    @Test
    public static void test_yieldToBetweenConts() {
        value = 0;
              
        Continuation cont1 = new Continuation(scope, () -> {
           System.out.println("enter cont1");
           foo();
           Continuation.yield(scope);
           foo();
           System.out.println("exit cont1");
        });
        Continuation cont2 = new Continuation(scope, () -> {
           System.out.println("enter cont2");
           foo();
           Continuation.yieldTo(cont1);
           foo();
           System.out.println("exit cont2");
        });
        assertEquals(value, 0);
        Continuation.yieldTo(cont1);
        System.out.println(value);
        assertEquals(value, 1);
        Continuation.yieldTo(cont2);
        Continuation.yieldTo(cont2);
    }

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
        Continuation[] conts = new Continuation[2];
        Runnable r = () -> {
           MyCont self = (MyCont)Continuation.getCurrentContinuation(scope);
           int yieldToIndex = (self.index + 1) % 2;
           System.out.println("Enter cont" + self.index);
           foo();
           Continuation.yieldTo(conts[yieldToIndex]);
           System.out.println("Enter resume1-" + self.index);
           foo();
           Continuation.yieldTo(conts[yieldToIndex]);
           System.out.println("Enter resume2-" + self.index);
           foo();
           Continuation.yieldTo(conts[yieldToIndex]);
           System.out.println("Enter resume3-" + self.index);
           System.out.println("exit cont" + self.index);
        };
        MyCont cont0 = new MyCont(r, 0);
        MyCont cont1 = new MyCont(r, 1);
        conts[0] = cont0;
        conts[1] = cont1;
        Continuation.yieldTo(cont0);
        Continuation.yieldTo(cont1);
    }


    // yieldToTest corss Threads
    // yield to continuation previsouly running on another thread
    @Test
    public static void test_yieldToBetweenThreads() {
        Continuation[] conts = new Continuation[2];
        Runnable r = () -> {
           MyCont self = (MyCont)Continuation.getCurrentContinuation(scope);
           int yieldToIndex = (self.index + 1) % 2;
           System.out.println("Enter cont" + self.index + " " + Thread.currentCarrierThread());
           foo();
           Continuation.yield(scope);
           System.out.println("Enter resume1-" + self.index + " " + Thread.currentCarrierThread());
           foo();
           Continuation.yieldTo(conts[yieldToIndex]);
           System.out.println("Enter resume2-" + self.index + " " + Thread.currentCarrierThread());
           foo();
           Continuation.yieldTo(conts[yieldToIndex]);
           System.out.println("Enter resume3-" + self.index + " " + Thread.currentCarrierThread());
           System.out.println("exit cont" + self.index + " " + Thread.currentCarrierThread());
        };
        MyCont cont0 = new MyCont(r, 0);
        MyCont cont1 = new MyCont(r, 1);
        conts[0] = cont0;
        conts[1] = cont1;
        Continuation.yieldTo(cont0);
        Continuation.yieldTo(cont1);
        Thread thread = new Thread(){
            public void run() {
				 Continuation.yieldTo(cont0);
                 Continuation.yieldTo(cont1);
            }
        };
        thread.start();
        try {
            thread.join();
        } catch (Exception e) {
        }
    }
}

