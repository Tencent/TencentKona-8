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

package com.sun.crypto.provider;

import javax.crypto.KeyGeneratorSpi;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidParameterException;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;

public final class HmacSM3KeyGenerator extends KeyGeneratorSpi {

    private int keySize = 32; // The key size in bytes.
    private SecureRandom random;

    @Override
    protected void engineInit(SecureRandom random) {
        this.random = random;
        this.keySize = 32;
    }

    @Override
    protected void engineInit(AlgorithmParameterSpec params, SecureRandom random)
            throws InvalidAlgorithmParameterException {
        throw new InvalidAlgorithmParameterException("No parameter is needed");
    }

    @Override
    protected void engineInit(int keySize, SecureRandom random) {
        if (keySize < 128) {
            throw new InvalidParameterException(
                    "Key size must be 128-bits at least");
        }

        this.keySize = (keySize + 7) >> 3;
        this.random = random;
    }

    @Override
    protected SecretKey engineGenerateKey() {
        if (random == null) {
            random = SunJCE.getRandom();
        }

        byte[] key = new byte[keySize];
        random.nextBytes(key);
        return new SecretKeySpec(key, "HmacSM3");
    }
}
