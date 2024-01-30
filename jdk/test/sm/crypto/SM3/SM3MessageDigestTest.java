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
 * @summary Test SM3 message digest.
 * @compile ../../Utils.java
 * @run testng SM3MessageDigestTest
 */

import java.nio.ByteBuffer;
import java.security.DigestException;
import java.security.MessageDigest;
import java.util.Random;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.expectThrows;

public class SM3MessageDigestTest {

    private static final byte[] MESSAGE_SHORT = Utils.hexToBytes("616263");
    private static final byte[] DIGEST_SHORT = Utils.hexToBytes(
            "66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0");

    private static final byte[] MESSAGE_LONG = Utils.hexToBytes(
            "61626364616263646162636461626364616263646162636461626364616263646162636461626364616263646162636461626364616263646162636461626364");
    private static final byte[] DIGEST_LONG = Utils.hexToBytes(
            "debe9ff92275b8a138604889c18e5a4d6fdb70e5387e5765293dcba39c0c5732");

    @Test
    public void testKAT() throws Exception {
        checkDigest(MESSAGE_SHORT, DIGEST_SHORT);
        checkDigest(MESSAGE_LONG, DIGEST_LONG);
    }

    private static void checkDigest(byte[] message, byte[] expectedDigest)
            throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        byte[] digest = md.digest(message);
        assertEquals(expectedDigest, digest);
    }

    @Test
    public void testGetDigestLength() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        int digestLength = md.getDigestLength();
        assertEquals(32, digestLength);
    }

    @Test
    public void testUpdateByte() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");

        for (byte b : MESSAGE_SHORT) {
            md.update(b);
        }
        byte[] digest= md.digest();

        assertEquals(DIGEST_SHORT, digest);
    }

    @Test
    public void testUpdateBytes() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");

        md.update(MESSAGE_SHORT, 0, MESSAGE_SHORT.length / 2);
        md.update(MESSAGE_SHORT, MESSAGE_SHORT.length / 2,
                MESSAGE_SHORT.length - MESSAGE_SHORT.length / 2);
        byte[] digest = md.digest();

        assertEquals(DIGEST_SHORT, digest);
    }

    @Test
    public void testOutputBuf() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update(MESSAGE_SHORT);
        byte[] out = new byte[32];
        int length = md.digest(out, 0, out.length);

        assertEquals(32, length);
        assertEquals(DIGEST_SHORT, out);
    }

    @Test
    public void testNullBytes() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        expectThrows(
                NullPointerException.class,
                () -> md.update((byte[]) null));
    }

    @Test
    public void testByteBuffer() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update(ByteBuffer.wrap(MESSAGE_SHORT));
        byte[] digest = md.digest();
        assertEquals(DIGEST_SHORT, digest);
    }

    @Test
    public void testNullByteBuffer() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        expectThrows(
                NullPointerException.class,
                () -> md.update((ByteBuffer) null));
    }

    @Test
    public void testEmptyInput() throws Exception {
        MessageDigest md1 = MessageDigest.getInstance("SM3");
        md1.update(new byte[0]);
        byte[] digest1 = md1.digest();

        MessageDigest md2 = MessageDigest.getInstance("SM3");
        md2.update(ByteBuffer.wrap(new byte[0]));
        byte[] digest2 = md2.digest();

        assertEquals(digest1, digest2);
    }

    @Test
    public void testNoInput() throws Exception {
        MessageDigest md1 = MessageDigest.getInstance("SM3");
        byte[] digest1 = md1.digest();

        MessageDigest md2 = MessageDigest.getInstance("SM3");
        md2.update(new byte[0]);
        byte[] digest2 = md2.digest();

        assertEquals(digest1, digest2);
    }

    @Test
    public void testReuse() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");

        byte[] digestShort = md.digest(MESSAGE_SHORT);
        assertEquals(DIGEST_SHORT, digestShort);

        byte[] digestLong = md.digest(MESSAGE_LONG);
        assertEquals(DIGEST_LONG, digestLong);
    }

    @Test
    public void testReset() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");

        md.update(MESSAGE_SHORT, 0, MESSAGE_SHORT.length / 2);
        md.reset();
        md.update(MESSAGE_SHORT, 0, MESSAGE_SHORT.length / 2);
        md.update(MESSAGE_SHORT, MESSAGE_SHORT.length / 2,
                MESSAGE_SHORT.length - MESSAGE_SHORT.length / 2);
        byte[] digest = md.digest();

        assertEquals(DIGEST_SHORT, digest);
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
        MessageDigest md = MessageDigest.getInstance("SM3");

        try {
            md.update(new byte[inputSize], ofs, len);
            throw new Exception("invalid call succeeded");
        } catch (ArrayIndexOutOfBoundsException | IllegalArgumentException e) {
            System.out.println("Expected: " + e);
        }
    }

    @Test
    public void testOutOfBoundsOnOutBuf() throws Exception {
        outOfBoundsOnOutBuf(16, 0, 32);
        outOfBoundsOnOutBuf(32, 0, 31);
        outOfBoundsOnOutBuf(32, -1, 32);
        outOfBoundsOnOutBuf(32, Integer.MAX_VALUE, 32);
    }

    private static void outOfBoundsOnOutBuf(int outSize, int ofs, int len)
            throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update(MESSAGE_SHORT);

        try {
            md.digest(new byte[outSize], ofs, len);
            throw new Exception("invalid call succeeded");
        } catch (IllegalArgumentException | DigestException
                 | ArrayIndexOutOfBoundsException e) {
            System.out.println("Expected: " + e);
        }
    }

    @Test
    public void testOffset() throws Exception {
        offset(0, 64, 0, 128);
        offset(0, 64, 0, 129);
        offset(1, 32, 1, 129);
    }

    private static void offset(
            int minOffset, int maxOffset, int minLen, int maxLen)
            throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");

        Random random = new Random();
        for (int len = minLen; len <= maxLen; len++) {
            byte[] data = new byte[len];
            random.nextBytes(data);
            byte[] digest = null;
            for (int offset = minOffset; offset <= maxOffset; offset++) {
                byte[] offsetData = new byte[len + maxOffset];
                random.nextBytes(offsetData);
                System.arraycopy(data, 0, offsetData, offset, len);
                md.update(offsetData, offset, len);
                byte[] offsetDigest = md.digest();
                if (digest == null) {
                    digest = offsetDigest;
                } else {
                    assertEquals(digest, offsetDigest);
                }
            }
        }
    }

    @Test
    public void testClone() throws Exception {
        MessageDigest md = MessageDigest.getInstance("SM3");
        md.update(MESSAGE_LONG, 0, MESSAGE_LONG.length / 3);
        md.update(MESSAGE_LONG[MESSAGE_LONG.length / 3]);

        MessageDigest clone = (MessageDigest) md.clone();
        clone.update(MESSAGE_LONG, MESSAGE_LONG.length / 3 + 1,
                MESSAGE_LONG.length - MESSAGE_LONG.length / 3 - 2);
        clone.update(MESSAGE_LONG[MESSAGE_LONG.length - 1]);
        byte[] digest = clone.digest();

        assertEquals(DIGEST_LONG, digest);
    }
}
