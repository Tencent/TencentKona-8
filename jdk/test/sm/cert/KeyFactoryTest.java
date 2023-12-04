/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.KeyFactory;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECPrivateKeySpec;
import java.security.spec.ECPublicKeySpec;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotNull;

/*
 * @test
 * @summary Test KeyFactory on SM certificate.
 * @compile ../Utils.java
 * @run testng KeyFactoryTest
 */
public class KeyFactoryTest {

    private static final String TEST_BASE = System.getProperty("test.src");
    private static final Path CERT_DIR = Paths.get(TEST_BASE)
            .resolve("..").resolve("certs");

    @Test
    public void testGetKeySpecs() throws Exception {
        X509Certificate x509Cert = Utils.x509CertAsFile(
                CERT_DIR.resolve("ca-sm.crt"));
        PublicKey publicKey = x509Cert.getPublicKey();

        KeyFactory keyFactory = KeyFactory.getInstance("EC");

        ECPublicKeySpec publicKeySpec = keyFactory.getKeySpec(
                publicKey, ECPublicKeySpec.class);
        assertNotNull(publicKeySpec);

        X509EncodedKeySpec x509KeySpec = keyFactory.getKeySpec(
                publicKey, X509EncodedKeySpec.class);
        assertNotNull(x509KeySpec);

        PrivateKey privateKey = Utils.ecPrivateKeyAsFile(
                CERT_DIR.resolve("ca-sm.key"));

        ECPrivateKeySpec ecPrivateKeySpec = keyFactory.getKeySpec(
                privateKey, ECPrivateKeySpec.class);
        assertNotNull(ecPrivateKeySpec);

        PKCS8EncodedKeySpec pkcs8KeySpec = keyFactory.getKeySpec(
                privateKey, PKCS8EncodedKeySpec.class);
        assertNotNull(pkcs8KeySpec);
    }

    @Test
    public void testGeneratePublicKey() throws Exception {
        testGeneratePublicKey(CERT_DIR.resolve("ca-sm.crt"));
        testGeneratePublicKey(CERT_DIR.resolve("intca-sm.crt"));
        testGeneratePublicKey(CERT_DIR.resolve("ee-sm.crt"));
    }

    private void testGeneratePublicKey(Path certPath) throws Exception {
        X509Certificate x509Cert = Utils.x509CertAsFile(certPath);
        ECPublicKey publicKey = (ECPublicKey) x509Cert.getPublicKey();

        KeyFactory keyFactory = KeyFactory.getInstance("EC");

        ECPublicKeySpec ecPublicKeySpec = keyFactory.getKeySpec(
                publicKey, ECPublicKeySpec.class);
        ECPublicKey ecPublicKey = (ECPublicKey) keyFactory.generatePublic(
                ecPublicKeySpec);
        assertEquals(publicKey.getW(), ecPublicKey.getW());

        X509EncodedKeySpec x509KeySpec = keyFactory.getKeySpec(
                publicKey, X509EncodedKeySpec.class);
        ECPublicKey x509Key = (ECPublicKey) keyFactory.generatePublic(x509KeySpec);
        assertEquals(
                publicKey.getEncoded(), x509Key.getEncoded());
    }

    @Test
    public void testGeneratePrivateKey() throws Exception {
        testGenerateECPrivateKey(Utils.ecPrivateKeyAsFile(
                CERT_DIR.resolve("ca-sm.key")));
        testGenerateECPrivateKey(Utils.ecPrivateKeyAsFile(
                CERT_DIR.resolve("intca-sm.key")));
        testGenerateECPrivateKey(Utils.ecPrivateKeyAsFile(
                CERT_DIR.resolve("ee-sm.key")));
    }

    private void testGenerateECPrivateKey(ECPrivateKey privateKey)
            throws Exception {
        KeyFactory keyFactory = KeyFactory.getInstance("EC");

        ECPrivateKeySpec ecPrivateKeySpec = keyFactory.getKeySpec(
                privateKey, ECPrivateKeySpec.class);
        ECPrivateKey ecPrivateKey = (ECPrivateKey) keyFactory.generatePrivate(
                ecPrivateKeySpec);
        assertEquals(privateKey.getS(), ecPrivateKey.getS());

        PKCS8EncodedKeySpec pkcs8KeySpec = keyFactory.getKeySpec(
                privateKey, PKCS8EncodedKeySpec.class);
        ECPrivateKey pkcs8Key = (ECPrivateKey) keyFactory.generatePrivate(
                pkcs8KeySpec);
        assertEquals(
                privateKey.getEncoded(), pkcs8Key.getEncoded());
    }
}
