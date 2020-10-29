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
import javax.crypto.spec.IvParameterSpec;
import java.security.*;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidParameterSpecException;
import java.util.Arrays;
import java.util.Locale;

/**
 * This class represents the symmetric algorithms in its various modes
 * (<code>ECB</code>, <code>CBC</code>, <code>CTR</code>, and <code>GCM</code>) and
 * padding schemes (<code>PKCS7Padding</code>, <code>NoPadding</code>).
 *
 */

final class CipherCore {

    private static final int DEFAULT_TAG_LEN = 16;
    // must be multiplier of 16.
    private static final int DEF_OUTPUT_INC_SIZE = 1 << (4 + 4);
    /*
     * internal buffer
     */
    private byte[] buffer = null;

    /*
     * block size of cipher in bytes
     */
    private int blockSize;

    /*
     * unit size (number of input bytes that can be processed at a time)
     */
    private int unitBytes = 16;

    /*
     * index of the content size left in the buffer
     */
    private int bufferd = 0;

    /*
     * minimum number of bytes in the buffer required for
     * FeedbackCipher.encryptFinal()/decryptFinal() call.
     * update() must buffer this many bytes before starting
     * to encrypt/decrypt data.
     * currently, only the following cases have non-zero values:
     * 1) CTS mode - due to its special handling on the last two blocks
     * (the last one may be incomplete).
     * 2) GCM mode + decryption - due to its trailing tag bytes
     */
    private int minBytes = 0;

    /*
     * number of bytes needed to make the total input length a multiple
     * of the blocksize (this is used in feedback mode, when the number of
     * input bytes that are processed at a time is different from the block
     * size)
     */
    private int diffBlocksize = 0;

    /*
     * padding class
     */
    private SM4Padding padding;

    /*
     * the cipher mode
     */
    private int cipherMode = ECB_MODE;

    AlgoCipher cipher = null;
    /*
     * are we encrypting or decrypting?
     */
    private boolean decrypting = false;

    private static final int DEFAULT_IV_LEN=16;

    /*
     * Block Mode constants
     */
    static final int ECB_MODE = 0;
    static final int CBC_MODE = 1;
    static final int GCM_MODE = 2;
    static final int CTR_MODE = 3;

    /*
     * variables used for performing the GCM (key+iv) uniqueness check.
     * To use GCM mode safely, the cipher object must be re-initialized
     * with a different combination of key + iv values for each
     * encryption operation. However, checking all past key + iv values
     * isn't feasible. Thus, we only do a per-instance check of the
     * key + iv values used in previous encryption.
     * For decryption operations, no checking is necessary.
     * NOTE: this key+iv check have to be done inside CipherCore class
     * since CipherCore class buffers potential tag bytes in GCM mode
     * and may not call GaloisCounterMode when there isn't sufficient
     * input to process.
     */
    private boolean requireReinit = false;
    private byte[] lastEncKey = null;
    private byte[] lastEncIv = null;

    private static boolean updateFailWithIllegalBlockSize = false;
    private static boolean updateFailWithShortBuffer = false;

    SymmetricCipher rawImpl;


    enum SM4Padding {
        PKCS7Padding,
        NoPadding
    }

    /**
     * Creates an instance of CipherCore with default ECB mode and
     * PKCS7Padding.
     */
    CipherCore(SymmetricCipher impl, int blkSize) {
        blockSize = blkSize;
        unitBytes = blkSize;
        diffBlocksize = blkSize;
        rawImpl = impl;
        /*
         * The buffer should be usable for all cipher mode and padding
         * schemes.
         * In decryption mode, it also hold the possible padding block.
         */
        buffer = new byte[blockSize*2];

        // set mode and padding
        // cipher = new ElectronicCodeBook(impl);
        cipherMode = ECB_MODE;
        padding = (padding == null) ? SM4Padding.PKCS7Padding : padding;
        cipher = new EcbSM4Cipher(impl);
        cipher.setPadding(padding);
    }

