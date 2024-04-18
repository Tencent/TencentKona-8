/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2023, Loongson Technology. All rights reserved.
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
#include "vm_version_mips.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif

#define A0 RA0

int VM_Version::_cpuFeatures;
const char* VM_Version::_features_str = "";
VM_Version::CpuidInfo VM_Version::_cpuid_info   = { 0, };
volatile bool VM_Version::_is_determine_cpucfg_supported_running = false;
bool VM_Version::_is_cpucfg_instruction_supported = true;
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
    __ push(V0);

    __ li(AT, (long)0);
    __ cpucfg(V0, AT);
    __ lw(AT, A0, in_bytes(VM_Version::Loongson_Cpucfg_id0_offset()));
    __ sw(V0, A0, in_bytes(VM_Version::Loongson_Cpucfg_id0_offset()));

    __ li(AT, 1);
    __ cpucfg(V0, AT);
    __ lw(AT, A0, in_bytes(VM_Version::Loongson_Cpucfg_id1_offset()));
    __ sw(V0, A0, in_bytes(VM_Version::Loongson_Cpucfg_id1_offset()));

    __ li(AT, 2);
    __ cpucfg(V0, AT);
    __ lw(AT, A0, in_bytes(VM_Version::Loongson_Cpucfg_id2_offset()));
    __ sw(V0, A0, in_bytes(VM_Version::Loongson_Cpucfg_id2_offset()));

    __ pop(V0);
    __ pop(AT);
    __ leave();
    __ jr(RA);
    __ delayed()->nop();
#   undef __

    return start;
  };
};

uint32_t VM_Version::get_feature_flags_by_cpucfg() {
  uint32_t result = 0;
  if (_cpuid_info.cpucfg_info_id1.bits.MMI != 0)
    result |= CPU_MMI;
  if (_cpuid_info.cpucfg_info_id1.bits.MSA1 != 0)
    result |= CPU_MSA1_0;
  if (_cpuid_info.cpucfg_info_id1.bits.MSA2 != 0)
    result |= CPU_MSA2_0;
  if (_cpuid_info.cpucfg_info_id1.bits.CGP != 0)
    result |= CPU_CGP;
  if (_cpuid_info.cpucfg_info_id1.bits.LSX1 != 0)
    result |= CPU_LSX1;
  if (_cpuid_info.cpucfg_info_id1.bits.LSX2 != 0)
    result |= CPU_LSX2;
  if (_cpuid_info.cpucfg_info_id1.bits.LASX != 0)
    result |= CPU_LASX;
  if (_cpuid_info.cpucfg_info_id1.bits.LLSYNC != 0)
    result |= CPU_LLSYNC;
  if (_cpuid_info.cpucfg_info_id1.bits.TGTSYNC != 0)
    result |= CPU_TGTSYNC;
  if (_cpuid_info.cpucfg_info_id1.bits.MUALP != 0)
    result |= CPU_MUALP;
  if (_cpuid_info.cpucfg_info_id2.bits.LEXT1 != 0)
    result |= CPU_LEXT1;
  if (_cpuid_info.cpucfg_info_id2.bits.LEXT2 != 0)
    result |= CPU_LEXT2;
  if (_cpuid_info.cpucfg_info_id2.bits.LEXT3 != 0)
    result |= CPU_LEXT3;
  if (_cpuid_info.cpucfg_info_id2.bits.LAMO != 0)
    result |= CPU_LAMO;
  if (_cpuid_info.cpucfg_info_id2.bits.LPIXU != 0)
    result |= CPU_LPIXU;

  result |= CPU_ULSYNC;

  return result;
}

void read_cpu_info(const char *path, char *result) {
  FILE *ptr;
  char buf[1024];
  int i = 0;
  if((ptr=fopen(path, "r")) != NULL) {
    while(fgets(buf, 1024, ptr)!=NULL) {
      strcat(result,buf);
      i++;
      if (i == 10) break;
    }
    fclose(ptr);
  } else {
    warning("Can't detect CPU info - cannot open %s", path);
  }
}

void strlwr(char *str) {
  for (; *str!='\0'; str++)
    *str = tolower(*str);
}

