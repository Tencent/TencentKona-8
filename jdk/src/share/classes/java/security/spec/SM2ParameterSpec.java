/*
 * Copyright (C) 2022, 2023, Tencent. All rights reserved.
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

package java.security.spec;

import java.math.BigInteger;
import java.security.MessageDigest;

/**
 * The EC domain parameters used by SM2.
 * The parameters are defined by China's specification GB/T 32918.5-2017.
 */
public final class SM2ParameterSpec extends ECParameterSpec {

    private static class InstanceHolder {

        private static final SM2ParameterSpec INSTANCE = new SM2ParameterSpec();
    }

    public static SM2ParameterSpec instance() {
        return InstanceHolder.INSTANCE;
    }

    /**
     * The SM2 elliptic curve.
     */
    public static final EllipticCurve CURVE = curve();

    /**
     * The generator or base point.
     */
    public static final ECPoint GENERATOR = generator();

    /**
     * The order of the generator.
     */
    public static final BigInteger ORDER = order();

    /**
     * The cofactor.
     */
    public static final BigInteger COFACTOR = cofactor();

    /**
     * The OID of the SM2 elliptic curve.
     */
    public static final String OID = "1.2.156.10197.1.301";

    // 0x06082A811CCF5501822D
    // OBJECT IDENTIFIER 1.2.156.10197.1.301
    private static final byte[] ENCODED_OID = new byte[] {
            6, 8, 42, -127, 28, -49, 85, 1, -126, 45 };

    private SM2ParameterSpec() {
        super(CURVE, GENERATOR, ORDER, COFACTOR.intValue());
    }

    private static EllipticCurve curve() {
        return new EllipticCurve(
                new ECFieldFp(new BigInteger(
                        "FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFF", 16)),
                new BigInteger("FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFC", 16),
                new BigInteger("28E9FA9E9D9F5E344D5A9E4BCF6509A7F39789F515AB8F92DDBCBD414D940E93", 16),
                null);
    }

    private static ECPoint generator() {
        return new ECPoint(
                new BigInteger("32C4AE2C1F1981195F9904466A39C9948FE30BBFF2660BE1715A4589334C74C7", 16),
                new BigInteger("BC3736A2F4F6779C59BDCEE36B692153D0A9877CC62A474002DF32E52139F0A0", 16));
    }

    private static BigInteger order() {
        return new BigInteger("FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123", 16);
    }

    private static BigInteger cofactor() {
        return BigInteger.ONE;
    }

    /**
     * Indicates if the encoded OID is SM2 elliptic curve.
     *
     * @param encodedOid the encoded OID.
     * @return true if the encoded OID is SM2 elliptic curve; otherwise false.
     */
    public static boolean isCurveSM2(byte[] encodedOid) {
        return MessageDigest.isEqual(ENCODED_OID, encodedOid);
    }

    /**
     * Indicates if the OID is SM2 elliptic curve.
     *
     * @param oid the encoded OID.
     * @return true if the OID is SM2 elliptic curve; otherwise false.
     */
    public static boolean isCurveSM2(String oid) {
        return OID.equals(oid);
    }

    @Override
    public String toString() {
        return "curveSM2 (" + OID + ")";
    }
}
