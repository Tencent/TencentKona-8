/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
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

import java.io.IOException;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.InvalidParameterException;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.SignatureException;
import java.security.SignatureSpi;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.SM2ParameterSpec;
import java.security.spec.SM2SignatureParameterSpec;

import static java.math.BigInteger.ONE;
import static java.math.BigInteger.ZERO;
import static java.security.spec.SM2ParameterSpec.CURVE;
import static java.security.spec.SM2ParameterSpec.GENERATOR;
import static java.security.spec.SM2ParameterSpec.ORDER;

import sun.security.ec.point.MutablePoint;
import sun.security.jca.JCAUtil;
import sun.security.provider.SM3MessageDigest;
import sun.security.util.ArrayUtil;
import sun.security.util.DerInputStream;
import sun.security.util.DerOutputStream;
import sun.security.util.DerValue;

import static sun.security.ec.ECOperations.*;
import static sun.security.util.SMUtil.*;

/**
 * SM2 signature in compliance with GB/T 32918.2-2016.
 */
public final class SM2Signature extends SignatureSpi {

    // The default ID 1234567812345678
    private static final byte[] DEFAULT_ID = new byte[] {
            49, 50, 51, 52, 53, 54, 55, 56,
            49, 50, 51, 52, 53, 54, 55, 56};

    private BigInteger privS;
    private ECPoint pubPoint;
    private byte[] id;

    private SecureRandom random;

    private byte[] z;

    private final MessageDigest sm3MD = new SM3MessageDigest();

    @Override
    protected void engineInitSign(PrivateKey privateKey, SecureRandom random)
            throws InvalidKeyException {
        privS = null;
        z = null;

        if (!(privateKey instanceof ECPrivateKey)) {
            throw new InvalidKeyException("Only ECPrivateKey accepted");
        }

        ECPrivateKey ecPrivateKey = (ECPrivateKey) privateKey;

        BigInteger s = ecPrivateKey.getS();
        if (s.compareTo(ZERO) <= 0 || s.compareTo(ORDER.subtract(ONE)) >= 0) {
            throw new InvalidKeyException(
                    "The private key must be within the range [1, n - 2]");
        }

        if (pubPoint == null) {
            pubPoint = toECPoint(SM2OPS.multiply(
                    SM2OPS.toAffPoint(GENERATOR),
                    toByteArrayLE(s)));
        } else {
            validatePublicKey(pubPoint, SM2ParameterSpec.instance());
        }

        privS = s;
        this.random = random != null ? random : JCAUtil.getSecureRandom();

        if (id == null) {
            id = DEFAULT_ID;
        }

        resetDigest();
    }

    @Override
    protected void engineInitSign(PrivateKey privateKey)
            throws InvalidKeyException {
        engineInitSign(privateKey, null);
    }

    @Override
    protected void engineInitVerify(PublicKey publicKey)
            throws InvalidKeyException {
        privS = null;
        pubPoint = null;
        z = null;

        if (!(publicKey instanceof ECPublicKey)) {
            throw new InvalidKeyException("Only ECPublicKey accepted");
        }

        ECPublicKey ecPublicKey = (ECPublicKey) publicKey;
        validatePublicKey(ecPublicKey.getW(), SM2ParameterSpec.instance());
        pubPoint = ecPublicKey.getW();

        if (id == null) {
            id = DEFAULT_ID;
        }

        resetDigest();
    }

    @Override
    protected void engineSetParameter(AlgorithmParameterSpec params)
            throws InvalidAlgorithmParameterException {
        if (!(params instanceof SM2SignatureParameterSpec)) {
            throw new InvalidAlgorithmParameterException(
                    "Only accept SM2SignatureParameterSpec");
        }

        SM2SignatureParameterSpec paramSpec = (SM2SignatureParameterSpec) params;
        pubPoint = paramSpec.getPublicKey().getW();
        id = paramSpec.getId();
    }

    @Override
    protected void engineSetParameter(String param, Object value)
            throws InvalidParameterException {
        throw new UnsupportedOperationException(
                "Use SetParameter(AlgorithmParameterSpec params) instead");
    }

    @Override
    protected Object engineGetParameter(String param)
            throws InvalidParameterException {
        throw new UnsupportedOperationException(
                "getParameter(String param) not supported");
    }

    private void resetDigest() {
        sm3MD.reset();
        if (z == null) {
            z = z();
        }
        sm3MD.update(z);
    }

    private byte[] getDigestValue() {
        byte[] digest = sm3MD.digest();
        resetDigest();
        return digest;
    }

    @Override
    protected void engineUpdate(byte b) throws SignatureException {
        sm3MD.update(b);
    }

    @Override
    protected void engineUpdate(byte[] b, int off, int len)
            throws SignatureException {
        sm3MD.update(b, off, len);
    }

