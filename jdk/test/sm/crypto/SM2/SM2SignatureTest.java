/*
 * Copyright (C) 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.security.InvalidKeyException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.Signature;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECPoint;
import java.security.spec.SM2ParameterSpec;
import java.security.spec.SM2PrivateKeySpec;
import java.security.spec.SM2PublicKeySpec;
import java.security.spec.SM2SignatureParameterSpec;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.expectThrows;

/*
 * @test
 * @summary Test SM2 signature.
 * @compile ../../Utils.java
 * @run testng SM2SignatureTest
 */
public class SM2SignatureTest {

    private static final byte[] EMPTY = new byte[0];
    private final static byte[] DEFAULT_ID = "1234567812345678".getBytes();

    private final static String PUB_KEY
            = "041D9E2952A06C913BAD21CCC358905ADB3A8097DB6F2F87EB5F393284EC2B7208C30B4D9834D0120216D6F1A73164FDA11A87B0A053F63D992BFB0E4FC1C5D9AD";
    private final static String PRI_KEY
            = "3B03B35C2F26DBC56F6D33677F1B28AF15E45FE9B594A6426BDCAD4A69FF976B";
    private final static byte[] ID = Utils.hexToBytes("01234567");

    private final static byte[] MESSAGE = Utils.hexToBytes(
            "4003607F75BEEE81A027BB6D265BA1499E71D5D7CD8846396E119161A57E01EEB91BF8C9FE");

    @Test
    public void testParameterSpec() throws Exception {
        KeyPairGenerator keyPairGen
                = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(SM2ParameterSpec.instance());
        KeyPair keyPair = keyPairGen.generateKeyPair();

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec((ECPublicKey) keyPair.getPublic());
        assertEquals(DEFAULT_ID, paramSpec.getId());

        expectThrows(IllegalArgumentException.class,
                ()-> new SM2SignatureParameterSpec(
                        Utils.data(8192), (ECPublicKey) keyPair.getPublic()));
        expectThrows(NullPointerException.class,
                ()-> new SM2SignatureParameterSpec(
                        DEFAULT_ID, null));
    }

    @Test
    public void testSignature() throws Exception {
        testSignature("SM3withSM2");
    }

    @Test
    public void testAlias() throws Exception {
        testSignature("SM2");
    }

    private void testSignature(String name) throws Exception {
        ECPrivateKey priKey = Utils.sm2PrivateKey(PRI_KEY);
        ECPublicKey pubKey = Utils.sm2PublicKey(PUB_KEY);

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(ID, pubKey);

        Signature signer = Signature.getInstance(name);
        signer.setParameter(paramSpec);
        signer.initSign(priKey);

        signer.update(MESSAGE);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance(name);
        verifier.setParameter(paramSpec);
        verifier.initVerify(pubKey);
        verifier.update(MESSAGE);
        boolean verified = verifier.verify(signature);

        assertTrue(verified);
    }

    @Test
    public void testSignatureWithByteBuffer() throws Exception {
        ECPrivateKey priKey = Utils.sm2PrivateKey(PRI_KEY);
        ECPublicKey pubKey = Utils.sm2PublicKey(PUB_KEY);

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(ID, pubKey);

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.setParameter(paramSpec);
        signer.initSign(priKey);

        signer.update(ByteBuffer.wrap(MESSAGE));
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.setParameter(paramSpec);
        verifier.initVerify(pubKey);
        verifier.update(ByteBuffer.wrap(MESSAGE));
        boolean verified = verifier.verify(signature);

        assertTrue(verified);
    }

    @Test
    public void testKeyRange() throws Exception {
        // privateKey = order - 2
        testKeyRange(2);

        // privateKey = order - 1
        // Per the specification, the private key cannot be (order - 1)
        // on generating the signature.
        expectThrows(InvalidKeyException.class, () -> testKeyRange(1));

        // privateKey = order
        expectThrows(IllegalArgumentException.class, () -> testKeyRange(0));

        // privateKey = order + 1
        expectThrows(InvalidKeyException.class, () -> testKeyRange(-1));
    }

    // orderOffset: the relative offset to the order
    private void testKeyRange(int orderOffset) throws Exception {
        KeyFactory keyFactory = KeyFactory.getInstance("SM2");

        BigInteger privateKeyS = ECOperator.SM2.getOrder().subtract(
                BigInteger.valueOf(orderOffset));
        ECPrivateKey privateKey = (ECPrivateKey) keyFactory.generatePrivate(
                new SM2PrivateKeySpec(privateKeyS.toByteArray()));

        ECPoint publicPoint = ECOperator.SM2.multiply(privateKeyS);
        ECPublicKey publicKey = (ECPublicKey) keyFactory.generatePublic(
                new SM2PublicKeySpec(publicPoint));

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.initSign(privateKey);
        signer.update(MESSAGE);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.initVerify(publicKey);
        verifier.update(MESSAGE);
        boolean verified = verifier.verify(signature);

        assertTrue(verified);
    }

