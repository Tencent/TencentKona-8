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

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CRLException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509CRL;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.SM2ParameterSpec;
import java.security.spec.SM2PrivateKeySpec;
import java.security.spec.SM2PublicKeySpec;
import java.util.Arrays;
import java.util.Base64;
import java.util.List;

/**
 * The utilities for SM tests.
 */
public final class Utils {

    private static final String BEGIN = "-----BEGIN";
    private static final String END = "-----END";

    public static byte[] hexToBytes(String hex) {
        int len = hex.length();
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((Character.digit(hex.charAt(i), 16) << 4)
                                 + Character.digit(hex.charAt(i+1), 16));
        }
        return data;
    }

    public static byte[] data(int size) {
        byte[] data = new byte[size];
        Arrays.fill(data, (byte) 'a');
        return data;
    }

    public static byte[] dataKB(int size) {
        return data(size * 1024);
    }

    public static KeyPair sm2KeyPair() throws Exception {
        KeyPairGenerator keyPairGen
                = KeyPairGenerator.getInstance("EC");
        keyPairGen.initialize(SM2ParameterSpec.instance());
        return keyPairGen.generateKeyPair();
    }

    public static ECPrivateKey sm2PrivateKey(String hex) throws Exception {
        KeyFactory keyFactory = KeyFactory.getInstance("SM2");
        return (ECPrivateKey) keyFactory.generatePrivate(
                new SM2PrivateKeySpec(hexToBytes(hex)));
    }

    public static ECPublicKey sm2PublicKey(String hex) throws Exception {
        KeyFactory keyFactory = KeyFactory.getInstance("SM2");
        return (ECPublicKey) keyFactory.generatePublic(
                new SM2PublicKeySpec(hexToBytes(hex)));
    }

    public static String fileAsStr(Path path) throws IOException {
        return String.join("\n", Files.readAllLines(path));
    }

    public static byte[] fileAsBytes(Path path) throws IOException {
        return fileAsStr(path).getBytes(StandardCharsets.UTF_8);
    }

    public static String certStr(Path... paths) throws IOException {
        return pkiStr(true, paths);
    }

    public static String keyStr(Path... paths) throws IOException {
        return pkiStr(false, paths);
    }

    private static String pkiStr(boolean keepSeparator, Path... paths)
            throws IOException {
        StringBuilder certStrs = new StringBuilder();
        for (Path path : paths) {
            certStrs.append(filterPem(Files.readAllLines(path), keepSeparator))
                    .append("\n");
        }
        return certStrs.toString();
    }

    public static byte[] certBytes(Path... paths) throws IOException {
        return pkiBytes(paths, true);
    }

    public static byte[] keyBytes(Path... paths) throws IOException {
        return pkiBytes(paths, false);
    }

    private static byte[] pkiBytes(Path[] paths, boolean keepSeparator)
            throws IOException {
        return pkiStr(keepSeparator, paths).getBytes(StandardCharsets.UTF_8);
    }

    public static X509Certificate x509CertAsStr(String x509CertPEM)
            throws CertificateException {
        CertificateFactory certFactory
                = CertificateFactory.getInstance("X.509");
        return (X509Certificate) certFactory.generateCertificate(
                new ByteArrayInputStream(
                        x509CertPEM.getBytes(StandardCharsets.UTF_8)));
    }

    public static X509Certificate x509CertAsFile(Path path)
            throws CertificateException, IOException {
        CertificateFactory certFactory
                = CertificateFactory.getInstance("X.509");
        return (X509Certificate) certFactory.generateCertificate(
                new ByteArrayInputStream(filterPem(
                        Files.readAllLines(path), true).getBytes(
                                StandardCharsets.UTF_8)));
    }

    public static X509CRL x509CRLAsFile(Path path)
            throws CertificateException, IOException, CRLException {
        CertificateFactory certFactory
                = CertificateFactory.getInstance("X.509");
        return (X509CRL) certFactory.generateCRL(
                new ByteArrayInputStream(filterPem(
                        Files.readAllLines(path), true).getBytes(
                                StandardCharsets.UTF_8)));
    }

    private static String filterPem(List<String> lines, boolean keepSeparator) {
        StringBuilder result = new StringBuilder();

        boolean begin = false;
        for (String line : lines) {
            if (line.startsWith(END)) {
                if (keepSeparator) {
                    result.append(line);
                }
                break;
            }

            if (line.startsWith(BEGIN)) {
                begin = true;
                if (keepSeparator) {
                    result.append(line).append("\n");
                }
                continue;
            }

            if (begin) {
                result.append(line).append("\n");
            }
        }

        return result.toString();
    }

    public static ECPrivateKey ecPrivateKeyAsFile(Path path) throws IOException,
            NoSuchAlgorithmException, InvalidKeySpecException {
        return ecPrivateKeyAsStr(keyStr(path));
    }

    private static ECPrivateKey ecPrivateKeyAsStr(String pkcs8KeyPEM)
            throws NoSuchAlgorithmException, InvalidKeySpecException {
        PKCS8EncodedKeySpec privateKeySpec = new PKCS8EncodedKeySpec(
                Base64.getMimeDecoder().decode(pkcs8KeyPEM));
        KeyFactory keyFactory = KeyFactory.getInstance("EC");
        return (ECPrivateKey) keyFactory.generatePrivate(privateKeySpec);
    }

    private Utils() {}
}
