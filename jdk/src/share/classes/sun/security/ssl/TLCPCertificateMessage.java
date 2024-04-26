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

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.PublicKey;
import java.security.cert.CertPathValidatorException;
import java.security.cert.CertPathValidatorException.BasicReason;
import java.security.cert.CertPathValidatorException.Reason;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.CertificateParsingException;
import java.security.cert.X509Certificate;
import java.text.MessageFormat;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLProtocolException;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.X509ExtendedTrustManager;
import javax.net.ssl.X509TrustManager;
import javax.security.auth.x500.X500Principal;
import sun.security.ssl.SSLHandshake.HandshakeMessage;
import sun.security.ssl.TLCPAuthentication.TLCP11Credentials;
import sun.security.ssl.TLCPAuthentication.TLCP11Possession;
import sun.security.util.SMUtil;

public class TLCPCertificateMessage {

    static final SSLConsumer tlcp11HandshakeConsumer =
        new TLCP11CertificateConsumer();
    static final HandshakeProducer tlcp11HandshakeProducer =
        new TLCP11CertificateProducer();

    private enum CertListFormat {

        // sign_cert | enc_cert | ca_list, the default
        SIGN_ENC_CA("SIGN|ENC|CA"),

        // sign_cert | ca_list | enc_cert
        SIGN_CA_ENC("SIGN|CA|ENC");

        private final String format;

        CertListFormat(String format) {
            this.format = format;
        }

        static CertListFormat format(String format) {
            return SIGN_CA_ENC.format.equalsIgnoreCase(format)
                    ? SIGN_CA_ENC : SIGN_ENC_CA;
        }
    }

    // The default format is SIGN_ENC_CA("SIGN|ENC|CA").
    private static final CertListFormat DEF_CERT_LIST_FORMAT
            = CertListFormat.format(AccessController.doPrivileged(
                    new PrivilegedAction<String>() {
                        public String run() {
                            return System.getProperty("jdk.tlcp.certListFormat",
                                    CertListFormat.SIGN_ENC_CA.format);
                        }
                    }));

    /**
     * The Certificate handshake message for TLCP 1.1.
     */
    static final class TLCP11CertificateMessage extends HandshakeMessage {
        final List<byte[]> encodedCertChain;

        TLCP11CertificateMessage(HandshakeContext handshakeContext,
                X509Certificate[] certChain) throws SSLException {
            super(handshakeContext);

            List<byte[]> encodedCerts = new ArrayList<>(certChain.length);
            for (X509Certificate cert : certChain) {
                try {
                    encodedCerts.add(cert.getEncoded());
                } catch (CertificateEncodingException cee) {
                    // unlikely
                    throw handshakeContext.conContext.fatal(
                            Alert.INTERNAL_ERROR,
                            "Could not encode certificate (" +
                            cert.getSubjectX500Principal() + ")", cee);
                }
            }

            this.encodedCertChain = encodedCerts;
        }

        TLCP11CertificateMessage(HandshakeContext handshakeContext,
                ByteBuffer m) throws IOException {
            super(handshakeContext);

            int listLen = Record.getInt24(m);
            if (listLen > m.remaining()) {
                throw handshakeContext.conContext.fatal(
                    Alert.ILLEGAL_PARAMETER,
                    "Error parsing certificate message:no sufficient data");
            }
            if (listLen > 0) {
                List<byte[]> encodedCerts = new LinkedList<>();
                while (listLen > 0) {
                    byte[] encodedCert = Record.getBytes24(m);
                    listLen -= (3 + encodedCert.length);
                    encodedCerts.add(encodedCert);
                    if (encodedCerts.size() > SSLConfiguration.maxCertificateChainLength) {
                        throw new SSLProtocolException(
                                "The certificate chain length ("
                                + encodedCerts.size()
                                + ") exceeds the maximum allowed length ("
                                + SSLConfiguration.maxCertificateChainLength
                                + ")");
                    }

                }
                this.encodedCertChain = encodedCerts;
            } else {
                this.encodedCertChain = Collections.emptyList();
            }
        }

        @Override
        public SSLHandshake handshakeType() {
            return SSLHandshake.CERTIFICATE;
        }

