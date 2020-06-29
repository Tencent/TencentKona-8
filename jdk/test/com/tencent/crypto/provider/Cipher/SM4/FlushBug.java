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

/*
 * @test
 * @summary FlushBug
 */

import com.tencent.crypto.provider.SMCSProvider;

import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;
import javax.crypto.KeyGenerator;
import javax.crypto.spec.IvParameterSpec;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.security.Key;
import java.security.Provider;
import java.security.SecureRandom;
import java.security.Security;

public class FlushBug {
    public static void main(String[] args) throws Exception {

        Provider prov = new SMCSProvider();
        Security.addProvider(prov);

        SecureRandom sr = new SecureRandom();

        // Create new DES key.
        KeyGenerator kg = KeyGenerator.getInstance("SM4");
        kg.init(sr);
        Key key = kg.generateKey();

        // Generate an IV.
        byte[] iv_bytes = new byte[16];
        sr.nextBytes(iv_bytes);
        IvParameterSpec iv = new IvParameterSpec(iv_bytes);

        // Create the consumer
        Cipher decrypter = Cipher.getInstance("SM4/CBC/PKCS7Padding");
        decrypter.init(Cipher.DECRYPT_MODE, key, iv);
        PipedInputStream consumer = new PipedInputStream();
        InputStream in = new CipherInputStream(consumer, decrypter);

        // Create the producer
        Cipher encrypter = Cipher.getInstance("SM4/CBC/PKCS7Padding");
        encrypter.init(Cipher.ENCRYPT_MODE, key, iv);
        PipedOutputStream producer = new PipedOutputStream();
        OutputStream out = new CipherOutputStream(producer, encrypter);

        producer.connect(consumer); // connect pipe

        byte[] plaintext = "abcdef".getBytes();
        for (int i = 0; i < plaintext.length; i++) {
            out.write(plaintext[i]);
            out.flush();
            int b = in.read();
            String original = new String(plaintext, i, 1);
            String result = new String(new byte[] { (byte)b });
            System.out.println("  " + original + " -> " + result);
        }
    }
}

