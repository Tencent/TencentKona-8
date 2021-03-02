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
 * This abstract class represents the core symmetric ciphers. It allows to
 * initialize the cipher and encrypt/decrypt single blocks.
 *
 * @see SM4Crypt
 * @see AlgoCipher
 */
abstract class SymmetricCipher {

    SymmetricCipher() {
        // empty
    }

    /**
     * Retrieves this cipher's block size.
     *
     * @return the block size of this cipher
     */
    abstract int getBlockSize();

    /**
     * Initializes the cipher in the specified mode with the given key.
     *
     * @param decrypting flag indicating encryption or decryption
     * @param algorithm the algorithm name
     * @param key the key
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this cipher
     */
    abstract void init(boolean decrypting, String algorithm, byte[] key)
        throws InvalidKeyException;

    /**
     * Encrypt one cipher block.
     *
     * <p>The input <code>plain</code>, starting at <code>plainOffset</code>
     * and ending at <code>(plainOffset+blockSize-1)</code>, is encrypted.
     * The result is stored in <code>cipher</code>, starting at
     * <code>cipherOffset</code>.
     * @param in the input buffer with the data to be encrypted
     * @param padding the padding mode
     * @param mode the cipher mode
     * @param iv
     */
     abstract byte[] encryptBlock(byte[] in,
                               CipherCore.SM4Padding padding, int mode, byte[] iv);

    /**
     * Decrypt one cipher block.
     *
     * <p>The input <code>cipher</code>, starting at <code>cipherOffset</code>
     * and ending at <code>(cipherOffset+blockSize-1)</code>, is decrypted.
     * The result is stored in <code>plain</code>, starting at
     * <code>plainOffset</code>.
     *
     * @param cipher the input buffer with the data to be decrypted
     * @param cipherOffset the offset in <code>cipher</code>

     */
    abstract byte[] decryptBlock(byte[] cipher, int cipherOffset,
                                 CipherCore.SM4Padding padding, int mode, byte[] iv);


}
