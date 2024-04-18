/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2019, Loongson Technology. All rights reserved.
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

#ifndef CPU_MIPS_VM_VM_VERSION_MIPS_HPP
#define CPU_MIPS_VM_VM_VERSION_MIPS_HPP

#include "runtime/globals_extension.hpp"
#include "runtime/vm_version.hpp"


class VM_Version: public Abstract_VM_Version {
public:

  union Loongson_Cpucfg_Id1 {
    uint32_t value;
    struct {
      uint32_t FP      : 1,
               FPREV   : 3,
               MMI     : 1,
               MSA1    : 1,
               MSA2    : 1,
               CGP     : 1,
               WRP     : 1,
               LSX1    : 1,
               LSX2    : 1,
               LASX    : 1,
               R6FXP   : 1,
               R6CRCP  : 1,
               R6FPP   : 1,
               CNT64   : 1,
               LSLDR0  : 1,
               LSPREF  : 1,
               LSPREFX : 1,
               LSSYNCI : 1,
               LSUCA   : 1,
               LLSYNC  : 1,
               TGTSYNC : 1,
               LLEXC   : 1,
               SCRAND  : 1,
               MUALP   : 1,
               KMUALEn : 1,
               ITLBT   : 1,
               LSUPERF : 1,
               SFBP    : 1,
               CDMAP   : 1,
                       : 1;
    } bits;
  };

  union Loongson_Cpucfg_Id2 {
    uint32_t value;
    struct {
      uint32_t LEXT1    : 1,
               LEXT2    : 1,
               LEXT3    : 1,
               LSPW     : 1,
               LBT1     : 1,
               LBT2     : 1,
               LBT3     : 1,
               LBTMMU   : 1,
               LPMP     : 1,
               LPMRev   : 3,
               LAMO     : 1,
               LPIXU    : 1,
               LPIXNU   : 1,
               LVZP     : 1,
               LVZRev   : 3,
               LGFTP    : 1,
               LGFTRev  : 3,
               LLFTP    : 1,
               LLFTRev  : 3,
               LCSRP    : 1,
               DISBLKLY : 1,
                        : 3;
    } bits;
  };

protected:

  enum {
    CPU_LOONGSON          = (1 << 1),
    CPU_LOONGSON_GS464    = (1 << 2),
    CPU_LOONGSON_GS464E   = (1 << 3),
    CPU_LOONGSON_GS264    = (1 << 4),
    CPU_MMI               = (1 << 11),
    CPU_MSA1_0            = (1 << 12),
    CPU_MSA2_0            = (1 << 13),
    CPU_CGP               = (1 << 14),
    CPU_LSX1              = (1 << 15),
    CPU_LSX2              = (1 << 16),
    CPU_LASX              = (1 << 17),
    CPU_LEXT1             = (1 << 18),
    CPU_LEXT2             = (1 << 19),
    CPU_LEXT3             = (1 << 20),
    CPU_LAMO              = (1 << 21),
    CPU_LPIXU             = (1 << 22),
    CPU_LLSYNC            = (1 << 23),
    CPU_TGTSYNC           = (1 << 24),
    CPU_ULSYNC           = (1 << 25),
    CPU_MUALP             = (1 << 26),

    //////////////////////add some other feature here//////////////////
  } cpuFeatureFlags;

  enum Loongson_Family {
    L_3A1000    = 0,
    L_3B1500    = 1,
    L_3A2000    = 2,
    L_3B2000    = 3,
    L_3A3000    = 4,
    L_3B3000    = 5,
    L_2K1000    = 6,
    L_UNKNOWN   = 7
  };

  struct Loongson_Cpuinfo {
    Loongson_Family    id;
    const char* const  match_str;
  };

  static int  _cpuFeatures;
  static const char* _features_str;
  static volatile bool _is_determine_cpucfg_supported_running;
  static bool _is_cpucfg_instruction_supported;
  static bool _cpu_info_is_initialized;

  struct CpuidInfo {
    uint32_t            cpucfg_info_id0;
    Loongson_Cpucfg_Id1 cpucfg_info_id1;
    Loongson_Cpucfg_Id2 cpucfg_info_id2;
    uint32_t            cpucfg_info_id3;
    uint32_t            cpucfg_info_id4;
    uint32_t            cpucfg_info_id5;
    uint32_t            cpucfg_info_id6;
    uint32_t            cpucfg_info_id8;
  };

