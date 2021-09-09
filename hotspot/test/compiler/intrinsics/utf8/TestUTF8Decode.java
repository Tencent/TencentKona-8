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
 * @run main/othervm -Xint TestUTF8Decode
 * @run main/othervm -Xint TestUTF8Decode prefix1
 * @run main/othervm -Xint TestUTF8Decode prefix2
 *
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=0 -XX:-UseSSE42Intrinsics -XX:-TieredCompilation -Xcomp -XX:+PrintCompilation -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -XX:-TieredCompilation -Xcomp -XX:+PrintCompilation -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -XX:-TieredCompilation -Xcomp -XX:+PrintCompilation -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=0 -XX:-UseSSE42Intrinsics -XX:-TieredCompilation -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix1
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -XX:-TieredCompilation -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix1
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -XX:-TieredCompilation -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix1
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=0 -XX:-UseSSE42Intrinsics -XX:-TieredCompilation -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix2
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -XX:-TieredCompilation -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix2
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -XX:-TieredCompilation -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix2
 *
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=0 -XX:-UseSSE42Intrinsics -XX:TieredStopAtLevel=1 -Xcomp -XX:+PrintCompilation -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -XX:TieredStopAtLevel=1 -Xcomp -XX:+PrintCompilation -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -XX:TieredStopAtLevel=1 -XX:+PrintCompilation -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=0 -XX:-UseSSE42Intrinsics -XX:TieredStopAtLevel=1 -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix1
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -XX:TieredStopAtLevel=1 -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix1
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -XX:TieredStopAtLevel=1 -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix1
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=0 -XX:-UseSSE42Intrinsics -XX:TieredStopAtLevel=1 -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix2
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=1 -XX:TieredStopAtLevel=1 -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix2
 * @run main/othervm -XX:+IgnoreUnrecognizedVMOptions -XX:UseAVX=2 -XX:TieredStopAtLevel=1 -Xcomp -XX:CompileCommand=compileonly,*::decodeArrayLoop TestUTF8Decode prefix2
 * @summary test pass and failure case for UTF8 decoder
 */
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.MalformedInputException;
import java.util.Random;

public class TestUTF8Decode {
    static final CharsetDecoder decoder;
    static {
        decoder = Charset.forName("UTF-8").newDecoder();
    }
    static int prefix_length;
    static int decode_length;
    static byte[] prefix;
    static String utf16_prefix;
    static byte[] byte_prefix1 = {
            97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103,
            97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103,
            97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103,
            97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103,
            97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103,
            97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103};
    static String utf16_prefix1 = "abcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefg";

