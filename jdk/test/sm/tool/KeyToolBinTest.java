/*
 * Copyright (C) 2023, 2024, THL A29 Limited, a Tencent company. All rights reserved.
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
 * @summary Test keytool binary on SM algorithms.
 *          jarsigner doesn't support these algorithms yet.
 * @library /lib/testlibrary
 * @compile ../Utils.java
 *          ../crypto/SM2/ECOperator.java
 * @run testng/timeout=600 KeyToolBinTest
 */

import java.math.BigInteger;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.KeyStore;
import java.security.PublicKey;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.SM2ParameterSpec;
import java.util.ArrayList;
import java.util.List;

import jdk.testlibrary.SecurityTools;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;

public class KeyToolBinTest {

    private static final String PASSWORD = "testpassword";

    @Test
    public void testGenKeyPairOnPKCS12() throws Throwable {
        testGenKeyPair(null, "PKCS12", null);
    }

    @Test
    public void testGenKeyPairWithECOnPKCS12() throws Throwable {
        testGenKeyPair("false", "PKCS12", "SHA256withECDSA");
    }

    @Test
    public void testGenKeyPairWithSMOnPKCS12() throws Throwable {
        testGenKeyPair("true", "PKCS12", "SM3withSM2");
    }

    @Test
    public void testGenKeyPairOnJKS() throws Throwable {
        testGenKeyPair(null, "JKS", null);
    }

    @Test
    public void testGenKeyPairWithECOnJKS() throws Throwable {
        testGenKeyPair("false", "JKS", "SHA256withECDSA");
    }

    @Test
    public void testGenKeyPairWithSMOnJKS() throws Throwable {
        testGenKeyPair("true", "JKS", "SM3withSM2");
    }

    private void testGenKeyPair(String toolsUseCurveSM2, String storeType,
            String sigAlg) throws Throwable {
        Path keystore = Paths.get(String.join("-",
                "testGenKeyPair", storeType, toolsUseCurveSM2 + ".ks"));
        genKeyPair(keystore, toolsUseCurveSM2, storeType, "ec", sigAlg);
        KeyStore keyStore = KeyStore.getInstance(storeType);
        keyStore.load(Files.newInputStream(keystore), PASSWORD.toCharArray());
        ECPrivateKey privateKey
                = (ECPrivateKey) keyStore.getKey("ec", PASSWORD.toCharArray());
        System.out.println("EC params: " + privateKey.getParams());
        assertEquals(expectedOrder(toolsUseCurveSM2),
                privateKey.getParams().getOrder());
    }

    @Test
    public void testGenSelfSignedCertOnPKCS12() throws Throwable {
        testGenSelfSignedCert(null, "PKCS12", null);
    }

    @Test
    public void testGenSelfSignedCertWithECOnPKCS12() throws Throwable {
        testGenSelfSignedCert("false", "PKCS12", "SHA256withECDSA");
    }

    @Test
    public void testGenSelfSignedCertWithSMOnPKCS12() throws Throwable {
        testGenSelfSignedCert("true", "PKCS12", "SM3withSM2");
    }

    @Test
    public void testGenSelfSignedCertOnJKS() throws Throwable {
        testGenSelfSignedCert(null, "JKS", null);
    }

    @Test
    public void testGenSelfSignedCertWithECOnJKS() throws Throwable {
        testGenSelfSignedCert("false", "JKS", "SHA256withECDSA");
    }

    @Test
    public void testGenSelfSignedCertWithSMOnJKS() throws Throwable {
        testGenSelfSignedCert("true", "JKS", "SM3withSM2");
    }

    private void testGenSelfSignedCert(String toolsUseCurveSM2,
            String storeType, String sigAlg) throws Throwable {
        Path keystore = Paths.get(String.join("-",
                "testGenSelfSignedCert", storeType, toolsUseCurveSM2 + ".ks"));
        String alias = curve(toolsUseCurveSM2) + "-" + sigAlg;
        genKeyPair(keystore, toolsUseCurveSM2, storeType, alias, sigAlg);
        outputCert(keystore, storeType, alias, Paths.get(alias + ".crt"));
    }

