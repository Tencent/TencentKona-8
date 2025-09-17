/*
 * Copyright (C) 2022, 2024, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * @test
 * @summary Test SM2 key factory.
 * @compile ../../Utils.java
 * @run testng SM2KeyFactoryTest
 */

import java.math.BigInteger;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECPoint;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;
import java.security.spec.SM2ParameterSpec;
import java.security.spec.SM2PrivateKeySpec;
import java.security.spec.SM2PublicKeySpec;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.expectThrows;

public class SM2KeyFactoryTest {

    // starts with 0x05
    private final static String INVALID_PUB_KEY_1
            = "051D9E2952A06C913BAD21CCC358905ADB3A8097DB6F2F87EB5F393284EC2B7208C30B4D9834D0120216D6F1A73164FDA11A87B0A053F63D992BFB0E4FC1C5D9AD";

    // 66-bytes
    private final static String INVALID_PUB_KEY_2
            = "04111D9E2952A06C913BAD21CCC358905ADB3A8097DB6F2F87EB5F393284EC2B7208C30B4D9834D0120216D6F1A73164FDA11A87B0A053F63D992BFB0E4FC1C5D9AD";

    // 33-bytes
    private final static String INVALID_PRI_KEY
            = "3B03B35C2F26DBC56F6D33677F1B28AF15E45FE9B594A6426BDCAD4A69FF976B00";

    @Test
    public void testGetKeySpecs() throws Exception {
        KeyPairGenerator keyPairGen
                = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(SM2ParameterSpec.instance());
        KeyPair keyPair = keyPairGen.generateKeyPair();

        KeyFactory keyFactory = KeyFactory.getInstance("SM2");

        KeySpec pubKeySpec = keyFactory.getKeySpec(
                keyPair.getPublic(), SM2PublicKeySpec.class);
        assertNotNull(pubKeySpec);

        KeySpec priKeySpec = keyFactory.getKeySpec(
                keyPair.getPrivate(), SM2PrivateKeySpec.class);
        assertNotNull(priKeySpec);
    }

    @Test
    public void testGetKeySpecsFailed() throws Exception {
        KeyPairGenerator keyPairGen
                = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(SM2ParameterSpec.instance());
        KeyPair keyPair = keyPairGen.generateKeyPair();

        KeyFactory keyFactory = KeyFactory.getInstance("SM2");

        expectThrows(InvalidKeySpecException.class,
                () -> keyFactory.getKeySpec(null, SM2PrivateKeySpec.class));

        expectThrows(InvalidKeySpecException.class,
                () -> keyFactory.getKeySpec(keyPair.getPublic(),
                        SM2PrivateKeySpec.class));

        expectThrows(InvalidKeySpecException.class,
                () -> keyFactory.getKeySpec(keyPair.getPrivate(),
                        SM2PublicKeySpec.class));

        expectThrows(InvalidKeySpecException.class,
                () -> keyFactory.getKeySpec(null, SM2PublicKeySpec.class));
    }

    @Test
    public void testGenerateRawKeys() throws Exception {
        KeyPairGenerator keyPairGen
                = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(SM2ParameterSpec.instance());
        KeyPair keyPair = keyPairGen.generateKeyPair();
        PublicKey publicKey = keyPair.getPublic();
        PrivateKey privateKey = keyPair.getPrivate();

        KeyFactory keyFactory = KeyFactory.getInstance("SM2");

        KeySpec publicKeySpec = keyFactory.getKeySpec(
                publicKey, SM2PublicKeySpec.class);
        ECPublicKey sm2PublicKey = (ECPublicKey) keyFactory.generatePublic(
                publicKeySpec);
        assertEquals(sm2PublicKey.getEncoded().length, 65);

        KeySpec privateKeySpec = keyFactory.getKeySpec(
                privateKey, SM2PrivateKeySpec.class);
        ECPrivateKey sm2PrivateKey = (ECPrivateKey) keyFactory.generatePrivate(
                privateKeySpec);
        assertEquals(sm2PrivateKey.getEncoded().length, 32);
    }

    @Test
    public void testGenerateKeysFailed() throws Exception {
        KeyPairGenerator keyPairGen
                = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(SM2ParameterSpec.instance());
        KeyPair keyPair = keyPairGen.generateKeyPair();
        PublicKey publicKey = keyPair.getPublic();
        PrivateKey privateKey = keyPair.getPrivate();

        KeyFactory keyFactory = KeyFactory.getInstance("SM2");

        KeySpec publicKeySpec = keyFactory.getKeySpec(
                publicKey, SM2PublicKeySpec.class);
        expectThrows(InvalidKeySpecException.class,
                () -> keyFactory.generatePrivate(publicKeySpec));

        KeySpec privateKeySpec = keyFactory.getKeySpec(
                privateKey, SM2PrivateKeySpec.class);
        expectThrows(InvalidKeySpecException.class,
                () -> keyFactory.generatePublic(privateKeySpec));

        expectThrows(IllegalArgumentException.class,
                () -> keyFactory.generatePrivate(
                        new SM2PrivateKeySpec(new BigInteger(INVALID_PRI_KEY, 16))));
        expectThrows(IllegalArgumentException.class,
                () -> keyFactory.generatePrivate(
                        new SM2PrivateKeySpec(Utils.hexToBytes(INVALID_PRI_KEY))));
        expectThrows(NullPointerException.class,
                () -> keyFactory.generatePrivate(
                        new SM2PrivateKeySpec((byte[]) null)));

        expectThrows(IllegalArgumentException.class,
                () -> keyFactory.generatePublic(
                        new SM2PublicKeySpec(Utils.hexToBytes(INVALID_PUB_KEY_1))));
        expectThrows(IllegalArgumentException.class,
                () -> keyFactory.generatePublic(
                        new SM2PublicKeySpec(Utils.hexToBytes(INVALID_PUB_KEY_2))));
        expectThrows(IllegalArgumentException.class,
                () -> keyFactory.generatePublic(
                        new SM2PublicKeySpec(ECPoint.POINT_INFINITY)));
        expectThrows(NullPointerException.class,
                () -> keyFactory.generatePublic(
                        new SM2PublicKeySpec((byte[]) null)));
    }
}
