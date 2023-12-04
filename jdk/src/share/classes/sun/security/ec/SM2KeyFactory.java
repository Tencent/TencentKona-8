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

package sun.security.ec;

import java.math.BigInteger;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.KeyFactorySpi;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.interfaces.ECPrivateKey;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECPoint;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;
import java.security.spec.SM2PrivateKeySpec;
import java.security.spec.SM2PublicKeySpec;

public final class SM2KeyFactory extends KeyFactorySpi {

    @Override
    protected PublicKey engineGeneratePublic(KeySpec keySpec)
            throws InvalidKeySpecException {
        if (!(keySpec instanceof SM2PublicKeySpec)) {
            throw new InvalidKeySpecException("Only accept SM2PublicKeySpec");
        }

        SM2PublicKeySpec spec = (SM2PublicKeySpec) keySpec;
        ECPoint pubPoint = spec.getW();
        if (pubPoint == null || pubPoint.getAffineX() == null
                || pubPoint.getAffineY() == null) {
            throw new InvalidKeySpecException("No public key");
        }

        return new SM2PublicKey(pubPoint);
    }

    @Override
    protected PrivateKey engineGeneratePrivate(KeySpec keySpec)
            throws InvalidKeySpecException {
        if (!(keySpec instanceof SM2PrivateKeySpec)) {
            throw new InvalidKeySpecException("Only accept SM2PrivateKeySpec");
        }

        SM2PrivateKeySpec spec = (SM2PrivateKeySpec) keySpec;
        BigInteger keyS = spec.getS();
        if (keyS == null) {
            throw new InvalidKeySpecException("No private key");
        }

        return new SM2PrivateKey(keyS);
    }

    @Override
    protected <T extends KeySpec> T engineGetKeySpec(Key key, Class<T> keySpecClass)
            throws InvalidKeySpecException {
        try {
            key = engineTranslateKey(key);
        } catch (InvalidKeyException e) {
            throw new InvalidKeySpecException(e);
        }

        if (key instanceof ECPrivateKey) {
            if (keySpecClass.isAssignableFrom(SM2PrivateKeySpec.class)) {
                ECPrivateKey privateKey = (ECPrivateKey) key;
                return keySpecClass.cast(new SM2PrivateKeySpec(privateKey.getS()));
            } else {
                throw new InvalidKeySpecException(
                        "keySpecClass must be SM2PrivateKeySpec for SM2 private key");
            }
        } else if (key instanceof ECPublicKey) {
            if (keySpecClass.isAssignableFrom(SM2PublicKeySpec.class)) {
                ECPublicKey publicKey = (ECPublicKey) key;
                return keySpecClass.cast(new SM2PublicKeySpec(publicKey.getW()));
            } else {
                throw new InvalidKeySpecException(
                        "keySpecClass must be SM2PublicKeySpec for SM2 public key");
            }
        }

        throw new InvalidKeySpecException("Neither public nor private key");
    }

    @Override
    protected Key engineTranslateKey(Key key) throws InvalidKeyException {
        if (key instanceof ECPrivateKey) {
            return new SM2PrivateKey(((ECPrivateKey) key).getS());
        }

        if (key instanceof ECPublicKey) {
            return new SM2PublicKey(((ECPublicKey) key).getW());
        }

        throw new InvalidKeyException(
                "key must be ECPrivateKey or ECPublicKey");
    }
}
