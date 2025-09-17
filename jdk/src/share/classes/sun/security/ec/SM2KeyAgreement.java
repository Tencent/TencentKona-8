/*
 * Copyright (C) 2022, 2023, Tencent. All rights reserved.
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

import javax.crypto.KeyAgreementSpi;
import javax.crypto.SecretKey;
import javax.crypto.ShortBufferException;
import javax.crypto.spec.SecretKeySpec;
import java.math.BigInteger;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.SM2KeyAgreementParamSpec;

import static java.math.BigInteger.ZERO;
import static java.security.spec.SM2ParameterSpec.*;

import sun.security.ec.point.MutablePoint;
import sun.security.ec.point.Point;
import sun.security.provider.SM3Engine;
import sun.security.util.ArrayUtil;

import static sun.security.ec.ECOperations.*;
import static sun.security.util.SMUtil.*;

/**
 * SM2 key agreement in compliance with GB/T 32918.3-2016.
 */
public final class SM2KeyAgreement extends KeyAgreementSpi {

    private ECPrivateKey ephemeralPrivateKey;
    private SM2KeyAgreementParamSpec paramSpec;
    private ECPublicKey peerEphemeralPublicKey;

    private final SM3Engine sm3 = new SM3Engine();

    @Override
    protected void engineInit(Key key, SecureRandom random) {
        throw new UnsupportedOperationException(
                "Use init(Key, AlgorithmParameterSpec, SecureRandom) instead");
    }

    @Override
    protected void engineInit(Key key, AlgorithmParameterSpec params,
            SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        if (!(key instanceof ECPrivateKey)) {
            throw new InvalidKeyException("Only accept ECPrivateKey");
        }

        if (!(params instanceof SM2KeyAgreementParamSpec)) {
            throw new InvalidAlgorithmParameterException(
                    "Only accept SM2KeyAgreementParamSpec");
        }

        ECPrivateKey ecPrivateKey = (ECPrivateKey) key;
        BigInteger s = ecPrivateKey.getS();
        if (s.compareTo(ZERO) <= 0 || s.compareTo(ORDER) >= 0) {
            throw new InvalidKeyException("The private key must be " +
                    "within the range [1, n - 1]");
        }

        ephemeralPrivateKey = ecPrivateKey;
        paramSpec = (SM2KeyAgreementParamSpec) params;
        peerEphemeralPublicKey = null;
    }

    @Override
    protected Key engineDoPhase(Key key, boolean lastPhase)
            throws InvalidKeyException, IllegalStateException {
        if (ephemeralPrivateKey == null || paramSpec == null) {
            throw new IllegalStateException("Not initialized");
        }

        if (peerEphemeralPublicKey != null) {
            throw new IllegalStateException("Phase already executed");
        }

        if (!lastPhase) {
            throw new IllegalStateException(
                    "Only two party agreement supported, lastPhase must be true");
        }

        if (!(key instanceof ECPublicKey)) {
            throw new InvalidKeyException("Only accept ECPublicKey");
        }

        // Validate public key
        validate((ECPublicKey) key);

        peerEphemeralPublicKey = (ECPublicKey) key;

        return null;
    }

    // Verify that x and y are integers in the interval [0, p - 1].
    private static void validateCoordinate(BigInteger c, BigInteger mod)
        throws InvalidKeyException{
        if (c.compareTo(BigInteger.ZERO) < 0 || c.compareTo(mod) >= 0) {
            throw new InvalidKeyException("Invalid coordinate");
        }
    }

