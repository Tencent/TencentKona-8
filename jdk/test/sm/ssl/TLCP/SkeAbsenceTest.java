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
 * @summary In a TLCP ECC static (K_SM2) handshake the ServerKeyExchange
 *          carries the only cryptographic binding between the server's sign
 *          certificate and enc certificate.  The client must refuse to
 *          continue if it is missing (and must reject an invalid one), and
 *          must not send ClientKeyExchange in either case.
 * @compile SSLEngineTest.java ../../Utils.java
 * @run main/othervm SkeAbsenceTest
 * @run main/othervm -Djdk.tlcp.dualCertCompat=true SkeAbsenceTest
 */

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLEngineResult;
import javax.net.ssl.SSLException;
import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;

public class SkeAbsenceTest {

    private static final String ECC_CBC = "TLCP_ECC_SM4_CBC_SM3";
    private static final String ECC_GCM = "TLCP_ECC_SM4_GCM_SM3";
    private static final String ECDHE_CBC = "TLCP_ECDHE_SM4_CBC_SM3";

    private enum Tamper {
        NONE,            // faithful pass-through
        DROP_SKE,        // remove the ServerKeyExchange from the stream
        CORRUPT_SKE_SIG  // flip the last signature byte of the ServerKeyExchange
    }

    public static void main(String[] args) throws Exception {
        // SKE missing: both ECC static suites must be rejected by the guard.
        for (String suite : new String[] { ECC_CBC, ECC_GCM }) {
            Result r = handshake(suite, Tamper.DROP_SKE, false);
            check(!r.clientFinished, suite + ": handshake must fail");
            check(r.skeDropped == 1, suite + ": exactly one SKE must be dropped");
            check(r.failure != null, suite + ": client must raise an exception");
            String msg = String.valueOf(r.failure);
            // JDK 8 exception messages do not carry the alert name; the
            // alert itself is verified below via the emitted record.
            check(msg.contains("ServerKeyExchange is required"),
                    suite + ": rejection must be the SKE-presence guard, was: "
                            + msg);
            check(!r.clientKeyExchangeSent,
                    suite + ": client must NOT send ClientKeyExchange");
            check(r.alertDescription == 10,
                    suite + ": client must emit an unexpected_message alert, "
                            + "was: " + r.alertDescription);
        }

        // SKE present but forged: must fail at signature verification.
        for (String suite : new String[] { ECC_CBC, ECC_GCM }) {
            Result r = handshake(suite, Tamper.CORRUPT_SKE_SIG, false);
            check(!r.clientFinished, suite + ": handshake must fail");
            check(r.skeCorrupted == 1,
                    suite + ": exactly one SKE signature must be corrupted");
            check(r.failure != null, suite + ": client must raise an exception");
            String msg = String.valueOf(r.failure);
            check(msg.contains("SM2 ServerKeyExchange signature"),
                    suite + ": rejection must be SKE signature verification, "
                            + "was: " + msg);
            check(!r.clientKeyExchangeSent,
                    suite + ": client must NOT send ClientKeyExchange");
            check(r.alertDescription == 40,
                    suite + ": client must emit a handshake_failure alert, "
                            + "was: " + r.alertDescription);
        }

        // Untouched handshakes still succeed.
        for (String suite : new String[] { ECC_CBC, ECC_GCM }) {
            Result r = handshake(suite, Tamper.NONE, false);
            check(r.failure == null, suite + ": normal handshake failed: "
                    + r.failure);
            check(r.clientFinished, suite + ": normal handshake must complete");
            check(r.clientKeyExchangeSent,
                    suite + ": a successful handshake must send "
                            + "ClientKeyExchange");
        }

        // ECDHE (K_SM2E) is out of scope for the guard; it must still work.
        Result r = handshake(ECDHE_CBC, Tamper.NONE, true);
        check(r.failure == null, "ECDHE handshake failed: " + r.failure);
        check(r.clientFinished, "ECDHE handshake must complete");

        System.out.println("PASSED");
    }

    private static void check(boolean cond, String message) {
        if (!cond) {
            throw new RuntimeException(message);
        }
    }

    private static final class Result {
        boolean clientFinished;
        boolean clientKeyExchangeSent;
        int alertDescription = -1;
        Throwable failure;
        int skeDropped;
        int skeCorrupted;
    }

