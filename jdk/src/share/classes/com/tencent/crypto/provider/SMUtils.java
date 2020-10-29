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

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.rmi.server.ExportException;

public class SMUtils {
    public static final String LIB_NAME = "TencentSM";
    private static volatile boolean isLoadSuccess = false;
    private volatile static SMUtils mInstance;
    private static final String OS = System.getProperty("os.name").toLowerCase();
    private static final String PT = System.getProperty("os.arch").toLowerCase();

    static {
        loadNativeLibs();
    }

    private static String getFileFromJar(String name) throws Exception {
        InputStream in = SMUtils.class.getClassLoader().getResourceAsStream(name);
        byte[] buffer = new byte[16 *1024];
        int read  = -1;
        File temp = File.createTempFile(name,"");
        temp.deleteOnExit();
        FileOutputStream fos = new FileOutputStream(temp);
        while((read = in.read(buffer)) != -1) {
            fos.write(buffer, 0, read);
        }
        fos.flush();
        fos.close();
        in.close();
        return temp.getAbsolutePath();
    }

    private static void loadJarDll(String name) throws Exception {
        String nativeLibPath = getFileFromJar(name);
        System.load(nativeLibPath);
    }

    private static void loadNativeLibs() {
        if (isLoadSuccess) {
            return;
        }
        // first try to find native lib in jar
        String nPath = getNativeLibPath("lib" + LIB_NAME);
        if (nPath == null) {
            isLoadSuccess = false;
            return;
        }
        try {
            loadJarDll(nPath);
            isLoadSuccess = true;
        } catch (final Throwable error) {
            // try to load from system library path
            try {
                System.loadLibrary(LIB_NAME);
                isLoadSuccess = true;
            } catch (Exception e) {
                isLoadSuccess = false;
            }
        }
    }

    private static boolean isMac() {
        return (OS.indexOf("mac") >= 0 || OS.indexOf("Mac") >= 0);
    }

    private static boolean isUnix() {
        return (OS.indexOf("nix") >= 0 || OS.indexOf("nux") >= 0 || OS.indexOf("aix") >= 0);
    }

    private static boolean isWindows() {
        return (OS.indexOf("indows") >= 0);
    }

    private static boolean isAarch64() { return (PT.indexOf("aarch64") >= 0 || PT.indexOf("AARCH64") >= 0); }
    private static boolean isAmd64() { return (PT.indexOf("amd64") >= 0 || PT.indexOf("AMD64") >= 0); }
    private static boolean isX86_64() { return (PT.indexOf("x86_64") >= 0 || PT.indexOf("X86_64") >= 0); }

    static String getNativeLibPath(String libName) {
        final String parentFolder = "native_libs/";
        String suffix = "";
        String platform_name = "";
        if (libName.isEmpty()) {
            return null;
        }

        if (isAmd64() || isX86_64()) {
            if (isMac()) {
                platform_name = "macos_x86_64";
                suffix = ".dylib";
            } else if (isUnix()) {
                platform_name = "linux_x86_64";
                suffix = ".so";
            } else if (isWindows()) {
                platform_name = "windows_x86_64";
                suffix = ".dll";
            }
        } else if (isAarch64()) {
            // only support linux
            if (isUnix()) {
                platform_name = "linux_aarch64";
                suffix = ".so";
            }
        }

        if (suffix.isEmpty() && platform_name.isEmpty()) {
            return null;
        }

        return parentFolder + platform_name + "/" + libName + suffix;
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

