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
package com.tencent.crypto.provider.Cipher.SM3;

/**
 * @test
 * @summary test the clone implementation of SM3 MessageDigest implementation.
 */

import java.security.MessageDigest;
import java.security.Provider;
import java.security.Security;
import java.util.Arrays;

public class TestSM3Clone {

    private static final String[] ALGOS = {
        "SM3"
    };

    private static byte[] input1 = {
        (byte)0x1, (byte)0x2,  (byte)0x3
    };

    private static byte[] input2 = {
        (byte)0x4, (byte)0x5,  (byte)0x6
    };

    private MessageDigest md;

    private TestSM3Clone(String algo, Provider p) throws Exception {
        md = MessageDigest.getInstance(algo, p);
    }

    private void run() throws Exception {
        md.update(input1);
        MessageDigest md2 = (MessageDigest) md.clone();
        md.update(input2);
        md2.update(input2);
        if (!Arrays.equals(md.digest(), md2.digest())) {
            throw new Exception(md.getAlgorithm() + ": comparison failed");
        } else {
            System.out.println(md.getAlgorithm() + ": passed");
        }
    }


    public static void main(String[] argv) throws Exception {
        Provider p = Security.getProvider("SMCSProvider");
        for (int i=0; i<ALGOS.length; i++) {
            TestSM3Clone test = new TestSM3Clone(ALGOS[i], p);
            test.run();
        }
    }
}