    /**
     * Sets the mode of this cipher.
     *
     * @param mode the cipher mode
     *
     * @exception NoSuchAlgorithmException if the requested cipher mode does
     * not exist for this cipher
     */
    void setMode(String mode) throws NoSuchAlgorithmException {
        if (mode == null)
            throw new NoSuchAlgorithmException("null mode");

        String modeUpperCase = mode.toUpperCase(Locale.ENGLISH);

        if (modeUpperCase.equals("ECB")) {
            if (cipherMode == ECB_MODE) {
                return;
            } else {
                cipherMode = ECB_MODE;
                cipher = new EcbSM4Cipher(rawImpl);
            }
        }

        if (modeUpperCase.equals("CBC")) {
            cipherMode = CBC_MODE;
            cipher = new CbcSM4Cipher(rawImpl);
        } else if (modeUpperCase.equals("GCM")) {
            // can only be used for block ciphers w/ 128-bit block size
            if (blockSize != 16) {
                throw new NoSuchAlgorithmException
                        ("GCM mode can only be used for SM4 cipher with 128bit block size");
            }
            cipherMode = GCM_MODE;
            cipher = new GcmSM4Cipher(rawImpl);
        } else if (modeUpperCase.equals("CTR")) {
            cipherMode = CTR_MODE;
            padding = SM4Padding.NoPadding;
            cipher = new CtrSM4Cipher(rawImpl);
        }
        else {
            throw new NoSuchAlgorithmException("Cipher mode: " + mode
                                               + " not found");
        }
        cipher.setPadding(padding);
    }

    /**
     * Returns the mode of this cipher.
     *
     * @return the parsed cipher mode
     */
    int getMode() {
        return cipherMode;
    }

    /**
     * Sets the padding mechanism of this cipher.
     *
     * @param paddingScheme the padding mechanism
     *
     * @exception NoSuchPaddingException if the requested padding mechanism
     * does not exist
     */
    void setPadding(String paddingScheme)
        throws NoSuchPaddingException
    {
        if (paddingScheme == null) {
            throw new NoSuchPaddingException("null padding");
        }
        if (paddingScheme.equalsIgnoreCase("NOPADDING")) {
            padding = SM4Padding.NoPadding;
        } else if (!paddingScheme.equalsIgnoreCase("PKCS7PADDING")) {
            throw new NoSuchPaddingException("Padding: " + paddingScheme
                                             + " not implemented");
        } else {
            padding = SM4Padding.PKCS7Padding;
        }

        if ((padding != null) && (padding != SM4Padding.NoPadding) &&
            ((cipherMode == CTR_MODE))) {
            padding = SM4Padding.NoPadding;
            String modeStr = null;
            switch (cipherMode) {
            case CTR_MODE:
                modeStr = "CTR";
                break;
            default:
                // should never happen
            }
            if (modeStr != null) {
                throw new NoSuchPaddingException
                    (modeStr + " mode must be used with NoPadding");
            }
        }
        cipher.setPadding(padding);
    }

    /**
     * Returns the length in bytes that an output buffer would need to be in
     * order to hold the result of the next <code>update</code> or
     * <code>doFinal</code> operation, given the input length
     * <code>inputLen</code> (in bytes).
     *
     * <p>This call takes into account any unprocessed (buffered) data from a
     * previous <code>update</code> call, padding, and AEAD tagging.
     *
     * <p>The actual output length of the next <code>update</code> or
     * <code>doFinal</code> call may be smaller than the length returned by
     * this method.
     *
     * @param inputLen the input length (in bytes)
     *
     * @return the required output buffer size (in bytes)
     */
   int getOutputSize(int inputLen) {
       if(decrypting)
           return inputLen == 0 ? bufferd : Math.max(bufferd, inputLen) ;
       else
           return inputLen + DEF_OUTPUT_INC_SIZE;
    }

    /**
     * Returns the initialization vector (IV) in a new buffer.
     *
     * <p>This is useful in the case where a random IV has been created
     * (see <a href = "#init">init</a>),
     * or in the context of password-based encryption or
     * decryption, where the IV is derived from a user-provided password.
     *
     * @return the initialization vector in a new buffer, or null if the
     * underlying algorithm does not use an IV, or if the IV has not yet
     * been set.
     */
    byte[] getIV() {
       return cipher.getIV();
    }

    byte[] getTag() {
        return ((GcmSM4Cipher)cipher).getTag();
    }

    /**
     * Returns the parameters used with this cipher.
     *
     * <p>The returned parameters may be the same that were used to initialize
     * this cipher, or may contain the default set of parameters or a set of
     * randomly generated parameters used by the underlying cipher
     * implementation (provided that the underlying cipher implementation
     * uses a default set of parameters or creates new parameters if it needs
     * parameters but was not initialized with any).
     *
     * @return the parameters used with this cipher, or null if this cipher
     * does not use any parameters.
     */
    AlgorithmParameters getParameters(String algName) {
        if (cipherMode == ECB_MODE) {
            return null;
        }
        AlgorithmParameters params = null;
        AlgorithmParameterSpec spec;
        byte[] iv = getIV();
        if (iv == null) {
                iv = new byte[DEFAULT_IV_LEN];
                SMCSProvider.getRandom().nextBytes(iv);
        }
        if (cipherMode == GCM_MODE) {
            algName = "SM4GCM";
            byte[] tag = getTag();
            spec = new SM4GCMParameterSpec(((GcmSM4Cipher)cipher).getTagLen() * 8, iv, tag);
        } else {
               spec = new IvParameterSpec(iv);
        }
        try {
            params = AlgorithmParameters.getInstance("SM4", SMCSProvider.getInstance());
            params.init(spec);
        } catch (NoSuchAlgorithmException nsae) {
            // should never happen
            throw new RuntimeException("Cannot find " + algName +
                " AlgorithmParameters implementation in TSMC provider");
        } catch (InvalidParameterSpecException ipse) {
            // should never happen
            throw new RuntimeException(spec.getClass() + " not supported");
        }
        return params;
    }

