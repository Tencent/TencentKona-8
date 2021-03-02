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
/**
 * @test
 * @summary Verify that our digests work correctly irrespective of input alignment
 */

import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Random;

public class Offsets {

    private static void outOfBounds(MessageDigest md, int arrayLen, int ofs, int len) throws Exception {
        try {
            md.reset();
            md.update(new byte[arrayLen], ofs, len);
            throw new Exception("invalid call succeeded");
        } catch (RuntimeException e) {
            // ignore
            //System.out.println(e);
        }
    }

    private static void test(String algorithm, int minOfs, int maxOfs, int minLen, int maxLen) throws Exception {
        Random random = new Random();
        MessageDigest md = MessageDigest.getInstance(algorithm, "SMCSProvider");
        System.out.println("Testing " + algorithm + "...");
        outOfBounds(md, 16, 0, 32);
        outOfBounds(md, 16, -8, 16);
        outOfBounds(md, 16, 8, -8);
        outOfBounds(md, 16, Integer.MAX_VALUE, 8);
        for (int n = minLen; n <= maxLen; n++) {
            System.out.print(n + " ");
            byte[] data = new byte[n];
            random.nextBytes(data);
            byte[] digest = null;
            for (int ofs = minOfs; ofs <= maxOfs; ofs++) {
                byte[] ofsData = new byte[n + maxOfs];
                random.nextBytes(ofsData);
                System.arraycopy(data, 0, ofsData, ofs, n);
                md.update(ofsData, ofs, n);
                byte[] ofsDigest = md.digest();
                if (digest == null) {
                    digest = ofsDigest;
                } else {
                    if (Arrays.equals(digest, ofsDigest) == false) {
                        throw new Exception("Digest mismatch " + algorithm + ", ofs: " + ofs + ", len: " + n);
                    }
                }
            }
        }
        System.out.println();
    }

    public static void main(String[] args) throws Exception {
        test("SM3", 0, 64, 0, 128);

    }

}