    @Test
    public void testEmptyInput() throws Exception {
        ECPrivateKey priKey = Utils.sm2PrivateKey(PRI_KEY);
        ECPublicKey pubKey = Utils.sm2PublicKey(PUB_KEY);

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(ID, pubKey);

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.setParameter(paramSpec);
        signer.initSign(priKey);

        signer.update(EMPTY);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.setParameter(paramSpec);
        verifier.initVerify(pubKey);
        verifier.update(EMPTY);
        boolean verified = verifier.verify(signature);

        assertTrue(verified);
    }

    @Test
    public void testEmptyInputWithByteBuffer() throws Exception {
        ECPrivateKey priKey = Utils.sm2PrivateKey(PRI_KEY);
        ECPublicKey pubKey = Utils.sm2PublicKey(PUB_KEY);

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(ID, pubKey);

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.setParameter(paramSpec);
        signer.initSign(priKey);

        signer.update(ByteBuffer.allocate(0));
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.setParameter(paramSpec);
        verifier.initVerify(pubKey);
        verifier.update(ByteBuffer.allocate(0));
        boolean verified = verifier.verify(signature);

        assertTrue(verified);
    }

    @Test
    public void testNullInput() throws Exception {
        ECPrivateKey priKey = Utils.sm2PrivateKey(PRI_KEY);
        ECPublicKey pubKey = Utils.sm2PublicKey(PUB_KEY);

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(ID, pubKey);

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.setParameter(paramSpec);
        signer.initSign(priKey);
        expectThrows(NullPointerException.class,
                () -> signer.update((byte[]) null));

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.setParameter(paramSpec);
        verifier.initVerify(pubKey);
        expectThrows(NullPointerException.class,
                () -> signer.update((byte[]) null));
    }

    @Test
    public void testNullInputWithByteBuffer() throws Exception {
        ECPrivateKey priKey = Utils.sm2PrivateKey(PRI_KEY);
        ECPublicKey pubKey = Utils.sm2PublicKey(PUB_KEY);

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(ID, pubKey);

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.setParameter(paramSpec);
        signer.initSign(priKey);
        expectThrows(NullPointerException.class,
                () -> signer.update((ByteBuffer) null));

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.setParameter(paramSpec);
        verifier.initVerify(pubKey);
        expectThrows(NullPointerException.class,
                () -> signer.update((ByteBuffer) null));
    }

    @Test
    public void testWithoutParamSpec() throws Exception {
        ECPrivateKey priKey = Utils.sm2PrivateKey(PRI_KEY);
        ECPublicKey pubKey = Utils.sm2PublicKey(PUB_KEY);

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.initSign(priKey);

        signer.update(MESSAGE);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.initVerify(pubKey);
        verifier.update(MESSAGE);
        boolean verified = verifier.verify(signature);

        assertTrue(verified);
    }

    @Test
    public void testTwice() throws Exception {
        ECPrivateKey priKey = Utils.sm2PrivateKey(PRI_KEY);
        ECPublicKey pubKey = Utils.sm2PublicKey(PUB_KEY);

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(ID, pubKey);

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.setParameter(paramSpec);
        signer.initSign(priKey);

        signer.update(MESSAGE, 0, MESSAGE.length / 2);
        signer.update(MESSAGE, MESSAGE.length / 2,
                MESSAGE.length - MESSAGE.length / 2);
        byte[] signature = signer.sign();

        signer.update(MESSAGE);
        signature = signer.sign();

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.setParameter(paramSpec);
        verifier.initVerify(pubKey);

        verifier.update(MESSAGE);
        assertTrue(verifier.verify(signature));

        verifier.update(MESSAGE, 0, MESSAGE.length / 2);
        verifier.update(MESSAGE, MESSAGE.length / 2,
                MESSAGE.length - MESSAGE.length / 2);
        assertTrue(verifier.verify(signature));
    }

    @Test
    public void testWithKeyGen() throws Exception {
        KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(SM2ParameterSpec.instance());
        KeyPair keyPair = keyPairGen.generateKeyPair();

        SM2SignatureParameterSpec paramSpec
                = new SM2SignatureParameterSpec(
                ID, (ECPublicKey) keyPair.getPublic());

        Signature signer = Signature.getInstance("SM3withSM2");
        signer.setParameter(paramSpec);
        signer.initSign(keyPair.getPrivate());

        signer.update(MESSAGE);
        byte[] signature = signer.sign();

        Signature verifier = Signature.getInstance("SM3withSM2");
        verifier.setParameter(paramSpec);
        verifier.initVerify(keyPair.getPublic());
        verifier.update(MESSAGE);
        boolean verified = verifier.verify(signature);

        assertTrue(verified);
    }
}
