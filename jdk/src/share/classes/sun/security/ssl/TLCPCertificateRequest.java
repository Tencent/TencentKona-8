/*
 * Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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

import javax.security.auth.x500.X500Principal;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.security.cert.X509Certificate;
import java.text.MessageFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;

import sun.security.ssl.SSLHandshake.HandshakeMessage;

final class TLCPCertificateRequest {

    static final SSLConsumer tlcp11HandshakeConsumer =
        new TLCP11CertificateRequestConsumer();
    static final HandshakeProducer tlcp11HandshakeProducer =
        new TLCP11CertificateRequestProducer();

    /**
     * The CertificateRequest handshake message for TLCP 1.1.
     */
    static final class TLCP11CertificateRequestMessage extends HandshakeMessage {
        final byte[] types;                 // certificate types
        final List<byte[]> authorities;     // certificate authorities

        TLCP11CertificateRequestMessage(HandshakeContext handshakeContext,
                X509Certificate[] trustedCerts) throws IOException {
            super(handshakeContext);

            this.types = new byte[] {
                    CertificateRequest.ClientCertificateType.ECDSA_SIGN.id,
                    CertificateRequest.ClientCertificateType.RSA_SIGN.id};

            this.authorities = new ArrayList<>(trustedCerts.length);
            for (X509Certificate cert : trustedCerts) {
                X500Principal x500Principal = cert.getSubjectX500Principal();
                authorities.add(x500Principal.getEncoded());
            }
        }

        TLCP11CertificateRequestMessage(HandshakeContext handshakeContext,
                ByteBuffer m) throws IOException {
            super(handshakeContext);

            // struct {
            //     ClientCertificateType certificate_types<1..2^8-1>;
            //     SignatureAndHashAlgorithm
            //       supported_signature_algorithms<2..2^16-2>;
            //     DistinguishedName certificate_authorities<0..2^16-1>;
            // } CertificateRequest;

            // certificate_authorities
            int minLen = handshakeContext.negotiatedProtocol.isTLCP11() ? 4 : 8;
            if (m.remaining() < minLen) {
                throw handshakeContext.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                        "Invalid CertificateRequest handshake message: " +
                        "no sufficient data");
            }
            this.types = Record.getBytes8(m);

            // certificate_authorities
            if (m.remaining() < 2) {
                throw handshakeContext.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                        "Invalid CertificateRequest handshake message: " +
                        "no sufficient data");
            }

            int listLen = Record.getInt16(m);
            if (listLen > m.remaining()) {
                throw handshakeContext.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "Invalid CertificateRequest message: no sufficient data");
            }

            if (listLen > 0) {
                this.authorities = new LinkedList<>();
                while (listLen > 0) {
                    // opaque DistinguishedName<1..2^16-1>;
                    byte[] encoded = Record.getBytes16(m);
                    listLen -= (2 + encoded.length);
                    authorities.add(encoded);
                }
            } else {
                this.authorities = Collections.emptyList();
            }
        }

        String[] getKeyTypes() {
            return CertificateRequest.ClientCertificateType.getKeyTypes(types);
        }

        X500Principal[] getAuthorities() {
            X500Principal[] principals = new X500Principal[authorities.size()];
            int i = 0;
            for (byte[] encoded : authorities) {
                principals[i++] = new X500Principal(encoded);
            }

            return principals;
        }

        @Override
        public SSLHandshake handshakeType() {
            return SSLHandshake.CERTIFICATE_REQUEST;
        }

        @Override
        public int messageLength() {
            int len = 1 + types.length + 2;
            for (byte[] encoded : authorities) {
                len += encoded.length + 2;
            }
            return len;
        }

        @Override
        public void send(HandshakeOutStream hos) throws IOException {
            hos.putBytes8(types);

            int listLen = 0;
            for (byte[] encoded : authorities) {
                listLen += encoded.length + 2;
            }

            hos.putInt16(listLen);
            for (byte[] encoded : authorities) {
                hos.putBytes16(encoded);
            }
        }

        @Override
        public String toString() {
            MessageFormat messageFormat = new MessageFormat(
                    "\"CertificateRequest\": '{'\n" +
                    "  \"certificate types\": {0}\n" +
                    "  \"certificate authorities\": {1}\n" +
                    "'}'",
                    Locale.ENGLISH);

            List<String> typeNames = new ArrayList<>(types.length);
            for (byte type : types) {
                typeNames.add(CertificateRequest.ClientCertificateType.nameOf(type));
            }

            List<String> authorityNames = new ArrayList<>(authorities.size());
            for (byte[] encoded : authorities) {
                X500Principal principal = new X500Principal(encoded);
                authorityNames.add(principal.toString());
            }
            Object[] messageFields = {
                typeNames,
                authorityNames
            };

            return messageFormat.format(messageFields);
        }
    }

    /**
     * The "CertificateRequest" handshake message producer for TLCP 1.1.
     */
    private static final
            class TLCP11CertificateRequestProducer implements HandshakeProducer {
        // Prevent instantiation of this class.
        private TLCP11CertificateRequestProducer() {
            // blank
        }

        @Override
        public byte[] produce(ConnectionContext context,
                SSLHandshake.HandshakeMessage message) throws IOException {
            // The producing happens in server side only.
            ServerHandshakeContext shc = (ServerHandshakeContext)context;

            X509Certificate[] caCerts =
                    shc.sslContext.getX509TrustManager().getAcceptedIssuers();
            TLCP11CertificateRequestMessage crm
                    = new TLCP11CertificateRequestMessage(shc, caCerts);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Produced CertificateRequest handshake message", crm);
            }

            // Output the handshake message.
            crm.write(shc.handshakeOutput);
            shc.handshakeOutput.flush();

            //
            // update
            //
            shc.handshakeConsumers.put(SSLHandshake.CERTIFICATE.id,
                    SSLHandshake.CERTIFICATE);
            shc.handshakeConsumers.put(SSLHandshake.CERTIFICATE_VERIFY.id,
                    SSLHandshake.CERTIFICATE_VERIFY);

            // The handshake message has been delivered.
            return null;
        }
    }

    /**
     * The "CertificateRequest" handshake message consumer for TLCP 1.1.
     */
    private static final
            class TLCP11CertificateRequestConsumer implements SSLConsumer {
        // Prevent instantiation of this class.
        private TLCP11CertificateRequestConsumer() {
            // blank
        }

        @Override
        public void consume(ConnectionContext context,
                ByteBuffer message) throws IOException {
            // The consuming happens in client side only.
            ClientHandshakeContext chc = (ClientHandshakeContext)context;

            // clean up this consumer
            chc.handshakeConsumers.remove(SSLHandshake.CERTIFICATE_REQUEST.id);
            chc.receivedCertReq = true;

            // If we're processing this message and the server's certificate
            // message consumer has not already run then this is a state
            // machine violation.
            if (chc.handshakeConsumers.containsKey(
                    SSLHandshake.CERTIFICATE.id)) {
                throw chc.conContext.fatal(Alert.UNEXPECTED_MESSAGE,
                        "Unexpected CertificateRequest handshake message");
            }

            SSLConsumer certStatCons = chc.handshakeConsumers.remove(
                    SSLHandshake.CERTIFICATE_STATUS.id);
            if (certStatCons != null) {
                // Stapling was active but no certificate status message
                // was sent.  We need to run the absence handler which will
                // check the certificate chain.
                CertificateStatus.handshakeAbsence.absent(context, null);
            }

            TLCP11CertificateRequestMessage crm
                    = new TLCP11CertificateRequestMessage(chc, message);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                        "Consuming CertificateRequest handshake message", crm);
            }

            //
            // validate
            //
            // blank

            //
            // update
            //

            // An empty client Certificate handshake message may be allow.
            chc.handshakeProducers.put(SSLHandshake.CERTIFICATE.id,
                    SSLHandshake.CERTIFICATE);

            chc.peerSupportedAuthorities = crm.getAuthorities();

            SSLPossession pos = TLCPAuthentication.createPossession(
                    chc, new String[] {"EC"});
            if (pos == null) {
                return;
            }

            chc.handshakePossessions.add(pos);
            chc.handshakeProducers.put(SSLHandshake.CERTIFICATE_VERIFY.id,
                    SSLHandshake.CERTIFICATE_VERIFY);
        }
    }
}
