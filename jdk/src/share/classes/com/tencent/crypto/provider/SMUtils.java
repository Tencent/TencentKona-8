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

package com.tencent.crypto.provider;

public class SMUtils {
    public static final String LIB_NAME = "TencentSM";
    private static boolean isLoadSuccess = false;
    private volatile static SMUtils mInstance;

    static {
        try {
            System.loadLibrary(LIB_NAME);
            isLoadSuccess = true;
        } catch (Exception e) {
            isLoadSuccess = false;
        }
    }

    private SMUtils() { }
    public static SMUtils getInstance() {
        if (mInstance == null) {
            synchronized (SMUtils.class) {
                if (mInstance == null) {
                    mInstance = new SMUtils();
                }
            }
        }
        return mInstance;
    }

    public static boolean isLoadOK() {
        return isLoadSuccess;
    }

    public native String version();
    /***********************************************SM2 ******************* */
    public native long    SM2InitCtx();
    public native long    SM2InitCtxWithPubKey(String strPubKey);
    public native void    SM2FreeCtx(long sm2Handler);
    /**
     * @return array[0] privatekey str
     *         array[1] publickey str
     */
    public native Object[]   SM2GenKeyPair(long sm2Handler);
    public native byte[]     SM2Encrypt(long sm2Handler, byte[] in, String strPubKey);
    public native byte[]     SM2Decrypt(long sm2Handler, byte[] in, String strPriKey);
    public native byte[]     SM2Sign(long sm2Handler, byte[] msg, byte[] id, String strPubKey, String strPriKey);
    public native int     SM2Verify(long sm2Handler, byte[] msg, byte[] id, String strPubKey, byte[] sig);

    /***********************************************SM3 ******************* */
    public native long    SM3Init();
    public native void    SM3Update(long sm3Handler, byte[] data);
    public native byte[]  SM3Final(long sm3Handler);
    public native void    SM3Free(long sm3Handler);
    /***********************************************SM4 ******************* */
    public native byte[]     SM4GenKey();

    public native byte[]     SM4CBCEncrypt(byte[] in, byte[] key, byte[] iv);
    public native byte[]     SM4CBCDecrypt(byte[] in, byte[] key, byte[] iv);
    public native byte[]     SM4CBCEncryptNoPadding(byte[] in, byte[] key, byte[] iv);
    public native byte[]     SM4CBCDecryptNoPadding(byte[] in, byte[] key, byte[] iv);
    public native byte[]     SM4ECBEncrypt(byte[] in, byte[] key);
    public native byte[]     SM4ECBDecrypt(byte[] in, byte[] key);
    public native byte[]     SM4ECBEncryptNoPadding(byte[] in, byte[] key);
    public native byte[]     SM4ECBDecryptNoPadding(byte[] in, byte[] key);

    public native byte[]     SM4CTREncrypt(byte[] in, byte[] key, byte[] iv);
    public native byte[]     SM4CTRDecrypt(byte[] in, byte[] key, byte[] iv);

    public native Object    SM4GCMEncrypt(byte[] in, byte[] key, byte[] iv, byte[] aad);
    public native byte[]     SM4GCMDecrypt(byte[] in, byte[] tag, int taglen, byte[] key, byte[] iv, byte[] aad);
    public native Object     SM4GCMEncryptNoPadding(byte[] in, byte[] key, byte[] iv, byte[] aad);
    public native byte[]     SM4GCMDecryptNoPadding(byte[] in, byte[] tag, int taglen, byte[] key, byte[] iv, byte[] aad);

 /**********************************************Random ***************** */
    public native int SMRandomReSeed(long handler, byte[] buf);
    public native byte[]     SMRandom(int len);
    public native byte[] SMRandom(long handler, int len);
    public native long SMInitRandomCtx();
    public native void SMFreeRandomCtx(long handler);
}

