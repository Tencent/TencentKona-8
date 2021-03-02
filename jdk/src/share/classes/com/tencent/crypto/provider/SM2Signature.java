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

import java.security.*;
import java.security.spec.AlgorithmParameterSpec;
import java.util.Arrays;

public class SM2Signature extends Signature {

    private SM2PrivateKey priKey;
    private SM2PublicKey pubKey;
    private long handler;
    private byte[] buffer;
    private byte[] id;

    private static final int ERR_TENCENTSM_OK = 0;
    private static final int ERR_ILLEGAL_ARGUMENT = -10001;
    private static final int ERR_TC_MALLOC = -10002;
    private static final int ERR_REMOVE_FILE = -10003;
    private static final int ERR_ASN1_FORMAT_ERROR = -11001;
    private static final int ERR_ASN1_DECODE_OBJ = -11002;

    /**
     * Creates a Signature object for the specified algorithm.
     */
    public SM2Signature() {
        super("SM2");
        if (handler != 0) {
            SMUtils.getInstance().SM2FreeCtx(handler);
            handler = 0;
        }
        handler = SMUtils.getInstance().SM2InitCtx();
    }

    private void reset() {
        buffer = null;
        pubKey = null;
        priKey = null;
        id = null;
    }

    @Override
    protected void engineInitVerify(PublicKey publicKey) throws InvalidKeyException {
        reset();
        if (publicKey instanceof SM2PublicKey) {
            pubKey = (SM2PublicKey) publicKey;
        } else {
            pubKey = null;
            throw new InvalidKeyException("only SM2PublicKey accepted!");
        }
        priKey = null;
    }

    @Override
    protected void engineInitSign(PrivateKey privateKey) throws InvalidKeyException {
        reset();
        if (privateKey instanceof SM2PrivateKey) {
            priKey = (SM2PrivateKey)privateKey;
        } else {
            throw new InvalidKeyException("Only SM2PriKey accepted!");
        }
        pubKey = null;
    }

    @Override
    protected void engineUpdate(byte b) throws SignatureException {
        buffer = new byte[1];
        buffer[0] = b;
    }

    @Override
    protected void engineUpdate(byte[] in, int off, int len) throws SignatureException {
        if ((len == 0) || (in == null)) {
            return;
        }
        buffer = new byte[len];
        System.arraycopy(in, off, buffer, 0, len);
    }

    @Override
    protected byte[] engineSign() throws SignatureException {
        if (pubKey == null) {
            throw new SignatureException("Public Key of SM2 Sign must be set with SetParameter(SM2ParameterSpec)");
        }
        if (priKey == null) {
            throw new SignatureException(("Private Key not initialized"));
        }
        if (id == null) {
            throw new SignatureException("id not initialized, must be set with SetParameter()");
        }
        String strPubKey = new String(pubKey.getEncoded());
        String strPriKey = new String(priKey.getEncoded());
        return SMUtils.getInstance().SM2Sign(handler, buffer, id, strPubKey, strPriKey);
    }

    @Override
    protected boolean engineVerify(byte[] sigBytes) throws SignatureException {
        if (pubKey == null) {
            throw new SignatureException("Public Key not initialized");
        }
        if (id == null) {
            throw new SignatureException("id not initialized, must be set with SetParameter()");
        }
        String strPubKey = new String(pubKey.getEncoded());

        int succ = SMUtils.getInstance().SM2Verify(handler,buffer, id, strPubKey, sigBytes);

        if (succ == ERR_TENCENTSM_OK) {
            return true;
        }
        return false;
    }

    @Override
    protected void engineSetParameter(String param, Object value) throws InvalidParameterException {
        if (param.equalsIgnoreCase("Id")) {
            id = (byte[])value;
        } else if (param.equalsIgnoreCase("publicKey")) {
            pubKey = (SM2PublicKey)value;
        } else {
            throw new InvalidParameterException("unsupported parameter: " + param);
        }
    }

    @Override
    protected void engineSetParameter(AlgorithmParameterSpec params)
            throws InvalidAlgorithmParameterException {
        if (params instanceof SM2SignatureParameterSpec) {
           id = ((SM2SignatureParameterSpec) params).getId();
           if (pubKey == null) {
               pubKey = ((SM2SignatureParameterSpec) params).getPublicKey();
           } else if (!Arrays.equals(pubKey.getEncoded(),
                   ((SM2SignatureParameterSpec) params).getPublicKey().getEncoded())) {
               throw new InvalidAlgorithmParameterException("public Key of Parameter and signature not match");
            }
        } else {
            throw new InvalidAlgorithmParameterException("only accept SM2ParameterSpec");
        }
    }

    @Override
    protected Object engineGetParameter(String param) throws InvalidParameterException {
        if (param.equalsIgnoreCase("Id")) {
            return id;
        } else if (param.equalsIgnoreCase("publicKey")) {
            return pubKey;
        } else {
            throw new InvalidParameterException("unsupported parameter: " + param);
        }
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
