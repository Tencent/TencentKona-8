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
 * This class specifies a SM4 key.
 */
public class SM4KeySpec implements java.security.spec.KeySpec {

    /**
     * The constant which defines the length of a SM4 key in bytes.
     */
    public static final int SM4_KEY_LEN = 16;

    private byte[] key;
    
    /**
     * Creates a SM4KeySpec object using the first 16 bytes in
     * <code>key</code> as the key material for the SM4 key.
     *
     * <p> The bytes that constitute the SM4 key are those between
     * <code>key[0]</code> and <code>key[7]</code> inclusive.
     *
     * @param key the buffer with the SM4 key material. The first 8 bytes
     * of the buffer are copied to protect against subsequent modification.
     *
     * @exception NullPointerException if the given key material is
     * <code>null</code>
     * @exception InvalidKeyException if the given key material is shorter
     * than 8 bytes.
     */
    public SM4KeySpec(byte[] key) throws InvalidKeyException {
        this(key, 0);
    }

    /**
     * Creates a SM4KeySpec object using the first 8 bytes in
     * <code>key</code>, beginning at <code>offset</code> inclusive,
     * as the key material for the SM4 key.
     *
     * <p> The bytes that constitute the SM4 key are those between
     * <code>key[offset]</code> and <code>key[offset+7]</code> inclusive.
     *
     * @param key the buffer with the SM4 key material. The first 8 bytes
     * of the buffer beginning at <code>offset</code> inclusive are copied
     * to protect against subsequent modification.
     * @param offset the offset in <code>key</code>, where the SM4 key
     * material starts.
     *
     * @exception NullPointerException if the given key material is
     * <code>null</code>
     * @exception InvalidKeyException if the given key material, starting at
     * <code>offset</code> inclusive, is shorter than 8 bytes.
     */
    public SM4KeySpec(byte[] key, int offset) throws InvalidKeyException {
        if (key.length - offset < SM4_KEY_LEN) {
            throw new InvalidKeyException("Wrong key size");
        }
        this.key = new byte[SM4_KEY_LEN];
        System.arraycopy(key, offset, this.key, 0, SM4_KEY_LEN);
    }

    /**
     * Returns the SM4 key material.
     *
     * @return the SM4 key material. Returns a new array
     * each time this method is called.
     */
    public byte[] getKey() {
        return this.key.clone();
    }

    /**
     * Checks if the given SM4 key material, starting at <code>offset</code>
     * inclusive, is parity-adjusted.
     *
     * @param key the buffer with the SM4 key material.
     * @param offset the offset in <code>key</code>, where the SM4 key
     * material starts.
     *
     * @return true if the given SM4 key material is parity-adjusted, false
     * otherwise.
     *
     * @exception InvalidKeyException if the given key material is
     * <code>null</code>, or starting at <code>offset</code> inclusive, is
     * shorter than 8 bytes.
     */
    public static boolean isParityAdjusted(byte[] key, int offset)
            throws InvalidKeyException {
        if (key == null) {
            throw new InvalidKeyException("null key");
        }
        if (key.length - offset < SM4_KEY_LEN) {
            throw new InvalidKeyException("Wrong key size");
        }

        for (int i = 0; i < SM4_KEY_LEN; i++) {
            int k = Integer.bitCount(key[offset++] & 0xff);
            if ((k & 1) == 0) {
                return false;
            }
        }

        return true;
    }
}
