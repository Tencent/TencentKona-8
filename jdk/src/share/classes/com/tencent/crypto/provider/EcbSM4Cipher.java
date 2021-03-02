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

import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;

import static com.tencent.crypto.provider.CipherCore.ECB_MODE;

public class EcbSM4Cipher extends AlgoCipher {

    public EcbSM4Cipher(SymmetricCipher impl) {
        super(impl);
    }

    @Override
    String getFeedback() {
        return "SM4ECB";
    }

    @Override
    void save() { }

    @Override
    void restore() { }

    @Override
    void init(boolean decrypting, String algorithm, byte[] key, byte[] iv) throws InvalidKeyException, InvalidAlgorithmParameterException {
        if ((key == null) || (iv != null)) {
            throw new InvalidKeyException("Internal error");
        }
        this.iv = iv;
        embeddedCipher.init(decrypting, algorithm, key);
    }

    @Override
    void reset() { }

    @Override
    byte[] encrypt(byte[] plain, int plainOffset, int plainLen) {
        RangeUtil.nullAndBoundsCheck(plain, plainOffset, plainLen);
        byte[] in;
        byte[] out;
        if (plainOffset == 0 && plainLen == plain.length) {
            in = plain;
        } else {
            int inLen = plainLen;
            in = new byte[inLen];
            System.arraycopy(plain, plainOffset, in, 0, inLen);
        }
        out = embeddedCipher.encryptBlock(in, getPadding(), ECB_MODE, iv);
        return out;
    }

    @Override
    byte[] decrypt(byte[] cipher, int cipherOffset, int cipherLen) {
        RangeUtil.nullAndBoundsCheck(cipher, cipherOffset, cipherLen);
        byte[] out;
        byte[] in;
        int inLen;
        if (cipherOffset == 0 && cipherLen == cipher.length) {
            in = cipher;
            inLen = cipherLen;
        } else {
            inLen = cipherLen;
            in = new byte[inLen];
            System.arraycopy(cipher, cipherOffset, in, 0, inLen);
        }

        out = embeddedCipher.decryptBlock(in,0, padding, ECB_MODE, iv);
        return out;
    }
}
