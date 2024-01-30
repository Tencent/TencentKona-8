/*
 * Copyright (C) 2022, 2024, THL A29 Limited, a Tencent company. All rights reserved.
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
 * @summary Test CertPathValidator on SM certificate.
 * @compile ../Utils.java
 * @run testng/othervm CertPathValidatorTest
 */

import java.math.BigInteger;
import java.net.InetAddress;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.KeyStore;
import java.security.Security;
import java.security.cert.CertPath;
import java.security.cert.CertPathValidator;
import java.security.cert.CertPathValidatorException;
import java.security.cert.CertStore;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.CollectionCertStoreParameters;
import java.security.cert.PKIXParameters;
import java.security.cert.TrustAnchor;
import java.security.cert.X509CRL;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECPrivateKey;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

import org.testng.annotations.BeforeTest;
import org.testng.annotations.Test;

public class CertPathValidatorTest {

    private static final String TEST_BASE = System.getProperty("test.src");
    private static final Path CERT_DIR = Paths.get(TEST_BASE)
            .resolve("..").resolve("certs");

    @BeforeTest
    public void beforeEach() {
        System.clearProperty("com.tencent.kona.pkix.enableCRLDP");
        System.clearProperty("ocsp.enable");
    }

    @Test
    public void testValidateSingleCert() throws Exception {
        validate(
                new String[] { "ca-sm.crt" },
                new String[] { "ca-sm.crt" });

        validate(
                new String[] { "intca-sm.crt" },
                new String[] { "ca-sm.crt" });

        validate(
                new String[] { "ee-sm.crt" },
                new String[] { "intca-sm.crt" });
    }

    @Test
    public void testValidateCertChain() throws Exception {
        validate(
                new String[] {"ee-sm.crt", "intca-sm.crt" },
                new String[] { "ca-sm.crt" });
    }

    @Test
    public void testValidateCertChainFailed() throws Exception {
        validate(
                new String[] { "ee-sm.crt", "intca-sm.crt" },
                new String[] { "ca-sm-alt.crt" }, // Not this CA
                false,
                CertPathValidatorException.class);
    }

    @Test
    public void testValidateCertChainWithCrl() throws Exception {
        validateWithCrl(
                new String[] {
                        "ee-sm.crt",
                        "intca-sm.crt" }, // Revoked by intca-sm.crl
                new String[] { "ca-sm.crt" },
                new String[] { "intca-sm.crl" },
                true,
                CertPathValidatorException.class);
    }

    @Test
    public void testValidateCertChainWithCrldp() throws Exception {
        System.setProperty("com.tencent.kona.pkix.enableCRLDP", "true");

        validateWithCrl(
                // Revoked by ee-sm-crldp.crl
                new String[] { "ee-sm-crldp.crt" },
                new String[] { "intca-sm.crt" },
                new String[] { "ca-sm-empty.crl" },
                true,
                CertPathValidatorException.class);
    }

    @Test
    public void testValidateCertChainWithOcsp() throws Exception {
        Security.setProperty("ocsp.enable", "true");

        testValidateCertChainWithOcsp(
                "intca-sm.crt",
                "intca-sm.key",
                "ee-sm-aia.crt",
                SimpleOCSPServer.CertStatus.CERT_STATUS_GOOD);
        testValidateCertChainWithOcsp(
                "intca-sm.crt",
                "intca-sm.key",
                "ee-sm-aia.crt",
                SimpleOCSPServer.CertStatus.CERT_STATUS_REVOKED,
                CertPathValidatorException.class);
    }

    private void testValidateCertChainWithOcsp(String issuerCertName,
            String issuerKeyName, String certName,
            SimpleOCSPServer.CertStatus certStatus,
            Class<? extends Exception> expectedEx) throws Exception {
        SimpleOCSPServer ocspServer = createOCSPServer(
                issuerCertName, issuerKeyName);
        ocspServer.start();
        if (!ocspServer.awaitServerReady(5, TimeUnit.SECONDS)) {
            throw new RuntimeException("OCSP server is not started");
        }

        X509Certificate eeCert = Utils.x509CertAsFile(
                CERT_DIR.resolve(certName));
        Map<BigInteger, SimpleOCSPServer.CertStatusInfo> certStatusInfos
                = Collections.singletonMap(
                        eeCert.getSerialNumber(),
                        new SimpleOCSPServer.CertStatusInfo(certStatus));
        ocspServer.updateStatusDb(certStatusInfos);

        // Not use the static OCSP URL defined by AIA extension in the
        // certificate, instead, just use the real OCSP responder URL.
        Security.setProperty("ocsp.responderURL",
                "http://127.0.0.1:" + ocspServer.getPort());
        validate(
                new String[] { certName },
                new String[] { issuerCertName },
                true,
                expectedEx);
    }

