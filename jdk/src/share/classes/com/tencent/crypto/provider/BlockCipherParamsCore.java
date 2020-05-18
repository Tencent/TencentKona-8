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

import sun.misc.HexDumpEncoder;
import sun.security.util.DerInputStream;
import sun.security.util.DerOutputStream;

import javax.crypto.spec.IvParameterSpec;
import java.io.IOException;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidParameterSpecException;

/**
 * This class implements the parameter (IV) used with Block Ciphers
 * in feedback-mode. IV is defined in the standards as follows:
 *
 * <pre>
 * IV ::= OCTET STRING  -- length depends on the block size of the
 * block ciphers
 * </pre>
 *
 *
 */
final class BlockCipherParamsCore {
    private int block_size = 0;
    private byte[] iv = null;
    private byte[] tag;

    BlockCipherParamsCore(int blksize) {
        block_size = blksize;
    }

    void init(AlgorithmParameterSpec paramSpec)
        throws InvalidParameterSpecException {
        byte[] tmpIv = new byte[0];
        if (!(paramSpec instanceof IvParameterSpec) &&
                !(paramSpec instanceof  SM4GCMParameterSpec) ) {
            throw new InvalidParameterSpecException
                ("Inappropriate parameter specification");
        }
        if (paramSpec instanceof IvParameterSpec) {
            tmpIv = ((IvParameterSpec) paramSpec).getIV();
        } else if (paramSpec instanceof SM4GCMParameterSpec) {
            tmpIv = ((SM4GCMParameterSpec)paramSpec).getIV();
            tag = ((SM4GCMParameterSpec)paramSpec).getTag();
        }

        if (tmpIv.length != block_size) {
            throw new InvalidParameterSpecException("IV not " +
                        block_size + " bytes long");
        }
        iv = tmpIv.clone();
    }

    void init(byte[] encoded) throws IOException {
        DerInputStream der = new DerInputStream(encoded);

        byte[] tmpIv = der.getOctetString();
        if (der.available() != 0) {
            throw new IOException("IV parsing error: extra data");
        }
        if (tmpIv.length != block_size) {
            throw new IOException("IV not " + block_size +
                " bytes long");
        }
        iv = tmpIv;
    }

    void init(byte[] encoded, String decodingMethod)
        throws IOException {
        if ((decodingMethod != null) &&
            (!decodingMethod.equalsIgnoreCase("ASN.1"))) {
            throw new IllegalArgumentException("Only support ASN.1 format");
        }
        init(encoded);
    }

    <T extends AlgorithmParameterSpec> T getParameterSpec(Class<T> paramSpec)
        throws InvalidParameterSpecException
    {
        if (IvParameterSpec.class.isAssignableFrom(paramSpec)) {
            return paramSpec.cast(new IvParameterSpec(this.iv));
        } else if (SM4GCMParameterSpec.class.isAssignableFrom(paramSpec)) {
            if (tag != null) {
                return paramSpec.cast(new SM4GCMParameterSpec(tag.length, iv, tag));
            } else throw new InvalidParameterSpecException("Invalid tag");
        } else {
            throw new InvalidParameterSpecException
                ("Inappropriate parameter specification");
        }
    }

    byte[] getEncoded() throws IOException {
        DerOutputStream out = new DerOutputStream();
        out.putOctetString(this.iv);
        return out.toByteArray();
    }

    byte[] getEncoded(String encodingMethod)
        throws IOException {
        return getEncoded();
    }

    /*
     * Returns a formatted string describing the parameters.
     */
    public String toString() {
        String LINE_SEP = System.getProperty("line.separator");

        String ivString = LINE_SEP + "    iv:" + LINE_SEP + "[";
        HexDumpEncoder encoder = new HexDumpEncoder();
        ivString += encoder.encodeBuffer(this.iv);
        ivString += "]" + LINE_SEP;
        return ivString;
    }
}
