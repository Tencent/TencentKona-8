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
package com.tencent.crypto.provider.Cipher.SM4;
/*
 * @test
 * @summary KeyWrapping
 */

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;

public class KeyWrapping {

    public static void main(String[] args) throws Exception {
        Cipher c1 = Cipher.getInstance("SM4", "SMCSProvider");
        Cipher c2 = Cipher.getInstance("SM4");

        KeyGenerator keyGen = KeyGenerator.getInstance("SM4");
        keyGen.init(128);

        // Generate two DES keys: sKey and sessionKey
        SecretKey sKey = keyGen.generateKey();
        SecretKey sessionKey = keyGen.generateKey();

        // wrap and unwrap the session key
        // make sure the unwrapped session key
        // can decrypt a message encrypted
        // with the session key
        c1.init(Cipher.WRAP_MODE, sKey);

        byte[] wrappedKey = c1.wrap(sessionKey);

        c1.init(Cipher.UNWRAP_MODE, sKey);

        SecretKey unwrappedSessionKey =
                (SecretKey)c1.unwrap(wrappedKey, "SM4",
                        Cipher.SECRET_KEY);

        c2.init(Cipher.ENCRYPT_MODE, unwrappedSessionKey);

        String msg = "Hello";

        byte[] cipherText = c2.doFinal(msg.getBytes());

        c2.init(Cipher.DECRYPT_MODE, unwrappedSessionKey);

        byte[] clearText = c2.doFinal(cipherText);

        if (!msg.equals(new String(clearText)))
            throw new Exception("The unwrapped session key is corrupted.");
        System.out.println("Test Passed!");
    }
}
