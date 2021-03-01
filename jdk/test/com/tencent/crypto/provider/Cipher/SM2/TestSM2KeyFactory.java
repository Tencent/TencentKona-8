/*
 *
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

/**
 * @test
 * @summary Test SM2 KeyFactory
 */

import com.tencent.crypto.provider.SM2PrivateKey;
import com.tencent.crypto.provider.SM2PrivateKeySpec;
import com.tencent.crypto.provider.SM2PublicKey;
import com.tencent.crypto.provider.SM2PublicKeySpec;

import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;

public class TestSM2KeyFactory {

    public static void main(String[] args) throws Exception {
        KeyPairGenerator keyPairGenerator = null;
        keyPairGenerator = KeyPairGenerator.getInstance("SM2");
        KeyPair kp = keyPairGenerator.generateKeyPair();
        SM2PublicKey pubKey = (SM2PublicKey) kp.getPublic();
        SM2PrivateKey priKey = (SM2PrivateKey) kp.getPrivate();
        KeyFactory kf = KeyFactory.getInstance("SM2");
        KeySpec publicKeySpec = kf.getKeySpec(pubKey, SM2PublicKeySpec.class);
        if (!(publicKeySpec instanceof SM2PublicKeySpec)) {
            throw new RuntimeException("Failure");
        }
        KeySpec privateKeySpec = kf.getKeySpec(priKey, SM2PrivateKeySpec.class);
        if (!(privateKeySpec instanceof SM2PrivateKeySpec)) {
            throw new RuntimeException("Failure");
        }
    }
}
