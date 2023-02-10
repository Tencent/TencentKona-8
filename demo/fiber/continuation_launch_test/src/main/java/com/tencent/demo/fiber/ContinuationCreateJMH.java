/*
 * Copyright (C) 2021, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

package com.tencent.demo.fiber;

import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.runner.*;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

@BenchmarkMode(Mode.Throughput)
@Warmup(iterations = 2, time = 10, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 5, time = 10, timeUnit = TimeUnit.SECONDS)
@OutputTimeUnit(TimeUnit.SECONDS)
@Fork(1)
@Threads(4)
@State(Scope.Benchmark)

/*
 * Test continuation creat/start/finish performance
 */
public class ContinuationCreateJMH {
    private static ContinuationScope scope = new ContinuationScope("scope");
    @Param({"1", "2", "3", "4", "5"})
    public int iter;
    public static void main() throws Exception {
        Options opt = new OptionsBuilder()
                .include(ContinuationCreateJMH.class.getSimpleName())
                .build();

        new Runner(opt).run();
    }

    @GenerateMicroBenchmark
    public void testCreate() throws Exception {
        Continuation cont = new Continuation(scope, () -> {
        });
        cont.run();
    }

    @Setup
    public void prepare() {
    }

    @TearDown
    public void shutdown() throws Exception {
    }
}
