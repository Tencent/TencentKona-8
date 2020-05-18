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

public class SM2KeyPairGenerator extends KeyPairGenerator {
    private long handler;

    /**
     * Creates a KeyPairGenerator object for the specified algorithm.
     */
    public SM2KeyPairGenerator() {
        super("SM2");
        if (handler != 0) {
            SMUtils.getInstance().SM2FreeCtx(handler);
            handler = 0;
        }
        handler = SMUtils.getInstance().SM2InitCtx();
    }

    @Override
    public KeyPair generateKeyPair() {
        Object[] objects = SMUtils.getInstance().SM2GenKeyPair(handler);
        String priString;
        String pubString;
        if (! ((objects[0] instanceof String) && (objects[1] instanceof String))) {
            throw new IllegalStateException("Invalid KeyPair Generation");
        }
        priString = (String)objects[0];
        pubString = (String)objects[1];
        SM2PublicKey pubKey = new SM2PublicKey(pubString);
        SM2PrivateKey priKey = new SM2PrivateKey(priString);

        return new KeyPair(pubKey, priKey);
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
