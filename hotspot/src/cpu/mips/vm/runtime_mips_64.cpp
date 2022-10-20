/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
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
#include "vmreg_mips.inline.hpp"
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
#define T8 RT8
#define T9 RT9

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
//  [stubGenerator_mips.cpp] generate_forward_exception()
//      |- V0, V1 are created
//      |- T9 <= SharedRuntime::exception_handler_for_return_address
//      `- jr T9
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

  __ daddiu(SP, SP, -1 * framesize * wordSize);   // Prolog!

  // this frame will be treated as the original caller method.
  // So, the return pc should be filled with the original exception pc.
  //   ref: X86's implementation
  __ sd(V1, SP, return_off  *wordSize);  // return address
  __ sd(FP, SP, fp_off  *wordSize);

  // Save callee saved registers.  None for UseSSE=0,
  // floats-only for UseSSE=1, and doubles for UseSSE=2.

  __ daddiu(FP, SP, fp_off * wordSize);

  // Store exception in Thread object. We cannot pass any arguments to the
  // handle_exception call, since we do not want to make any assumption
  // about the size of the frame where the exception happened in.
  Register thread = TREG;

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif

  __ sd(V0, Address(thread, JavaThread::exception_oop_offset()));
  __ sd(V1, Address(thread, JavaThread::exception_pc_offset()));

  // This call does all the hard work.  It checks if an exception handler
  // exists in the method.
  // If so, it returns the handler address.
  // If not, it prepares for stack-unwinding, restoring the callee-save
  // registers of the frame being removed.
  __ set_last_Java_frame(thread, NOREG, NOREG, NULL);

  __ move(AT, -(StackAlignmentInBytes));
  __ andr(SP, SP, AT);   // Fix stack alignment as required by ABI

  __ relocate(relocInfo::internal_pc_type);

  {
    long save_pc = (long)__ pc() + 48;
    __ patchable_set48(AT, save_pc);
  }
  __ sd(AT, thread, in_bytes(JavaThread::last_Java_pc_offset()));

  __ move(A0, thread);
  __ patchable_set48(T9, (long)OptoRuntime::handle_exception_C);
  __ jalr(T9);
  __ delayed()->nop();

  // Set an oopmap for the call site
  OopMapSet *oop_maps = new OopMapSet();
  OopMap* map =  new OopMap( framesize, 0 );

  oop_maps->add_gc_map( __ offset(), map);

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ reset_last_Java_frame(thread, true);

  // Pop self-frame.
  __ leave();     // Epilog!

  // V0: exception handler

  // We have a handler in V0, (could be deopt blob)
  __ move(T9, V0);

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  // Get the exception
  __ ld(A0, Address(thread, JavaThread::exception_oop_offset()));
  // Get the exception pc in case we are deoptimized
  __ ld(A1, Address(thread, JavaThread::exception_pc_offset()));
#ifdef ASSERT
  __ sd(R0, Address(thread, JavaThread::exception_handler_pc_offset()));
  __ sd(R0, Address(thread, JavaThread::exception_pc_offset()));
#endif
  // Clear the exception oop so GC no longer processes it as a root.
  __ sd(R0, Address(thread, JavaThread::exception_oop_offset()));

  // Fix seg fault when running:
  //    Eclipse + Plugin + Debug As
  //  This is the only condition where C2 calls SharedRuntime::generate_deopt_blob()
  //
  __ move(V0, A0);
  __ move(V1, A1);

  // V0: exception oop
  // T9: exception handler
  // A1: exception pc
  __ jr(T9);
  __ delayed()->nop();

  // make sure all code is generated
  masm->flush();

  _exception_blob = ExceptionBlob::create(&buffer, oop_maps, framesize);
}
