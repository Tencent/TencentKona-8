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

package sun.security.ec;

import java.util.Map;

/**
 * Defines the entries on ShangMi algorithms.
 */
final class SMEntries {

    static void putEntries(Map<Object, Object> map) {
        map.put("Cipher.SM2", "sun.security.ec.SM2Cipher");
        map.put("KeyFactory.SM2", "sun.security.ec.SM2KeyFactory");
        map.put("Signature.SM3withSM2", "sun.security.ec.SM2Signature");
        map.put("Alg.Alias.Signature.SM2", "SM3withSM2");
        map.put("KeyAgreement.SM2", "sun.security.ec.SM2KeyAgreement");
    }
}
