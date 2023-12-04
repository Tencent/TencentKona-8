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

package com.sun.crypto.provider;

import java.security.InvalidKeyException;

final class SM4Crypt extends SymmetricCipher {

    private SM4Engine engine;

    @Override
    int getBlockSize() {
        return 16;
    }

    @Override
    void init(boolean decrypting, String algorithm, byte[] key)
            throws InvalidKeyException {
        if (!algorithm.equalsIgnoreCase("SM4")) {
            throw new InvalidKeyException("The algorithm must be SM4");
        }

        engine = new SM4Engine(key, !decrypting);
    }

    @Override
    void encryptBlock(byte[] plain, int plainOffset,
                      byte[] cipher, int cipherOffset) {
        engine.processBlock(plain, plainOffset, cipher, cipherOffset);
    }

    @Override
    void decryptBlock(byte[] cipher, int cipherOffset,
                      byte[] plain, int plainOffset) {
        engine.processBlock(cipher, cipherOffset, plain, plainOffset);
    }
}