    /**
     * Initializes this cipher with a key and a source of randomness.
     *
     * <p>The cipher is initialized for one of the following four operations:
     * encryption, decryption, key wrapping or key unwrapping, depending on
     * the value of <code>opmode</code>.
     *
     * <p>If this cipher requires an initialization vector (IV), it will get
     * it from <code>random</code>.
     * This behaviour should only be used in encryption or key wrapping
     * mode, however.
     * When initializing a cipher that requires an IV for decryption or
     * key unwrapping, the IV
     * (same IV that was used for encryption or key wrapping) must be provided
     * explicitly as a
     * parameter, in order to get the correct result.
     *
     * <p>This method also cleans existing buffer and other related state
     * information.
     *
     * @param opmode the operation mode of this cipher (this is one of
     * the following:
     * <code>ENCRYPT_MODE</code>, <code>DECRYPT_MODE</code>,
     * <code>WRAP_MODE</code> or <code>UNWRAP_MODE</code>)
     * @param key the secret key
     * @param random the source of randomness
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this cipher
     */
    void init(int opmode, Key key, SecureRandom random)
            throws InvalidKeyException {
        try {
            init(opmode, key, (AlgorithmParameterSpec)null, random);
        } catch (InvalidAlgorithmParameterException e) {
            throw new InvalidKeyException(e.getMessage());
        }
    }

    /**
     * Initializes this cipher with a key, a set of
     * algorithm parameters, and a source of randomness.
     *
     * <p>The cipher is initialized for one of the following four operations:
     * encryption, decryption, key wrapping or key unwrapping, depending on
     * the value of <code>opmode</code>.
     *
     * <p>If this cipher (including its underlying feedback or padding scheme)
     * requires any random bytes, it will get them from <code>random</code>.
     *
     * @param opmode the operation mode of this cipher (this is one of
     * the following:
     * <code>ENCRYPT_MODE</code>, <code>DECRYPT_MODE</code>,
     * <code>WRAP_MODE</code> or <code>UNWRAP_MODE</code>)
     * @param key the encryption key
     * @param params the algorithm parameters
     * @param random the source of randomness
     *
     * @exception InvalidKeyException if the given key is inappropriate for
     * initializing this cipher
     * @exception InvalidAlgorithmParameterException if the given algorithm
     * parameters are inappropriate for this cipher
     */
    void init(int opmode, Key key, AlgorithmParameterSpec params,
            SecureRandom random)
            throws InvalidKeyException, InvalidAlgorithmParameterException {
        decrypting = (opmode == Cipher.DECRYPT_MODE)
                  || (opmode == Cipher.UNWRAP_MODE);
        updateFailWithIllegalBlockSize = false;
        updateFailWithShortBuffer = false;

        byte[] keyBytes = getKeyBytes(key);
        // tag lenth in bytes
        int tagLen = -1;
        byte[] ivBytes = null;
        byte[] tag = null;
        if (params != null) {
            if (cipherMode == GCM_MODE) {
                if (params instanceof SM4GCMParameterSpec) {
                    if (decrypting) {
                        tag = ((SM4GCMParameterSpec) params).getTag();
                        if (tag == null) {
                            throw new InvalidAlgorithmParameterException("Must set Tag use SM4GCMParameterSpec, or getParameter after encrypt");
                        }
                        tagLen = tag.length;
                    }
                    ivBytes = ((SM4GCMParameterSpec)params).getIV();
                } else {
                    throw new InvalidAlgorithmParameterException
                            ("Unsupported parameter: " + params);
               }
            } else {
                if (params instanceof IvParameterSpec) {
                    ivBytes = ((IvParameterSpec)params).getIV();
                } else {
                    throw new InvalidAlgorithmParameterException
                        ("Unsupported parameter: " + params);
                }
            }
        }
        if (cipherMode == ECB_MODE) {
            if (ivBytes != null) {
                throw new InvalidAlgorithmParameterException
                                                ("ECB mode cannot use IV");
            }
        } else if (ivBytes == null)  {
            if (decrypting) {
                throw new InvalidAlgorithmParameterException("Parameters iv for decrypting missing"
                                                             + "must be set with SetParameter()");
            }

            if (random == null) {
                random = SMCSProvider.getRandom();
            }
            ivBytes = new byte[DEFAULT_IV_LEN];
            random.nextBytes(ivBytes);
        } else if (ivBytes.length < DEFAULT_IV_LEN) {
            byte[] tmp = new byte[DEFAULT_IV_LEN];
            System.arraycopy(ivBytes, 0, tmp, 0, ivBytes.length);
            ivBytes = tmp;
        }

        bufferd = 0;
        diffBlocksize = blockSize;

        String algorithm = key.getAlgorithm();

        // GCM mode needs additional handling
        if (cipherMode == GCM_MODE) {
            if(tagLen == -1) {
                tagLen = DEFAULT_TAG_LEN;
            }
            if (decrypting) {
                minBytes = tagLen;
            } else {
                // check key+iv for encryption in GCM mode
                requireReinit =
                    Arrays.equals(ivBytes, lastEncIv) &&
                    MessageDigest.isEqual(keyBytes, lastEncKey);
                if (requireReinit) {
                    throw new InvalidAlgorithmParameterException
                        ("Cannot reuse iv for GCM encryption");
                }
                lastEncIv = ivBytes;
                lastEncKey = keyBytes;
            }

            ((GcmSM4Cipher)cipher).init
                (decrypting, algorithm, keyBytes, ivBytes);

            if (decrypting) {
                ((GcmSM4Cipher) cipher).setTagLen(tagLen);
                ((GcmSM4Cipher) cipher).setTag(tag);
            }
        } else {
            cipher.init(decrypting, algorithm, keyBytes, ivBytes);
        }
        // skip checking key+iv from now on until after doFinal()
        requireReinit = false;
    }

