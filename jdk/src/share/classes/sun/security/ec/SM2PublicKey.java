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

import java.math.BigInteger;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.SM2ParameterSpec;
import java.util.Arrays;

import static sun.security.util.SMUtil.bigIntToBytes32;

/**
 * The raw EC public key on SM2. The format is {@code 04||x||y}.
 * <p>
 * {@code x} and {@code y} are EC point coordinates, and both of them are 32-bytes.
 */
public final class SM2PublicKey implements ECPublicKey {

    private static final long serialVersionUID = 682873544399078680L;

    private final byte[] encoded;
    private final transient ECPoint pubPoint;

    // 0x04||x||y
    public SM2PublicKey(byte[] encoded) {
        if (encoded == null || encoded.length == 0) {
            throw new IllegalArgumentException("Missing encoded public key");
        }

        pubPoint = decodePubPoint(encoded);
        this.encoded = encoded.clone();
    }

    public SM2PublicKey(ECPoint pubPoint) {
        if (pubPoint == null) {
            throw new IllegalArgumentException("Missing public key");
        }

        if (pubPoint.equals(ECPoint.POINT_INFINITY)) {
            throw new IllegalArgumentException(
                    "Public point cannot be infinite point");
        }

        encoded = encodePubPoint(pubPoint);
        this.pubPoint = pubPoint;
    }

    public SM2PublicKey(ECPublicKey ecPublicKey) {
        this(ecPublicKey.getW());
    }

    @Override
    public String getAlgorithm() {
        return "SM2";
    }

    /**
     * Uncompressed EC point: 0x04||x||y
     */
    @Override
    public String getFormat() {
        return "RAW";
    }

    @Override
    public byte[] getEncoded() {
        return encoded.clone();
    }

    @Override
    public ECPoint getW() {
        return pubPoint;
    }

    @Override
    public ECParameterSpec getParams() {
        return SM2ParameterSpec.instance();
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        SM2PublicKey that = (SM2PublicKey) o;
        return Arrays.equals(encoded, that.encoded);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(encoded);
    }

    // Format: 0x04||x||y
    private static ECPoint decodePubPoint(byte[] encodedPubPoint) {
        if (encodedPubPoint.length != 65) {
            throw new IllegalArgumentException(
                    "The encoded public key must be 65-bytes: "
                            + encodedPubPoint.length);
        }

        if (encodedPubPoint[0] != 0x04) {
            throw new IllegalArgumentException(
                    "The encoded public key must start with 0x04");
        }

        BigInteger x = new BigInteger(1,
                Arrays.copyOfRange(encodedPubPoint, 1, 33));
        BigInteger y = new BigInteger(1,
                Arrays.copyOfRange(encodedPubPoint, 33, 65));
        return new ECPoint(x, y);
    }

    private static byte[] encodePubPoint(ECPoint pubPoint) {
        byte[] x = bigIntToBytes32(pubPoint.getAffineX());
        byte[] y = bigIntToBytes32(pubPoint.getAffineY());

        byte[] encoded = new byte[65];
        encoded[0] = 0x04;
        System.arraycopy(x, 0, encoded, 1, 32);
        System.arraycopy(y, 0, encoded, 33, 32);
        return encoded;
    }
}
