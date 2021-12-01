/*
 *
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
 * @run main/othervm -Xint TestUTF8Encode
 *
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:-TieredCompilation -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -XX:-TieredCompilation -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -XX:-TieredCompilation -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:TieredStopAtLevel=1 -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -XX:TieredStopAtLevel=1 -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -XX:TieredStopAtLevel=1 -Xcomp -XX:-BackgroundCompilation -XX:+PrintCompilation -XX:CompileOnly=Encoder.encodeArrayLoop TestUTF8Encode
 * @summary test pass and failure case for UTF8 encoder
 */

import java.util.*;
import java.nio.*;
import java.nio.charset.*;
import java.lang.reflect.*;
import sun.nio.cs.Surrogate;

public class TestUTF8Encode {
    private static CharsetEncoder encoder;
    private static Class encoderC;
    private static Method encodeArrayLoopM;
    private static Surrogate.Parser sgp;
    
    public static void main(String[] args) throws Exception {
        try {
            Class utf8C = Class.forName("sun.nio.cs.UTF_8");
            Constructor[] cons = utf8C.getDeclaredConstructors();
            Constructor con = cons[0];
            con.setAccessible(true);
            Object obj = con.newInstance();
            Method newEncoder = utf8C.getMethod("newEncoder");
            newEncoder.setAccessible(true);
            encoder = (CharsetEncoder)newEncoder.invoke(obj);

            encoderC = Class.forName("sun.nio.cs.UTF_8$Encoder");
            encodeArrayLoopM = encoderC.getDeclaredMethod("encodeArrayLoop", CharBuffer.class, ByteBuffer.class);
            encodeArrayLoopM.setAccessible(true);
        } catch (Exception e) {
            e.printStackTrace();
        }
        TestOneByte();
        TestTwoByte();
        TestThreeByte();
        TestSurrogate(); // first JIT compilation, and codes will be deoptimized after this test.
        TestSurrogate(); // trigger second JIT compilation
        TestHybird();
        TestOverflow();
    }

    private static void Error(String msg) {
        throw new RuntimeException(msg);
    }

    private static void updatePositions(Buffer src, int sp,
                                        Buffer dst, int dp) {
        src.position(sp - src.arrayOffset());
        dst.position(dp - dst.arrayOffset());
    }

    private static CoderResult overflow(CharBuffer src, int sp,
                                        ByteBuffer dst, int dp) {
        updatePositions(src, sp, dst, dp);
        return CoderResult.OVERFLOW;
    }

    private static CoderResult encodeArrayLoop(CharBuffer src, ByteBuffer dst)
    {
        char[] sa = src.array();
        int sp = src.arrayOffset() + src.position();
        int sl = src.arrayOffset() + src.limit();
    
        byte[] da = dst.array();
        int dp = dst.arrayOffset() + dst.position();
        int dl = dst.arrayOffset() + dst.limit();
        int dlASCII = dp + Math.min(sl - sp, dl - dp);
    
        // ASCII only loop
        while (dp < dlASCII && sa[sp] < '\u0080')
            da[dp++] = (byte) sa[sp++];
    
        while (sp < sl) {
            char c = sa[sp];
            if (c < 0x80) {
                // Have at most seven bits
                if (dp >= dl)
                    return overflow(src, sp, dst, dp);
                da[dp++] = (byte)c;
            } else if (c < 0x800) {
                // 2 bytes, 11 bits
                if (dl - dp < 2)
                    return overflow(src, sp, dst, dp);
                da[dp++] = (byte)(0xc0 | (c >> 6));
                da[dp++] = (byte)(0x80 | (c & 0x3f));
            } else if (Character.isSurrogate(c)) {
                // Have a surrogate pair
                if (sgp == null)
                    sgp = new Surrogate.Parser();
                int uc = sgp.parse(c, sa, sp, sl);
                if (uc < 0) {
                    updatePositions(src, sp, dst, dp);
                    return sgp.error();
                }
                if (dl - dp < 4)
                    return overflow(src, sp, dst, dp);
                da[dp++] = (byte)(0xf0 | ((uc >> 18)));
                da[dp++] = (byte)(0x80 | ((uc >> 12) & 0x3f));
                da[dp++] = (byte)(0x80 | ((uc >>  6) & 0x3f));
                da[dp++] = (byte)(0x80 | (uc & 0x3f));
                sp++;  // 2 chars
            } else {
                // 3 bytes, 16 bits
                if (dl - dp < 3)
                    return overflow(src, sp, dst, dp);
                da[dp++] = (byte)(0xe0 | ((c >> 12)));
                da[dp++] = (byte)(0x80 | ((c >>  6) & 0x3f));
                da[dp++] = (byte)(0x80 | (c & 0x3f));
            }
            sp++;
        }
        updatePositions(src, sp, dst, dp);
        return CoderResult.UNDERFLOW;
    }

