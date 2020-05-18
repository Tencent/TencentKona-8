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
package com.tencent.crypto.provider.Cipher.SM4;

/*
 * @test
 * @summary SM4APITest
 */

import com.tencent.crypto.provider.SMCSProvider;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.IvParameterSpec;
import java.security.Security;
import java.security.spec.KeySpec;

public class SM4APITest {

    Cipher cipher;
    IvParameterSpec params = null;
    SecretKey cipherKey = null;

    public static byte[] key = {
            (byte)0x01,(byte)0x23,(byte)0x45,(byte)0x67,
            (byte)0x89,(byte)0xab,(byte)0xcd,(byte)0xef,
            (byte)0xf0,(byte)0xe1,(byte)0xd2,(byte)0xc3,
            (byte)0xb4,(byte)0xa5,(byte)0x96,(byte)0x87
    };

    public static byte[] iv  = {
            (byte)0xfe,(byte)0xdc,(byte)0xba,(byte)0x98,
            (byte)0x76,(byte)0x54,(byte)0x32,(byte)0x10
    };

    static String[] crypts = {"SM4"};
    static String[] modes = {"ECB", "CTR", "GCM", "CBC"};
    static String[] paddings = {"PKCS7Padding", "NoPadding"};

    public static void main(String[] args) throws Exception {
        SM4APITest test = new SM4APITest();
        test.run();
    }

    public void run() throws Exception {

        for (int i=0; i<crypts.length; i++) {
            for (int j=0; j<modes.length; j++) {
                for (int k=0; k<paddings.length; k++) {
                    System.out.println
                            ("===============================");
                    System.out.println
                            (crypts[i]+" "+modes[j]+" " + paddings[k]);
                    if (modes[j].equalsIgnoreCase("CTR") && paddings[k].equalsIgnoreCase("PKCS7Padding")) {
                        // skip illegal mode and padding.
                        System.out.println("skip: " + modes[i] + "  " + paddings[k]);
                        continue;
                    }
                    init(crypts[i], modes[j], paddings[k]);
                    runTest();
                }
            }
        }
    }

    public void init(String crypt, String mode, String padding)
            throws Exception {

        SMCSProvider smcsProvider = new SMCSProvider();
        Security.addProvider(smcsProvider);

        KeySpec sm4KeySpec = null;
        SecretKeyFactory factory = null;

        StringBuffer cipherName = new StringBuffer(crypt);
        if (mode.length() != 0)
            cipherName.append("/" + mode);
        if (padding.length() != 0)
            cipherName.append("/" + padding);

        cipher = Cipher.getInstance(cipherName.toString());

        cipherKey = KeyGenerator.getInstance("SM4").generateKey();

        // retrieve iv
        if ( !mode.equals("ECB") && !mode.equals("GCM"))
            params = new IvParameterSpec(iv);
        else
            params = null;
    }

    public void runTest() throws Exception {

        int bufferLen = 512;
        byte[] input = new byte[bufferLen];
        int len;

        // encrypt test
        cipher.init(Cipher.ENCRYPT_MODE, cipherKey, params);

        // getIV
        System.out.println("getIV, " + cipher.getIV());
        byte[] output = null;
        boolean thrown = false;
        try {
            input = null;
            output = cipher.update(input, 0, -1);
        } catch (IllegalArgumentException ex) {
            thrown = true;
        }
        if (!thrown) {
            throw new Exception("Expected IAE not thrown!");
        }
        byte[] inbuf = "itaoti7890123456".getBytes();
        System.out.println("inputLength: " + inbuf.length);
        output = cipher.update(inbuf);

        len = cipher.getOutputSize(16);
        byte[] out = new byte[len];
        output = cipher.doFinal();
        System.out.println(len + " " + TestUtility.hexDump(output));
    }
}
