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
 * @summary The TLCP enc certificate must be verified against the same CA path
 *          as the sign certificate. A forged issuer, a CA certificate, a
 *          wrong extended key usage and an expired validity in the enc
 *          certificate must be rejected on both sides by the built-in trust
 *          managers, and accepted in dual-cert compat mode or with a custom
 *          trust manager.
 * @compile ../../Utils.java
 * @run main/othervm EncCertChainTest
 */

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLEngineResult.HandshakeStatus;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLSession;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509ExtendedTrustManager;
import java.io.ByteArrayInputStream;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.KeyFactory;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.Base64;

public class EncCertChainTest {

    private static final String PROP = "jdk.tlcp.dualCertCompat";
    private static final String ECC = "TLCP_ECC_SM4_CBC_SM3";
    private static final String ECDHE = "TLCP_ECDHE_SM4_CBC_SM3";
    private static final String[] SUITES = { ECC, ECDHE };

    private static final Path CERT_DIR = Paths.get(System.getProperty("test.src"))
            .resolve("..").resolve("..").resolve("tlcp-certs");

    public static void main(String[] args) throws Exception {
        // Valid enc certificates are accepted by both trust managers.
        for (String suite : SUITES) {
            for (String tm : new String[] { "PKIX", "SunX509" }) {
                run("server-enc", suite, tm, false, false, null, null);
                run("client-enc", suite, tm, true, false, null, null);
            }
        }

        // The bad enc certificate presented by the server is rejected by the
        // client, and the failure comes from the enc certificate verification.
        for (String suite : SUITES) {
            serverEnc("server-enc-forged", suite, "certificate_unknown",
                    "does not chain", "signature");
            serverEnc("server-enc-ca", suite, "bad_certificate", "must not be ca");
            serverEnc("server-enc-eku", suite, "certificate_unknown",
                    "extended key usage");
            serverEnc("server-enc-expired", suite, "certificate_expired",
                    "expired", "notafter", "validity");
        }

        // The bad enc certificate presented by the client is rejected by
        // the server.
        for (String suite : SUITES) {
            clientEnc("client-enc-forged", suite, "certificate_unknown",
                    "does not chain", "signature");
            clientEnc("client-enc-ca", suite, "bad_certificate", "must not be ca");
            clientEnc("client-enc-eku", suite, "certificate_unknown",
                    "extended key usage");
            clientEnc("client-enc-expired", suite, "certificate_unknown",
                    "expired", "notafter", "validity");
        }

        // SunX509 (SimpleValidator) rejects them too.
        for (String name : new String[] {
                "server-enc-forged", "server-enc-eku", "server-enc-expired" }) {
            expectReject(name, ECC, "SunX509", false, null, null,
                    null, new String[0]);
        }

        // Two trusted CAs share the name but not the key. The enc certificate
        // issued by the other one must not pass.
        expectReject("server-enc-forged", ECC, "PKIX", false,
                new String[] { "tlcp-intca.crt", "tlcp-rogue-intca.crt" },
                null, "bad_certificate", new String[] { "same ca key" });
        run("server-enc-forged", ECC, "PKIX", false, true,
                new String[] { "tlcp-intca.crt", "tlcp-rogue-intca.crt" },
                null);

        // A custom trust manager only validates the sign chain.
        run("server-enc-ca", ECC, "PKIX", false, false, null, new TrustAll());
        run("server-enc-expired", ECC, "PKIX", false, false, null, new TrustAll());
    }

    private static void serverEnc(String name, String suite, String alert,
            String... reasons) throws Exception {
        expectReject(name, suite, "PKIX", false, null, null, alert, reasons);
        // The legacy compat mode does not verify the enc certificate.
        run(name, suite, "PKIX", false, true, null, null);
    }

    private static void clientEnc(String name, String suite, String alert,
            String... reasons) throws Exception {
        expectReject(name, suite, "PKIX", true, null, null, alert, reasons);
        run(name, suite, "PKIX", true, true, null, null);
    }

    private static void expectReject(String name, String suite, String tm,
            boolean clientSide, String[] trusted, TrustManager custom,
            String alert, String[] reasons) throws Exception {
        try {
            run(name, suite, tm, clientSide, false, trusted, custom);
        } catch (SSLException e) {
            StringBuilder all = new StringBuilder();
            for (Throwable t = e; t != null; t = t.getCause()) {
                all.append(t).append(" | ");
            }
            String msg = all.toString().toLowerCase();
            System.out.println(name + " (" + suite + ", " + tm
                    + ") rejected: " + e.getMessage());
            // JDK 8 does not put the alert name in the exception message,
            // so only the failure reason is checked.
            if (reasons.length > 0) {
                for (String reason : reasons) {
                    if (msg.contains(reason)) {
                        return;
                    }
                }
                throw new RuntimeException(name
                        + " was rejected for another reason: " + all, e);
            }
            return;
        }
        throw new RuntimeException(name + " (" + suite + ", " + tm
                + ") must be rejected");
    }

