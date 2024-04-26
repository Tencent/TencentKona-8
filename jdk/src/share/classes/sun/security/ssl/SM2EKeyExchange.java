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

import javax.crypto.KeyAgreement;
import javax.crypto.SecretKey;
import javax.net.ssl.SSLHandshakeException;
import java.io.IOException;
import java.security.AlgorithmConstraints;
import java.security.CryptoPrimitive;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.SecureRandom;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.ECParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.ECPublicKeySpec;
import java.security.spec.SM2KeyAgreementParamSpec;
import java.util.EnumSet;

import sun.security.ssl.SupportedGroupsExtension.NamedGroup;
import sun.security.ssl.SupportedGroupsExtension.NamedGroupType;
import sun.security.ssl.SupportedGroupsExtension.SupportedGroups;
import sun.security.ssl.TLCPAuthentication.TLCP11Possession;
import sun.security.util.ECUtil;

/**
 * TLCP 1.1 ephemeral SM2 key exchange.
 */
public class SM2EKeyExchange {

    static final SSLPossessionGenerator sm2ePoGenerator
            = new SM2EPossessionGenerator();
    static final SSLKeyAgreementGenerator sm2eKAGenerator
            = new SM2EKAGenerator();

    static final class SM2ECredentials implements SSLCredentials {

        final ECPublicKey ephemeralPublicKey;
        final NamedGroup namedGroup;

        SM2ECredentials(ECPublicKey ephemeralPublicKey, NamedGroup namedGroup) {
            this.ephemeralPublicKey = ephemeralPublicKey;
            this.namedGroup = namedGroup;
        }

        static SM2ECredentials valueOf(NamedGroup namedGroup,
            byte[] encodedPoint) throws IOException, GeneralSecurityException {

            if (namedGroup != NamedGroup.CURVESM2) {
                throw new RuntimeException(
                        "Credentials decoding: Not named group curveSM2");
            }

            if (encodedPoint == null || encodedPoint.length == 0) {
                return null;
            }

            ECParameterSpec parameters =
                    JsseJce.getECParameterSpec(namedGroup.oid);
            if (parameters == null) {
                return null;
            }

            ECPoint point = JsseJce.decodePoint(
                    encodedPoint, parameters.getCurve());
            KeyFactory factory = KeyFactory.getInstance("SM2");
            ECPublicKey publicKey = (ECPublicKey)factory.generatePublic(
                    new ECPublicKeySpec(point, parameters));
            return new SM2ECredentials(publicKey, namedGroup);
        }
    }

    static final class SM2EPossession implements SSLPossession {

        final ECPrivateKey ephemeralPrivateKey;
        final ECPublicKey ephemeralPublicKey;

        final ECPrivateKey popEncPrivateKey;
        final ECPublicKey popEncPublicKey;
        final NamedGroup namedGroup;

        SM2EPossession(TLCP11Possession tlcpPossession,
                       NamedGroup namedGroup, SecureRandom random) {
            try {
                KeyPairGenerator kpg = JsseJce.getKeyPairGenerator("EC");
                ECGenParameterSpec params =
                        (ECGenParameterSpec)namedGroup.getParameterSpec();
                kpg.initialize(params, random);
                KeyPair kp = kpg.generateKeyPair();
                ephemeralPrivateKey = (ECPrivateKey) kp.getPrivate();
                ephemeralPublicKey = (ECPublicKey)kp.getPublic();
            } catch (GeneralSecurityException e) {
                throw new RuntimeException("Could not generate SM2 keypair", e);
            }

            popEncPrivateKey = (ECPrivateKey) tlcpPossession.popEncPrivateKey;
            popEncPublicKey = (ECPublicKey) tlcpPossession.popEncPublicKey;
            this.namedGroup = namedGroup;
        }

        @Override
        public byte[] encode() {
            return ECUtil.encodePoint(
                    ephemeralPublicKey.getW(),
                    ephemeralPublicKey.getParams().getCurve());
        }

        // called by ClientHandshaker with either the server's static or
        // ephemeral public key
        SecretKey getAgreedSecret(
                ECPublicKey peerEphemeralPublicKey, boolean isInitiator)
                throws SSLHandshakeException {
            try {
                SM2KeyAgreementParamSpec params = new SM2KeyAgreementParamSpec(
                        popEncPrivateKey,
                        popEncPublicKey,
                        peerEphemeralPublicKey,
                        isInitiator,
                        32);

                KeyAgreement ka = KeyAgreement.getInstance("SM2");
                ka.init(ephemeralPrivateKey, params);
                ka.doPhase(peerEphemeralPublicKey, true);
                return ka.generateSecret("TlsPremasterSecret");
            } catch (GeneralSecurityException e) {
                throw (SSLHandshakeException) new SSLHandshakeException(
                        "Could not generate secret").initCause(e);
            }
        }