  // The actual cpuid info block
  static CpuidInfo _cpuid_info;

  static uint32_t get_feature_flags_by_cpucfg();
  static int      get_feature_flags_by_cpuinfo(int features);
  static void     get_processor_features();

public:
  // Offsets for cpuid asm stub
  static ByteSize Loongson_Cpucfg_id0_offset() { return byte_offset_of(CpuidInfo, cpucfg_info_id0); }
  static ByteSize Loongson_Cpucfg_id1_offset() { return byte_offset_of(CpuidInfo, cpucfg_info_id1); }
  static ByteSize Loongson_Cpucfg_id2_offset() { return byte_offset_of(CpuidInfo, cpucfg_info_id2); }
  static ByteSize Loongson_Cpucfg_id3_offset() { return byte_offset_of(CpuidInfo, cpucfg_info_id3); }
  static ByteSize Loongson_Cpucfg_id4_offset() { return byte_offset_of(CpuidInfo, cpucfg_info_id4); }
  static ByteSize Loongson_Cpucfg_id5_offset() { return byte_offset_of(CpuidInfo, cpucfg_info_id5); }
  static ByteSize Loongson_Cpucfg_id6_offset() { return byte_offset_of(CpuidInfo, cpucfg_info_id6); }
  static ByteSize Loongson_Cpucfg_id8_offset() { return byte_offset_of(CpuidInfo, cpucfg_info_id8); }

  static bool is_determine_features_test_running() { return _is_determine_cpucfg_supported_running; }

  static void clean_cpuFeatures()   { _cpuFeatures = 0; }

  // Initialization
  static void initialize();

  static bool cpu_info_is_initialized()                   { return _cpu_info_is_initialized; }

  static bool supports_cpucfg()                  { return _is_cpucfg_instruction_supported; }
  static bool set_supports_cpucfg(bool value)    { return _is_cpucfg_instruction_supported = value; }

  static bool is_loongson()      { return _cpuFeatures & CPU_LOONGSON; }
  static bool is_gs264()         { return _cpuFeatures & CPU_LOONGSON_GS264; }
  static bool is_gs464()         { return _cpuFeatures & CPU_LOONGSON_GS464; }
  static bool is_gs464e()        { return _cpuFeatures & CPU_LOONGSON_GS464E; }
  static bool supports_dsp()     { return 0; /*not supported yet*/}
  static bool supports_ps()      { return 0; /*not supported yet*/}
  static bool supports_3d()      { return 0; /*not supported yet*/}
  static bool supports_msa1_0()  { return _cpuFeatures & CPU_MSA1_0; }
  static bool supports_msa2_0()  { return _cpuFeatures & CPU_MSA2_0; }
  static bool supports_cgp()     { return _cpuFeatures & CPU_CGP; }
  static bool supports_mmi()     { return _cpuFeatures & CPU_MMI; }
  static bool supports_lsx1()    { return _cpuFeatures & CPU_LSX1; }
  static bool supports_lsx2()    { return _cpuFeatures & CPU_LSX2; }
  static bool supports_lasx()    { return _cpuFeatures & CPU_LASX; }
  static bool supports_lext1()   { return _cpuFeatures & CPU_LEXT1; }
  static bool supports_lext2()   { return _cpuFeatures & CPU_LEXT2; }
  static bool supports_lext3()   { return _cpuFeatures & CPU_LEXT3; }
  static bool supports_lamo()    { return _cpuFeatures & CPU_LAMO; }
  static bool supports_lpixu()   { return _cpuFeatures & CPU_LPIXU; }
  static bool needs_llsync()     { return _cpuFeatures & CPU_LLSYNC; }
  static bool needs_tgtsync()    { return _cpuFeatures & CPU_TGTSYNC; }
  static bool needs_ulsync()     { return _cpuFeatures & CPU_ULSYNC; }
  static bool supports_mualp()   { return _cpuFeatures & CPU_MUALP; }

  //mips has no such instructions, use ll/sc instead
  static bool supports_compare_and_exchange() { return false; }

  static const char* cpu_features()           { return _features_str; }

};

#endif // CPU_MIPS_VM_VM_VERSION_MIPS_HPP