    @Override
    protected void engineUpdate(ByteBuffer byteBuffer) {
        int len = byteBuffer.remaining();
        if (len <= 0) {
            return;
        }

        sm3MD.update(byteBuffer);
    }

    @Override
    protected byte[] engineSign() throws SignatureException {
        if (privS == null) {
            throw new SignatureException("Private key not initialized");
        }

        byte[] eHash = getDigestValue();

        // A2
        BigInteger e = new BigInteger(1, eHash);

        BigInteger r;
        BigInteger s;
        do {
            BigInteger k;
            do {
                // A3
                byte[] kArr = nextK();

                // A4
                MutablePoint p = SM2OPS.multiply(SM2OPS.toAffPoint(GENERATOR), kArr);

                // Little-Endian bytes to Big-Endian BigInteger
                ArrayUtil.reverse(kArr);
                k = new BigInteger(1, kArr);

                // A5
                r = e.add(toECPoint(p).getAffineX()).mod(ORDER);
            } while (r.equals(ZERO) || r.add(k).equals(ORDER));

            // A6
            s = privS.add(ONE).modInverse(ORDER)
                    .multiply(k.subtract(r.multiply(privS)).mod(ORDER))
                    .mod(ORDER);
        } while (s.equals(ZERO));

        // A7
        return encodeSignature(r, s);
    }

    private byte[] nextK() {
        return SM2OPS.generatePrivateScalar(random);
    }

    private byte[] encodeSignature(BigInteger r, BigInteger s)
            throws SignatureException {
        try {
            DerOutputStream out = new DerOutputStream();
            out.putInteger(r);
            out.putInteger(s);
            DerValue result = new DerValue(
                    DerValue.tag_Sequence, out.toByteArray());
            return result.toByteArray();
        } catch (Exception e) {
            throw new SignatureException("Could not encode signature", e);
        }
    }

    @Override
    protected boolean engineVerify(byte[] sigBytes) throws SignatureException {
        if (pubPoint == null) {
            throw new SignatureException("Public key not initialized");
        }

        // Partial public key validation
        try {
            validatePublicKey(pubPoint, SM2ParameterSpec.instance());
        } catch (InvalidKeyException e) {
            return false;
        }

        BigInteger[] values = decodeSignature(sigBytes);
        BigInteger r = values[0];
        BigInteger s = values[1];

        // B1
        if (r.compareTo(ONE) < 0 || r.compareTo(ORDER) >= 0) {
            return false;
        }

        // B2
        if (s.compareTo(ONE) < 0 || s.compareTo(ORDER) >= 0) {
            return false;
        }

        // B3
        byte[] eHash = getDigestValue();

        // B4
        BigInteger e = new BigInteger(1, eHash);

        // B5
        BigInteger t = r.add(s).mod(ORDER);
        if (t.equals(ZERO)) {
            return false;
        }

        // B6: p = S'G + tPA
        MutablePoint p = SM2OPS.multiply(SM2OPS.toAffPoint(GENERATOR), toByteArrayLE(s));
        MutablePoint p2 = SM2OPS.multiply(SM2OPS.toAffPoint(pubPoint), toByteArrayLE(t));
        SM2OPS.setSum(p, p2.asAffine());
        if (isInfinitePoint(p)) {
            return false;
        }
        ECPoint point = toECPoint(p);

        // B7
        BigInteger expectedR = e.add(point.getAffineX()).mod(ORDER);
        return expectedR.equals(r);
    }

    private BigInteger[] decodeSignature(byte[] signature)
            throws SignatureException {
        try {
            // Enforce strict DER checking for signatures
            DerInputStream in = new DerInputStream(signature, 0, signature.length, false);
            DerValue[] values = in.getSequence(2);

            // check number of components in the read sequence
            // and trailing data
            if ((values.length != 2) || (in.available() != 0)) {
                throw new IOException("Invalid encoding for signature");
            }

            BigInteger r = values[0].getPositiveBigInteger();
            BigInteger s = values[1].getPositiveBigInteger();

            return new BigInteger[] { r, s };
        } catch (Exception e) {
            throw new SignatureException("Could not decode signature", e);
        }
    }

    private static final byte[] A = bigIntToBytes32(CURVE.getA());
    private static final byte[] B = bigIntToBytes32(CURVE.getB());
    private static final byte[] GEN_X = bigIntToBytes32(GENERATOR.getAffineX());
    private static final byte[] GEN_Y = bigIntToBytes32(GENERATOR.getAffineY());

    private byte[] z() {
        MessageDigest md = new SM3MessageDigest();

        int userIdLen = id.length << 3;
        md.update((byte)(userIdLen >>> 8));
        md.update((byte)userIdLen);
        md.update(id);

        md.update(A);
        md.update(B);

        md.update(GEN_X);
        md.update(GEN_Y);

        md.update(bigIntToBytes32(pubPoint.getAffineX()));
        md.update(bigIntToBytes32(pubPoint.getAffineY()));

        return md.digest();
    }
}
