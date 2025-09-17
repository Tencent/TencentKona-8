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

package java.security.spec;

import java.security.interfaces.ECPublicKey;
import java.util.Objects;

/**
 * The parameters used by SM2 signature.
 */
public final class SM2SignatureParameterSpec implements AlgorithmParameterSpec {

    // The default ID 1234567812345678
    private static final byte[] DEFAULT_ID = new byte[] {
            49, 50, 51, 52, 53, 54, 55, 56,
            49, 50, 51, 52, 53, 54, 55, 56};

    private final byte[] id;

    private final ECPublicKey publicKey;

    /**
     * Create a new {@code SM2SignatureParameterSpec} with ID and public key.
     *
     * @param id the ID. it must not longer than 8192-bytes.
     * @param publicKey the SM2 public key.
     *
     * @throws NullPointerException if {@code publicKey} is null.
     */
    public SM2SignatureParameterSpec(byte[] id, ECPublicKey publicKey) {
        Objects.requireNonNull(id);
        Objects.requireNonNull(publicKey);

        if (id.length >= 8192) {
            throw new IllegalArgumentException(
                    "The length of ID must be less than 8192-bytes");
        }

        this.id = id.clone();
        this.publicKey = publicKey;
    }

    /**
     * Create a new {@code SM2SignatureParameterSpec} with public key.
     * It just uses the default ID, exactly {@code 1234567812345678}.
     *
     * @param publicKey the SM2 public key.
     *
     * @throws NullPointerException if {@code publicKey} is null.
     */
    public SM2SignatureParameterSpec(ECPublicKey publicKey) {
        this(DEFAULT_ID, publicKey);
    }

    /**
     * Returns the ID.
     *
     * @return the ID.
     */
    public byte[] getId() {
        return id.clone();
    }

    /**
     * Returns the public key.
     *
     * @return the public key.
     */
    public ECPublicKey getPublicKey() {
        return publicKey;
    }
}