    // mixed code
    static byte[] byte_prefix2 = {
            (byte) 0xe4, (byte) 0xb8, (byte) 0x8a, (byte) 0xe6, (byte) 0xb5, (byte) 0xb7, (byte) 0xe6, (byte) 0xb5, (byte) 0xa6, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb0, (byte) 0xe5, (byte) 0x8c, (byte) 0xba, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb9, (byte) 0xe6, (byte) 0x98, (byte) 0x8e, (byte) 0xe7, (byte) 0x8f, (byte) 0xa0, (byte) 0xe9, (byte) 0x99, (byte) 0x86, (byte) 0xe5, (byte) 0xae, (byte) 0xb6, (byte) 0xe5, (byte) 0x98, (byte) 0xb4, (byte) 0xef, (byte) 0xbc, (byte) 0x8c, (byte) 0xe3, (byte) 0x80, (byte) 0x82, (byte) 0x70, (byte) 0x72, (byte) 0x6f, (byte) 0x67, (byte) 0x72, (byte) 0x61, (byte) 0x6d, (byte) 0x6d, (byte) 0x65, (byte) 0x72, (byte) 0x73, (byte) 0x2e, (byte) 0xce, (byte) 0xb1, (byte) 0xce, (byte) 0xb2, (byte) 0xce, (byte) 0xb3, (byte) 0xce, (byte) 0xb4, (byte) 0xce, (byte) 0xb5, (byte) 0xce, (byte) 0xb6, (byte) 0xce, (byte) 0xb7, (byte) 0xce, (byte) 0xb8, (byte) 0xce, (byte) 0xb9, (byte) 0xce, (byte) 0xba, (byte) 0xce, (byte) 0xbb, (byte) 0xce, (byte) 0xbc, (byte) 0xce, (byte) 0xbd, (byte) 0xce, (byte) 0xbe, (byte) 0xce, (byte) 0xbf, (byte) 0xcf, (byte) 0x80, (byte) 0xcf, (byte) 0x81, (byte) 0xcf, (byte) 0x83, (byte) 0xcf, (byte) 0x84, (byte) 0xcf, (byte) 0x85, (byte) 0xcf, (byte) 0x86, (byte) 0xcf, (byte) 0x87, (byte) 0xcf, (byte) 0x88, (byte) 0xcf, (byte) 0x89,
            (byte) 0xe4, (byte) 0xb8, (byte) 0x8a, (byte) 0xe6, (byte) 0xb5, (byte) 0xb7, (byte) 0xe6, (byte) 0xb5, (byte) 0xa6, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb0, (byte) 0xe5, (byte) 0x8c, (byte) 0xba, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb9, (byte) 0xe6, (byte) 0x98, (byte) 0x8e, (byte) 0xe7, (byte) 0x8f, (byte) 0xa0, (byte) 0xe9, (byte) 0x99, (byte) 0x86, (byte) 0xe5, (byte) 0xae, (byte) 0xb6, (byte) 0xe5, (byte) 0x98, (byte) 0xb4, (byte) 0xef, (byte) 0xbc, (byte) 0x8c, (byte) 0xe3, (byte) 0x80, (byte) 0x82, (byte) 0x70, (byte) 0x72, (byte) 0x6f, (byte) 0x67, (byte) 0x72, (byte) 0x61, (byte) 0x6d, (byte) 0x6d, (byte) 0x65, (byte) 0x72, (byte) 0x73, (byte) 0x2e, (byte) 0xce, (byte) 0xb1, (byte) 0xce, (byte) 0xb2, (byte) 0xce, (byte) 0xb3, (byte) 0xce, (byte) 0xb4, (byte) 0xce, (byte) 0xb5, (byte) 0xce, (byte) 0xb6, (byte) 0xce, (byte) 0xb7, (byte) 0xce, (byte) 0xb8, (byte) 0xce, (byte) 0xb9, (byte) 0xce, (byte) 0xba, (byte) 0xce, (byte) 0xbb, (byte) 0xce, (byte) 0xbc, (byte) 0xce, (byte) 0xbd, (byte) 0xce, (byte) 0xbe, (byte) 0xce, (byte) 0xbf, (byte) 0xcf, (byte) 0x80, (byte) 0xcf, (byte) 0x81, (byte) 0xcf, (byte) 0x83, (byte) 0xcf, (byte) 0x84, (byte) 0xcf, (byte) 0x85, (byte) 0xcf, (byte) 0x86, (byte) 0xcf, (byte) 0x87, (byte) 0xcf, (byte) 0x88, (byte) 0xcf, (byte) 0x89,
            (byte) 0xe4, (byte) 0xb8, (byte) 0x8a, (byte) 0xe6, (byte) 0xb5, (byte) 0xb7, (byte) 0xe6, (byte) 0xb5, (byte) 0xa6, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb0, (byte) 0xe5, (byte) 0x8c, (byte) 0xba, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb9, (byte) 0xe6, (byte) 0x98, (byte) 0x8e, (byte) 0xe7, (byte) 0x8f, (byte) 0xa0, (byte) 0xe9, (byte) 0x99, (byte) 0x86, (byte) 0xe5, (byte) 0xae, (byte) 0xb6, (byte) 0xe5, (byte) 0x98, (byte) 0xb4, (byte) 0xef, (byte) 0xbc, (byte) 0x8c, (byte) 0xe3, (byte) 0x80, (byte) 0x82, (byte) 0x70, (byte) 0x72, (byte) 0x6f, (byte) 0x67, (byte) 0x72, (byte) 0x61, (byte) 0x6d, (byte) 0x6d, (byte) 0x65, (byte) 0x72, (byte) 0x73, (byte) 0x2e, (byte) 0xce, (byte) 0xb1, (byte) 0xce, (byte) 0xb2, (byte) 0xce, (byte) 0xb3, (byte) 0xce, (byte) 0xb4, (byte) 0xce, (byte) 0xb5, (byte) 0xce, (byte) 0xb6, (byte) 0xce, (byte) 0xb7, (byte) 0xce, (byte) 0xb8, (byte) 0xce, (byte) 0xb9, (byte) 0xce, (byte) 0xba, (byte) 0xce, (byte) 0xbb, (byte) 0xce, (byte) 0xbc, (byte) 0xce, (byte) 0xbd, (byte) 0xce, (byte) 0xbe, (byte) 0xce, (byte) 0xbf, (byte) 0xcf, (byte) 0x80, (byte) 0xcf, (byte) 0x81, (byte) 0xcf, (byte) 0x83, (byte) 0xcf, (byte) 0x84, (byte) 0xcf, (byte) 0x85, (byte) 0xcf, (byte) 0x86, (byte) 0xcf, (byte) 0x87, (byte) 0xcf, (byte) 0x88, (byte) 0xcf, (byte) 0x89,
            (byte) 0xe4, (byte) 0xb8, (byte) 0x8a, (byte) 0xe6, (byte) 0xb5, (byte) 0xb7, (byte) 0xe6, (byte) 0xb5, (byte) 0xa6, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb0, (byte) 0xe5, (byte) 0x8c, (byte) 0xba, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb9, (byte) 0xe6, (byte) 0x98, (byte) 0x8e, (byte) 0xe7, (byte) 0x8f, (byte) 0xa0, (byte) 0xe9, (byte) 0x99, (byte) 0x86, (byte) 0xe5, (byte) 0xae, (byte) 0xb6, (byte) 0xe5, (byte) 0x98, (byte) 0xb4, (byte) 0xef, (byte) 0xbc, (byte) 0x8c, (byte) 0xe3, (byte) 0x80, (byte) 0x82, (byte) 0x70, (byte) 0x72, (byte) 0x6f, (byte) 0x67, (byte) 0x72, (byte) 0x61, (byte) 0x6d, (byte) 0x6d, (byte) 0x65, (byte) 0x72, (byte) 0x73, (byte) 0x2e, (byte) 0xce, (byte) 0xb1, (byte) 0xce, (byte) 0xb2, (byte) 0xce, (byte) 0xb3, (byte) 0xce, (byte) 0xb4, (byte) 0xce, (byte) 0xb5, (byte) 0xce, (byte) 0xb6, (byte) 0xce, (byte) 0xb7, (byte) 0xce, (byte) 0xb8, (byte) 0xce, (byte) 0xb9, (byte) 0xce, (byte) 0xba, (byte) 0xce, (byte) 0xbb, (byte) 0xce, (byte) 0xbc, (byte) 0xce, (byte) 0xbd, (byte) 0xce, (byte) 0xbe, (byte) 0xce, (byte) 0xbf, (byte) 0xcf, (byte) 0x80, (byte) 0xcf, (byte) 0x81, (byte) 0xcf, (byte) 0x83, (byte) 0xcf, (byte) 0x84, (byte) 0xcf, (byte) 0x85, (byte) 0xcf, (byte) 0x86, (byte) 0xcf, (byte) 0x87, (byte) 0xcf, (byte) 0x88, (byte) 0xcf, (byte) 0x89,
            (byte) 0xe4, (byte) 0xb8, (byte) 0x8a, (byte) 0xe6, (byte) 0xb5, (byte) 0xb7, (byte) 0xe6, (byte) 0xb5, (byte) 0xa6, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb0, (byte) 0xe5, (byte) 0x8c, (byte) 0xba, (byte) 0xe4, (byte) 0xb8, (byte) 0x9c, (byte) 0xe6, (byte) 0x96, (byte) 0xb9, (byte) 0xe6, (byte) 0x98, (byte) 0x8e, (byte) 0xe7, (byte) 0x8f, (byte) 0xa0, (byte) 0xe9, (byte) 0x99, (byte) 0x86, (byte) 0xe5, (byte) 0xae, (byte) 0xb6, (byte) 0xe5, (byte) 0x98, (byte) 0xb4, (byte) 0xef, (byte) 0xbc, (byte) 0x8c, (byte) 0xe3, (byte) 0x80, (byte) 0x82, (byte) 0x70, (byte) 0x72, (byte) 0x6f, (byte) 0x67, (byte) 0x72, (byte) 0x61, (byte) 0x6d, (byte) 0x6d, (byte) 0x65, (byte) 0x72, (byte) 0x73, (byte) 0x2e, (byte) 0xce, (byte) 0xb1, (byte) 0xce, (byte) 0xb2, (byte) 0xce, (byte) 0xb3, (byte) 0xce, (byte) 0xb4, (byte) 0xce, (byte) 0xb5, (byte) 0xce, (byte) 0xb6, (byte) 0xce, (byte) 0xb7, (byte) 0xce, (byte) 0xb8, (byte) 0xce, (byte) 0xb9, (byte) 0xce, (byte) 0xba, (byte) 0xce, (byte) 0xbb, (byte) 0xce, (byte) 0xbc, (byte) 0xce, (byte) 0xbd, (byte) 0xce, (byte) 0xbe, (byte) 0xce, (byte) 0xbf, (byte) 0xcf, (byte) 0x80, (byte) 0xcf, (byte) 0x81, (byte) 0xcf, (byte) 0x83, (byte) 0xcf, (byte) 0x84, (byte) 0xcf, (byte) 0x85, (byte) 0xcf, (byte) 0x86, (byte) 0xcf, (byte) 0x87, (byte) 0xcf, (byte) 0x88, (byte) 0xcf, (byte) 0x89
    };
    static String utf16_prefix2 = "\u4e0a\u6d77\u6d66\u4e1c\u65b0\u533a\u4e1c\u65b9\u660e\u73e0\u9646\u5bb6\u5634\uff0c\u3002\u0070\u0072\u006f\u0067\u0072\u0061\u006d\u006d\u0065\u0072\u0073\u002e\u03b1\u03b2\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9\u4e0a\u6d77\u6d66\u4e1c\u65b0\u533a\u4e1c\u65b9\u660e\u73e0\u9646\u5bb6\u5634\uff0c\u3002\u0070\u0072\u006f\u0067\u0072\u0061\u006d\u006d\u0065\u0072\u0073\u002e\u03b1\u03b2\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9\u4e0a\u6d77\u6d66\u4e1c\u65b0\u533a\u4e1c\u65b9\u660e\u73e0\u9646\u5bb6\u5634\uff0c\u3002\u0070\u0072\u006f\u0067\u0072\u0061\u006d\u006d\u0065\u0072\u0073\u002e\u03b1\u03b2\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9\u4e0a\u6d77\u6d66\u4e1c\u65b0\u533a\u4e1c\u65b9\u660e\u73e0\u9646\u5bb6\u5634\uff0c\u3002\u0070\u0072\u006f\u0067\u0072\u0061\u006d\u006d\u0065\u0072\u0073\u002e\u03b1\u03b2\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9\u4e0a\u6d77\u6d66\u4e1c\u65b0\u533a\u4e1c\u65b9\u660e\u73e0\u9646\u5bb6\u5634\uff0c\u3002\u0070\u0072\u006f\u0067\u0072\u0061\u006d\u006d\u0065\u0072\u0073\u002e\u03b1\u03b2\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9";

