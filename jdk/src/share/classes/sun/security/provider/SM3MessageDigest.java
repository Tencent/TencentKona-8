/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package sun.security.provider;

import java.security.DigestException;
import java.security.MessageDigest;

/**
 * SM3 hash in compliance with GB/T 32905-2016.
 */
public final class SM3MessageDigest extends MessageDigest implements Cloneable {

    private SM3Engine engine = new SM3Engine();

    public SM3MessageDigest() {
        super("SM3");
    }

    @Override
    protected int engineGetDigestLength() {
        return 32;
    }

    @Override
    protected void engineUpdate(byte input) {
        engine.update(input);
    }

    @Override
    protected void engineUpdate(byte[] input, int offset, int length) {
        if (length == 0) {
            return;
        }

        if ((offset < 0) || (length < 0) || (offset > input.length - length)) {
            throw new ArrayIndexOutOfBoundsException();
        }

        engine.update(input, offset, length);
    }

    @Override
    protected byte[] engineDigest() {
        byte[] digest = new byte[32];
        engine.doFinal(digest);
        return digest;
    }

    @Override
    protected int engineDigest(byte[] buf, int offset, int length)
            throws DigestException {
        if (length != 32) {
            throw new DigestException("The length must be 32-bytes");
        }

        engine.doFinal(buf, offset);
        return 32;
    }

    @Override
    protected void engineReset() {
        engine.reset();
    }

    public Object clone() throws CloneNotSupportedException {
        SM3MessageDigest clone = (SM3MessageDigest) super.clone();
        clone.engine = engine.clone();
        return clone;
    }
}