    @Test
    public void testGenCertChainOnPKCS12() throws Throwable {
        genCertChain(null, "PKCS12", null);
    }

    @Test
    public void testGenCertChainWithECOnPKCS12() throws Throwable {
        genCertChain("false", "PKCS12", "SHA256withECDSA");
    }

    @Test
    public void testGenCertChainWithSMOnPKCS12() throws Throwable {
        genCertChain("true", "PKCS12", "SM3withSM2");
    }

    @Test
    public void testGenCertChainOnJKS() throws Throwable {
        genCertChain(null, "JKS", null);
    }

    @Test
    public void testGenCertChainWithECOnJKS() throws Throwable {
        genCertChain("false", "JKS", "SHA256withECDSA");
    }

    @Test
    public void testGenCertChainWithSMOnJKS() throws Throwable {
        genCertChain("true", "JKS", "SM3withSM2");
    }

    private void genCertChain(String toolsUseCurveSM2, String storeType,
            String sigAlg) throws Throwable {
        Path rootKeystore = Paths.get(String.join("-",
                "genCertChain", storeType, toolsUseCurveSM2, "root.ks"));
        Path caKeystore = Paths.get(String.join("-",
                "genCertChain", storeType, toolsUseCurveSM2, "ca.ks"));
        Path eeKeystore = Paths.get(String.join("-",
                "genCertChain", storeType, toolsUseCurveSM2, "ee.ks"));

        String suffix = curve(toolsUseCurveSM2) + "-" + sigAlg;
        String rootAlias = "root-" + suffix;
        String caAlias = "ca-" + suffix;
        String eeAlias = "ee-" + suffix;

        Path caCSRPath = Paths.get(caAlias + ".csr");
        Path eeCSRPath = Paths.get(eeAlias + ".csr");

        genKeyPair(rootKeystore, toolsUseCurveSM2, storeType, rootAlias, sigAlg);
        genKeyPair(caKeystore, toolsUseCurveSM2, storeType, caAlias, sigAlg);
        genKeyPair(eeKeystore, toolsUseCurveSM2, storeType, eeAlias, sigAlg);

        outputCert(rootKeystore, storeType, rootAlias, Paths.get(rootAlias + ".crt"));
        genCSR(caKeystore, toolsUseCurveSM2, storeType, caAlias, sigAlg, caCSRPath);

        Path caCertPath = Paths.get(caAlias + ".crt");
        genCert(rootKeystore, toolsUseCurveSM2, storeType, rootAlias, sigAlg, caCSRPath, caCertPath);
        checkCert(caCertPath, expectedOrder(toolsUseCurveSM2), expectedSigAlg(toolsUseCurveSM2));

        genCSR(eeKeystore, toolsUseCurveSM2, storeType, eeAlias, sigAlg, eeCSRPath);

        Path eeCertPath = Paths.get(eeAlias + ".crt");
        genCert(caKeystore, toolsUseCurveSM2, storeType, caAlias, sigAlg, eeCSRPath, eeCertPath);
        checkCert(eeCertPath, expectedOrder(toolsUseCurveSM2), expectedSigAlg(toolsUseCurveSM2));
    }

    private static String curve(String toolsUseCurveSM2) {
        return "true".equalsIgnoreCase(toolsUseCurveSM2)
                ? "curveSM2" : "secp256r1";
    }

    private static BigInteger expectedOrder(String toolsUseCurveSM2) {
        return "true".equalsIgnoreCase(toolsUseCurveSM2)
                ? SM2ParameterSpec.ORDER : ECOperator.SECP256R1.getOrder();
    }

    private static String expectedSigAlg(String toolsUseCurveSM2) {
        return "true".equalsIgnoreCase(toolsUseCurveSM2)
                ? "SM3withSM2" : "SHA256withECDSA";
    }