    static int[] prefix2_valid_index = {0, 0, 0, 3, 0, 0, 6, 0, 0, 9, 0, 0, 12, 0, 0, 15, 0, 0, 18, 0, 0, 21, 0, 0, 24, 0, 0, 27, 0, 0, 30, 0, 0, 33, 0, 0, 36, 0, 0, 39, 0, 0, 42, 0, 0, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 0, 59, 0, 61, 0, 63, 0, 65, 0, 67, 0, 69, 0, 71, 0, 73, 0, 75, 0, 77, 0, 79, 0, 81, 0, 83, 0, 85, 0, 87, 0, 89, 0, 91, 0, 93, 0, 95, 0, 97, 0, 99, 0, 101, 0, 103, 0, 105, 0, 0, 108, 0, 0, 111, 0, 0, 114, 0, 0, 117, 0, 0, 120, 0, 0, 123, 0, 0, 126, 0, 0, 129, 0, 0, 132, 0, 0, 135, 0, 0, 138, 0, 0, 141, 0, 0, 144, 0, 0, 147, 0, 0, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 0, 164, 0, 166, 0, 168, 0, 170, 0, 172, 0, 174, 0, 176, 0, 178, 0, 180, 0, 182, 0, 184, 0, 186, 0, 188, 0, 190, 0, 192, 0, 194, 0, 196, 0, 198, 0, 200, 0, 202, 0, 204, 0, 206, 0, 208, 0, 210, 0, 0, 213, 0, 0, 216, 0, 0, 219, 0, 0, 222, 0, 0, 225, 0, 0, 228, 0, 0, 231, 0, 0, 234, 0, 0, 237, 0, 0, 240, 0, 0, 243, 0, 0, 246, 0, 0, 249, 0, 0, 252, 0, 0, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 0, 269, 0, 271, 0, 273, 0, 275, 0, 277, 0, 279, 0, 281, 0, 283, 0, 285, 0, 287, 0, 289, 0, 291, 0, 293, 0, 295, 0, 297, 0, 299, 0, 301, 0, 303, 0, 305, 0, 307, 0, 309, 0, 311, 0, 313, 0, 315, 0, 0, 318, 0, 0, 321, 0, 0, 324, 0, 0, 327, 0, 0, 330, 0, 0, 333, 0, 0, 336, 0, 0, 339, 0, 0, 342, 0, 0, 345, 0, 0, 348, 0, 0, 351, 0, 0, 354, 0, 0, 357, 0, 0, 360, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 0, 374, 0, 376, 0, 378, 0, 380, 0, 382, 0, 384, 0, 386, 0, 388, 0, 390, 0, 392, 0, 394, 0, 396, 0, 398, 0, 400, 0, 402, 0, 404, 0, 406, 0, 408, 0, 410, 0, 412, 0, 414, 0, 416, 0, 418, 0, 420, 0, 0, 423, 0, 0, 426, 0, 0, 429, 0, 0, 432, 0, 0, 435, 0, 0, 438, 0, 0, 441, 0, 0, 444, 0, 0, 447, 0, 0, 450, 0, 0, 453, 0, 0, 456, 0, 0, 459, 0, 0, 462, 0, 0, 465, 466, 467, 468, 469, 470, 471, 472, 473, 474, 475, 476, 477, 0, 479, 0, 481, 0, 483, 0, 485, 0, 487, 0, 489, 0, 491, 0, 493, 0, 495, 0, 497, 0, 499, 0, 501, 0, 503, 0, 505, 0, 507, 0, 509, 0, 511, 0, 513, 0, 515, 0, 517, 0, 519, 0, 521, 0, 523, 0, 525};
    static int[] prefix2_output_len = {0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6, 0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12, 0, 0, 13, 0, 0, 14, 0, 0, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 0, 28, 0, 29, 0, 30, 0, 31, 0, 32, 0, 33, 0, 34, 0, 35, 0, 36, 0, 37, 0, 38, 0, 39, 0, 40, 0, 41, 0, 42, 0, 43, 0, 44, 0, 45, 0, 46, 0, 47, 0, 48, 0, 49, 0, 50, 0, 51, 0, 0, 52, 0, 0, 53, 0, 0, 54, 0, 0, 55, 0, 0, 56, 0, 0, 57, 0, 0, 58, 0, 0, 59, 0, 0, 60, 0, 0, 61, 0, 0, 62, 0, 0, 63, 0, 0, 64, 0, 0, 65, 0, 0, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 0, 79, 0, 80, 0, 81, 0, 82, 0, 83, 0, 84, 0, 85, 0, 86, 0, 87, 0, 88, 0, 89, 0, 90, 0, 91, 0, 92, 0, 93, 0, 94, 0, 95, 0, 96, 0, 97, 0, 98, 0, 99, 0, 100, 0, 101, 0, 102, 0, 0, 103, 0, 0, 104, 0, 0, 105, 0, 0, 106, 0, 0, 107, 0, 0, 108, 0, 0, 109, 0, 0, 110, 0, 0, 111, 0, 0, 112, 0, 0, 113, 0, 0, 114, 0, 0, 115, 0, 0, 116, 0, 0, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 0, 130, 0, 131, 0, 132, 0, 133, 0, 134, 0, 135, 0, 136, 0, 137, 0, 138, 0, 139, 0, 140, 0, 141, 0, 142, 0, 143, 0, 144, 0, 145, 0, 146, 0, 147, 0, 148, 0, 149, 0, 150, 0, 151, 0, 152, 0, 153, 0, 0, 154, 0, 0, 155, 0, 0, 156, 0, 0, 157, 0, 0, 158, 0, 0, 159, 0, 0, 160, 0, 0, 161, 0, 0, 162, 0, 0, 163, 0, 0, 164, 0, 0, 165, 0, 0, 166, 0, 0, 167, 0, 0, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 0, 181, 0, 182, 0, 183, 0, 184, 0, 185, 0, 186, 0, 187, 0, 188, 0, 189, 0, 190, 0, 191, 0, 192, 0, 193, 0, 194, 0, 195, 0, 196, 0, 197, 0, 198, 0, 199, 0, 200, 0, 201, 0, 202, 0, 203, 0, 204, 0, 0, 205, 0, 0, 206, 0, 0, 207, 0, 0, 208, 0, 0, 209, 0, 0, 210, 0, 0, 211, 0, 0, 212, 0, 0, 213, 0, 0, 214, 0, 0, 215, 0, 0, 216, 0, 0, 217, 0, 0, 218, 0, 0, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 0, 232, 0, 233, 0, 234, 0, 235, 0, 236, 0, 237, 0, 238, 0, 239, 0, 240, 0, 241, 0, 242, 0, 243, 0, 244, 0, 245, 0, 246, 0, 247, 0, 248, 0, 249, 0, 250, 0, 251, 0, 252, 0, 253, 0, 254, 0, 255};
    static byte[] update_utf8_prefix(byte[] utf8_array) {
        if (prefix != null) {
            byte[] result = new byte[utf8_array.length + prefix_length];
            System.arraycopy(prefix, 0, result, 0, prefix_length);
            System.arraycopy(utf8_array, 0, result, prefix_length, utf8_array.length);
            return result;
        }
        return utf8_array;
    }

