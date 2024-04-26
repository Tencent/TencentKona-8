/*
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
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

import javax.crypto.SecretKey;
import javax.net.ssl.SSLHandshakeException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.security.CryptoPrimitive;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.SM2KeyAgreementParamSpec;
import java.security.spec.SM2PublicKeySpec;
import java.text.MessageFormat;
import java.util.EnumSet;
import java.util.Locale;

import sun.misc.HexDumpEncoder;
import sun.security.ssl.SM2EKeyExchange.SM2ECredentials;
import sun.security.ssl.SM2EKeyExchange.SM2EPossession;
import sun.security.ssl.SSLHandshake.HandshakeMessage;
import sun.security.ssl.SupportedGroupsExtension.NamedGroup;
import sun.security.ssl.TLCPAuthentication.TLCP11Credentials;
import sun.security.ssl.TLCPAuthentication.TLCP11Possession;

/**
 * Pack of the ephemeral SM2 ClientKeyExchange handshake message.
 */
public class SM2EClientKeyExchange {

    static final SSLConsumer sm2eHandshakeConsumer
            = new SM2EClientKeyExchangeConsumer();
    static final HandshakeProducer sm2eHandshakeProducer
            = new SM2EClientKeyExchangeProducer();

    private static final class SM2EClientKeyExchangeMessage
            extends HandshakeMessage {
        private static final byte CURVE_NAMED_CURVE = (byte)0x03;
        private final byte[] encodedPoint;

        SM2EClientKeyExchangeMessage(HandshakeContext handshakeContext,
                                     byte[] encodedPublicKey) {
            super(handshakeContext);

            this.encodedPoint = encodedPublicKey;
        }

        SM2EClientKeyExchangeMessage(HandshakeContext handshakeContext,
                                     ByteBuffer m) throws IOException {
            super(handshakeContext);

            Record.getInt8(m);
            Record.getInt16(m);

            if (m.remaining() != 0) {       // explicit PublicValueEncoding
                this.encodedPoint = Record.getBytes8(m);
            } else {
                this.encodedPoint = new byte[0];
            }
        }

        @Override
        public SSLHandshake handshakeType() {
            return SSLHandshake.CLIENT_KEY_EXCHANGE;
        }

        @Override
        public int messageLength() {
            if (encodedPoint == null || encodedPoint.length == 0) {
                return 0;
            } else {
                return 1 + encodedPoint.length + 3;
            }
        }

        @Override
        public void send(HandshakeOutStream hos) throws IOException {
            hos.putInt8(CURVE_NAMED_CURVE);
            hos.putInt16(NamedGroup.CURVESM2.id);

            if (encodedPoint != null && encodedPoint.length != 0) {
                hos.putBytes8(encodedPoint);
            }
        }

        @Override
        public String toString() {
            MessageFormat messageFormat = new MessageFormat(
                "\"SM2 ClientKeyExchange\": '{'\n" +
                "  \"SM2 public\": '{'\n" +
                "{0}\n" +
                "  '}',\n" +
                "'}'",
                Locale.ENGLISH);
            if (encodedPoint == null || encodedPoint.length == 0) {
                Object[] messageFields = {
                    "    <implicit>"
                };
                return messageFormat.format(messageFields);
            } else {
                HexDumpEncoder hexEncoder = new HexDumpEncoder();
                Object[] messageFields = {
                    Utilities.indent(
                            hexEncoder.encodeBuffer(encodedPoint), "    "),
                };
                return messageFormat.format(messageFields);
            }
        }
    }

