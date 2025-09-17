/*
 * Copyright (C) 2023, 2024, Tencent. All rights reserved.
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
 * @summary Test EC key pair generation on SM2 curve.
 * @compile ../../Utils.java
 * @run testng ECKeyPairGeneratorTest
 */

import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.Signature;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.SM2KeyAgreementParamSpec;
import java.security.spec.SM2ParameterSpec;
import java.security.spec.SM2SignatureParameterSpec;
import javax.crypto.Cipher;
import javax.crypto.KeyAgreement;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;

public class ECKeyPairGeneratorTest {

    private final static byte[] MESSAGE
            = "SM2 MESSAGE".getBytes(StandardCharsets.UTF_8);

    @Test
    public void testKeyPairGenWithParams() throws Exception {
        testKeyPairGen(SM2ParameterSpec.instance());
    }

    @Test
    public void testKeyPairGenWithName() throws Exception {
        testKeyPairGen(new ECGenParameterSpec("curveSM2"));
    }

    private void testKeyPairGen(AlgorithmParameterSpec spec) throws Exception {
        KeyPairGenerator keyPairGen
                = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(spec);
        KeyPair keyPair = keyPairGen.generateKeyPair();
        ECPublicKey pubKey = (ECPublicKey) keyPair.getPublic();
        ECPrivateKey priKey = (ECPrivateKey) keyPair.getPrivate();

        ECPoint pubPoint = ECOperator.SM2.multiply(
                ECOperator.SM2.getGenerator(), priKey.getS());
        assertEquals(pubKey.getW(), pubPoint);
    }

    @Test
    public void testKeyPairGenKeySize() throws Exception {
        KeyPairGenerator keyPairGen
                = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(256); // should select secp256r1 rather than curveSM2
        KeyPair keyPair = keyPairGen.generateKeyPair();
        ECPublicKey pubKey = (ECPublicKey) keyPair.getPublic();
        ECPrivateKey priKey = (ECPrivateKey) keyPair.getPrivate();

        ECPoint pubPoint = ECOperator.SECP256R1.multiply(
                ECOperator.SECP256R1.getGenerator(), priKey.getS());
        assertEquals(pubKey.getW(), pubPoint);
    }

    @Test
    public void testCipherWithECKeyPair() throws Exception {
        KeyPair keyPair = Utils.sm2KeyPair();
        ECPublicKey pubKey = (ECPublicKey) keyPair.getPublic();
        ECPrivateKey priKey = (ECPrivateKey) keyPair.getPrivate();

        Cipher encrypter = Cipher.getInstance("SM2");
        encrypter.init(Cipher.ENCRYPT_MODE, pubKey);
        byte[] ciphertext = encrypter.doFinal(MESSAGE);

        Cipher decrypter = Cipher.getInstance("SM2");
        decrypter.init(Cipher.DECRYPT_MODE, priKey);
        byte[] cleartext = decrypter.doFinal(ciphertext);

        assertEquals(MESSAGE, cleartext);
    }

    @Test
    public void testSignatureWithECKeyPair() throws Exception {
        KeyPair keyPair = Utils.sm2KeyPair();
        ECPublicKey pubKey = (ECPublicKey) keyPair.getPublic();
        ECPrivateKey priKey = (ECPrivateKey) keyPair.getPrivate();

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(pubKey);

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.setParameter(paramSpec);
        signer.initSign(priKey);
        signer.update(MESSAGE);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.setParameter(paramSpec);
        verifier.initVerify(pubKey);
        verifier.update(MESSAGE);
        boolean verified = verifier.verify(signature);

        assertTrue(verified);
    }

    @Test
    public void testKeyAgreementWithECKeyPair() throws Exception {
        KeyPair keyPairA = Utils.sm2KeyPair();
        KeyPair eKeyPairA = Utils.sm2KeyPair();

        KeyPair keyPairB = Utils.sm2KeyPair();
        KeyPair eKeyPairB = Utils.sm2KeyPair();

        SM2KeyAgreementParamSpec paramSpecA = new SM2KeyAgreementParamSpec(
                (ECPrivateKey) keyPairA.getPrivate(),
                (ECPublicKey) keyPairA.getPublic(),
                (ECPublicKey) keyPairB.getPublic(),
                true,
                16);
        KeyAgreement keyAgreementA = KeyAgreement.getInstance("SM2");
        keyAgreementA.init(eKeyPairA.getPrivate(), paramSpecA);
        keyAgreementA.doPhase(eKeyPairB.getPublic(), true);
        byte[] sharedKeyA = keyAgreementA.generateSecret();

        SM2KeyAgreementParamSpec paramSpecB = new SM2KeyAgreementParamSpec(
                (ECPrivateKey) keyPairB.getPrivate(),
                (ECPublicKey) keyPairB.getPublic(),
                (ECPublicKey) keyPairA.getPublic(),
                false,
                16);
        KeyAgreement keyAgreementB = KeyAgreement.getInstance("SM2");
        keyAgreementB.init(eKeyPairB.getPrivate(), paramSpecB);
        keyAgreementB.doPhase(eKeyPairA.getPublic(), true);
        byte[] sharedKeyB = keyAgreementB.generateSecret();

        assertEquals(sharedKeyA, sharedKeyB);
    }
}
