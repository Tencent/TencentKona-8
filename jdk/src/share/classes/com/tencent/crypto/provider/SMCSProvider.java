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

import java.security.*;

/**
 * The "SM  Cryptographic Service" Provider.
 */

/**
 * Defines the "SMCS" provider.
 *
 * Supported algorithms and their names:
 *
 * - SM2/SM4 encryption
 *
 * - Cipher modes ECB, CBC for SM4 cipher
 *
 * - Cipher padding NoPadding for all block ciphers
 *
 * - SM3 digest
 */

public final class SMCSProvider extends Provider {

    private static final long serialVersionUID = 6812507587804309411L; // ??

    private static final String info = "Tencent SMCS Provider " +
    "(implements SM2, SM3 and SM4 algorithms)";

    /* Are we debugging? -- for developers */
    static final boolean debug = false;

    // Instance of this provider, so we don't have to call the provider list
    // to find ourselves or run the risk of not being in the list.
    private static volatile SMCSProvider instance;

    // lazy initialize SecureRandom to avoid potential recursion if Sun
    // provider has not been installed yet
    private static class SecureRandomHolder {
        static SecureRandom RANDOM;

        static {
            try {
                RANDOM = SecureRandom.getInstance("SM");
            } catch (NoSuchAlgorithmException e) {
                RANDOM = new SecureRandom();
            }
        }
    }
    static SecureRandom getRandom() { return SecureRandomHolder.RANDOM; }

    public SMCSProvider() {
        /* We are the "SMCS" provider */
        super("SMCSProvider", 1.8d, info);

        final String BLOCK_MODES = "ECB|CBC|GCM|CTR";
        final String BLOCK_PADS = "NOPADDING|PKCS7PADDING";
        final String SM2_MODES = "C1C3C2_ASN1|C1C3C2|C1C2C3_ASN1|C1C2C3";

        AccessController.doPrivileged(
            new java.security.PrivilegedAction<Object>() {
                public Object run() {

                    /*
                     * Cipher engines
                     */
                    put("Cipher.SM2", "com.tencent.crypto.provider.SM2Cipher");
                    put("Cipher.SM2 SupportedModes", SM2_MODES);
                    put("Cipher.SM2 SupportedPaddings", "NOPADDING");
                    put("Cipher.SM2 SupportedKeyClass", "SM2PrivateKey|SM2PublicKey");

                    put("Cipher.SM4", "com.tencent.crypto.provider.SM4Cipher");
                    put("Cipher.SM4 SupportedModes", BLOCK_MODES);
                    put("Cipher.SM4 SupportedPaddings", BLOCK_PADS);
                    put("Cipher.SM4 SupportedKeyFormats", "RAW");

                    /*
                     * Key(pair) Generator engines
                     */
                    put("KeyPairGenerator.SM2",
                        "com.tencent.crypto.provider.SM2KeyPairGenerator");

                    put("KeyFactory.SM2", "com.tencent.crypto.provider.SM2KeyFactory");


                    put("Signature.SM2",
                            "com.tencent.crypto.provider.SM2Signature");

                    put("AlgorithmParameters.SM2",
                            "com.tencent.crypto.provider.SM2Parameters");
                    put("AlgorithmParameters.SM4",
                            "com.tencent.crypto.provider.SM4Parameters");

                    put("AlgorithmParameterSpec.SM4GCM",
                            "com.tencent.crypto.provider.SM4GCMParameterSpec");

                    put("KeyGenerator.SM4",
                            "com.tencent.crypto.provider.SM4KeyGenerator");

                    put("SecureRandom.SM",
                            "com.tencent.crypto.provider.SMSecureRandom");

                    put("MessageDigest.SM3",
                            "com.tencent.crypto.provider.SM3MessageDigest");
                    put("SecretKeyFactory.SM4",
                            "com.tencent.crypto.provider.SM4KeyFactory");
                    put("SecretKey.SM4",
                            "com.tencent.crypto.provider.SM4Key");

                    return null;
                }
            });

        if (instance == null) {
            instance = this;
        }
    }

    // Return the instance of this class or create one if needed.
    static SMCSProvider getInstance() {
        if (instance == null) {
            instance = new SMCSProvider();
        }
        return instance;
    }

    public static void install() {
        Security.addProvider(getInstance());
    }
}