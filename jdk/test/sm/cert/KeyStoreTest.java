/*
 * Copyright (C) 2022, 2024, Tencent. All rights reserved.
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
 * @summary Test KeyStore on SM certificate.
 * @compile ../Utils.java
 * @run testng KeyStoreTest
 */

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.KeyStore;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECPrivateKey;
import java.security.spec.SM2ParameterSpec;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotNull;

public class KeyStoreTest {

    private static final String TEST_BASE = System.getProperty("test.src");
    private static final Path CERT_DIR = Paths.get(TEST_BASE)
            .resolve("..").resolve("certs");

    private static final String PASSWD = "password";
    private static final char[] PASSWD_CHARS = PASSWD.toCharArray();
    private static final char[] EMPTY_PASSWD = new char[0];

    @Test
    public void testCreateTrustStore() throws Exception {
        testCreateTrustStore("PKCS12");
        testCreateTrustStore("JKS");
    }

    private void testCreateTrustStore(String type) throws Exception {
        KeyStore keyStore = KeyStore.getInstance(type);
        keyStore.load(null, null);

        keyStore.setCertificateEntry("ca-sm",
                Utils.x509CertAsFile(CERT_DIR.resolve("ca-sm.crt")));

        X509Certificate caCert = (X509Certificate) keyStore.getCertificate("ca-sm");
        assertEquals("EC", caCert.getPublicKey().getAlgorithm());
        assertEquals("SM3withSM2", caCert.getSigAlgName());
    }

    @Test
    public void testCreateKeyStore() throws Exception {
        testCreateKeyStore("PKCS12");
        testCreateKeyStore("JKS");
    }

    private void testCreateKeyStore(String type) throws Exception {
        KeyStore keyStore = KeyStore.getInstance(type);
        keyStore.load(null, null);

        keyStore.setKeyEntry(
                "ee-sm",
                Utils.ecPrivateKeyAsFile(CERT_DIR.resolve("ee-sm.key")),
                EMPTY_PASSWD,
                new Certificate[] {
                        Utils.x509CertAsFile(CERT_DIR.resolve("ee-sm.crt")),
                        Utils.x509CertAsFile(CERT_DIR.resolve("intca-sm.crt")),
                        Utils.x509CertAsFile(CERT_DIR.resolve("ca-sm.crt")) });

        ECPrivateKey eeEcPrivateKey
                = (ECPrivateKey) keyStore.getKey("ee-sm", EMPTY_PASSWD);
        assertEquals("EC", eeEcPrivateKey.getAlgorithm());
        assertEquals(
                SM2ParameterSpec.instance().getCurve(),
                eeEcPrivateKey.getParams().getCurve());
    }

    @Test
    public void testSaveAndLoadKeyStore() throws Exception {
        testSaveAndLoadKeyStore("PKCS12");
        testSaveAndLoadKeyStore("JKS");
    }

    private void testSaveAndLoadKeyStore(String type) throws Exception {
        KeyStore keyStore = KeyStore.getInstance(type);
        keyStore.load(null, null);

        keyStore.setCertificateEntry("ca-sm",
                Utils.x509CertAsFile(CERT_DIR.resolve("ca-sm.crt")));

        keyStore.setKeyEntry(
                "ee-sm",
                Utils.ecPrivateKeyAsFile(CERT_DIR.resolve("ee-sm.key")),
                PASSWD_CHARS,
                new Certificate[] {
                        Utils.x509CertAsFile(CERT_DIR.resolve("ee-sm.crt")),
                        Utils.x509CertAsFile(CERT_DIR.resolve("intca-sm.crt")),
                        Utils.x509CertAsFile(CERT_DIR.resolve("ca-sm.crt")) });

        Path tempKeyStoreFile = Files.createTempDirectory(
                Paths.get("."), "pkix").resolve("test.ks");
        try (FileOutputStream out = new FileOutputStream(
                tempKeyStoreFile.toFile())) {
            keyStore.store(out, PASSWD_CHARS);
        }

        KeyStore loadedKeyStore = KeyStore.getInstance(type);
        try (FileInputStream keyStoreIn
                = new FileInputStream(tempKeyStoreFile.toFile())) {
            loadedKeyStore.load(keyStoreIn, PASSWD_CHARS);
        } finally {
            Files.deleteIfExists(tempKeyStoreFile);
            Files.deleteIfExists(tempKeyStoreFile.getParent());
        }

        assertNotNull(loadedKeyStore.getCertificate("ca-sm"));
        assertNotNull(loadedKeyStore.getKey("ee-sm", PASSWD_CHARS));
    }
}