    private static Result handshake(String suite, Tamper tamper,
            boolean mutualAuth) throws Exception {
        SSLEngineTest.ContextParameters params =
                new SSLEngineTest.ContextParameters("TLCP", "PKIX", "NewSunX509");

        SSLContext serverCtx = SSLEngineTest.createSSLContext(
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.CA_CERT },
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.SERVER_SIGN_CERT,
                        SSLEngineTest.Cert.SERVER_ENC_CERT },
                params);
        SSLContext clientCtx = SSLEngineTest.createSSLContext(
                new SSLEngineTest.Cert[] { SSLEngineTest.Cert.CA_CERT },
                mutualAuth
                        ? new SSLEngineTest.Cert[] {
                                SSLEngineTest.Cert.CLIENT_SIGN_CERT,
                                SSLEngineTest.Cert.CLIENT_ENC_CERT }
                        : null,
                params);

        SSLEngine server = serverCtx.createSSLEngine();
        server.setUseClientMode(false);
        server.setNeedClientAuth(mutualAuth);
        server.setEnabledProtocols(new String[] { "TLCPv1.1" });
        server.setEnabledCipherSuites(new String[] { suite });

        SSLEngine client = clientCtx.createSSLEngine();
        client.setUseClientMode(true);
        client.setEnabledProtocols(new String[] { "TLCPv1.1" });
        client.setEnabledCipherSuites(new String[] { suite });

        int netMax = client.getSession().getPacketBufferSize();
        int appMax = client.getSession().getApplicationBufferSize();
        ByteBuffer wrapOut = ByteBuffer.allocate(netMax + 64);
        ByteBuffer appIn = ByteBuffer.allocate(appMax + 64);
        ByteBuffer empty = ByteBuffer.allocate(0);

        RecordRewriter rewriter = new RecordRewriter(tamper);
        RecordScanner scanner = new RecordScanner();
        Result result = new Result();

        client.beginHandshake();
        server.beginHandshake();

        ByteArrayOutputStream toClient = new ByteArrayOutputStream();
        ByteArrayOutputStream toServer = new ByteArrayOutputStream();

        try {
            for (int guard = 0; guard < 500; guard++) {
                boolean acted = false;

                // Server flight; the rewriter drops/corrupts the SKE in transit.
                while (server.getHandshakeStatus()
                        == SSLEngineResult.HandshakeStatus.NEED_WRAP) {
                    wrapOut.clear();
                    server.wrap(empty, wrapOut);
                    wrapOut.flip();
                    if (wrapOut.hasRemaining()) {
                        byte[] raw = new byte[wrapOut.remaining()];
                        wrapOut.get(raw);
                        toClient.write(rewriter.feed(raw));
                    }
                    runTasks(server);
                    acted = true;
                }

                // Client consumes the (tampered) bytes; the guard or the
                // signature check throws here.
                if (toClient.size() > 0) {
                    ByteBuffer in = ByteBuffer.wrap(toClient.toByteArray());
                    while (in.hasRemaining()) {
                        appIn.clear();
                        SSLEngineResult res = client.unwrap(in, appIn);
                        runTasks(client);
                        acted = true;
                        if (res.getHandshakeStatus()
                                == SSLEngineResult.HandshakeStatus.FINISHED) {
                            result.clientFinished = true;
                        }
                        if (res.getStatus() == SSLEngineResult.Status.BUFFER_UNDERFLOW
                                || res.getStatus() == SSLEngineResult.Status.CLOSED
                                || (res.bytesConsumed() == 0
                                    && res.getStatus() == SSLEngineResult.Status.OK)) {
                            break;
                        }
                    }
                    byte[] rem = new byte[in.remaining()];
                    in.get(rem);
                    toClient.reset();
                    toClient.write(rem);
                }

                // Client flight; watch for a ClientKeyExchange.
                while (client.getHandshakeStatus()
                        == SSLEngineResult.HandshakeStatus.NEED_WRAP) {
                    wrapOut.clear();
                    SSLEngineResult res = client.wrap(empty, wrapOut);
                    wrapOut.flip();
                    if (wrapOut.hasRemaining()) {
                        byte[] raw = new byte[wrapOut.remaining()];
                        wrapOut.get(raw);
                        scanner.scan(raw);
                        if (scanner.sawClientKeyExchange) {
                            result.clientKeyExchangeSent = true;
                        }
                        toServer.write(raw);
                    }
                    runTasks(client);
                    acted = true;
                    if (res.getHandshakeStatus()
                            == SSLEngineResult.HandshakeStatus.FINISHED) {
                        result.clientFinished = true;
                    }
                }

                // Server consumes client bytes; its failure is irrelevant.
                if (toServer.size() > 0) {
                    ByteBuffer in = ByteBuffer.wrap(toServer.toByteArray());
                    try {
                        while (in.hasRemaining()) {
                            appIn.clear();
                            SSLEngineResult res = server.unwrap(in, appIn);
                            runTasks(server);
                            acted = true;
                            if (res.getStatus() == SSLEngineResult.Status.BUFFER_UNDERFLOW
                                    || res.getStatus() == SSLEngineResult.Status.CLOSED
                                    || (res.bytesConsumed() == 0
                                        && res.getStatus() == SSLEngineResult.Status.OK)) {
                                break;
                            }
                        }
                    } catch (SSLException ignored) {
                        // the server legitimately breaks after the client aborts
                    }
                    byte[] rem = new byte[in.remaining()];
                    in.get(rem);
                    toServer.reset();
                    toServer.write(rem);
                }

                if (result.clientFinished && toClient.size() == 0
                        && toServer.size() == 0) {
                    break;
                }
                if (!acted) {
                    break;
                }
            }
        } catch (SSLException e) {
            result.failure = e;
            // The engine may still hold queued handshake output (e.g. a
            // ClientKeyExchange produced before the fatal alert), so drain
            // it and scan everything the client actually emits.
            drainClientOutbound(client, wrapOut, scanner, result);
        }

        result.alertDescription = scanner.alertDescription;
        result.skeDropped = rewriter.droppedSke;
        result.skeCorrupted = rewriter.corruptedSke;
        return result;
    }

    private static void drainClientOutbound(SSLEngine client,
            ByteBuffer wrapOut, RecordScanner scanner, Result result) {
        ByteBuffer empty = ByteBuffer.allocate(0);
        for (int i = 0; i < 20 && !client.isOutboundDone(); i++) {
            wrapOut.clear();
            int produced;
            try {
                produced = client.wrap(empty, wrapOut).bytesProduced();
            } catch (SSLException e) {
                // A rethrown fatal; keep draining within the bound.
                continue;
            }
            wrapOut.flip();
            if (produced > 0) {
                byte[] raw = new byte[wrapOut.remaining()];
                wrapOut.get(raw);
                scanner.scan(raw);
                if (scanner.sawClientKeyExchange) {
                    result.clientKeyExchangeSent = true;
                }
            }
            runTasks(client);
            if (produced == 0
                    && client.getHandshakeStatus()
                        != SSLEngineResult.HandshakeStatus.NEED_WRAP) {
                break;
            }
        }
    }

    private static void runTasks(SSLEngine engine) {
        if (engine.getHandshakeStatus()
                == SSLEngineResult.HandshakeStatus.NEED_TASK) {
            Runnable task;
            while ((task = engine.getDelegatedTask()) != null) {
                task.run();
            }
        }
    }

    // Read-only scanner for the client->server plaintext handshake records
    // (everything before ChangeCipherSpec is plaintext in TLCP 1.1).
    private static final class RecordScanner {
        private final ByteArrayOutputStream recordBuf = new ByteArrayOutputStream();
        private final ByteArrayOutputStream hsBuf = new ByteArrayOutputStream();
        private boolean raw = false;
        boolean sawClientKeyExchange = false;
        int alertDescription = -1;       // description of the last alert sent

        void scan(byte[] data) {
            if (raw) {
                return;
            }
            recordBuf.write(data, 0, data.length);
            while (!raw) {
                byte[] all = recordBuf.toByteArray();
                if (all.length < 5) {
                    break;
                }
                int type = all[0] & 0xFF;
                int recLen = ((all[3] & 0xFF) << 8) | (all[4] & 0xFF);
                if (all.length < 5 + recLen) {
                    break;
                }
                if (type == 20) {               // ChangeCipherSpec -> ciphertext
                    raw = true;
                    break;
                }
                if (type == 22) {               // handshake
                    hsBuf.write(all, 5, recLen);
                    drainHandshake();
                } else if (type == 21 && recLen == 2) {     // plaintext alert
                    alertDescription = all[6] & 0xFF;
                }
                recordBuf.reset();
                recordBuf.write(all, 5 + recLen, all.length - 5 - recLen);
            }
        }

        private void drainHandshake() {
            while (true) {
                byte[] hb = hsBuf.toByteArray();
                if (hb.length < 4) {
                    break;
                }
                int msgType = hb[0] & 0xFF;
                int msgLen = ((hb[1] & 0xFF) << 16)
                        | ((hb[2] & 0xFF) << 8) | (hb[3] & 0xFF);
                if (hb.length < 4 + msgLen) {
                    break;
                }
                if (msgType == 16) {            // ClientKeyExchange
                    sawClientKeyExchange = true;
                }
                hsBuf.reset();
                hsBuf.write(hb, 4 + msgLen, hb.length - 4 - msgLen);
            }
        }
    }

    // Server->client record rewriter: reassembles handshake messages across
    // record boundaries and drops/corrupts the SKE (handshake type 12).
    private static final class RecordRewriter {
        final Tamper tamper;
        private final ByteArrayOutputStream recordBuf = new ByteArrayOutputStream();
        private final ByteArrayOutputStream hsBuf = new ByteArrayOutputStream();
        private boolean raw = false;
        private int verHi = 0x01;
        private int verLo = 0x01;
        int droppedSke = 0;
        int corruptedSke = 0;

        RecordRewriter(Tamper tamper) {
            this.tamper = tamper;
        }

        byte[] feed(byte[] data) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            if (raw) {
                out.write(data, 0, data.length);
                return out.toByteArray();
            }
            recordBuf.write(data, 0, data.length);
            while (!raw) {
                byte[] all = recordBuf.toByteArray();
                if (all.length < 5) {
                    break;
                }
                int type = all[0] & 0xFF;
                verHi = all[1] & 0xFF;
                verLo = all[2] & 0xFF;
                int recLen = ((all[3] & 0xFF) << 8) | (all[4] & 0xFF);
                if (all.length < 5 + recLen) {
                    break;
                }
                byte[] body = new byte[recLen];
                System.arraycopy(all, 5, body, 0, recLen);
                recordBuf.reset();
                recordBuf.write(all, 5 + recLen, all.length - 5 - recLen);

                if (type == 20) {               // ChangeCipherSpec
                    writeRecord(out, type, body);
                    raw = true;
                } else if (type == 22) {        // handshake
                    byte[] emitted = rewriteHandshake(body);
                    if (emitted.length > 0) {
                        writeRecord(out, type, emitted);
                    }
                } else {
                    writeRecord(out, type, body);
                }
            }
            return out.toByteArray();
        }

        private byte[] rewriteHandshake(byte[] body) {
            hsBuf.write(body, 0, body.length);
            ByteArrayOutputStream emitted = new ByteArrayOutputStream();
            while (true) {
                byte[] hb = hsBuf.toByteArray();
                if (hb.length < 4) {
                    break;
                }
                int msgType = hb[0] & 0xFF;
                int msgLen = ((hb[1] & 0xFF) << 16)
                        | ((hb[2] & 0xFF) << 8) | (hb[3] & 0xFF);
                if (hb.length < 4 + msgLen) {
                    break;
                }
                byte[] msg = new byte[4 + msgLen];
                System.arraycopy(hb, 0, msg, 0, msg.length);
                hsBuf.reset();
                hsBuf.write(hb, msg.length, hb.length - msg.length);

                if (msgType == 12) {            // ServerKeyExchange
                    if (tamper == Tamper.DROP_SKE) {
                        droppedSke++;
                        continue;
                    }
                    if (tamper == Tamper.CORRUPT_SKE_SIG && msg.length > 4) {
                        // The last byte lies within the trailing signature.
                        msg[msg.length - 1] ^= 0x55;
                        corruptedSke++;
                    }
                }
                emitted.write(msg, 0, msg.length);
            }
            return emitted.toByteArray();
        }

        private void writeRecord(ByteArrayOutputStream out, int type, byte[] body) {
            out.write(type);
            out.write(verHi);
            out.write(verLo);
            out.write((body.length >> 8) & 0xFF);
            out.write(body.length & 0xFF);
            out.write(body, 0, body.length);
        }
    }
}
