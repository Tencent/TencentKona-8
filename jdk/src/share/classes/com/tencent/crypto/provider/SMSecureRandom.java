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

import java.security.InvalidAlgorithmParameterException;
import java.security.SecureRandomSpi;

public class SMSecureRandom extends SecureRandomSpi {
    long handler;

    public SMSecureRandom() throws InvalidAlgorithmParameterException {
        handler = SMUtils.getInstance().SMInitRandomCtx();
        if (handler == 0) {
            throw new InvalidAlgorithmParameterException("Fail to initialize SMRandom Context");
        }
    }

    @Override
    protected void engineSetSeed(byte[] seed) {
        SMUtils.getInstance().SMRandomReSeed(handler, seed);
    }

    @Override
    protected void engineNextBytes(byte[] bytes) {
        assert(bytes != null);
        int len = bytes.length;
        byte[] random;
        if (handler == 0) {
            random = SMUtils.getInstance().SMRandom(len);
        } else {
            random = SMUtils.getInstance().SMRandom(handler, len);
        }
        System.arraycopy(random, 0, bytes, 0, len);
    }

    @Override
    protected byte[] engineGenerateSeed(int numBytes) {
        byte[] seed = new byte[numBytes];
        engineNextBytes(seed);
        return seed;
    }

    @Override
    protected void finalize() throws Throwable {
        if (handler != 0) {
            SMUtils.getInstance().SMFreeRandomCtx(handler);
        }
        super.finalize();
    }
}
