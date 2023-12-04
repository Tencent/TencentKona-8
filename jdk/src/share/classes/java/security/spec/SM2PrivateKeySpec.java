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

package java.security.spec;

import java.math.BigInteger;

/**
 * A SM2 private key with the specified value.
 */
public final class SM2PrivateKeySpec extends ECPrivateKeySpec {

    /**
     * Create a new {@code SM2PrivateKeySpec} with a key value.
     *
     * @param s the private key value.
     */
    public SM2PrivateKeySpec(BigInteger s) {
        super(s, SM2ParameterSpec.instance());
    }

    /**
     * Create a new {@code SM2PrivateKeySpec} with a key in byte array.
     *
     * @param sKey the private key value represented by a byte array.
     */
    public SM2PrivateKeySpec(byte[] sKey) {
        this(new BigInteger(1, sKey));
    }
}