    private static void outputCert(Path keystore, String storeType,
            String alias, Path certPath) throws Throwable {
        List<String> args = new ArrayList<>();

        args.add("-v");

        args.add("-exportcert");
        args.add("-rfc");

        args.add("-keystore");
        args.add(keystore.toString());

        args.add("-storetype");
        args.add(storeType);

        args.add("-storepass");
        args.add(PASSWORD);

        args.add("-alias");
        args.add(alias);

        if (certPath != null) {
            args.add("-file");
            args.add(certPath.toString());
        }

        SecurityTools.keytool(args);
    }

    private static void genKeyPair(Path keystorePath, String toolsUseCurveSM2,
            String storeType, String alias, String sigAlg) throws Throwable {
        List<String> args = new ArrayList<>();

        if (toolsUseCurveSM2 != null) {
            args.add("-J-Djdk.tools.useCurveSM2=" + toolsUseCurveSM2);
        }

        args.add("-v");

        args.add("-genkeypair");

        args.add("-keystore");
        args.add(keystorePath.toString());

        args.add("-storetype");
        args.add(storeType);

        args.add("-storepass");
        args.add(PASSWORD);

        args.add("-alias");
        args.add(alias);

        args.add("-keyalg");
        args.add("EC");

        args.add("-keypass");
        args.add(PASSWORD);

        if (sigAlg != null) {
            args.add("-sigalg");
            args.add(sigAlg);
        }

        args.add("-dname");
        args.add("CN=" + alias);

        SecurityTools.keytool(args);
    }

    private static void genCSR(Path keystorePath, String toolsUseCurveSM2,
            String storeType, String alias, String sigAlg, Path csrPath)
            throws Throwable {
        List<String> args = new ArrayList<>();

        if (toolsUseCurveSM2 != null) {
            args.add("-J-Djdk.tools.useCurveSM2=" + toolsUseCurveSM2);
        }

        args.add("-v");

        args.add("-certreq");

        args.add("-keystore");
        args.add(keystorePath.toString());

        args.add("-storetype");
        args.add(storeType);

        args.add("-storepass");
        args.add(PASSWORD);

        args.add("-alias");
        args.add(alias);

        args.add("-keypass");
        args.add(PASSWORD);

        if (sigAlg != null) {
            args.add("-sigalg");
            args.add(sigAlg);
        }

        if (csrPath != null) {
            args.add("-file");
            args.add(csrPath.toString());
        }

        SecurityTools.keytool(args);
    }

    private static void genCert(Path keystorePath, String toolsUseCurveSM2,
            String storeType, String issuerAlias, String sigAlg,
            Path csrPath, Path certPath) throws Throwable {
        List<String> args = new ArrayList<>();

        if (toolsUseCurveSM2 != null) {
            args.add("-J-Djdk.tools.useCurveSM2=" + toolsUseCurveSM2);
        }

        args.add("-v");

        args.add("-gencert");
        args.add("-rfc");

        args.add("-keystore");
        args.add(keystorePath.toString());

        args.add("-storetype");
        args.add(storeType);

        args.add("-storepass");
        args.add(PASSWORD);

        args.add("-alias");
        args.add(issuerAlias);

        args.add("-keypass");
        args.add(PASSWORD);

        if (sigAlg != null) {
            args.add("-sigalg");
            args.add(sigAlg);
        }

        args.add("-infile");
        args.add(csrPath.toString());

        if (certPath != null) {
            args.add("-outfile");
            args.add(certPath.toString());
        }

        SecurityTools.keytool(args);
    }

    private static void checkCert(Path certPath, BigInteger expectedOrder,
            String expectedSigAlg) throws Exception {
        X509Certificate cert = Utils.x509CertAsFile(certPath);
        PublicKey publicKey = cert.getPublicKey();
        assertEquals("EC", publicKey.getAlgorithm());

        ECPublicKey ecPublicKey = (ECPublicKey) publicKey;
        assertEquals(expectedOrder, ecPublicKey.getParams().getOrder());

        assertEquals(expectedSigAlg, cert.getSigAlgName());
    }
}
