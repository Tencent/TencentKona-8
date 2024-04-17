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

import java.io.IOException;
import java.nio.ByteBuffer;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.Signature;
import java.security.SignatureException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECPublicKey;
import java.security.spec.SM2SignatureParameterSpec;
import java.text.MessageFormat;
import java.util.Locale;

import sun.misc.HexDumpEncoder;
import sun.security.ssl.SSLHandshake.HandshakeMessage;
import sun.security.ssl.TLCPAuthentication.TLCP11Credentials;
import sun.security.ssl.TLCPAuthentication.TLCP11Possession;

/**
 * Pack of the static SM2 ServerKeyExchange handshake message.
 */
final class SM2ServerKeyExchange {

    static final SSLConsumer sm2HandshakeConsumer
            = new SM2ServerKeyExchangeConsumer();
    static final HandshakeProducer sm2HandshakeProducer
            = new SM2ServerKeyExchangeProducer();

    private static final class SM2ServerKeyExchangeMessage
            extends HandshakeMessage {

        // signature bytes, or null if anonymous
        private final byte[] paramsSignature;

        private final boolean useExplicitSigAlgorithm;

        // the signature algorithm used by this ServerKeyExchange message
        private final SignatureScheme signatureScheme;

        SM2ServerKeyExchangeMessage(
                HandshakeContext handshakeContext) throws IOException {
            super(handshakeContext);

            // This happens in server side only.
            ServerHandshakeContext shc =
                    (ServerHandshakeContext)handshakeContext;

            TLCP11Possession tlcpPossession = null;
            for (SSLPossession possession : shc.handshakePossessions) {
                if (possession instanceof TLCP11Possession) {
                    tlcpPossession = (TLCP11Possession) possession;
                    break;
                }
            }

            if (tlcpPossession == null) {
                // unlikely
                throw shc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "No SM2 credentials negotiated for server key exchange");
            }

            useExplicitSigAlgorithm =
                        shc.negotiatedProtocol.useTLS12PlusSpec();
            if (useExplicitSigAlgorithm) {
                if (shc.peerRequestedSignatureSchemes == null
                        || !shc.peerRequestedSignatureSchemes.contains(
                                SignatureScheme.SM2SIG_SM3)) {
                    // Unlikely, the credentials generator should have
                    // selected the preferable signature algorithm properly.
                    throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                            "No supported signature algorithm for " +
                            tlcpPossession.popSignPrivateKey.getAlgorithm() +
                            " key");
                }
                signatureScheme = SignatureScheme.SM2SIG_SM3;
            } else {
                signatureScheme = null;
            }

