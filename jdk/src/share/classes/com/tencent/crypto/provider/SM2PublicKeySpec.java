package com.tencent.crypto.provider;

import java.security.spec.KeySpec;

public class SM2PublicKeySpec implements KeySpec {
    private String keyByteString;
    public SM2PublicKeySpec(String keyString) {
        keyByteString = keyString;
    }

    public SM2PublicKeySpec(byte[] bytes) {
        keyByteString = new String(bytes);
    }

    String getKeyByteString() {
        return keyByteString;
    }

}