    static String check_and_clear(String s) throws Exception {
        if (prefix != null) {
            String pre = s.substring(0, decode_length);
            if (!pre.equals(utf16_prefix)) {
                Exception e = new RuntimeException("perfix fail");
                throw e;
            }
            String res = s.substring(decode_length);
            return res;
        }
        return s;
    }

    static void TestEmpty() throws Exception {
        byte[] utf8_array = {97, 98, 99, 100, 101, 102, 103};
        String s =  decoder.decode(ByteBuffer.wrap(utf8_array, 3, 0)).toString();
        if (!s.equals("")) {
            throw new RuntimeException("TestEmpty fail");
        } else {
            System.out.println("TestEmpty pass");
        }
    }

    static void TestASCIIOnly() throws Exception {
        byte[] utf8_array = {97, 98, 99, 100, 101, 102, 103};
        utf8_array = update_utf8_prefix(utf8_array);
        String s =  decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        s = check_and_clear(s);
        if (!s.equals("abcdefg")) {
            System.out.println("s is :" + s);
            throw new RuntimeException("TestASCIIOnly fail");
        } else {
            System.out.println("TestASCIIOnly pass");
        }
    }

    static void TestASCIILong() throws Exception {
        byte[] utf8_array = {97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103,
                97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103, 97, 98, 99, 100, 101, 102, 103};
        utf8_array = update_utf8_prefix(utf8_array);
        String s =  decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        s = check_and_clear(s);
        if (!s.equals("abcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefg")) {
            System.out.println("s is :" + s);
            throw new RuntimeException("TestASCIILong fail");
        } else {
            System.out.println("TestASCIILong pass");
        }
    }