    void init(int opmode, Key key, AlgorithmParameters params,
              SecureRandom random)
        throws InvalidKeyException, InvalidAlgorithmParameterException {
        AlgorithmParameterSpec spec = null;
        String paramType = null;
        if (params != null) {
            try {
                if (cipherMode == GCM_MODE) {
                    paramType = "SM4GCM";
                    spec = params.getParameterSpec(SM4GCMParameterSpec.class);
                } else {
                    paramType = "IV";
                    spec = params.getParameterSpec(IvParameterSpec.class);
                }
            } catch (InvalidParameterSpecException ipse) {
                throw new InvalidAlgorithmParameterException
                    ("Wrong parameter type: " + paramType + " expected");
            }
        }
        init(opmode, key, spec, random);
    }

    /**
     * Return the key bytes of the specified key. Throw an InvalidKeyException
     * if the key is not usable.
     */
    static byte[] getKeyBytes(Key key) throws InvalidKeyException {
        if (key == null) {
            throw new InvalidKeyException("No key given");
        }
        // note: key.getFormat() may return null
        if (!"RAW".equalsIgnoreCase(key.getFormat())) {
            throw new InvalidKeyException("Wrong format: RAW bytes needed");
        }
        byte[] keyBytes = key.getEncoded();
        if (keyBytes == null) {
            throw new InvalidKeyException("RAW key bytes missing");
        }
        return keyBytes;
    }

    /**
     * Continues a multiple-part encryption or decryption operation
     * (depending on how this cipher was initialized), processing another data
     * part.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, are processed, and the
     * result is stored in a new buffer.
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     *
     * @return the new buffer with the result
     *
     * @exception IllegalStateException if this cipher is in a wrong state
     * (e.g., has not been initialized)
     */
    byte[] update(byte[] input, int inputOffset, int inputLen) {
        checkReinit();
        byte[] output = null;
        try {
            output = new byte[getOutputSize(inputLen)];
            int len = update(input, inputOffset, inputLen, output,
                    0);
            if (len == output.length) {
                return output;
            } else {
                byte[] copy = Arrays.copyOf(output, len);
                if (decrypting) {
                    // Zero out internal buffer which is no longer required
                    Arrays.fill(output, (byte) 0x00);
                }
                return copy;
            }
        } catch (ShortBufferException e) {
            updateFailWithShortBuffer = true;
            // should never happen
            throw new ProviderException("Unexpected exception", e);
        }
    }

