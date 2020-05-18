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

import java.io.ByteArrayOutputStream;
import java.security.InvalidKeyException;

import static com.tencent.crypto.provider.SM4Constants.SM4_BLOCK_SIZE;

public class GcmSM4Cipher extends AlgoCipher {
    // buffer for AAD data; if null, meaning update has been called
    private ByteArrayOutputStream aadBuffer = new ByteArrayOutputStream();

    // buffer for storing input in decryption, not used for encryption
    private ByteArrayOutputStream ibuffer = null;

    // in bytes; need to convert to bits (default value 128) when needed
    public GcmSM4Cipher(SymmetricCipher rawImpl) {
        super(rawImpl);
    }
    private SM4GcmTagData tagData;


    @Override
    String getFeedback() {
        return null;
    }

    public int getTagLen() { return tagData.tagLength; }

    public byte[] getTag() { return tagData.tag; }

    public void setTag(byte[] t) { tagData.tag = t; tagData.tagLength = t.length; }

    public void setTagLen(int l) { tagData.tagLength = l; }


    @Override
    void save() {
        //TODO.
    }

    @Override
    void restore() {
        // TODO.
    }
    
    @Override
    void init(boolean decrypting, String algorithm, byte[] key, byte[] iv) throws InvalidKeyException {
        if ((key == null) || (iv == null)) {
            throw new InvalidKeyException("Internal error");
        }
        this.iv = iv;
        this.decrypting = decrypting;
        if (!algorithm.equalsIgnoreCase("SM4")) {
            throw new InvalidKeyException("Wrong algorithm: SM4 required");
        }
        if (key.length != SM4_BLOCK_SIZE) {
            throw new InvalidKeyException("Wrong key size");
        }
        rawKey = key;
        if (tagData == null) {
            tagData = new SM4GcmTagData();
        }
    }

    @Override
    void reset() {
        if (aadBuffer == null) {
            aadBuffer = new ByteArrayOutputStream();
        } else {
            aadBuffer.reset();
        }
        if (ibuffer != null) {
            ibuffer.reset();
        }
    }

    @Override
    byte[] encrypt(byte[] plain, int plainOffset, int plainLen) {
        RangeUtil.nullAndBoundsCheck(plain, plainOffset, plainLen);

        byte[] in;
        int inLen;
        byte[] out;
        if (plainOffset == 0 && plainLen == plain.length) {
            in = plain;
        } else {
            inLen = plainLen;
            in = new byte[inLen];
            System.arraycopy(plain, plainOffset, in, 0, inLen);
        }

        byte[] aad = aadBuffer.toByteArray();
         out = GcmEncrypt(in, rawKey, getPadding(), iv, aad);

        return out;
    }

    @Override
    byte[] decrypt(byte[] cipher, int cipherOffset, int cipherLen) {
        RangeUtil.nullAndBoundsCheck(cipher, cipherOffset, cipherLen);

        byte[] in;
        int inLen;
        byte[] out;
        int outLen = 0;
        if (cipherOffset == 0 && cipherLen == cipher.length) {
            in = cipher;
        } else {
            inLen = cipherLen;
            in = new byte[inLen];
            System.arraycopy(cipher, cipherOffset, in, 0, inLen);
        }

        byte[] aad = aadBuffer.toByteArray();
        out = GcmDecrypt(in, rawKey, getPadding(), iv, aad);
        return out;
    }


    private byte[] GcmEncrypt(byte[] in, byte[] key, CipherCore.SM4Padding padding, byte[] iv, byte[] aad) {

        // tag is generated
        SM4GcmSecretMessage secretMessage;
        if (padding == CipherCore.SM4Padding.NoPadding) {
            secretMessage = (SM4GcmSecretMessage) SMUtils.getInstance().SM4GCMEncryptNoPadding(in, key, iv, aad);
        } else if (padding == CipherCore.SM4Padding.PKCS7Padding) {
            secretMessage = (SM4GcmSecretMessage) SMUtils.getInstance().SM4GCMEncrypt(in, key, iv, aad);
        } else {
            throw new IllegalStateException("Unexpected Padding: " + padding);
        }
        byte[] secret = secretMessage.secret;
        tagData.tag = secretMessage.tag;
        tagData.tagLength = secretMessage.tagLen;
        return secret;
    }

    private byte[] GcmDecrypt(byte[] in, byte[] key, CipherCore.SM4Padding padding, byte[] iv, byte[] aad) {
        byte[] tag= tagData.tag;
        int taglen = tagData.tagLength;
        byte[] decrypt;
        if (padding == CipherCore.SM4Padding.NoPadding) {
            decrypt = SMUtils.getInstance().SM4GCMDecryptNoPadding(in, tag, taglen, key, iv, aad);
        } else if (padding == CipherCore.SM4Padding.PKCS7Padding) {
            decrypt = SMUtils.getInstance().SM4GCMDecrypt(in, tag, taglen, key, iv, aad);;
        } else {
            throw new IllegalStateException("Unexpected Padding: " + padding);
        }
        return decrypt;
    }

    /**
     * Continues a multi-part update of the Additional Authentication
     * Data (AAD), using a subset of the provided buffer. If this
     * cipher is operating in either GCM or CCM mode, all AAD must be
     * supplied before beginning operations on the ciphertext (via the
     * {@code update} and {@code doFinal} methods).
     * <p>
     * NOTE: Given most modes do not accept AAD, default impl for this
     * method throws IllegalStateException.
     *
     * @param src the buffer containing the AAD
     * @param offset the offset in {@code src} where the AAD input starts
     * @param len the number of AAD bytes
     *
     * @throws IllegalStateException if this cipher is in a wrong state
     * (e.g., has not been initialized), does not accept AAD, or if
     * operating in either GCM or CCM mode and one of the {@code update}
     * methods has already been called for the active
     * encryption/decryption operation
     * @throws UnsupportedOperationException if this method
     * has not been overridden by an implementation
     *
     * @since 1.8
     */
    void updateAAD(byte[] src, int offset, int len) {
        if (aadBuffer != null) {
            aadBuffer.write(src, offset, len);
        } else {
            // update has already been called
            throw new IllegalStateException
                    ("Update has been called; no more AAD data");
        }
    }

    private class SM4GcmTagData {
        int tagLength;
        byte[] tag;
    }

}