            byte[] signature;
            try {
                Signature signer = Signature.getInstance(
                        SignatureScheme.SM2SIG_SM3.algorithm);

                // Set ID and public key for SM3withSM2.
                signer.setParameter(new SM2SignatureParameterSpec(
                        (ECPublicKey) tlcpPossession.popSignPublicKey));

                signer.initSign(tlcpPossession.popSignPrivateKey);

                updateSignature(signer,
                        shc.clientHelloRandom.randomBytes,
                        shc.serverHelloRandom.randomBytes,
                        tlcpPossession.popEncCert);

                signature = signer.sign();
            } catch (SignatureException | NoSuchAlgorithmException
                    | InvalidKeyException
                    | InvalidAlgorithmParameterException
                    | CertificateEncodingException ex) {
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                    "Failed to sign SM2 parameters: " +
                    tlcpPossession.popSignPrivateKey.getAlgorithm(), ex);
            }
            paramsSignature = signature;
        }

        SM2ServerKeyExchangeMessage(HandshakeContext handshakeContext,
                                    ByteBuffer m) throws IOException {
            super(handshakeContext);

            // This happens in client side only.
            ClientHandshakeContext chc =
                    (ClientHandshakeContext)handshakeContext;

            TLCP11Credentials tlcpCredentials = null;
            for (SSLCredentials cd : chc.handshakeCredentials) {
                if (cd instanceof TLCP11Credentials) {
                    tlcpCredentials = (TLCP11Credentials)cd;
                    break;
                }
            }

            if (tlcpCredentials == null) {
                // unlikely
                throw chc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "No SM2 credentials negotiated for server key exchange");
            }

            this.useExplicitSigAlgorithm =
                    chc.negotiatedProtocol.useTLS12PlusSpec();
            if (useExplicitSigAlgorithm) {
                int ssid = Record.getInt16(m);
                signatureScheme = SignatureScheme.valueOf(ssid);
                if (signatureScheme == null) {
                    throw chc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "Invalid signature algorithm (" + ssid +
                        ") used in SM2 ServerKeyExchange handshake message");
                }

                if (!chc.localSupportedSignAlgs.contains(signatureScheme)) {
                    throw chc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "Unsupported signature algorithm (" +
                        signatureScheme.name +
                        ") used in SM2 ServerKeyExchange handshake message");
                }
            } else {
                signatureScheme = null;
            }

            // read and verify the signature
            paramsSignature = Record.getBytes16(m);

            try {
                Signature signer = Signature.getInstance(
                        SignatureScheme.SM2SIG_SM3.algorithm);

                // Set ID and public key for SM3withSM2.
                signer.setParameter(new SM2SignatureParameterSpec(
                        (ECPublicKey) tlcpCredentials.popSignCert.getPublicKey()));

                signer.initVerify(tlcpCredentials.popSignPublicKey);

                updateSignature(signer,
                        chc.clientHelloRandom.randomBytes,
                        chc.serverHelloRandom.randomBytes,
                        tlcpCredentials.popEncCert);

                if (!signer.verify(paramsSignature)) {
                    throw chc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "Invalid SM2 ServerKeyExchange signature");
                }
            } catch (NoSuchAlgorithmException | InvalidKeyException
                    | InvalidAlgorithmParameterException
                    | SignatureException | CertificateEncodingException ex) {
                throw chc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "Cannot verify SM2 ServerKeyExchange signature", ex);
            }
        }

        @Override
        public SSLHandshake handshakeType() {
            return SSLHandshake.SERVER_KEY_EXCHANGE;
        }

        @Override
        public int messageLength() {
            int sigLen = 2 + paramsSignature.length;
            if (useExplicitSigAlgorithm) {
                sigLen += SignatureScheme.sizeInRecord();
            }
            return sigLen;
        }

        @Override
        public void send(HandshakeOutStream hos) throws IOException {
            if (useExplicitSigAlgorithm) {
                hos.putInt16(signatureScheme.id);
            }
            hos.putBytes16(paramsSignature);
        }

        @Override
        public String toString() {
            if (useExplicitSigAlgorithm) {
                MessageFormat messageFormat = new MessageFormat(
                        "\"SM2E ServerKeyExchange\": '{'\n" +
                        "  \"digital signature\":  '{'\n" +
                        "    \"signature algorithm\": \"{0}\"\n" +
                        "    \"signature\": '{'\n" +
                        "{1}\n" +
                        "    '}',\n" +
                        "  '}'\n" +
                        "'}'",
                        Locale.ENGLISH);

                HexDumpEncoder hexEncoder = new HexDumpEncoder();
                Object[] messageFields = {
                        signatureScheme.name,
                        Utilities.indent(
                                hexEncoder.encodeBuffer(paramsSignature), "      ")
                };
                return messageFormat.format(messageFields);

            } else {
                MessageFormat messageFormat = new MessageFormat(
                        "\"SM2 ServerKeyExchange\": '{'\n" +
                        "  \"digital signature\":  '{'\n" +
                        "    \"signature\": '{'\n" +
                        "{0}\n" +
                        "    '}',\n" +
                        "  '}'\n" +
                        "'}'",
                        Locale.ENGLISH);

                HexDumpEncoder hexEncoder = new HexDumpEncoder();
                Object[] messageFields = {
                        Utilities.indent(
                                hexEncoder.encodeBuffer(paramsSignature), "      ")
                };
                return messageFormat.format(messageFields);
            }
        }

        private static void updateSignature(Signature sig,
                byte[] clntNonce, byte[] svrNonce, X509Certificate encCert)
                throws SignatureException, CertificateEncodingException {
            sig.update(clntNonce);
            sig.update(svrNonce);

            byte[] encodedEncCert = encCert.getEncoded();
            int certLength = encodedEncCert.length;
            sig.update((byte)(certLength >> 16 & 0x0ff));
            sig.update((byte)(certLength >> 8 & 0x0ff));
            sig.update((byte)(certLength & 0x0ff));
            sig.update(encodedEncCert);
        }
    }

    private static final class SM2ServerKeyExchangeProducer
            implements HandshakeProducer {

        // Prevent instantiation of this class.
        private SM2ServerKeyExchangeProducer() {
            // blank
        }

        @Override
        public byte[] produce(ConnectionContext context,
                HandshakeMessage message) throws IOException {
            // The producing happens in server side only.
            ServerHandshakeContext shc = (ServerHandshakeContext)context;
            SM2ServerKeyExchangeMessage skem =
                    new SM2ServerKeyExchangeMessage(shc);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Produced SM2 ServerKeyExchange handshake message", skem);
            }

            // Output the handshake message.
            skem.write(shc.handshakeOutput);
            shc.handshakeOutput.flush();

            // The handshake message has been delivered.
            return null;
        }
    }

    private static final class SM2ServerKeyExchangeConsumer
            implements SSLConsumer {

        // Prevent instantiation of this class.
        private SM2ServerKeyExchangeConsumer() {
            // blank
        }

        @Override
        public void consume(ConnectionContext context,
                ByteBuffer message) throws IOException {
            // The consuming happens in client side only.
            ClientHandshakeContext chc = (ClientHandshakeContext)context;

            // AlgorithmConstraints are checked during decoding
            SM2ServerKeyExchangeMessage skem =
                    new SM2ServerKeyExchangeMessage(chc, message);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Consuming SM2 ServerKeyExchange handshake message", skem);
            }

            //
            // produce
            //
            // Need no new handshake message producers here.
        }
    }
}
