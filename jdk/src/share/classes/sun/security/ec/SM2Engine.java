/*
 * Copyright (C) 2022, 2024, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
 * this particular file as subject to the "Classpath" exception as provided
 * in the LICENSE file that accompanied this code.
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

package sun.security.ec;

import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import java.io.IOException;
import java.math.BigInteger;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.security.interfaces.ECKey;
import java.security.spec.ECPoint;
import java.util.Arrays;

import static java.security.spec.SM2ParameterSpec.COFACTOR;
import static java.security.spec.SM2ParameterSpec.GENERATOR;

import sun.security.ec.SM2PrivateKey;
import sun.security.ec.SM2PublicKey;
import sun.security.provider.SM3Engine;
import sun.security.util.DerInputStream;
import sun.security.util.DerOutputStream;
import sun.security.util.DerValue;

import static sun.security.ec.ECOperations.*;
import static sun.security.util.SMUtil.*;

final class SM2Engine {

    private SM2PublicKey publicKey;
    private SM2PrivateKey privateKey;
    private SecureRandom random;

    private boolean encrypted;

    private SM3Engine sm3;

    public void init(boolean encrypted, ECKey key, SecureRandom random) {
        publicKey = null;
        privateKey = null;
        sm3 = null;

        if (encrypted) {
            publicKey = (SM2PublicKey) key;
        } else {
            privateKey = (SM2PrivateKey) key;
        }
        this.random = random;

        this.encrypted = encrypted;

        sm3 = new SM3Engine();
    }

    public boolean encrypted() {
        return encrypted;
    }

    public byte[] processBlock(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException {
        if (!checkInputBound(input, inputOffset, inputLen)) {
            throw new BadPaddingException("Invalid input");
        }

        if (encrypted) {
            return encrypt(input, inputOffset, inputLen);
        } else {
            return decrypt(input, inputOffset, inputLen);
        }
    }

    private byte[] encrypt(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException {
        byte[] c2 = new byte[inputLen]; // c2 is message
        System.arraycopy(input, inputOffset, c2, 0, c2.length);

        ECPoint pC1;
        ECPoint kPB;
        byte[] t;
        do {
            // A1
            byte[] kArr = nextK();

            // A2
            pC1 = toECPoint(SM2OPS.multiply(SM2OPS.toAffPoint(GENERATOR), kArr));

            // A3
            // Check if S = hPB is infinite point
            // It may be unnecessary to check this point, especially in this loop (?)

            // A4
            kPB = toECPoint(SM2OPS.multiply(SM2OPS.toAffPoint(publicKey.getW()), kArr));

            // A5
            t = kdf(kPB, c2.length);
        } while (isAllZero(t));

        // A6
        xor(c2, t); // c2 is encrypted as ciphertext

        // A7
        byte[] c3 = new byte[32];
        sm3.update(bigIntToBytes32(kPB.getAffineX()));
        sm3.update(input, inputOffset, inputLen);
        sm3.update(bigIntToBytes32(kPB.getAffineY()));
        sm3.doFinal(c3);

        // A8
        return c(pC1, c3, c2);
    }

    // C1 || C3 || C2 in ASN.1 DER
    private byte[] c(ECPoint pC1, byte[] c3, byte[] c2)
            throws BadPaddingException {
        DerValue[] values = new DerValue[4];
        values[0] = new DerValue(DerValue.tag_Integer,
                pC1.getAffineX().toByteArray());
        values[1] = new DerValue(DerValue.tag_Integer,
                pC1.getAffineY().toByteArray());
        values[2] = new DerValue(DerValue.tag_OctetString, c3);
        values[3] = new DerValue(DerValue.tag_OctetString, c2);
        DerOutputStream derOut = new DerOutputStream();
        try {
            derOut.putSequence(values);
        } catch (IOException e) {
            throw new BadPaddingException("Encode SM2 ciphertext failed");
        }
        return derOut.toByteArray();
    }

    private static boolean isAllZero(byte[] byteArr) {
        boolean result = byteArr.length > 0;

        for (byte b : byteArr){
            result &= b == 0;
        }

        return result;
    }

    private byte[] decrypt(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException {
        // C1 || C3 || C2 in ASN.1 DER
        byte[] c = new byte[inputLen];
        System.arraycopy(input, inputOffset, c, 0, inputLen);

        DerInputStream derIn;
        DerValue[] values;
        try {
            derIn = new DerInputStream(c);
            values = derIn.getSequence(2);
        } catch (IOException e) {
            throw new BadPaddingException("Decode SM2 ciphertext failed");
        }

        if (values.length != 4 || derIn.available() != 0) {
            throw new BadPaddingException("Invalid encoding for SM2 ciphertext");
        }

        // B1
        byte[] pC1X;
        byte[] pC1Y;
        byte[] c3;
        byte[] c2;

        try {
            pC1X = values[0].getDataBytes();
            pC1Y = values[1].getDataBytes();
            c3 = values[2].getDataBytes();
            c2 = values[3].getDataBytes();
        } catch (IOException e) {
            throw new BadPaddingException("Invalid SM2 ciphertext");
        }

        ECPoint pC1 = new ECPoint(new BigInteger(1, pC1X), new BigInteger(1, pC1Y));

        // B2
        byte[] hArr = toByteArrayLE(COFACTOR);
        ECPoint s = toECPoint(SM2OPS.multiply(SM2OPS.toAffPoint(pC1), hArr));
        if (!SM2OPS.checkOrder(s)) {
            throw new BadPaddingException("The peer public point is invalid");
        }

        // B3
        byte[] dBArr = toByteArrayLE(privateKey.getS());
        ECPoint dBPC1 = toECPoint(SM2OPS.multiply(SM2OPS.toAffPoint(pC1), dBArr));

        // B4
        byte[] t = kdf(dBPC1, c2.length);
        if (isAllZero(t)) {
            throw new BadPaddingException("Derived key is zero");
        }

        // B5
        xor(c2, t); // c2 is decrypted as M

        // B6
        byte[] u = new byte[32];
        sm3.update(bigIntToBytes32(dBPC1.getAffineX()));
        sm3.update(c2);
        sm3.update(bigIntToBytes32(dBPC1.getAffineY()));
        sm3.doFinal(u);

        boolean checkDigest = MessageDigest.isEqual(u, c3);

        Arrays.fill(pC1X, (byte)0);
        Arrays.fill(pC1Y, (byte)0);
        Arrays.fill(c3, (byte)0);

        if (!checkDigest) {
            Arrays.fill(c2, (byte)0);
            throw new BadPaddingException("Invalid ciphertext");
        }

        return c2;
    }

    private void xor(byte[] c2, byte[] t) {
        for (int i = 0; i < c2.length; i++) {
            c2[i] ^= t[i];
        }
    }

    private byte[] nextK() {
        return SM2OPS.generatePrivateScalar(random);
    }

    private byte[] kdf(ECPoint point, int keyLen) {
        byte[] xArr = bigIntToBytes32(point.getAffineX());
        byte[] yArr = bigIntToBytes32(point.getAffineY());

        byte[] input = new byte[xArr.length + yArr.length];
        System.arraycopy(xArr, 0, input, 0, xArr.length);
        System.arraycopy(yArr, 0, input, xArr.length, yArr.length);

        return kdf(input, keyLen);
    }

    private byte[] kdf(byte[] input, int keyLen) {
        byte[] derivedKey = new byte[keyLen];
        byte[] digest = new byte[32];

        int remainder = keyLen % 32;
        int count = (keyLen + 32 - 1) / 32;
        for (int i = 1; i <= count; i++) {
            sm3.update(input);
            sm3.update(intToBytes4(i));
            sm3.doFinal(digest);

            int length = i == count && remainder != 0 ? remainder : 32;
            System.arraycopy(digest, 0, derivedKey, (i - 1) * 32, length);
        }

        return derivedKey;
    }

    private static boolean checkInputBound(byte[] input, int offset, int len) {
        return input != null
                && offset >= 0 && len > 0
                && (input.length >= (offset + len));
    }
}