    private void testValidateCertChainWithOcsp(String issuerCertName,
            String issuerKeyName, String certName,
            SimpleOCSPServer.CertStatus certStatus) throws Exception {
        testValidateCertChainWithOcsp(
                issuerCertName, issuerKeyName, certName, certStatus, null);
    }

    private void validate(String[] certChain, String[] cas)
            throws Exception {
        validate(certChain, cas, false, null);
    }

    private void validate(String[] certChain, String[] cas,
            boolean checkCertStatus, Class<? extends Exception> expectedEx)
            throws Exception {
        validateWithCrl(certChain, cas, null, checkCertStatus, expectedEx);
    }

    private void validateWithCrl(String[] certChain, String[] cas,
            String[] crls, boolean checkCertStatus,
            Class<? extends Exception> expectedEx) throws Exception {
        CertPathValidator cpv = CertPathValidator.getInstance("PKIX");
        try {
            cpv.validate(certPath(certChain), certPathParams(
                    cas, crls, checkCertStatus));
            if (expectedEx != null) {
                throw new RuntimeException(
                        "Expected " + expectedEx + " was not thrown");
            }
        } catch (Exception ex) {
            if (expectedEx != null && expectedEx.isInstance(ex)) {
                System.out.printf("Expected %s: %s%n",
                        expectedEx.getSimpleName(), ex.getMessage());
            } else {
                throw ex;
            }
        }
    }

    private CertPath certPath(String[] certChain) throws Exception {
        List<X509Certificate> certs = new ArrayList<>();

        for (String s : certChain) {
            X509Certificate x509Cert = Utils.x509CertAsFile(
                    CERT_DIR.resolve(s));
            certs.add(x509Cert);
        }

        CertificateFactory cf = CertificateFactory.getInstance("X.509");;
        return cf.generateCertPath(certs);
    }

    private PKIXParameters certPathParams(String[] cas, boolean checkCertStatus)
            throws Exception {
        Set<TrustAnchor> anchors = new LinkedHashSet<>();
        for (String ca : cas) {
            anchors.add(new TrustAnchor(
                    Utils.x509CertAsFile(CERT_DIR.resolve(ca)), null));
        }

        PKIXParameters params = new PKIXParameters(anchors);
        params.setRevocationEnabled(checkCertStatus);
        return params;
    }

    private PKIXParameters certPathParams(String[] cas, String[] crls,
            boolean checkCertStatus) throws Exception {
        PKIXParameters params = certPathParams(cas, checkCertStatus);

        if (crls != null) {
            Set<X509CRL> x509Crls = new LinkedHashSet<>();
            for (String crl : crls) {
                x509Crls.add(Utils.x509CRLAsFile(CERT_DIR.resolve(crl)));
            }
            CertStore certStore = CertStore.getInstance("Collection",
                    new CollectionCertStoreParameters(x509Crls));
            params.addCertStore(certStore);
        }

        return params;
    }

    private SimpleOCSPServer createOCSPServer(
            String issuerCertName, String issuerKeyName) throws Exception {
        KeyStore keyStore = KeyStore.getInstance("PKCS12");
        keyStore.load(null, null);

        String password = "password";
        String alias = "issuer";
        X509Certificate signerCert = Utils.x509CertAsFile(
                CERT_DIR.resolve(issuerCertName));
        ECPrivateKey signerKey = Utils.ecPrivateKeyAsFile(
                CERT_DIR.resolve(issuerKeyName));
        keyStore.setKeyEntry(alias, signerKey, password.toCharArray(),
                new Certificate[] { signerCert });

        SimpleOCSPServer ocspServer = new SimpleOCSPServer(
                InetAddress.getLoopbackAddress(), 0,
                keyStore, password, alias, null);
        ocspServer.setNextUpdateInterval(3600);
        return ocspServer;
    }
}
