/*
 * Copyright (C) 2023, 2026, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
 * this particular file as subject to the "Classpath" exception as provided
 * in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package sun.security.util;

import java.math.BigInteger;
import java.security.AccessController;
import java.security.InvalidKeyException;
import java.security.PrivilegedAction;
import java.security.cert.X509Certificate;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECField;
import java.security.spec.ECFieldFp;
import java.security.spec.ECParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.SM2ParameterSpec;

/**
 * The utilities for ShangMi features.
 */
public final class SMUtil {

    public static int rotateLeft(int n, int bits) {
        return (n << bits) | (n >>> (32 - bits));
    }

    // BigInteger to little-endian byte array
    public static byte[] toByteArrayLE(BigInteger value) {
        byte[] byteArr = value.toByteArray();
        ArrayUtil.reverse(byteArr);
        return byteArr;
    }

    // Convert 4-bytes in big-endian to integer
    public static int bytes4ToInt(byte[] bytes4, int offset) {
        return ( bytes4[  offset]         << 24)
             | ((bytes4[++offset] & 0xFF) << 16)
             | ((bytes4[++offset] & 0xFF) <<  8)
             |  (bytes4[++offset] & 0xFF);
    }

    // Convert integer to 4-bytes in big-endian
    public static void intToBytes4(int n, byte[] dest, int offset) {
        dest[  offset] = (byte)(n >>> 24);
        dest[++offset] = (byte)(n >>> 16);
        dest[++offset] = (byte)(n >>>  8);
        dest[++offset] = (byte) n;
    }

    public static byte[] intToBytes4(int n) {
        byte[] bytes4 = new byte[4];
        intToBytes4(n, bytes4, 0);
        return bytes4;
    }

    public static void intsToBytes(int[] ints, int offset,
            byte[] dest, int destOffset, int length) {
        for(int i = offset; i < offset + length; i++) {
            intToBytes4(ints[i], dest, destOffset + i * 4);
        }
    }

    public static byte[] bigIntToBytes32(BigInteger value) {
        byte[] bytes32 = new byte[32];
        bigIntToBytes32(value, bytes32);
        return bytes32;
    }

    private static void bigIntToBytes32(BigInteger value, byte[] dest) {
        byte[] byteArr = value.toByteArray();

        if (byteArr.length == 32) {
            System.arraycopy(byteArr, 0, dest, 0, 32);
        } else {
            int start = (byteArr[0] == 0 && byteArr.length != 1) ? 1 : 0;
            int count = byteArr.length - start;

            if (count > 32) {
                throw new IllegalArgumentException(
                        "The value must not be longer than 32-bytes");
            }

            System.arraycopy(byteArr, start, dest, 32 - count, count);
        }
    }

