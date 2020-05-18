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

import javax.crypto.*;
import java.security.*;
import java.security.spec.AlgorithmParameterSpec;

/**
 * SM2 cipher implementation. Supports SM2 en/decryption and signing/verifying
 *
 * Objects should be instantiated by calling Cipher.getInstance() using the
 * following algorithm names:
 * "SM2/NoPadding" for SM2 Nopadding.
 *
 * Note: SM2 keys should be 128 bits long
 */
public final class SM2Cipher extends CipherSpi {

    // mode constant for public key encryption
    private final static int MODE_ENCRYPT = 1;
    // mode constant for private key decryption
    private final static int MODE_DECRYPT = 2;
    // mode constant for private key encryption (signing)
    private final static int MODE_SIGN    = 3;
    // mode constant for public key decryption (verifying)
    private final static int MODE_VERIFY  = 4;

    // constant for raw SM2
    private final static String PAD_NONE  = "NoPadding";
    private static final int OUTPUT_BUFFER_LENTH_INC = 1024;
    // current mode, one of MODE_* above. Set when init() is called
    private int mode;

    private final static int CIPHER_MODE_C1C3C2_ASN1 = 0;
    private final static int CIPHER_MODE_C1C3C2 = 1;
    private final static int CIPHER_MODE_C1C2C3_ASN1 = 2;
    private final static int CIPHER_MODE_C1C2C3 = 3;

    private byte[] buffer;
    // offset into the buffer (number of bytes buffered)
    private int bufOfs;

    // size of the output
    private int outputSize;

    // the public key, if we were initialized using a public key
    // private RSAPublicKey publicKey;
    private SM2PublicKey publicKey;
    // the private key, if we were initialized using a private key
    private SM2PrivateKey privateKey;

    private long handler;

    public SM2Cipher() {
    }


    // TODO: the mode is not used at present.
    protected void engineSetMode(String mode) throws NoSuchAlgorithmException {
        if (mode.equalsIgnoreCase("C1C3C2_ASN1")) {
        } else if (mode.equalsIgnoreCase("C1C3C2")) {
        } else if (mode.equalsIgnoreCase("C1C2C3_ASN1")) {
        } else if (mode.equalsIgnoreCase("C1C2C3")) {
        } else {
            throw new NoSuchAlgorithmException("Unsupported mode " + mode);
        }
    }

    // set the padding type
    protected void engineSetPadding(String paddingName)
            throws NoSuchPaddingException {
        if (paddingName.equalsIgnoreCase(PAD_NONE)) {
        } else {
            throw new NoSuchPaddingException
                    ("Padding " + paddingName + " not supported");
        }
    }

    // return 0 as block size, we are not a block cipher
    protected int engineGetBlockSize() {
        return 0;
    }

    // return the output size
    protected int engineGetOutputSize(int inputLen) {
        return outputSize;
    }

    // no iv, return null
    protected byte[] engineGetIV() {
        return null;
    }

    protected AlgorithmParameters engineGetParameters() {
        return null;
    }

    protected void engineInit(int opmode, Key key, SecureRandom random)
            throws InvalidKeyException {
        try {
            init(opmode, key, random, null);
        } catch (InvalidAlgorithmParameterException iape) {
            // never thrown when null parameters are used;
            // but re-throw it just in case
            InvalidKeyException ike =
                new InvalidKeyException("Wrong parameters");
            ike.initCause(iape);
            throw ike;
        }
    }

    protected void engineInit(int opmode, Key key,
            AlgorithmParameterSpec params, SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        init(opmode, key, random, params);
    }

    protected void engineInit(int opmode, Key key,
            AlgorithmParameters params, SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        if (params == null) {
            init(opmode, key, random, null);
        } else {
            throw new InvalidAlgorithmParameterException("Wrong parameter");
        }
    }

    // initialize this cipher
    private void init(int opmode, Key key, SecureRandom random,
            AlgorithmParameterSpec params)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        boolean encrypt;

        if (handler != 0) {
            // reinitialize, first free previous handler.
            SMUtils.getInstance().SM2FreeCtx(handler);
            handler = 0;
        }

        switch (opmode) {
        case Cipher.ENCRYPT_MODE:
        case Cipher.WRAP_MODE:
            encrypt = true;
            break;
        case Cipher.DECRYPT_MODE:
        case Cipher.UNWRAP_MODE:
            encrypt = false;
            break;
        default:
            throw new InvalidKeyException("Unknown mode: " + opmode);
        }
        if (key instanceof SM2PublicKey) {
            mode = encrypt ? MODE_ENCRYPT : MODE_VERIFY;
            publicKey = (SM2PublicKey)key;
            privateKey = null;
        } else {
            mode = encrypt ? MODE_SIGN : MODE_DECRYPT;
            privateKey = (SM2PrivateKey)key;
            publicKey = null;
        }