    // Check whether a public key is valid, following the ECC
    // Full Public-key Validation Routine (See section 5.6.2.3.3,
    // NIST SP 800-56A Revision 3).
    private static void validate(ECPublicKey key)
        throws InvalidKeyException {

        // Note: Per the NIST 800-56A specification, it is required
        // to verify that the public key is not the identity element
        // (point of infinity).  However, the point of infinity has no
        // affine coordinates, although the point of infinity could
        // be encoded.  Per IEEE 1363.3-2013 (see section A.6.4.1),
        // the point of infinity is represented by a pair of
        // coordinates (x, y) not on the curve.  For EC prime finite
        // field (q = p^m), the point of infinity is (0, 0) unless
        // b = 0; in which case it is (0, 1).
        //
        // It means that this verification could be covered by the
        // validation that the public key is on the curve.  As will be
        // verified in the following steps.

        // Ensure that integers are in proper range.
        BigInteger x = key.getW().getAffineX();
        BigInteger y = key.getW().getAffineY();

        BigInteger p = SM2OPS.getField().getSize();
        validateCoordinate(x, p);
        validateCoordinate(y, p);

        // Ensure the point is on the curve.
        BigInteger rhs = x.modPow(BigInteger.valueOf(3), p).add(CURVE.getA()
            .multiply(x)).add(CURVE.getB()).mod(p);
        BigInteger lhs = y.modPow(BigInteger.valueOf(2), p);
        if (!rhs.equals(lhs)) {
            throw new InvalidKeyException("Point is not on curve");
        }

        // Check the order of the point.
        //
        // Compute nQ (using elliptic curve arithmetic), and verify that
        // nQ is the identity element.
        byte[] order = ORDER.toByteArray();
        ArrayUtil.reverse(order);
        Point product = SM2OPS.multiply(SM2OPS.toAffPoint(key.getW()), order);
        if (!SM2OPS.isNeutral(product)) {
            throw new InvalidKeyException("Point has incorrect order");
        }
    }

    private static final BigInteger TWO_POW_W = BigInteger.ONE.shiftLeft(w());
    private static final BigInteger TWO_POW_W_SUB_ONE
            = TWO_POW_W.subtract(BigInteger.ONE);

    // w = ceil(ceil(log2(n) / 2) - 1
    private static int w() {
        return (int) Math.ceil((double) ORDER.subtract(
                BigInteger.ONE).bitLength() / 2) - 1;
    }

    @Override
    protected byte[] engineGenerateSecret() throws IllegalStateException {
        if (ephemeralPrivateKey == null || (peerEphemeralPublicKey == null)) {
            throw new IllegalStateException("Not initialized correctly");
        }

        byte[] result;
        try {
            result = deriveKeyImpl();
        } catch (Exception e) {
            throw new IllegalStateException(e);
        }
        peerEphemeralPublicKey = null;
        return result;
    }

    private byte[] deriveKeyImpl() {
        // RA = rA * G = (x1, y1)
        BigInteger rA = ephemeralPrivateKey.getS();
        MutablePoint rAMutablePoint = SM2OPS.multiply(
                SM2OPS.toAffPoint(GENERATOR), toByteArrayLE(rA));
        BigInteger x1 = rAMutablePoint.asAffine().getX().asBigInteger();

        // x1Bar = 2 ^ w + (x1 & (2 ^ w - 1))
        BigInteger x1Bar = TWO_POW_W.add(x1.and(TWO_POW_W_SUB_ONE));

        // tA = (dA + x1Bar * rA) mod n
        BigInteger dA = paramSpec.privateKey().getS();
        BigInteger tA = dA.add(x1Bar.multiply(rA)).mod(ORDER);

        // RB = (x2, y2)
        ECPoint rBPubPoint = peerEphemeralPublicKey.getW();
        BigInteger x2 = rBPubPoint.getAffineX();

        // x2Bar = 2 ^ w + (x2 & (2 ^ w - 1))
        BigInteger x2Bar = TWO_POW_W.add(x2.and(TWO_POW_W_SUB_ONE));

        // U = (h * tA) * (PB + x2Bar * RB)
        ECPoint pBPubPoint = paramSpec.peerPublicKey().getW();
        MutablePoint interimMutablePoint = SM2OPS.multiply(
                SM2OPS.toAffPoint(rBPubPoint), toByteArrayLE(x2Bar));
        SM2OPS.setSum(interimMutablePoint, SM2OPS.toAffPoint(pBPubPoint));
        MutablePoint uPoint = SM2OPS.multiply(
                interimMutablePoint.asAffine(),
                toByteArrayLE(COFACTOR.multiply(tA)));
        if (isInfinitePoint(uPoint)) {
            throw new IllegalStateException("Generate secret failed");
        }

        ECPoint uECPoint = toECPoint(uPoint);
        byte[] vX = bigIntToBytes32(uECPoint.getAffineX());
        byte[] vY = bigIntToBytes32(uECPoint.getAffineY());

        byte[] zA = z(paramSpec.id(), paramSpec.publicKey().getW());
        byte[] zB = z(paramSpec.peerId(), paramSpec.peerPublicKey().getW());

        byte[] combined = combine(vX, vY, zA, zB);
        return kdf(combined);
    }