    private static void RunTestCase(char[] chars, byte[] buffer, byte[] resultRight,
                                    CoderResult result,
                                    int charEndPos, int byteEndPos)
                                    throws Exception {
        if (buffer == null) {
            buffer = new byte[chars.length * 4];
        }
        CharBuffer cbuf = CharBuffer.wrap(chars);
        ByteBuffer bbuf = ByteBuffer.wrap(buffer);
        CoderResult ret = (CoderResult)encodeArrayLoopM.invoke(encoder, cbuf, bbuf);
        if (!ret.equals(result)) {
            Error("Invalid CoderResult: " + ret + " (should be " + result + ")");
        }
        if (cbuf.position() != charEndPos) {
            Error("Invalid CharBuffer position: " + cbuf.position() + " (should be " + charEndPos + ")");
        }
        if (bbuf.position() != byteEndPos) {
            Error("Invalid ByteBuffer position: " + bbuf.position() + " (should be " + byteEndPos + ")");
        }
        int resultByteNum = byteEndPos;
        byte[] resultBytes = new byte[chars.length * 4];
        bbuf.flip();
        bbuf.get(resultBytes, 0, resultByteNum);
        for (int i = 0; i < resultByteNum; i++) {
            if (resultBytes[i] != resultRight[i]) {
                Error("Invalid encoded byte :" + resultBytes[i] + " (should be " +
                      resultRight[i] + ") at " + (i));
            }
        }
    }

    private static void PrepareAndRunTestCase(char[] chars, byte[] buffer) throws Exception {
        byte[] destBytes = new byte[buffer.length];
        CharBuffer cbuf = CharBuffer.wrap(chars);
        ByteBuffer bbuf = ByteBuffer.wrap(destBytes);
        CoderResult ret = encodeArrayLoop(cbuf, bbuf);
        int byteEndPos = bbuf.position();
        bbuf.flip();
        byte[] results = new byte[byteEndPos];
        bbuf.get(results, 0, byteEndPos);
        RunTestCase(chars, buffer, results, ret, cbuf.position(), byteEndPos);
    }
    private static void PrepareAndRunTestCase(char[] chars) throws Exception {
        PrepareAndRunTestCase(chars, new byte[chars.length * 4]);
    }

    private static void TestSequentialArray(int begin, int end, int len) throws Exception {
        int charLen = end - begin;
        char[] chars = new char[charLen];
        byte[] bytes = new byte[charLen * len];
        for (int j = begin; j < end; j++) {
            chars[j - begin] = (char)j;
        }
        PrepareAndRunTestCase(chars, bytes);
    }

    private static void TestOneByte() throws Exception {
        System.out.println("Testing 1-byte char array...");
        TestSequentialArray(1, 0x80, 1);
    }

    private static void TestTwoByte() throws Exception {
        System.out.println("Testing 2-bytes char array...");
        TestSequentialArray(0x80, 0x800, 2);
    }

    private static void TestThreeByte() throws Exception {
        System.out.println("Testing 3-bytes char array...");
        TestSequentialArray(0x800, 0xD800, 3);
        TestSequentialArray(0xE000, 0x10000, 3);
    }

    private static void TestSurrogate() throws Exception {
        System.out.println("Testing surrogate char array...");
        TestSequentialArray(0xD800, 0xE000, 4);
    }

    static class CharArrayGenerator {
        private static Random rand = new Random();
        private char[] charArray;
        private int length;
        private int pos;
        private StringBuilder msg = new StringBuilder();

        public CharArrayGenerator(int len) {
            charArray = new char[len];
            length = len;
            pos = 0;
            msg.append("    Hybird String[");
            msg.append(len);
            msg.append("] ");
        }

        private CharArrayGenerator AddBytes(int begin, int end, int num) {
            if (pos + num > length) {
                num = length - pos;
            }

            for (int i = 0; i < num; i++) {
                int c = begin + rand.nextInt(end - begin);
                charArray[pos + i] = (char)c; 
            }
            pos += num;
            return this;
        }

