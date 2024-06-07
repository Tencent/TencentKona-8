/*
 * Copyright (C) 2022, 2024, THL A29 Limited, a Tencent company. All rights reserved.
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

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.CipherSpi;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.SecretKey;
import javax.crypto.ShortBufferException;
import javax.crypto.spec.SecretKeySpec;
import java.io.ByteArrayOutputStream;
import java.math.BigInteger;
import java.security.AlgorithmParameters;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.KeyFactory;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import java.util.Arrays;

import static java.math.BigInteger.ZERO;
import static java.security.spec.SM2ParameterSpec.ORDER;

import sun.security.jca.JCAUtil;

/**
 * SM2 cipher in compliance with GB/T 32918.4-2016.
 */
public final class SM2Cipher extends CipherSpi {

    private static final byte[] B0 = new byte[0];

    private final SM2Engine engine = new SM2Engine();
    private final Buffer buffer = new Buffer();

    @Override
    public void engineSetMode(String mode) throws NoSuchAlgorithmException {
        if (!mode.equalsIgnoreCase("none")) {
            throw new NoSuchAlgorithmException("Mode must be none");
        }
    }

    @Override
    public void engineSetPadding(String paddingName)
            throws NoSuchPaddingException {
        if (!paddingName.equalsIgnoreCase("NoPadding")) {
            throw new NoSuchPaddingException("Padding must be NoPadding");
        }
    }

    @Override
    public void engineInit(int opmode, Key key, SecureRandom random)
            throws InvalidKeyException {
        buffer.reset();

        SecureRandom rand = random != null ? random : JCAUtil.getSecureRandom();;

        if (opmode == Cipher.ENCRYPT_MODE || opmode == Cipher.WRAP_MODE) {
            if (key instanceof ECPublicKey) {
                SM2PublicKey publicKey = new SM2PublicKey((ECPublicKey) key);
                engine.init(true, publicKey, rand);
            } else {
                throw new InvalidKeyException(
                        "Only accept ECPublicKey for encryption");
            }
        } else if (opmode == Cipher.DECRYPT_MODE || opmode == Cipher.UNWRAP_MODE) {
            if (key instanceof ECPrivateKey) {
                SM2PrivateKey privateKey = new SM2PrivateKey((ECPrivateKey) key);

                BigInteger s = privateKey.getS();
                if (s.compareTo(ZERO) <= 0 || s.compareTo(ORDER) >= 0) {
                    throw new InvalidKeyException(
                            "The private key must be within the range [1, n - 1]");
                }

                engine.init(false, privateKey, rand);
            } else {
                throw new InvalidKeyException(
                        "Only accept ECPrivateKey for decryption");
            }
        }
    }

    @Override
    public void engineInit(int opmode, Key key,
            AlgorithmParameterSpec params, SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        engineInit(opmode, key, random);
    }

    @Override
    public void engineInit(int opmode, Key key,
            AlgorithmParameters params, SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        if (params != null) {
            throw new InvalidAlgorithmParameterException(
                    "Not need AlgorithmParameters");
        }

        engineInit(opmode, key, random);
    }

    @Override
    public byte[] engineUpdate(byte[] input, int inputOffset, int inputLen) {
        buffer.write(input, inputOffset, inputLen);
        return B0;
    }

    @Override
    public int engineUpdate(byte[] input, int inputOffset, int inputLen,
                            byte[] output, int outputOffset) {
        buffer.write(input, inputOffset, inputLen);
        return 0;
    }

    @Override
    public byte[] engineDoFinal(byte[] input, int inputOffset, int inputLen)
            throws IllegalBlockSizeException, BadPaddingException {
        update(input, inputOffset, inputLen);
        return doFinal();
    }

    @Override
    public int engineDoFinal(byte[] input, int inputOffset, int inputLen,
            byte[] output, int outputOffset)
            throws ShortBufferException, IllegalBlockSizeException,
            BadPaddingException {
        int outputSize = engineGetOutputSize(buffer.size());
        if (outputSize > output.length - outputOffset) {
            throw new ShortBufferException(
                    "Need " + outputSize + " bytes for output");
        }

        update(input, inputOffset, inputLen);
        byte[] result = doFinal();
        int n = result.length;
        System.arraycopy(result, 0, output, outputOffset, n);
        Arrays.fill(result, (byte)0);
        return result.length;
    }

    @Override
    public byte[] engineWrap(Key key) throws InvalidKeyException,
            IllegalBlockSizeException {
        byte[] encoded = key.getEncoded();
        if (encoded == null || encoded.length == 0) {
            throw new InvalidKeyException("No encoded key");
        }

        try {
            return engineDoFinal(encoded, 0, encoded.length);
        } catch (BadPaddingException e) {
            throw new InvalidKeyException("Wrap key failed", e);
        } finally {
            Arrays.fill(encoded, (byte)0);
        }
    }