    /**
     * Continues a multiple-part encryption or decryption operation
     * (depending on how this cipher was initialized), processing another data
     * part.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, are processed, and the
     * result is stored in the <code>output</code> buffer, starting at
     * <code>outputOffset</code>.
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     * @param output the buffer for the result
     * @param outputOffset the offset in <code>output</code> where the result
     * is stored
     *
     * @return the number of bytes stored in <code>output</code>
     *
     * @exception ShortBufferException if the given output buffer is too small
     * to hold the result
     */
    int update(byte[] input, int inputOffset, int inputLen, byte[] output,
               int outputOffset) throws ShortBufferException {
        checkReinit();

        // figure out how much can be sent to crypto function
        int len = Math.addExact(bufferd, inputLen);

        // if padding, handle all inputLen buffer, otherwise SM4 can out calculate decrypting correctly.
        if (padding == SM4Padding.PKCS7Padding) {
            if (len != 0) {
                if ((input == output)
                        && (outputOffset - inputOffset < inputLen)
                        && (inputOffset - outputOffset < buffer.length)) {
                    // copy 'input' out to avoid its content being
                    // overwritten prematurely.
                    input = Arrays.copyOfRange(input, inputOffset,
                            Math.addExact(inputOffset, inputLen));
                    inputOffset = 0;
                }
                byte[] out = new byte[0];
                try {
                    out = encDecOutput(input, 0, inputLen);
                    if (out == null) {
                        out = new byte[0];
                    }
                    int outLen = out.length;
                    if (out != null) {
                        // check output buffer capacity only for encrypting mode.
                        // Since the encrypted data size may be larger than decrypted data.
                        // it is acceptable for shorter buffer length for output in decrypting mode.
                        if ((decrypting == false) && ((output == null) ||
                                ((output.length - outputOffset) < outLen))) {
                            throw new ShortBufferException("Output buffer must be "
                                    + "(at least) " + outLen
                                    + " bytes long");
                        }
                        outLen =  Math.min(outLen, output.length - outputOffset);
                        System.arraycopy(out, 0, output, outputOffset, outLen);
                    }
                    return outLen;
                }  catch (IllegalBlockSizeException e) {
                    updateFailWithIllegalBlockSize = true;
                    e.printStackTrace();
                    return 0;
                }
            } else {
                // nothing to do .
                return 0;
            }
        }

        len -= minBytes;

        // do not count the trailing bytes which do not make up a unit
        len = (len > 0 ? (len - (len % unitBytes)) : 0);

        // check output buffer capacity
        // only check at encrypting phase because the out buffer could be lower
        // delay the check of decrypting later
        if (!decrypting) {
            if ((output == null) ||
                    ((output.length - outputOffset) < len)) {
                throw new ShortBufferException("Output buffer must be "
                        + "(at least) " + len
                        + " bytes long");
            }
        } else {
            if (output == null) {
                throw new ShortBufferException("Output buffer must be (at least) " + len + " bytes long");
            }
        }

        byte[] out = null;
        int outLen = 0;

        if (len != 0) { // there is some work to do
            if ((input == output)
                    && (outputOffset - inputOffset < inputLen)
                    && (inputOffset - outputOffset < buffer.length)) {
                // copy 'input' out to avoid its content being
                // overwritten prematurely.
                input = Arrays.copyOfRange(input, inputOffset,
                        Math.addExact(inputOffset, inputLen));
                inputOffset = 0;
            }

            try {
                if (len <= bufferd) {
                    // all to-be-processed data are from 'buffer'
                    if (decrypting) {
                        out = encDecOutput(buffer, 0, len);
                        outLen = out.length;
                    } else {
                        out = encDecOutput(buffer, 0, len);
                        outLen = out.length;
                    }
                    bufferd -= len;
                    if (bufferd != 0) {
                        // reset buffer
                        System.arraycopy(buffer, len, buffer, 0, bufferd);
                    }
                    if (out != null) {
                        // check output buffer capacity only for encrypting mode.
                        // Since the encrypted data size may be larger than decrypted data.
                        // it is acceptable for shorter buffer length for output in decrypting mode.
                        if ((decrypting == false) && ((output == null) ||
                                ((output.length - outputOffset) < outLen))) {
                            throw new ShortBufferException("Output buffer must be "
                                    + "(at least) " + outLen
                                    + " bytes long");
                        }
                        outLen =  Math.min(outLen, output.length - outputOffset);
                        System.arraycopy(out, 0, output, outputOffset, outLen);
                    }
                } else { // len > buffered
                    int inputConsumed = len - bufferd;
                    int temp;
                    if (bufferd > 0) {
                        int bufferCapacity = buffer.length - bufferd;
                        if (bufferCapacity != 0) {
                            temp = Math.min(bufferCapacity, inputConsumed);
                            if (unitBytes != blockSize) {
                                temp -= (Math.addExact(bufferd, temp) % unitBytes);
                            }
                            System.arraycopy(input, inputOffset, buffer, bufferd, temp);
                            inputOffset = Math.addExact(inputOffset, temp);
                            inputConsumed -= temp;
                            inputLen -= temp;
                            bufferd = Math.addExact(bufferd, temp);
                        }
                        // process 'buffer'. When finished we can null out 'buffer'
                        // Only necessary to null out if buffer holds data for encryption
                        if (decrypting) {
                            out = encDecOutput(buffer, 0, bufferd);
                            outLen = out.length;
                        } else {
                            out = encDecOutput(buffer, 0, bufferd);
                            outLen = out.length;
                            Arrays.fill(buffer, (byte) 0x00);
                        }
                        bufferd = 0;
                        if (out != null) {
                            // check output buffer capacity only for encrypting mode.
                            // Since the encrypted data size may be larger than decrypted data.
                            // it is acceptable for shorter buffer length for output in decrypting mode.
                            if ((decrypting == false) && ((output == null) ||
                                    ((output.length - outputOffset) < outLen))) {
                                throw new ShortBufferException("Output buffer must be "
                                        + "(at least) " + outLen
                                        + " bytes long");
                            }
                            int outLength =  Math.min(out.length, output.length - outputOffset);
                            System.arraycopy(out, 0, output, outputOffset, outLength);
                            outputOffset = Math.addExact(outputOffset, outLen);
                        }
                    }
                    if (inputConsumed > 0) { // still has input to process
                        byte[] out1;
                        if (decrypting) {
                            out1 = encDecOutput(input, inputOffset, inputConsumed);
                            outLen += out1.length;
                        } else {
                            out1 = encDecOutput(input, inputOffset, inputConsumed);
                            outLen += out1.length;
                        }
                        inputOffset += inputConsumed;
                        inputLen -= inputConsumed;
                        if (out1 != null) {
                            // check output buffer capacity only for encrypting mode.
                            // Since the encrypted data size may be larger than decrypted data.
                            // it is acceptable for shorter buffer length for output in decrypting mode.
                            if ((decrypting == false) && ((output == null) ||
                                    ((output.length - outputOffset) < outLen))) {
                                throw new ShortBufferException("Output buffer must be "
                                        + "(at least) " + outLen
                                        + " bytes long");
                            }
                            if (out != null) {
                                int outLength =  Math.min(out.length, output.length - outputOffset);
                                System.arraycopy(out, 0, output, outputOffset, outLength);
                                int outLength1 = Math.min(out1.length, output.length - outputOffset - outLength);
                                System.arraycopy(out1, 0, output, outputOffset + outLength, outLength1);
                            } else {
                                int outLength =  Math.min(out1.length, output.length - outputOffset);
                                System.arraycopy(out1, 0, output, outputOffset, outLength);
                            }
                        }
                    }
                }
            } catch (IllegalBlockSizeException e) {
                updateFailWithIllegalBlockSize = true;
                e.printStackTrace();
                return 0;
            }
            // Let's keep track of how many bytes are needed to make
            // the total input length a multiple of blocksize when
            // padding is applied
            if (unitBytes != blockSize) {
                if (len < diffBlocksize) {
                    diffBlocksize -= len;
                } else {
                    diffBlocksize = blockSize -
                            ((len - diffBlocksize) % blockSize);
                }
            }
        }
        // Store remaining input into 'buffer' again
        if (inputLen > 0) {
            System.arraycopy(input, inputOffset, buffer, bufferd,
                    inputLen);
            bufferd = Math.addExact(bufferd, inputLen);
        }
        return outLen;
    }

