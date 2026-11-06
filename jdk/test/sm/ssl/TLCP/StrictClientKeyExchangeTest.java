/*
 * Copyright (c) 2026, Tencent. All rights reserved.
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

//
// Please run in othervm mode.  SunJSSE does not support dynamic system
// properties, no way to re-use system properties in samevm/agentvm mode.
//

/*
 * @test
 * @summary TLCP 1.1 ephemeral SM2 (ECDHE) handshake with GB/T 38636-2020
 *          compliant ClientKeyExchange framing enabled via
 *          -Djdk.tlcp.strictClientKeyExchange=true.  A successful
 *          handshake proves the client emits a well-formed length-prefixed
 *          ClientECDHEParams and the server accepts it.
 * @compile ../../Utils.java SSLSocketTest.java
 * @run main/othervm -Djdk.tls.client.protocols=TLCPv1.1
 *                   -Djdk.tls.client.cipherSuites=TLCP_ECDHE_SM4_CBC_SM3
 *                   -Djdk.tlcp.strictClientKeyExchange=true
 *                   StrictClientKeyExchangeTest
 * @run main/othervm -Djdk.tls.client.protocols=TLCPv1.1
 *                   -Djdk.tls.client.cipherSuites=TLCP_ECDHE_SM4_GCM_SM3
 *                   -Djdk.tlcp.strictClientKeyExchange=true
 *                   StrictClientKeyExchangeTest
 */

public class StrictClientKeyExchangeTest extends SSLSocketTest {

    public static void main(String[] args) throws Exception {
        new StrictClientKeyExchangeTest().run();
    }
}
