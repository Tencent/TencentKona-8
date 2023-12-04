/*
 * Copyright (C) 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

package sun.security.util;

import java.security.AccessController;

import sun.security.action.GetBooleanAction;

/**
 * The constants for ShangMi features.
 */
public final class SMConst {

    /**
     * Indicates if the tools, like keytool, use {@code curveSM2}.
     * <p>
     * If using {@code curveSM2}, the keytool option {@code -sigalg} must be {@code SM3withSM2}.
     */
    public static final boolean TOOLS_USE_CURVESM2
            = AccessController.doPrivileged(new GetBooleanAction(
                    "jdk.tools.useCurveSM2"));

    private SMConst() { }
}
