/*
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.
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

import javax.net.ssl.X509ExtendedKeyManager;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECParameterSpec;
import java.util.AbstractMap;
import java.util.Arrays;
import java.util.Map;

import sun.security.ssl.SupportedGroupsExtension.NamedGroup;
import sun.security.util.SMUtil;

enum TLCPAuthentication implements SSLAuthentication {

    SM2("EC", "EC");

    final String keyAlgorithm;
    final String[] keyTypes;

    TLCPAuthentication(String keyAlgorithm,
                       String... keyTypes) {
        this.keyAlgorithm = keyAlgorithm;
        this.keyTypes = keyTypes;
    }

    @Override
    public SSLPossession createPossession(HandshakeContext handshakeContext) {
        return createPossession(handshakeContext, keyTypes);
    }

    @Override
    public SSLHandshake[] getRelatedHandshakers(
            HandshakeContext handshakeContext) {
        if (handshakeContext.negotiatedProtocol.isTLCP11()) {
            return new SSLHandshake[] {
                    SSLHandshake.CERTIFICATE,
                    SSLHandshake.CERTIFICATE_REQUEST
            };
        }   // Otherwise, SSL/TLS does not use this method.

        return new SSLHandshake[0];
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    @Override
    public Map.Entry<Byte, HandshakeProducer>[] getHandshakeProducers(
            HandshakeContext handshakeContext) {
        if (handshakeContext.negotiatedProtocol.isTLCP11()) {
            return (Map.Entry<Byte, HandshakeProducer>[])(new Map.Entry[] {
                    new AbstractMap.SimpleImmutableEntry<Byte, HandshakeProducer>(
                            SSLHandshake.CERTIFICATE.id,
                            SSLHandshake.CERTIFICATE
                    )
            });
        }   // Otherwise, SSL/TLS does not use this method.

        return new Map.Entry[0];
    }

    static final class TLCP11Possession implements SSLPossession {

        // Proof of possession of the private key corresponding to the public
        // key for which a certificate is being provided for authentication.
        final PrivateKey popSignPrivateKey;
        final X509Certificate[] popSignCerts;
        final X509Certificate popSignCert;
        final PublicKey popSignPublicKey;

        final PrivateKey popEncPrivateKey;
        final X509Certificate[] popEncCerts;
        final X509Certificate popEncCert;
        final PublicKey popEncPublicKey;

        TLCP11Possession(PrivateKey popSignPrivateKey,
                       X509Certificate[] popSignCerts,
                       PrivateKey popEncPrivateKey,
                       X509Certificate[] popEncCerts) {
            this.popSignPrivateKey = popSignPrivateKey;
            this.popSignCerts = popSignCerts;
            if (popSignCerts != null && popSignCerts.length > 0) {
                popSignCert = popSignCerts[0];
                popSignPublicKey = popSignCert.getPublicKey();
            } else {
                popSignCert = null;
                popSignPublicKey = null;
            }

            this.popEncPrivateKey = popEncPrivateKey;
            this.popEncCerts = popEncCerts;
            if (popEncCerts != null && popEncCerts.length > 0) {
                popEncCert = popEncCerts[0];
                popEncPublicKey = popEncCert.getPublicKey();
            } else {
                popEncCert = null;
                popEncPublicKey = null;
            }
        }

        TLCP11Possession(PossessionEntry signPossEntry,
                       PossessionEntry encPossEntry) {
            this.popSignPrivateKey = signPossEntry.popPrivateKey;
            this.popSignCerts = signPossEntry.popCerts;
            this.popSignCert = signPossEntry.popCert;
            popSignPublicKey = signPossEntry.popPublicKey;

            this.popEncPrivateKey = encPossEntry.popPrivateKey;
            this.popEncCerts = encPossEntry.popCerts;
            this.popEncCert = encPossEntry.popCert;
            popEncPublicKey = encPossEntry.popPublicKey;
        }

        ECParameterSpec getECParameterSpec() {
            return getECParameterSpec(popSignPrivateKey, popSignCerts);
        }

        ECParameterSpec getECParameterSpec(
                PrivateKey popPrivateKey, X509Certificate[] popCerts) {
            if (popPrivateKey == null ||
                    !"EC".equals(popPrivateKey.getAlgorithm())) {
                return null;
            }

            if (popPrivateKey instanceof ECKey) {
                return ((ECKey) popPrivateKey).getParams();
            } else if (popCerts != null && popCerts.length != 0) {
                // The private key not extractable, get the parameters from
                // the X.509 certificate.
                PublicKey publicKey = popCerts[0].getPublicKey();
                if (publicKey instanceof ECKey) {
                    return ((ECKey)publicKey).getParams();
                }
            }

            return null;
        }
    }

    static final class PossessionEntry {

        final PrivateKey popPrivateKey;
        final X509Certificate[] popCerts;
        final X509Certificate popCert;
        final PublicKey popPublicKey;

        PossessionEntry(PrivateKey popPrivateKey,
                        X509Certificate[] popCerts) {
            this.popPrivateKey = popPrivateKey;
            this.popCerts = popCerts;
            popCert = popCerts[0];
            popPublicKey = popCert.getPublicKey();
        }
    }

    static final class TLCP11Credentials implements SSLCredentials {

        final PublicKey popSignPublicKey;
        final X509Certificate[] popSignCerts;
        final X509Certificate popSignCert;

        final PublicKey popEncPublicKey;
        final X509Certificate[] popEncCerts;
        final X509Certificate popEncCert;

        TLCP11Credentials(PublicKey popSignPublicKey,
                        X509Certificate[] popSignCerts,
                        PublicKey popEncPublicKey,
                        X509Certificate[] popEncCerts) {
            this.popSignPublicKey = popSignPublicKey;
            this.popSignCerts = popSignCerts;
            this.popSignCert = popSignCerts[0];

            this.popEncPublicKey = popEncPublicKey;
            this.popEncCerts = popEncCerts;
            this.popEncCert = popEncCerts[0];
        }
    }

    public static SSLPossession createPossession(
            HandshakeContext context, String[] keyTypes) {
        if (context.sslConfig.isClientMode) {
            return createClientPossession(
                    (ClientHandshakeContext) context, keyTypes);
        } else {
            return createServerPossession(
                    (ServerHandshakeContext) context, keyTypes);
        }
    }

    private static SSLPossession createClientPossession(
            ClientHandshakeContext chc, String[] keyTypes) {
        X509ExtendedKeyManager km = chc.sslContext.getX509KeyManager();
        for (String keyType : keyTypes) {
            String[] clientAliases = km.getClientAliases(
                        keyType,
                        chc.peerSupportedAuthorities == null ? null :
                                chc.peerSupportedAuthorities.clone());
            if (clientAliases == null || clientAliases.length == 0) {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                    SSLLogger.finest("No X.509 cert selected for "
                            + Arrays.toString(keyTypes));
                }
                return null;
            }

            PossessionEntry signPossEntry = null;
            PossessionEntry encPossEntry = null;
            for (String clientAlias : clientAliases) {
                PossessionEntry bufPossEntry = clientPossEntry(
                        chc, keyType, km, clientAlias);
                if (bufPossEntry == null) {
                    continue;
                }

                if (signPossEntry == null
                        && SMUtil.isSignCert(bufPossEntry.popCert)) {
                    signPossEntry = bufPossEntry;
                } else if (SMUtil.isEncCert(bufPossEntry.popCert)) {
                    encPossEntry = bufPossEntry;
                }

                if (signPossEntry != null && encPossEntry != null) {
                    break;
                }
            }

            TLCP11Possession tlcpPossession
                    = createPossession(signPossEntry, encPossEntry);
            if (tlcpPossession != null) {
                return tlcpPossession;
            }
        }

        return null;
    }

    private static PossessionEntry clientPossEntry(
            ClientHandshakeContext chc, String keyType,
            X509ExtendedKeyManager km, String clientAlias) {
        PrivateKey clientPrivateKey = km.getPrivateKey(clientAlias);
        if (clientPrivateKey == null) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.finest(
                        clientAlias + " is not a private key entry");
            }
            return null;
        }

        X509Certificate[] clientCerts = km.getCertificateChain(clientAlias);
        if ((clientCerts == null) || (clientCerts.length == 0)) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.finest(clientAlias +
                        " is a private key entry with no cert chain stored");
            }
            return null;
        }

        String privateKeyAlgorithm = clientPrivateKey.getAlgorithm();
        if (!Arrays.asList(keyType).contains(privateKeyAlgorithm)) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.fine(
                        clientAlias + " private key algorithm " +
                                privateKeyAlgorithm + " not in request list");
            }
            return null;
        }

        PublicKey clientPublicKey = clientCerts[0].getPublicKey();
        String publicKeyAlgorithm = clientPublicKey.getAlgorithm();
        if (!privateKeyAlgorithm.equals(publicKeyAlgorithm)) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.fine(
                        clientAlias + " private or public key is not of " +
                                "same algorithm: " +
                                privateKeyAlgorithm + " vs " +
                                publicKeyAlgorithm);
            }
            return null;
        }

        if (!checkPublicKey(clientAlias, clientPublicKey, chc)) {
            return null;
        }

        return new PossessionEntry(clientPrivateKey, clientCerts);
    }

    private static SSLPossession createServerPossession(
            ServerHandshakeContext shc, String[] keyTypes) {
        X509ExtendedKeyManager km = shc.sslContext.getX509KeyManager();
        for (String keyType : keyTypes) {
            String[] serverAliases = km.getServerAliases(keyType,
                        shc.peerSupportedAuthorities == null ? null :
                                shc.peerSupportedAuthorities.clone());

            if (serverAliases == null || serverAliases.length == 0) {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                    SSLLogger.finest("No X.509 cert selected for " + keyType);
                }
                continue;
            }

            PossessionEntry signPossEntry = null;
            PossessionEntry encPossEntry = null;
            for (String serverAlias : serverAliases) {
                PossessionEntry bufPossEntry = serverPossEntry(
                        shc, keyType, km, serverAlias);
                if (bufPossEntry == null) {
                    continue;
                }

                if (signPossEntry == null
                        && SMUtil.isSignCert(bufPossEntry.popCert)) {
                    signPossEntry = bufPossEntry;
                } else if (SMUtil.isEncCert(bufPossEntry.popCert)) {
                    encPossEntry = bufPossEntry;
                }

                if (signPossEntry != null && encPossEntry != null) {
                    break;
                }
            }

            TLCP11Possession tlcpPossession
                    = createPossession(signPossEntry, encPossEntry);
            if (tlcpPossession != null) {
                return tlcpPossession;
            }
        }

        return null;
    }

    private static TLCP11Possession createPossession(
            PossessionEntry signPossEntry, PossessionEntry encPossEntry) {
        if (signPossEntry == null) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.warning("No X.509 sign cert selected");
            }

            return null;
        }

        // Use sign cert as enc cert if possible
        if (encPossEntry == null
                && SMUtil.isEncCert(signPossEntry.popCert)) {
            encPossEntry = signPossEntry;
        }

        if (encPossEntry == null) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.warning("No X.509 enc cert selected");
            }

            return null;
        }

        return new TLCP11Possession(signPossEntry, encPossEntry);
    }

    private static PossessionEntry serverPossEntry(
            ServerHandshakeContext shc, String keyType,
            X509ExtendedKeyManager km, String serverAlias) {
        PrivateKey serverPrivateKey = km.getPrivateKey(serverAlias);
        if (serverPrivateKey == null) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.finest(
                        serverAlias + " is not a private key entry");
            }
            return null;
        }

        X509Certificate[] serverCerts = km.getCertificateChain(serverAlias);
        if ((serverCerts == null) || (serverCerts.length == 0)) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.finest(
                        serverAlias + " is not a certificate entry");
            }
            return null;
        }

        PublicKey serverPublicKey = serverCerts[0].getPublicKey();
        if ((!serverPrivateKey.getAlgorithm().equals(keyType))
                || (!serverPublicKey.getAlgorithm().equals(keyType))) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.fine(
                        serverAlias + " private or public key is not of " +
                                keyType + " algorithm");
            }
            return null;
        }

        // For TLS 1.2 and prior versions, the public key of an EC cert
        // MUST use a curve and point format supported by the client.
        // But for TLS 1.3, signature algorithms are negotiated
        // independently via the "signature_algorithms" extension.
        if (!shc.negotiatedProtocol.useTLS13PlusSpec() &&
                keyType.equals("EC")) {
            if (!checkPublicKey(serverAlias, serverPublicKey, shc)) {
                return null;
            }
        }

        return new PossessionEntry(serverPrivateKey, serverCerts);
    }

    private static boolean checkPublicKey(String alias, PublicKey publicKey,
                                          HandshakeContext hc) {
        if (!(publicKey instanceof ECPublicKey)) {
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.warning(alias +
                        " public key is not an instance of ECPublicKey");
            }
            return false;
        }

        // For ECC certs, check whether we support the EC domain
        // parameters.  If the client sent a supported_groups
        // ClientHello extension, check against that too for
        // TLS 1.2 and prior versions.
        ECParameterSpec params =
                ((ECPublicKey) publicKey).getParams();
        NamedGroup namedGroup = NamedGroup.valueOf(params);
        if (namedGroup != NamedGroup.CURVESM2) { // Only accept curveSM2
            if (SSLLogger.isOn && SSLLogger.isOn("ssl")) {
                SSLLogger.warning(
                        "Unsupported named group (" + namedGroup +
                                ") used in the " + alias + " certificate");
            }

            return false;
        }

        return true;
    }
}
