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

#ifndef CPU_LOONGARCH_C1_FRAMEMAP_LOONGARCH_HPP
#define CPU_LOONGARCH_C1_FRAMEMAP_LOONGARCH_HPP

//  On LoongArch64 the frame looks as follows:
//
//  +-----------------------------+---------+----------------------------------------+----------------+-----------
//  | size_arguments-nof_reg_args | 2 words | size_locals-size_arguments+numreg_args | _size_monitors | spilling .
//  +-----------------------------+---------+----------------------------------------+----------------+-----------

 public:
  static const int pd_c_runtime_reserved_arg_size;

  enum {
    first_available_sp_in_frame = 0,
    frame_pad_in_bytes = 16,
    nof_reg_args = 8
  };

 public:
  static LIR_Opr receiver_opr;

  static LIR_Opr r0_opr;
  static LIR_Opr ra_opr;
  static LIR_Opr tp_opr;
  static LIR_Opr sp_opr;
  static LIR_Opr a0_opr;
  static LIR_Opr a1_opr;
  static LIR_Opr a2_opr;
  static LIR_Opr a3_opr;
  static LIR_Opr a4_opr;
  static LIR_Opr a5_opr;
  static LIR_Opr a6_opr;
  static LIR_Opr a7_opr;
  static LIR_Opr t0_opr;
  static LIR_Opr t1_opr;
  static LIR_Opr t2_opr;
  static LIR_Opr t3_opr;
  static LIR_Opr t4_opr;
  static LIR_Opr t5_opr;
  static LIR_Opr t6_opr;
  static LIR_Opr t7_opr;
  static LIR_Opr t8_opr;
  static LIR_Opr rx_opr;
  static LIR_Opr fp_opr;
  static LIR_Opr s0_opr;
  static LIR_Opr s1_opr;
  static LIR_Opr s2_opr;
  static LIR_Opr s3_opr;
  static LIR_Opr s4_opr;
  static LIR_Opr s5_opr;
  static LIR_Opr s6_opr;
  static LIR_Opr s7_opr;
  static LIR_Opr s8_opr;

  static LIR_Opr ra_oop_opr;
  static LIR_Opr a0_oop_opr;
  static LIR_Opr a1_oop_opr;
  static LIR_Opr a2_oop_opr;
  static LIR_Opr a3_oop_opr;
  static LIR_Opr a4_oop_opr;
  static LIR_Opr a5_oop_opr;
  static LIR_Opr a6_oop_opr;
  static LIR_Opr a7_oop_opr;
  static LIR_Opr t0_oop_opr;
  static LIR_Opr t1_oop_opr;
  static LIR_Opr t2_oop_opr;
  static LIR_Opr t3_oop_opr;
  static LIR_Opr t4_oop_opr;
  static LIR_Opr t5_oop_opr;
  static LIR_Opr t6_oop_opr;
  static LIR_Opr t7_oop_opr;
  static LIR_Opr t8_oop_opr;
  static LIR_Opr fp_oop_opr;
  static LIR_Opr s0_oop_opr;
  static LIR_Opr s1_oop_opr;
  static LIR_Opr s2_oop_opr;
  static LIR_Opr s3_oop_opr;
  static LIR_Opr s4_oop_opr;
  static LIR_Opr s5_oop_opr;
  static LIR_Opr s6_oop_opr;
  static LIR_Opr s7_oop_opr;
  static LIR_Opr s8_oop_opr;

  static LIR_Opr scr1_opr;
  static LIR_Opr scr2_opr;
  static LIR_Opr scr1_long_opr;
  static LIR_Opr scr2_long_opr;

  static LIR_Opr a0_metadata_opr;
  static LIR_Opr a1_metadata_opr;
  static LIR_Opr a2_metadata_opr;
  static LIR_Opr a3_metadata_opr;
  static LIR_Opr a4_metadata_opr;
  static LIR_Opr a5_metadata_opr;

  static LIR_Opr long0_opr;
  static LIR_Opr long1_opr;
  static LIR_Opr fpu0_float_opr;
  static LIR_Opr fpu0_double_opr;

  static LIR_Opr as_long_opr(Register r) {
    return LIR_OprFact::double_cpu(cpu_reg2rnr(r), cpu_reg2rnr(r));
  }
  static LIR_Opr as_pointer_opr(Register r) {
    return LIR_OprFact::double_cpu(cpu_reg2rnr(r), cpu_reg2rnr(r));
  }

  // VMReg name for spilled physical FPU stack slot n
  static VMReg fpu_regname (int n);

  static bool is_caller_save_register(LIR_Opr opr) { return true; }
  static bool is_caller_save_register(Register r) { return true; }

  static int nof_caller_save_cpu_regs() { return pd_nof_caller_save_cpu_regs_frame_map; }
  static int last_cpu_reg() { return pd_last_cpu_reg;  }
  static int last_byte_reg() { return pd_last_byte_reg; }

#endif // CPU_LOONGARCH_C1_FRAMEMAP_LOONGARCH_HPP
