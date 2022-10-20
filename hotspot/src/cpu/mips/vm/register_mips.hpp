/*
 * Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2022, Loongson Technology. All rights reserved.
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

#ifndef CPU_MIPS_VM_REGISTER_MIPS_HPP
#define CPU_MIPS_VM_REGISTER_MIPS_HPP

#include "asm/register.hpp"
#include "vm_version_mips.hpp"

class VMRegImpl;
typedef VMRegImpl* VMReg;

// Use Register as shortcut
class RegisterImpl;
typedef RegisterImpl* Register;


// The implementation of integer registers for the mips architecture
inline Register as_Register(int encoding) {
  return (Register)(intptr_t) encoding;
}

class RegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
    number_of_registers     = 32
  };

  // derived registers, offsets, and addresses
  Register successor() const                          { return as_Register(encoding() + 1); }

  // construction
  inline friend Register as_Register(int encoding);

  VMReg as_VMReg();

  // accessors
  int   encoding() const                         { assert(is_valid(),err_msg( "invalid register (%d)", (int)(intptr_t)this)); return (intptr_t)this; }
  bool  is_valid() const                         { return 0 <= (intptr_t)this && (intptr_t)this < number_of_registers; }
  const char* name() const;
};


// The integer registers of the MIPS32 architecture
CONSTANT_REGISTER_DECLARATION(Register, noreg, (-1));


CONSTANT_REGISTER_DECLARATION(Register, i0,    (0));
CONSTANT_REGISTER_DECLARATION(Register, i1,    (1));
CONSTANT_REGISTER_DECLARATION(Register, i2,    (2));
CONSTANT_REGISTER_DECLARATION(Register, i3,    (3));
CONSTANT_REGISTER_DECLARATION(Register, i4,    (4));
CONSTANT_REGISTER_DECLARATION(Register, i5,    (5));
CONSTANT_REGISTER_DECLARATION(Register, i6,    (6));
CONSTANT_REGISTER_DECLARATION(Register, i7,    (7));
CONSTANT_REGISTER_DECLARATION(Register, i8,    (8));
CONSTANT_REGISTER_DECLARATION(Register, i9,    (9));
CONSTANT_REGISTER_DECLARATION(Register, i10,   (10));
CONSTANT_REGISTER_DECLARATION(Register, i11,   (11));
CONSTANT_REGISTER_DECLARATION(Register, i12,   (12));
CONSTANT_REGISTER_DECLARATION(Register, i13,   (13));
CONSTANT_REGISTER_DECLARATION(Register, i14,   (14));
CONSTANT_REGISTER_DECLARATION(Register, i15,   (15));
CONSTANT_REGISTER_DECLARATION(Register, i16,   (16));
CONSTANT_REGISTER_DECLARATION(Register, i17,   (17));
CONSTANT_REGISTER_DECLARATION(Register, i18,   (18));
CONSTANT_REGISTER_DECLARATION(Register, i19,   (19));
CONSTANT_REGISTER_DECLARATION(Register, i20,   (20));
CONSTANT_REGISTER_DECLARATION(Register, i21,   (21));
CONSTANT_REGISTER_DECLARATION(Register, i22,   (22));
CONSTANT_REGISTER_DECLARATION(Register, i23,   (23));
CONSTANT_REGISTER_DECLARATION(Register, i24,   (24));
CONSTANT_REGISTER_DECLARATION(Register, i25,   (25));
CONSTANT_REGISTER_DECLARATION(Register, i26,   (26));
CONSTANT_REGISTER_DECLARATION(Register, i27,   (27));
CONSTANT_REGISTER_DECLARATION(Register, i28,   (28));
CONSTANT_REGISTER_DECLARATION(Register, i29,   (29));
CONSTANT_REGISTER_DECLARATION(Register, i30,   (30));
CONSTANT_REGISTER_DECLARATION(Register, i31,   (31));

#ifndef DONT_USE_REGISTER_DEFINES
#define NOREG ((Register)(noreg_RegisterEnumValue))

#define I0 ((Register)(i0_RegisterEnumValue))
#define I1 ((Register)(i1_RegisterEnumValue))
#define I2 ((Register)(i2_RegisterEnumValue))
#define I3 ((Register)(i3_RegisterEnumValue))
#define I4 ((Register)(i4_RegisterEnumValue))
#define I5 ((Register)(i5_RegisterEnumValue))
#define I6 ((Register)(i6_RegisterEnumValue))
#define I7 ((Register)(i7_RegisterEnumValue))
#define I8 ((Register)(i8_RegisterEnumValue))
#define I9 ((Register)(i9_RegisterEnumValue))
#define I10 ((Register)(i10_RegisterEnumValue))
#define I11 ((Register)(i11_RegisterEnumValue))
#define I12 ((Register)(i12_RegisterEnumValue))
#define I13 ((Register)(i13_RegisterEnumValue))
#define I14 ((Register)(i14_RegisterEnumValue))
#define I15 ((Register)(i15_RegisterEnumValue))
#define I16 ((Register)(i16_RegisterEnumValue))
#define I17 ((Register)(i17_RegisterEnumValue))
#define I18 ((Register)(i18_RegisterEnumValue))
#define I19 ((Register)(i19_RegisterEnumValue))
#define I20 ((Register)(i20_RegisterEnumValue))
#define I21 ((Register)(i21_RegisterEnumValue))
#define I22 ((Register)(i22_RegisterEnumValue))
#define I23 ((Register)(i23_RegisterEnumValue))
#define I24 ((Register)(i24_RegisterEnumValue))
#define I25 ((Register)(i25_RegisterEnumValue))
#define I26 ((Register)(i26_RegisterEnumValue))
#define I27 ((Register)(i27_RegisterEnumValue))
#define I28 ((Register)(i28_RegisterEnumValue))
#define I29 ((Register)(i29_RegisterEnumValue))
#define I30 ((Register)(i30_RegisterEnumValue))
#define I31 ((Register)(i31_RegisterEnumValue))

#define R0 ((Register)(i0_RegisterEnumValue))
#define AT ((Register)(i1_RegisterEnumValue))
#define V0 ((Register)(i2_RegisterEnumValue))
#define V1 ((Register)(i3_RegisterEnumValue))
#define RA0 ((Register)(i4_RegisterEnumValue))
#define RA1 ((Register)(i5_RegisterEnumValue))
#define RA2 ((Register)(i6_RegisterEnumValue))
#define RA3 ((Register)(i7_RegisterEnumValue))
#define RA4 ((Register)(i8_RegisterEnumValue))
#define RA5 ((Register)(i9_RegisterEnumValue))
#define RA6 ((Register)(i10_RegisterEnumValue))
#define RA7 ((Register)(i11_RegisterEnumValue))
#define RT0 ((Register)(i12_RegisterEnumValue))
#define RT1 ((Register)(i13_RegisterEnumValue))
#define RT2 ((Register)(i14_RegisterEnumValue))
#define RT3 ((Register)(i15_RegisterEnumValue))
#define S0 ((Register)(i16_RegisterEnumValue))
#define S1 ((Register)(i17_RegisterEnumValue))
#define S2 ((Register)(i18_RegisterEnumValue))
#define S3 ((Register)(i19_RegisterEnumValue))
#define S4 ((Register)(i20_RegisterEnumValue))
#define S5 ((Register)(i21_RegisterEnumValue))
#define S6 ((Register)(i22_RegisterEnumValue))
#define S7 ((Register)(i23_RegisterEnumValue))
#define RT8 ((Register)(i24_RegisterEnumValue))
#define RT9 ((Register)(i25_RegisterEnumValue))
#define K0 ((Register)(i26_RegisterEnumValue))
#define K1 ((Register)(i27_RegisterEnumValue))
#define GP ((Register)(i28_RegisterEnumValue))
#define SP ((Register)(i29_RegisterEnumValue))
#define FP ((Register)(i30_RegisterEnumValue))
#define S8 ((Register)(i30_RegisterEnumValue))
#define RA ((Register)(i31_RegisterEnumValue))

#define c_rarg0       RT0
#define c_rarg1       RT1
#define Rmethod       S3
#define Rsender       S4
#define Rnext         S1

/*
#define RT0       T0
#define RT1       T1
#define RT2       T2
#define RT3       T3
#define RT4       T8
#define RT5       T9
*/