        // called by ServerHandshaker
        SecretKey getAgreedSecret(
                byte[] peerEphemeralEncodedPoint, boolean initiator)
                throws SSLHandshakeException {
            try {
                ECParameterSpec params = ephemeralPublicKey.getParams();
                ECPoint point = ECUtil.decodePoint(
                        peerEphemeralEncodedPoint, params.getCurve());
                KeyFactory kf = KeyFactory.getInstance("SM2");
                ECPublicKeySpec spec = new ECPublicKeySpec(point, params);
                ECPublicKey peerPublicKey = (ECPublicKey) kf.generatePublic(spec);
                return getAgreedSecret(peerPublicKey, initiator);
            } catch (GeneralSecurityException | IOException e) {
                throw (SSLHandshakeException) new SSLHandshakeException(
                        "Could not generate secret").initCause(e);
            }
        }

        // Check constraints of the specified EC public key.
        void checkConstraints(AlgorithmConstraints constraints,
                              byte[] encodedPoint) throws SSLHandshakeException {
            try {

                ECParameterSpec params = ephemeralPublicKey.getParams();
                ECPoint point =
                        ECUtil.decodePoint(encodedPoint, params.getCurve());
                ECPublicKeySpec spec = new ECPublicKeySpec(point, params);

                KeyFactory kf = KeyFactory.getInstance("SM2");
                ECPublicKey pubKey = (ECPublicKey)kf.generatePublic(spec);

                // check constraints of ECPublicKey
                if (!constraints.permits(
                        EnumSet.of(CryptoPrimitive.KEY_AGREEMENT), pubKey)) {
                    throw new SSLHandshakeException(
                        "ECPublicKey does not comply to algorithm constraints");
                }
            } catch (GeneralSecurityException | IOException e) {
                throw (SSLHandshakeException) new SSLHandshakeException(
                        "Could not generate ECPublicKey").initCause(e);
            }
        }
    }

    private static final class SM2EPossessionGenerator
            implements SSLPossessionGenerator {

        // Prevent instantiation of this class.
        private SM2EPossessionGenerator() {
            // blank
        }

        @Override
        public SSLPossession createPossession(HandshakeContext context) {
            NamedGroup preferableNamedGroup = null;
            if ((context.clientRequestedNamedGroups != null) &&
                    (!context.clientRequestedNamedGroups.isEmpty())) {
                preferableNamedGroup = SupportedGroups.getPreferredGroup(
                        context.negotiatedProtocol,
                        context.algorithmConstraints,
                        NamedGroupType.NAMED_GROUP_ECDHE,
                        context.clientRequestedNamedGroups);
            } else {
                preferableNamedGroup = SupportedGroups.getPreferredGroup(
                        context.negotiatedProtocol,
                        context.algorithmConstraints,
                        NamedGroupType.NAMED_GROUP_ECDHE);
            }

            ServerHandshakeContext shc = (ServerHandshakeContext) context;
            TLCP11Possession tlcpPossession = null;
            if (shc.interimAuthn instanceof TLCP11Possession) {
                tlcpPossession = ((TLCP11Possession) shc.interimAuthn);
            }
            if (preferableNamedGroup == NamedGroup.CURVESM2) {
                return new SM2EPossession(tlcpPossession,
                        preferableNamedGroup, context.sslContext.getSecureRandom());
            }

            // no match found, cannot use this cipher suite.
            return null;
        }
    }

    private static final class SM2EKAGenerator
            implements SSLKeyAgreementGenerator {

        // Prevent instantiation of this class.
        private SM2EKAGenerator() {
            // blank
        }

        @Override
        public SSLKeyDerivation createKeyDerivation(
                HandshakeContext context) throws IOException {
            SM2EPossession sm2ePossession = null;
            SM2ECredentials sm2eCredentials = null;
            for (SSLPossession poss : context.handshakePossessions) {
                if (!(poss instanceof SM2EPossession)) {
                    continue;
                }

                NamedGroup ng = ((SM2EPossession)poss).namedGroup;
                for (SSLCredentials cred : context.handshakeCredentials) {
                    if (!(cred instanceof SM2ECredentials)) {
                        continue;
                    }
                    if (ng.equals(((SM2ECredentials)cred).namedGroup)) {
                        sm2eCredentials = (SM2ECredentials)cred;
                        break;
                    }
                }

                if (sm2eCredentials != null) {
                    sm2ePossession = (SM2EPossession)poss;
                    break;
                }
            }

            if (sm2ePossession == null || sm2eCredentials == null) {
                throw context.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "No sufficient SM2 key agreement parameters negotiated");
            }

            return new SM2KAKeyDerivation("SM2", context,
                    sm2ePossession.ephemeralPrivateKey,
                    sm2eCredentials.ephemeralPublicKey);
        }
    }
}
