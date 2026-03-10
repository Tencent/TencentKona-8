/*
 * Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, Loongson Technology. All rights reserved.
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
#include "c1/c1_FrameMap.hpp"
#include "c1/c1_LIR.hpp"
#include "runtime/sharedRuntime.hpp"
#include "vmreg_loongarch.inline.hpp"

#define A0 RA0
#define A1 RA1
#define A2 RA2
#define A3 RA3
#define A4 RA4
#define A5 RA5
#define A6 RA6
#define A7 RA7
#define T0 RT0
#define T1 RT1
#define T2 RT2
#define T3 RT3
#define T4 RT4
#define T5 RT5
#define T6 RT6
#define T7 RT7
#define T8 RT8

LIR_Opr FrameMap::map_to_opr(BasicType type, VMRegPair* reg, bool) {
  LIR_Opr opr = LIR_OprFact::illegalOpr;
  VMReg r_1 = reg->first();
  VMReg r_2 = reg->second();
  if (r_1->is_stack()) {
    // Convert stack slot to an SP offset
    // The calling convention does not count the SharedRuntime::out_preserve_stack_slots() value
    // so we must add it in here.
    int st_off = (r_1->reg2stack() + SharedRuntime::out_preserve_stack_slots()) * VMRegImpl::stack_slot_size;
    opr = LIR_OprFact::address(new LIR_Address(sp_opr, st_off, type));
  } else if (r_1->is_Register()) {
    Register reg = r_1->as_Register();
    if (r_2->is_Register() && (type == T_LONG || type == T_DOUBLE)) {
      Register reg2 = r_2->as_Register();
      assert(reg2 == reg, "must be same register");
      opr = as_long_opr(reg);
    } else if (is_reference_type(type)) {
      opr = as_oop_opr(reg);
    } else if (type == T_METADATA) {
      opr = as_metadata_opr(reg);
    } else if (type == T_ADDRESS) {
      opr = as_address_opr(reg);
    } else {
      opr = as_opr(reg);
    }
  } else if (r_1->is_FloatRegister()) {
    assert(type == T_DOUBLE || type == T_FLOAT, "wrong type");
    int num = r_1->as_FloatRegister()->encoding();
    if (type == T_FLOAT) {
      opr = LIR_OprFact::single_fpu(num);
    } else {
      opr = LIR_OprFact::double_fpu(num);
    }
  } else {
    ShouldNotReachHere();
  }
  return opr;
}

LIR_Opr FrameMap::r0_opr;
LIR_Opr FrameMap::ra_opr;
LIR_Opr FrameMap::tp_opr;
LIR_Opr FrameMap::sp_opr;
LIR_Opr FrameMap::a0_opr;
LIR_Opr FrameMap::a1_opr;
LIR_Opr FrameMap::a2_opr;
LIR_Opr FrameMap::a3_opr;
LIR_Opr FrameMap::a4_opr;
LIR_Opr FrameMap::a5_opr;
LIR_Opr FrameMap::a6_opr;
LIR_Opr FrameMap::a7_opr;
LIR_Opr FrameMap::t0_opr;
LIR_Opr FrameMap::t1_opr;
LIR_Opr FrameMap::t2_opr;
LIR_Opr FrameMap::t3_opr;
LIR_Opr FrameMap::t4_opr;
LIR_Opr FrameMap::t5_opr;
LIR_Opr FrameMap::t6_opr;
LIR_Opr FrameMap::t7_opr;
LIR_Opr FrameMap::t8_opr;
LIR_Opr FrameMap::rx_opr;
LIR_Opr FrameMap::fp_opr;
LIR_Opr FrameMap::s0_opr;
LIR_Opr FrameMap::s1_opr;
LIR_Opr FrameMap::s2_opr;
LIR_Opr FrameMap::s3_opr;
LIR_Opr FrameMap::s4_opr;
LIR_Opr FrameMap::s5_opr;
LIR_Opr FrameMap::s6_opr;
LIR_Opr FrameMap::s7_opr;
LIR_Opr FrameMap::s8_opr;

LIR_Opr FrameMap::receiver_opr;

LIR_Opr FrameMap::ra_oop_opr;
LIR_Opr FrameMap::a0_oop_opr;
LIR_Opr FrameMap::a1_oop_opr;
LIR_Opr FrameMap::a2_oop_opr;
LIR_Opr FrameMap::a3_oop_opr;
LIR_Opr FrameMap::a4_oop_opr;
LIR_Opr FrameMap::a5_oop_opr;
LIR_Opr FrameMap::a6_oop_opr;
LIR_Opr FrameMap::a7_oop_opr;
LIR_Opr FrameMap::t0_oop_opr;
LIR_Opr FrameMap::t1_oop_opr;
LIR_Opr FrameMap::t2_oop_opr;
LIR_Opr FrameMap::t3_oop_opr;
LIR_Opr FrameMap::t4_oop_opr;
LIR_Opr FrameMap::t5_oop_opr;
LIR_Opr FrameMap::t6_oop_opr;
LIR_Opr FrameMap::t7_oop_opr;
LIR_Opr FrameMap::t8_oop_opr;
LIR_Opr FrameMap::fp_oop_opr;
LIR_Opr FrameMap::s0_oop_opr;
LIR_Opr FrameMap::s1_oop_opr;
LIR_Opr FrameMap::s2_oop_opr;
LIR_Opr FrameMap::s3_oop_opr;
LIR_Opr FrameMap::s4_oop_opr;
LIR_Opr FrameMap::s5_oop_opr;
LIR_Opr FrameMap::s6_oop_opr;
LIR_Opr FrameMap::s7_oop_opr;
LIR_Opr FrameMap::s8_oop_opr;

LIR_Opr FrameMap::scr1_opr;
LIR_Opr FrameMap::scr2_opr;
LIR_Opr FrameMap::scr1_long_opr;
LIR_Opr FrameMap::scr2_long_opr;

LIR_Opr FrameMap::a0_metadata_opr;
LIR_Opr FrameMap::a1_metadata_opr;
LIR_Opr FrameMap::a2_metadata_opr;
LIR_Opr FrameMap::a3_metadata_opr;
LIR_Opr FrameMap::a4_metadata_opr;
LIR_Opr FrameMap::a5_metadata_opr;

LIR_Opr FrameMap::long0_opr;
LIR_Opr FrameMap::long1_opr;
LIR_Opr FrameMap::fpu0_float_opr;
LIR_Opr FrameMap::fpu0_double_opr;

LIR_Opr FrameMap::_caller_save_cpu_regs[] = { 0 };
LIR_Opr FrameMap::_caller_save_fpu_regs[] = { 0 };

//--------------------------------------------------------
//               FrameMap
//--------------------------------------------------------

void FrameMap::initialize() {
  assert(!_init_done, "once");
  int i = 0;

  // caller save register
  map_register(i, A0); a0_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, A1); a1_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, A2); a2_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, A3); a3_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, A4); a4_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, A5); a5_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, A6); a6_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, A7); a7_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, T0); t0_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, T1); t1_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, T2); t2_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, T3); t3_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, T5); t5_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, T6); t6_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, T8); t8_opr = LIR_OprFact::single_cpu(i); i++;

  // callee save register
  map_register(i, S0); s0_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, S1); s1_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, S2); s2_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, S3); s3_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, S4); s4_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, S7); s7_opr = LIR_OprFact::single_cpu(i); i++;
  map_register(i, S8); s8_opr = LIR_OprFact::single_cpu(i); i++;

  // special register
  map_register(i, S5); s5_opr = LIR_OprFact::single_cpu(i); i++; // heapbase
  map_register(i, S6); s6_opr = LIR_OprFact::single_cpu(i); i++; // thread
  map_register(i, TP); tp_opr = LIR_OprFact::single_cpu(i); i++; // tp
  map_register(i, FP); fp_opr = LIR_OprFact::single_cpu(i); i++; // fp
  map_register(i, RA); ra_opr = LIR_OprFact::single_cpu(i); i++; // ra
  map_register(i, SP); sp_opr = LIR_OprFact::single_cpu(i); i++; // sp

  // tmp register
  map_register(i, T7); t7_opr = LIR_OprFact::single_cpu(i); i++; // scr1
  map_register(i, T4); t4_opr = LIR_OprFact::single_cpu(i); i++; // scr2

  scr1_opr = t7_opr;
  scr2_opr = t4_opr;
  scr1_long_opr = LIR_OprFact::double_cpu(t7_opr->cpu_regnr(), t7_opr->cpu_regnr());
  scr2_long_opr = LIR_OprFact::double_cpu(t4_opr->cpu_regnr(), t4_opr->cpu_regnr());

  long0_opr = LIR_OprFact::double_cpu(a0_opr->cpu_regnr(), a0_opr->cpu_regnr());
  long1_opr = LIR_OprFact::double_cpu(a1_opr->cpu_regnr(), a1_opr->cpu_regnr());

  fpu0_float_opr   = LIR_OprFact::single_fpu(0);
  fpu0_double_opr  = LIR_OprFact::double_fpu(0);

  // scr1, scr2 not included
  _caller_save_cpu_regs[0] = a0_opr;
  _caller_save_cpu_regs[1] = a1_opr;
  _caller_save_cpu_regs[2] = a2_opr;
  _caller_save_cpu_regs[3] = a3_opr;
  _caller_save_cpu_regs[4] = a4_opr;
  _caller_save_cpu_regs[5] = a5_opr;
  _caller_save_cpu_regs[6] = a6_opr;
  _caller_save_cpu_regs[7] = a7_opr;
  _caller_save_cpu_regs[8] = t0_opr;
  _caller_save_cpu_regs[9] = t1_opr;
  _caller_save_cpu_regs[10] = t2_opr;
  _caller_save_cpu_regs[11] = t3_opr;
  _caller_save_cpu_regs[12] = t5_opr;
  _caller_save_cpu_regs[13] = t6_opr;
  _caller_save_cpu_regs[14] = t8_opr;

  for (int i = 0; i < 8; i++) {
    _caller_save_fpu_regs[i] = LIR_OprFact::single_fpu(i);
  }

  _init_done = true;

  ra_oop_opr = as_oop_opr(RA);
  a0_oop_opr = as_oop_opr(A0);
  a1_oop_opr = as_oop_opr(A1);
  a2_oop_opr = as_oop_opr(A2);
  a3_oop_opr = as_oop_opr(A3);
  a4_oop_opr = as_oop_opr(A4);
  a5_oop_opr = as_oop_opr(A5);
  a6_oop_opr = as_oop_opr(A6);
  a7_oop_opr = as_oop_opr(A7);
  t0_oop_opr = as_oop_opr(T0);
  t1_oop_opr = as_oop_opr(T1);
  t2_oop_opr = as_oop_opr(T2);
  t3_oop_opr = as_oop_opr(T3);
  t4_oop_opr = as_oop_opr(T4);
  t5_oop_opr = as_oop_opr(T5);
  t6_oop_opr = as_oop_opr(T6);
  t7_oop_opr = as_oop_opr(T7);
  t8_oop_opr = as_oop_opr(T8);
  fp_oop_opr = as_oop_opr(FP);
  s0_oop_opr = as_oop_opr(S0);
  s1_oop_opr = as_oop_opr(S1);
  s2_oop_opr = as_oop_opr(S2);
  s3_oop_opr = as_oop_opr(S3);
  s4_oop_opr = as_oop_opr(S4);
  s5_oop_opr = as_oop_opr(S5);
  s6_oop_opr = as_oop_opr(S6);
  s7_oop_opr = as_oop_opr(S7);
  s8_oop_opr = as_oop_opr(S8);

  a0_metadata_opr = as_metadata_opr(A0);
  a1_metadata_opr = as_metadata_opr(A1);
  a2_metadata_opr = as_metadata_opr(A2);
  a3_metadata_opr = as_metadata_opr(A3);
  a4_metadata_opr = as_metadata_opr(A4);
  a5_metadata_opr = as_metadata_opr(A5);

  sp_opr = as_pointer_opr(SP);
  fp_opr = as_pointer_opr(FP);

  VMRegPair regs;
  BasicType sig_bt = T_OBJECT;
  SharedRuntime::java_calling_convention(&sig_bt, &regs, 1, true);
  receiver_opr = as_oop_opr(regs.first()->as_Register());

  for (int i = 0; i < nof_caller_save_fpu_regs; i++) {
    _caller_save_fpu_regs[i] = LIR_OprFact::single_fpu(i);
  }
}

Address FrameMap::make_new_address(ByteSize sp_offset) const {
  // for sp, based address use this:
  // return Address(sp, in_bytes(sp_offset) - (framesize() - 2) * 4);
  return Address(SP, in_bytes(sp_offset));
}

// ----------------mapping-----------------------
// all mapping is based on fp addressing, except for simple leaf methods where we access
// the locals sp based (and no frame is built)

// Frame for simple leaf methods (quick entries)
//
//   +----------+
//   | ret addr |   <- TOS
//   +----------+
//   | args     |
//   | ......   |

// Frame for standard methods
//
//   | .........|  <- TOS
//   | locals   |
//   +----------+
//   |  old fp, |  <- RFP
//   +----------+
//   | ret addr |
//   +----------+
//   |  args    |
//   | .........|

// For OopMaps, map a local variable or spill index to an VMRegImpl name.
// This is the offset from sp() in the frame of the slot for the index,
// skewed by VMRegImpl::stack0 to indicate a stack location (vs.a register.)
//
//           framesize +
//           stack0         stack0          0  <- VMReg
//             |              | <registers> |
//  ...........|..............|.............|
//      0 1 2 3 x x 4 5 6 ... |                <- local indices
//      ^           ^        sp()                 ( x x indicate link
//      |           |                               and return addr)
//  arguments   non-argument locals

VMReg FrameMap::fpu_regname(int n) {
  // Return the OptoReg name for the fpu stack slot "n"
  // A spilled fpu stack slot comprises to two single-word OptoReg's.
  return as_FloatRegister(n)->as_VMReg();
}

LIR_Opr FrameMap::stack_pointer() {
  return FrameMap::sp_opr;
}

// JSR 292
LIR_Opr FrameMap::method_handle_invoke_SP_save_opr() {
  return LIR_OprFact::illegalOpr;  // Not needed on LoongArch64
}

bool FrameMap::validate_frame() {
  return true;
}
