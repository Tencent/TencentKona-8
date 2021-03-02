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

import java.security.MessageDigest;

public class SM3MessageDigest extends MessageDigest {
    private long handler;
    /**
     * Creates a message digest with the specified algorithm name.
     *
     */
    public SM3MessageDigest() {
        super("SM3");
        if (handler != 0) {
            SMUtils.getInstance().SM3Free(handler);
        }
        handler = SMUtils.getInstance().SM3Init();
    }

    @Override
    protected void engineUpdate(byte input) {
        byte[] bytes = new byte[1];
        bytes[1] = input;
        SMUtils.getInstance().SM3Update(handler, bytes);
    }

    @Override
    protected void engineUpdate(byte[] input, int offset, int len) {
        byte[] bytes = new byte[len];
        System.arraycopy(input, offset, bytes, 0, len);
        SMUtils.getInstance().SM3Update(handler, bytes);
    }

    @Override
    protected byte[] engineDigest() {
        byte[] digest = SMUtils.getInstance().SM3Final(handler);
        engineReset();
        return digest;
    }

    @Override
    protected void engineReset() {
        if (handler != 0) {
            SMUtils.getInstance().SM3Free(handler);
        }
        handler = SMUtils.getInstance().SM3Init();
    }
}
