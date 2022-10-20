/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/java.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "vm_version_loongarch.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif

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

int VM_Version::_cpuFeatures;
const char* VM_Version::_features_str = "";
VM_Version::CpuidInfo VM_Version::_cpuid_info   = { 0, };
bool VM_Version::_cpu_info_is_initialized = false;

static BufferBlob* stub_blob;
static const int stub_size = 600;

extern "C" {
  typedef void (*get_cpu_info_stub_t)(void*);
}
static get_cpu_info_stub_t get_cpu_info_stub = NULL;


class VM_Version_StubGenerator: public StubCodeGenerator {
 public:

  VM_Version_StubGenerator(CodeBuffer *c) : StubCodeGenerator(c) {}

  address generate_get_cpu_info() {
    assert(!VM_Version::cpu_info_is_initialized(), "VM_Version should not be initialized");
    StubCodeMark mark(this, "VM_Version", "get_cpu_info_stub");
#   define __ _masm->

    address start = __ pc();

    __ enter();
    __ push(AT);
    __ push(T5);

    __ li(AT, (long)0);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id0_offset()));

    __ li(AT, 1);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id1_offset()));

    __ li(AT, 2);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id2_offset()));

    __ li(AT, 3);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id3_offset()));

    __ li(AT, 4);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id4_offset()));

    __ li(AT, 5);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id5_offset()));

    __ li(AT, 6);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id6_offset()));

    __ li(AT, 10);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id10_offset()));

    __ li(AT, 11);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id11_offset()));

    __ li(AT, 12);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id12_offset()));

    __ li(AT, 13);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id13_offset()));

    __ li(AT, 14);
    __ cpucfg(T5, AT);
    __ st_w(T5, A0, in_bytes(VM_Version::Loongson_Cpucfg_id14_offset()));

    __ pop(T5);
    __ pop(AT);
    __ leave();
    __ jr(RA);
#   undef __
    return start;
  };
};

uint32_t VM_Version::get_feature_flags_by_cpucfg() {
  uint32_t result = 0;
  if (_cpuid_info.cpucfg_info_id1.bits.ARCH == 0b00 || _cpuid_info.cpucfg_info_id1.bits.ARCH == 0b01 ) {
    result |= CPU_LA32;
  } else if (_cpuid_info.cpucfg_info_id1.bits.ARCH == 0b10 ) {
    result |= CPU_LA64;
  }
  if (_cpuid_info.cpucfg_info_id1.bits.UAL != 0)
    result |= CPU_UAL;

  if (_cpuid_info.cpucfg_info_id2.bits.FP_CFG != 0)
    result |= CPU_FP;
  if (_cpuid_info.cpucfg_info_id2.bits.LSX != 0)
    result |= CPU_LSX;
  if (_cpuid_info.cpucfg_info_id2.bits.LASX != 0)
    result |= CPU_LASX;
  if (_cpuid_info.cpucfg_info_id2.bits.COMPLEX != 0)
    result |= CPU_COMPLEX;
  if (_cpuid_info.cpucfg_info_id2.bits.CRYPTO != 0)
    result |= CPU_CRYPTO;
  if (_cpuid_info.cpucfg_info_id2.bits.LBT_X86 != 0)
    result |= CPU_LBT_X86;
  if (_cpuid_info.cpucfg_info_id2.bits.LBT_ARM != 0)
    result |= CPU_LBT_ARM;
  if (_cpuid_info.cpucfg_info_id2.bits.LBT_MIPS != 0)
    result |= CPU_LBT_MIPS;
  if (_cpuid_info.cpucfg_info_id2.bits.LAM != 0)
    result |= CPU_LAM;

  if (_cpuid_info.cpucfg_info_id3.bits.CCDMA != 0)
    result |= CPU_CCDMA;
  if (_cpuid_info.cpucfg_info_id3.bits.LLDBAR != 0)
    result |= CPU_LLDBAR;
  if (_cpuid_info.cpucfg_info_id3.bits.SCDLY != 0)
    result |= CPU_SCDLY;
  if (_cpuid_info.cpucfg_info_id3.bits.LLEXC != 0)
    result |= CPU_LLEXC;

  result |= CPU_ULSYNC;

  return result;
}