    static void TestValidTwoBytesOnly() throws Exception {
        byte[] utf8_array = {(byte)0xce, (byte)0xb1, (byte)0xce, (byte)0xb2, (byte)0xce, (byte)0xb3, (byte)0xc2, (byte)0xb2};
        utf8_array = update_utf8_prefix(utf8_array);
        String s =  decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        s = check_and_clear(s);
        if (!s.equals("\u03b1\u03b2\u03b3\u00b2")) {
            System.out.println("s is :" + s);
            throw new RuntimeException("TestValidTwoBytesOnly fail");
        } else {
            System.out.println("TestValidTwoBytesOnly pass");
        }
    }

    static void TestTwoBytesLostByte() throws Exception {
        byte[] utf8_array = {(byte)0xce, (byte)0xb1, (byte)0xce, (byte)0xb2, (byte)0xce, (byte)0xb3, (byte)0xc2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
             decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestTwoBytesLostByte pass");
                return;
            } else {
                System.out.println("TestTwoBytesLostByte wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestTwoBytesLostByte fail");
    }

    static void TestTwoBytesGotAscii() throws Exception {
        byte[] utf8_array = {(byte)0xce, (byte)0xb1, (byte)0xc1, (byte)0xb2, (byte)0xce, (byte)0xb3, (byte)0xc2, (byte)0xb2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestTwoBytesGotAscii pass");
                return;
            } else {
                System.out.println("TestTwoBytesGotAscii wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestTwoBytesGotAscii fail");
    }

    static void TestTwoBytesNotContinue1() throws Exception {
        byte[] utf8_array = {(byte)0xce, (byte)0xb1, (byte)0xce, (byte)0x12, (byte)0xce, (byte)0xb3, (byte)0xc2, (byte)0xb2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestTwoBytesNotContinue1 pass");
                return;
            } else {
                System.out.println("TestTwoBytesNotContinue1 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestTwoBytesNotContinue1 fail");
    }

    static void TestTwoBytesNotContinue2() throws Exception {
        byte[] utf8_array = {(byte)0xce, (byte)0xb1, (byte)0xce, (byte)0xc2, (byte)0xce, (byte)0xb3, (byte)0xc2, (byte)0xb2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestTwoBytesNotContinue2 pass");
                return;
            } else {
                System.out.println("TestTwoBytesNotContinue2 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestTwoBytesNotContinue2 fail");
    }

    static void TestTwoBytesNotContinue3() throws Exception {
        byte[] utf8_array = {(byte)0xce, (byte)0xb1, (byte)0xce, (byte)0xe2, (byte)0xce, (byte)0xb3, (byte)0xc2, (byte)0xb2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestTwoBytesNotContinue3 pass");
                return;
            } else {
                System.out.println("TestTwoBytesNotContinue3 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestTwoBytesNotContinue3 fail");
    }

    static void TestValidThreeBytesOnly() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE6, (byte) 0xB5,
                (byte) 0x81, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5, (byte) 0xB0, (byte) 0xBA};
        utf8_array = update_utf8_prefix(utf8_array);
        String s = decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        s = check_and_clear(s);
        if (!s.equals("\u98de\u6d41\u76f4\u4e0b\u4e09\u5343\u5c3a")) {
            System.out.println("s is :" + s);
            throw new RuntimeException("TestValidThreeBytesOnly fail");
        } else {
            System.out.println("TestValidThreeBytesOnly pass");
        }
    }

    // min value D800
    static void TestThreeBytes11Surrogate1() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE6, (byte) 0xB5,
                (byte) 0x81, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5, (byte) 0xB0, (byte) 0xBA,
                (byte) 0xED, (byte) 0xA0, (byte) 0x80};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 3) {
                System.out.println("TestThreeBytes11Surrogate1 pass");
                return;
            } else {
                System.out.println("TestThreeBytes11Surrogate1 wrong length expected 3 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestThreeBytes11Surrogate1 fail");
    }

    // max value DFFF
    static void TestThreeBytes11Surrogate2() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE6, (byte) 0xB5,
                (byte) 0x81, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5, (byte) 0xB0, (byte) 0xBA,
                (byte) 0xED, (byte) 0xBF, (byte) 0xBF};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 3) {
                System.out.println("TestThreeBytes11Surrogate2 pass");
                return;
            } else {
                System.out.println("TestThreeBytes11Surrogate2 wrong length expected 3 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestThreeBytes11Surrogate2 fail");
    }

    // middle value D800 DFFF  DC23
    static void TestThreeBytes11Surrogate3() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE6, (byte) 0xB5,
                (byte) 0x81, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5, (byte) 0xB0, (byte) 0xBA,
                (byte) 0xED, (byte) 0xB0, (byte) 0xA3};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 3) {
                System.out.println("TestThreeBytes11Surrogate3 pass");
                return;
            } else {
                System.out.println("TestThreeBytes11Surrogate3 wrong length expected 3 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestThreeBytes11Surrogate3 fail");
    }

    static void TestThreeBytes11Bits1() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE0, (byte) 0x85,
                (byte) 0x81, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5, (byte) 0xB0, (byte) 0xBA};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestThreeBytes11Bits1 pass");
                return;
            } else {
                System.out.println("TestThreeBytes11Bits1 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestThreeBytes11Bits1 fail");
    }

    static void TestThreeBytesLostBytes1() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE6, (byte) 0xB5,
                (byte) 0x81, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestThreeBytesLostBytes1 pass");
                return;
            } else {
                System.out.println("TestThreeBytesLostBytes1 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestThreeBytesLostBytes1 fail");
    }

    static void TestThreeBytesLostBytes2() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE6, (byte) 0xB5,
                (byte) 0x81, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5, (byte) 0xB0};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 2) {
                System.out.println("TestThreeBytesLostBytes2 pass");
                return;
            } else {
                System.out.println("TestThreeBytesLostBytes2 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestThreeBytesLostBytes2 fail");
    }

    static void TestThreeBytesNotCont1() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE6, (byte) 0xC5,
                (byte) 0x81, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5, (byte) 0xB0, (byte) 0xBA};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestThreeBytesNotCont1 pass");
                return;
            } else {
                System.out.println("TestThreeBytesNotCont1 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestThreeBytesNotCont1 fail");
    }

    static void TestThreeBytesNotCont2() throws Exception {
        byte[] utf8_array = {(byte) 0xE9, (byte) 0xA3, (byte) 0x9E, (byte) 0xE6, (byte) 0xB5,
                (byte) 0x01, (byte) 0xE7, (byte) 0x9B, (byte) 0xB4, (byte) 0xE4, (byte) 0xB8,
                (byte) 0x8B, (byte) 0xE4, (byte) 0xB8, (byte) 0x89, (byte) 0xE5, (byte) 0x8D,
                (byte) 0x83, (byte) 0xE5, (byte) 0xB0, (byte) 0xBA};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length));
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 2) {
                System.out.println("TestThreeBytesNotCont2 pass");
                return;
            } else {
                System.out.println("TestThreeBytesNotCont2 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestThreeBytesNotCont2 fail");
    }

    static void TestValidFourBytesOnly() throws Exception {
        byte[] utf8_array = {(byte)0xF0, (byte)0x90, (byte)0x84, (byte)0x82};
        utf8_array = update_utf8_prefix(utf8_array);
        String s =  decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        s = check_and_clear(s);
        if (!s.equals("\uD800\uDD02")) {
            System.out.println("s is :" + s);
            throw new RuntimeException("TestValidFourBytesOnly fail");
        } else {
            System.out.println("TestValidFourBytesOnly pass");
        }
    }

    static void TestValidMixed1() throws Exception {
        byte[] utf8_array = {(byte)0x78, (byte)0xE2, (byte)0x89, (byte)0xA4, (byte)0x28, (byte)0xCE, (byte)0xB1, (byte)0x2B, (byte)0xCE, (byte)0xB2, (byte)0x29, (byte)0xC2, (byte)0xB2, (byte)0xCE, (byte)0xB3, (byte)0xC2, (byte)0xB2};
        utf8_array = update_utf8_prefix(utf8_array);
        String s =  decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        s = check_and_clear(s);
        if (!s.equals("\u0078\u2264\u0028\u03b1\u002b\u03b2\u0029\u00b2\u03b3\u00b2")) {
            System.out.println("s is :" + s);
            throw new RuntimeException("TestValidMixed1 fail");
        } else {
            System.out.println("TestValidMixed1 pass");
        }
    }

    static void TestInvalidLeadingBit1() throws Exception {
        byte[] utf8_array = {(byte)0x78, (byte)0xE2, (byte)0x89, (byte)0xA4, (byte)0xA8, (byte)0xCE, (byte)0xB1, (byte)0x2B, (byte)0xCE, (byte)0xB2, (byte)0x29, (byte)0xC2, (byte)0xB2, (byte)0xCE, (byte)0xB3, (byte)0xC2, (byte)0xB2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestInvalidLeadingBit1 pass");
                return;
            } else {
                System.out.println("TestInvalidLeadingBit1 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestInvalidLeadingBit1 fail");
    }

    static void TestInvalidLeadingBit2() throws Exception {
        byte[] utf8_array = {(byte)0x78, (byte)0xE2, (byte)0x89, (byte)0xA4, (byte)0x28, (byte)0xFE, (byte)0xB1, (byte)0x2B, (byte)0xCE, (byte)0xB2, (byte)0x29, (byte)0xC2, (byte)0xB2, (byte)0xCE, (byte)0xB3, (byte)0xC2, (byte)0xB2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestInvalidLeadingBit2 pass");
                return;
            } else {
                System.out.println("TestInvalidLeadingBit2 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestInvalidLeadingBit2 fail");
    }

    static void TestInvalidLeadingBit3() throws Exception {
        byte[] utf8_array = {(byte)0x78, (byte)0xE2, (byte)0x89, (byte)0xA4, (byte)0x28, (byte)0xCE, (byte)0xB1, (byte)0xF0, (byte)0xCE, (byte)0xB2, (byte)0x29, (byte)0xC2, (byte)0xB2, (byte)0xCE, (byte)0xB3, (byte)0xC2, (byte)0xB2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestInvalidLeadingBit3 pass");
                return;
            } else {
                System.out.println("TestInvalidLeadingBit3 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestInvalidLeadingBit3 fail");
    }

    static void TestInvalidLeadingBit4() throws Exception {
        byte[] utf8_array = {(byte)0x78, (byte)0xE2, (byte)0x89, (byte)0xA4, (byte)0x28, (byte)0xCE, (byte)0xB1, (byte)0xF9, (byte)0xCE, (byte)0xB2, (byte)0x29, (byte)0xC2, (byte)0xB2, (byte)0xCE, (byte)0xB3, (byte)0xC2, (byte)0xB2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestInvalidLeadingBit4 pass");
                return;
            } else {
                System.out.println("TestInvalidLeadingBit4 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestInvalidLeadingBit4 fail");
    }

    static void TestInvalidLeadingBit5() throws Exception {
        byte[] utf8_array = {(byte)0x78, (byte)0xE2, (byte)0x89, (byte)0xA4, (byte)0x28, (byte)0xFF, (byte)0xB1, (byte)0x2B, (byte)0xCE, (byte)0xB2, (byte)0x29, (byte)0xC2, (byte)0xB2, (byte)0xCE, (byte)0xB3, (byte)0xC2, (byte)0xB2};
        utf8_array = update_utf8_prefix(utf8_array);
        try {
            decoder.decode(ByteBuffer.wrap(utf8_array, 0, utf8_array.length)).toString();
        } catch (MalformedInputException e) {
            System.out.println(e);
            if (e.getInputLength() == 1) {
                System.out.println("TestInvalidLeadingBit5 pass");
                return;
            } else {
                System.out.println("TestInvalidLeadingBit5 wrong length expected 1 but got " + e.getInputLength());
            }
        }
        throw new RuntimeException("TestInvalidLeadingBit5 fail");
    }
    public static void main(String[] args) throws Exception {
        if (args.length == 1) {
            if (args[0].equals("prefix1")) {
                prefix_length = (new Random()).nextInt(byte_prefix1.length - 1);
                prefix_length += 1;
                decode_length = prefix_length;
                prefix = byte_prefix1;
                utf16_prefix = utf16_prefix1.substring(0, prefix_length);
            } else if (args[0].equals("prefix2")) {
                prefix_length = (new Random()).nextInt(byte_prefix2.length);
                // fix prefix_length
                prefix_length += 1;
                for (int i = prefix_length; i < prefix2_valid_index.length; i++) {
                    if (prefix2_valid_index[i] != 0) {
                        decode_length = prefix2_output_len[i];
                        prefix_length = i;
                        break;
                    }
                }
                prefix = byte_prefix2;
                utf16_prefix = utf16_prefix2.substring(0, decode_length);
            }
            System.out.println(args[0] + " " + prefix_length);
        }
        TestEmpty();
        TestASCIIOnly();
        TestASCIILong();
        TestValidTwoBytesOnly();
        TestValidThreeBytesOnly();
        TestValidFourBytesOnly();
        TestValidMixed1();

        // two byte illegal
        // 1. not enough space
        TestTwoBytesLostByte();
        // 2. not continuation
        TestTwoBytesNotContinue1();
        TestTwoBytesNotContinue2();
        TestTwoBytesNotContinue3();
        // 3. encode as ascii
        TestTwoBytesGotAscii();

        // three bytes illegal
        // 1. not enough space
        TestThreeBytesLostBytes1();
        TestThreeBytesLostBytes2();
        // 2. not continuation
        TestThreeBytesNotCont1();
        TestThreeBytesNotCont2();
        // 3. encode in 11 bits
        TestThreeBytes11Bits1();
        // 4. SURROGATE UTF16
        TestThreeBytes11Surrogate1();
        TestThreeBytes11Surrogate2();
        TestThreeBytes11Surrogate3();

        // leading bits not support, for example
        // 1. b10 in first byte
        // 2. b111110, b1111110, b11111110
        // 3. 4 bytes mode: in long vector/ in short vector
        TestInvalidLeadingBit1();  // b10 first byte
        TestInvalidLeadingBit2();  // b11111110
        TestInvalidLeadingBit3();  // b111100
        TestInvalidLeadingBit4();  // b111110
        TestInvalidLeadingBit5();  // b11111111
    }
}
