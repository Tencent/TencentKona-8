/*
 * Copyright (C) 2023, 2024, THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * @test
 * @summary Test HmacSM3 key generator.
 * @run testng HmacSM3KeyGeneratorTest
 */

import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import java.security.InvalidParameterException;

import org.testng.annotations.Test;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.expectThrows;

public class HmacSM3KeyGeneratorTest {

    @Test
    public void testKeyGen() throws Exception {
        KeyGenerator keyGen = KeyGenerator.getInstance("HmacSM3");

        expectThrows(InvalidParameterException.class, ()-> keyGen.init(127));

        SecretKey key = keyGen.generateKey();
        assertEquals(32, key.getEncoded().length);

        keyGen.init(128);
        key = keyGen.generateKey();
        assertEquals(16, key.getEncoded().length);

        keyGen.init(192);
        key = keyGen.generateKey();
        assertEquals(24, key.getEncoded().length);

        keyGen.init(191);
        key = keyGen.generateKey();
        assertEquals(24, key.getEncoded().length);
    }
}
