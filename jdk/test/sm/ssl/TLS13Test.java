/*
 * Copyright (c) 2016, 2021, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

//
// Please run in othervm mode.  SunJSSE does not support dynamic system
// properties, no way to re-use system properties in samevm/agentvm mode.
//

/*
 * @test
 * @summary TLS 1.3 with RFC 8998 algorithms, including TLS_SM4_GCM_SM3, curveSM2 and sm2sig_sm3.
 *
 * @run main/othervm TLS13Test ecdsaCert
 * @run main/othervm TLS13Test sm2Cert
 *
 * @run main/othervm -Djdk.tls.namedGroups=secp256r1
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   TLS13Test ecdsaCert
 * @run main/othervm -Djdk.tls.namedGroups=secp256r1
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=ecdsa_secp256r1_sha256
 *                   TLS13Test ecdsaCert
 * @run main/othervm -Djdk.tls.namedGroups=ffdhe2048
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=ecdsa_secp256r1_sha256
 *                   TLS13Test ecdsaCert
 * @run main/othervm -Djdk.tls.namedGroups=curvesm2
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=ecdsa_secp256r1_sha256
 *                   TLS13Test ecdsaCert
 * @run main/othervm -Djdk.tls.namedGroups=secp256r1
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=ecdsa_secp256r1_sha256
 *                   TLS13Test ecdsaCert
 * @run main/othervm -Djdk.tls.namedGroups=ffdhe2048
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=ecdsa_secp256r1_sha256
 *                   TLS13Test ecdsaCert
 * @run main/othervm -Djdk.tls.namedGroups=curvesm2
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=ecdsa_secp256r1_sha256
 *                   TLS13Test ecdsaCert
 *
 * @run main/othervm -Djdk.tls.namedGroups=secp256r1
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=sm2sig_sm3
 *                   TLS13Test sm2Cert
 * @run main/othervm -Djdk.tls.namedGroups=ffdhe2048
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=sm2sig_sm3
 *                   TLS13Test sm2Cert
 * @run main/othervm -Djdk.tls.namedGroups=curvesm2
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=sm2sig_sm3
 *                   TLS13Test sm2Cert
 * @run main/othervm -Djdk.tls.namedGroups=secp256r1
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=sm2sig_sm3
 *                   TLS13Test sm2Cert
 * @run main/othervm -Djdk.tls.namedGroups=ffdhe2048
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=sm2sig_sm3
 *                   TLS13Test sm2Cert
 * @run main/othervm -Djdk.tls.namedGroups=curvesm2
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   TLS13Test sm2Cert
 * @run main/othervm -Djdk.tls.namedGroups=curvesm2
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=sm2sig_sm3
 *                   TLS13Test sm2Cert
 *
 * @run main/othervm -Djdk.tls.namedGroups=secp256r1
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=rsa_pkcs1_sha256,rsa_pss_rsae_sha256
 *                   TLS13Test rsaCert
 * @run main/othervm -Djdk.tls.namedGroups=ffdhe2048
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=rsa_pkcs1_sha256,rsa_pss_rsae_sha256
 *                   TLS13Test rsaCert
 * @run main/othervm -Djdk.tls.namedGroups=curvesm2
 *                   -Djdk.tls.client.cipherSuites=TLS_AES_128_GCM_SHA256
 *                   -Djdk.tls.client.SignatureSchemes=rsa_pkcs1_sha256,rsa_pss_rsae_sha256
 *                   TLS13Test rsaCert
 * @run main/othervm -Djdk.tls.namedGroups=secp256r1
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=rsa_pkcs1_sha256,rsa_pss_rsae_sha256
 *                   TLS13Test rsaCert
 * @run main/othervm -Djdk.tls.namedGroups=ffdhe2048
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=rsa_pkcs1_sha256,rsa_pss_rsae_sha256
 *                   TLS13Test rsaCert
 * @run main/othervm -Djdk.tls.namedGroups=curvesm2
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   TLS13Test sm2Cert
 * @run main/othervm -Djdk.tls.namedGroups=curvesm2
 *                   -Djdk.tls.client.cipherSuites=TLS_SM4_GCM_SM3
 *                   -Djdk.tls.client.SignatureSchemes=rsa_pkcs1_sha256,rsa_pss_rsae_sha256
 *                   TLS13Test rsaCert
 */

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLServerSocket;
import javax.net.ssl.SSLServerSocketFactory;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManagerFactory;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.security.KeyFactory;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.Base64;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class TLS13Test {

    public static void main(String[] args) throws Exception {
//        System.setProperty("javax.net.debug", "all");
        new TLS13Test(args[0]).run();
    }

    private final Cert[] TRUSTED_CERTS;
    private final Cert[] END_ENTITY_CERTS;

    public TLS13Test(String certType) {
        switch (certType) {
            case "ecdsaCert":
                TRUSTED_CERTS = new Cert[] {Cert.CA_ECDSA_SECP256R1};
                END_ENTITY_CERTS = new Cert[] {Cert.EE_ECDSA_SECP256R1};
                break;
            case "sm2Cert":
                TRUSTED_CERTS = new Cert[] {Cert.CA_SM2_CURVESM2};
                END_ENTITY_CERTS = new Cert[] {Cert.EE_SM2_CURVESM2};
                break;
            case "rsaCert":
                TRUSTED_CERTS = new Cert[] {Cert.CA_RSA};
                END_ENTITY_CERTS = new Cert[] {Cert.EE_RSA};
                break;
            default:
                TRUSTED_CERTS = null;
                END_ENTITY_CERTS = null;
                break;
        }
    }

    /*
     * Run the test case.
     */
    public void run() throws Exception {
        bootup();
    }

    /*
     * Define the server side application of the test for the specified socket.
     */
    protected void runServerApplication(SSLSocket socket) throws Exception {
        // here comes the test logic
        InputStream sslIS = socket.getInputStream();
        OutputStream sslOS = socket.getOutputStream();

        sslIS.read();
        sslOS.write(85);
        sslOS.flush();
    }

    /*
     * Define the client side application of the test for the specified socket.
     * This method is used if the returned value of
     * isCustomizedClientConnection() is false.
     *
     * @param socket may be null is no client socket is generated.
     *
     * @see #isCustomizedClientConnection()
     */
    protected void runClientApplication(SSLSocket socket) throws Exception {
        InputStream sslIS = socket.getInputStream();
        OutputStream sslOS = socket.getOutputStream();

        sslOS.write(280);
        sslOS.flush();
        sslIS.read();
    }

    /*
     * Define the client side application of the test for the specified
     * server port.  This method is used if the returned value of
     * isCustomizedClientConnection() is true.
     *
     * Note that the client need to connect to the server port by itself
     * for the actual message exchange.
     *
     * @see #isCustomizedClientConnection()
     */
    protected void runClientApplication(int serverPort) throws Exception {
        // blank
    }

    /*
     * Create an instance of SSLContext for client use.
     */
    protected SSLContext createClientSSLContext() throws Exception {
        return createSSLContext(TRUSTED_CERTS, END_ENTITY_CERTS,
                getClientContextParameters());
    }

    /*
     * Create an instance of SSLContext for server use.
     */
    protected SSLContext createServerSSLContext() throws Exception {
        return createSSLContext(TRUSTED_CERTS, END_ENTITY_CERTS,
                getServerContextParameters());
    }

    /*
     * The parameters used to configure SSLContext.
     */
    protected static final class ContextParameters {
        final String contextProtocol;
        final String tmAlgorithm;
        final String kmAlgorithm;

        ContextParameters(String contextProtocol,
                String tmAlgorithm, String kmAlgorithm) {

            this.contextProtocol = contextProtocol;
            this.tmAlgorithm = tmAlgorithm;
            this.kmAlgorithm = kmAlgorithm;
        }
    }

    /*
     * Get the client side parameters of SSLContext.
     */
    protected ContextParameters getClientContextParameters() {
        return new ContextParameters("TLS", "PKIX", "NewSunX509");
    }

    /*
     * Get the server side parameters of SSLContext.
     */
    protected ContextParameters getServerContextParameters() {
        return new ContextParameters("TLS", "PKIX", "NewSunX509");
    }

    /*
     * Does the client side use customized connection other than
     * explicit Socket.connect(), for example, URL.openConnection()?
     */
    protected boolean isCustomizedClientConnection() {
        return false;
    }

    /*
     * Configure the client side socket.
     */
    protected void configureClientSocket(SSLSocket socket) {

    }

    /*
     * Configure the server side socket.
     */
    protected void configureServerSocket(SSLServerSocket socket) {
        socket.setNeedClientAuth(true);
    }

    /*
     * =============================================
     * Define the client and server side operations.
     *
     * If the client or server is doing some kind of object creation
     * that the other side depends on, and that thread prematurely
     * exits, you may experience a hang.  The test harness will
     * terminate all hung threads after its timeout has expired,
     * currently 3 minutes by default, but you might try to be
     * smart about it....
     */

    /*
     * Is the server ready to serve?
     */
    protected final CountDownLatch serverCondition = new CountDownLatch(1);

    /*
     * Is the client ready to handshake?
     */
    protected final CountDownLatch clientCondition = new CountDownLatch(1);

    /*
     * What's the server port?  Use any free port by default
     */
    protected volatile int serverPort = 0;

    /*
     * What's the server address?  null means binding to the wildcard.
     */
    protected volatile InetAddress serverAddress = null;

    /*
     * Define the server side of the test.
     */
    protected void doServerSide() throws Exception {
        // kick start the server side service
        SSLContext context = createServerSSLContext();
        SSLServerSocketFactory sslssf = context.getServerSocketFactory();
        InetAddress serverAddress = this.serverAddress;
        SSLServerSocket sslServerSocket = serverAddress == null ?
                (SSLServerSocket)sslssf.createServerSocket(serverPort)
                : (SSLServerSocket)sslssf.createServerSocket();
        if (serverAddress != null) {
            sslServerSocket.bind(new InetSocketAddress(serverAddress, serverPort));
        }
        configureServerSocket(sslServerSocket);
        serverPort = sslServerSocket.getLocalPort();

        // Signal the client, the server is ready to accept connection.
        serverCondition.countDown();

        // Try to accept a connection in 30 seconds.
        SSLSocket sslSocket;
        try {
            sslServerSocket.setSoTimeout(300000);
            sslSocket = (SSLSocket)sslServerSocket.accept();
        } catch (SocketTimeoutException ste) {
            // Ignore the test case if no connection within 30 seconds.
            System.out.println(
                "No incoming client connection in 30 seconds. " +
                "Ignore in server side.");
            return;
        } finally {
            sslServerSocket.close();
        }

        // handle the connection
        try {
            // Is it the expected client connection?
            //
            // Naughty test cases or third party routines may try to
            // connection to this server port unintentionally.  In
            // order to mitigate the impact of unexpected client
            // connections and avoid intermittent failure, it should
            // be checked that the accepted connection is really linked
            // to the expected client.
            boolean clientIsReady =
                    clientCondition.await(30L, TimeUnit.SECONDS);

            if (clientIsReady) {
                // Run the application in server side.
                runServerApplication(sslSocket);
            } else {    // Otherwise, ignore
                // We don't actually care about plain socket connections
                // for TLS communication testing generally.  Just ignore
                // the test if the accepted connection is not linked to
                // the expected client or the client connection timeout
                // in 30 seconds.
                System.out.println(
                        "The client is not the expected one or timeout. " +
                        "Ignore in server side.");
            }
        } finally {
            sslSocket.close();
        }
    }

    /*
     * Define the client side of the test.
     */
    protected void doClientSide() throws Exception {

        // Wait for server to get started.
        //
        // The server side takes care of the issue if the server cannot
        // get started in 90 seconds.  The client side would just ignore
        // the test case if the serer is not ready.
        boolean serverIsReady =
                serverCondition.await(90L, TimeUnit.SECONDS);
        if (!serverIsReady) {
            System.out.println(
                    "The server is not ready yet in 90 seconds. " +
                    "Ignore in client side.");
            return;
        }

        if (isCustomizedClientConnection()) {
            // Signal the server, the client is ready to communicate.
            clientCondition.countDown();

            // Run the application in client side.
            runClientApplication(serverPort);

            return;
        }

        SSLContext context = createClientSSLContext();
        SSLSocketFactory sslsf = context.getSocketFactory();

        try (SSLSocket sslSocket = (SSLSocket)sslsf.createSocket()) {
            try {
                configureClientSocket(sslSocket);
                InetAddress serverAddress = this.serverAddress;
                InetSocketAddress connectAddress = serverAddress == null
                        ? new InetSocketAddress("localhost", serverPort)
                        : new InetSocketAddress(serverAddress, serverPort);
                sslSocket.connect(connectAddress, 15000);
            } catch (IOException ioe) {
                // The server side may be impacted by naughty test cases or
                // third party routines, and cannot accept connections.
                //
                // Just ignore the test if the connection cannot be
                // established.
                System.out.println(
                        "Cannot make a connection in 15 seconds. " +
                        "Ignore in client side.");
                return;
            }

            // OK, here the client and server get connected.

            // Signal the server, the client is ready to communicate.
            clientCondition.countDown();

            // There is still a chance in theory that the server thread may
            // wait client-ready timeout and then quit.  The chance should
            // be really rare so we don't consider it until it becomes a
            // real problem.

            // Run the application in client side.
            runClientApplication(sslSocket);
        }
    }

    /*
     * =============================================
     * Stuffs to customize the SSLContext instances.
     */

    /*
     * Create an instance of SSLContext with the specified trust/key materials.
     */
    public static SSLContext createSSLContext(
            Cert[] trustedCerts,
            Cert[] endEntityCerts,
            ContextParameters params) throws Exception {

        KeyStore ts = null;     // trust store
        KeyStore ks = null;     // key store
        char passphrase[] = "passphrase".toCharArray();

        // Generate certificate from cert string.
        CertificateFactory cf = CertificateFactory.getInstance("X.509");

        // Import the trused certs.
        ByteArrayInputStream is;
        if (trustedCerts != null && trustedCerts.length != 0) {
            ts = KeyStore.getInstance("PKCS12");
            ts.load(null, null);

            Certificate[] trustedCert = new Certificate[trustedCerts.length];
            for (int i = 0; i < trustedCerts.length; i++) {
                is = new ByteArrayInputStream(trustedCerts[i].certStr.getBytes());
                try {
                    trustedCert[i] = cf.generateCertificate(is);
                } finally {
                    is.close();
                }

                ts.setCertificateEntry(
                        "trusted-cert-" + trustedCerts[i].name(), trustedCert[i]);
            }
        }

        // Import the key materials.
        if (endEntityCerts != null && endEntityCerts.length != 0) {
            ks = KeyStore.getInstance("PKCS12");
            ks.load(null, null);

            for (int i = 0; i < endEntityCerts.length; i++) {
                // generate the private key.
                PKCS8EncodedKeySpec priKeySpec = new PKCS8EncodedKeySpec(
                    Base64.getMimeDecoder().decode(endEntityCerts[i].privKeyStr));
                KeyFactory kf = KeyFactory.getInstance(
                        endEntityCerts[i].keyAlgo);
                PrivateKey priKey = kf.generatePrivate(priKeySpec);

                // generate certificate chain
                is = new ByteArrayInputStream(
                        endEntityCerts[i].certStr.getBytes());
                Certificate keyCert = null;
                try {
                    keyCert = cf.generateCertificate(is);
                } finally {
                    is.close();
                }

                Certificate[] chain = new Certificate[] { keyCert };

                // import the key entry.
                ks.setKeyEntry("cert-" + endEntityCerts[i].name(),
                        priKey, passphrase, chain);
            }
        }

        // Create an SSLContext object.
        TrustManagerFactory tmf =
                TrustManagerFactory.getInstance(params.tmAlgorithm);
        tmf.init(ts);

        SSLContext context = SSLContext.getInstance(params.contextProtocol);
        if (endEntityCerts != null && endEntityCerts.length != 0 && ks != null) {
            KeyManagerFactory kmf =
                    KeyManagerFactory.getInstance(params.kmAlgorithm);
            kmf.init(ks, passphrase);

            context.init(kmf.getKeyManagers(), tmf.getTrustManagers(), null);
        } else {
            context.init(null, tmf.getTrustManagers(), null);
        }

        return context;
    }

    /*
     * =================================================
     * Stuffs to boot up the client-server mode testing.
     */
    private Thread clientThread = null;
    private Thread serverThread = null;
    private volatile Exception serverException = null;
    private volatile Exception clientException = null;

    /*
     * Should we run the client or server in a separate thread?
     * Both sides can throw exceptions, but do you have a preference
     * as to which side should be the main thread.
     */
    private final boolean separateServerThread = false;

    /*
     * Boot up the testing, used to drive remainder of the test.
     */
    private void bootup() throws Exception {
        Exception startException = null;
        try {
            if (separateServerThread) {
                startServer(true);
                startClient(false);
            } else {
                startClient(true);
                startServer(false);
            }
        } catch (Exception e) {
            startException = e;
        }

        /*
         * Wait for other side to close down.
         */
        if (separateServerThread) {
            if (serverThread != null) {
                serverThread.join();
            }
        } else {
            if (clientThread != null) {
                clientThread.join();
            }
        }

        /*
         * When we get here, the test is pretty much over.
         * Which side threw the error?
         */
        Exception local;
        Exception remote;

        if (separateServerThread) {
            remote = serverException;
            local = clientException;
        } else {
            remote = clientException;
            local = serverException;
        }

        Exception exception = null;

        /*
         * Check various exception conditions.
         */
        if ((local != null) && (remote != null)) {
            // If both failed, return the curthread's exception.
            local.initCause(remote);
            exception = local;
        } else if (local != null) {
            exception = local;
        } else if (remote != null) {
            exception = remote;
        } else if (startException != null) {
            exception = startException;
        }

        /*
         * If there was an exception *AND* a startException,
         * output it.
         */
        if (exception != null) {
            if (exception != startException && startException != null) {
                exception.addSuppressed(startException);
            }
            throw exception;
        }

        // Fall-through: no exception to throw!
    }

    private void startServer(boolean newThread) throws Exception {
        if (newThread) {
            serverThread = new Thread() {
                @Override
                public void run() {
                    try {
                        doServerSide();
                    } catch (Exception e) {
                        /*
                         * Our server thread just died.
                         *
                         * Release the client, if not active already...
                         */
                        logException("Server died", e);
                        serverException = e;
                    }
                }
            };
            serverThread.start();
        } else {
            try {
                doServerSide();
            } catch (Exception e) {
                logException("Server failed", e);
                serverException = e;
            }
        }
    }

    private void startClient(boolean newThread) throws Exception {
        if (newThread) {
            clientThread = new Thread() {
                @Override
                public void run() {
                    try {
                        doClientSide();
                    } catch (Exception e) {
                        /*
                         * Our client thread just died.
                         */
                        logException("Client died", e);
                        clientException = e;
                    }
                }
            };
            clientThread.start();
        } else {
            try {
                doClientSide();
            } catch (Exception e) {
                logException("Client failed", e);
                clientException = e;
            }
        }
    }

    private synchronized void logException(String prefix, Throwable cause) {
        System.out.println(prefix + ": " + cause);
        cause.printStackTrace(System.out);
    }

    public static enum Cert {

        CA_ECDSA_SECP256R1(
                "EC",
                // SHA256withECDSA, curve secp256r1
                // Validity
                //     Not Before: May 22 07:18:16 2018 GMT
                //     Not After : May 17 07:18:16 2038 GMT
                // Subject Key Identifier:
                //     60:CF:BD:73:FF:FA:1A:30:D2:A4:EC:D3:49:71:46:EF:1A:35:A0:86
                "-----BEGIN CERTIFICATE-----\n" +
                "MIIBvjCCAWOgAwIBAgIJAIvFG6GbTroCMAoGCCqGSM49BAMCMDsxCzAJBgNVBAYT\n" +
                "AlVTMQ0wCwYDVQQKDARKYXZhMR0wGwYDVQQLDBRTdW5KU1NFIFRlc3QgU2VyaXZj\n" +
                "ZTAeFw0xODA1MjIwNzE4MTZaFw0zODA1MTcwNzE4MTZaMDsxCzAJBgNVBAYTAlVT\n" +
                "MQ0wCwYDVQQKDARKYXZhMR0wGwYDVQQLDBRTdW5KU1NFIFRlc3QgU2VyaXZjZTBZ\n" +
                "MBMGByqGSM49AgEGCCqGSM49AwEHA0IABBz1WeVb6gM2mh85z3QlvaB/l11b5h0v\n" +
                "LIzmkC3DKlVukZT+ltH2Eq1oEkpXuf7QmbM0ibrUgtjsWH3mULfmcWmjUDBOMB0G\n" +
                "A1UdDgQWBBRgz71z//oaMNKk7NNJcUbvGjWghjAfBgNVHSMEGDAWgBRgz71z//oa\n" +
                "MNKk7NNJcUbvGjWghjAMBgNVHRMEBTADAQH/MAoGCCqGSM49BAMCA0kAMEYCIQCG\n" +
                "6wluh1r2/T6L31mZXRKf9JxeSf9pIzoLj+8xQeUChQIhAJ09wAi1kV8yePLh2FD9\n" +
                "2YEHlSQUAbwwqCDEVB5KxaqP\n" +
                "-----END CERTIFICATE-----",
                "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg/HcHdoLJCdq3haVd\n" +
                "XZTSKP00YzM3xX97l98vGL/RI1KhRANCAAQc9VnlW+oDNpofOc90Jb2gf5ddW+Yd\n" +
                "LyyM5pAtwypVbpGU/pbR9hKtaBJKV7n+0JmzNIm61ILY7Fh95lC35nFp"),

        EE_ECDSA_SECP256R1(
                "EC",
                // SHA256withECDSA, curve secp256r1
                // Validity
                //     Not Before: May 22 07:18:16 2018 GMT
                //     Not After : May 17 07:18:16 2038 GMT
                // Authority Key Identifier:
                //     60:CF:BD:73:FF:FA:1A:30:D2:A4:EC:D3:49:71:46:EF:1A:35:A0:86
                "-----BEGIN CERTIFICATE-----\n" +
                "MIIBqjCCAVCgAwIBAgIJAPLY8qZjgNRAMAoGCCqGSM49BAMCMDsxCzAJBgNVBAYT\n" +
                "AlVTMQ0wCwYDVQQKDARKYXZhMR0wGwYDVQQLDBRTdW5KU1NFIFRlc3QgU2VyaXZj\n" +
                "ZTAeFw0xODA1MjIwNzE4MTZaFw0zODA1MTcwNzE4MTZaMFUxCzAJBgNVBAYTAlVT\n" +
                "MQ0wCwYDVQQKDARKYXZhMR0wGwYDVQQLDBRTdW5KU1NFIFRlc3QgU2VyaXZjZTEY\n" +
                "MBYGA1UEAwwPUmVncmVzc2lvbiBUZXN0MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcD\n" +
                "QgAEb+9n05qfXnfHUb0xtQJNS4JeSi6IjOfW5NqchvKnfJey9VkJzR7QHLuOESdf\n" +
                "xlR7q8YIWgih3iWLGfB+wxHiOqMjMCEwHwYDVR0jBBgwFoAUYM+9c//6GjDSpOzT\n" +
                "SXFG7xo1oIYwCgYIKoZIzj0EAwIDSAAwRQIgWpRegWXMheiD3qFdd8kMdrkLxRbq\n" +
                "1zj8nQMEwFTUjjQCIQDRIrAjZX+YXHN9b0SoWWLPUq0HmiFIi8RwMnO//wJIGQ==\n" +
                "-----END CERTIFICATE-----",
                "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgn5K03bpTLjEtFQRa\n" +
                "JUtx22gtmGEvvSUSQdimhGthdtihRANCAARv72fTmp9ed8dRvTG1Ak1Lgl5KLoiM\n" +
                "59bk2pyG8qd8l7L1WQnNHtAcu44RJ1/GVHurxghaCKHeJYsZ8H7DEeI6"),

        CA_SM2_CURVESM2(
                "EC",
                // SM3withSM2, curve curveSM2
                // Validity
                //     Not Before: Nov  8 08:04:58 2023 GMT
                //     Not After : Nov  5 08:04:58 2033 GMT
                // Subject Key Identifier:
                //     06:43:93:0F:3D:B0:84:49:C0:CB:39:D3:01:43:B1:D2:53:80:B2:4B
                "-----BEGIN CERTIFICATE-----\n" +
                "MIIBoDCCAUagAwIBAgIUfVbjIL0wWl0RVRU4eKA82EySHRQwCgYIKoEcz1UBg3Uw\n" +
                "EDEOMAwGA1UEAwwFY2Etc20wHhcNMjMxMTA4MDgwNDU4WhcNMzMxMTA1MDgwNDU4\n" +
                "WjATMREwDwYDVQQDDAhpbnRjYS1zbTBZMBMGByqGSM49AgEGCCqBHM9VAYItA0IA\n" +
                "BEBnR4bkc5ft6pvgM2Zns0WVqR/UTfTys9lpFfqRH4makwFK/hdKjNPRdr2vb44W\n" +
                "cs/prJgO10L6z5d0zizcnY+jezB5MB0GA1UdDgQWBBQGQ5MPPbCEScDLOdMBQ7HS\n" +
                "U4CySzAfBgNVHSMEGDAWgBQBt6TgeH5h2ZVgAqzezU+9hK4KljAPBgNVHRMBAf8E\n" +
                "BTADAQH/MA4GA1UdDwEB/wQEAwIBhjAWBgNVHSUBAf8EDDAKBggrBgEFBQcDCTAK\n" +
                "BggqgRzPVQGDdQNIADBFAiAhMwnrFg3RSSykJ3Zo3Ykq2FbSfdc9b2kOUm3K968+\n" +
                "ywIhANZ+0DVhfJFOpcR/IRobTN0lwXUdYeGTIcfb9Gvy4IAj\n" +
                "-----END CERTIFICATE-----",
                "MIGHAgEAMBMGByqGSM49AgEGCCqBHM9VAYItBG0wawIBAQQghICsSnErHdSfGfMJ\n" +
                "klkiXG4VsQYJSNnLjP4bbMqBTSahRANCAARAZ0eG5HOX7eqb4DNmZ7NFlakf1E30\n" +
                "8rPZaRX6kR+JmpMBSv4XSozT0Xa9r2+OFnLP6ayYDtdC+s+XdM4s3J2P"),

        EE_SM2_CURVESM2(
                "EC",
                // SM3withSM2, curve curveSM2
                // Validity
                //     Not Before: Nov  8 08:04:58 2023 GMT
                //     Not After : Nov  5 08:04:58 2033 GMT
                // Authority Key Identifier:
                //     FE:0C:68:E4:86:E6:B5:47:E8:E1:7C:CE:8E:70:FE:58:36:1E:1E:2B
                "-----BEGIN CERTIFICATE-----\n" +
                "MIIBZzCCAQ2gAwIBAgIUdOUfy21j1UH2SxN6RnvmShqkQwgwCgYIKoEcz1UBg3Uw\n" +
                "EzERMA8GA1UEAwwIaW50Y2Etc20wHhcNMjMxMTA4MDgwNDU4WhcNMzMxMTA1MDgw\n" +
                "NDU4WjAQMQ4wDAYDVQQDDAVlZS1zbTBZMBMGByqGSM49AgEGCCqBHM9VAYItA0IA\n" +
                "BPdQ2ynkD1L6cUac/F9IIVBV0mhZvyiGicSeRif2d21JS4TJYNaafpTVWayfjF+5\n" +
                "ZpxvJw2r4pEE7KofE6CSFCWjQjBAMB0GA1UdDgQWBBT+DGjkhua1R+jhfM6OcP5Y\n" +
                "Nh4eKzAfBgNVHSMEGDAWgBQGQ5MPPbCEScDLOdMBQ7HSU4CySzAKBggqgRzPVQGD\n" +
                "dQNIADBFAiEAvKXq041ePnvJxxzS1YjmfRdl/af/Lf0/YfvIhhg9EI4CID9cVT3U\n" +
                "xZuh/JEfTHTU2Vst4MUuPgmg5sMFYLmAHzsp\n" +
                "-----END CERTIFICATE-----",
                "MIGHAgEAMBMGByqGSM49AgEGCCqBHM9VAYItBG0wawIBAQQgcwy6rkyOkWldP7lk\n" +
                "MtJShuyWM1BVotjThmD7KL/1Qc6hRANCAAT3UNsp5A9S+nFGnPxfSCFQVdJoWb8o\n" +
                "honEnkYn9ndtSUuEyWDWmn6U1Vmsn4xfuWacbycNq+KRBOyqHxOgkhQl"),

        CA_RSA(
                "RSA",
                // SHA256withRSA, 2048 bits
                // Validity
                //     Not Before: May 22 07:18:16 2018 GMT
                //     Not After : May 17 07:18:16 2038 GMT
                // Subject Key Identifier:
                //     0D:DD:93:C9:FE:4B:BD:35:B7:E8:99:78:90:FB:DB:5A:3D:DB:15:4C
                "-----BEGIN CERTIFICATE-----\n" +
                "MIIDSTCCAjGgAwIBAgIJAI4ZF3iy8zG+MA0GCSqGSIb3DQEBCwUAMDsxCzAJBgNV\n" +
                "BAYTAlVTMQ0wCwYDVQQKDARKYXZhMR0wGwYDVQQLDBRTdW5KU1NFIFRlc3QgU2Vy\n" +
                "aXZjZTAeFw0xODA1MjIwNzE4MTZaFw0zODA1MTcwNzE4MTZaMDsxCzAJBgNVBAYT\n" +
                "AlVTMQ0wCwYDVQQKDARKYXZhMR0wGwYDVQQLDBRTdW5KU1NFIFRlc3QgU2VyaXZj\n" +
                "ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALpMcY7aWieXDEM1/YJf\n" +
                "JW27b4nRIFZyEYhEloyGsKTuQiiQjc8cqRZFNXe2vwziDB4IyTEl0Hjl5QF6ZaQE\n" +
                "huPzzwvQm1pv64KrRXrmj3FisQK8B5OWLty9xp6xDqsaMRoyObLK+oIb20T5fSlE\n" +
                "evmo1vYjnh8CX0Yzx5Gr5ye6YSEHQvYOWEws8ad17OlyToR2KMeC8w4qo6rs59pW\n" +
                "g7Mxn9vo22ImDzrtAbTbXbCias3xlE0Bp0h5luyf+5U4UgksoL9B9r2oP4GrLNEV\n" +
                "oJk57t8lwaR0upiv3CnS8LcJELpegZub5ggqLY8ZPYFQPjlK6IzLOm6rXPgZiZ3m\n" +
                "RL0CAwEAAaNQME4wHQYDVR0OBBYEFA3dk8n+S701t+iZeJD721o92xVMMB8GA1Ud\n" +
                "IwQYMBaAFA3dk8n+S701t+iZeJD721o92xVMMAwGA1UdEwQFMAMBAf8wDQYJKoZI\n" +
                "hvcNAQELBQADggEBAJTRC3rKUUhVH07/1+stUungSYgpM08dY4utJq0BDk36BbmO\n" +
                "0AnLDMbkwFdHEoqF6hQIfpm7SQTmXk0Fss6Eejm8ynYr6+EXiRAsaXOGOBCzF918\n" +
                "/RuKOzqABfgSU4UBKECLM5bMfQTL60qx+HdbdVIpnikHZOFfmjCDVxoHsGyXc1LW\n" +
                "Jhkht8IGOgc4PMGvyzTtRFjz01kvrVQZ75aN2E0GQv6dCxaEY0i3ypSzjUWAKqDh\n" +
                "3e2OLwUSvumcdaxyCdZAOUsN6pDBQ+8VRG7KxnlRlY1SMEk46QgQYLbPDe/+W/yH\n" +
                "ca4PejicPeh+9xRAwoTpiE2gulfT7Lm+fVM7Ruc=\n" +
                "-----END CERTIFICATE-----",
                "-----BEGIN PRIVATE KEY-----\n" +
                "MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQC6THGO2lonlwxD\n" +
                "Nf2CXyVtu2+J0SBWchGIRJaMhrCk7kIokI3PHKkWRTV3tr8M4gweCMkxJdB45eUB\n" +
                "emWkBIbj888L0Jtab+uCq0V65o9xYrECvAeTli7cvcaesQ6rGjEaMjmyyvqCG9tE\n" +
                "+X0pRHr5qNb2I54fAl9GM8eRq+cnumEhB0L2DlhMLPGndezpck6EdijHgvMOKqOq\n" +
                "7OfaVoOzMZ/b6NtiJg867QG0212womrN8ZRNAadIeZbsn/uVOFIJLKC/Qfa9qD+B\n" +
                "qyzRFaCZOe7fJcGkdLqYr9wp0vC3CRC6XoGbm+YIKi2PGT2BUD45SuiMyzpuq1z4\n" +
                "GYmd5kS9AgMBAAECggEAFHSoU2MuWwJ+2jJnb5U66t2V1bAcuOE1g5zkWvG/G5z9\n" +
                "rq6Qo5kmB8f5ovdx6tw3MGUOklLwnRXBG3RxDJ1iokz3AvkY1clMNsDPlDsUrQKF\n" +
                "JSO4QUBQTPSZhnsyfR8XHSU+qJ8Y+ohMfzpVv95BEoCzebtXdVgxVegBlcEmVHo2\n" +
                "kMmkRN+bYNsr8eb2r+b0EpyumS39ZgKYh09+cFb78y3T6IFMGcVJTP6nlGBFkmA/\n" +
                "25pYeCF2tSki08qtMJZQAvKfw0Kviibk7ZxRbJqmc7B1yfnOEHP6ftjuvKl2+RP/\n" +
                "+5P5f8CfIP6gtA0LwSzAqQX/hfIKrGV5j0pCqrD0kQKBgQDeNR6Xi4sXVq79lihO\n" +
                "a1bSeV7r8yoQrS8x951uO+ox+UIZ1MsAULadl7zB/P0er92p198I9M/0Jth3KBuS\n" +
                "zj45mucvpiiGvmQlMKMEfNq4nN7WHOu55kufPswQB2mR4J3xmwI+4fM/nl1zc82h\n" +
                "De8JSazRldJXNhfx0RGFPmgzbwKBgQDWoVXrXLbCAn41oVnWB8vwY9wjt92ztDqJ\n" +
                "HMFA/SUohjePep9UDq6ooHyAf/Lz6oE5NgeVpPfTDkgvrCFVKnaWdwALbYoKXT2W\n" +
                "9FlyJox6eQzrtHAacj3HJooXWuXlphKSizntfxj3LtMR9BmrmRJOfK+SxNOVJzW2\n" +
                "+MowT20EkwKBgHmpB8jdZBgxI7o//m2BI5Y1UZ1KE5vx1kc7VXzHXSBjYqeV9FeF\n" +
                "2ZZLP9POWh/1Fh4pzTmwIDODGT2UPhSQy0zq3O0fwkyT7WzXRknsuiwd53u/dejg\n" +
                "iEL2NPAJvulZ2+AuiHo5Z99LK8tMeidV46xoJDDUIMgTG+UQHNGhK5gNAoGAZn/S\n" +
                "Cn7SgMC0CWSvBHnguULXZO9wH1wZAFYNLL44OqwuaIUFBh2k578M9kkke7woTmwx\n" +
                "HxQTjmWpr6qimIuY6q6WBN8hJ2Xz/d1fwhYKzIp20zHuv5KDUlJjbFfqpsuy3u1C\n" +
                "kts5zwI7pr1ObRbDGVyOdKcu7HI3QtR5qqyjwaUCgYABo7Wq6oHva/9V34+G3Goh\n" +
                "63bYGUnRw2l5BD11yhQv8XzGGZFqZVincD8gltNThB0Dc/BI+qu3ky4YdgdZJZ7K\n" +
                "z51GQGtaHEbrHS5caV79yQ8QGY5mUVH3E+VXSxuIqb6pZq2DH4sTAEFHyncddmOH\n" +
                "zoXBInYwRG9KE/Bw5elhUw==\n" +
                "-----END PRIVATE KEY-----"),
        EE_RSA(
                "RSA",
                // SHA256withRSA, 2048 bits
                // Validity
                //     Not Before: May 22 07:18:16 2018 GMT
                //     Not After : May 17 07:18:16 2038 GMT
                // Authority Key Identifier:
                //     0D:DD:93:C9:FE:4B:BD:35:B7:E8:99:78:90:FB:DB:5A:3D:DB:15:4C
                "-----BEGIN CERTIFICATE-----\n" +
                "MIIDNjCCAh6gAwIBAgIJAO2+yPcFryUTMA0GCSqGSIb3DQEBCwUAMDsxCzAJBgNV\n" +
                "BAYTAlVTMQ0wCwYDVQQKDARKYXZhMR0wGwYDVQQLDBRTdW5KU1NFIFRlc3QgU2Vy\n" +
                "aXZjZTAeFw0xODA1MjIwNzE4MTZaFw0zODA1MTcwNzE4MTZaMFUxCzAJBgNVBAYT\n" +
                "AlVTMQ0wCwYDVQQKDARKYXZhMR0wGwYDVQQLDBRTdW5KU1NFIFRlc3QgU2VyaXZj\n" +
                "ZTEYMBYGA1UEAwwPUmVncmVzc2lvbiBUZXN0MIIBIjANBgkqhkiG9w0BAQEFAAOC\n" +
                "AQ8AMIIBCgKCAQEAszfBobWfZIp8AgC6PiWDDavP65mSvgCXUGxACbxVNAfkLhNR\n" +
                "QOsHriRB3X1Q3nvO9PetC6wKlvE9jlnDDj7D+1j1r1CHO7ms1fq8rfcQYdkanDtu\n" +
                "4AlHo8v+SSWX16MIXFRYDj2VVHmyPtgbltcg4zGAuwT746FdLI94uXjJjq1IOr/v\n" +
                "0VIlwE5ORWH5Xc+5Tj+oFWK0E4a4GHDgtKKhn2m72hN56/GkPKGkguP5NRS1qYYV\n" +
                "/EFkdyQMOV8J1M7HaicSft4OL6eKjTrgo93+kHk+tv0Dc6cpVBnalX3TorG8QI6B\n" +
                "cHj1XQd78oAlAC+/jF4pc0mwi0un49kdK9gRfQIDAQABoyMwITAfBgNVHSMEGDAW\n" +
                "gBQN3ZPJ/ku9NbfomXiQ+9taPdsVTDANBgkqhkiG9w0BAQsFAAOCAQEApXS0nKwm\n" +
                "Kp8gpmO2yG1rpd1+2wBABiMU4JZaTqmma24DQ3RzyS+V2TeRb29dl5oTUEm98uc0\n" +
                "GPZvhK8z5RFr4YE17dc04nI/VaNDCw4y1NALXGs+AHkjoPjLyGbWpi1S+gfq2sNB\n" +
                "Ekkjp6COb/cb9yiFXOGVls7UOIjnVZVd0r7KaPFjZhYh82/f4PA/A1SnIKd1+nfH\n" +
                "2yk7mSJNC7Z3qIVDL8MM/jBVwiC3uNe5GPB2uwhd7k5LGAVN3j4HQQGB0Sz+VC1h\n" +
                "92oi6xDa+YBva2fvHuCd8P50DDjxmp9CemC7rnZ5j8egj88w14X44Xjb/Fd/ApG9\n" +
                "e57NnbT7KM+Grw==\n" +
                "-----END CERTIFICATE-----",
                "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCzN8GhtZ9kinwC\n" +
                "ALo+JYMNq8/rmZK+AJdQbEAJvFU0B+QuE1FA6weuJEHdfVDee870960LrAqW8T2O\n" +
                "WcMOPsP7WPWvUIc7uazV+ryt9xBh2RqcO27gCUejy/5JJZfXowhcVFgOPZVUebI+\n" +
                "2BuW1yDjMYC7BPvjoV0sj3i5eMmOrUg6v+/RUiXATk5FYfldz7lOP6gVYrQThrgY\n" +
                "cOC0oqGfabvaE3nr8aQ8oaSC4/k1FLWphhX8QWR3JAw5XwnUzsdqJxJ+3g4vp4qN\n" +
                "OuCj3f6QeT62/QNzpylUGdqVfdOisbxAjoFwePVdB3vygCUAL7+MXilzSbCLS6fj\n" +
                "2R0r2BF9AgMBAAECggEASIkPkMCuw4WdTT44IwERus3IOIYOs2IP3BgEDyyvm4B6\n" +
                "JP/iihDWKfA4zEl1Gqcni1RXMHswSglXra682J4kui02Ov+vzEeJIY37Ibn2YnP5\n" +
                "ZjRT2s9GtI/S2o4hl8A/mQb2IMViFC+xKehTukhV4j5d6NPKk0XzLR7gcMjnYxwn\n" +
                "l21fS6D2oM1xRG/di7sL+uLF8EXLRzfiWDNi12uQv4nwtxPKvuKhH6yzHt7YqMH0\n" +
                "46pmDKDaxV4w1JdycjCb6NrCJOYZygoQobuZqOQ30UZoZsPJrtovkncFr1e+lNcO\n" +
                "+aWDfOLCtTH046dEQh5oCShyXMybNlry/QHsOtHOwQKBgQDh2iIjs+FPpQy7Z3EX\n" +
                "DGEvHYqPjrYO9an2KSRr1m9gzRlWYxKY46WmPKwjMerYtra0GP+TBHrgxsfO8tD2\n" +
                "wUAII6sd1qup0a/Sutgf2JxVilLykd0+Ge4/Cs51tCdJ8EqDV2B6WhTewOY2EGvg\n" +
                "JiKYkeNwgRX/9M9CFSAMAk0hUQKBgQDLJAartL3DoGUPjYtpJnfgGM23yAGl6G5r\n" +
                "NSXDn80BiYIC1p0bG3N0xm3yAjqOtJAUj9jZbvDNbCe3GJfLARMr23legX4tRrgZ\n" +
                "nEdKnAFKAKL01oM+A5/lHdkwaZI9yyv+hgSVdYzUjB8rDmzeVQzo1BT7vXypt2yV\n" +
                "6O1OnUpCbQKBgA/0rzDChopv6KRcvHqaX0tK1P0rYeVQqb9ATNhpf9jg5Idb3HZ8\n" +
                "rrk91BNwdVz2G5ZBpdynFl9G69rNAMJOCM4KZw5mmh4XOEq09Ivba8AHU7DbaTv3\n" +
                "7QL7KnbaUWRB26HHzIMYVh0el6T+KADf8NXCiMTr+bfpfbL3dxoiF3zhAoGAbCJD\n" +
                "Qse1dBs/cKYCHfkSOsI5T6kx52Tw0jS6Y4X/FOBjyqr/elyEexbdk8PH9Ar931Qr\n" +
                "NKMvn8oA4iA/PRrXX7M2yi3YQrWwbkGYWYjtzrzEAdzmg+5eARKAeJrZ8/bg9l3U\n" +
                "ttKaItJsDPlizn8rngy3FsJpR9aSAMK6/+wOiYkCgYEA1tZkI1rD1W9NYZtbI9BE\n" +
                "qlJVFi2PBOJMKNuWdouPX3HLQ72GJSQff2BFzLTELjweVVJ0SvY4IipzpQOHQOBy\n" +
                "5qh/p6izXJZh3IHtvwVBjHoEVplg1b2+I5e3jDCfqnwcQw82dW5SxOJMg1h/BD0I\n" +
                "qAL3go42DYeYhu/WnECMeis=");

        final String keyAlgo;
        final String certStr;
        final String privKeyStr;

        Cert(String keyAlgo, String certStr, String privKeyStr) {
            this.keyAlgo = keyAlgo;
            this.certStr = certStr;
            this.privKeyStr = privKeyStr;
        }
    }
}
