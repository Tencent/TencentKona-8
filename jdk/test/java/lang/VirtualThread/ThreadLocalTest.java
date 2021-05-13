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
 * @run testng ThreadLocalTest
 * @summary Basic test thread local access
 */
import org.testng.annotations.Test;
import static org.testng.Assert.*;
import java.util.concurrent.Executor;
import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;

public class ThreadLocalTest {
    static ThreadLocal<String> tl = new ThreadLocal<>();

    @Test
    public static void test() {
        final JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
        tl.set("main");

        Executor e = new Executor() {
            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };

        Thread vt = Thread.builder().virtual(e).task(()->{
            System.out.println(tl.get());
            assertEquals(tl.get(), null);
            System.out.println("vt1");
            tl.set("virtual");
            System.out.println(tl.get());
            assertEquals(tl.get().equals("virtual"), true);
        }).build();
        vt.start();
        System.out.println("carrier thread " + tl.get());
        assertEquals(tl.get().equals("main"), true);
        vt = Thread.builder().virtual(e).task(()->{
            System.out.println(jla.getCarrierThreadLocal(tl));
            assertEquals(jla.getCarrierThreadLocal(tl).equals("main"), true);
            System.out.println("vt2");
            jla.setCarrierThreadLocal(tl, "virtual");
            System.out.println(jla.getCarrierThreadLocal(tl));
        }).build();
        vt.start();
        System.out.println("carrier thread " + tl.get());
        assertEquals(jla.getCarrierThreadLocal(tl).equals("virtual"), true);
    }
}

