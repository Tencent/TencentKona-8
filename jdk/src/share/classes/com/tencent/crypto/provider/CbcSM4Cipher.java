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

import java.security.InvalidKeyException;

import static com.tencent.crypto.provider.CipherCore.CBC_MODE;

public class CbcSM4Cipher extends AlgoCipher {

    public CbcSM4Cipher(SymmetricCipher impl) {
        super(impl);
    }

    @Override
    String getFeedback() {
        return "SM4CBC";
    }

    /**
     * Resets the iv to its original value.
     * This is used when doFinal is called in the Cipher class, so that the
     * cipher can be reused (with its original iv).
     */
    void reset() {
    }

    /**
     * Save the current content of this cipher.
     */
    void save() {
    }

    /**
     * Restores the content of this cipher to the previous saved one.
     */
    void restore() {

    }

    @Override
    void init(boolean decrypting, String algorithm, byte[] key, byte[] iv) throws InvalidKeyException {
        if ((key == null) || (iv == null) || (iv.length != blockSize)) {
            throw new InvalidKeyException("Internal error");
        }
        this.iv = iv;
        reset();
        embeddedCipher.init(decrypting, algorithm, key);
    }

    @Override
    byte[] encrypt(byte[] plain, int plainOffset, int plainLen) {
        RangeUtil.nullAndBoundsCheck(plain, plainOffset, plainLen);

        byte[] in;
        int inLen;
        byte[] out;
        int outLen = 0;
        if (plainOffset == 0 && plainLen == plain.length) {
            in = plain;
            inLen = plainLen;
        } else {
            inLen = plainLen;
            in = new byte[inLen];
            System.arraycopy(plain, plainOffset, in, 0, inLen);
        }
        out = embeddedCipher.encryptBlock(in, getPadding(), CBC_MODE, iv);
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

        out = embeddedCipher.decryptBlock(in,0, padding, CBC_MODE, iv);
        return out;
    }

}
