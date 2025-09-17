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
 * @summary Test CertStore on SM certificate.
 * @compile ../Utils.java
 * @run testng CertStoreTest
 */

import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.cert.CertStore;
import java.security.cert.Certificate;
import java.security.cert.CollectionCertStoreParameters;
import java.security.cert.X509CertSelector;
import java.security.cert.X509Certificate;
import java.util.Collection;
import java.util.HashSet;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;

public class CertStoreTest {

    private static final String TEST_BASE = System.getProperty("test.src");
    private static final Path CERT_DIR = Paths.get(TEST_BASE)
            .resolve("..").resolve("certs");

    @Test
    public void testGetCertificates() throws Exception {
        Collection<X509Certificate> certs = new HashSet<>();
        X509Certificate target = Utils.x509CertAsFile(
                CERT_DIR.resolve("ee-sm.crt"));
        certs.add(Utils.x509CertAsFile(
                CERT_DIR.resolve("ee-sm.crt")));
        certs.add(Utils.x509CertAsFile(
                CERT_DIR.resolve("intca-sm.crt")));
        certs.add(Utils.x509CertAsFile(
                CERT_DIR.resolve("ca-sm.crt")));

        CertStore certStore = CertStore.getInstance("Collection",
                new CollectionCertStoreParameters(certs));

        X509CertSelector certSelector = new X509CertSelector();
        certSelector.setCertificate(target);

        Collection<? extends Certificate> foundCerts
                = certStore.getCertificates(certSelector);
        assertEquals(
                target.getSerialNumber(),
                ((X509Certificate) foundCerts.iterator().next())
                        .getSerialNumber());
    }
}
