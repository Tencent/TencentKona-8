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
 * @summary Test SM4 parameters.
 * @compile ../../Utils.java
 * @run testng SM4ParametersTest
 */

import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.security.AlgorithmParameters;
import java.security.spec.InvalidParameterSpecException;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.expectThrows;

public class SM4ParametersTest {

    private static final int SM4_GCM_TAG_LEN = 16;

    private static final byte[] KEY = Utils.hexToBytes("0123456789abcdef0123456789abcdef");
    private static final byte[] IV = Utils.hexToBytes("30313233343536373839616263646566");
    private static final byte[] GCM_IV = Utils.hexToBytes("303132333435363738396162");
    private static final byte[] ENCODED_PARAMS = Utils.hexToBytes("041030313233343536373839616263646566");
    private static final byte[] GCM_ENCODED_PARAMS = Utils.hexToBytes("3011040c303132333435363738396162020110");

    private static final byte[] MESSAGE = Utils.hexToBytes(
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

    @Test
    public void testEncode() throws Exception {
        AlgorithmParameters params = AlgorithmParameters.getInstance("SM4");
        params.init(new IvParameterSpec(IV));
        byte[] encodedParams = params.getEncoded();
        assertEquals(ENCODED_PARAMS, encodedParams);

        params = AlgorithmParameters.getInstance("GCM");
        params.init(new GCMParameterSpec(SM4_GCM_TAG_LEN << 3, GCM_IV));
        encodedParams = params.getEncoded();
        assertEquals(GCM_ENCODED_PARAMS, encodedParams);
    }

    @Test
    public void testDecode() throws Exception {
        AlgorithmParameters params = AlgorithmParameters.getInstance("SM4");
        params.init(ENCODED_PARAMS);
        IvParameterSpec decodedParams
                = params.getParameterSpec(IvParameterSpec.class);
        assertEquals(IV, decodedParams.getIV());

        final AlgorithmParameters gcmParams = AlgorithmParameters.getInstance("GCM");
        gcmParams.init(GCM_ENCODED_PARAMS);
        GCMParameterSpec decodedGcmParams
                = gcmParams.getParameterSpec(GCMParameterSpec.class);
        assertEquals(GCM_IV, decodedGcmParams.getIV());
    }

    @Test
    public void testDecodeFailed() throws Exception {
        final AlgorithmParameters params = AlgorithmParameters.getInstance("GCM");
        params.init(GCM_ENCODED_PARAMS);
        expectThrows(
                InvalidParameterSpecException.class,
                () -> params.getParameterSpec(IvParameterSpec.class));

        final AlgorithmParameters gcmParams = AlgorithmParameters.getInstance("SM4");
        gcmParams.init(ENCODED_PARAMS);
        expectThrows(
                InvalidParameterSpecException.class,
                () -> gcmParams.getParameterSpec(GCMParameterSpec.class));
    }

    @Test
    public void testGetParameters() throws Exception {
        Cipher encrypter = Cipher.getInstance("SM4/CBC/NoPadding");
        encrypter.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(KEY, "SM4"));
        AlgorithmParameters params = encrypter.getParameters();
        byte[] cipertext = encrypter.doFinal(MESSAGE);

        Cipher decrypter = Cipher.getInstance("SM4/CBC/NoPadding");
        decrypter.init(Cipher.DECRYPT_MODE, new SecretKeySpec(KEY, "SM4"), params);
        byte[] cleartext = decrypter.doFinal(cipertext);

        assertEquals(MESSAGE, cleartext);
    }
}
