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

package com.tencent.crypto.provider;
import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;
import java.security.*;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;

/**
 * This class is a helper class which construct key objects
 * from encoded keys.
 *
 */

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
        throws InvalidKeyException, NoSuchAlgorithmException
    {
        PublicKey key = null;

        try {
            KeyFactory keyFactory =
                KeyFactory.getInstance(encodedKeyAlgorithm,
                    SMCSProvider.getInstance());
            X509EncodedKeySpec keySpec = new X509EncodedKeySpec(encodedKey);
            key = keyFactory.generatePublic(keySpec);
        } catch (NoSuchAlgorithmException nsae) {
            // Try to see whether there is another
            // provider which supports this algorithm
            try {
                KeyFactory keyFactory =
                    KeyFactory.getInstance(encodedKeyAlgorithm);
                X509EncodedKeySpec keySpec =
                    new X509EncodedKeySpec(encodedKey);
                key = keyFactory.generatePublic(keySpec);
            } catch (NoSuchAlgorithmException nsae2) {
                throw new NoSuchAlgorithmException("No installed providers " +
                                                   "can create keys for the " +
                                                   encodedKeyAlgorithm +
                                                   "algorithm");
            } catch (InvalidKeySpecException ikse2) {
                InvalidKeyException ike =
                    new InvalidKeyException("Cannot construct public key");
                ike.initCause(ikse2);
                throw ike;
            }
        } catch (InvalidKeySpecException ikse) {
            InvalidKeyException ike =
                new InvalidKeyException("Cannot construct public key");
            ike.initCause(ikse);
            throw ike;
        }

        return key;
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
                                                String encodedKeyAlgorithm)
        throws InvalidKeyException, NoSuchAlgorithmException
    {
        PrivateKey key = null;

        try {
            KeyFactory keyFactory =
                KeyFactory.getInstance(encodedKeyAlgorithm,
                    SMCSProvider.getInstance());
            PKCS8EncodedKeySpec keySpec = new PKCS8EncodedKeySpec(encodedKey);
            return keyFactory.generatePrivate(keySpec);
        } catch (NoSuchAlgorithmException nsae) {
            // Try to see whether there is another
            // provider which supports this algorithm
            try {
                KeyFactory keyFactory =
                    KeyFactory.getInstance(encodedKeyAlgorithm);
                PKCS8EncodedKeySpec keySpec =
                    new PKCS8EncodedKeySpec(encodedKey);
                key = keyFactory.generatePrivate(keySpec);
            } catch (NoSuchAlgorithmException nsae2) {
                throw new NoSuchAlgorithmException("No installed providers " +
                                                   "can create keys for the " +
                                                   encodedKeyAlgorithm +
                                                   "algorithm");
            } catch (InvalidKeySpecException ikse2) {
                InvalidKeyException ike =
                    new InvalidKeyException("Cannot construct private key");
                ike.initCause(ikse2);
                throw ike;
            }
        } catch (InvalidKeySpecException ikse) {
            InvalidKeyException ike =
                new InvalidKeyException("Cannot construct private key");
            ike.initCause(ikse);
            throw ike;
        }

        return key;
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
                                              String encodedKeyAlgorithm)
    {
        return (new SecretKeySpec(encodedKey, encodedKeyAlgorithm));
    }

    static final Key constructKey(byte[] encoding, String keyAlgorithm,
                                  int keyType)
        throws InvalidKeyException, NoSuchAlgorithmException {
        Key result = null;
        switch (keyType) {
        case Cipher.SECRET_KEY:
            result = ConstructKeys.constructSecretKey(encoding, keyAlgorithm);
            break;
        case Cipher.PRIVATE_KEY:
            result = ConstructKeys.constructPrivateKey(encoding, keyAlgorithm);
            break;
        case Cipher.PUBLIC_KEY:
            result = constructPublicKey(encoding, keyAlgorithm);
            break;
        }
        return result;
    }
}
