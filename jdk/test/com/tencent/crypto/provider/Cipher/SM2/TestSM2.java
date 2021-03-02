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
 * @summary Test SM2 Cipher implementation
 */

import com.tencent.crypto.provider.SM2PrivateKeySpec;
import com.tencent.crypto.provider.SM2PublicKeySpec;

import javax.crypto.Cipher;
import java.security.*;
import java.util.Arrays;
import java.util.Random;

public class TestSM2 {

    private final static char[] hexDigits = "0123456789abcdef".toCharArray();
    private final static String pubKeyString = "041D9E2952A06C913BAD21CCC358905ADB3A8097DB6F2F87EB5F393284EC2B7208C30B4D9834D0120216D6F1A73164FDA11A87B0A053F63D992BFB0E4FC1C5D9AD";
    private final static String priKeyString = "3B03B35C2F26DBC56F6D33677F1B28AF15E45FE9B594A6426BDCAD4A69FF976B";

    public static String toString(byte[] b) {
        if (b == null) {
            return "(null)";
        }
        StringBuffer sb = new StringBuffer(b.length * 3);
        for (int i = 0; i < b.length; i++) {
            int k = b[i] & 0xff;
            if (i != 0) {
                sb.append(':');
            }
            sb.append(hexDigits[k >>> 4]);
            sb.append(hexDigits[k & 0xf]);
        }
        return sb.toString();
    }

    private final static Random RANDOM = new Random();

    private static Provider p;

    private static void testEncDec(String alg, int len, Key encKey, Key decKey) throws Exception {
        System.out.println("Testing en/decryption using " + alg + "...");
        Cipher c = Cipher.getInstance(alg, p);

        byte[] b = new byte[len];
        RANDOM.nextBytes(b);
        b[0] &= 0x3f;
        b[0] |= 1;

        c.init(Cipher.ENCRYPT_MODE, encKey);
        byte[] enc = c.doFinal(b);

        c.init(Cipher.DECRYPT_MODE, decKey);
        byte[] dec = c.doFinal(enc);

        if (Arrays.equals(b, dec) == false) {
            System.out.println("in:  " + toString(b));
            System.out.println("dec: " + toString(dec));
            throw new RuntimeException("Failure");
        }
    }


    public static void main(String[] args) throws Exception {
        long start = System.currentTimeMillis();


        p = Security.getProvider("SMCSProvider");
        System.out.println("Testing provider " + p.getName() + "...");
        KeyFactory kf = KeyFactory.getInstance("SM2", p);
        SM2PublicKeySpec publicKeySpec = new SM2PublicKeySpec(pubKeyString);
        PublicKey publicKey = kf.generatePublic(publicKeySpec);
        SM2PrivateKeySpec privateKeySpec = new SM2PrivateKeySpec(priKeyString);
        PrivateKey privateKey = kf.generatePrivate(privateKeySpec);

        testEncDec("SM2", 96, publicKey, privateKey);
        testEncDec("SM2/C1C3C2_ASN1/NoPadding", 96, publicKey, privateKey);
        testEncDec("SM2/C1C3C2/NoPadding", 96, publicKey, privateKey);
        testEncDec("SM2/C1C2C3_ASN1/NoPadding", 96, publicKey, privateKey);
        testEncDec("SM2/C1C2C3/NoPadding", 96, publicKey, privateKey);

        long stop = System.currentTimeMillis();
        System.out.println("Done (" + (stop - start) + " ms).");
    }

}
