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
 * @summary Sealtest
 */

import com.tencent.crypto.provider.SM2PrivateKey;
import com.tencent.crypto.provider.SMCSProvider;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SealedObject;
import javax.crypto.SecretKey;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.security.*;

public class Sealtest {

    public static void main(String[] args) throws Exception {

        Security.addProvider(new SMCSProvider());

        KeyPairGenerator generator = KeyPairGenerator.getInstance("SM2");
        generator.initialize(2048, new SecureRandom());
        KeyPair kp = generator.generateKeyPair();

        // create SM4 key
        KeyGenerator kg = KeyGenerator.getInstance("SM4");
        SecretKey skey = kg.generateKey();

        // create cipher
        Cipher c = Cipher.getInstance("SM4");
        c.init(Cipher.ENCRYPT_MODE, skey);

        // seal the SM2 private key
        SealedObject sealed = new SealedObject(kp.getPrivate(), c);

        // serialize
        try (FileOutputStream fos = new FileOutputStream("sealed");
             ObjectOutputStream oos = new ObjectOutputStream(fos)) {
            oos.writeObject(sealed);
        }

        // deserialize
        try (FileInputStream fis = new FileInputStream("sealed");
             ObjectInputStream ois = new ObjectInputStream(fis)) {
            sealed = (SealedObject)ois.readObject();
        }

        System.out.println(sealed.getAlgorithm());

        // compare unsealed private key with original
        SM2PrivateKey priv = (SM2PrivateKey)sealed.getObject(skey);

        // SM2PrivateKeys
        if (!priv.equals(kp.getPrivate()))
            throw new Exception("TEST FAILED");

        System.out.println("TEST SUCCEEDED");
    }
}