int VM_Version::get_feature_flags_by_cpuinfo(int features) {
  assert(!cpu_info_is_initialized(), "VM_Version should not be initialized");

  char res[10240];
  int i;
  memset(res, '\0', 10240 * sizeof(char));
  read_cpu_info("/proc/cpuinfo", res);
  // res is converted to lower case
  strlwr(res);

  if (strstr(res, "loongson")) {
    // Loongson CPU
    features |= CPU_LOONGSON;

    const struct Loongson_Cpuinfo loongson_cpuinfo[] = {
      {L_3A1000,  "3a1000"},
      {L_3B1500,  "3b1500"},
      {L_3A2000,  "3a2000"},
      {L_3B2000,  "3b2000"},
      {L_3A3000,  "3a3000"},
      {L_3B3000,  "3b3000"},
      {L_2K1000,  "2k1000"},
      {L_UNKNOWN, "unknown"}
    };

    // Loongson Family
    int detected = 0;
    for (i = 0; i <= L_UNKNOWN; i++) {
      switch (i) {
        // 3A1000 and 3B1500 may use an old kernel and further comparsion is needed
        // test PRID REV in /proc/cpuinfo
        // 3A1000: V0.5, model name: ICT Loongson-3A V0.5  FPU V0.1
        // 3B1500: V0.7, model name: ICT Loongson-3B V0.7  FPU V0.1
        case L_3A1000:
          if (strstr(res, loongson_cpuinfo[i].match_str) || strstr(res, "loongson-3a v0.5")) {
            features |= CPU_LOONGSON_GS464;
            detected++;
            //tty->print_cr("3A1000 platform");
          }
          break;
        case L_3B1500:
          if (strstr(res, loongson_cpuinfo[i].match_str) || strstr(res, "loongson-3b v0.7")) {
            features |= CPU_LOONGSON_GS464;
            detected++;
            //tty->print_cr("3B1500 platform");
          }
          break;
        case L_3A2000:
        case L_3B2000:
        case L_3A3000:
        case L_3B3000:
          if (strstr(res, loongson_cpuinfo[i].match_str)) {
            features |= CPU_LOONGSON_GS464E;
            detected++;
            //tty->print_cr("3A2000/3A3000/3B2000/3B3000 platform");
          }
          break;
        case L_2K1000:
          if (strstr(res, loongson_cpuinfo[i].match_str)) {
            features |= CPU_LOONGSON_GS264;
            detected++;
            //tty->print_cr("2K1000 platform");
          }
          break;
        case L_UNKNOWN:
          if (detected == 0) {
            detected++;
            //tty->print_cr("unknown Loongson platform");
          }
          break;
        default:
          ShouldNotReachHere();
      }
    }
    assert (detected == 1, "one and only one of LOONGSON_CPU_FAMILY should be detected");
  } else { // not Loongson
    // Not Loongson CPU
    //tty->print_cr("MIPS platform");
  }

  if (features & CPU_LOONGSON_GS264) {
    features |= CPU_LEXT1;
    features |= CPU_LEXT2;
    features |= CPU_TGTSYNC;
    features |= CPU_ULSYNC;
    features |= CPU_MSA1_0;
    features |= CPU_LSX1;
  } else if (features & CPU_LOONGSON_GS464) {
    features |= CPU_LEXT1;
    features |= CPU_LLSYNC;
    features |= CPU_TGTSYNC;
  } else if (features & CPU_LOONGSON_GS464E) {
    features |= CPU_LEXT1;
    features |= CPU_LEXT2;
    features |= CPU_LEXT3;
    features |= CPU_TGTSYNC;
    features |= CPU_ULSYNC;
  } else if (features & CPU_LOONGSON) {
    // unknow loongson
    features |= CPU_LLSYNC;
    features |= CPU_TGTSYNC;
    features |= CPU_ULSYNC;
  }
  VM_Version::_cpu_info_is_initialized = true;

  return features;
}

