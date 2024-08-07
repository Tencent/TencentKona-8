/*
 * Copyright (c) 2022, 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package sun.security.ssl;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.net.ssl.SSLHandshakeException;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;

import sun.security.internal.spec.TlsRsaPremasterSecretParameterSpec;
import sun.security.util.KeyUtil;

/**
 * TLCP 1.1 static SM2 key exchange.
 */
final class SM2KeyExchange {

    static final SSLPossessionGenerator sm2PoGenerator =
            new SM2PossessionGenerator();
    static final SSLKeyAgreementGenerator sm2KAGenerator =
            new SM2KAGenerator();

    private static final class SM2PossessionGenerator
            implements SSLPossessionGenerator {

        // Prevent instantiation of this class.
        private SM2PossessionGenerator() {
            // blank
        }

        @Override
        public SSLPossession createPossession(HandshakeContext context) {
            return null;
        }
    }

    static final class SM2PremasterSecret
            implements SSLPossession, SSLCredentials {

        final SecretKey premasterSecret;

        SM2PremasterSecret(SecretKey premasterSecret) {
            this.premasterSecret = premasterSecret;
        }

        byte[] getEncoded(PublicKey publicKey,
                SecureRandom secureRandom) throws GeneralSecurityException {
            Cipher cipher = Cipher.getInstance("SM2");
            cipher.init(Cipher.WRAP_MODE, publicKey, secureRandom);
            return cipher.wrap(premasterSecret);
        }

        @SuppressWarnings("deprecation")
        static SM2PremasterSecret createPremasterSecret(
                ClientHandshakeContext chc) throws GeneralSecurityException {
            String algorithm = chc.negotiatedProtocol.useTLS12PlusSpec() ?
                    "SunTls12RsaPremasterSecret" : "SunTlsRsaPremasterSecret";
            KeyGenerator kg = KeyGenerator.getInstance(algorithm);
            TlsRsaPremasterSecretParameterSpec spec =
                    new TlsRsaPremasterSecretParameterSpec(
                            chc.clientHelloVersion,
                            chc.negotiatedProtocol.id);
            kg.init(spec, chc.sslContext.getSecureRandom());

            return new SM2PremasterSecret(kg.generateKey());
        }

        @SuppressWarnings("deprecation")
        static SM2PremasterSecret decode(ServerHandshakeContext shc,
                PrivateKey privateKey, byte[] encrypted)
                throws GeneralSecurityException {

            byte[] encoded = null;
            boolean needFailover;
            Cipher cipher = Cipher.getInstance("SM2");
            try {
                // Try UNWRAP_MODE mode firstly.
                cipher.init(Cipher.UNWRAP_MODE, privateKey,
                        new TlsRsaPremasterSecretParameterSpec(
                                shc.clientHelloVersion,
                                shc.negotiatedProtocol.id),
                                shc.sslContext.getSecureRandom());

                // The provider selection can be delayed, please don't call
                // any Cipher method before the call to Cipher.init().
                String providerName = cipher.getProvider().getName();
                needFailover = !KeyUtil.isOracleJCEProvider(providerName);
            } catch (InvalidKeyException | UnsupportedOperationException iue) {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.warning("The Cipher provider "
                            + safeProviderName(cipher)
                            + " caused exception: " + iue.getMessage());
                }

                needFailover = true;
            }

            SecretKey preMaster;
            if (needFailover) {
                // The cipher might be spoiled by unsuccessful call to init(),
                // so request a fresh instance
                cipher = Cipher.getInstance("SM2");

                // Use DECRYPT_MODE and dispose the previous initialization.
                cipher.init(Cipher.DECRYPT_MODE, privateKey);
                boolean failed = false;
                try {
                    encoded = cipher.doFinal(encrypted);
                } catch (BadPaddingException bpe) {
                    // Note: encoded == null
                    failed = true;
                }
                encoded = KeyUtil.checkTlsPreMasterSecretKey(
                        shc.clientHelloVersion, shc.negotiatedProtocol.id,
                        shc.sslContext.getSecureRandom(), encoded, failed);
                preMaster = generatePremasterSecret(
                        shc.clientHelloVersion, shc.negotiatedProtocol.id,
                        encoded, shc.sslContext.getSecureRandom());
            } else {
                // the cipher should have been initialized
                preMaster = (SecretKey)cipher.unwrap(encrypted,
                        "TlsRsaPremasterSecret", Cipher.SECRET_KEY);
            }

            return new SM2PremasterSecret(preMaster);
        }

