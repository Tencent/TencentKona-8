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
 * @summary Test keytool on SM algorithms.
 * @compile ../Utils.java
 *          ../crypto/SM2/ECOperator.java
 * @run testng/othervm -Duser.language=en
 *                     -Duser.country=US
 *                     -Djava.security.egd=file:/dev/./urandom
 *                     KeyToolTest
 * @run testng/othervm -Duser.language=en
 *                     -Duser.country=US
 *                     -Djava.security.egd=file:/dev/./urandom
 *                     -Djdk.tools.useCurveSM2=false
 *                     KeyToolTest
 * @run testng/othervm -Duser.language=en
 *                     -Duser.country=US
 *                     -Djava.security.egd=file:/dev/./urandom
 *                     -Djdk.tools.useCurveSM2=true
 *                     KeyToolTest
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

import sun.security.tools.keytool.Main;

import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;

public class KeyToolTest {

    private static final String USE_CURVE_SM2 = System.getProperty(
            "jdk.tools.useCurveSM2", "None");
    private static final Path BASE_DIR = Paths.get("UseCurveSM2-" + USE_CURVE_SM2);

    private static final String PASSWORD = "testpassword";

    private static final String TOOLS_USE_CURVE_SM2
            = System.getProperty("jdk.tools.useCurveSM2");
    private static final String SIG_ALGO = sigAlg();
    private static final String EXPECTED_SIGALG = expectedSigAlg();
    private static final String CURVE = curve();
    private static final BigInteger EXPECTED_ORDER = expectedOrder();

    private static String sigAlg() {
        if ("false".equalsIgnoreCase(TOOLS_USE_CURVE_SM2)) {
            return "SHA256withECDSA";
        } else if ("true".equalsIgnoreCase(TOOLS_USE_CURVE_SM2)) {
            return "SM3withSM2";
        }

        return null;
    }

    private static String expectedSigAlg() {
        return "true".equalsIgnoreCase(TOOLS_USE_CURVE_SM2)
                ? "SM3withSM2" : "SHA256withECDSA";
    }

    private static String curve() {
        return "true".equalsIgnoreCase(TOOLS_USE_CURVE_SM2)
                ? "curveSM2" : "secp256r1";
    }

    private static BigInteger expectedOrder() {
        return "true".equalsIgnoreCase(TOOLS_USE_CURVE_SM2)
                ? SM2ParameterSpec.ORDER : ECOperator.SECP256R1.getOrder();
    }

    @BeforeClass
    public void prepare() throws Throwable {
        Files.createDirectories(BASE_DIR);
    }

    @Test
    public void testGenKeyPairOnPKCS12() throws Throwable {
        testGenKeyPair("PKCS12");
    }

    @Test
    public void testGenKeyPairOnJKS() throws Throwable {
        testGenKeyPair("JKS");
    }

    private void testGenKeyPair(String storeType) throws Throwable {
        Path keystore = path("testGenKeyPair-" + storeType + ".ks");
        genKeyPair(keystore, storeType, "ec");
        KeyStore keyStore = KeyStore.getInstance(storeType);
        keyStore.load(Files.newInputStream(keystore), PASSWORD.toCharArray());
        ECPrivateKey privateKey
                = (ECPrivateKey) keyStore.getKey("ec", PASSWORD.toCharArray());
        System.out.println("EC params: " + privateKey.getParams());
        assertEquals(EXPECTED_ORDER, privateKey.getParams().getOrder());
    }

    @Test
    public void testGenSelfSignedCertOnPKCS12() throws Throwable {
        testGenSelfSignedCert("PKCS12");
    }

    @Test
    public void testGenSelfSignedCertOnJKS() throws Throwable {
        testGenSelfSignedCert("JKS");
    }

    private void testGenSelfSignedCert(String storeType) throws Throwable {
        Path keystore = path("testGenSelfSignedCert-" + storeType + ".ks");
        String alias = CURVE + "-" + SIG_ALGO;
        genKeyPair(keystore, storeType, alias);
        outputCert(keystore, storeType, alias, path(alias + ".crt"));
    }

    @Test
    public void testGenCSROnPKCS12() throws Throwable {
        testGenCSR("PKCS12");
    }

    @Test
    public void testGenCSROnJKS() throws Throwable {
        testGenCSR("JKS");
    }

    private void testGenCSR(String storeType) throws Throwable {
        Path keystore = path("testGenCSR-" + storeType + ".ks");
        String alias = "ec-testGenCSR-" + storeType;
        genKeyPair(keystore, storeType, alias);
        genCSR(keystore, storeType, alias, path("ec-sm2-root.csr"));
    }