//for interpreter frame
// bytecode pointer register
#define BCP            S0
// local variable pointer register
#define LVP            S7
// temperary callee saved register, we use this register to save the register maybe blowed cross call_VM
// be sure to save and restore its value in call_stub
#define TSR            S2

//OPT_SAFEPOINT not supported yet
#define OPT_SAFEPOINT 1

#define OPT_THREAD 1

#define TREG           S6

#define  S5_heapbase   S5

#define mh_SP_save     SP

#define FSR            V0
#define SSR            V1
#define FSF            F0
#define SSF            F1
#define FTF            F14
#define STF            F15

#define AFT            F30

#define RECEIVER       T0
#define IC_Klass       T1

#define SHIFT_count    T3

#endif // DONT_USE_REGISTER_DEFINES

// Use FloatRegister as shortcut
class FloatRegisterImpl;
typedef FloatRegisterImpl* FloatRegister;

inline FloatRegister as_FloatRegister(int encoding) {
  return (FloatRegister)(intptr_t) encoding;
}

// The implementation of floating point registers for the mips architecture
class FloatRegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
    float_arg_base      = 12,
    number_of_registers = 32
  };

  // construction
  inline friend FloatRegister as_FloatRegister(int encoding);

  VMReg as_VMReg();

  // derived registers, offsets, and addresses
  FloatRegister successor() const                          { return as_FloatRegister(encoding() + 1); }

  // accessors
  int   encoding() const                          { assert(is_valid(), "invalid register"); return (intptr_t)this; }
  bool  is_valid() const                          { return 0 <= (intptr_t)this && (intptr_t)this < number_of_registers; }
  const char* name() const;

};

