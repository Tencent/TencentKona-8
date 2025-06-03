/*
 * Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "interpreter/bytecodeHistogram.hpp"
#include "interpreter/cppInterpreter.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterGenerator.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "oops/arrayOop.hpp"
#include "oops/methodData.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "runtime/arguments.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/timer.hpp"
#include "runtime/vframeArray.hpp"
#include "utilities/debug.hpp"
#ifdef SHARK
#include "shark/shark_globals.hpp"
#endif

#ifdef CC_INTERP

// Routine exists to make tracebacks look decent in debugger
// while "shadow" interpreter frames are on stack. It is also
// used to distinguish interpreter frames.

extern "C" void RecursiveInterpreterActivation(interpreterState istate) {
  ShouldNotReachHere();
}

bool CppInterpreter::contains(address pc) {
  Unimplemented();
}

#define STATE(field_name) Lstate, in_bytes(byte_offset_of(BytecodeInterpreter, field_name))
#define __ _masm->

Label frame_manager_entry;
Label fast_accessor_slow_entry_path;  // fast accessor methods need to be able to jmp to unsynchronized
                                      // c++ interpreter entry point this holds that entry point label.

static address unctrap_frame_manager_entry  = NULL;

static address interpreter_return_address  = NULL;
static address deopt_frame_manager_return_atos  = NULL;
static address deopt_frame_manager_return_btos  = NULL;
static address deopt_frame_manager_return_itos  = NULL;
static address deopt_frame_manager_return_ltos  = NULL;
static address deopt_frame_manager_return_ftos  = NULL;
static address deopt_frame_manager_return_dtos  = NULL;
static address deopt_frame_manager_return_vtos  = NULL;

const Register prevState = G1_scratch;

void InterpreterGenerator::save_native_result(void) {
  Unimplemented();
}

void InterpreterGenerator::restore_native_result(void) {
  Unimplemented();
}

// A result handler converts/unboxes a native call result into
// a java interpreter/compiler result. The current frame is an
// interpreter frame. The activation frame unwind code must be
// consistent with that of TemplateTable::_return(...). In the
// case of native methods, the caller's SP was not modified.
address CppInterpreterGenerator::generate_result_handler_for(BasicType type) {
  Unimplemented();
}

address CppInterpreterGenerator::generate_tosca_to_stack_converter(BasicType type) {
  Unimplemented();
}

address CppInterpreterGenerator::generate_stack_to_stack_converter(BasicType type) {
  Unimplemented();
}

address CppInterpreterGenerator::generate_stack_to_native_abi_converter(BasicType type) {
  Unimplemented();
}

address CppInterpreter::return_entry(TosState state, int length) {
  Unimplemented();
}

address CppInterpreter::deopt_entry(TosState state, int length) {
  Unimplemented();
}

void InterpreterGenerator::generate_counter_incr(Label* overflow, Label* profile_method, Label* profile_method_continue) {
  Unimplemented();
}

address InterpreterGenerator::generate_empty_entry(void) {
  Unimplemented();
}

address InterpreterGenerator::generate_accessor_entry(void) {
  Unimplemented();
}

address InterpreterGenerator::generate_native_entry(bool synchronized) {
  Unimplemented();
}

void CppInterpreterGenerator::generate_compute_interpreter_state(const Register state,
                                                              const Register prev_state,
                                                              bool native) {
  Unimplemented();
}

void InterpreterGenerator::lock_method(void) {
  Unimplemented();
}

void CppInterpreterGenerator::generate_deopt_handling() {
  Unimplemented();
}

void CppInterpreterGenerator::generate_more_monitors() {
  Unimplemented();
}


static address interpreter_frame_manager = NULL;

void CppInterpreterGenerator::adjust_callers_stack(Register args) {
  Unimplemented();
}

address InterpreterGenerator::generate_normal_entry(bool synchronized) {
  Unimplemented();
}

InterpreterGenerator::InterpreterGenerator(StubQueue* code)
 : CppInterpreterGenerator(code) {
  Unimplemented();
}


static int size_activation_helper(int callee_extra_locals, int max_stack, int monitor_size) {
  Unimplemented();
}

int AbstractInterpreter::size_top_interpreter_activation(methodOop method) {
  Unimplemented();
}

void BytecodeInterpreter::layout_interpreterState(interpreterState to_fill,
                                           frame* caller,
                                           frame* current,
                                           methodOop method,
                                           intptr_t* locals,
                                           intptr_t* stack,
                                           intptr_t* stack_base,
                                           intptr_t* monitor_base,
                                           intptr_t* frame_bottom,
                                           bool is_top_frame
                                           )
{
  Unimplemented();
}

void BytecodeInterpreter::pd_layout_interpreterState(interpreterState istate, address last_Java_pc, intptr_t* last_Java_fp) {
  Unimplemented();
}


int AbstractInterpreter::layout_activation(methodOop method,
                                           int tempcount, // Number of slots on java expression stack in use
                                           int popframe_extra_args,
                                           int moncount,  // Number of active monitors
                                           int callee_param_size,
                                           int callee_locals_size,
                                           frame* caller,
                                           frame* interpreter_frame,
                                           bool is_top_frame) {
  Unimplemented();
}

#endif // CC_INTERP
