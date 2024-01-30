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
 * @summary Test HmacSM3.
 * @compile ../../Utils.java
 * @run testng HmacSM3Test
 */

import javax.crypto.Mac;
import javax.crypto.ShortBufferException;
import javax.crypto.spec.SecretKeySpec;
import java.nio.ByteBuffer;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.expectThrows;

public class HmacSM3Test {

    private static final byte[] KEY = Utils.hexToBytes("0123456789abcdef0123456789abcdef");
    private static final byte[] MESSAGE = Utils.hexToBytes("616263");

    private static final int SM3_HMAC_LEN = 32;

    @Test
    public void testMac() throws Exception {
        Mac hmacSM3 = Mac.getInstance("HmacSM3");
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");
        hmacSM3.init(keySpec);
        byte[] mac = hmacSM3.doFinal(Utils.hexToBytes("616263"));
        assertEquals(SM3_HMAC_LEN, mac.length);
    }

    @Test
    public void testUpdateByte() throws Exception {
        Mac hmacSM3 = Mac.getInstance("HmacSM3");
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");
        hmacSM3.init(keySpec);
        for (byte b : MESSAGE) {
            hmacSM3.update(b);
        }
        byte[] mac = hmacSM3.doFinal();
        assertEquals(SM3_HMAC_LEN, mac.length);
    }

    @Test
    public void testUpdateBytes() throws Exception {
        Mac hmacSM3 = Mac.getInstance("HmacSM3");
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");
        hmacSM3.init(keySpec);
        hmacSM3.update(MESSAGE, 0, MESSAGE.length / 2);
        hmacSM3.update(MESSAGE, MESSAGE.length / 2, MESSAGE.length - MESSAGE.length / 2);
        byte[] mac = hmacSM3.doFinal();
        assertEquals(SM3_HMAC_LEN, mac.length);
    }

    @Test
    public void testNullBytes() throws Exception {
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");

        Mac hmacSM31 = Mac.getInstance("HmacSM3");
        hmacSM31.init(keySpec);
        hmacSM31.update((byte[]) null);
        byte[] mac1 = hmacSM31.doFinal();

        Mac hmacSM32 = Mac.getInstance("HmacSM3");
        hmacSM32.init(keySpec);
        hmacSM32.update(new byte[0]);
        byte[] mac2 = hmacSM32.doFinal();

        assertEquals(mac1, mac2);
    }

    @Test
    public void testByteBuffer() throws Exception {
        Mac hmacSM3 = Mac.getInstance("HmacSM3");
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");
        hmacSM3.init(keySpec);
        ByteBuffer buffer = ByteBuffer.wrap(MESSAGE);
        hmacSM3.update(buffer);
        byte[] mac = hmacSM3.doFinal();
        assertEquals(SM3_HMAC_LEN, mac.length);
    }

    @Test
    public void testNullByteBuffer() throws Exception {
        Mac hmacSM3 = Mac.getInstance("HmacSM3");
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");
        hmacSM3.init(keySpec);
        expectThrows(
                IllegalArgumentException.class,
                () -> {
                    hmacSM3.update((ByteBuffer) null);
                });
    }

    @Test
    public void testEmptyInput() throws Exception {
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");

        Mac hmacSM31 = Mac.getInstance("HmacSM3");
        hmacSM31.init(keySpec);
        hmacSM31.update(new byte[0]);
        byte[] mac1 = hmacSM31.doFinal();

        Mac hmacSM32 = Mac.getInstance("HmacSM3");
        hmacSM32.init(keySpec);
        hmacSM32.update(ByteBuffer.wrap(new byte[0]));
        byte[] mac2 = hmacSM32.doFinal();

        assertEquals(mac1, mac2);
    }

    @Test
    public void testNoInput() throws Exception {
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");

        Mac hmacSM31 = Mac.getInstance("HmacSM3");
        hmacSM31.init(keySpec);
        byte[] mac1 = hmacSM31.doFinal();

        Mac hmacSM32 = Mac.getInstance("HmacSM3");
        hmacSM32.init(keySpec);
        hmacSM32.update(new byte[0]);
        byte[] mac2 = hmacSM32.doFinal();

        assertEquals(mac1, mac2);
    }

    @Test
    public void testOutOfBoundsOnUpdate() throws Exception {
        outOfBoundsOnUpdate(16, 0, 32);
        outOfBoundsOnUpdate(7, 0, 32);
        outOfBoundsOnUpdate(16, -8, 16);
        outOfBoundsOnUpdate(16, 8, -8);
        outOfBoundsOnUpdate(16, Integer.MAX_VALUE, 8);
    }

    private static void outOfBoundsOnUpdate(int inputSize, int ofs, int len)
            throws Exception {
        Mac hmacSM3 = Mac.getInstance("HmacSM3");
        hmacSM3.init(new SecretKeySpec(KEY, "HmacSM3"));

        try {
            hmacSM3.update(new byte[inputSize], ofs, len);
            throw new Exception("invalid call succeeded");
        } catch (IllegalArgumentException e) {
            System.out.println("Expected: " + e);
        }
    }

    @Test
    public void testOutOfBoundsOnOutBuf() throws Exception {
        outOfBoundsOnOutBuf(16, 0);
        outOfBoundsOnOutBuf(31, 0);
        outOfBoundsOnOutBuf(32, -1);
        outOfBoundsOnOutBuf(32, Integer.MAX_VALUE);
    }

    private static void outOfBoundsOnOutBuf(int outSize, int ofs)
            throws Exception {
        Mac hmacSM3 = Mac.getInstance("HmacSM3");
        hmacSM3.init(new SecretKeySpec(KEY, "HmacSM3"));
        hmacSM3.update(MESSAGE);

        try {
            hmacSM3.doFinal(new byte[outSize], ofs);
            throw new Exception("invalid call succeeded");
        } catch (ShortBufferException | ArrayIndexOutOfBoundsException e) {
            System.out.println("Expected: " + e);
        }
    }

    @Test
    public void testClone() throws Exception {
        Mac hmacSM3 = Mac.getInstance("HmacSM3");
        SecretKeySpec keySpec = new SecretKeySpec(KEY, "HmacSM3");
        hmacSM3.init(keySpec);

        hmacSM3.update(MESSAGE, 0, MESSAGE.length / 3);
        hmacSM3.update(MESSAGE[MESSAGE.length / 3]);

        Mac clone = (Mac) hmacSM3.clone();

        hmacSM3.update(MESSAGE, MESSAGE.length / 3 + 1,
                MESSAGE.length - MESSAGE.length / 3 - 2);
        clone.update(MESSAGE, MESSAGE.length / 3 + 1,
                MESSAGE.length - MESSAGE.length / 3 - 2);

        hmacSM3.update(MESSAGE[MESSAGE.length - 1]);
        clone.update(MESSAGE[MESSAGE.length - 1]);

        byte[] mac = hmacSM3.doFinal();
        byte[] macClone = clone.doFinal();

        assertEquals(mac, macClone);
    }
}