	public CharArrayGenerator AddOneByte(int num) {
            msg.append("1-byte[");
            msg.append(num);
            msg.append("] ");
            return AddBytes(1, 0x80, num);
        }
        public CharArrayGenerator AddTwoByte(int num) {
            msg.append("2-bytes[");
            msg.append(num);
            msg.append("] ");
            return AddBytes(0x80, 0x800, num);
        }
        public CharArrayGenerator AddThreeByte(int num) {
            msg.append("3-bytes[");
            msg.append(num);
            msg.append("] ");
            return AddBytes(0x800, 0xD800, num);
        }
        public CharArrayGenerator AddSurrogateByte(int num) {
            msg.append("Surrogate[");
            msg.append(num);
            msg.append("] ");
            return AddBytes(0xD800, 0xE000, num);
        }
        public char[] GetChars(boolean print) {
            if (pos != length) {
                throw new RuntimeException("Invalid char array position: " + pos + " (" + length + ")");
            }
            if (print) System.out.println(msg);
            return charArray;
        }
        public char[] GetChars() {
            return GetChars(false);
        }
    }

    private static void TestHybird() throws Exception {
        System.out.println("Testing hybird char array...");
        int len = 128;
        for (int i = 1; i < len; i++) {
            int second_len = len - i;
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddTwoByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddThreeByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddSurrogateByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddOneByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddThreeByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddSurrogateByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddOneByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddTwoByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddSurrogateByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddOneByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddTwoByte(second_len).GetChars());
            PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddThreeByte(second_len).GetChars());
        }
        len = 64;
        for (int i = 1; i < len - 1; i++) {
            for (int j = 1; j < len - i; j++) {
                int k = len - i - j;
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddTwoByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddTwoByte(j).AddSurrogateByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddThreeByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddThreeByte(j).AddSurrogateByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddSurrogateByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddSurrogateByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddTwoByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddThreeByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddSurrogateByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddOneByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddOneByte(j).AddSurrogateByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddThreeByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddThreeByte(j).AddSurrogateByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddSurrogateByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddSurrogateByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddOneByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddThreeByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddSurrogateByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddOneByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddOneByte(j).AddSurrogateByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddTwoByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddTwoByte(j).AddSurrogateByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddSurrogateByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddSurrogateByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddOneByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddTwoByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddSurrogateByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddOneByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddOneByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddTwoByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddTwoByte(j).AddThreeByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddThreeByte(j).AddOneByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddThreeByte(j).AddTwoByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddOneByte(j).AddSurrogateByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddTwoByte(j).AddSurrogateByte(k).GetChars());
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddThreeByte(j).AddSurrogateByte(k).GetChars());
            }
        }
    }

    private static void TestOverflow() throws Exception {
        System.out.println("Testing overflow char array...");
        int len = 64;
        for (int i = 1; i < len - 1; i++) {
            for (int j = 1; j < len - i; j++) {
                int k = len - i - j;
                int byteLen1 = i >> 1;
                int byteLen2 = i + (j >> 1);
                int byteLen3 = i + j + (k >> 1);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddTwoByte(j).AddThreeByte(k).GetChars(), new byte[byteLen1]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddOneByte(j).AddThreeByte(k).GetChars(), new byte[byteLen2]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddThreeByte(j).AddOneByte(k).GetChars(), new byte[byteLen3]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddTwoByte(i).AddOneByte(j).AddThreeByte(k).GetChars(), new byte[byteLen1]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddTwoByte(j).AddThreeByte(k).GetChars(), new byte[byteLen2]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddThreeByte(j).AddTwoByte(k).GetChars(), new byte[byteLen3]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddThreeByte(i).AddOneByte(j).AddTwoByte(k).GetChars(), new byte[byteLen1]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddThreeByte(j).AddTwoByte(k).GetChars(), new byte[byteLen2]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddTwoByte(j).AddThreeByte(k).GetChars(), new byte[byteLen3]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddSurrogateByte(i).AddOneByte(j).AddTwoByte(k).GetChars(), new byte[byteLen1]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddSurrogateByte(j).AddTwoByte(k).GetChars(), new byte[byteLen2]);
                PrepareAndRunTestCase(new CharArrayGenerator(len).AddOneByte(i).AddTwoByte(j).AddSurrogateByte(k).GetChars(), new byte[byteLen3]);
            }
        }

    }
}

