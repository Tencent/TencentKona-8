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
 * @summary Basic known-answer-test for all SM3 MessageDigest algorithms
 */

import java.io.*;
import java.security.MessageDigest;
import java.security.Provider;
import java.security.Security;
import java.util.Arrays;
import java.util.Random;

public class DigestKAT {

    private final static char[] hexDigits = "0123456789abcdef".toCharArray();

    public static String toString(byte[] b) {
        StringBuffer sb = new StringBuffer(b.length * 3);
        for (int i = 0; i < b.length; i++) {
            int k = b[i] & 0xff;
            if (i != 0) {
                sb.append(':');
            }
            sb.append(hexDigits[k >>> 4]);
            sb.append(hexDigits[k & 0xf]);
        }
        return sb.toString();
    }

    public static String deParse(byte[] s) {
        try {
            int n = s.length;
            ByteArrayInputStream in = new ByteArrayInputStream(s);
            DataInputStream din = new DataInputStream(in);
            ByteArrayOutputStream out = new ByteArrayOutputStream(n*3);
            while (true) {
                short r = din.readShort();
                char c = nextDeNibble(r);
                if (c == ' ') {
                    break;
                }
                r = din.readShort();
                c = nextDeNibble(r);
                if (c == ' ') {
                    throw new RuntimeException("Invalid byte array " + s);
                }
                out.write(c);
                out.write(':');
            }
            return new String(out.toByteArray());
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public static byte[] parse(String s) {
        try {
            int n = s.length();
            ByteArrayOutputStream out = new ByteArrayOutputStream(n / 3);
            StringReader r = new StringReader(s);
            while (true) {
                int b1 = nextNibble(r);
                if (b1 < 0) {
                    break;
                }
                int b2 = nextNibble(r);
                if (b2 < 0) {
                    throw new RuntimeException("Invalid string " + s);
                }
                int b = (b1 << 4) | b2;
                out.write(b);
            }
            return out.toByteArray();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public static byte[] b(String s) {
        return parse(s);
    }

    private static int nextNibble(StringReader r) throws IOException {
        while (true) {
            int ch = r.read();
            if (ch == -1) {
                return -1;
            } else if ((ch >= '0') && (ch <= '9')) {
                return ch - '0';
            } else if ((ch >= 'a') && (ch <= 'f')) {
                return ch - 'a' + 10;
            } else if ((ch >= 'A') && (ch <= 'F')) {
                return ch - 'A' + 10;
            }
        }
    }

    private static char nextDeNibble(Short r) throws IOException {
        int ch = r;

        char nibble = (char) (ch + '0');
        if (nibble <= '9') {
            return nibble;
        }
        nibble = (char) (ch - 10 + 'a');
        if (nibble <= 'z') {
            return nibble;
        }
        nibble = (char) (ch - 10 + 'A');
        if (nibble <= 'Z') {
            return nibble;
        }
        return ' ';
    }

    static abstract class Test {
        abstract void run(Provider p) throws Exception;
    }

    static class DigestTest extends Test {
        private final String alg;
        private final byte[] data;
        private final byte[] digest;
        DigestTest(String alg, byte[] data, byte[] digest) {
            this.alg = alg;
            this.data = data;
            this.digest = digest;
            String str = deParse(digest);
            System.out.println("deParse:" + str);
        }
        void run(Provider p) throws Exception {
            MessageDigest md;
            if (alg.equals("MD4")) {
                md = sun.security.provider.MD4.getInstance();
            } else {
                md = MessageDigest.getInstance(alg, p);
            }
            md.update(data);
            byte[] myDigest = md.digest();
            if (Arrays.equals(digest, myDigest) == false) {
                System.out.println("Digest test for " + alg + " failed:");
                if (data.length < 256) {
                    System.out.println("data: " + DigestKAT.toString(data));
                }
                System.out.println("dig:  " + DigestKAT.toString(digest));
                System.out.println("out:  " + DigestKAT.toString(myDigest));
                throw new Exception("Digest test for " + alg + " failed");
            }
//          System.out.println("Passed " + alg);
        }
    }

    private static byte[] s(String s) {
        try {
            return s.getBytes("UTF8");
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static Test t(String alg, byte[] data, String digest) {
        return new DigestTest(alg, data, parse(digest));
    }

    private final static byte[] ALONG, BLONG;

    static {
        ALONG = new byte[1024 * 128];
        Arrays.fill(ALONG, (byte)'a');
        Random random = new Random(12345678);
        BLONG = new byte[1024 * 128];
        random.nextBytes(BLONG);
    }

    private final static Test[] tests = {
            t("SM3", s(""), "1a:b2:1d:83:55:cf:a1:7f:8e:61:19:48:31:e8:1a:8f:22:be:c8:c7:28:fe:fb:74:7e:d0:35:eb:50:82:aa:2b"),
            t("SM3", s("a"), "62:34:76:ac:18:f6:5a:29:09:e4:3c:7f:ec:61:b4:9c:7e:76:4a:91:a1:8c:cb:82:f1:91:7a:29:c8:6c:5e:88"),
            t("SM3", s("abc"), "66:c7:f0:f4:62:ee:ed:d9:d1:f2:d4:6b:dc:10:e4:e2:41:67:c4:87:5c:f2:f7:a2:29:7d:a0:2b:8f:4b:a8:e0"),
            t("SM3", s("message digest"), "c5:22:a9:42:e8:9b:d8:0d:97:dd:66:6e:7a:55:31:b3:61:88:c9:81:71:49:e9:b2:58:df:e5:1e:ce:98:ed:77"),
            t("SM3", s("abcdefghijklmnopqrstuvwxyz"), "b8:0f:e9:7a:4d:a2:4a:fc:27:75:64:f6:6a:35:9e:f4:40:46:2a:d2:8d:cc:6d:63:ad:b2:4d:5c:20:a6:15:95"),
            t("SM3", s("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"), "29:71:d1:0c:88:42:b7:0c:97:9e:55:06:34:80:c5:0b:ac:ff:d9:0e:98:e2:e6:0d:25:12:ab:8a:bf:df:ce:c5"),
            t("SM3", s("12345678901234567890123456789012345678901234567890123456789012345678901234567890"), "ad:81:80:53:21:f3:e6:9d:25:12:35:bf:88:6a:56:48:44:87:3b:56:dd:7d:de:40:0f:05:5b:7d:de:39:30:7a"),
            t("SM3", ALONG, "93:36:1b:ee:3d:ea:be:a8:4a:88:a7:e2:9f:25:74:a2:80:ee:67:1e:16:fa:d6:bf:21:8f:f9:68:ac:a9:a2:87"),
            t("SM3", BLONG, "74:c9:34:8b:aa:22:98:ca:eb:96:c5:e8:d5:bd:c5:98:e0:c2:36:14:7d:49:2e:21:5f:3c:df:90:0c:4d:94:41"),

    };

    static void runTests(Test[] tests) throws Exception {
        long start = System.currentTimeMillis();
        Provider p = Security.getProvider("SMCSProvider");
        System.out.println("Testing provider " + p.getName() + "...");
        for (int i = 0; i < tests.length; i++) {
            Test test = tests[i];
            test.run(p);
        }
        System.out.println("All tests passed");
        long stop = System.currentTimeMillis();
        System.out.println("Done (" + (stop - start) + " ms).");
    }

    public static void main(String[] args) throws Exception {
        runTests(tests);
    }

}
