/*
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/**
 * @test
 * @summary SM4CipherStreamTest
 */
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.security.Key;
import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;
import javax.crypto.spec.SecretKeySpec;
import com.tencent.crypto.provider.SMCSProvider;

public class SM4CipherStreamTest
{

    static String[] cipherTests =
    {
        "0123456789abcdef",
        "0123456789abcdeffedcba9876543210",
    };

    public static boolean areEqual(byte[] var0, byte[] var1) {
        return java.util.Arrays.equals(var0, var1);
    }

    public void test(
        byte[]      keyBytes,
        byte[]      input)
        throws Exception
    {
        Key key;
        Cipher in, out;
        CipherInputStream cIn;
        CipherOutputStream cOut;
        ByteArrayInputStream bIn;
        ByteArrayOutputStream bOut;

        key = new SecretKeySpec(keyBytes, "SM4");

        in = Cipher.getInstance("SM4/ECB/NoPadding", "SMCSProvider");
        out = Cipher.getInstance("SM4/ECB/NoPadding", "SMCSProvider");

        try
        {
            out.init(Cipher.ENCRYPT_MODE, key);
        }
        catch (Exception e)
        {
            fail("SM4 failed initialisation - " + e.toString(), e);
        }

        try
        {
            in.init(Cipher.DECRYPT_MODE, key);
        }
        catch (Exception e)
        {
            fail("SM4 failed initialisation - " + e.toString(), e);
        }

        //
        // encryption pass
        //
        bOut = new ByteArrayOutputStream();

        cOut = new CipherOutputStream(bOut, out);

        try
        {
            for (int i = 0; i != input.length / 2; i++)
            {
                cOut.write(input[i]);
            }
            cOut.write(input, input.length / 2, input.length - input.length / 2);
            cOut.close();
        }
        catch (IOException e)
        {
            fail("SM4 failed encryption - " + e.toString(), e);
        }

        byte[]    bytes;
        bytes = bOut.toByteArray();

        //
        // decryption pass
        //
        bIn = new ByteArrayInputStream(bytes);
        cIn = new CipherInputStream(bIn, in);

        try
        {
            DataInputStream dIn = new DataInputStream(cIn);

            bytes = new byte[input.length];

            for (int i = 0; i != input.length / 2; i++)
            {
                bytes[i] = (byte)dIn.read();
            }
            dIn.readFully(bytes, input.length / 2, bytes.length - input.length / 2);
        }
        catch (Exception e)
        {
            fail("SM4 failed encryption - " + e.toString(), e);
        }

        if (!areEqual(bytes, input))
        {
            fail("SM4 failed decryption");
        }
    }

    private void fail(String s, Exception e) {
        System.err.println(s);
    }

    private void fail(String s) {
        System.err.println(s);
    }

    public void performTest()
        throws Exception
    {
        for (int i = 0; i != cipherTests.length; i += 2)
        {
            test(cipherTests[i].getBytes(), cipherTests[i + 1].getBytes());
        }
    }

    public static void main(String[] args)
    {
        SMCSProvider.install();
        SM4CipherStreamTest sm = new SM4CipherStreamTest();
        try {
            sm.performTest();
        } catch (Exception e) {
            sm.fail("failed !!!! " + e.toString());
        }
    }
}
