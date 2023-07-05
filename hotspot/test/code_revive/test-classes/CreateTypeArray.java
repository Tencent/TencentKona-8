/*
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
public class CreateTypeArray {
    public static void main(String args[]) {
        int total = 0;
        for (int i = 0; i < 2000; i++) {
            total += test();
        }
        System.out.println(total);
    }

    public static int test() {
        int total = 0;
        for (int i = 0; i < 2000; i ++) {
            boolean[][][] ba = new boolean[1][1][1];
            byte[][][] bba = new byte[1][1][1];
            short[][][] sa = new short[1][1][1];
            char[][][] ca = new char[1][1][1];
            int[][][] ia = new int[1][1][1];
            long[][][] la = new long[1][1][1];
            float[][][] fa = new float[1][1][1];
            double[][][] da = new double[1][1][1];
            total += ba.length + bba.length + sa.length + ca.length + ia.length +
                     la.length + fa.length + da.length;

            boolean[][] ba2 = new boolean[1][1];
            byte[][] bba2 = new byte[1][1];
            short[][] sa2 = new short[1][1];
            char[][] ca2 = new char[1][1];
            int[][] ia2 = new int[1][1];
            long[][] la2 = new long[1][1];
            float[][] fa2 = new float[1][1];
            double[][] da2 = new double[1][1];
            total += ba2.length + bba2.length + sa2.length + ca2.length + ia2.length +
                     la2.length + fa2.length + da2.length;

            boolean[] ba3 = new boolean[1];
            byte[] bba3 = new byte[1];
            short[] sa3 = new short[1];
            char[] ca3 = new char[1];
            int[] ia3 = new int[1];
            long[] la3 = new long[1];
            float[] fa3 = new float[1];
            double[] da3 = new double[1];
            total += ba3.length + bba3.length + sa3.length + ca3.length + ia3.length +
                     la3.length + fa3.length + da3.length;
        }
        return total;
    }
}
