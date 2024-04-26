/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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
import java.text.MessageFormat;
import java.util.Locale;

import sun.misc.HexDumpEncoder;
import sun.security.ssl.SSLHandshake.HandshakeMessage;
import sun.security.ssl.TLCPAuthentication.TLCP11Credentials;
import sun.security.ssl.TLCPAuthentication.TLCP11Possession;
import sun.security.util.SMUtil;

final class TLCPCertificateVerify {

    static final SSLConsumer tlcp11HandshakeConsumer =
        new TLCP11CertificateVerifyConsumer();
    static final HandshakeProducer tlcp11HandshakeProducer =
        new TLCP11CertificateVerifyProducer();

    /**
     * The CertificateVerify handshake message (TLCP 1.1).
     */
    private static final
            class TLCP11CertificateVerifyMessage extends HandshakeMessage {

        // signature bytes
        private final byte[] signature;

        TLCP11CertificateVerifyMessage(HandshakeContext context,
                TLCP11Possession tlcpPossession) throws IOException {
            super(context);

            // This happens in client side only.
            ClientHandshakeContext chc = (ClientHandshakeContext) context;

            byte[] temporary;
            try {
                Signature signer = SignatureScheme.SM2SIG_SM3.getSigner(
                        tlcpPossession.popSignPrivateKey,
                        tlcpPossession.popSignPublicKey,
                        false);
                signer.update(chc.handshakeHash.digest());
                temporary = signer.sign();
            } catch (SignatureException se) {
                throw chc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "Cannot produce CertificateVerify signature", se);
            }

            this.signature = temporary;
        }

        TLCP11CertificateVerifyMessage(HandshakeContext handshakeContext,
                ByteBuffer m) throws IOException {
            super(handshakeContext);

            // This happens in server side only.
            ServerHandshakeContext shc =
                    (ServerHandshakeContext) handshakeContext;

            // struct {
            //     SignatureAndHashAlgorithm algorithm;
            //     opaque signature<0..2^16-1>;
            // } DigitallySigned;

            if (m.remaining() < 2) {
                throw shc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                        "Invalid CertificateVerify message: no sufficient data");
            }

            // read and verify the signature
            TLCP11Credentials tlcpCredentials = null;
            for (SSLCredentials cd : shc.handshakeCredentials) {
                if (cd instanceof TLCP11Credentials) {
                    tlcpCredentials = (TLCP11Credentials) cd;
                    break;
                }
            }

            if (tlcpCredentials == null ||
                    tlcpCredentials.popSignPublicKey == null) {
                throw shc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "No X509 credentials negotiated for CertificateVerify");
            }

            // opaque signature<0..2^16-1>;
            this.signature = Record.getBytes16(m);

            if (!(SMUtil.isSMCert(tlcpCredentials.popSignCert))) {
                throw shc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "Only support SM certificate");
            }

            try {
                Signature signer = SignatureScheme.SM2SIG_SM3.getVerifier(
                        tlcpCredentials.popSignPublicKey);

                signer.update(shc.handshakeHash.digest());
                if (!signer.verify(signature)) {
                    throw shc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                            "Invalid CertificateVerify signature");
                }
            } catch (NoSuchAlgorithmException |
                     InvalidAlgorithmParameterException nsae) {
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Unsupported signature algorithm (sm2sig_sm3) " +
                                "used in CertificateVerify handshake message", nsae);
            } catch (InvalidKeyException | SignatureException ikse) {
                throw shc.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "Cannot verify CertificateVerify signature", ikse);
            }
        }

        @Override
        public SSLHandshake handshakeType() {
            return SSLHandshake.CERTIFICATE_VERIFY;
        }

        @Override
        public int messageLength() {
            // It isn't (4 + signature.length) due to no signature scheme.
            return 2 + signature.length;
        }

        @Override
        public void send(HandshakeOutStream hos) throws IOException {
            hos.putBytes16(signature);
        }

        @Override
        public String toString() {
            MessageFormat messageFormat = new MessageFormat(
                    "\"CertificateVerify\": '{'\n" +
                    "  \"signature algorithm\": sm2sig_sm3\n" +
                    "  \"signature\": '{'\n" +
                    "{0}\n" +
                    "  '}'\n" +
                    "'}'",
                    Locale.ENGLISH);

            HexDumpEncoder hexEncoder = new HexDumpEncoder();
            Object[] messageFields = {
                Utilities.indent(
                        hexEncoder.encodeBuffer(signature), "    ")
            };

            return messageFormat.format(messageFields);
        }
    }

    /**
     * The "CertificateVerify" handshake message producer.
     */
    private static final
            class TLCP11CertificateVerifyProducer implements HandshakeProducer {
        // Prevent instantiation of this class.
        private TLCP11CertificateVerifyProducer() {
            // blank
        }

        @Override
        public byte[] produce(ConnectionContext context,
                SSLHandshake.HandshakeMessage message) throws IOException {
            // The producing happens in client side only.
            ClientHandshakeContext chc = (ClientHandshakeContext)context;

            TLCP11Possession tlcpPossession = null;
            for (SSLPossession possession : chc.handshakePossessions) {
                if (possession instanceof TLCP11Possession) {
                    tlcpPossession = (TLCP11Possession)possession;
                    break;
                }
            }

            if (tlcpPossession == null ||
                    tlcpPossession.popSignPrivateKey == null) {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine(
                        "No X.509 credentials negotiated for CertificateVerify");
                }

                return null;
            }

            TLCP11CertificateVerifyMessage cvm =
                    new TLCP11CertificateVerifyMessage(chc, tlcpPossession);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                        "Produced CertificateVerify handshake message", cvm);
            }

            // Output the handshake message.
            cvm.write(chc.handshakeOutput);
            chc.handshakeOutput.flush();

            // The handshake message has been delivered.
            return null;
        }
    }

    /**
     * The "CertificateVerify" handshake message consumer.
     */
    private static final
            class TLCP11CertificateVerifyConsumer implements SSLConsumer {
        // Prevent instantiation of this class.
        private TLCP11CertificateVerifyConsumer() {
            // blank
        }

        @Override
        public void consume(ConnectionContext context,
                ByteBuffer message) throws IOException {
            // The consuming happens in server side only.
            ServerHandshakeContext shc = (ServerHandshakeContext)context;

            // Clean up this consumer
            shc.handshakeConsumers.remove(SSLHandshake.CERTIFICATE_VERIFY.id);

            // Ensure that the CV message follows the CKE
            if (shc.handshakeConsumers.containsKey(
                    SSLHandshake.CLIENT_KEY_EXCHANGE.id)) {
                throw shc.conContext.fatal(Alert.UNEXPECTED_MESSAGE,
                        "Unexpected CertificateVerify handshake message");
            }

            TLCP11CertificateVerifyMessage cvm =
                    new TLCP11CertificateVerifyMessage(shc, message);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                        "Consuming CertificateVerify handshake message", cvm);
            }

            //
            // update
            //
            // Need no additional validation.

            //
            // produce
            //
            // Need no new handshake message producers here.
        }
    }
}
