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
import java.security.GeneralSecurityException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.SM2ParameterSpec;
import java.text.MessageFormat;
import java.util.Locale;
import javax.crypto.SecretKey;

import sun.misc.HexDumpEncoder;
import sun.security.ssl.SM2KeyExchange.SM2PremasterSecret;
import sun.security.ssl.SSLHandshake.HandshakeMessage;
import sun.security.ssl.TLCPAuthentication.TLCP11Credentials;
import sun.security.ssl.TLCPAuthentication.TLCP11Possession;

/**
 * Pack of the "ClientKeyExchange" handshake message.
 */
final class SM2ClientKeyExchange {

    static final SSLConsumer sm2HandshakeConsumer
        = new SM2ClientKeyExchangeConsumer();
    static final HandshakeProducer sm2HandshakeProducer
        = new SM2ClientKeyExchangeProducer();

    /**
     * The SM2 ClientKeyExchange handshake message.
     */
    private static final class SM2ClientKeyExchangeMessage
            extends HandshakeMessage {

        final int protocolVersion;
        final byte[] encrypted;

        SM2ClientKeyExchangeMessage(HandshakeContext context,
                SM2PremasterSecret premaster, PublicKey publicKey)
                throws GeneralSecurityException {
            super(context);
            this.protocolVersion = context.clientHelloVersion;
            this.encrypted = premaster.getEncoded(
                    publicKey, context.sslContext.getSecureRandom());
        }

        SM2ClientKeyExchangeMessage(HandshakeContext context,
                                    ByteBuffer m) throws IOException {
            super(context);

            if (m.remaining() < 2) {
                throw context.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                    "Invalid SM2 ClientKeyExchange message: insufficient data");
            }

            this.protocolVersion = context.clientHelloVersion;
            this.encrypted = Record.getBytes16(m);
        }

        @Override
        public SSLHandshake handshakeType() {
            return SSLHandshake.CLIENT_KEY_EXCHANGE;
        }

        @Override
        public int messageLength() {
            return encrypted.length + 2;
        }

        @Override
        public void send(HandshakeOutStream hos) throws IOException {
            hos.putBytes16(encrypted);
        }

