/*
 * Copyright (C) 2022, 2024, THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package sun.security.ssl;

import java.io.IOException;
import java.util.AbstractMap;
import java.util.Map;

final class TLCPKeyExchange {

    enum TLCP11KeyAgreement implements SSLKeyAgreement {
        SM2  ("sm2",  SM2KeyExchange.sm2PoGenerator,
                      SM2KeyExchange.sm2KAGenerator),
        SM2E ("sm2e", SM2EKeyExchange.sm2ePoGenerator,
                      SM2EKeyExchange.sm2eKAGenerator);

        final String name;
        final SSLPossessionGenerator possessionGenerator;
        final SSLKeyAgreementGenerator keyAgreementGenerator;

        TLCP11KeyAgreement(String name,
                SSLPossessionGenerator possessionGenerator,
                SSLKeyAgreementGenerator keyAgreementGenerator) {
            this.name = name;
            this.possessionGenerator = possessionGenerator;
            this.keyAgreementGenerator = keyAgreementGenerator;
        }

        @Override
        public SSLPossession createPossession(HandshakeContext context) {
            if (possessionGenerator != null) {
                return possessionGenerator.createPossession(context);
            }

            return null;
        }

        @Override
        public SSLKeyDerivation createKeyDerivation(
                HandshakeContext context) throws IOException {
            return keyAgreementGenerator.createKeyDerivation(context);
        }

        @Override
        public SSLHandshake[] getRelatedHandshakers(
                HandshakeContext handshakeContext) {
            if (!handshakeContext.negotiatedProtocol.useTLS13PlusSpec()) {
                if (this.possessionGenerator != null) {
                    return new SSLHandshake[] {
                            SSLHandshake.SERVER_KEY_EXCHANGE
                    };
                }
            }

            return new SSLHandshake[0];
        }

        @Override
        @SuppressWarnings({"unchecked", "rawtypes"})
        public Map.Entry<Byte, HandshakeProducer>[] getHandshakeProducers(
                HandshakeContext handshakeContext) {
            if (handshakeContext.negotiatedProtocol.useTLS13PlusSpec()) {
                return (Map.Entry<Byte, HandshakeProducer>[])(new Map.Entry[0]);
            }

            if (handshakeContext.sslConfig.isClientMode) {
                switch (this) {
                    case SM2:
                        return (Map.Entry<Byte,
                                HandshakeProducer>[])(new Map.Entry[] {
                            new AbstractMap.SimpleImmutableEntry<>(
                                    SSLHandshake.CLIENT_KEY_EXCHANGE.id,
                                    SM2ClientKeyExchange.sm2HandshakeProducer
                            )
                        });

                    case SM2E:
                        return (Map.Entry<Byte,
                                HandshakeProducer>[])(new Map.Entry[] {
                            new AbstractMap.SimpleImmutableEntry<>(
                                    SSLHandshake.CLIENT_KEY_EXCHANGE.id,
                                    SM2EClientKeyExchange.sm2eHandshakeProducer
                            )
                        });
                }
            } else {
                switch (this) {
                    case SM2:
                        return (Map.Entry<Byte,
                                HandshakeProducer>[])(new Map.Entry[] {
                            new AbstractMap.SimpleImmutableEntry<>(
                                    SSLHandshake.SERVER_KEY_EXCHANGE.id,
                                    SM2ServerKeyExchange.sm2HandshakeProducer
                            )
                        });
                    case SM2E:
                        return (Map.Entry<Byte,
                                HandshakeProducer>[])(new Map.Entry[] {
                            new AbstractMap.SimpleImmutableEntry<>(
                                    SSLHandshake.SERVER_KEY_EXCHANGE.id,
                                    SM2EServerKeyExchange.sm2eHandshakeProducer
                            )
                        });
                }
            }

            return (Map.Entry<Byte, HandshakeProducer>[])(new Map.Entry[0]);
        }

        @Override
        @SuppressWarnings({"unchecked", "rawtypes"})
        public Map.Entry<Byte, SSLConsumer>[] getHandshakeConsumers(
                HandshakeContext handshakeContext) {
            if (handshakeContext.negotiatedProtocol.useTLS13PlusSpec()) {
                return (Map.Entry<Byte, SSLConsumer>[])(new Map.Entry[0]);
            }

            if (handshakeContext.sslConfig.isClientMode) {
                switch (this) {
                    case SM2:
                        return (Map.Entry<Byte,
                                SSLConsumer>[])(new Map.Entry[] {
                            new AbstractMap.SimpleImmutableEntry<>(
                                    SSLHandshake.SERVER_KEY_EXCHANGE.id,
                                    SM2ServerKeyExchange.sm2HandshakeConsumer
                            )
                        });
                    case SM2E:
                        return (Map.Entry<Byte,
                                SSLConsumer>[])(new Map.Entry[] {
                            new AbstractMap.SimpleImmutableEntry<>(
                                    SSLHandshake.SERVER_KEY_EXCHANGE.id,
                                    SM2EServerKeyExchange.sm2eHandshakeConsumer
                            )
                        });
                }
            } else {
                switch (this) {
                    case SM2:
                        return (Map.Entry<Byte,
                                SSLConsumer>[])(new Map.Entry[] {
                            new AbstractMap.SimpleImmutableEntry<>(
                                    SSLHandshake.CLIENT_KEY_EXCHANGE.id,
                                    SM2ClientKeyExchange.sm2HandshakeConsumer
                            )
                        });

                    case SM2E:
                        return (Map.Entry<Byte,
                                SSLConsumer>[])(new Map.Entry[] {
                            new AbstractMap.SimpleImmutableEntry<>(
                                    SSLHandshake.CLIENT_KEY_EXCHANGE.id,
                                    SM2EClientKeyExchange.sm2eHandshakeConsumer
                            )
                        });
                }
            }

            return (Map.Entry<Byte, SSLConsumer>[])(new Map.Entry[0]);
        }
    }
}
