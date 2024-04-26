/*
 * Copyright (c) 1999, 2022, Oracle and/or its affiliates. All rights reserved.
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

import java.util.ArrayList;
import java.util.List;

import sun.security.ssl.SSLContextImpl.AbstractTLSContext;

public class TLCPContexts {

    public static final class TLCP11Context extends AbstractTLSContext {

        private static final List<ProtocolVersion> serverDefaultProtocols;
        private static final List<CipherSuite> serverDefaultCipherSuites;

        private static final List<ProtocolVersion> clientDefaultProtocols;
        private static final List<CipherSuite> clientDefaultCipherSuites;

        static {
            serverDefaultProtocols = getAvailableProtocols(
                    new ProtocolVersion[] {
                ProtocolVersion.TLCP11,
            });
            clientDefaultProtocols = getAvailableProtocols(
                    new ProtocolVersion[] {
                ProtocolVersion.TLCP11,
            });

            serverDefaultCipherSuites = getApplicableEnabledCipherSuites(
                    serverDefaultProtocols, false);
            clientDefaultCipherSuites = getApplicableEnabledCipherSuites(
                    clientDefaultProtocols, true);
        }

        @Override
        List<ProtocolVersion> getServerDefaultProtocolVersions() {
            return serverDefaultProtocols;
        }

        @Override
        List<CipherSuite> getServerDefaultCipherSuites() {
            return serverDefaultCipherSuites;
        }

        @Override
        List<ProtocolVersion> getClientDefaultProtocolVersions() {
            return clientDefaultProtocols;
        }

        @Override
        List<CipherSuite> getClientDefaultCipherSuites() {
            return clientDefaultCipherSuites;
        }
    }

    public static final class TLCPContext extends AbstractTLSContext {

        private static final List<ProtocolVersion> serverDefaultProtocols;
        private static final List<CipherSuite> serverDefaultCipherSuites;

        private static final List<ProtocolVersion> clientDefaultProtocols;
        private static final List<CipherSuite> clientDefaultCipherSuites;

        private static final IllegalArgumentException reservedException;

        static {
            reservedException = CustomizedSSLProtocols.reservedException;
            if (reservedException == null) {
                clientDefaultProtocols = customizedProtocols(true,
                        CustomizedSSLProtocols.customizedClientProtocols);
                serverDefaultProtocols = customizedProtocols(false,
                        CustomizedSSLProtocols.customizedServerProtocols);

                clientDefaultCipherSuites =
                        getApplicableEnabledCipherSuites(
                                clientDefaultProtocols, true);
                serverDefaultCipherSuites =
                        getApplicableEnabledCipherSuites(
                                serverDefaultProtocols, false);
            } else {
                // unlikely to be used
                clientDefaultProtocols = null;
                serverDefaultProtocols = null;
                clientDefaultCipherSuites = null;
                serverDefaultCipherSuites = null;
            }
        }

        private static List<ProtocolVersion> customizedProtocols(
                boolean client, List<ProtocolVersion> customized) {
            List<ProtocolVersion> refactored = new ArrayList<>();
            for (ProtocolVersion pv : customized) {
                refactored.add(pv);
            }

            // Use the default enabled protocols if no customization
            ProtocolVersion[] candidates;
            if (refactored.isEmpty()) {
                // Client and server use the same default protocols.
                candidates = new ProtocolVersion[] {
                        ProtocolVersion.TLS13,
                        ProtocolVersion.TLS12,
                        ProtocolVersion.TLCP11,
                        ProtocolVersion.TLS11,
                        ProtocolVersion.TLS10
                    };
            } else {
                // Use the customized TLS protocols.
                candidates =
                    refactored.toArray(new ProtocolVersion[0]);
            }

            return getAvailableProtocols(candidates);
        }

        @Override
        List<ProtocolVersion> getServerDefaultProtocolVersions() {
            return serverDefaultProtocols;
        }

        @Override
        List<CipherSuite> getServerDefaultCipherSuites() {
            return serverDefaultCipherSuites;
        }

        @Override
        List<ProtocolVersion> getClientDefaultProtocolVersions() {
            return clientDefaultProtocols;
        }

        @Override
        List<CipherSuite> getClientDefaultCipherSuites() {
            return clientDefaultCipherSuites;
        }
    }
}