        @Override
        public String toString() {
            MessageFormat messageFormat = new MessageFormat(
                "\"SM2 ClientKeyExchange\": '{'\n" +
                "  \"client_version\":  {0}\n" +
                "  \"encncrypted\": '{'\n" +
                "{1}\n" +
                "  '}'\n" +
                "'}'",
                Locale.ENGLISH);

            HexDumpEncoder hexEncoder = new HexDumpEncoder();
            Object[] messageFields = {
                ProtocolVersion.nameOf(protocolVersion),
                Utilities.indent(
                        hexEncoder.encodeBuffer(encrypted), "    "),
            };
            return messageFormat.format(messageFields);
        }
    }

    /**
     * The SM2 "ClientKeyExchange" handshake message producer.
     */
    private static final class SM2ClientKeyExchangeProducer
            implements HandshakeProducer {

        // Prevent instantiation of this class.
        private SM2ClientKeyExchangeProducer() {
            // blank
        }

        @Override
        public byte[] produce(ConnectionContext context,
                HandshakeMessage message) throws IOException {
            // This happens in client side only.
            ClientHandshakeContext chc = (ClientHandshakeContext)context;

            TLCP11Credentials tlcpCredentials = null;
            for (SSLCredentials credential : chc.handshakeCredentials) {
                if (credential instanceof TLCP11Credentials) {
                    tlcpCredentials = (TLCP11Credentials)credential;
                    break;
                }
            }

            if (tlcpCredentials == null) {
                throw chc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "No SM2 credentials negotiated for client key exchange");
            }

            ECPublicKey publicKey = (ECPublicKey) tlcpCredentials.popEncPublicKey;
            if (!publicKey.getAlgorithm().equals("EC")
                    || publicKey.getParams() instanceof SM2ParameterSpec) {
                // unlikely
                throw chc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "Not SM2 public key for client key exchange");
            }

            SM2PremasterSecret premaster;
            SM2ClientKeyExchangeMessage ckem;
            try {
                premaster = SM2PremasterSecret.createPremasterSecret(chc);
                chc.handshakePossessions.add(premaster);
                ckem = new SM2ClientKeyExchangeMessage(
                        chc, premaster, publicKey);
            } catch (GeneralSecurityException gse) {
                throw chc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                        "Cannot generate SM2 premaster secret", gse);
            }
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Produced SM2 ClientKeyExchange handshake message", ckem);
            }

            // Output the handshake message.
            ckem.write(chc.handshakeOutput);
            chc.handshakeOutput.flush();

            // update the states
            SSLKeyExchange ke = SSLKeyExchange.valueOf(
                    chc.negotiatedCipherSuite.keyExchange,
                    chc.negotiatedProtocol);
            if (ke == null) {   // unlikely
                throw chc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Not supported key exchange type");
            } else {
                SSLKeyDerivation masterKD = ke.createKeyDerivation(chc);
                SecretKey masterSecret =
                        masterKD.deriveKey("MasterSecret", null);

                // update the states
                chc.handshakeSession.setMasterSecret(masterSecret);
                SSLTrafficKeyDerivation kd =
                        SSLTrafficKeyDerivation.valueOf(chc.negotiatedProtocol);
                if (kd == null) {   // unlikely
                    throw chc.conContext.fatal(Alert.INTERNAL_ERROR,
                            "Not supported key derivation: " +
                            chc.negotiatedProtocol);
                } else {
                    chc.handshakeKeyDerivation =
                        kd.createKeyDerivation(chc, masterSecret);
                }
            }

            // The handshake message has been delivered.
            return null;
        }
    }

    /**
     * The SM2 "ClientKeyExchange" handshake message consumer.
     */
    private static final class SM2ClientKeyExchangeConsumer
            implements SSLConsumer {

        // Prevent instantiation of this class.
        private SM2ClientKeyExchangeConsumer() {
            // blank
        }

        @Override
        public void consume(ConnectionContext context,
                ByteBuffer message) throws IOException {
            // The consuming happens in server side only.
            ServerHandshakeContext shc = (ServerHandshakeContext)context;

            TLCP11Possession tlcpPossession = null;
            for (SSLPossession possession : shc.handshakePossessions) {
                if (possession instanceof TLCP11Possession) {
                    tlcpPossession = (TLCP11Possession)possession;
                    break;
                }
            }

            if (tlcpPossession == null) {  // unlikely
                throw shc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "No SM2 possessions negotiated for client key exchange");
            }

            PrivateKey privateKey = tlcpPossession.popEncPrivateKey;
            if (!privateKey.getAlgorithm().equals("EC")) {     // unlikely
                throw shc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "Not SM2 private key for client key exchange");
            }

            SM2ClientKeyExchangeMessage ckem =
                    new SM2ClientKeyExchangeMessage(shc, message);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Consuming SM2 ClientKeyExchange handshake message", ckem);
            }

            // create the credentials
            SM2PremasterSecret premaster;
            try {
                premaster =
                    SM2PremasterSecret.decode(shc, privateKey, ckem.encrypted);
                shc.handshakeCredentials.add(premaster);
            } catch (GeneralSecurityException gse) {
                throw shc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "Cannot decode SM2 premaster secret", gse);
            }

            // update the states
            SSLKeyExchange ke = SSLKeyExchange.valueOf(
                    shc.negotiatedCipherSuite.keyExchange,
                    shc.negotiatedProtocol);
            if (ke == null) {   // unlikely
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Not supported key exchange type");
            } else {
                SSLKeyDerivation masterKD = ke.createKeyDerivation(shc);
                SecretKey masterSecret =
                        masterKD.deriveKey("MasterSecret", null);

                // update the states
                shc.handshakeSession.setMasterSecret(masterSecret);
                SSLTrafficKeyDerivation kd =
                        SSLTrafficKeyDerivation.valueOf(shc.negotiatedProtocol);
                if (kd == null) {       // unlikely
                    throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                            "Not supported key derivation: " +
                            shc.negotiatedProtocol);
                } else {
                    shc.handshakeKeyDerivation =
                        kd.createKeyDerivation(shc, masterSecret);
                }
            }
        }
    }
}