    /**
     * Encrypts or decrypts data in a single-part operation,
     * or finishes a multiple-part operation.
     * The data is encrypted or decrypted, depending on how this cipher was
     * initialized.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, and any input bytes that
     * may have been buffered during a previous <code>update</code> operation,
     * are processed, with padding (if requested) being applied.
     * The result is stored in a new buffer.
     *
     * <p>The cipher is reset to its initial state (uninitialized) after this
     * call.
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     *
     * @return the new buffer with the result
     *
     * @exception IllegalBlockSizeException if this cipher is a block cipher,
     * no padding has been requested (only in encryption mode), and the total
     * input length of the data processed by this cipher is not a multiple of
     * block size
     * @exception BadPaddingException if this cipher is in decryption mode,
     * and (un)padding has been requested, but the decrypted data is not
     * bounded by the appropriate padding bytes
     */
    byte[] doFinal(byte[] input, int inputOffset, int inputLen)
        throws IllegalBlockSizeException {
        try {
            checkReinit();
            if (updateFailWithIllegalBlockSize) {
                throw new IllegalBlockSizeException("Illegal Block Size");
            }
            if (updateFailWithShortBuffer) {
                throw new ShortBufferException("Short Buffer Exception");
            }
            byte[] output = new byte[getOutputSize(inputLen)];
            byte[] finalBuf = prepareInputBuffer(input, inputOffset,
                    inputLen, output, 0);
            if (finalBuf == null || finalBuf.length == 0) {
                return new byte[0];
            }
            int finalOffset = (finalBuf == input) ? inputOffset : 0;
            int finalBufLen = (finalBuf == input) ? inputLen : finalBuf.length;

            byte[] ot = encDecOutput(finalBuf, finalOffset,finalBufLen);
            int outLen = ot.length;
            endDoFinal();
            if (outLen < output.length) {
                byte[] copy = Arrays.copyOf(ot, outLen);
                if (decrypting) {
                    // Zero out internal (ouput) array
                    Arrays.fill(output, (byte) 0x00);
                }
                return copy;
            } else {
                return ot;
            }
        } catch (ShortBufferException e) {
            // never thrown
            throw new ProviderException("Unexpected exception", e);
        }
    }

