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
 * @summary Test CertPathBuilder on SM certificate.
 * @compile ../Utils.java
 * @run testng CertPathBuilderTest
 */

import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.cert.CertPath;
import java.security.cert.CertPathBuilder;
import java.security.cert.CertPathBuilderResult;
import java.security.cert.CertStore;
import java.security.cert.CollectionCertStoreParameters;
import java.security.cert.PKIXBuilderParameters;
import java.security.cert.TrustAnchor;
import java.security.cert.X509CertSelector;
import java.security.cert.X509Certificate;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;

public class CertPathBuilderTest {

    private static final String TEST_BASE = System.getProperty("test.src");
    private static final Path CERT_DIR = Paths.get(TEST_BASE)
            .resolve("..").resolve("certs");

    @Test
    public void testBuild() throws Exception {
        X509CertSelector selector = new X509CertSelector();
        selector.setCertificate(Utils.x509CertAsFile(
                CERT_DIR.resolve("ee-sm.crt")));

        Set<TrustAnchor> anchors = new HashSet<>();
        anchors.add(new TrustAnchor(Utils.x509CertAsFile(
                CERT_DIR.resolve("ca-sm.crt")), null));
        PKIXBuilderParameters params = new PKIXBuilderParameters(
                anchors, selector);
        params.setRevocationEnabled(false);

        Collection<X509Certificate> certs = new HashSet<>();
        certs.add(Utils.x509CertAsFile(CERT_DIR.resolve("ee-sm.crt")));
        certs.add(Utils.x509CertAsFile(CERT_DIR.resolve("intca-sm.crt")));
        CertStore certStore = CertStore.getInstance("Collection",
                new CollectionCertStoreParameters(certs));
        params.addCertStore(certStore);

        CertPathBuilder cpb = CertPathBuilder.getInstance("PKIX");
        CertPathBuilderResult result = cpb.build(params);
        CertPath certPath = result.getCertPath();

        @SuppressWarnings("unchecked")
        List<X509Certificate> certPathCerts
                = (List<X509Certificate>) certPath.getCertificates();
        assertEquals(certPathCerts.get(0),
                Utils.x509CertAsFile(CERT_DIR.resolve("ee-sm.crt")));
        assertEquals(certPathCerts.get(1),
                Utils.x509CertAsFile(CERT_DIR.resolve("intca-sm.crt")));
    }
}
