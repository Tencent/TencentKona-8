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

import java.io.ByteArrayInputStream;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.cert.CertPath;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.X509CRL;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

import sun.security.x509.AlgorithmId;
import sun.security.x509.X509Key;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;

/*
 * @test
 * @summary Test CertificateFactory on SM certificate.
 * @compile ../Utils.java
 * @run testng CertificateFactoryTest
 */
public class CertificateFactoryTest {

    private static final String TEST_BASE = System.getProperty("test.src");
    private static final Path CERT_DIR = Paths.get(TEST_BASE)
            .resolve("..").resolve("certs");

    private static final String SM3withSM2_OID = "1.2.156.10197.1.501";

    @Test
    public void testGenCertCA() throws Exception {
        byte[] certBytes = Utils.certBytes(CERT_DIR.resolve("ca-sm.crt"));

        CertificateFactory cf = CertificateFactory.getInstance("X.509");
        X509Certificate cert = (X509Certificate) cf.generateCertificate(
                new ByteArrayInputStream(certBytes));
        X509Key pubKey = (X509Key) cert.getPublicKey();
        AlgorithmId algId = pubKey.getAlgorithmId();
        assertEquals("EC", algId.getName());
        assertEquals(SM3withSM2_OID, cert.getSigAlgOID());
        assertEquals("SM3withSM2", cert.getSigAlgName());
    }

    @Test
    public void testGenCerts() throws Exception {
        byte[] allCertBytes = Utils.certBytes(
                CERT_DIR.resolve("ca-sm.crt"),
                CERT_DIR.resolve("intca-sm.crt"),
                CERT_DIR.resolve("ee-sm.crt"));

        CertificateFactory cf = CertificateFactory.getInstance("X.509");
        Collection<? extends Certificate> certs = cf.generateCertificates(
                new ByteArrayInputStream(allCertBytes));
        Iterator<? extends Certificate> iterator = certs.iterator();
        X509Certificate cert = (X509Certificate) iterator.next();
        assertEquals("CN=ca-sm", cert.getSubjectDN().getName());
        cert = (X509Certificate) iterator.next();
        assertEquals("CN=intca-sm", cert.getSubjectDN().getName());
        cert = (X509Certificate) iterator.next();
        assertEquals("CN=ee-sm", cert.getSubjectDN().getName());
    }

    @Test
    public void testGenCertPath() throws Exception {
        List<X509Certificate> certs = new ArrayList<>();
        certs.add(Utils.x509CertAsFile(CERT_DIR.resolve("ca-sm.crt")));
        certs.add(Utils.x509CertAsFile(CERT_DIR.resolve("intca-sm.crt")));
        certs.add(Utils.x509CertAsFile(CERT_DIR.resolve("ee-sm.crt")));

        CertificateFactory cf = CertificateFactory.getInstance("X.509");

        CertPath certPath = cf.generateCertPath(certs);
        assertEquals(certs.size(), certPath.getCertificates().size());

        byte[] pkiPathEncodedCertPath = certPath.getEncoded("PkiPath");

        CertPath defaultCertPath = cf.generateCertPath(
                new ByteArrayInputStream(pkiPathEncodedCertPath));
        assertEquals(certs.size(), defaultCertPath.getCertificates().size());

        CertPath pkiPathCertPath = cf.generateCertPath(
                new ByteArrayInputStream(pkiPathEncodedCertPath), "PkiPath");
        assertEquals(certs.size(), pkiPathCertPath.getCertificates().size());

        byte[] pkcs7EncodedCertPath = certPath.getEncoded("PKCS7");
        CertPath pkcs7CertPath = cf.generateCertPath(
                new ByteArrayInputStream(pkcs7EncodedCertPath), "PKCS7");
        assertEquals(certs.size(), pkcs7CertPath.getCertificates().size());
    }

    @Test
    public void testGenCRL() throws Exception {
        testGenCRL("intca-sm.crl", SM3withSM2_OID);
        testGenCRL("ee-sm.crl", SM3withSM2_OID);
    }

    private void testGenCRL(String crlFileName, String sigAlgOid)
            throws Exception {
        CertificateFactory cf = CertificateFactory.getInstance("X.509");
        X509CRL crl = (X509CRL) cf.generateCRL(new ByteArrayInputStream(
                Utils.fileAsBytes(CERT_DIR.resolve(crlFileName))));
        assertEquals(sigAlgOid, crl.getSigAlgOID());
    }

    @Test
    public void testGenCRLs() throws Exception {
        CertificateFactory cf = CertificateFactory.getInstance("X.509");

        @SuppressWarnings("unchecked")
        List<? extends X509CRL> crls = (List<? extends X509CRL>) cf.generateCRLs(
                new ByteArrayInputStream(Utils.certBytes(
                        CERT_DIR.resolve("ee-sm.crl"),
                        CERT_DIR.resolve("intca-sm.crl"))));
        assertEquals(2, crls.size());
        assertEquals(SM3withSM2_OID, crls.get(0).getSigAlgOID());
        assertEquals(SM3withSM2_OID, crls.get(1).getSigAlgOID());
    }
}