    private static final class SM2EClientKeyExchangeProducer
            implements HandshakeProducer {

        // Prevent instantiation of this class.
        private SM2EClientKeyExchangeProducer() {
            // blank
        }

        @Override
        public byte[] produce(ConnectionContext context,
                HandshakeMessage message) throws IOException {
            // The producing happens in client side only.
            ClientHandshakeContext chc = (ClientHandshakeContext)context;

            SM2ECredentials sm2eCredentials = null;
            for (SSLCredentials cd : chc.handshakeCredentials) {
                if (cd instanceof SM2ECredentials) {
                    sm2eCredentials = (SM2ECredentials) cd;
                    break;
                }
            }

            if (sm2eCredentials == null) {
                throw chc.conContext.fatal(Alert.INTERNAL_ERROR,
                    "No SM2E credentials negotiated for client key exchange");
            }

            TLCP11Possession tlcpPossession = null;
            for (SSLPossession possession : chc.handshakePossessions) {
                if (possession instanceof TLCP11Possession) {
                    tlcpPossession = (TLCP11Possession) possession;
                    break;
                }
            }
            SM2EPossession sm2ePossession = new SM2EPossession(
                    tlcpPossession, sm2eCredentials.namedGroup,
                    chc.sslContext.getSecureRandom());

            chc.handshakePossessions.add(sm2ePossession);

            // Write the EC/XEC message.
            SM2EClientKeyExchangeMessage cke =
                    new SM2EClientKeyExchangeMessage(
                            chc, sm2ePossession.encode());

            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Produced SM2E ClientKeyExchange handshake message", cke);
            }

            // Output the handshake message.
            cke.write(chc.handshakeOutput);
            chc.handshakeOutput.flush();

            TLCP11Credentials tlcpCredentials = null;
            for (SSLCredentials sslCredentials : chc.handshakeCredentials) {
                if (sslCredentials instanceof TLCP11Credentials) {
                    tlcpCredentials = (TLCP11Credentials)sslCredentials;
                    break;
                }
            }

            // update the states
            SSLKeyExchange ke = SSLKeyExchange.valueOf(
                    chc.negotiatedCipherSuite.keyExchange,
                    chc.negotiatedProtocol);
            if (ke == null) {
                // unlikely
                throw chc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Not supported key exchange type");
            } else {
                SM2KeyAgreementParamSpec params = new SM2KeyAgreementParamSpec(
                        (ECPrivateKey) tlcpPossession.popEncPrivateKey,
                        (ECPublicKey) tlcpPossession.popEncPublicKey,
                        (ECPublicKey) tlcpCredentials.popEncPublicKey,
                        false,
                        48);
                SSLKeyDerivation masterKD = ke.createKeyDerivation(chc);
                SecretKey masterSecret =
                        masterKD.deriveKey("MasterSecret", params);
                chc.handshakeSession.setMasterSecret(masterSecret);

                SSLTrafficKeyDerivation kd =
                        SSLTrafficKeyDerivation.valueOf(chc.negotiatedProtocol);
                if (kd == null) {
                    // unlikely
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

    private static final class SM2EClientKeyExchangeConsumer
            implements SSLConsumer {

        // Prevent instantiation of this class.
        private SM2EClientKeyExchangeConsumer() {
            // blank
        }

        @Override
        public void consume(ConnectionContext context,
                ByteBuffer message) throws IOException {
            // The consuming happens in server side only.
            ServerHandshakeContext shc = (ServerHandshakeContext)context;

            // Find a good EC/XEC credential to use, determine the
            // NamedGroup to use for creating Possessions/Credentials/Keys.
            SM2EPossession sm2ePossession = null;
            for (SSLPossession possession : shc.handshakePossessions) {
                if (possession instanceof SM2EPossession) {
                    sm2ePossession = (SM2EPossession)possession;
                    break;
                }
            }

            if (sm2ePossession == null) {
                // unlikely
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                    "No expected SM2E possessions for client key exchange");
            }

            NamedGroup namedGroup = NamedGroup.valueOf(
                    sm2ePossession.popEncPublicKey.getParams());
            if (namedGroup != NamedGroup.CURVESM2) {
                // unlikely, have been checked during cipher suite negotiation
                throw shc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "Unsupported EC server cert for SM2E client key exchange");
            }

            SSLKeyExchange ke = SSLKeyExchange.valueOf(
                    shc.negotiatedCipherSuite.keyExchange,
                    shc.negotiatedProtocol);
            if (ke == null) {
                // unlikely
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Not supported key exchange type");
            }

            // parse the EC/XEC handshake message
            SM2EClientKeyExchangeMessage cke =
                    new SM2EClientKeyExchangeMessage(shc, message);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Consuming SM2E ClientKeyExchange handshake message", cke);
            }

            // create the credentials
            try {
                KeyFactory kf = JsseJce.getKeyFactory("SM2");
                ECPublicKey peerPublicKey = (ECPublicKey) kf.generatePublic(
                        new SM2PublicKeySpec(cke.encodedPoint));

                // check constraints of peer ECPublicKey
                if (shc.algorithmConstraints != null &&
                        !shc.algorithmConstraints.permits(
                                EnumSet.of(CryptoPrimitive.KEY_AGREEMENT),
                                peerPublicKey)) {
                    throw new SSLHandshakeException(
                        "ECPublicKey does not comply to algorithm constraints");
                }

                shc.handshakeCredentials.add(new SM2ECredentials(
                        peerPublicKey, namedGroup));
            } catch (GeneralSecurityException e) {
                throw (SSLHandshakeException)(new SSLHandshakeException(
                        "Could not generate ECPublicKey").initCause(e));
            }

            TLCP11Credentials tlcpCredentials = null;
            for (SSLCredentials sslCredentials : shc.handshakeCredentials) {
                if (sslCredentials instanceof TLCP11Credentials) {
                    tlcpCredentials = (TLCP11Credentials)sslCredentials;
                    break;
                }
            }

            // update the states
            SM2KeyAgreementParamSpec params = new SM2KeyAgreementParamSpec(
                    sm2ePossession.popEncPrivateKey,
                    sm2ePossession.popEncPublicKey,
                    (ECPublicKey) tlcpCredentials.popEncPublicKey,
                    true,
                    48);
            SSLKeyDerivation masterKD = ke.createKeyDerivation(shc);
            SecretKey masterSecret =
                    masterKD.deriveKey("MasterSecret", params);
            shc.handshakeSession.setMasterSecret(masterSecret);

            SSLTrafficKeyDerivation kd =
                    SSLTrafficKeyDerivation.valueOf(shc.negotiatedProtocol);
            if (kd == null) {
                // unlikely
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                    "Not supported key derivation: " + shc.negotiatedProtocol);
            } else {
                shc.handshakeKeyDerivation =
                    kd.createKeyDerivation(shc, masterSecret);
            }
        }
    }
}
