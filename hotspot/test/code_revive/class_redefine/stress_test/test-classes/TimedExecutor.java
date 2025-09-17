/*
 * Copyright (C) 2023, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
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
 */

public class TimedExecutor {
    private static final String version = "TimedExecutor Version 0";
    public static void main(String args[]) {
        if (args.length < 1) {
            System.out.println("Usage: TimedExecutor run_time_seconds");
            System.exit(-1);
        }
        test(Integer.parseInt(args[0]));
    }

    public static void test(int seconds) {
        int total = 0;
        long beginTime = System.currentTimeMillis();
        while (true) {
            for (int i = 0; i < 10000; i++) {
                total += add1(i);
                total += add2(i);
            }
            long curTime = System.currentTimeMillis();
            if (curTime - beginTime > seconds * 1000L) {
                System.out.println("Running for " + (curTime - beginTime) + "ms, test end.");
                break;
            }
        }
        System.out.println(total);
    }

    private static int add1(int idx) {
        int sum = 0;
        for (int i = 0; i < idx; i++) {
            sum += i;
        }
        return sum;
    }

    private static int add2(int idx) {
        int sum = 0;
        for (int i = 0; i < idx; i++) {
            sum += i;
        }
        return sum;
    }
}