CONSTANT_REGISTER_DECLARATION(FloatRegister, fnoreg , (-1));

CONSTANT_REGISTER_DECLARATION(FloatRegister, f0     , ( 0));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f1     , ( 1));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f2     , ( 2));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f3     , ( 3));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f4     , ( 4));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f5     , ( 5));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f6     , ( 6));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f7     , ( 7));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f8     , ( 8));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f9     , ( 9));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f10    , (10));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f11    , (11));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f12    , (12));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f13    , (13));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f14    , (14));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f15    , (15));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f16    , (16));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f17    , (17));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f18    , (18));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f19    , (19));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f20    , (20));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f21    , (21));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f22    , (22));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f23    , (23));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f24    , (24));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f25    , (25));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f26    , (26));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f27    , (27));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f28    , (28));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f29    , (29));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f30    , (30));
CONSTANT_REGISTER_DECLARATION(FloatRegister, f31    , (31));

#ifndef DONT_USE_REGISTER_DEFINES
#define FNOREG ((FloatRegister)(fnoreg_FloatRegisterEnumValue))
#define F0     ((FloatRegister)(    f0_FloatRegisterEnumValue))
#define F1     ((FloatRegister)(    f1_FloatRegisterEnumValue))
#define F2     ((FloatRegister)(    f2_FloatRegisterEnumValue))
#define F3     ((FloatRegister)(    f3_FloatRegisterEnumValue))
#define F4     ((FloatRegister)(    f4_FloatRegisterEnumValue))
#define F5     ((FloatRegister)(    f5_FloatRegisterEnumValue))
#define F6     ((FloatRegister)(    f6_FloatRegisterEnumValue))
#define F7     ((FloatRegister)(    f7_FloatRegisterEnumValue))
#define F8     ((FloatRegister)(    f8_FloatRegisterEnumValue))
#define F9     ((FloatRegister)(    f9_FloatRegisterEnumValue))
#define F10    ((FloatRegister)(   f10_FloatRegisterEnumValue))
#define F11    ((FloatRegister)(   f11_FloatRegisterEnumValue))
#define F12    ((FloatRegister)(   f12_FloatRegisterEnumValue))
#define F13    ((FloatRegister)(   f13_FloatRegisterEnumValue))
#define F14    ((FloatRegister)(   f14_FloatRegisterEnumValue))
#define F15    ((FloatRegister)(   f15_FloatRegisterEnumValue))
#define F16    ((FloatRegister)(   f16_FloatRegisterEnumValue))
#define F17    ((FloatRegister)(   f17_FloatRegisterEnumValue))
#define F18    ((FloatRegister)(   f18_FloatRegisterEnumValue))
#define F19    ((FloatRegister)(   f19_FloatRegisterEnumValue))
#define F20    ((FloatRegister)(   f20_FloatRegisterEnumValue))
#define F21    ((FloatRegister)(   f21_FloatRegisterEnumValue))
#define F22    ((FloatRegister)(   f22_FloatRegisterEnumValue))
#define F23    ((FloatRegister)(   f23_FloatRegisterEnumValue))
#define F24    ((FloatRegister)(   f24_FloatRegisterEnumValue))
#define F25    ((FloatRegister)(   f25_FloatRegisterEnumValue))
#define F26    ((FloatRegister)(   f26_FloatRegisterEnumValue))
#define F27    ((FloatRegister)(   f27_FloatRegisterEnumValue))
#define F28    ((FloatRegister)(   f28_FloatRegisterEnumValue))
#define F29    ((FloatRegister)(   f29_FloatRegisterEnumValue))
#define F30    ((FloatRegister)(   f30_FloatRegisterEnumValue))
#define F31    ((FloatRegister)(   f31_FloatRegisterEnumValue))
#endif // DONT_USE_REGISTER_DEFINES


const int MIPS_ARGS_IN_REGS_NUM = 4;

// Need to know the total number of registers of all sorts for SharedInfo.
// Define a class that exports it.
class ConcreteRegisterImpl : public AbstractRegisterImpl {
 public:
  enum {
  // A big enough number for C2: all the registers plus flags
  // This number must be large enough to cover REG_COUNT (defined by c2) registers.
  // There is no requirement that any ordering here matches any ordering c2 gives
  // it's optoregs.
    number_of_registers = (RegisterImpl::number_of_registers + FloatRegisterImpl::number_of_registers) * 2
  };

  static const int max_gpr;
  static const int max_fpr;
};

#endif //CPU_MIPS_VM_REGISTER_MIPS_HPP