    @Override
    protected int engineGenerateSecret(byte[] sharedSecret, int offset)
            throws IllegalStateException, ShortBufferException {
        if (offset + paramSpec.sharedKeyLength() > sharedSecret.length) {
            throw new ShortBufferException("Need " + paramSpec.sharedKeyLength()
                    + " bytes, only " + (sharedSecret.length - offset)
                    + " available");
        }

        byte[] secret = engineGenerateSecret();
        System.arraycopy(secret, 0, sharedSecret, offset, secret.length);
        return secret.length;
    }

    @Override
    protected SecretKey engineGenerateSecret(String algorithm)
            throws IllegalStateException, NoSuchAlgorithmException {
        if (algorithm == null) {
            throw new NoSuchAlgorithmException("Algorithm must not be null");
        }

        return new SecretKeySpec(engineGenerateSecret(), algorithm);
    }

    private static final byte[] A = bigIntToBytes32(CURVE.getA());
    private static final byte[] B = bigIntToBytes32(CURVE.getB());
    private static final byte[] GEN_X = bigIntToBytes32(GENERATOR.getAffineX());
    private static final byte[] GEN_Y = bigIntToBytes32(GENERATOR.getAffineY());

    private byte[] z(byte[] id, ECPoint pubPoint) {
        int idLen = id.length << 3;
        sm3.update((byte)(idLen >>> 8));
        sm3.update((byte)idLen);
        sm3.update(id);

        sm3.update(A);
        sm3.update(B);

        sm3.update(GEN_X);
        sm3.update(GEN_Y);

        sm3.update(bigIntToBytes32(pubPoint.getAffineX()));
        sm3.update(bigIntToBytes32(pubPoint.getAffineY()));

        return sm3.doFinal();
    }

    private byte[] kdf(byte[] input) {
        byte[] derivedKey = new byte[paramSpec.sharedKeyLength()];
        byte[] digest = new byte[32];

        int remainder = paramSpec.sharedKeyLength() % 32;
        int count = paramSpec.sharedKeyLength() / 32 + (remainder == 0 ? 0 : 1);
        for (int i = 1; i <= count; i++) {
            sm3.update(input);
            sm3.update(intToBytes4(i));
            sm3.doFinal(digest);

            int length = i == count && remainder != 0 ? remainder : 32;
            System.arraycopy(digest, 0, derivedKey, (i - 1) * 32, length);
        }

        return derivedKey;
    }

    // isInitiator = true,  vX || vY || ZA || ZB
    // isInitiator = false, vX || vY || ZB || ZA
    private byte[] combine(byte[] vX, byte[] vY, byte[] zA, byte[] zB) {
        byte[] result = new byte[vX.length + vY.length + zA.length + zB.length];

        System.arraycopy(vX, 0, result, 0, vX.length);
        System.arraycopy(vY, 0, result, vX.length, vY.length);

        if (paramSpec.isInitiator()) {
            System.arraycopy(zA, 0, result, vX.length + vY.length, zA.length);
            System.arraycopy(zB, 0, result, vX.length + vY.length + zA.length, zB.length);
        } else {
            System.arraycopy(zB, 0, result, vX.length + vY.length, zB.length);
            System.arraycopy(zA, 0, result, vX.length + vY.length + zB.length, zA.length);
        }

        return result;
    }
}