    public static byte[] hexToBytes(String hex) {
        int len = hex.length();
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((Character.digit(hex.charAt(i), 16) << 4)
                                 + Character.digit(hex.charAt(i+1), 16));
        }
        return data;
    }

    // little-endian
    public static byte[] hexToBytesLE(String hex) {
        byte[] byteArr = hexToBytes(hex);
        ArrayUtil.reverse(byteArr);
        return byteArr;
    }

    public static byte[] encodePubPoint(ECPoint pubPoint) {
        byte[] x = bigIntToBytes32(pubPoint.getAffineX());
        byte[] y = bigIntToBytes32(pubPoint.getAffineY());

        byte[] encoded = new byte[65];
        encoded[0] = 0x04;
        System.arraycopy(x, 0, encoded, 1, 32);
        System.arraycopy(y, 0, encoded, 33, 32);
        return encoded;
    }

    public static byte[] encodePrivKey(BigInteger privKeyValue) {
        return bigIntToBytes32(privKeyValue);
    }

    // Partial Public key validation as described in NIST SP 800-186 Appendix D.1.1.1.
    // The extra step in the full validation (described in Appendix D.1.1.2) is implemented
    // as sun.security.ec.ECOperations#checkOrder inside the jdk.crypto.ec module.
    public static void validatePublicKey(ECPoint point, ECParameterSpec spec)
            throws InvalidKeyException {
        BigInteger p;
        ECField field = spec.getCurve().getField();
        if (field instanceof ECFieldFp) {
            ECFieldFp f = (ECFieldFp) field;
            p = f.getP();
        } else {
            throw new InvalidKeyException("Only curves over prime fields are supported");
        }

        // 1. If Q is the point at infinity, output REJECT
        if (point.equals(ECPoint.POINT_INFINITY)) {
            throw new InvalidKeyException("Public point is at infinity");
        }
        // 2. Verify that x and y are integers in the interval [0, p-1]. Output REJECT if verification fails.
        BigInteger x = point.getAffineX();
        if (x.signum() < 0 || x.compareTo(p) >= 0) {
            throw new InvalidKeyException("Public point x is not in the interval [0, p-1]");
        }
        BigInteger y = point.getAffineY();
        if (y.signum() < 0 || y.compareTo(p) >= 0) {
            throw new InvalidKeyException("Public point y is not in the interval [0, p-1]");
        }
        // 3. Verify that (x, y) is a point on the W_a,b by checking that (x, y) satisfies the defining
        // equation y^2 = x^3 + a x + b where computations are carried out in GF(p). Output REJECT
        // if verification fails.
        BigInteger left = y.modPow(BigInteger.valueOf(2), p);
        BigInteger right = x.pow(3).add(spec.getCurve().getA().multiply(x)).add(spec.getCurve().getB()).mod(p);
        if (!left.equals(right)) {
            throw new InvalidKeyException("Public point is not on the curve");
        }
    }

    // An SM certificate must use curveSM2 as ECC curve
    // and SM2withSM3 as signature scheme.
    public static boolean isSMCert(X509Certificate cert) {
        if (!(cert.getPublicKey() instanceof ECPublicKey)) {
            return false;
        }

        ECParameterSpec ecParams = ((ECPublicKey) cert.getPublicKey()).getParams();
        return SM2ParameterSpec.ORDER.equals(ecParams.getOrder())
                && KnownOIDs.SM3withSM2.value().equals(cert.getSigAlgOID());
    }

    // CA has basic constraints extension.
    public static boolean isCA(X509Certificate certificate) {
        return certificate.getBasicConstraints() != -1;
    }

    // If the key usage is critical, it must contain digitalSignature.
    public static boolean isSignCert(X509Certificate certificate) {
        if (certificate == null) {
            return false;
        }

        boolean[] keyUsage = certificate.getKeyUsage();
        return keyUsage == null || keyUsage[0];
    }

    // If the key usage is critical, it must contain one or more of
    // keyEncipherment, dataEncipherment and keyAgreement.
    public static boolean isEncCert(X509Certificate certificate) {
        if (certificate == null) {
            return false;
        }

        boolean[] keyUsage = certificate.getKeyUsage();
        return keyUsage == null || keyUsage[2] || keyUsage[3] || keyUsage[4];
    }

    // KeyUsage bit indices (RFC 5280 4.2.1.3):
    //   0 digitalSignature, 1 nonRepudiation, 2 keyEncipherment,
    //   3 dataEncipherment, 4 keyAgreement.

    // Role-exclusive sign-cert check for peer-certificate role validation.
    // When exclusive is false, delegates to the legacy lenient check. When
    // exclusive is true, the keyUsage extension MUST be present, assert
    // digitalSignature, and NOT assert any encryption/agreement usage, so that
    // an encryption certificate cannot be used in the signing (identity) slot.
    public static boolean isSignCert(X509Certificate certificate, boolean exclusive) {
        if (!exclusive) {
            return isSignCert(certificate);
        }
        if (certificate == null) {
            return false;
        }
        boolean[] ku = certificate.getKeyUsage();
        return ku != null && ku.length >= 5
                && ku[0] && !ku[2] && !ku[3] && !ku[4];
    }

    // Role-exclusive enc-cert check for peer-certificate role validation.
    // When exclusive is false, delegates to the legacy lenient check. When
    // exclusive is true, the keyUsage extension MUST be present, assert one of
    // keyEncipherment/dataEncipherment/keyAgreement, and NOT assert any
    // signature/non-repudiation usage.
    public static boolean isEncCert(X509Certificate certificate, boolean exclusive) {
        if (!exclusive) {
            return isEncCert(certificate);
        }
        if (certificate == null) {
            return false;
        }
        boolean[] ku = certificate.getKeyUsage();
        return ku != null && ku.length >= 5
                && (ku[2] || ku[3] || ku[4]) && !ku[0] && !ku[1];
    }

    // The result of the strict TLCP double-certificate pair validation;
    // anything other than OK is the first failed rule.
    public enum TlcpDualCertPairCheck {
        OK, SIGN_KEY_USAGE, ENC_KEY_USAGE, SAME_KEY, DIFFERENT_IDENTITY
    }

    // Stable "same key" test: compare the encoded key material rather than
    // relying on whether the provider overrides PublicKey.equals(). Fails
    // closed on null certificates or missing encodings (cannot prove the keys
    // differ -> treat as the same).
    public static boolean hasSamePublicKey(X509Certificate a, X509Certificate b) {
        if (a == null || b == null) {
            return true;
        }
        byte[] ea = a.getPublicKey().getEncoded();
        byte[] eb = b.getPublicKey().getEncoded();
        return ea == null || eb == null || java.util.Arrays.equals(ea, eb);
    }

    // TLCP double-certificate same-identity test: requires the subject and the
    // issuer to be identical.
    public static boolean isTlcpPairSameIdentity(
            X509Certificate sign, X509Certificate enc) {
        return sign.getSubjectX500Principal().equals(enc.getSubjectX500Principal())
                && sign.getIssuerX500Principal().equals(enc.getIssuerX500Principal());
    }

    // The single source of truth for the strict TLCP sign/enc pairing rule.
    public static TlcpDualCertPairCheck checkTlcpDualCertPair(
            X509Certificate sign, X509Certificate enc) {
        if (!isSignCert(sign, true)) {
            return TlcpDualCertPairCheck.SIGN_KEY_USAGE;
        }
        if (!isEncCert(enc, true)) {
            return TlcpDualCertPairCheck.ENC_KEY_USAGE;
        }
        if (hasSamePublicKey(sign, enc)) {
            return TlcpDualCertPairCheck.SAME_KEY;
        }
        if (!isTlcpPairSameIdentity(sign, enc)) {
            return TlcpDualCertPairCheck.DIFFERENT_IDENTITY;
        }
        return TlcpDualCertPairCheck.OK;
    }

    // Boolean wrapper over checkTlcpDualCertPair for the producer-side pair
    // search. Strict-only by name, to avoid being misread as "valid in lenient
    // mode too".
    public static boolean isValidTlcpDualCertPairStrict(
            X509Certificate sign, X509Certificate enc) {
        return checkTlcpDualCertPair(sign, enc) == TlcpDualCertPairCheck.OK;
    }

    // Whether the legacy TLCP dual-certificate compat mode is enabled. When
    // true, the sign/enc role keyUsage checks are relaxed and the
    // same-identity binding is not enforced. A single certificate serving both
    // roles is NEVER accepted, even in this mode.
    //
    // SECURITY: enabling this weakens peer-certificate role validation; use
    // only as an interop break-glass. Opt in with:
    //   -Djdk.tlcp.dualCertCompat=true
    public static boolean isDualCertCompat() {
        return Boolean.parseBoolean(AccessController.doPrivileged(
                new PrivilegedAction<String>() {
                    public String run() {
                        return System.getProperty(
                                "jdk.tlcp.dualCertCompat", "false");
                    }
                }));
    }

    private SMUtil() { }
}
