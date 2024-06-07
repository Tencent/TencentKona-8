/*
 * Copyright (C) 2023, 2024, THL A29 Limited, a Tencent company. All rights reserved.
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

import java.security.Provider;

/**
 * Defines the entries on ShangMi algorithms.
 */
final class SMEntries {

    static void putEntries(Provider p) {
        p.put("Mac.HmacSM3", "com.sun.crypto.provider.HmacSM3");
        p.put("KeyGenerator.HmacSM3", "com.sun.crypto.provider.HmacSM3KeyGenerator");

        p.put("Cipher.SM4/CBC/NoPadding", "com.sun.crypto.provider.SM4Cipher$SM4_CBC_NoPadding");
        p.put("Cipher.SM4/CBC/PKCS7Padding", "com.sun.crypto.provider.SM4Cipher$SM4_CBC_PKCS7Padding");
        p.put("Cipher.SM4/CTR/NoPadding", "com.sun.crypto.provider.SM4Cipher$SM4_CTR_NoPadding");
        p.put("Cipher.SM4/ECB/NoPadding", "com.sun.crypto.provider.SM4Cipher$SM4_ECB_NoPadding");
        p.put("Cipher.SM4/ECB/PKCS7Padding", "com.sun.crypto.provider.SM4Cipher$SM4_ECB_PKCS7Padding");
        p.put("Cipher.SM4/GCM/NoPadding", "com.sun.crypto.provider.SM4Cipher$SM4_GCM_NoPadding");
        p.put("AlgorithmParameters.SM4", "com.sun.crypto.provider.SM4Parameters");
        p.put("KeyGenerator.SM4", "com.sun.crypto.provider.SM4KeyGenerator");
    }
}
