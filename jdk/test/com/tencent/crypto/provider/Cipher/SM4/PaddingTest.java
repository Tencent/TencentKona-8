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
 * @summary PaddingTest
 */

import com.tencent.crypto.provider.SM4GCMParameterSpec;
import com.tencent.crypto.provider.SM4KeySpec;

import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.IvParameterSpec;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.InvalidKeyException;
import java.security.spec.KeySpec;
import java.util.Arrays;

public class PaddingTest {

    Cipher cipher;
    IvParameterSpec params = null;
    SecretKey cipherKey = null;
    String pinfile = null;
    String cfile = null;
    String poutfile = null;

    public static byte[] key = {
            (byte)0x01,(byte)0x23,(byte)0x45,(byte)0x67,
            (byte)0x89,(byte)0xab,(byte)0xcd,(byte)0xef,
            (byte)0xf0,(byte)0xe1,(byte)0xd2,(byte)0xc3,
            (byte)0xb4,(byte)0xa5,(byte)0x96,(byte)0x87
    };

    static byte[] iv = {
            (byte)0x03, (byte)0xAD, (byte)0x20, (byte)0x8C,
            (byte)0xB5, (byte)0xC4, (byte)0xA4, (byte)0x39,
            (byte)0xD2, (byte)0xA4, (byte)0xE0, (byte)0x87,
            (byte)0xCD, (byte)0x56, (byte)0x53, (byte)0x20
    };

    static String[] crypts = {"SM4"};
    static String[] modes =  {"ECB", "CBC", "CTR", "GCM"};
    static String[] paddings =  {"PKCS7Padding", "NoPadding"};
    static int numFiles = 11;
    static final String currFile = PaddingTest.class.getResource("PaddingTest.class").getPath();
    static final int lastIdx = currFile.lastIndexOf("PaddingTest.class");
    static final String currDir = currFile.substring(0,lastIdx);
    static String dataDir = currDir + "/inputData/";

    private String padding = null;

    public static void main(String argv[]) throws Exception {
        PaddingTest pt = new PaddingTest();
        pt.run();
    }

    public PaddingTest() {
    }

    public void run() throws Exception {

        for (int l=0; l<numFiles; l++) {
            pinfile = dataDir + "plain" + l + ".txt";
            for (int i=0; i<crypts.length; i++) {
                for (int j=0; j<modes.length; j++) {
                    for (int k=0; k<paddings.length; k++) {
                        System.out.println
                                ("===============================");
                        System.out.println
                                (crypts[i]+" "+modes[j]+" " + paddings[k]+ " " +
                                        "plain" + l + " test");
                        cfile = "c" + l + "_" +
                                crypts[i] + "_" +
                                modes[j] + "_" +
                                paddings[k] + ".bin";
                        poutfile = "p" + l +
                                "_" + crypts[i] + modes[j] + paddings[k] + ".txt";

                        init(crypts[i], modes[j], paddings[k]);
                        padding = paddings[k];
                        int len = 512;
                        // Padding only accept len that is not mod of 16.
                        if (padding.equalsIgnoreCase("PKCS7Padding")) {
                            len = 513;
                        }
                        runTest(len);
                    }
                }
            }
        }
    }

    public void init(String crypt, String mode, String padding)
            throws Exception {

        KeySpec sm4KeySpec = null;
        SecretKeyFactory factory = null;

        StringBuffer cipherName = new StringBuffer(crypt);
        if (mode.length() != 0)
            cipherName.append("/" + mode);
        if (padding.length() != 0)
            cipherName.append("/" + padding);

        cipher = Cipher.getInstance(cipherName.toString());

        sm4KeySpec = new SM4KeySpec(key);
        factory = SecretKeyFactory.getInstance("SM4", "SMCSProvider");


        // retrieve the cipher key
        cipherKey = factory.generateSecret(sm4KeySpec);

        // retrieve iv
        if ((!mode.equals("ECB")) && (!mode.equals("GCM"))) {
            params = new IvParameterSpec(iv);
        }
        else
            params = null;
    }

    public void runTest(int lenth) throws Exception {

        int bufferLen = lenth;
        byte[] input = new byte[bufferLen];
        int len;
        int totalInputLen = 0;

        try {
            try (FileInputStream fin = new FileInputStream(pinfile);
                 BufferedInputStream pin = new BufferedInputStream(fin);
                 FileOutputStream fout = new FileOutputStream(cfile);
                 BufferedOutputStream cout = new BufferedOutputStream(fout)) {
                cipher.init(Cipher.ENCRYPT_MODE, cipherKey, params);

                while ((len = pin.read(input, 0, bufferLen)) > 0) {
                    totalInputLen += len;
                    byte[] output = cipher.update(input, 0, len);
                    cout.write(output, 0, output.length);
                }

                len = cipher.getOutputSize(0);

                byte[] out = new byte[len];
                len = cipher.doFinal(out, 0);
                cout.write(out, 0, len);
            }

            try (FileInputStream fin = new FileInputStream(cfile);
                 BufferedInputStream cin = new BufferedInputStream(fin);
                 FileOutputStream fout = new FileOutputStream(poutfile);
                 BufferedOutputStream pout = new BufferedOutputStream(fout)) {
                if (cipher.getAlgorithm().contains("GCM")) {
                    SM4GCMParameterSpec gmcParams = cipher.getParameters().getParameterSpec(SM4GCMParameterSpec.class);
                    cipher.init(Cipher.DECRYPT_MODE, cipherKey, gmcParams);
                } else {
                    cipher.init(Cipher.DECRYPT_MODE, cipherKey, params);
                }

                byte[] output = null;
                while ((len = cin.read(input, 0, bufferLen)) > 0) {
                    output = cipher.update(input, 0, len);
                    pout.write(output, 0, output.length);
                }

                len = cipher.getOutputSize(0);
                byte[] out = new byte[len];
                cipher.doFinal(out, 0);
                pout.write(out, 0, len);
            }
            diff(pinfile, poutfile);
        } catch (IllegalBlockSizeException ex) {
            if ((totalInputLen % 16 != 0) && (padding.equals("NoPadding"))) {
                System.out.println("Test Passed with expected exception!");
                return;
            } else {
                System.out.println("Test failed!");
                throw ex;
            }
        } catch (InvalidKeyException ex) {
            if ((!padding.equals("NoPadding")) && cipher.getAlgorithm().contains("CTR")) {
                System.out.println("Test Passed with expected exception!");
                return;
            } else {
                System.out.println("Test failed!");
                throw ex;
            }
        }
        System.out.println("Test Passed!");
    }

    private static void diff(String fname1, String fname2) throws Exception {
        if (!Arrays.equals(Files.readAllBytes(Paths.get(fname1)),
                Files.readAllBytes(Paths.get(fname2)))) {
            throw new Exception(
                    "files " + fname1 + " and " + fname2 + " differ");
        }
    }
}
