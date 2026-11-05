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
 * @summary A TLCP handshake requires two distinct certificates (a sign
 *          certificate and an enc certificate). A single certificate serving
 *          both roles must be rejected unconditionally, even when the
 *          jdk.tlcp.dualCertCompat property is enabled.
 * @compile SSLEngineTest.java ../../Utils.java
 * @run main/othervm DualCertRequiredTest
 * @run main/othervm -Djdk.tlcp.dualCertCompat=true DualCertRequiredTest
 */

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLEngineResult.HandshakeStatus;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLSession;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class DualCertRequiredTest {

    private static final String SUITE = "TLCP_ECC_SM4_GCM_SM3";

    public static void main(String[] args) throws Exception {
        boolean compat = Boolean.getBoolean("jdk.tlcp.dualCertCompat");
        System.out.println("jdk.tlcp.dualCertCompat=" + compat);

        try {
            singleCertHandshake();
            throw new RuntimeException("A single-certificate TLCP handshake "
                    + "must be rejected (compat=" + compat + ")");
        } catch (SSLException e) {
            System.out.println("Rejected as expected: " + e.getMessage());
        }
    }

    // A TLCP mutual-auth handshake where each peer owns only a single
    // certificate (no separate enc certificate). This must never complete:
    // the producer cannot build a sign/enc possession from one certificate,
    // and the consumer requires at least two certificates.
    private static void singleCertHandshake() throws Exception {
        SSLEngineTest.ContextParameters params =
                new SSLEngineTest.ContextParameters("TLCP", "PKIX", "NewSunX509");

        SSLContext serverCtx = SSLEngineTest.createSSLContext(
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.CA_CERT },
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.SERVER_SIGN_CERT },
                params);
        SSLContext clientCtx = SSLEngineTest.createSSLContext(
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.CA_CERT },
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.CLIENT_SIGN_CERT },
                params);

        SSLEngine server = serverCtx.createSSLEngine();
        server.setUseClientMode(false);
        server.setNeedClientAuth(true);
        server.setEnabledCipherSuites(new String[] { SUITE });

        SSLEngine client = clientCtx.createSSLEngine();
        client.setUseClientMode(true);
        client.setEnabledCipherSuites(new String[] { SUITE });

        drive(client, server);
    }

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
            client.wrap(cOut, cToS);
            server.wrap(sOut, sToC);
            runTasks(client);
            runTasks(server);

            cToS.flip();
            sToC.flip();

            client.unwrap(sToC, cIn);
            server.unwrap(cToS, sIn);
            runTasks(client);
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

    private static void runTasks(SSLEngine engine) {
        if (engine.getHandshakeStatus() == HandshakeStatus.NEED_TASK) {
            Runnable task;
            while ((task = engine.getDelegatedTask()) != null) {
                task.run();
            }
        }
    }
}