    // clientSide: the bad enc certificate is presented by the client, which
    // requires client authentication on the server. The custom trust manager
    // is installed on the verifying side.
    // compat: the verifying side runs in dual-cert compat mode.
    private static void run(String encName, String suite, String tm,
            boolean clientSide, boolean compat, String[] trusted,
            TrustManager custom) throws Exception {
        String[] trustedCerts = trusted != null ? trusted
                : new String[] { "tlcp-intca.crt" };

        SSLContext serverCtx = createContext(trustedCerts, tm,
                clientSide ? custom : null,
                "tlcp-server-sign", clientSide ? "tlcp-server-enc"
                        : "tlcp-" + encName);
        SSLContext clientCtx = createContext(trustedCerts, tm,
                clientSide ? null : custom,
                "tlcp-client-sign", clientSide ? "tlcp-" + encName
                        : "tlcp-client-enc");

        SSLEngine server = serverCtx.createSSLEngine();
        server.setUseClientMode(false);
        server.setNeedClientAuth(clientSide);
        server.setEnabledCipherSuites(new String[] { suite });

        SSLEngine client = clientCtx.createSSLEngine();
        client.setUseClientMode(true);
        client.setEnabledCipherSuites(new String[] { suite });

        try {
            // The presenting side always runs in compat mode, otherwise it
            // may refuse to build the certificate list. Only the verifying
            // side is switched by the compat flag.
            boolean good = encName.equals("server-enc")
                    || encName.equals("client-enc");
            drive(client, server,
                    clientSide ? !good : compat,
                    clientSide ? compat : !good);
        } finally {
            System.clearProperty(PROP);
        }
    }

    private static SSLContext createContext(String[] trustedCerts, String tm,
            TrustManager custom, String signName, String encName)
            throws Exception {
        CertificateFactory cf = CertificateFactory.getInstance("X.509");

        KeyStore ts = KeyStore.getInstance("PKCS12");
        ts.load(null, null);
        for (String trusted : trustedCerts) {
            ts.setCertificateEntry("trusted-" + trusted, cf.generateCertificate(
                    new ByteArrayInputStream(Utils.certStr(
                            CERT_DIR.resolve(trusted)).getBytes())));
        }

        char[] passphrase = "passphrase".toCharArray();
        KeyStore ks = KeyStore.getInstance("PKCS12");
        ks.load(null, null);
        for (String name : new String[] { signName, encName }) {
            PrivateKey key = KeyFactory.getInstance("EC").generatePrivate(
                    new PKCS8EncodedKeySpec(Base64.getMimeDecoder().decode(
                            Utils.keyStr(CERT_DIR.resolve(name + ".key")))));
            Certificate cert = cf.generateCertificate(new ByteArrayInputStream(
                    Utils.certStr(CERT_DIR.resolve(name + ".crt")).getBytes()));
            ks.setKeyEntry("cert-" + name, key, passphrase,
                    new Certificate[] { cert });
        }

        TrustManager[] tms;
        if (custom != null) {
            tms = new TrustManager[] { custom };
        } else {
            TrustManagerFactory tmf = TrustManagerFactory.getInstance(tm);
            tmf.init(ts);
            tms = tmf.getTrustManagers();
        }

        KeyManagerFactory kmf = KeyManagerFactory.getInstance("NewSunX509");
        kmf.init(ks, passphrase);

        SSLContext context = SSLContext.getInstance("TLCP");
        context.init(kmf.getKeyManagers(), tms, null);
        return context;
    }

    // Drive an in-process handshake. jdk.tlcp.dualCertCompat is read fresh on
    // each use, so toggling it per engine action splits the two sides within
    // one JVM.
    private static void drive(SSLEngine client, SSLEngine server,
            boolean clientCompat, boolean serverCompat) throws Exception {
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
            System.setProperty(PROP, Boolean.toString(clientCompat));
            client.wrap(cOut, cToS);
            runTasks(client);

            System.setProperty(PROP, Boolean.toString(serverCompat));
            server.wrap(sOut, sToC);
            runTasks(server);

            cToS.flip();
            sToC.flip();

            System.setProperty(PROP, Boolean.toString(clientCompat));
            client.unwrap(sToC, cIn);
            runTasks(client);

            System.setProperty(PROP, Boolean.toString(serverCompat));
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

    private static void runTasks(SSLEngine engine) {
        if (engine.getHandshakeStatus() == HandshakeStatus.NEED_TASK) {
            Runnable task;
            while ((task = engine.getDelegatedTask()) != null) {
                task.run();
            }
        }
    }

    private static final class TrustAll extends X509ExtendedTrustManager {
        @Override public void checkClientTrusted(X509Certificate[] c, String a) { }
        @Override public void checkServerTrusted(X509Certificate[] c, String a) { }
        @Override public void checkClientTrusted(X509Certificate[] c, String a, Socket s) { }
        @Override public void checkServerTrusted(X509Certificate[] c, String a, Socket s) { }
        @Override public void checkClientTrusted(X509Certificate[] c, String a, SSLEngine e) { }
        @Override public void checkServerTrusted(X509Certificate[] c, String a, SSLEngine e) { }
        @Override public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
    }
}