        bufOfs = 0;

        if (publicKey != null) {
            handler = SMUtils.getInstance().SM2InitCtxWithPubKey(publicKey.getKeyString());
        } else {
            handler = SMUtils.getInstance().SM2InitCtx();
        }

        if (handler == 0) {
          throw new InvalidAlgorithmParameterException("Fail to initialize SM2 Context");
        }
    }

    // internal update method
    private byte[] update(byte[] in, int inOfs, int inLen) {
        if ((inLen == 0) || (in == null)) {
            return null;
        }
        byte[] src = new byte[inLen];
        System.arraycopy(in, inOfs, src, 0, inLen);

        byte[] out = process(src);
        int len = out.length;

        if (buffer == null) {
            buffer = new byte[len + OUTPUT_BUFFER_LENTH_INC];
        } else if (bufOfs + len > buffer.length) {
            byte[] b = new byte[buffer.length + len + OUTPUT_BUFFER_LENTH_INC];
            System.arraycopy(buffer, 0, b, 0, bufOfs);
            buffer = b;
        }
        System.arraycopy(out, 0, buffer, bufOfs, len);
        bufOfs += len;
        outputSize += len;
        return out;
    }

    private byte[] process(byte[] in) {
        switch (mode) {
            case MODE_ENCRYPT:
                String strPub = new String(publicKey.getEncoded());
                byte[] sec = SMUtils.getInstance().SM2Encrypt(handler, in, strPub);
                return sec;
            case MODE_DECRYPT:
                String strPri = new String(privateKey.getEncoded());
                byte[] ret =  SMUtils.getInstance().SM2Decrypt(handler, in, strPri);
                return ret;
            default:
                throw new AssertionError("Internal error");
        }
    }

    // internal doFinal() method.
    private byte[] doFinal() throws BadPaddingException,
            IllegalBlockSizeException {
        try {
            byte[] output = new byte[outputSize];
            assert outputSize == bufOfs;
            System.arraycopy(buffer, 0, output, 0, outputSize);
            return output;
        } finally {
            bufOfs = 0;
            buffer = null;
            outputSize =0;
        }
    }

    protected byte[] engineUpdate(byte[] in, int inOfs, int inLen) {
        byte[] ret = update(in, inOfs, inLen);
        return ret;
    }

    protected int engineUpdate(byte[] in, int inOfs, int inLen, byte[] out,
            int outOfs) {
        byte[] ret = update(in, inOfs, inLen);
        return ret.length;
    }

    protected byte[] engineDoFinal(byte[] in, int inOfs, int inLen)
            throws BadPaddingException, IllegalBlockSizeException {
        if (in == null) {
            return doFinal();
        }
        byte[] result = update(in, inOfs, inLen);
        return result;
    }

    protected int engineDoFinal(byte[] in, int inOfs, int inLen, byte[] out,
            int outOfs) throws ShortBufferException, BadPaddingException,
            IllegalBlockSizeException {
        if (outputSize > out.length - outOfs) {
            throw new ShortBufferException
                ("Need " + outputSize + " bytes for output");
        }
        byte[] result = update(in, inOfs, inLen);
        int n = result.length;
        System.arraycopy(result, 0, out, outOfs, n);
        return n;
    }

    protected byte[] engineWrap(Key key) throws InvalidKeyException,
            IllegalBlockSizeException {
        byte[] encoded = key.getEncoded();
        if ((encoded == null) || (encoded.length == 0)) {
            throw new InvalidKeyException("Could not obtain encoded key");
        }
        update(encoded, 0, encoded.length);
        try {
            return doFinal();
        } catch (BadPaddingException e) {
            // should not occur
            throw new InvalidKeyException("Wrapping failed", e);
        }
    }

    protected Key engineUnwrap(byte[] wrappedKey, String algorithm,
            int type) throws InvalidKeyException, NoSuchAlgorithmException {
        Exception failover = null;
        byte[] encoded = null;

        update(wrappedKey, 0, wrappedKey.length);
        try {
            encoded = doFinal();
        } catch (BadPaddingException e) {
            throw new InvalidKeyException("Unwrapping failed", e);
        } catch (IllegalBlockSizeException e) {
            // should not occur, handled with length check above
            throw new InvalidKeyException("Unwrapping failed", e);
        }

        return ConstructKeys.constructKey(encoded, algorithm, type);
    }

    protected int engineGetKeySize(Key key) throws InvalidKeyException {
        byte[] keyBytes = key.getEncoded();
        if (keyBytes == null) {
            throw new InvalidKeyException("RAW key bytes missing");
        }
        return keyBytes.length;
    }

    @Override
    protected void finalize() throws Throwable {
        if (handler != 0) {
            SMUtils.getInstance().SM2FreeCtx(handler);
            handler = 0;
        }
        super.finalize();
    }
}