    /**
     * Encrypts or decrypts data in a single-part operation,
     * or finishes a multiple-part operation.
     * The data is encrypted or decrypted, depending on how this cipher was
     * initialized.
     *
     * <p>The first <code>inputLen</code> bytes in the <code>input</code>
     * buffer, starting at <code>inputOffset</code>, and any input bytes that
     * may have been buffered during a previous <code>update</code> operation,
     * are processed, with padding (if requested) being applied.
     * The result is stored in the <code>output</code> buffer, starting at
     * <code>outputOffset</code>.
     *
     * <p>The cipher is reset to its initial state (uninitialized) after this
     * call.
     *
     * @param input the input buffer
     * @param inputOffset the offset in <code>input</code> where the input
     * starts
     * @param inputLen the input length
     * @param output the buffer for the result
     * @param outputOffset the offset in <code>output</code> where the result
     * is stored
     *
     * @return the number of bytes stored in <code>output</code>
     *
     * @exception IllegalBlockSizeException if this cipher is a block cipher,
     * no padding has been requested (only in encryption mode), and the total
     * input length of the data processed by this cipher is not a multiple of
     * block size
     * @exception ShortBufferException if the given output buffer is too small
     * to hold the result
     * @exception BadPaddingException if this cipher is in decryption mode,
     * and (un)padding has been requested, but the decrypted data is not
     * bounded by the appropriate padding bytes
     */
    int doFinal(byte[] input, int inputOffset, int inputLen, byte[] output,
                int outputOffset)
        throws IllegalBlockSizeException, ShortBufferException {

        byte[] out = doFinal(input, inputOffset, inputLen);
        int len = Math.min(out.length, output.length - outputOffset);
        System.arraycopy(out, 0, output, outputOffset, len);
        return len;
    }

    private byte[] prepareInputBuffer(byte[] input, int inputOffset,
                                      int inputLen, byte[] output, int outputOffset)
            throws IllegalBlockSizeException, ShortBufferException {
        // calculate total input length
        int len = Math.addExact(bufferd, inputLen);
        if (len == 0) {
            // no work to do.
            return null;
        }

        if (padding == SM4Padding.NoPadding) {
            if (len % unitBytes != 0) {
                throw new IllegalBlockSizeException("Illegal Block Size");
            }
        }

        // calculate padding length
        int totalLen = Math.addExact(len, cipher.getBufferedLength());
        int paddingLen = 0;
        // will the total input length be a multiple of blockSize?
        if (unitBytes != blockSize) {
            if (totalLen < diffBlocksize) {
                paddingLen = diffBlocksize - totalLen;
            } else {
                paddingLen = blockSize -
                        ((totalLen - diffBlocksize) % blockSize);
            }
        }

        /*
         * prepare the final input, assemble a new buffer if any
         * of the following is true:
         *  - 'input' and 'output' are the same buffer
         *  - there are internally buffered bytes
         *  - doing encryption and padding is needed
         */
        if ((bufferd != 0) || (!decrypting) ||
                ((input == output)
                        && (outputOffset - inputOffset < inputLen)
                        && (inputOffset - outputOffset < buffer.length))) {
            byte[] finalBuf;
            if (decrypting || padding == null) {
                paddingLen = 0;
            }
            finalBuf = new byte[Math.addExact(len, paddingLen)];
            if (bufferd != 0) {
                System.arraycopy(buffer, 0, finalBuf, 0, bufferd);
                if (!decrypting) {
                    // done with input buffer. We should zero out the
                    // data if we're in encrypt mode.
                    Arrays.fill(buffer, (byte) 0x00);
                }
            }
            if (inputLen != 0) {
                System.arraycopy(input, inputOffset, finalBuf,
                        bufferd, inputLen);
            }
            return finalBuf;
        }
        return input;
    }


