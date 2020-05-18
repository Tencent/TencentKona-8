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
package com.tencent.crypto.provider;

import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;

public class SM2SignatureParameterSpec implements AlgorithmParameterSpec {
    private static final int DEFAULT_ID_LEN = 128;
    private byte[] id;
    private SM2PublicKey publicKey;

    public SM2SignatureParameterSpec(byte[] id, int offset, int len, PublicKey aPublic) throws IllegalArgumentException, ArrayIndexOutOfBoundsException {
        if (aPublic == null) {
            throw new IllegalArgumentException("must provide publicKey for SM2ParameterSpec");
        }
        if (len < 0) {
            throw new ArrayIndexOutOfBoundsException("len is negative");
        }
        if (id != null) {
            if (offset > id.length) {
                throw new ArrayIndexOutOfBoundsException("offset is larger than length of ID");
            }
            this.id = new byte[len];
            System.arraycopy(id, offset, this.id, 0, len);
        }
        if (id == null) {
            SecureRandom random = SMCSProvider.getRandom();
            this.id = new byte[DEFAULT_ID_LEN];
            random.nextBytes(this.id);
        }
        publicKey = (SM2PublicKey) aPublic;
    }

    public SM2SignatureParameterSpec(byte[] tmpId, PublicKey publicKey) {
        this(tmpId, 0, tmpId.length, publicKey);
    }

    public SM2SignatureParameterSpec(PublicKey publicKey) {
        this(null, 0, 0, publicKey);
    }

    public byte[] getId() {
        return this.id;
    }

    public SM2PublicKey getPublicKey() {
        return publicKey;
    }
}