void VM_Version::get_processor_features() {

  clean_cpuFeatures();

  get_cpu_info_stub(&_cpuid_info);
  _cpuFeatures = get_feature_flags_by_cpucfg();

  _supports_cx8 = true;

  if (UseG1GC && FLAG_IS_DEFAULT(MaxGCPauseMillis)) {
    FLAG_SET_CMDLINE(uintx, MaxGCPauseMillis, 650);
  }

  if (supports_lsx()) {
    if (FLAG_IS_DEFAULT(UseLSX)) {
      FLAG_SET_DEFAULT(UseLSX, true);
    }
  } else if (UseLSX) {
    warning("LSX instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseLSX, false);
  }

  if (supports_lasx()) {
    if (FLAG_IS_DEFAULT(UseLASX)) {
      FLAG_SET_DEFAULT(UseLASX, true);
    }
  } else if (UseLASX) {
    warning("LASX instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseLASX, false);
  }

  if (UseLASX && !UseLSX) {
    warning("LASX instructions depends on LSX, setting UseLASX to false");
    FLAG_SET_DEFAULT(UseLASX, false);
  }

#ifdef COMPILER2
  int max_vector_size = 0;
  int min_vector_size = 0;
  if (UseLASX) {
    max_vector_size = 32;
    min_vector_size = 16;
  }
  else if (UseLSX) {
    max_vector_size = 16;
    min_vector_size = 16;
  }

  if (!FLAG_IS_DEFAULT(MaxVectorSize)) {
    if (MaxVectorSize == 0) {
      // do nothing
    } else if (MaxVectorSize > max_vector_size) {
      warning("MaxVectorSize must be at most %i on this platform", max_vector_size);
      FLAG_SET_DEFAULT(MaxVectorSize, max_vector_size);
    } else if (MaxVectorSize < min_vector_size) {
      warning("MaxVectorSize must be at least %i or 0 on this platform, setting to: %i", min_vector_size, min_vector_size);
      FLAG_SET_DEFAULT(MaxVectorSize, min_vector_size);
    } else if (!is_power_of_2(MaxVectorSize)) {
      warning("MaxVectorSize must be a power of 2, setting to default: %i", max_vector_size);
      FLAG_SET_DEFAULT(MaxVectorSize, max_vector_size);
    }
  } else {
    // If default, use highest supported configuration
    FLAG_SET_DEFAULT(MaxVectorSize, max_vector_size);
  }
#endif

  if (needs_llsync() && needs_tgtsync() && !needs_ulsync()) {
    if (FLAG_IS_DEFAULT(UseSyncLevel)) {
      FLAG_SET_DEFAULT(UseSyncLevel, 1000);
    }
  } else if (!needs_llsync() && needs_tgtsync() && needs_ulsync()) {
    if (FLAG_IS_DEFAULT(UseSyncLevel)) {
      FLAG_SET_DEFAULT(UseSyncLevel, 2000);
    }
  } else if (!needs_llsync() && !needs_tgtsync() && needs_ulsync()) {
    if (FLAG_IS_DEFAULT(UseSyncLevel)) {
      FLAG_SET_DEFAULT(UseSyncLevel, 3000);
    }
  } else if (needs_llsync() && !needs_tgtsync() && needs_ulsync()) {
    if (FLAG_IS_DEFAULT(UseSyncLevel)) {
      FLAG_SET_DEFAULT(UseSyncLevel, 4000);
    }
  } else if (needs_llsync() && needs_tgtsync() && needs_ulsync()) {
    if (FLAG_IS_DEFAULT(UseSyncLevel)) {
      FLAG_SET_DEFAULT(UseSyncLevel, 10000);
    }
  } else {
    assert(false, "Should Not Reach Here, what is the cpu type?");
    if (FLAG_IS_DEFAULT(UseSyncLevel)) {
      FLAG_SET_DEFAULT(UseSyncLevel, 10000);
    }
  }

  char buf[256];

  // A note on the _features_string format:
  //   There are jtreg tests checking the _features_string for various properties.
  //   For some strange reason, these tests require the string to contain
  //   only _lowercase_ characters. Keep that in mind when being surprised
  //   about the unusual notation of features - and when adding new ones.
  //   Features may have one comma at the end.
  //   Furthermore, use one, and only one, separator space between features.
  //   Multiple spaces are considered separate tokens, messing up everything.
  jio_snprintf(buf, sizeof(buf), "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s, "
    "0x%lx, fp_ver: %d, lvz_ver: %d, "
    "usesynclevel:%d",
    (is_la64()             ?  "la64"  : ""),
    (is_la32()             ?  "la32"  : ""),
    (supports_lsx()        ?  ", lsx" : ""),
    (supports_lasx()       ?  ", lasx" : ""),
    (supports_crypto()     ?  ", crypto" : ""),
    (supports_lam()        ?  ", am" : ""),
    (supports_ual()        ?  ", ual" : ""),
    (supports_lldbar()     ?  ", lldbar" : ""),
    (supports_scdly()      ?  ", scdly" : ""),
    (supports_llexc()      ?  ", llexc" : ""),
    (supports_lbt_x86()    ?  ", lbt_x86" : ""),
    (supports_lbt_arm()    ?  ", lbt_arm" : ""),
    (supports_lbt_mips()   ?  ", lbt_mips" : ""),
    (needs_llsync()        ?  ", needs_llsync" : ""),
    (needs_tgtsync()       ?  ", needs_tgtsync": ""),
    (needs_ulsync()        ?  ", needs_ulsync": ""),
    _cpuid_info.cpucfg_info_id0.bits.PRID,
    _cpuid_info.cpucfg_info_id2.bits.FP_VER,
    _cpuid_info.cpucfg_info_id2.bits.LVZ_VER,
    UseSyncLevel);
  _features_str = strdup(buf);

  assert(!is_la32(), "Should Not Reach Here, what is the cpu type?");
  assert( is_la64(), "Should be LoongArch64");

  if (FLAG_IS_DEFAULT(AllocatePrefetchStyle)) {
    FLAG_SET_DEFAULT(AllocatePrefetchStyle, 1);
  }

  if (FLAG_IS_DEFAULT(AllocatePrefetchLines)) {
    FLAG_SET_DEFAULT(AllocatePrefetchLines, 3);
  }

  if (FLAG_IS_DEFAULT(AllocatePrefetchStepSize)) {
    FLAG_SET_DEFAULT(AllocatePrefetchStepSize, 64);
  }

  if (FLAG_IS_DEFAULT(AllocatePrefetchDistance)) {
    FLAG_SET_DEFAULT(AllocatePrefetchDistance, 192);
  }

  if (FLAG_IS_DEFAULT(AllocateInstancePrefetchLines)) {
    FLAG_SET_DEFAULT(AllocateInstancePrefetchLines, 1);
  }

  // Basic instructions are used to implement SHA Intrinsics on LA, so sha
  // instructions support is not needed.
  if (/*supports_crypto()*/ 1) {
    if (FLAG_IS_DEFAULT(UseSHA)) {
      FLAG_SET_DEFAULT(UseSHA, true);
    }
  } else if (UseSHA) {
    warning("SHA instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseSHA, false);
  }

  if (UseSHA/* && supports_crypto()*/) {
    if (FLAG_IS_DEFAULT(UseSHA1Intrinsics)) {
      FLAG_SET_DEFAULT(UseSHA1Intrinsics, true);
    }
  } else if (UseSHA1Intrinsics) {
    warning("Intrinsics for SHA-1 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA1Intrinsics, false);
  }

  if (UseSHA/* && supports_crypto()*/) {
    if (FLAG_IS_DEFAULT(UseSHA256Intrinsics)) {
      FLAG_SET_DEFAULT(UseSHA256Intrinsics, true);
    }
  } else if (UseSHA256Intrinsics) {
    warning("Intrinsics for SHA-224 and SHA-256 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA256Intrinsics, false);
  }

  if (UseSHA512Intrinsics) {
    warning("Intrinsics for SHA-384 and SHA-512 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA512Intrinsics, false);
  }

  if (!(UseSHA1Intrinsics || UseSHA256Intrinsics || UseSHA512Intrinsics)) {
    FLAG_SET_DEFAULT(UseSHA, false);
  }

  // Basic instructions are used to implement AES Intrinsics on LA, so AES
  // instructions support is not needed.
  if (/*supports_crypto()*/ 1) {
    if (FLAG_IS_DEFAULT(UseAES)) {
      FLAG_SET_DEFAULT(UseAES, true);
    }
  } else if (UseAES) {
    if (!FLAG_IS_DEFAULT(UseAES))
      warning("AES instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseAES, false);
  }

  if (UseAES/* && supports_crypto()*/) {
    if (FLAG_IS_DEFAULT(UseAESIntrinsics)) {
      FLAG_SET_DEFAULT(UseAESIntrinsics, true);
    }
  } else if (UseAESIntrinsics) {
    if (!FLAG_IS_DEFAULT(UseAESIntrinsics))
      warning("AES intrinsics are not available on this CPU");
    FLAG_SET_DEFAULT(UseAESIntrinsics, false);
  }

  if (FLAG_IS_DEFAULT(UseCRC32)) {
    FLAG_SET_DEFAULT(UseCRC32, true);
  }

  if (UseCRC32) {
    if (FLAG_IS_DEFAULT(UseCRC32Intrinsics)) {
      UseCRC32Intrinsics = true;
    }
  }

  if (FLAG_IS_DEFAULT(UseMontgomeryMultiplyIntrinsic)) {
    UseMontgomeryMultiplyIntrinsic = true;
  }
  if (FLAG_IS_DEFAULT(UseMontgomerySquareIntrinsic)) {
    UseMontgomerySquareIntrinsic = true;
  }

  // This machine allows unaligned memory accesses
  if (FLAG_IS_DEFAULT(UseUnalignedAccesses)) {
    FLAG_SET_DEFAULT(UseUnalignedAccesses, true);
  }
}

void VM_Version::initialize() {
  ResourceMark rm;
  // Making this stub must be FIRST use of assembler

  stub_blob = BufferBlob::create("get_cpu_info_stub", stub_size);
  if (stub_blob == NULL) {
    vm_exit_during_initialization("Unable to allocate get_cpu_info_stub");
  }
  CodeBuffer c(stub_blob);
  VM_Version_StubGenerator g(&c);
  get_cpu_info_stub = CAST_TO_FN_PTR(get_cpu_info_stub_t,
                                     g.generate_get_cpu_info());

  get_processor_features();
}