    @Test
    public void testGenCertChainOnPKCS12() throws Throwable {
        genCertChain("PKCS12");
    }

    @Test
    public void testGenCertChainOnJKS() throws Throwable {
        genCertChain("JKS");
    }

    private void genCertChain(String storeType)
            throws Throwable {
        Path rootKeystore = path("genCertChain-" + storeType + "-root.ks");
        Path caKeystore = path("genCertChain-" + storeType + "-ca.ks");
        Path eeKeystore = path("genCertChain-" + storeType + "-ee.ks");

        String suffix = CURVE + "-" + SIG_ALGO;
        String rootAlias = "root-" + suffix;
        String caAlias = "ca-" + suffix;
        String eeAlias = "ee-" + suffix;

        Path caCSRPath = path(caAlias + ".csr");
        Path eeCSRPath = path(eeAlias + ".csr");

        genKeyPair(rootKeystore, storeType, rootAlias);
        genKeyPair(caKeystore, storeType, caAlias);
        genKeyPair(eeKeystore, storeType, eeAlias);

        outputCert(rootKeystore, storeType, rootAlias, path(rootAlias + ".crt"));
        genCSR(caKeystore, storeType, caAlias, caCSRPath);

        Path caCertPath = path(caAlias + ".crt");
        genCert(rootKeystore, storeType, rootAlias, caCSRPath, caCertPath);
        checkCert(caCertPath);

        genCSR(eeKeystore, storeType, eeAlias, eeCSRPath);

        Path eeCertPath = path(eeAlias + ".crt");
        genCert(caKeystore, storeType, caAlias, eeCSRPath, eeCertPath);
        checkCert(eeCertPath);
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

        System.out.println("outputCert: " + keytoolCmd(args));
        Main.main(args.toArray(new String[0]));
    }

    private static void genKeyPair(Path keystorePath, String storeType,
            String alias) throws Throwable {
        List<String> args = new ArrayList<>();

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

        if (SIG_ALGO != null) {
            args.add("-sigalg");
            args.add(SIG_ALGO);
        }

        args.add("-dname");
        args.add("CN=" + alias);

        System.out.println("genKeyPair: " + keytoolCmd(args));
        Main.main(args.toArray(new String[0]));
    }

    private static void genCSR(Path keystorePath, String storeType,
            String alias, Path csrPath) throws Throwable {
        List<String> args = new ArrayList<>();

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

        if (SIG_ALGO != null) {
            args.add("-sigalg");
            args.add(SIG_ALGO);
        }

        if (csrPath != null) {
            args.add("-file");
            args.add(csrPath.toString());
        }

        System.out.println("genCSR: " + keytoolCmd(args));
        Main.main(args.toArray(new String[0]));
    }

    private static void genCert(Path keystorePath, String storeType,
            String issuerAlias, Path csrPath, Path certPath)
            throws Throwable {
        List<String> args = new ArrayList<>();

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

        if (SIG_ALGO != null) {
            args.add("-sigalg");
            args.add(SIG_ALGO);
        }

        args.add("-infile");
        args.add(csrPath.toString());

        if (certPath != null) {
            args.add("-outfile");
            args.add(certPath.toString());
        }

        System.out.println("genCert: " + keytoolCmd(args));
        Main.main(args.toArray(new String[0]));
    }

    private static void checkCert(Path certPath) throws Exception {
        X509Certificate cert = Utils.x509CertAsFile(certPath);
        PublicKey publicKey = cert.getPublicKey();
        assertEquals("EC", publicKey.getAlgorithm());

        ECPublicKey ecPublicKey = (ECPublicKey) publicKey;
        assertEquals(EXPECTED_ORDER, ecPublicKey.getParams().getOrder());

        assertEquals(EXPECTED_SIGALG, cert.getSigAlgName());
    }

    private static Path path(String file) {
        return BASE_DIR.resolve(Paths.get(file));
    }

    private static String keytoolCmd(List<String> args) {
        String cmd = String.join(" ", "keytool",
                "-J-Duser.language=en",
                "-J-Duser.country=US ",
                "-J-Djava.security.egd=file:/dev/./urandom");
        String useCurveSM2 = TOOLS_USE_CURVE_SM2 == null
                ? "" : "-J-Djdk.tools.useCurveSM2=" + TOOLS_USE_CURVE_SM2;
        return String.join(" ", cmd, useCurveSM2, String.join(" ", args));
    }
}