    private void endDoFinal() {
        bufferd = 0;
        diffBlocksize = blockSize;
        if (cipherMode != ECB_MODE) {
            cipher.reset();
        }
        updateFailWithShortBuffer = false;
        updateFailWithIllegalBlockSize = false;
    }


    private void checkReinit() {
        if (requireReinit) {
            throw new IllegalStateException
                ("Must use either different key or iv for GCM encryption");
        }
    }

    private byte[] encDecOutput(byte[] in, int inOfs, int len)
        throws IllegalBlockSizeException, ShortBufferException {
        byte[] out;
        if ((cipherMode != GCM_MODE) && (in == null || len == 0)) {
            throw new ShortBufferException("invalid input size");
        }
        if (
            (cipherMode != GCM_MODE) && (cipherMode != CTR_MODE) && ((len % unitBytes) != 0)) {
                if (padding == SM4Padding.NoPadding) {
                    throw new IllegalBlockSizeException
                        ("Input length with NoPadding not multiple of " +
                         unitBytes + " bytes");
                }
        }

        if (decrypting) {
            out = cipher.decryptFinal(in, inOfs, len);
        } else {
            out = cipher.encryptFinal(in, inOfs, len);
        }
        return out;
    }

    /**
     * Wrap a key.
     *
     * @param key the key to be wrapped.
     *
     * @return the wrapped key.
     *
     * @exception IllegalBlockSizeException if this cipher is a block
     * cipher, no padding has been requested, and the length of the
     * encoding of the key to be wrapped is not a
     * multiple of the block size.
     *
     * @exception InvalidKeyException if it is impossible or unsafe to
     * wrap the key with this cipher (e.g., a hardware protected key is
     * being passed to a software only cipher).
     */
    byte[] wrap(Key key)
        throws IllegalBlockSizeException, InvalidKeyException {
        byte[] result = null;

        byte[] encodedKey = key.getEncoded();
        if ((encodedKey == null) || (encodedKey.length == 0)) {
            throw new InvalidKeyException("Cannot get an encoding of " +
                                          "the key to be wrapped");
        }
        result = doFinal(encodedKey, 0, encodedKey.length);
        return result;
    }

    /**
     * Unwrap a previously wrapped key.
     *
     * @param wrappedKey the key to be unwrapped.
     *
     * @param wrappedKeyAlgorithm the algorithm the wrapped key is for.
     *
     * @param wrappedKeyType the type of the wrapped key.
     * This is one of <code>Cipher.SECRET_KEY</code>,
     * <code>Cipher.PRIVATE_KEY</code>, or <code>Cipher.PUBLIC_KEY</code>.
     *
     * @return the unwrapped key.
     *
     * @exception NoSuchAlgorithmException if no installed providers
     * can create keys of type <code>wrappedKeyType</code> for the
     * <code>wrappedKeyAlgorithm</code>.
     *
     * @exception InvalidKeyException if <code>wrappedKey</code> does not
     * represent a wrapped key of type <code>wrappedKeyType</code> for
     * the <code>wrappedKeyAlgorithm</code>.
     */
    Key unwrap(byte[] wrappedKey, String wrappedKeyAlgorithm,
               int wrappedKeyType)
        throws InvalidKeyException, NoSuchAlgorithmException {
        byte[] encodedKey;
        try {
            encodedKey = doFinal(wrappedKey, 0, wrappedKey.length);
        } catch (IllegalBlockSizeException eBlockSize) {
            throw new InvalidKeyException("The wrapped key does not have " +
                                          "the correct length");
        }
        return ConstructKeys.constructKey(encodedKey, wrappedKeyAlgorithm,
                                          wrappedKeyType);
    }

    /**
     * Continues a multi-part update of the Additional Authentication
     * Data (AAD), using a subset of the provided buffer.
     * <p>
     * Calls to this method provide AAD to the cipher when operating in
     * modes such as AEAD (GCM/CCM).  If this cipher is operating in
     * either GCM or CCM mode, all AAD must be supplied before beginning
     * operations on the ciphertext (via the {@code update} and {@code
     * doFinal} methods).
     *
     * @param src the buffer containing the AAD
     * @param offset the offset in {@code src} where the AAD input starts
     * @param len the number of AAD bytes
     *
     * @throws IllegalStateException if this cipher is in a wrong state
     * (e.g., has not been initialized), does not accept AAD, or if
     * operating in either GCM or CCM mode and one of the {@code update}
     * methods has already been called for the active
     * encryption/decryption operation
     * @throws UnsupportedOperationException if this method
     * has not been overridden by an implementation
     *
     * @since 1.8
     */
    void updateAAD(byte[] src, int offset, int len) {
        checkReinit();
        cipher.updateAAD(src, offset, len);
    }
}
