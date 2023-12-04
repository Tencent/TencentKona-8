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

package java.security.spec;

import java.math.BigInteger;
import java.util.Arrays;

/**
 * A SM2 public key with the specified value.
 */
public final class SM2PublicKeySpec extends ECPublicKeySpec {

    /**
     * Create a new {@code SM2PublicKeySpec} with a public point.
     *
     * @param pubPoint the public point.
     */
    public SM2PublicKeySpec(ECPoint pubPoint) {
        super(pubPoint, SM2ParameterSpec.instance());
    }

    /**
     * Create a new {@code SM2PublicKeySpec} with an encoded public point.
     *
     * @param encodedPubPoint the public point encoded in the format {@code 0x04||x||y}.
     *                        {@code x} and {@code y} are the point coordinates.
     */
    public SM2PublicKeySpec(byte[] encodedPubPoint) {
        this(pubPoint(encodedPubPoint));
    }

    // Format: 0x04||x||y, and both of x and y are 32-bytes.
    private static ECPoint pubPoint(byte[] encodedPubKey) {
        if (encodedPubKey.length != 65) {
            throw new IllegalArgumentException(
                    "The encoded public key must be 65-bytes: "
                            + encodedPubKey.length);
        }

        if (encodedPubKey[0] != 0x04) {
            throw new IllegalArgumentException(
                    "The encoded public key must start with 0x04");
        }

        BigInteger x = new BigInteger(1, Arrays.copyOfRange(encodedPubKey, 1, 33));
        BigInteger y = new BigInteger(1, Arrays.copyOfRange(encodedPubKey, 33, 65));
        return new ECPoint(x, y);
    }
}