        /*
         * Retrieving the cipher's provider name for the debug purposes
         * can throw an exception by itself.
         */
        private static String safeProviderName(Cipher cipher) {
            try {
                return cipher.getProvider().toString();
            } catch (Exception e) {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine("Retrieving The Cipher provider name" +
                            " caused exception ", e);
                }
            }
            try {
                return cipher.toString() + " (provider name not available)";
            } catch (Exception e) {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine("Retrieving The Cipher name" +
                            " caused exception ", e);
                }
            }

            return "(cipher/provider names not available)";
        }

        // generate a premaster secret with the specified version number
        @SuppressWarnings("deprecation")
        private static SecretKey generatePremasterSecret(
                int clientVersion, int serverVersion, byte[] encodedSecret,
                SecureRandom generator) throws GeneralSecurityException {

            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine("Generating a premaster secret");
            }

            try {
                String s = ((clientVersion >= ProtocolVersion.TLS12.id) ?
                        "SunTls12RsaPremasterSecret" : "SunTlsRsaPremasterSecret");
                KeyGenerator kg = KeyGenerator.getInstance(s);
                kg.init(new TlsRsaPremasterSecretParameterSpec(
                        clientVersion, serverVersion, encodedSecret),
                        generator);
                return kg.generateKey();
            } catch (InvalidAlgorithmParameterException |
                    NoSuchAlgorithmException iae) {
                // unlikely to happen, otherwise, must be a provider exception
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine("ECC premaster secret generation error:");
                    iae.printStackTrace(System.out);
                }

                throw new GeneralSecurityException(
                        "Could not generate premaster secret", iae);
            }
        }
    }

    private static final class SM2KAGenerator
            implements SSLKeyAgreementGenerator {

        // Prevent instantiation of this class.
        private SM2KAGenerator() {
            // blank
        }

        @Override
        public SSLKeyDerivation createKeyDerivation(
                HandshakeContext context) throws IOException {
            SM2PremasterSecret premaster = null;
            if (context instanceof ClientHandshakeContext) {
                for (SSLPossession possession : context.handshakePossessions) {
                    if (possession instanceof SM2PremasterSecret) {
                        premaster = (SM2PremasterSecret)possession;
                        break;
                    }
                }
            } else {
                for (SSLCredentials credential : context.handshakeCredentials) {
                    if (credential instanceof SM2PremasterSecret) {
                        premaster = (SM2PremasterSecret)credential;
                        break;
                    }
                }
            }

            if (premaster == null) {
                throw context.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                    "No sufficient SM2 key agreement parameters negotiated");
            }

            return new SM2KAKeyDerivation(context, premaster.premasterSecret);
        }

        private static final class SM2KAKeyDerivation
                implements SSLKeyDerivation {

            private final HandshakeContext context;
            private final SecretKey preMasterSecret;

            SM2KAKeyDerivation(
                    HandshakeContext context, SecretKey preMasterSecret) {
                this.context = context;
                this.preMasterSecret = preMasterSecret;
            }

            @Override
            public SecretKey deriveKey(String algorithm,
                    AlgorithmParameterSpec params) throws IOException {
                SSLMasterKeyDerivation mskd =
                        SSLMasterKeyDerivation.valueOf(
                                context.negotiatedProtocol);
                if (mskd == null) {
                    // unlikely
                    throw new SSLHandshakeException(
                            "No expected master key derivation for protocol: " +
                            context.negotiatedProtocol.name);
                }
                SSLKeyDerivation kd = mskd.createKeyDerivation(
                        context, preMasterSecret);
                return kd.deriveKey("MasterSecret", params);
            }
        }
    }
}
