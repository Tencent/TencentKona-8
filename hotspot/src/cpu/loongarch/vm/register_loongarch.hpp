/*
 * Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2020, Loongson Technology. All rights reserved.
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

#ifndef CPU_LOONGARCH_VM_REGISTER_LOONGARCH_HPP
#define CPU_LOONGARCH_VM_REGISTER_LOONGARCH_HPP

#include "asm/register.hpp"
#include "vm_version_loongarch.hpp"

class VMRegImpl;
typedef VMRegImpl* VMReg;

// Use Register as shortcut
class RegisterImpl;
typedef RegisterImpl* Register;


// The implementation of integer registers for the LoongArch architecture
inline Register as_Register(int encoding) {
  return (Register)(intptr_t) encoding;
}

class RegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
    number_of_registers     = 32,
    max_slots_per_register  = 2
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


// The integer registers of the LoongArch architecture
CONSTANT_REGISTER_DECLARATION(Register, noreg, (-1));


CONSTANT_REGISTER_DECLARATION(Register, r0,    (0));
CONSTANT_REGISTER_DECLARATION(Register, r1,    (1));
CONSTANT_REGISTER_DECLARATION(Register, r2,    (2));
CONSTANT_REGISTER_DECLARATION(Register, r3,    (3));
CONSTANT_REGISTER_DECLARATION(Register, r4,    (4));
CONSTANT_REGISTER_DECLARATION(Register, r5,    (5));
CONSTANT_REGISTER_DECLARATION(Register, r6,    (6));
CONSTANT_REGISTER_DECLARATION(Register, r7,    (7));
CONSTANT_REGISTER_DECLARATION(Register, r8,    (8));
CONSTANT_REGISTER_DECLARATION(Register, r9,    (9));
CONSTANT_REGISTER_DECLARATION(Register, r10,   (10));
CONSTANT_REGISTER_DECLARATION(Register, r11,   (11));
CONSTANT_REGISTER_DECLARATION(Register, r12,   (12));
CONSTANT_REGISTER_DECLARATION(Register, r13,   (13));
CONSTANT_REGISTER_DECLARATION(Register, r14,   (14));
CONSTANT_REGISTER_DECLARATION(Register, r15,   (15));
CONSTANT_REGISTER_DECLARATION(Register, r16,   (16));
CONSTANT_REGISTER_DECLARATION(Register, r17,   (17));
CONSTANT_REGISTER_DECLARATION(Register, r18,   (18));
CONSTANT_REGISTER_DECLARATION(Register, r19,   (19));
CONSTANT_REGISTER_DECLARATION(Register, r20,   (20));
CONSTANT_REGISTER_DECLARATION(Register, r21,   (21));
CONSTANT_REGISTER_DECLARATION(Register, r22,   (22));
CONSTANT_REGISTER_DECLARATION(Register, r23,   (23));
CONSTANT_REGISTER_DECLARATION(Register, r24,   (24));
CONSTANT_REGISTER_DECLARATION(Register, r25,   (25));
CONSTANT_REGISTER_DECLARATION(Register, r26,   (26));
CONSTANT_REGISTER_DECLARATION(Register, r27,   (27));
CONSTANT_REGISTER_DECLARATION(Register, r28,   (28));
CONSTANT_REGISTER_DECLARATION(Register, r29,   (29));
CONSTANT_REGISTER_DECLARATION(Register, r30,   (30));
CONSTANT_REGISTER_DECLARATION(Register, r31,   (31));

#ifndef DONT_USE_REGISTER_DEFINES
#define NOREG ((Register)(noreg_RegisterEnumValue))

#define R0  ((Register)(r0_RegisterEnumValue))
#define R1  ((Register)(r1_RegisterEnumValue))
#define R2  ((Register)(r2_RegisterEnumValue))
#define R3  ((Register)(r3_RegisterEnumValue))
#define R4  ((Register)(r4_RegisterEnumValue))
#define R5  ((Register)(r5_RegisterEnumValue))
#define R6  ((Register)(r6_RegisterEnumValue))
#define R7  ((Register)(r7_RegisterEnumValue))
#define R8  ((Register)(r8_RegisterEnumValue))
#define R9  ((Register)(r9_RegisterEnumValue))
#define R10 ((Register)(r10_RegisterEnumValue))
#define R11 ((Register)(r11_RegisterEnumValue))
#define R12 ((Register)(r12_RegisterEnumValue))
#define R13 ((Register)(r13_RegisterEnumValue))
#define R14 ((Register)(r14_RegisterEnumValue))
#define R15 ((Register)(r15_RegisterEnumValue))
#define R16 ((Register)(r16_RegisterEnumValue))
#define R17 ((Register)(r17_RegisterEnumValue))
#define R18 ((Register)(r18_RegisterEnumValue))
#define R19 ((Register)(r19_RegisterEnumValue))
#define R20 ((Register)(r20_RegisterEnumValue))
#define R21 ((Register)(r21_RegisterEnumValue))
#define R22 ((Register)(r22_RegisterEnumValue))
#define R23 ((Register)(r23_RegisterEnumValue))
#define R24 ((Register)(r24_RegisterEnumValue))
#define R25 ((Register)(r25_RegisterEnumValue))
#define R26 ((Register)(r26_RegisterEnumValue))
#define R27 ((Register)(r27_RegisterEnumValue))
#define R28 ((Register)(r28_RegisterEnumValue))
#define R29 ((Register)(r29_RegisterEnumValue))
#define R30 ((Register)(r30_RegisterEnumValue))
#define R31 ((Register)(r31_RegisterEnumValue))


#define RA           R1
#define TP           R2
#define SP           R3
#define RA0          R4
#define RA1          R5
#define RA2          R6
#define RA3          R7
#define RA4          R8
#define RA5          R9
#define RA6          R10
#define RA7          R11
#define RT0          R12
#define RT1          R13
#define RT2          R14
#define RT3          R15
#define RT4          R16
#define RT5          R17
#define RT6          R18
#define RT7          R19
#define RT8          R20
#define RX           R21
#define FP           R22
#define S0           R23
#define S1           R24
#define S2           R25
#define S3           R26
#define S4           R27
#define S5           R28
#define S6           R29
#define S7           R30
#define S8           R31

#define c_rarg0       RT0
#define c_rarg1       RT1
#define Rmethod       S3
#define Rsender       S4
#define Rnext         S1

#define V0       RA0
#define V1       RA1

#define SCR1     RT7
#define SCR2     RT4

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

#define S5_heapbase    S5

#define FSR            V0
#define SSR            T6
#define FSF            FV0

#define RECEIVER       T0
#define IC_Klass       T1

#define SHIFT_count    T3

// ---------- Scratch Register ----------
#define AT             RT7
#define fscratch       F23

#endif // DONT_USE_REGISTER_DEFINES

// Use FloatRegister as shortcut
class FloatRegisterImpl;
typedef FloatRegisterImpl* FloatRegister;

inline FloatRegister as_FloatRegister(int encoding) {
  return (FloatRegister)(intptr_t) encoding;
}

// The implementation of floating point registers for the LoongArch architecture
class FloatRegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
    number_of_registers     = 32,
    save_slots_per_register = 2,
    slots_per_lsx_register  = 4,
    slots_per_lasx_register = 8,
    max_slots_per_register  = 8
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

#define FA0    F0
#define FA1    F1
#define FA2    F2
#define FA3    F3
#define FA4    F4
#define FA5    F5
#define FA6    F6
#define FA7    F7

#define FV0    F0
#define FV1    F1

#define FT0    F8
#define FT1    F9
#define FT2    F10
#define FT3    F11
#define FT4    F12
#define FT5    F13
#define FT6    F14
#define FT7    F15
#define FT8    F16
#define FT9    F17
#define FT10   F18
#define FT11   F19
#define FT12   F20
#define FT13   F21
#define FT14   F22
#define FT15   F23

#define FS0    F24
#define FS1    F25
#define FS2    F26
#define FS3    F27
#define FS4    F28
#define FS5    F29
#define FS6    F30
#define FS7    F31

#endif // DONT_USE_REGISTER_DEFINES

// Use ConditionalFlagRegister as shortcut
class ConditionalFlagRegisterImpl;
typedef ConditionalFlagRegisterImpl* ConditionalFlagRegister;

inline ConditionalFlagRegister as_ConditionalFlagRegister(int encoding) {
  return (ConditionalFlagRegister)(intptr_t) encoding;
}

// The implementation of floating point registers for the LoongArch architecture
class ConditionalFlagRegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
//    conditionalflag_arg_base      = 12,
    number_of_registers = 8
  };

  // construction
  inline friend ConditionalFlagRegister as_ConditionalFlagRegister(int encoding);

  VMReg as_VMReg();

  // derived registers, offsets, and addresses
  ConditionalFlagRegister successor() const                          { return as_ConditionalFlagRegister(encoding() + 1); }

  // accessors
  int   encoding() const                          { assert(is_valid(), "invalid register"); return (intptr_t)this; }
  bool  is_valid() const                          { return 0 <= (intptr_t)this && (intptr_t)this < number_of_registers; }
  const char* name() const;

};

CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fccnoreg , (-1));

CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fcc0     , ( 0));
CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fcc1     , ( 1));
CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fcc2     , ( 2));
CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fcc3     , ( 3));
CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fcc4     , ( 4));
CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fcc5     , ( 5));
CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fcc6     , ( 6));
CONSTANT_REGISTER_DECLARATION(ConditionalFlagRegister, fcc7     , ( 7));

#ifndef DONT_USE_REGISTER_DEFINES
#define FCCNOREG ((ConditionalFlagRegister)(fccnoreg_ConditionalFlagRegisterEnumValue))
#define FCC0     ((ConditionalFlagRegister)(    fcc0_ConditionalFlagRegisterEnumValue))
#define FCC1     ((ConditionalFlagRegister)(    fcc1_ConditionalFlagRegisterEnumValue))
#define FCC2     ((ConditionalFlagRegister)(    fcc2_ConditionalFlagRegisterEnumValue))
#define FCC3     ((ConditionalFlagRegister)(    fcc3_ConditionalFlagRegisterEnumValue))
#define FCC4     ((ConditionalFlagRegister)(    fcc4_ConditionalFlagRegisterEnumValue))
#define FCC5     ((ConditionalFlagRegister)(    fcc5_ConditionalFlagRegisterEnumValue))
#define FCC6     ((ConditionalFlagRegister)(    fcc6_ConditionalFlagRegisterEnumValue))
#define FCC7     ((ConditionalFlagRegister)(    fcc7_ConditionalFlagRegisterEnumValue))

#endif // DONT_USE_REGISTER_DEFINES

// Need to know the total number of registers of all sorts for SharedInfo.
// Define a class that exports it.
class ConcreteRegisterImpl : public AbstractRegisterImpl {
 public:
  enum {
  // A big enough number for C2: all the registers plus flags
  // This number must be large enough to cover REG_COUNT (defined by c2) registers.
  // There is no requirement that any ordering here matches any ordering c2 gives
  // it's optoregs.
    number_of_registers = RegisterImpl::max_slots_per_register * RegisterImpl::number_of_registers +
                          FloatRegisterImpl::max_slots_per_register * FloatRegisterImpl::number_of_registers
  };

  static const int max_gpr;
  static const int max_fpr;


};

#endif //CPU_LOONGARCH_VM_REGISTER_LOONGARCH_HPP
