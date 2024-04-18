/*
 * Copyright (c) 2002, 2013, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2016, Loongson Technology. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "asm/assembler.hpp"
#include "asm/register.hpp"
#include "register_mips.hpp"
#ifdef TARGET_ARCH_MODEL_mips_32
# include "interp_masm_mips_32.hpp"
#endif
#ifdef TARGET_ARCH_MODEL_mips_64
# include "interp_masm_mips_64.hpp"
#endif

REGISTER_DEFINITION(Register, noreg);
REGISTER_DEFINITION(Register, i0);
REGISTER_DEFINITION(Register, i1);
REGISTER_DEFINITION(Register, i2);
REGISTER_DEFINITION(Register, i3);
REGISTER_DEFINITION(Register, i4);
REGISTER_DEFINITION(Register, i5);
REGISTER_DEFINITION(Register, i6);
REGISTER_DEFINITION(Register, i7);
REGISTER_DEFINITION(Register, i8);
REGISTER_DEFINITION(Register, i9);
REGISTER_DEFINITION(Register, i10);
REGISTER_DEFINITION(Register, i11);
REGISTER_DEFINITION(Register, i12);
REGISTER_DEFINITION(Register, i13);
REGISTER_DEFINITION(Register, i14);
REGISTER_DEFINITION(Register, i15);
REGISTER_DEFINITION(Register, i16);
REGISTER_DEFINITION(Register, i17);
REGISTER_DEFINITION(Register, i18);
REGISTER_DEFINITION(Register, i19);
REGISTER_DEFINITION(Register, i20);
REGISTER_DEFINITION(Register, i21);
REGISTER_DEFINITION(Register, i22);
REGISTER_DEFINITION(Register, i23);
REGISTER_DEFINITION(Register, i24);
REGISTER_DEFINITION(Register, i25);
REGISTER_DEFINITION(Register, i26);
REGISTER_DEFINITION(Register, i27);
REGISTER_DEFINITION(Register, i28);
REGISTER_DEFINITION(Register, i29);
REGISTER_DEFINITION(Register, i30);
REGISTER_DEFINITION(Register, i31);

REGISTER_DEFINITION(FloatRegister, fnoreg);
REGISTER_DEFINITION(FloatRegister, f0);
REGISTER_DEFINITION(FloatRegister, f1);
REGISTER_DEFINITION(FloatRegister, f2);
REGISTER_DEFINITION(FloatRegister, f3);
REGISTER_DEFINITION(FloatRegister, f4);
REGISTER_DEFINITION(FloatRegister, f5);
REGISTER_DEFINITION(FloatRegister, f6);
REGISTER_DEFINITION(FloatRegister, f7);
REGISTER_DEFINITION(FloatRegister, f8);
REGISTER_DEFINITION(FloatRegister, f9);
REGISTER_DEFINITION(FloatRegister, f10);
REGISTER_DEFINITION(FloatRegister, f11);
REGISTER_DEFINITION(FloatRegister, f12);
REGISTER_DEFINITION(FloatRegister, f13);
REGISTER_DEFINITION(FloatRegister, f14);
REGISTER_DEFINITION(FloatRegister, f15);
REGISTER_DEFINITION(FloatRegister, f16);
REGISTER_DEFINITION(FloatRegister, f17);
REGISTER_DEFINITION(FloatRegister, f18);
REGISTER_DEFINITION(FloatRegister, f19);
REGISTER_DEFINITION(FloatRegister, f20);
REGISTER_DEFINITION(FloatRegister, f21);
REGISTER_DEFINITION(FloatRegister, f22);
REGISTER_DEFINITION(FloatRegister, f23);
REGISTER_DEFINITION(FloatRegister, f24);
REGISTER_DEFINITION(FloatRegister, f25);
REGISTER_DEFINITION(FloatRegister, f26);
REGISTER_DEFINITION(FloatRegister, f27);
REGISTER_DEFINITION(FloatRegister, f28);
REGISTER_DEFINITION(FloatRegister, f29);
REGISTER_DEFINITION(FloatRegister, f30);
REGISTER_DEFINITION(FloatRegister, f31);