        @Override
        public int messageLength() {
            int msgLen = 3;
            for (byte[] encodedCert : encodedCertChain) {
                msgLen += (encodedCert.length + 3);
            }

            return msgLen;
        }

        @Override
        public void send(HandshakeOutStream hos) throws IOException {
            int listLen = 0;
            for (byte[] encodedCert : encodedCertChain) {
                listLen += (encodedCert.length + 3);
            }

            hos.putInt24(listLen);
            for (byte[] encodedCert : encodedCertChain) {
                hos.putBytes24(encodedCert);
            }
        }

        @Override
        public String toString() {
            if (encodedCertChain.isEmpty()) {
                return "\"Certificates\": <empty list>";
            }

            Object[] x509Certs = new Object[encodedCertChain.size()];
            try {
                CertificateFactory cf = CertificateFactory.getInstance("X.509");
                int i = 0;
                for (byte[] encodedCert : encodedCertChain) {
                    Object obj;
                    try {
                        obj = (X509Certificate)cf.generateCertificate(
                                    new ByteArrayInputStream(encodedCert));
                    } catch (CertificateException ce) {
                        obj = encodedCert;
                    }
                    x509Certs[i++] = obj;
                }
            } catch (CertificateException ce) {
                // no X.509 certificate factory service
                int i = 0;
                for (byte[] encodedCert : encodedCertChain) {
                    x509Certs[i++] = encodedCert;
                }
            }

            MessageFormat messageFormat = new MessageFormat(
                    "\"Certificates\": [\n" +
                    "{0}\n" +
                    "]",
                    Locale.ENGLISH);
            Object[] messageFields = {
                SSLLogger.toString(x509Certs)
            };

            return messageFormat.format(messageFields);
        }
    }

    /**
     * The "Certificate" handshake message producer for TLCP 1.1.
     */
    private static final
            class TLCP11CertificateProducer implements HandshakeProducer {
        // Prevent instantiation of this class.
        private TLCP11CertificateProducer() {
            // blank
        }

        @Override
        public byte[] produce(ConnectionContext context,
                HandshakeMessage message) throws IOException {
            // The producing happens in handshake context only.
            HandshakeContext hc = (HandshakeContext)context;
            if (hc.sslConfig.isClientMode) {
                return onProduceCertificate(
                        (ClientHandshakeContext)context, message);
            } else {
                return onProduceCertificate(
                        (ServerHandshakeContext)context, message);
            }
        }

        private byte[] onProduceCertificate(ServerHandshakeContext shc,
                SSLHandshake.HandshakeMessage message) throws IOException {
            TLCP11Possession tlcpPossession = null;
            for (SSLPossession possession : shc.handshakePossessions) {
                if (possession instanceof TLCP11Possession) {
                    tlcpPossession = (TLCP11Possession)possession;
                    break;
                }
            }

            if (tlcpPossession == null) {       // unlikely
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "No expected X.509 certificate for server authentication");
            }

            shc.handshakeSession.setLocalPrivateKey(
                    tlcpPossession.popSignPrivateKey);