    @Override
    public Key engineUnwrap(byte[] wrappedKey, String algorithm,
            int type) throws InvalidKeyException, NoSuchAlgorithmException {
        if (wrappedKey == null || wrappedKey.length == 0) {
            throw new InvalidKeyException("No wrapped key");
        }

        byte[] encoded;
        try {
            encoded = engineDoFinal(wrappedKey, 0, wrappedKey.length);
        } catch (IllegalBlockSizeException | BadPaddingException e) {
            throw new InvalidKeyException("Unwrap key failed", e);
        }

        return ConstructKeys.constructKey(encoded, algorithm, type);
    }

    private void update(byte[] input, int inputOffset, int inputLen) {
        if (input == null || inputLen == 0) {
            return;
        }

        buffer.write(input, inputOffset, inputLen);
    }

    private byte[] doFinal() throws BadPaddingException, IllegalBlockSizeException {
        try {
            byte[] input = buffer.toByteArray();
            return engine.processBlock(input, 0, input.length);
        } finally {
            buffer.reset();
        }
    }

    @Override
    public AlgorithmParameters engineGetParameters() {
        return null;
    }

    @Override
    public byte[] engineGetIV() {
        return null;
    }

    @Override
    public int engineGetBlockSize() {
        return 0;
    }

    @Override
    public int engineGetOutputSize(int inputLen) {
        // 1 + 2 * SM2_curve_field_size + SM3_digest_length
        int offset = 1 + 2 * 32 + 32;
        return engine.encrypted() ? inputLen + offset
                                  : Math.max(0, inputLen - offset);
    }

    @Override
    public int engineGetKeySize(Key key) {
        return 256;
    }

    private static final class Buffer extends ByteArrayOutputStream {

        public void reset() {
            Arrays.fill(buf, (byte)0);
            super.reset();
        }
    }
}

final class ConstructKeys {
    /**
     * Construct a public key from its encoding.
     *
     * @param encodedKey the encoding of a public key.
     *
     * @param encodedKeyAlgorithm the algorithm the encodedKey is for.
     *
     * @return a public key constructed from the encodedKey.
     */
    private static final PublicKey constructPublicKey(byte[] encodedKey,
            String encodedKeyAlgorithm)
            throws InvalidKeyException, NoSuchAlgorithmException {
        try {
            KeyFactory keyFactory =
                    KeyFactory.getInstance(encodedKeyAlgorithm);
            X509EncodedKeySpec keySpec = new X509EncodedKeySpec(encodedKey);
            return keyFactory.generatePublic(keySpec);
        } catch (NoSuchAlgorithmException nsae) {
            throw new NoSuchAlgorithmException("No installed providers " +
                    "can create keys for the " +
                    encodedKeyAlgorithm +
                    "algorithm", nsae);
        } catch (InvalidKeySpecException ike) {
            throw new InvalidKeyException("Cannot construct public key", ike);
        }
    }

    /**
     * Construct a private key from its encoding.
     *
     * @param encodedKey the encoding of a private key.
     *
     * @param encodedKeyAlgorithm the algorithm the wrapped key is for.
     *
     * @return a private key constructed from the encodedKey.
     */
    private static final PrivateKey constructPrivateKey(byte[] encodedKey,
            String encodedKeyAlgorithm) throws InvalidKeyException,
            NoSuchAlgorithmException {
        try {
            KeyFactory keyFactory =
                    KeyFactory.getInstance(encodedKeyAlgorithm);
            PKCS8EncodedKeySpec keySpec = new PKCS8EncodedKeySpec(encodedKey);
            return keyFactory.generatePrivate(keySpec);
        } catch (NoSuchAlgorithmException nsae) {
            throw new NoSuchAlgorithmException("No installed providers " +
                    "can create keys for the " +
                    encodedKeyAlgorithm +
                    "algorithm", nsae);
        } catch (InvalidKeySpecException ike) {
            throw new InvalidKeyException("Cannot construct private key", ike);
        }
    }

    /**
     * Construct a secret key from its encoding.
     *
     * @param encodedKey the encoding of a secret key.
     *
     * @param encodedKeyAlgorithm the algorithm the secret key is for.
     *
     * @return a secret key constructed from the encodedKey.
     */
    private static final SecretKey constructSecretKey(byte[] encodedKey,
        String encodedKeyAlgorithm) {
        return new SecretKeySpec(encodedKey, encodedKeyAlgorithm);
    }

    static final Key constructKey(byte[] encoding, String keyAlgorithm,
                                  int keyType) throws InvalidKeyException, NoSuchAlgorithmException {
        switch (keyType) {
            case Cipher.SECRET_KEY:
                return constructSecretKey(encoding, keyAlgorithm);
            case Cipher.PRIVATE_KEY:
                return constructPrivateKey(encoding, keyAlgorithm);
            case Cipher.PUBLIC_KEY:
                return constructPublicKey(encoding, keyAlgorithm);
            default:
                throw new InvalidKeyException("Unknown keytype " + keyType);
        }
    }
}
