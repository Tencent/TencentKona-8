/*
 * Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_MIPS_VM_GLOBALS_MIPS_HPP
#define CPU_MIPS_VM_GLOBALS_MIPS_HPP

#include "utilities/globalDefinitions.hpp"
#include "utilities/macros.hpp"

// Sets the default values for platform dependent flags used by the runtime system.
// (see globals.hpp)

#ifdef CORE
define_pd_global(bool,  UseSSE,      0);
#endif /* CORE */
define_pd_global(bool,  ConvertSleepToYield,      true);
define_pd_global(bool,  ShareVtableStubs,         true);
define_pd_global(bool,  CountInterpCalls,         true);

define_pd_global(bool, ImplicitNullChecks,          true);  // Generate code for implicit null checks
define_pd_global(bool, TrapBasedNullChecks,      false); // Not needed on x86.
define_pd_global(bool, UncommonNullCast,         true);  // Uncommon-trap NULLs passed to check cast
define_pd_global(bool, NeedsDeoptSuspend,           false); // only register window machines need this

// See 4827828 for this change. There is no globals_core_i486.hpp. I can't
// assign a different value for C2 without touching a number of files. Use
// #ifdef to minimize the change as it's late in Mantis. -- FIXME.
// c1 doesn't have this problem because the fix to 4858033 assures us
// the the vep is aligned at CodeEntryAlignment whereas c2 only aligns
// the uep and the vep doesn't get real alignment but just slops on by
// only assured that the entry instruction meets the 5 byte size requirement.
define_pd_global(intx,  CodeEntryAlignment,      16);
define_pd_global(intx, OptoLoopAlignment,        16);
define_pd_global(intx, InlineFrequencyCount,     100);
define_pd_global(intx, InlineSmallCode,          4000); // MIPS generates 3x instructions than X86

define_pd_global(uintx, TLABSize,                 0);
define_pd_global(uintx, NewSize,                  1024 * K);
define_pd_global(intx,  PreInflateSpin,      10);

define_pd_global(intx, PrefetchCopyIntervalInBytes, -1);
define_pd_global(intx, PrefetchScanIntervalInBytes, -1);
define_pd_global(intx, PrefetchFieldsAhead,         -1);

define_pd_global(intx, StackYellowPages, 2);
define_pd_global(intx, StackRedPages, 1);
define_pd_global(intx, StackShadowPages, 3 DEBUG_ONLY(+1));

define_pd_global(bool, RewriteBytecodes,     true);
define_pd_global(bool, RewriteFrequentPairs, true);
define_pd_global(bool, UseMembar,            true);
// GC Ergo Flags
define_pd_global(intx, CMSYoungGenPerWorker, 64*M);  // default max size of CMS young gen, per GC worker thread

define_pd_global(uintx, TypeProfileLevel, 111);

define_pd_global(bool, PreserveFramePointer, false);
// Only c2 cares about this at the moment
define_pd_global(intx, AllocatePrefetchStyle,        2);
define_pd_global(intx, AllocatePrefetchDistance,     -1);

#define ARCH_FLAGS(develop, product, diagnostic, experimental, notproduct) \
                                                                            \
  product(bool, UseLEXT1, false,                                            \
                "Use LoongISA general EXTensions 1")                        \
                                                                            \
  product(bool, UseLEXT2, false,                                            \
                "Use LoongISA general EXTensions 2")                        \
                                                                            \
  product(bool, UseLEXT3, false,                                            \
                "Use LoongISA general EXTensions 3")                        \
                                                                            \
  product(bool, UseCodeCacheAllocOpt, true,                                 \
                "Allocate code cache within 32-bit memory address space")   \
                                                                            \
  product(intx, UseSyncLevel, 10000,                                        \
                "The sync level on Loongson CPUs"                           \
                "UseSyncLevel == 10000, 111, for all Loongson CPUs, "       \
                "UseSyncLevel == 4000, 101, maybe for GS464V"               \
                "UseSyncLevel == 3000, 001, maybe for GS464V"               \
                "UseSyncLevel == 2000, 011, maybe for GS464E/GS264"         \
                "UseSyncLevel == 1000, 110, maybe for GS464")               \
                                                                            \
  develop(bool, UseBoundCheckInstruction, false,                            \
                "Use bound check instruction")                              \
                                                                            \
  product(intx, SetFSFOFN, 999,                                             \
          "Set the FS/FO/FN bits in FCSR"                                   \
          "999 means FS/FO/FN will not be changed"                          \
          "=XYZ, with X:FS, Y:FO, Z:FN, X, Y and Z in 0=off, 1=on")         \
                                                                            \
  /* assembler */                                                           \
  product(bool, UseCountLeadingZerosInstructionMIPS64, true,                \
          "Use count leading zeros instruction")                            \
                                                                            \
  product(bool, UseCountTrailingZerosInstructionMIPS64, false,              \
          "Use count trailing zeros instruction")                           \
                                                                            \
  product(bool, UseActiveCoresMP, false,                                    \
                "Eliminate barriers for single active cpu")

#endif // CPU_MIPS_VM_GLOBALS_MIPS_HPP
