/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2021, Loongson Technology. All rights reserved.
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
#ifdef COMPILER2
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "classfile/systemDictionary.hpp"
#include "code/vmreg.hpp"
#include "interpreter/interpreter.hpp"
#include "opto/runtime.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/vframeArray.hpp"
#include "utilities/globalDefinitions.hpp"
#include "vmreg_loongarch.inline.hpp"
#endif

#define __ masm->

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

//-------------- generate_exception_blob -----------
// creates _exception_blob.
// The exception blob is jumped to from a compiled method.
// (see emit_exception_handler in sparc.ad file)
//
// Given an exception pc at a call we call into the runtime for the
// handler in this method. This handler might merely restore state
// (i.e. callee save registers) unwind the frame and jump to the
// exception handler for the nmethod if there is no Java level handler
// for the nmethod.
//
// This code is entered with a jump, and left with a jump.
//
// Arguments:
//   V0: exception oop
//   V1: exception pc
//
// Results:
//   A0: exception oop
//   A1: exception pc in caller or ???
//   jumps to: exception handler of caller
//
// Note: the exception pc MUST be at a call (precise debug information)
//
//  [stubGenerator_loongarch_64.cpp] generate_forward_exception()
//      |- V0, V1 are created
//      |- T4 <= SharedRuntime::exception_handler_for_return_address
//      `- jr T4
//           `- the caller's exception_handler
//                 `- jr OptoRuntime::exception_blob
//                        `- here
//
void OptoRuntime::generate_exception_blob() {
  // Capture info about frame layout
  enum layout {
    fp_off,
    return_off,                 // slot for return address
    framesize
  };

  // allocate space for the code
  ResourceMark rm;
  // setup code generation tools
  CodeBuffer   buffer("exception_blob", 5120, 5120);
  MacroAssembler* masm = new MacroAssembler(&buffer);

  address start = __ pc();

  __ addi_d(SP, SP, -1 * framesize * wordSize);   // Prolog!

  // this frame will be treated as the original caller method.
  // So, the return pc should be filled with the original exception pc.
  //   ref: X86's implementation
  __ st_d(V1, SP, return_off * wordSize);  // return address
  __ st_d(FP, SP, fp_off * wordSize);

  // Save callee saved registers.  None for UseSSE=0,
  // floats-only for UseSSE=1, and doubles for UseSSE=2.

  __ addi_d(FP, SP, fp_off * wordSize);

  // Store exception in Thread object. We cannot pass any arguments to the
  // handle_exception call, since we do not want to make any assumption
  // about the size of the frame where the exception happened in.
  Register thread = TREG;

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif

  __ st_d(V0, Address(thread, JavaThread::exception_oop_offset()));
  __ st_d(V1, Address(thread, JavaThread::exception_pc_offset()));

  // This call does all the hard work.  It checks if an exception handler
  // exists in the method.
  // If so, it returns the handler address.
  // If not, it prepares for stack-unwinding, restoring the callee-save
  // registers of the frame being removed.
  Label L;
  address the_pc = __ pc();
  __ bind(L);
  __ set_last_Java_frame(thread, NOREG, NOREG, L);

  __ li(AT, -(StackAlignmentInBytes));
  __ andr(SP, SP, AT);   // Fix stack alignment as required by ABI

  __ move(A0, thread);
  // TODO: confirm reloc
  __ call((address)OptoRuntime::handle_exception_C, relocInfo::runtime_call_type);

  // Set an oopmap for the call site
  OopMapSet *oop_maps = new OopMapSet();

  oop_maps->add_gc_map(the_pc - start, new OopMap(framesize, 0));

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ reset_last_Java_frame(thread, true);

  // Pop self-frame.
  __ leave();     // Epilog!

  // V0: exception handler

  // We have a handler in V0, (could be deopt blob)
  __ move(T4, V0);

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  // Get the exception
  __ ld_d(A0, Address(thread, JavaThread::exception_oop_offset()));
  // Get the exception pc in case we are deoptimized
  __ ld_d(A1, Address(thread, JavaThread::exception_pc_offset()));
#ifdef ASSERT
  __ st_d(R0, Address(thread, JavaThread::exception_handler_pc_offset()));
  __ st_d(R0, Address(thread, JavaThread::exception_pc_offset()));
#endif
  // Clear the exception oop so GC no longer processes it as a root.
  __ st_d(R0, Address(thread, JavaThread::exception_oop_offset()));

  // Fix seg fault when running:
  //    Eclipse + Plugin + Debug As
  //  This is the only condition where C2 calls SharedRuntime::generate_deopt_blob()
  //
  __ move(V0, A0);
  __ move(V1, A1);

  // V0: exception oop
  // T4: exception handler
  // A1: exception pc
  __ jr(T4);

  // make sure all code is generated
  masm->flush();
  _exception_blob = ExceptionBlob::create(&buffer, oop_maps, framesize);
}
