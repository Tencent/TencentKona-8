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

/**
 * This is the internal SM$ class responsible for encryption and
 * decryption of a byte array of size <code>SM4_BLOCK_SIZE</code>.
 *
 * @see SM4Constants
 * @see SM4Cipher
 */

class SM4Crypt extends SymmetricCipher implements SM4Constants {

    /*
     * the encryption key array after expansion and permutation
     */
    byte[] key = null;

    /*
     * Are we encrypting or decrypting?
     */
    boolean decrypting = false;

    static final boolean isKeySizeValid(int len) {
        return len == SM4_KEY_SIZE;
    }

    /**
     * Returns this cipher's block size.
     *
     * @return this cipher's block size
     */
    int getBlockSize() {
        return SM4_BLOCK_SIZE;
    }

    void init(boolean decrypting, String algorithm, byte[] rawKey)
            throws InvalidKeyException {
        this.decrypting = decrypting;
        if (!"SM4".equalsIgnoreCase(algorithm)) {
            throw new InvalidKeyException("Wrong algorithm: expected SM4, actual: " + algorithm);
        }
        if (rawKey.length != SM4_BLOCK_SIZE) {
            throw new InvalidKeyException("Wrong key size");
        }
        key = rawKey;
    }



    /**
     * Performs encryption operation.
     *
     * <p>The input plain text <code>plain</code>, starting at
     * <code>plainOffset</code> and ending at
     * <code>(plainOffset + len - 1)</code>, is encrypted.
     * The result is stored in <code>cipher</code>, starting at
     * <code>cipherOffset</code>.
     *
     * <p>The subclass that implements Cipher should ensure that
     * <code>init</code> has been called before this method is called.
     */
    byte[] encryptBlock(byte[] in, CipherCore.SM4Padding padding, int mode, byte[] iv) {
        byte[] out;
        switch (mode) {
            case CipherCore.ECB_MODE:
                out = EcbEncrypt(in, key, padding);
                break;
            case CipherCore.CBC_MODE:
                out = CbcEncrypt(in,  key, padding, iv);
                break;
            case CipherCore.CTR_MODE:
                out = SMUtils.getInstance().SM4CTREncrypt(in, key, iv);
                break;
            default:
                throw new IllegalStateException("Unexpected value: " + mode);
        }
        return out;
    }

    /**
     * Performs decryption operation.
     *
     * <p>The input cipher text <code>cipher</code>, starting at
     * <code>cipherOffset</code> and ending at
     * <code>(cipherOffset + len - 1)</code>, is decrypted.
     * The result is stored in <code>plain</code>, starting at
     * <code>plainOffset</code>.
     *
     * <p>The subclass that implements Cipher should ensure that
     * <code>init</code> has been called before this method is called.
     *
     * @param cipher the buffer with the input data to be decrypted
     * @param cipherOffset the offset in <code>cipherOffset</code>
     *
     */
    byte[] decryptBlock(byte[] cipher, int cipherOffset,
                      CipherCore.SM4Padding padding, int mode, byte[] iv)
    {
        byte[] out;
        byte[] in;
        if (cipherOffset != 0) {
            int len = cipher.length - cipherOffset;
            in = new byte[len];
            System.arraycopy(cipher, cipherOffset, in, 0, len);
        } else {
            in = cipher;
        }
        switch (mode) {
            case CipherCore.ECB_MODE:
                out = EcbDecrypt(in, key, padding);
                break;
            case CipherCore.CBC_MODE:
                out = CbcDecrypt(in, key, padding, iv);
                break;
            case CipherCore.CTR_MODE:
                out = SMUtils.getInstance().SM4CTRDecrypt(in, key, iv);
                break;
            default:
                throw new IllegalStateException("Unexpected value: " + mode);
        }
        return out;
    }


    private byte[] CbcEncrypt(byte[] in, byte[] key, CipherCore.SM4Padding padding, byte[] iv) {
        byte[] out;
        if (padding == CipherCore.SM4Padding.NoPadding) {
            out = SMUtils.getInstance().SM4CBCEncryptNoPadding(in, key, iv);
        } else if (padding == CipherCore.SM4Padding.PKCS7Padding) {
            out = SMUtils.getInstance().SM4CBCEncrypt(in, key, iv);
        } else {
            throw new IllegalStateException("Unexpected Padding: " + padding);
        }
        return out;
    }

    private byte[] EcbEncrypt(byte[] in, byte[] key, CipherCore.SM4Padding padding) {
        byte[] out;
        if (padding == CipherCore.SM4Padding.NoPadding) {
            out = SMUtils.getInstance().SM4ECBEncryptNoPadding(in, key);
        } else if (padding == CipherCore.SM4Padding.PKCS7Padding) {
            out = SMUtils.getInstance().SM4ECBEncrypt(in, key);
        } else {
            throw new IllegalStateException("Unexpected Padding: " + padding);
        }
        return out;
    }

    // TODO: CTR mode should not have padding!
    private byte[] CtrDecrypt(byte[] in, byte[] key, CipherCore.SM4Padding padding, byte[] iv) {
        byte[] decrypt = SMUtils.getInstance().SM4CTRDecrypt(in, key, iv);
        return decrypt;
    }

    private byte[] CbcDecrypt(byte[] in, byte[] key, CipherCore.SM4Padding padding, byte[] iv) {
        byte[] decrypt;
        if (padding == CipherCore.SM4Padding.NoPadding) {
            decrypt = SMUtils.getInstance().SM4CBCDecryptNoPadding(in, key, iv);
        } else if (padding == CipherCore.SM4Padding.PKCS7Padding) {
            decrypt = SMUtils.getInstance().SM4CBCDecrypt(in, key, iv);
        } else {
            throw new IllegalStateException("Unexpected Padding: " + padding);
        }
        return decrypt;
    }

    private byte[] EcbDecrypt(byte[] in, byte[] key, CipherCore.SM4Padding padding) {
        byte[] decrypt;
        if (padding == CipherCore.SM4Padding.NoPadding) {
            decrypt = SMUtils.getInstance().SM4ECBDecryptNoPadding(in, key);
        } else if (padding == CipherCore.SM4Padding.PKCS7Padding) {
            decrypt = SMUtils.getInstance().SM4ECBDecrypt(in, key);
        } else {
            throw new IllegalStateException("Unexpected Padding: " + padding);
        }
        return decrypt;
    }
}