            X509Certificate[] certs = mergeCertChains(
                    tlcpPossession.popSignCerts, tlcpPossession.popEncCerts);
            shc.handshakeSession.setLocalCertificates(certs);
            TLCP11CertificateMessage cm =
                    new TLCP11CertificateMessage(shc, certs);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                        "Produced server Certificate handshake message", cm);
            }

            // Output the handshake message.
            cm.write(shc.handshakeOutput);
            shc.handshakeOutput.flush();

            // The handshake message has been delivered.
            return null;
        }

        private byte[] onProduceCertificate(ClientHandshakeContext chc,
                SSLHandshake.HandshakeMessage message) throws IOException {
            TLCP11Possession tlcpPossession = null;
            for (SSLPossession possession : chc.handshakePossessions) {
                if (possession instanceof TLCP11Possession) {
                    tlcpPossession = (TLCP11Possession)possession;
                    break;
                }
            }

            // Report to the server if no appropriate cert was found.  For
            // SSL 3.0, send a no_certificate alert;  TLCP and TLS 1.0/1.1/1.2
            // uses an empty cert chain instead.
            if (tlcpPossession == null) {
                if (chc.negotiatedProtocol.isTLCP11()
                        || chc.negotiatedProtocol.useTLS10PlusSpec()) {
                    if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                        SSLLogger.fine(
                                "No X.509 certificate for client authentication, " +
                                        "use empty Certificate message instead");
                    }

                    tlcpPossession = new TLCP11Possession(
                            null, new X509Certificate[0],
                            null, new X509Certificate[0]);
                } else {
                    if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                        SSLLogger.fine(
                                "No X.509 certificate for client authentication, " +
                                        "send a no_certificate alert");
                    }

                    chc.conContext.warning(Alert.NO_CERTIFICATE);
                    return null;
                }
            }

            chc.handshakeSession.setLocalPrivateKey(
                    tlcpPossession.popSignPrivateKey);
            X509Certificate[] certs = mergeCertChains(
                    tlcpPossession.popSignCerts, tlcpPossession.popEncCerts);
            if (certs != null && certs.length != 0) {
                chc.handshakeSession.setLocalCertificates(certs);
            } else {
                chc.handshakeSession.setLocalCertificates(null);
            }
            TLCP11CertificateMessage cm =
                    new TLCP11CertificateMessage(chc, certs);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                        "Produced client Certificate handshake message", cm);
            }

            // Output the handshake message.
            cm.write(chc.handshakeOutput);
            chc.handshakeOutput.flush();

            // The handshake message has been delivered.
            return null;
        }
    }

    // Merge the sign cert chain and enc cert chain to a single chain.
    private static X509Certificate[] mergeCertChains(
            X509Certificate[] signCertChain, X509Certificate[] encCertChain) {
        if (signCertChain == null || signCertChain.length == 0
                || encCertChain == null || encCertChain.length == 0) {
            return new X509Certificate[0];
        }

        X509Certificate[] mergedCerts
                = new X509Certificate[signCertChain.length + 1];

        if (DEF_CERT_LIST_FORMAT == CertListFormat.SIGN_CA_ENC) {
            System.arraycopy(signCertChain, 0, mergedCerts, 0,
                    signCertChain.length);
            mergedCerts[mergedCerts.length - 1] = encCertChain[0];
        } else {
            mergedCerts[0] = signCertChain[0];
            mergedCerts[1] = encCertChain[0];
            if (signCertChain.length > 1) {
                System.arraycopy(signCertChain, 1, mergedCerts, 2,
                        signCertChain.length - 1);
            }
        }

        return mergedCerts;
    }

    /**
     * The "Certificate" handshake message consumer for TLCP 1.1.
     */
    static final
            class TLCP11CertificateConsumer implements SSLConsumer {
        // Prevent instantiation of this class.
        private TLCP11CertificateConsumer() {
            // blank
        }

        @Override
        public void consume(ConnectionContext context,
                ByteBuffer message) throws IOException {
            // The consuming happens in handshake context only.
            HandshakeContext hc = (HandshakeContext)context;

            // clean up this consumer
            hc.handshakeConsumers.remove(SSLHandshake.CERTIFICATE.id);

            TLCP11CertificateMessage cm = new TLCP11CertificateMessage(hc, message);
            if (hc.sslConfig.isClientMode) {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine(
                        "Consuming server Certificate handshake message", cm);
                }
                onCertificate((ClientHandshakeContext)context, cm);
            } else {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine(
                        "Consuming client Certificate handshake message", cm);
                }
                onCertificate((ServerHandshakeContext)context, cm);
            }
        }

        private void onCertificate(ServerHandshakeContext shc,
                TLCP11CertificateMessage certificateMessage )throws IOException {
            List<byte[]> encodedCerts = certificateMessage.encodedCertChain;
            if (encodedCerts == null || encodedCerts.isEmpty()) {
                // For empty Certificate messages, we should not expect
                // a CertificateVerify message to follow
                shc.handshakeConsumers.remove(
                        SSLHandshake.CERTIFICATE_VERIFY.id);
                if (shc.sslConfig.clientAuthType !=
                        ClientAuthType.CLIENT_AUTH_REQUESTED) {
                    // unexpected or require client authentication
                    throw shc.conContext.fatal(Alert.BAD_CERTIFICATE,
                            "Empty client certificate chain");
                } else {
                    return;
                }
            }

            X509Certificate[] x509Certs =
                    new X509Certificate[encodedCerts.size()];
            try {
                CertificateFactory cf = CertificateFactory.getInstance("X.509");
                int i = 0;
                for (byte[] encodedCert : encodedCerts) {
                    x509Certs[i++] = (X509Certificate)cf.generateCertificate(
                            new ByteArrayInputStream(encodedCert));
                }
            } catch (CertificateException ce) {
                throw shc.conContext.fatal(Alert.BAD_CERTIFICATE,
                        "Failed to parse server certificates", ce);
            }

            checkClientCerts(shc, x509Certs);

            //
            // update
            //
            shc.handshakeCredentials.add(createCredentials(x509Certs, shc));
            shc.handshakeSession.setPeerCertificates(x509Certs);
        }

        private void onCertificate(ClientHandshakeContext chc,
                TLCP11CertificateMessage certificateMessage) throws IOException {
            List<byte[]> encodedCerts = certificateMessage.encodedCertChain;
            if (encodedCerts == null || encodedCerts.isEmpty()) {
                throw chc.conContext.fatal(Alert.BAD_CERTIFICATE,
                        "Empty server certificate chain");
            }

            X509Certificate[] x509Certs =
                    new X509Certificate[encodedCerts.size()];

            if (x509Certs.length < 2) {
                throw chc.conContext.fatal(Alert.BAD_CERTIFICATE,
                            "Server must send at least two certificates");
            }

            try {
                CertificateFactory cf = CertificateFactory.getInstance("X.509");
                int i = 0;
                for (byte[] encodedCert : encodedCerts) {
                    x509Certs[i++] = (X509Certificate)cf.generateCertificate(
                            new ByteArrayInputStream(encodedCert));
                }
            } catch (CertificateException ce) {
                throw chc.conContext.fatal(Alert.BAD_CERTIFICATE,
                        "Failed to parse server certificates", ce);
            }

            // Allow server certificate change in client side during
            // renegotiation after a session-resumption abbreviated
            // initial handshake?
            //
            // DO NOT need to check allowUnsafeServerCertChange here. We only
            // reserve server certificates when allowUnsafeServerCertChange is
            // false.
            if (chc.reservedServerCerts != null &&
                    !chc.handshakeSession.useExtendedMasterSecret) {
                // It is not necessary to check the certificate update if
                // endpoint identification is enabled.
                String identityAlg = chc.sslConfig.identificationProtocol;
                if ((identityAlg == null || identityAlg.isEmpty()) &&
                        !isIdentityEquivalent(x509Certs[0],
                                chc.reservedServerCerts[0])) {
                    throw chc.conContext.fatal(Alert.BAD_CERTIFICATE,
                            "server certificate change is restricted " +
                                    "during renegotiation");
                }
            }

            // ask the trust manager to verify the chain
            if (chc.staplingActive) {
                // Defer the certificate check until after we've received the
                // CertificateStatus message.  If that message doesn't come in
                // immediately following this message we will execute the
                // check from CertificateStatus' absent handler.
                chc.deferredCerts = x509Certs;
            } else {
                // We're not doing stapling, so perform the check right now
                checkServerCerts(chc, x509Certs);
            }

            //
            // update
            //
            chc.handshakeCredentials.add(createCredentials(x509Certs, chc));
            chc.handshakeSession.setPeerCertificates(x509Certs);
        }

        // Assume the cert list contains at least one certificate,
        // and the structure is either of:
        // 1. sign_cert | enc_cert | sign_cert_CA_list
        // 2. sign_cert | sign_cert_CA_list | enc_cert
        //
        // sign_cert and enc_cert are issued by the same CA.
        private static TLCP11Credentials createCredentials(
                X509Certificate[] certs, HandshakeContext hc) throws SSLException {
            X509Certificate signCert = null;
            X509Certificate encCert = null;
            X509Certificate[] signCertChain = null;
            X509Certificate[] encCertChain = null;

            if (certs.length == 1) {
                signCert = certs[0];
                encCert = certs[0];
                signCertChain = new X509Certificate[] { signCert };
                encCertChain = new X509Certificate[] { encCert };
            } else if (certs.length == 2) {
                signCert = certs[0];
                encCert = certs[1];
                signCertChain = new X509Certificate[] { signCert };
                encCertChain = new X509Certificate[] { encCert };
            } else if (certs.length > 2) {
                CertListFormat format = null;
                if (SMUtil.isCA(certs[certs.length - 1])) {
                    format = CertListFormat.SIGN_ENC_CA;
                } else if (SMUtil.isCA(certs[1])) {
                    format = CertListFormat.SIGN_CA_ENC;
                } else {
                    // Cannot determine the CA, just use the default format
                    format = DEF_CERT_LIST_FORMAT;
                }

                signCertChain = new X509Certificate[certs.length - 1];
                encCertChain = new X509Certificate[certs.length - 1];
                if (format == CertListFormat.SIGN_CA_ENC) {
                    signCert = certs[0];
                    System.arraycopy(certs, 0, signCertChain, 0, certs.length - 1);

                    encCert = certs[certs.length - 1];
                    encCertChain[0] = encCert;
                    System.arraycopy(certs, 1, encCertChain, 1, certs.length - 2);
                } else { // SIGN_ENC_CA
                    signCert = certs[0];
                    signCertChain[0] = signCert;
                    System.arraycopy(certs, 2, signCertChain, 1, certs.length - 2);

                    encCert = certs[1];
                    encCertChain[0] = encCert;
                    System.arraycopy(certs, 2, encCertChain, 1, certs.length - 2);
                }
            }

            // Check sign cert
            if (!SMUtil.isSignCert(signCert)) {
                throw hc.conContext.fatal(Alert.BAD_CERTIFICATE,
                        "The sign cert doesn't contain key usage digitalSignature");
            }

            // Check enc cert
            if (!SMUtil.isEncCert(encCert)) {
                throw hc.conContext.fatal(Alert.BAD_CERTIFICATE,
                        "The enc cert doesn't contain key usages in " +
                        "keyEncipherment, dataEncipherment and keyAgreement.");
            }

            return new TLCP11Credentials(
                    signCert.getPublicKey(), signCertChain,
                    encCert.getPublicKey(), encCertChain);
        }

        /*
         * Whether the certificates can represent the same identity?
         *
         * The certificates can be used to represent the same identity:
         *     1. If the subject alternative names of IP address are present
         *        in both certificates, they should be identical; otherwise,
         *     2. if the subject alternative names of DNS name are present in
         *        both certificates, they should be identical; otherwise,
         *     3. if the subject fields are present in both certificates, the
         *        certificate subjects and issuers should be identical.
         */
        private static boolean isIdentityEquivalent(X509Certificate thisCert,
                X509Certificate prevCert) {
            if (thisCert.equals(prevCert)) {
                return true;
            }

            // check subject alternative names
            Collection<List<?>> thisSubjectAltNames = null;
            try {
                thisSubjectAltNames = thisCert.getSubjectAlternativeNames();
            } catch (CertificateParsingException cpe) {
                if (SSLLogger.isOn && SSLLogger.isOn("handshake")) {
                    SSLLogger.fine(
                        "Attempt to obtain subjectAltNames extension failed!");
                }
            }

            Collection<List<?>> prevSubjectAltNames = null;
            try {
                prevSubjectAltNames = prevCert.getSubjectAlternativeNames();
            } catch (CertificateParsingException cpe) {
                if (SSLLogger.isOn && SSLLogger.isOn("handshake")) {
                    SSLLogger.fine(
                        "Attempt to obtain subjectAltNames extension failed!");
                }
            }

            if (thisSubjectAltNames != null && prevSubjectAltNames != null) {
                // check the iPAddress field in subjectAltName extension
                //
                // 7: subject alternative name of type IP.
                Collection<String> thisSubAltIPAddrs =
                            getSubjectAltNames(thisSubjectAltNames, 7);
                Collection<String> prevSubAltIPAddrs =
                            getSubjectAltNames(prevSubjectAltNames, 7);
                if (thisSubAltIPAddrs != null && prevSubAltIPAddrs != null &&
                    isEquivalent(thisSubAltIPAddrs, prevSubAltIPAddrs)) {
                    return true;
                }

                // check the dNSName field in subjectAltName extension
                // 2: subject alternative name of type IP.
                Collection<String> thisSubAltDnsNames =
                            getSubjectAltNames(thisSubjectAltNames, 2);
                Collection<String> prevSubAltDnsNames =
                            getSubjectAltNames(prevSubjectAltNames, 2);
                if (thisSubAltDnsNames != null && prevSubAltDnsNames != null &&
                    isEquivalent(thisSubAltDnsNames, prevSubAltDnsNames)) {
                    return true;
                }
            }

            // check the certificate subject and issuer
            X500Principal thisSubject = thisCert.getSubjectX500Principal();
            X500Principal prevSubject = prevCert.getSubjectX500Principal();
            X500Principal thisIssuer = thisCert.getIssuerX500Principal();
            X500Principal prevIssuer = prevCert.getIssuerX500Principal();

            return (!thisSubject.getName().isEmpty() &&
                    !prevSubject.getName().isEmpty() &&
                    thisSubject.equals(prevSubject) &&
                    thisIssuer.equals(prevIssuer));
        }

        /*
         * Returns the subject alternative name of the specified type in the
         * subjectAltNames extension of a certificate.
         *
         * Note that only those subjectAltName types that use String data
         * should be passed into this function.
         */
        private static Collection<String> getSubjectAltNames(
                Collection<List<?>> subjectAltNames, int type) {
            HashSet<String> subAltDnsNames = null;
            for (List<?> subjectAltName : subjectAltNames) {
                int subjectAltNameType = (Integer)subjectAltName.get(0);
                if (subjectAltNameType == type) {
                    String subAltDnsName = (String)subjectAltName.get(1);
                    if ((subAltDnsName != null) && !subAltDnsName.isEmpty()) {
                        if (subAltDnsNames == null) {
                            subAltDnsNames =
                                    new HashSet<>(subjectAltNames.size());
                        }
                        subAltDnsNames.add(subAltDnsName);
                    }
                }
            }

            return subAltDnsNames;
        }

        private static boolean isEquivalent(Collection<String> thisSubAltNames,
                Collection<String> prevSubAltNames) {
            for (String thisSubAltName : thisSubAltNames) {
                for (String prevSubAltName : prevSubAltNames) {
                    // Only allow the exactly match.  No wildcard character
                    // checking.
                    if (thisSubAltName.equalsIgnoreCase(prevSubAltName)) {
                        return true;
                    }
                }
            }

            return false;
        }

        /**
         * Perform client-side checking of server certificates.
         *
         * @param certs an array of {@code X509Certificate} objects presented
         *      by the server in the ServerCertificate message.
         *
         * @throws IOException if a failure occurs during validation or
         *      the trust manager associated with the {@code SSLContext} is not
         *      an {@code X509ExtendedTrustManager}.
         */
        static void checkServerCerts(ClientHandshakeContext chc,
                X509Certificate[] certs) throws IOException {

            X509TrustManager tm = chc.sslContext.getX509TrustManager();

            // find out the key exchange algorithm used
            // use "RSA" for non-ephemeral "RSA_EXPORT"
            String keyExchangeString;
            if (chc.negotiatedCipherSuite.keyExchange ==
                    CipherSuite.KeyExchange.K_RSA_EXPORT ||
                    chc.negotiatedCipherSuite.keyExchange ==
                            CipherSuite.KeyExchange.K_DHE_RSA_EXPORT) {
                keyExchangeString = CipherSuite.KeyExchange.K_RSA.name;
            } else {
                keyExchangeString = chc.negotiatedCipherSuite.keyExchange.name;
            }

            try {
                if (tm instanceof X509ExtendedTrustManager) {
                    if (chc.conContext.transport instanceof SSLEngine) {
                        SSLEngine engine = (SSLEngine)chc.conContext.transport;
                        ((X509ExtendedTrustManager)tm).checkServerTrusted(
                            certs.clone(),
                            keyExchangeString,
                            engine);
                    } else {
                        SSLSocket socket = (SSLSocket)chc.conContext.transport;
                        ((X509ExtendedTrustManager)tm).checkServerTrusted(
                            certs.clone(),
                            keyExchangeString,
                            socket);
                    }
                } else {
                    // Unlikely to happen, because we have wrapped the old
                    // X509TrustManager with the new X509ExtendedTrustManager.
                    throw new CertificateException(
                            "Improper X509TrustManager implementation");
                }

                // Once the server certificate chain has been validated, set
                // the certificate chain in the TLS session.
                chc.handshakeSession.setPeerCertificates(certs);
            } catch (CertificateException ce) {
                throw chc.conContext.fatal(getCertificateAlert(chc, ce), ce);
            }
        }

        private static void checkClientCerts(ServerHandshakeContext shc,
                X509Certificate[] certs) throws IOException {
            X509TrustManager tm = shc.sslContext.getX509TrustManager();

            // find out the types of client authentication used
            PublicKey key = certs[0].getPublicKey();
            String keyAlgorithm = key.getAlgorithm();
            String authType;
            switch (keyAlgorithm) {
                case "RSA":
                case "DSA":
                case "EC":
                case "RSASSA-PSS":
                    authType = keyAlgorithm;
                    break;
                default:
                    // unknown public key type
                    authType = "UNKNOWN";
            }

            try {
                if (tm instanceof X509ExtendedTrustManager) {
                    if (shc.conContext.transport instanceof SSLEngine) {
                        SSLEngine engine = (SSLEngine)shc.conContext.transport;
                        ((X509ExtendedTrustManager)tm).checkClientTrusted(
                            certs.clone(),
                            authType,
                            engine);
                    } else {
                        SSLSocket socket = (SSLSocket)shc.conContext.transport;
                        ((X509ExtendedTrustManager)tm).checkClientTrusted(
                            certs.clone(),
                            authType,
                            socket);
                    }
                } else {
                    // Unlikely to happen, because we have wrapped the old
                    // X509TrustManager with the new X509ExtendedTrustManager.
                    throw new CertificateException(
                            "Improper X509TrustManager implementation");
                }
            } catch (CertificateException ce) {
                throw shc.conContext.fatal(Alert.CERTIFICATE_UNKNOWN, ce);
            }
        }

        /**
         * When a failure happens during certificate checking from an
         * {@link X509TrustManager}, determine what TLS alert description
         * to use.
         *
         * @param cexc The exception thrown by the {@link X509TrustManager}
         *
         * @return A byte value corresponding to a TLS alert description number.
         */
        private static Alert getCertificateAlert(
                ClientHandshakeContext chc, CertificateException cexc) {
            // The specific reason for the failure will determine how to
            // set the alert description value
            Alert alert = Alert.CERTIFICATE_UNKNOWN;

            Throwable baseCause = cexc.getCause();
            if (baseCause instanceof CertPathValidatorException) {
                CertPathValidatorException cpve =
                        (CertPathValidatorException)baseCause;
                Reason reason = cpve.getReason();
                if (reason == BasicReason.REVOKED) {
                    alert = chc.staplingActive ?
                            Alert.BAD_CERT_STATUS_RESPONSE :
                            Alert.CERTIFICATE_REVOKED;
                } else if (
                        reason == BasicReason.UNDETERMINED_REVOCATION_STATUS) {
                    alert = chc.staplingActive ?
                            Alert.BAD_CERT_STATUS_RESPONSE :
                            Alert.CERTIFICATE_UNKNOWN;
                } else if (reason == BasicReason.ALGORITHM_CONSTRAINED) {
                    alert = Alert.UNSUPPORTED_CERTIFICATE;
                } else if (reason == BasicReason.EXPIRED) {
                    alert = Alert.CERTIFICATE_EXPIRED;
                } else if (reason == BasicReason.INVALID_SIGNATURE ||
                        reason == BasicReason.NOT_YET_VALID) {
                    alert = Alert.BAD_CERTIFICATE;
                }
            }

            return alert;
        }

    }
}
