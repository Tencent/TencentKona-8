package com.tencent.crypto.provider;

import java.security.*;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;

public class SM2KeyFactory extends KeyFactorySpi {
    @Override
    protected PublicKey engineGeneratePublic(KeySpec keySpec) throws InvalidKeySpecException {
        if ((keySpec instanceof SM2PublicKeySpec)) {
            SM2PublicKeySpec spec =  (SM2PublicKeySpec)keySpec;
            if (spec.getKeyByteString() == null || spec.getKeyByteString().isEmpty()) {
                throw new InvalidKeySpecException("Invalid Public KeySpec, empty Key");
            } else {
                return new SM2PublicKey(spec.getKeyByteString());
            }
        } else {
            throw new InvalidKeySpecException("Invalid KeySpec type, only accept SM2PublicKeySpec");
        }
    }

    @Override
    protected PrivateKey engineGeneratePrivate(KeySpec keySpec) throws InvalidKeySpecException {
        if ((keySpec instanceof SM2PrivateKeySpec)) {
            SM2PrivateKeySpec spec =  (SM2PrivateKeySpec)keySpec;
            if (spec.getKeyByteString() == null || spec.getKeyByteString().isEmpty()) {
                throw new InvalidKeySpecException("Invalid Private KeySpec, empty Key");
            } else {
                return new SM2PrivateKey(spec.getKeyByteString());
            }
        } else {
            throw new InvalidKeySpecException("Invalid KeySpec type, only accept SM2PrivateKeySpec");
        }
    }

    @Override
    protected <T extends KeySpec> T engineGetKeySpec(Key key, Class<T> keySpec) throws InvalidKeySpecException {
        if (key instanceof SM2PrivateKey) {
            String keyString = ((SM2PrivateKey) key).getKeyString();
            return keySpec.cast( new SM2PrivateKeySpec(keyString));
        } else if (key instanceof SM2PublicKey) {
            String keyString = ((SM2PublicKey) key).getKeyString();
            return keySpec.cast( new SM2PublicKeySpec(keyString));
        } else {
            throw new InvalidKeySpecException("Invalid Key, must be SM2PrivateKey or SM2PublicKey");
        }
    }

    @Override
    protected Key engineTranslateKey(Key key) throws InvalidKeyException {
        if (key instanceof SM2PublicKey) {
            return (SM2PublicKey)key;
        } else if (key instanceof SM2PrivateKey) {
            return (SM2PrivateKey)key;
        } else {
            throw new InvalidKeyException("Invalid Key, must be SM2PrivateKey or SM2PublicKey");
        }
    }
}
