/*
 * Copyright (C) 2026, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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
 */

/*
 * @test
 * @summary A strict TLCP server must reject a client certificate list whose
 *          two certificates violate the role/pairing rules, exercising the
 *          real Certificate -> createCredentials consume path. Covers the
 *          report's primary payload (two distinct dual-keyUsage certificates
 *          with different subjects -> SIGN_KEY_USAGE) and the cross-subject
 *          strict-role payload (-> DIFFERENT_IDENTITY).
 * @compile SSLEngineTest.java ../../Utils.java
 * @run main/othervm DualCertRolePairTest
 */

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLEngineResult.HandshakeStatus;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLSession;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class DualCertRolePairTest {

    private static final String PROP = "jdk.tlcp.dualCertCompat";
    private static final String SUITE = "TLCP_ECC_SM4_GCM_SM3";

    public static void main(String[] args) throws Exception {
        // Primary attack payload: two distinct dual-keyUsage certs, different
        // subjects. The strict server rejects it at the sign-cert keyUsage
        // check (dual-KU cannot act as the pure signing/identity cert).
        attack(new SSLEngineTest.Cert[] {
                        SSLEngineTest.Cert.CLIENT_DUAL_CERT,
                        SSLEngineTest.Cert.SERVER_DUAL_CERT },
                "digitalSignature key usage only");

        // Cross-subject payload with strict-role certs (DS-only sign +
        // KE/DE-only enc, different subjects). Passes the keyUsage checks but
        // is rejected because the two certs do not represent one identity.
        attack(new SSLEngineTest.Cert[] {
                        SSLEngineTest.Cert.CLIENT_SIGN_CERT,
                        SSLEngineTest.Cert.SERVER_ENC_CERT },
                "same identity");
    }

    private static void attack(SSLEngineTest.Cert[] clientCerts,
            String expectedReason) throws Exception {
        SSLEngineTest.ContextParameters params =
                new SSLEngineTest.ContextParameters("TLCP", "PKIX", "NewSunX509");

        SSLContext serverCtx = SSLEngineTest.createSSLContext(
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.CA_CERT },
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.SERVER_SIGN_CERT,
                        SSLEngineTest.Cert.SERVER_ENC_CERT },
                params);
        SSLContext clientCtx = SSLEngineTest.createSSLContext(
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.CA_CERT },
                clientCerts, params);

        SSLEngine server = serverCtx.createSSLEngine();
        server.setUseClientMode(false);
        server.setNeedClientAuth(true);
        server.setEnabledCipherSuites(new String[] { SUITE });

        SSLEngine client = clientCtx.createSSLEngine();
        client.setUseClientMode(true);
        client.setEnabledCipherSuites(new String[] { SUITE });

        try {
            drive(client, server);
            throw new RuntimeException("Strict server must reject the payload; "
                    + "expected reason: " + expectedReason);
        } catch (SSLException e) {
            String msg = String.valueOf(e.getMessage());
            System.out.println("Rejected as expected: " + msg);
            if (!msg.contains(expectedReason)) {
                throw new RuntimeException("Rejected for the wrong reason; "
                        + "expected \"" + expectedReason + "\" but got: " + msg, e);
            }
        } finally {
            System.clearProperty(PROP);
        }
    }

    // Drive an in-process handshake. The client acts in dual-cert compat mode
    // (so its producer actually emits the malicious two-cert list), while the
    // server acts in strict mode (so its createCredentials consume path runs
    // the hardened checks). jdk.tlcp.dualCertCompat is read fresh on each use,
    // so toggling it per engine action splits the two sides within one JVM.
    private static void drive(SSLEngine client, SSLEngine server)
            throws Exception {
        SSLSession session = client.getSession();
        int netMax = session.getPacketBufferSize();
        int appMax = session.getApplicationBufferSize();

        ByteBuffer cOut = ByteBuffer.wrap("client".getBytes(StandardCharsets.UTF_8));
        ByteBuffer sOut = ByteBuffer.wrap("server".getBytes(StandardCharsets.UTF_8));
        ByteBuffer cToS = ByteBuffer.allocate(netMax + 100);
        ByteBuffer sToC = ByteBuffer.allocate(netMax + 100);
        ByteBuffer cIn = ByteBuffer.allocate(appMax + 100);
        ByteBuffer sIn = ByteBuffer.allocate(appMax + 100);

        for (int i = 0; i < 50; i++) {
            clientMode();
            client.wrap(cOut, cToS);
            runTasks(client);

            serverMode();
            server.wrap(sOut, sToC);
            runTasks(server);

            cToS.flip();
            sToC.flip();

            clientMode();
            client.unwrap(sToC, cIn);
            runTasks(client);

            serverMode();
            server.unwrap(cToS, sIn);
            runTasks(server);

            cToS.compact();
            sToC.compact();

            if (client.getHandshakeStatus() == HandshakeStatus.NOT_HANDSHAKING
                    && server.getHandshakeStatus() == HandshakeStatus.NOT_HANDSHAKING) {
                return;
            }
        }
        throw new IllegalStateException("TLCP handshake did not complete");
    }

    // Client side: legacy compat, so the producer will emit the crafted list.
    private static void clientMode() {
        System.setProperty(PROP, "true");
    }

    // Server side: strict (default), so the consumer enforces the hardening.
    private static void serverMode() {
        System.setProperty(PROP, "false");
    }

    private static void runTasks(SSLEngine engine) {
        if (engine.getHandshakeStatus() == HandshakeStatus.NEED_TASK) {
            Runnable task;
            while ((task = engine.getDelegatedTask()) != null) {
                task.run();
            }
        }
    }
}