void VM_Version::get_processor_features() {

  clean_cpuFeatures();

  // test if cpucfg instruction is supported
  VM_Version::_is_determine_cpucfg_supported_running = true;
  __asm__ __volatile__(
    ".insn \n\t"
    ".word (0xc8080118)\n\t" // cpucfg zero, zero
    :
    :
    :
    );
  VM_Version::_is_determine_cpucfg_supported_running = false;

  if (supports_cpucfg()) {
    get_cpu_info_stub(&_cpuid_info);
    _cpuFeatures = get_feature_flags_by_cpucfg();
    // Only Loongson CPUs support cpucfg
    _cpuFeatures |= CPU_LOONGSON;
  } else {
    _cpuFeatures = get_feature_flags_by_cpuinfo(0);
  }

  _supports_cx8 = true;

  if (UseG1GC && FLAG_IS_DEFAULT(MaxGCPauseMillis)) {
    FLAG_SET_CMDLINE(uintx, MaxGCPauseMillis, 650);
  }

#ifdef COMPILER2
  if (MaxVectorSize > 0) {
    if (!is_power_of_2(MaxVectorSize)) {
      warning("MaxVectorSize must be a power of 2");
      MaxVectorSize = 8;
    }
    if (MaxVectorSize > 0 && supports_ps()) {
      MaxVectorSize = 8;
    } else {
      MaxVectorSize = 0;
    }
  }
  //
  // Vector optimization of MIPS works in most cases, but cannot pass hotspot/test/compiler/6340864/TestFloatVect.java.
  // Vector optimization was closed by default.
  // The reasons:
  // 1. The kernel does not have emulation of PS instructions yet, so the emulation of PS instructions must be done in JVM, see JVM_handle_linux_signal.
  // 2. It seems the gcc4.4.7 had some bug related to ucontext_t, which is used in signal handler to emulate PS instructions.
  //
  if (FLAG_IS_DEFAULT(MaxVectorSize)) {
    MaxVectorSize = 0;
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

  if (supports_lext1()) {
    if (FLAG_IS_DEFAULT(UseLEXT1)) {
      FLAG_SET_DEFAULT(UseLEXT1, true);
    }
  } else if (UseLEXT1) {
    warning("LEXT1 instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseLEXT1, false);
  }

  if (supports_lext2()) {
    if (FLAG_IS_DEFAULT(UseLEXT2)) {
      FLAG_SET_DEFAULT(UseLEXT2, true);
    }
  } else if (UseLEXT2) {
    warning("LEXT2 instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseLEXT2, false);
  }

  if (supports_lext3()) {
    if (FLAG_IS_DEFAULT(UseLEXT3)) {
      FLAG_SET_DEFAULT(UseLEXT3, true);
    }
  } else if (UseLEXT3) {
    warning("LEXT3 instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseLEXT3, false);
  }

  if (UseLEXT2) {
    if (FLAG_IS_DEFAULT(UseCountTrailingZerosInstructionMIPS64)) {
      FLAG_SET_DEFAULT(UseCountTrailingZerosInstructionMIPS64, 1);
    }
  } else if (UseCountTrailingZerosInstructionMIPS64) {
    if (!FLAG_IS_DEFAULT(UseCountTrailingZerosInstructionMIPS64))
      warning("ctz/dctz instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseCountTrailingZerosInstructionMIPS64, 0);
  }

  if (TieredCompilation) {
    if (!FLAG_IS_DEFAULT(TieredCompilation))
      warning("TieredCompilation not supported");
    FLAG_SET_DEFAULT(TieredCompilation, false);
  }

  char buf[256];
  bool is_unknown_loongson_cpu = is_loongson() && !is_gs464() && !is_gs464e() && !is_gs264() && !supports_cpucfg();

  // A note on the _features_string format:
  //   There are jtreg tests checking the _features_string for various properties.
  //   For some strange reason, these tests require the string to contain
  //   only _lowercase_ characters. Keep that in mind when being surprised
  //   about the unusual notation of features - and when adding new ones.
  //   Features may have one comma at the end.
  //   Furthermore, use one, and only one, separator space between features.
  //   Multiple spaces are considered separate tokens, messing up everything.
  jio_snprintf(buf, sizeof(buf), "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s, usesynclevel:%d",
              (is_loongson()           ?  "mips-compatible loongson cpu"  : "mips cpu"),
              (is_gs464()              ?  ", gs464 (3a1000/3b1500)" : ""),
              (is_gs464e()             ?  ", gs464e (3a2000/3a3000/3b2000/3b3000)" : ""),
              (is_gs264()              ?  ", gs264 (2k1000)" : ""),
              (is_unknown_loongson_cpu ?  ", unknown loongson cpu" : ""),
              (supports_dsp()          ?  ", dsp" : ""),
              (supports_ps()           ?  ", ps" : ""),
              (supports_3d()           ?  ", 3d" : ""),
              (supports_mmi()          ?  ", mmi" : ""),
              (supports_msa1_0()       ?  ", msa1_0" : ""),
              (supports_msa2_0()       ?  ", msa2_0" : ""),
              (supports_lsx1()         ?  ", lsx1" : ""),
              (supports_lsx2()         ?  ", lsx2" : ""),
              (supports_lasx()         ?  ", lasx" : ""),
              (supports_lext1()        ?  ", lext1" : ""),
              (supports_lext2()        ?  ", lext2" : ""),
              (supports_lext3()        ?  ", lext3" : ""),
              (supports_cgp()          ?  ", aes, crc, sha1, sha256, sha512" : ""),
              (supports_lamo()         ?  ", lamo" : ""),
              (supports_lpixu()        ?  ", lpixu" : ""),
              (needs_llsync()          ?  ", llsync" : ""),
              (needs_tgtsync()         ?  ", tgtsync": ""),
              (needs_ulsync()          ?  ", ulsync": ""),
              (supports_mualp()        ?  ", mualp" : ""),
              UseSyncLevel);
  _features_str = strdup(buf);

  if (FLAG_IS_DEFAULT(AllocatePrefetchStyle)) {
    FLAG_SET_DEFAULT(AllocatePrefetchStyle, 1);
  }

  if (FLAG_IS_DEFAULT(AllocatePrefetchLines)) {
    FLAG_SET_DEFAULT(AllocatePrefetchLines, 1);
  }

  if (FLAG_IS_DEFAULT(AllocatePrefetchStepSize)) {
    FLAG_SET_DEFAULT(AllocatePrefetchStepSize, 64);
  }

  if (FLAG_IS_DEFAULT(AllocatePrefetchDistance)) {
    FLAG_SET_DEFAULT(AllocatePrefetchDistance, 64);
  }

  if (FLAG_IS_DEFAULT(AllocateInstancePrefetchLines)) {
    FLAG_SET_DEFAULT(AllocateInstancePrefetchLines, 1);
  }

  if (UseSHA) {
    warning("SHA instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseSHA, false);
  }

  if (UseSHA1Intrinsics || UseSHA256Intrinsics || UseSHA512Intrinsics) {
    warning("SHA intrinsics are not available on this CPU");
    FLAG_SET_DEFAULT(UseSHA1Intrinsics, false);
    FLAG_SET_DEFAULT(UseSHA256Intrinsics, false);
    FLAG_SET_DEFAULT(UseSHA512Intrinsics, false);
  }

  if (UseAES) {
    if (!FLAG_IS_DEFAULT(UseAES)) {
      warning("AES instructions are not available on this CPU");
      FLAG_SET_DEFAULT(UseAES, false);
    }
  }

  if (UseCRC32Intrinsics) {
    if (!FLAG_IS_DEFAULT(UseCRC32Intrinsics)) {
      warning("CRC32Intrinsics instructions are not available on this CPU");
      FLAG_SET_DEFAULT(UseCRC32Intrinsics, false);
    }
  }

  if (UseAESIntrinsics) {
    if (!FLAG_IS_DEFAULT(UseAESIntrinsics)) {
      warning("AES intrinsics are not available on this CPU");
      FLAG_SET_DEFAULT(UseAESIntrinsics, false);
    }
  }

  if (FLAG_IS_DEFAULT(UseMontgomeryMultiplyIntrinsic)) {
    UseMontgomeryMultiplyIntrinsic = true;
  }
  if (FLAG_IS_DEFAULT(UseMontgomerySquareIntrinsic)) {
    UseMontgomerySquareIntrinsic = true;
  }

  if (CriticalJNINatives) {
    if (FLAG_IS_CMDLINE(CriticalJNINatives)) {
      warning("CriticalJNINatives specified, but not supported in this VM");
    }
    FLAG_SET_DEFAULT(CriticalJNINatives, false);
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
