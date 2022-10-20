/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "asm/macroAssembler.hpp"
#include "interpreter/bytecodeHistogram.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterGenerator.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "oops/arrayOop.hpp"
#include "oops/methodData.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "runtime/arguments.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/timer.hpp"
#include "runtime/vframeArray.hpp"
#include "utilities/debug.hpp"

#define __ _masm->

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

#ifndef CC_INTERP

// asm based interpreter deoptimization helpers
int AbstractInterpreter::size_activation(int max_stack,
                                         int temps,
                                         int extra_args,
                                         int monitors,
                                         int callee_params,
                                         int callee_locals,
                                         bool is_top_frame) {
  // Note: This calculation must exactly parallel the frame setup
  // in AbstractInterpreterGenerator::generate_method_entry.

  // fixed size of an interpreter frame:
  int overhead = frame::sender_sp_offset -
                 frame::interpreter_frame_initial_sp_offset;
  // Our locals were accounted for by the caller (or last_frame_adjust
  // on the transistion) Since the callee parameters already account
  // for the callee's params we only need to account for the extra
  // locals.
  int size = overhead +
         (callee_locals - callee_params)*Interpreter::stackElementWords +
         monitors * frame::interpreter_frame_monitor_size() +
         temps* Interpreter::stackElementWords + extra_args;

  return size;
}


const int Interpreter::return_sentinel = 0xfeedbeed;
const int method_offset = frame::interpreter_frame_method_offset * wordSize;
const int bci_offset    = frame::interpreter_frame_bcx_offset    * wordSize;
const int locals_offset = frame::interpreter_frame_locals_offset * wordSize;

//-----------------------------------------------------------------------------

address TemplateInterpreterGenerator::generate_StackOverflowError_handler() {
  address entry = __ pc();

#ifdef ASSERT
  {
    Label L;
    __ addi_d(T1, FP, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    __ sub_d(T1, T1, SP); // T1 = maximal sp for current fp
    __ bge(T1, R0, L);     // check if frame is complete
    __ stop("interpreter frame not set up");
    __ bind(L);
  }
#endif // ASSERT
  // Restore bcp under the assumption that the current frame is still
  // interpreted
  // FIXME: please change the func restore_bcp
  // S0 is the conventional register for bcp
  __ restore_bcp();

  // expression stack must be empty before entering the VM if an
  // exception happened
  __ empty_expression_stack();
  // throw exception
  // FIXME: why do not pass parameter thread ?
  __ call_VM(NOREG, CAST_FROM_FN_PTR(address, InterpreterRuntime::throw_StackOverflowError));
  return entry;
}

address TemplateInterpreterGenerator::generate_ArrayIndexOutOfBounds_handler(
        const char* name) {
  address entry = __ pc();
  // expression stack must be empty before entering the VM if an
  // exception happened
  __ empty_expression_stack();
  __ li(A1, (long)name);
  __ call_VM(noreg, CAST_FROM_FN_PTR(address,
  InterpreterRuntime::throw_ArrayIndexOutOfBoundsException), A1, A2);
  return entry;
}

address TemplateInterpreterGenerator::generate_ClassCastException_handler() {
  address entry = __ pc();
  // expression stack must be empty before entering the VM if an
  // exception happened
  __ empty_expression_stack();
  __ empty_FPU_stack();
  __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::throw_ClassCastException),  FSR);
  return entry;
}

address TemplateInterpreterGenerator::generate_exception_handler_common(
        const char* name, const char* message, bool pass_oop) {
  assert(!pass_oop || message == NULL, "either oop or message but not both");
  address entry = __ pc();

  // expression stack must be empty before entering the VM if an exception happened
  __ empty_expression_stack();
  // setup parameters
  __ li(A1, (long)name);
  if (pass_oop) {
    __ call_VM(V0,
    CAST_FROM_FN_PTR(address, InterpreterRuntime::create_klass_exception), A1, FSR);
  } else {
    __ li(A2, (long)message);
    __ call_VM(V0,
    CAST_FROM_FN_PTR(address, InterpreterRuntime::create_exception), A1, A2);
  }
  // throw exception
  __ jmp(Interpreter::throw_exception_entry(), relocInfo::none);
  return entry;
}


address TemplateInterpreterGenerator::generate_continuation_for(TosState state) {
  address entry = __ pc();
  // NULL last_sp until next java call
  __ st_d(R0,Address(FP, frame::interpreter_frame_last_sp_offset * wordSize));
  __ dispatch_next(state);
  return entry;
}


address TemplateInterpreterGenerator::generate_return_entry_for(TosState state, int step, size_t index_size) {
  address entry = __ pc();

  // Restore stack bottom in case i2c adjusted stack
  __ ld_d(SP, Address(FP, frame::interpreter_frame_last_sp_offset * wordSize));
  // and NULL it as marker that sp is now tos until next java call
  __ st_d(R0, FP, frame::interpreter_frame_last_sp_offset * wordSize);

  __ restore_bcp();
  __ restore_locals();

  // mdp: T8
  // ret: FSR
  // tmp: T4
  if (state == atos) {
    Register mdp = T8;
    Register tmp = T4;
    __ profile_return_type(mdp, FSR, tmp);
  }


  const Register cache = T4;
  const Register index = T3;
  __ get_cache_and_index_at_bcp(cache, index, 1, index_size);

  const Register flags = cache;
  __ alsl_d(AT, index, cache, Address::times_ptr - 1);
  __ ld_w(flags, AT, in_bytes(ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::flags_offset()));
  __ andi(flags, flags, ConstantPoolCacheEntry::parameter_size_mask);
  __ alsl_d(SP, flags, SP, Interpreter::stackElementScale() - 1);

  __ dispatch_next(state, step);

  return entry;
}


address TemplateInterpreterGenerator::generate_deopt_entry_for(TosState state,
                                                               int step) {
  address entry = __ pc();
  // NULL last_sp until next java call
  __ st_d(R0, FP, frame::interpreter_frame_last_sp_offset * wordSize);
  __ restore_bcp();
  __ restore_locals();
  // handle exceptions
  {
    Label L;
    const Register thread = TREG;
#ifndef OPT_THREAD
    __ get_thread(thread);
#endif
    __ ld_d(AT, thread, in_bytes(Thread::pending_exception_offset()));
    __ beq(AT, R0, L);
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::throw_pending_exception));
    __ should_not_reach_here();
    __ bind(L);
  }
  __ dispatch_next(state, step);
  return entry;
}

int AbstractInterpreter::BasicType_as_index(BasicType type) {
  int i = 0;
  switch (type) {
    case T_BOOLEAN: i = 0; break;
    case T_CHAR   : i = 1; break;
    case T_BYTE   : i = 2; break;
    case T_SHORT  : i = 3; break;
    case T_INT    : // fall through
    case T_LONG   : // fall through
    case T_VOID   : i = 4; break;
    case T_FLOAT  : i = 5; break;
    case T_DOUBLE : i = 6; break;
    case T_OBJECT : // fall through
    case T_ARRAY  : i = 7; break;
    default       : ShouldNotReachHere();
  }
  assert(0 <= i && i < AbstractInterpreter::number_of_result_handlers,
         "index out of bounds");
  return i;
}


address TemplateInterpreterGenerator::generate_result_handler_for(
        BasicType type) {
  address entry = __ pc();
  switch (type) {
    case T_BOOLEAN: __ c2bool(V0);                break;
    case T_CHAR   : __ bstrpick_d(V0, V0, 15, 0); break;
    case T_BYTE   : __ sign_extend_byte (V0);     break;
    case T_SHORT  : __ sign_extend_short(V0);     break;
    case T_INT    : /* nothing to do */           break;
    case T_FLOAT  : /* nothing to do */           break;
    case T_DOUBLE : /* nothing to do */           break;
    case T_OBJECT :
    {
      __ ld_d(V0, FP, frame::interpreter_frame_oop_temp_offset * wordSize);
      __ verify_oop(V0);         // and verify it
    }
    break;
    default       : ShouldNotReachHere();
  }
  __ jr(RA);                                  // return from result handler
  return entry;
}

address TemplateInterpreterGenerator::generate_safept_entry_for(
        TosState state,
        address runtime_entry) {
  address entry = __ pc();
  __ push(state);
  __ call_VM(noreg, runtime_entry);
  __ dispatch_via(vtos, Interpreter::_normal_table.table_for(vtos));
  return entry;
}



// Helpers for commoning out cases in the various type of method entries.
//


// increment invocation count & check for overflow
//
// Note: checking for negative value instead of overflow
//       so we have a 'sticky' overflow test
//
// Rmethod: method
// T3     : invocation counter
//
void InterpreterGenerator::generate_counter_incr(
        Label* overflow,
        Label* profile_method,
        Label* profile_method_continue) {
  Label done;
  if (TieredCompilation) {
    int increment = InvocationCounter::count_increment;
    int mask = ((1 << Tier0InvokeNotifyFreqLog)  - 1) << InvocationCounter::count_shift;
    Label no_mdo;
    if (ProfileInterpreter) {
      // Are we profiling?
      __ ld_d(FSR, Address(Rmethod, Method::method_data_offset()));
      __ beq(FSR, R0, no_mdo);
      // Increment counter in the MDO
      const Address mdo_invocation_counter(FSR, in_bytes(MethodData::invocation_counter_offset()) +
                                                in_bytes(InvocationCounter::counter_offset()));
      __ increment_mask_and_jump(mdo_invocation_counter, increment, mask, T3, false, Assembler::zero, overflow);
      __ beq(R0, R0, done);
    }
    __ bind(no_mdo);
    // Increment counter in MethodCounters
    const Address invocation_counter(FSR,
                  MethodCounters::invocation_counter_offset() +
                  InvocationCounter::counter_offset());
    __ get_method_counters(Rmethod, FSR, done);
    __ increment_mask_and_jump(invocation_counter, increment, mask, T3, false, Assembler::zero, overflow);
    __ bind(done);
  } else {
    const Address invocation_counter(FSR, in_bytes(MethodCounters::invocation_counter_offset())
        + in_bytes(InvocationCounter::counter_offset()));
    const Address backedge_counter  (FSR, in_bytes(MethodCounters::backedge_counter_offset())
        + in_bytes(InvocationCounter::counter_offset()));

    __ get_method_counters(Rmethod, FSR, done);

    if (ProfileInterpreter) { // %%% Merge this into methodDataOop
      __ ld_w(T4, FSR, in_bytes(MethodCounters::interpreter_invocation_counter_offset()));
      __ addi_d(T4, T4, 1);
      __ st_w(T4, FSR, in_bytes(MethodCounters::interpreter_invocation_counter_offset()));
    }
    // Update standard invocation counters
    __ ld_w(T3, invocation_counter);
    __ increment(T3, InvocationCounter::count_increment);
    __ st_w(T3, invocation_counter);  // save invocation count

    __ ld_w(FSR, backedge_counter);  // load backedge counter
    __ li(AT, InvocationCounter::count_mask_value);   // mask out the status bits
    __ andr(FSR, FSR, AT);

    __ add_d(T3, T3, FSR);          // add both counters

    if (ProfileInterpreter && profile_method != NULL) {
      // Test to see if we should create a method data oop
      if (Assembler::is_simm(InvocationCounter::InterpreterProfileLimit, 12)) {
        __ slti(AT, T3, InvocationCounter::InterpreterProfileLimit);
        __ bne_far(AT, R0, *profile_method_continue);
      } else {
        __ li(AT, (long)&InvocationCounter::InterpreterProfileLimit);
        __ ld_w(AT, AT, 0);
        __ blt_far(T3, AT, *profile_method_continue, true /* signed */);
      }

      // if no method data exists, go to profile_method
      __ test_method_data_pointer(FSR, *profile_method);
    }

    if (Assembler::is_simm(CompileThreshold, 12)) {
      __ srli_w(AT, T3, InvocationCounter::count_shift);
      __ slti(AT, AT, CompileThreshold);
      __ beq_far(AT, R0, *overflow);
    } else {
      __ li(AT, (long)&InvocationCounter::InterpreterInvocationLimit);
      __ ld_w(AT, AT, 0);
      __ bge_far(T3, AT, *overflow, true /* signed */);
    }

    __ bind(done);
  }
}

void InterpreterGenerator::generate_counter_overflow(Label* do_continue) {

  // Asm interpreter on entry
  // S7 - locals
  // S0 - bcp
  // Rmethod - method
  // FP - interpreter frame

  // On return (i.e. jump to entry_point)
  // Rmethod - method
  // RA - return address of interpreter caller
  // tos - the last parameter to Java method
  // SP - sender_sp

  // the bcp is valid if and only if it's not null
  __ call_VM(NOREG, CAST_FROM_FN_PTR(address,
      InterpreterRuntime::frequency_counter_overflow), R0);
  __ ld_d(Rmethod, FP, method_offset);
  // Preserve invariant that S0/S7 contain bcp/locals of sender frame
  __ b_far(*do_continue);
}

// See if we've got enough room on the stack for locals plus overhead.
// The expression stack grows down incrementally, so the normal guard
// page mechanism will work for that.
//
// NOTE: Since the additional locals are also always pushed (wasn't
// obvious in generate_method_entry) so the guard should work for them
// too.
//
// Args:
//      T2: number of additional locals this frame needs (what we must check)
//      T0: Method*
//
void InterpreterGenerator::generate_stack_overflow_check(void) {
  // see if we've got enough room on the stack for locals plus overhead.
  // the expression stack grows down incrementally, so the normal guard
  // page mechanism will work for that.
  //
  // Registers live on entry:
  //
  // T0: Method*
  // T2: number of additional locals this frame needs (what we must check)

  // NOTE:  since the additional locals are also always pushed (wasn't obvious in
  // generate_method_entry) so the guard should work for them too.
  //

  const int entry_size    = frame::interpreter_frame_monitor_size() * wordSize;

  // total overhead size: entry_size + (saved fp thru expr stack bottom).
  // be sure to change this if you add/subtract anything to/from the overhead area
  const int overhead_size = -(frame::interpreter_frame_initial_sp_offset*wordSize)
    + entry_size;

  const int page_size = os::vm_page_size();
  Label after_frame_check;

  // see if the frame is greater than one page in size. If so,
  // then we need to verify there is enough stack space remaining
  // for the additional locals.
  __ li(AT, (page_size - overhead_size) / Interpreter::stackElementSize);
  __ bge(AT, T2, after_frame_check);

  // compute sp as if this were going to be the last frame on
  // the stack before the red zone
#ifndef OPT_THREAD
  Register thread = T1;
  __ get_thread(thread);
#else
  Register thread = TREG;
#endif

  // locals + overhead, in bytes
  __ slli_d(T3, T2, Interpreter::stackElementScale());
  __ addi_d(T3, T3, overhead_size);   // locals * 4 + overhead_size --> T3

#ifdef ASSERT
  Label stack_base_okay, stack_size_okay;
  // verify that thread stack base is non-zero
  __ ld_d(AT, thread, in_bytes(Thread::stack_base_offset()));
  __ bne(AT, R0, stack_base_okay);
  __ stop("stack base is zero");
  __ bind(stack_base_okay);
  // verify that thread stack size is non-zero
  __ ld_d(AT, thread, in_bytes(Thread::stack_size_offset()));
  __ bne(AT, R0, stack_size_okay);
  __ stop("stack size is zero");
  __ bind(stack_size_okay);
#endif

  // Add stack base to locals and subtract stack size
  __ ld_d(AT, thread, in_bytes(Thread::stack_base_offset())); // stack_base --> AT
  __ add_d(T3, T3, AT);   // locals * 4 + overhead_size + stack_base--> T3
  __ ld_d(AT, thread, in_bytes(Thread::stack_size_offset()));  // stack_size --> AT
  __ sub_d(T3, T3, AT);  // locals * 4 + overhead_size + stack_base - stack_size --> T3


  // add in the redzone and yellow size
  __ li(AT, (StackRedPages+StackYellowPages) * page_size);
  __ add_d(T3, T3, AT);

  // check against the current stack bottom
  __ blt(T3, SP, after_frame_check);

  // Note: the restored frame is not necessarily interpreted.
  // Use the shared runtime version of the StackOverflowError.
  __ move(SP, Rsender);
  assert(StubRoutines::throw_StackOverflowError_entry() != NULL, "stub not yet generated");
  __ jmp(StubRoutines::throw_StackOverflowError_entry(), relocInfo::runtime_call_type);

  // all done with frame size check
  __ bind(after_frame_check);
}

// Allocate monitor and lock method (asm interpreter)
// Rmethod - Method*
void InterpreterGenerator::lock_method(void) {
  // synchronize method
  const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;

#ifdef ASSERT
  { Label L;
    __ ld_w(T0, Rmethod, in_bytes(Method::access_flags_offset()));
    __ andi(T0, T0, JVM_ACC_SYNCHRONIZED);
    __ bne(T0, R0, L);
    __ stop("method doesn't need synchronization");
    __ bind(L);
  }
#endif // ASSERT
  // get synchronization object
  {
    Label done;
    const int mirror_offset = in_bytes(Klass::java_mirror_offset());
    __ ld_w(T0, Rmethod, in_bytes(Method::access_flags_offset()));
    __ andi(T2, T0, JVM_ACC_STATIC);
    __ ld_d(T0, LVP, Interpreter::local_offset_in_bytes(0));
    __ beq(T2, R0, done);
    __ ld_d(T0, Rmethod, in_bytes(Method::const_offset()));
    __ ld_d(T0, T0, in_bytes(ConstMethod::constants_offset()));
    __ ld_d(T0, T0, ConstantPool::pool_holder_offset_in_bytes());
    __ ld_d(T0, T0, mirror_offset);
    __ bind(done);
  }
  // add space for monitor & lock
  __ addi_d(SP, SP, (-1) * entry_size);           // add space for a monitor entry
  __ st_d(SP, FP, frame::interpreter_frame_monitor_block_top_offset * wordSize);
  // set new monitor block top
  __ st_d(T0, SP, BasicObjectLock::obj_offset_in_bytes());   // store object
  // FIXME: I do not know what lock_object will do and what it will need
  __ move(c_rarg0, SP);      // object address
  __ lock_object(c_rarg0);
}

/**
 * Method entry for static native methods:
 *   int java.util.zip.CRC32.update(int crc, int b)
 */
address InterpreterGenerator::generate_CRC32_update_entry() {
  if (UseCRC32Intrinsics) {
    address entry = __ pc();

    // rmethod: Method*
    // Rsender: senderSP must preserved for slow path
    // SP: args

    Label slow_path;
    // If we need a safepoint check, generate full interpreter entry.
    __ li(AT, SafepointSynchronize::_not_synchronized);
    __ li(T8, (long)SafepointSynchronize::address_of_state());
    __ bne(T8, AT, slow_path);

    // We don't generate local frame and don't align stack because
    // we call stub code and there is no safepoint on this path.

    const Register crc = A0;  // crc
    const Register val = A1;  // source java byte value
    const Register tbl = A2;  // scratch

    // Arguments are reversed on java expression stack
    __ ld_w(val, SP, 0);              // byte value
    __ ld_w(crc, SP, wordSize);       // Initial CRC

    __ li(tbl, (long)StubRoutines::crc_table_addr());

    __ nor(crc, crc, R0); // ~crc
    __ update_byte_crc32(crc, val, tbl);
    __ nor(crc, crc, R0); // ~crc

    // restore caller SP
    __ move(SP, Rsender);
    __ jr(RA);

    // generate a vanilla native entry as the slow path
    __ bind(slow_path);

    (void) generate_native_entry(false);

    return entry;
  }
  return generate_native_entry(false);
}

/**
 * Method entry for static native methods:
 *   int java.util.zip.CRC32.updateBytes(int crc, byte[] b, int off, int len)
 *   int java.util.zip.CRC32.updateByteBuffer(int crc, long buf, int off, int len)
 */
address InterpreterGenerator::generate_CRC32_updateBytes_entry(AbstractInterpreter::MethodKind kind) {
  if (UseCRC32Intrinsics) {
    address entry = __ pc();

    // rmethod: Method*
    // Rsender: senderSP must preserved for slow path
    // SP: args

    Label slow_path;
    // If we need a safepoint check, generate full interpreter entry.
    __ li(AT, SafepointSynchronize::_not_synchronized);
    __ li(T8, (long)SafepointSynchronize::address_of_state());
    __ bne(T8, AT, slow_path);

    // We don't generate local frame and don't align stack because
    // we call stub code and there is no safepoint on this path.

    const Register crc = A0;  // crc
    const Register buf = A1;  // source java byte array address
    const Register len = A2;  // length
    const Register tmp = A3;

    const Register off = len; // offset (never overlaps with 'len')

    // Arguments are reversed on java expression stack
    // Calculate address of start element
    __ ld_w(off, SP, wordSize);       // int offset
    __ ld_d(buf, SP, 2 * wordSize);   // byte[] buf | long buf
    __ add_d(buf, buf, off);          // + offset
    if (kind == Interpreter::java_util_zip_CRC32_updateByteBuffer) {
      __ ld_w(crc, SP, 4 * wordSize); // long crc
    } else {
      __ addi_d(buf, buf, arrayOopDesc::base_offset_in_bytes(T_BYTE)); // + header size
      __ ld_w(crc, SP, 3 * wordSize); // long crc
    }

    // Can now load 'len' since we're finished with 'off'
    __ ld_w(len, SP, 0); // length

    __ kernel_crc32(crc, buf, len, tmp);

    // restore caller SP
    __ move(SP, Rsender);
    __ jr(RA);

    // generate a vanilla native entry as the slow path
    __ bind(slow_path);

    (void) generate_native_entry(false);

    return entry;
  }
  return generate_native_entry(false);
}

// Generate a fixed interpreter frame. This is identical setup for
// interpreted methods and for native methods hence the shared code.
void TemplateInterpreterGenerator::generate_fixed_frame(bool native_call) {

  // [ local var m-1      ] <--- sp
  //   ...
  // [ local var 0        ]
  // [ argumnet word n-1  ] <--- T0(sender's sp)
  //   ...
  // [ argument word 0    ] <--- S7

  // initialize fixed part of activation frame
  // sender's sp in Rsender
  int i = 0;
  int frame_size = 9;
#ifndef CORE
  ++frame_size;
#endif
  __ addi_d(SP, SP, (-frame_size) * wordSize);
  __ st_d(RA, SP, (frame_size - 1) * wordSize);   // save return address
  __ st_d(FP, SP, (frame_size - 2) * wordSize);  // save sender's fp
  __ addi_d(FP, SP, (frame_size - 2) * wordSize);
  __ st_d(Rsender, FP, (-++i) * wordSize);  // save sender's sp
  __ st_d(R0, FP,(-++i) * wordSize);       //save last_sp as null
  __ st_d(LVP, FP, (-++i) * wordSize);  // save locals offset
  __ ld_d(BCP, Rmethod, in_bytes(Method::const_offset())); // get constMethodOop
  __ addi_d(BCP, BCP, in_bytes(ConstMethod::codes_offset())); // get codebase
  __ st_d(Rmethod, FP, (-++i) * wordSize);                              // save Method*
#ifndef CORE
  if (ProfileInterpreter) {
    Label method_data_continue;
    __ ld_d(AT, Rmethod,  in_bytes(Method::method_data_offset()));
    __ beq(AT, R0, method_data_continue);
    __ addi_d(AT, AT, in_bytes(MethodData::data_offset()));
    __ bind(method_data_continue);
    __ st_d(AT, FP,  (-++i) * wordSize);
  } else {
    __ st_d(R0, FP, (-++i) * wordSize);
  }
#endif // !CORE

  __ ld_d(T2, Rmethod, in_bytes(Method::const_offset()));
  __ ld_d(T2, T2, in_bytes(ConstMethod::constants_offset()));
  __ ld_d(T2, T2, ConstantPool::cache_offset_in_bytes());
  __ st_d(T2, FP, (-++i) * wordSize);                    // set constant pool cache
  if (native_call) {
    __ st_d(R0, FP, (-++i) * wordSize);          // no bcp
  } else {
    __ st_d(BCP, FP, (-++i) * wordSize);          // set bcp
  }
  __ st_d(SP, FP, (-++i) * wordSize);               // reserve word for pointer to expression stack bottom
  assert(i + 2 == frame_size, "i + 2 should be equal to frame_size");
}

// End of helpers

// Various method entries
//------------------------------------------------------------------------------------------------------------------------
//
//

// Call an accessor method (assuming it is resolved, otherwise drop
// into vanilla (slow path) entry
address InterpreterGenerator::generate_accessor_entry(void) {
  // Rmethod: Method*
  // V0: receiver (preserve for slow entry into asm interpreter)
  //  Rsender: senderSP must preserved for slow path, set SP to it on fast path

  address entry_point = __ pc();
  Label xreturn_path;
  // do fastpath for resolved accessor methods
  if (UseFastAccessorMethods) {
    Label slow_path;
    __ li(T2, SafepointSynchronize::address_of_state());
    __ ld_w(AT, T2, 0);
    __ addi_d(AT, AT, -(SafepointSynchronize::_not_synchronized));
    __ bne(AT, R0, slow_path);
    // Code: _aload_0, _(i|a)getfield, _(i|a)return or any rewrites thereof;
    // parameter size = 1
    // Note: We can only use this code if the getfield has been resolved
    //       and if we don't have a null-pointer exception => check for
    //       these conditions first and use slow path if necessary.
    // Rmethod: method
    // V0: receiver

    // [ receiver  ] <-- sp
    __ ld_d(T0, SP, 0);

    // check if local 0 != NULL and read field
    __ beq(T0, R0, slow_path);
    __ ld_d(T2, Rmethod, in_bytes(Method::const_offset()));
    __ ld_d(T2, T2, in_bytes(ConstMethod::constants_offset()));
    // read first instruction word and extract bytecode @ 1 and index @ 2
    __ ld_d(T3, Rmethod, in_bytes(Method::const_offset()));
    __ ld_w(T3, T3, in_bytes(ConstMethod::codes_offset()));
    // Shift codes right to get the index on the right.
    // The bytecode fetched looks like <index><0xb4><0x2a>
    __ srli_d(T3, T3, 2 * BitsPerByte);
    // FIXME: maybe it's wrong
    __ slli_d(T3, T3, exact_log2(in_words(ConstantPoolCacheEntry::size())));
    __ ld_d(T2, T2, ConstantPool::cache_offset_in_bytes());

    // T0: local 0
    // Rmethod: method
    // V0: receiver - do not destroy since it is needed for slow path!
    // T1: scratch use which register instead ?
    // T3: constant pool cache index
    // T2: constant pool cache
    // Rsender: send's sp
    // check if getfield has been resolved and read constant pool cache entry
    // check the validity of the cache entry by testing whether _indices field
    // contains Bytecode::_getfield in b1 byte.
    assert(in_words(ConstantPoolCacheEntry::size()) == 4, "adjust shift below");

    __ slli_d(T8, T3, Address::times_8);
    __ li(T1, in_bytes(ConstantPoolCache::base_offset()
    + ConstantPoolCacheEntry::indices_offset()));
    __ add_d(T1, T8, T1);
    __ ldx_w(T1, T1, T2);
    __ srli_d(T1, T1, 2 * BitsPerByte);
    __ andi(T1, T1, 0xFF);
    __ addi_d(T1, T1, (-1) * Bytecodes::_getfield);
    __ bne(T1, R0, slow_path);

    // Note: constant pool entry is not valid before bytecode is resolved

    __ li(T1, in_bytes(ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::f2_offset()));
    __ add_d(T1, T1, T8);
    __ ldx_w(AT, T1, T2);

    __ li(T1, in_bytes(ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::flags_offset()));
    __ add_d(T1, T1, T8);
    __ ldx_w(T3, T1, T2);

    Label notByte, notBool, notShort, notChar, notObj;

    // Need to differentiate between igetfield, agetfield, bgetfield etc.
    // because they are different sizes.
    // Use the type from the constant pool cache
    __ srli_w(T3, T3, ConstantPoolCacheEntry::tos_state_shift);
    // Make sure we don't need to mask T3 for tosBits after the above shift
    ConstantPoolCacheEntry::verify_tos_state_shift();
    // btos = 0
    __ add_d(T0, T0, AT);
    __ bne(T3, R0, notByte);

    __ ld_b(V0, T0, 0);
    __ b(xreturn_path);

    //ztos
    __ bind(notByte);
    __ addi_d(T1, T3, (-1) * ztos);
    __ bne(T1, R0, notBool);
    __ ld_b(V0, T0, 0);
    __ b(xreturn_path);

    //stos
    __ bind(notBool);
    __ addi_d(T1, T3, (-1) * stos);
    __ bne(T1, R0, notShort);
    __ ld_h(V0, T0, 0);
    __ b(xreturn_path);

    //ctos
    __ bind(notShort);
    __ addi_d(T1, T3, (-1) * ctos);
    __ bne(T1, R0, notChar);
    __ ld_hu(V0, T0, 0);
    __ b(xreturn_path);

    //atos
    __ bind(notChar);
    __ addi_d(T1, T3, (-1) * atos);
    __ bne(T1, R0, notObj);
    //add for compressedoops
    __ load_heap_oop(V0, Address(T0, 0));
    __ b(xreturn_path);

    //itos
    __ bind(notObj);
#ifdef ASSERT
    Label okay;
    __ addi_d(T1, T3, (-1) * itos);
    __ beq(T1, R0, okay);
    __ stop("what type is this?");
    __ bind(okay);
#endif // ASSERT
    __ ld_w(V0, T0, 0);

    __ bind(xreturn_path);

    // _ireturn/_areturn
    //FIXME
    __ move(SP, Rsender);//FIXME, set sender's fp to SP
    __ jr(RA);

    // generate a vanilla interpreter entry as the slow path
    __ bind(slow_path);
    (void) generate_normal_entry(false);
  } else {
    (void) generate_normal_entry(false);
  }
  return entry_point;
}

// Method entry for java.lang.ref.Reference.get.
address InterpreterGenerator::generate_Reference_get_entry(void) {
#if INCLUDE_ALL_GCS
  // Code: _aload_0, _getfield, _areturn
  // parameter size = 1
  //
  // The code that gets generated by this routine is split into 2 parts:
  //    1. The "intrinsified" code for G1 (or any SATB based GC),
  //    2. The slow path - which is an expansion of the regular method entry.
  //
  // Notes:-
  // * In the G1 code we do not check whether we need to block for
  //   a safepoint. If G1 is enabled then we must execute the specialized
  //   code for Reference.get (except when the Reference object is null)
  //   so that we can log the value in the referent field with an SATB
  //   update buffer.
  //   If the code for the getfield template is modified so that the
  //   G1 pre-barrier code is executed when the current method is
  //   Reference.get() then going through the normal method entry
  //   will be fine.
  // * The G1 code can, however, check the receiver object (the instance
  //   of java.lang.Reference) and jump to the slow path if null. If the
  //   Reference object is null then we obviously cannot fetch the referent
  //   and so we don't need to call the G1 pre-barrier. Thus we can use the
  //   regular method entry code to generate the NPE.
  //
  // This code is based on generate_accessor_enty.
  //
  // Rmethod: Method*

  // Rsender: senderSP must preserve for slow path, set SP to it on fast path (Rsender)

  address entry = __ pc();

  const int referent_offset = java_lang_ref_Reference::referent_offset;
  guarantee(referent_offset > 0, "referent offset not initialized");
  if (UseG1GC) {
    Label slow_path;

    // Check if local 0 != NULL
    // If the receiver is null then it is OK to jump to the slow path.
    __ ld_d(V0, SP, 0);

    __ beq(V0, R0, slow_path);

    // Generate the G1 pre-barrier code to log the value of
    // the referent field in an SATB buffer.

    // Load the value of the referent field.
    const Address field_address(V0, referent_offset);
    __ load_heap_oop(V0, field_address);

    __ push(RA);
    // Generate the G1 pre-barrier code to log the value of
    // the referent field in an SATB buffer.
    __ g1_write_barrier_pre(noreg /* obj */,
                            V0 /* pre_val */,
                            TREG /* thread */,
                            Rmethod /* tmp */,
                            true /* tosca_live */,
                            true /* expand_call */);
    __ pop(RA);

    __ add_d(SP, Rsender, R0);      // set sp to sender sp
    __ jr(RA);

    // generate a vanilla interpreter entry as the slow path
    __ bind(slow_path);
    (void) generate_normal_entry(false);

    return entry;
  }
#endif // INCLUDE_ALL_GCS

  // If G1 is not enabled then attempt to go through the accessor entry point
  // Reference.get is an accessor
  return generate_accessor_entry();
}

// Interpreter stub for calling a native method. (asm interpreter)
// This sets up a somewhat different looking stack for calling the
// native method than the typical interpreter frame setup.
address InterpreterGenerator::generate_native_entry(bool synchronized) {
  // determine code generation flags
  bool inc_counter  = UseCompiler || CountCompiledCalls;
  // Rsender: sender's sp
  // Rmethod: Method*
  address entry_point = __ pc();

#ifndef CORE
  const Address invocation_counter(Rmethod,in_bytes(MethodCounters::invocation_counter_offset() +
  InvocationCounter::counter_offset()));
#endif
  // get parameter size (always needed)
  // the size in the java stack
  __ ld_d(V0, Rmethod, in_bytes(Method::const_offset()));
  __ ld_hu(V0, V0, in_bytes(ConstMethod::size_of_parameters_offset()));

  // native calls don't need the stack size check since they have no expression stack
  // and the arguments are already on the stack and we only add a handful of words
  // to the stack

  // Rmethod: Method*
  // V0: size of parameters
  // Layout of frame at this point
  //
  // [ argument word n-1  ] <--- sp
  //   ...
  // [ argument word 0    ]

  // for natives the size of locals is zero

  // compute beginning of parameters (S7)
  __ slli_d(LVP, V0, Address::times_8);
  __ addi_d(LVP, LVP, (-1) * wordSize);
  __ add_d(LVP, LVP, SP);


  // add 2 zero-initialized slots for native calls
  // 1 slot for native oop temp offset (setup via runtime)
  // 1 slot for static native result handler3 (setup via runtime)
  __ push2(R0, R0);

  // Layout of frame at this point
  // [ method holder mirror  ] <--- sp
  // [ result type info      ]
  // [ argument word n-1     ] <--- T0
  //   ...
  // [ argument word 0       ] <--- LVP


#ifndef CORE
  if (inc_counter) __ ld_w(T3, invocation_counter);  // (pre-)fetch invocation count
#endif

  // initialize fixed part of activation frame
  generate_fixed_frame(true);
  // after this function, the layout of frame is as following
  //
  // [ monitor block top        ] <--- sp ( the top monitor entry )
  // [ byte code pointer (0)    ] (if native, bcp = 0)
  // [ constant pool cache      ]
  // [ Method*                  ]
  // [ locals offset            ]
  // [ sender's sp              ]
  // [ sender's fp              ]
  // [ return address           ] <--- fp
  // [ method holder mirror     ]
  // [ result type info         ]
  // [ argumnet word n-1        ] <--- sender's sp
  //   ...
  // [ argument word 0          ] <--- S7


  // make sure method is native & not abstract
#ifdef ASSERT
  __ ld_w(T0, Rmethod, in_bytes(Method::access_flags_offset()));
  {
    Label L;
    __ andi(AT, T0, JVM_ACC_NATIVE);
    __ bne(AT, R0, L);
    __ stop("tried to execute native method as non-native");
    __ bind(L);
  }
  {
    Label L;
    __ andi(AT, T0, JVM_ACC_ABSTRACT);
    __ beq(AT, R0, L);
    __ stop("tried to execute abstract method in interpreter");
    __ bind(L);
  }
#endif

  // Since at this point in the method invocation the exception handler
  // would try to exit the monitor of synchronized methods which hasn't
  // been entered yet, we set the thread local variable
  // _do_not_unlock_if_synchronized to true. The remove_activation will
  // check this flag.
  Register thread = TREG;
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ li(AT, (int)true);
  __ st_b(AT, thread, in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));

#ifndef CORE
  // increment invocation count & check for overflow
  Label invocation_counter_overflow;
  if (inc_counter) {
    generate_counter_incr(&invocation_counter_overflow, NULL, NULL);
  }

  Label continue_after_compile;
  __ bind(continue_after_compile);
#endif // CORE

  bang_stack_shadow_pages(true);

  // reset the _do_not_unlock_if_synchronized flag
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ st_b(R0, thread, in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));

  // check for synchronized methods
  // Must happen AFTER invocation_counter check and stack overflow check,
  // so method is not locked if overflows.
  if (synchronized) {
    lock_method();
  } else {
    // no synchronization necessary
#ifdef ASSERT
    {
      Label L;
      __ ld_w(T0, Rmethod, in_bytes(Method::access_flags_offset()));
      __ andi(AT, T0, JVM_ACC_SYNCHRONIZED);
      __ beq(AT, R0, L);
      __ stop("method needs synchronization");
      __ bind(L);
    }
#endif
  }

  // after method_lock, the layout of frame is as following
  //
  // [ monitor entry            ] <--- sp
  //   ...
  // [ monitor entry            ]
  // [ monitor block top        ] ( the top monitor entry )
  // [ byte code pointer (0)    ] (if native, bcp = 0)
  // [ constant pool cache      ]
  // [ Method*                  ]
  // [ locals offset            ]
  // [ sender's sp              ]
  // [ sender's fp              ]
  // [ return address           ] <--- fp
  // [ method holder mirror     ]
  // [ result type info         ]
  // [ argumnet word n-1        ] <--- ( sender's sp )
  //   ...
  // [ argument word 0          ] <--- S7

  // start execution
#ifdef ASSERT
  {
    Label L;
    __ ld_d(AT, FP, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    __ beq(AT, SP, L);
    __ stop("broken stack frame setup in interpreter in asm");
    __ bind(L);
  }
#endif

  // jvmti/jvmpi support
  __ notify_method_entry();

  // work registers
  const Register method = Rmethod;
  //const Register thread = T2;
  const Register t      = T8;

  __ get_method(method);
  __ verify_oop(method);
  {
    Label L, Lstatic;
    __ ld_d(t,method,in_bytes(Method::const_offset()));
    __ ld_hu(t, t, in_bytes(ConstMethod::size_of_parameters_offset()));
    // LoongArch ABI: caller does not reserve space for the register auguments.
    // A0 and A1(if needed)
    __ ld_w(AT, Rmethod, in_bytes(Method::access_flags_offset()));
    __ andi(AT, AT, JVM_ACC_STATIC);
    __ beq(AT, R0, Lstatic);
    __ addi_d(t, t, 1);
    __ bind(Lstatic);
    __ addi_d(t, t, -7);
    __ bge(R0, t, L);
    __ slli_d(t, t, Address::times_8);
    __ sub_d(SP, SP, t);
    __ bind(L);
  }
  __ li(AT, -(StackAlignmentInBytes));
  __ andr(SP, SP, AT);
  __ move(AT, SP);
  // [                          ] <--- sp
  //   ...                        (size of parameters - 8 )
  // [ monitor entry            ]
  //   ...
  // [ monitor entry            ]
  // [ monitor block top        ] ( the top monitor entry )
  // [ byte code pointer (0)    ] (if native, bcp = 0)
  // [ constant pool cache      ]
  // [ Method*                  ]
  // [ locals offset            ]
  // [ sender's sp              ]
  // [ sender's fp              ]
  // [ return address           ] <--- fp
  // [ method holder mirror     ]
  // [ result type info         ]
  // [ argumnet word n-1        ] <--- ( sender's sp )
  //   ...
  // [ argument word 0          ] <--- LVP

  // get signature handler
  {
    Label L;
    __ ld_d(T4, method, in_bytes(Method::signature_handler_offset()));
    __ bne(T4, R0, L);
    __ call_VM(NOREG, CAST_FROM_FN_PTR(address,
               InterpreterRuntime::prepare_native_call), method);
    __ get_method(method);
    __ ld_d(T4, method, in_bytes(Method::signature_handler_offset()));
    __ bind(L);
  }

  // call signature handler
  // FIXME: when change codes in InterpreterRuntime, note this point
  // from: begin of parameters
  assert(InterpreterRuntime::SignatureHandlerGenerator::from() == LVP, "adjust this code");
  // to: current sp
  assert(InterpreterRuntime::SignatureHandlerGenerator::to  () == SP, "adjust this code");
  // temp: T3
  assert(InterpreterRuntime::SignatureHandlerGenerator::temp() == t  , "adjust this code");

  __ jalr(T4);
  __ get_method(method);

  //
  // if native function is static, and its second parameter has type length of double word,
  // and first parameter has type length of word, we have to reserve one word
  // for the first parameter, according to LoongArch abi.
  // if native function is not static, and its third parameter has type length of double word,
  // and second parameter has type length of word, we have to reserve one word for the second
  // parameter.
  //


  // result handler is in V0
  // set result handler
  __ st_d(V0, FP, (frame::interpreter_frame_result_handler_offset)*wordSize);

#define FIRSTPARA_SHIFT_COUNT 5
#define SECONDPARA_SHIFT_COUNT 9
#define THIRDPARA_SHIFT_COUNT 13
#define PARA_MASK  0xf

  // pass mirror handle if static call
  {
    Label L;
    const int mirror_offset = in_bytes(Klass::java_mirror_offset());
    __ ld_w(t, method, in_bytes(Method::access_flags_offset()));
    __ andi(AT, t, JVM_ACC_STATIC);
    __ beq(AT, R0, L);

    // get mirror
    __ ld_d(t, method, in_bytes(Method:: const_offset()));
    __ ld_d(t, t, in_bytes(ConstMethod::constants_offset())); //??
    __ ld_d(t, t, ConstantPool::pool_holder_offset_in_bytes());
    __ ld_d(t, t, mirror_offset);
    // copy mirror into activation frame
    //__ st_w(t, FP, frame::interpreter_frame_oop_temp_offset * wordSize);
    // pass handle to mirror
    __ st_d(t, FP, frame::interpreter_frame_oop_temp_offset * wordSize);
    __ addi_d(t, FP, frame::interpreter_frame_oop_temp_offset * wordSize);
    __ move(A1, t);
    __ bind(L);
  }

  // [ mthd holder mirror ptr   ] <--- sp  --------------------| (only for static method)
  // [                          ]                              |
  //   ...                        size of parameters(or +1)    |
  // [ monitor entry            ]                              |
  //   ...                                                     |
  // [ monitor entry            ]                              |
  // [ monitor block top        ] ( the top monitor entry )    |
  // [ byte code pointer (0)    ] (if native, bcp = 0)         |
  // [ constant pool cache      ]                              |
  // [ Method*                  ]                              |
  // [ locals offset            ]                              |
  // [ sender's sp              ]                              |
  // [ sender's fp              ]                              |
  // [ return address           ] <--- fp                      |
  // [ method holder mirror     ] <----------------------------|
  // [ result type info         ]
  // [ argumnet word n-1        ] <--- ( sender's sp )
  //   ...
  // [ argument word 0          ] <--- S7

  // get native function entry point
  { Label L;
    __ ld_d(T4, method, in_bytes(Method::native_function_offset()));
    __ li(T6, SharedRuntime::native_method_throw_unsatisfied_link_error_entry());
    __ bne(T6, T4, L);
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::prepare_native_call), method);
    __ get_method(method);
    __ verify_oop(method);
    __ ld_d(T4, method, in_bytes(Method::native_function_offset()));
    __ bind(L);
  }

  // pass JNIEnv
  // native function in T4
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ addi_d(t, thread, in_bytes(JavaThread::jni_environment_offset()));
  __ move(A0, t);
  // [ jni environment          ] <--- sp
  // [ mthd holder mirror ptr   ] ---------------------------->| (only for static method)
  // [                          ]                              |
  //   ...                        size of parameters           |
  // [ monitor entry            ]                              |
  //   ...                                                     |
  // [ monitor entry            ]                              |
  // [ monitor block top        ] ( the top monitor entry )    |
  // [ byte code pointer (0)    ] (if native, bcp = 0)         |
  // [ constant pool cache      ]                              |
  // [ Method*                  ]                              |
  // [ locals offset            ]                              |
  // [ sender's sp              ]                              |
  // [ sender's fp              ]                              |
  // [ return address           ] <--- fp                      |
  // [ method holder mirror     ] <----------------------------|
  // [ result type info         ]
  // [ argumnet word n-1        ] <--- ( sender's sp )
  //   ...
  // [ argument word 0          ] <--- S7

  // Set the last Java PC in the frame anchor to be the return address from
  // the call to the native method: this will allow the debugger to
  // generate an accurate stack trace.
  Label native_return;
  __ set_last_Java_frame(thread, SP, FP, native_return);

  // change thread state
#ifdef ASSERT
  {
    Label L;
    __ ld_w(t, thread, in_bytes(JavaThread::thread_state_offset()));
    __ addi_d(t, t, (-1) * _thread_in_Java);
    __ beq(t, R0, L);
    __ stop("Wrong thread state in native stub");
    __ bind(L);
  }
#endif

  __ li(t, _thread_in_native);
  if (os::is_MP()) {
    __ membar(Assembler::Membar_mask_bits(__ LoadStore|__ StoreStore)); // store release
  }
  __ st_w(t, thread, in_bytes(JavaThread::thread_state_offset()));

  // call native method
  __ jalr(T4);
  __ bind(native_return);
  // result potentially in V0 or F0


  // via _last_native_pc and not via _last_jave_sp
  // NOTE: the order of theses push(es) is known to frame::interpreter_frame_result.
  //  If the order changes or anything else is added to the stack the code in
  // interpreter_frame_result will have to be changed.
  //FIXME, should modify here
  // save return value to keep the value from being destroyed by other calls
  __ push(dtos);
  __ push(ltos);

  // change thread state
  __ get_thread(thread);
  __ li(t, _thread_in_native_trans);
  if (os::is_MP()) {
    __ membar(Assembler::Membar_mask_bits(__ LoadStore|__ StoreStore)); // store release
  }
  __ st_w(t, thread, in_bytes(JavaThread::thread_state_offset()));

  if(os::is_MP()) {
    if (UseMembar) {
      // Force this write out before the read below
      __ membar(__ AnyAny);
    } else {
      // Write serialization page so VM thread can do a pseudo remote membar.
      // We use the current thread pointer to calculate a thread specific
      // offset to write to within the page. This minimizes bus traffic
      // due to cache line collision.
      __ serialize_memory(thread, A0);
    }
  }

  // check for safepoint operation in progress and/or pending suspend requests
  { Label Continue;

    // Don't use call_VM as it will see a possible pending exception and forward it
    // and never return here preventing us from clearing _last_native_pc down below.
    // Also can't use call_VM_leaf either as it will check to see if BCP & LVP are
    // preserved and correspond to the bcp/locals pointers. So we do a runtime call
    // by hand.
    //
    Label L;
    __ li(AT, SafepointSynchronize::address_of_state());
    __ ld_w(AT, AT, 0);
    __ bne(AT, R0, L);
    __ ld_w(AT, thread, in_bytes(JavaThread::suspend_flags_offset()));
    __ beq(AT, R0, Continue);
    __ bind(L);
    __ move(A0, thread);
    __ call(CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans),
                             relocInfo::runtime_call_type);

#ifndef OPT_THREAD
    __ get_thread(thread);
#endif
    //add for compressedoops
    __ reinit_heapbase();
    __ bind(Continue);
  }

  // change thread state
  __ li(t, _thread_in_Java);
  if (os::is_MP()) {
    __ membar(Assembler::Membar_mask_bits(__ LoadStore|__ StoreStore)); // store release
  }
  __ st_w(t, thread, in_bytes(JavaThread::thread_state_offset()));
  __ reset_last_Java_frame(thread, true);

  // reset handle block
  __ ld_d(t, thread, in_bytes(JavaThread::active_handles_offset()));
  __ st_w(R0, t, JNIHandleBlock::top_offset_in_bytes());

  // If result was an oop then unbox and save it in the frame
  {
    Label no_oop;
    //FIXME, addi only support 12-bit imeditate
    __ ld_d(AT, FP, frame::interpreter_frame_result_handler_offset*wordSize);
    __ li(T0, AbstractInterpreter::result_handler(T_OBJECT));
    __ bne(AT, T0, no_oop);
    __ pop(ltos);
    // Unbox oop result, e.g. JNIHandles::resolve value.
    __ resolve_jobject(V0, thread, T4);
    __ st_d(V0, FP, (frame::interpreter_frame_oop_temp_offset)*wordSize);
    // keep stack depth as expected by pushing oop which will eventually be discarded
    __ push(ltos);
    __ bind(no_oop);
  }
  {
    Label no_reguard;
    __ ld_w(t, thread, in_bytes(JavaThread::stack_guard_state_offset()));
    __ li(AT,(int) JavaThread::stack_guard_yellow_disabled);
    __ bne(t, AT, no_reguard);
    __ pushad();
    __ move(S5_heapbase, SP);
    __ li(AT, -StackAlignmentInBytes);
    __ andr(SP, SP, AT);
    __ call(CAST_FROM_FN_PTR(address, SharedRuntime::reguard_yellow_pages), relocInfo::runtime_call_type);
    __ move(SP, S5_heapbase);
    __ popad();
    //add for compressedoops
    __ reinit_heapbase();
    __ bind(no_reguard);
  }
  // restore BCP to have legal interpreter frame,
  // i.e., bci == 0 <=> BCP == code_base()
  // Can't call_VM until bcp is within reasonable.
  __ get_method(method);      // method is junk from thread_in_native to now.
  __ verify_oop(method);
  __ ld_d(BCP, method, in_bytes(Method::const_offset()));
  __ lea(BCP, Address(BCP, in_bytes(ConstMethod::codes_offset())));
  // handle exceptions (exception handling will handle unlocking!)
  {
    Label L;
    __ ld_d(t, thread, in_bytes(Thread::pending_exception_offset()));
    __ beq(t, R0, L);
    // Note: At some point we may want to unify this with the code used in
    // call_VM_base();
    // i.e., we should use the StubRoutines::forward_exception code. For now this
    // doesn't work here because the sp is not correctly set at this point.
    __ MacroAssembler::call_VM(noreg,
                               CAST_FROM_FN_PTR(address,
                               InterpreterRuntime::throw_pending_exception));
    __ should_not_reach_here();
    __ bind(L);
  }

  // do unlocking if necessary
  {
    Label L;
    __ ld_w(t, method, in_bytes(Method::access_flags_offset()));
    __ andi(t, t, JVM_ACC_SYNCHRONIZED);
    __ addi_d(c_rarg0, FP, frame::interpreter_frame_initial_sp_offset * wordSize - (int)sizeof(BasicObjectLock));
    __ beq(t, R0, L);
    // the code below should be shared with interpreter macro assembler implementation
    {
      Label unlock;
      // BasicObjectLock will be first in list,
      // since this is a synchronized method. However, need
      // to check that the object has not been unlocked by
      // an explicit monitorexit bytecode.
      // address of first monitor

      __ ld_d(t, c_rarg0, BasicObjectLock::obj_offset_in_bytes());
      __ bne(t, R0, unlock);

      // Entry already unlocked, need to throw exception
      __ MacroAssembler::call_VM(NOREG, CAST_FROM_FN_PTR(address,
      InterpreterRuntime::throw_illegal_monitor_state_exception));
      __ should_not_reach_here();

      __ bind(unlock);
      __ unlock_object(c_rarg0);
    }
    __ bind(L);
  }

  // jvmti/jvmpi support
  // Note: This must happen _after_ handling/throwing any exceptions since
  //       the exception handler code notifies the runtime of method exits
  //       too. If this happens before, method entry/exit notifications are
  //       not properly paired (was bug - gri 11/22/99).
  __ notify_method_exit(vtos, InterpreterMacroAssembler::NotifyJVMTI);

  // restore potential result in V0,
  // call result handler to restore potential result in ST0 & handle result

  __ pop(ltos);
  __ pop(dtos);

  __ ld_d(t, FP, (frame::interpreter_frame_result_handler_offset) * wordSize);
  __ jalr(t);


  // remove activation
  __ ld_d(SP, FP, frame::interpreter_frame_sender_sp_offset * wordSize); // get sender sp
  __ ld_d(RA, FP, frame::interpreter_frame_return_addr_offset * wordSize); // get return address
  __ ld_d(FP, FP, frame::interpreter_frame_sender_fp_offset * wordSize); // restore sender's fp
  __ jr(RA);

#ifndef CORE
  if (inc_counter) {
    // Handle overflow of counter and compile method
    __ bind(invocation_counter_overflow);
    generate_counter_overflow(&continue_after_compile);
    // entry_point is the beginning of this
    // function and checks again for compiled code
  }
#endif
  return entry_point;
}

//
// Generic interpreted method entry to (asm) interpreter
//
// Layout of frame just at the entry
//
//   [ argument word n-1  ] <--- sp
//     ...
//   [ argument word 0    ]
// assume Method* in Rmethod before call this method.
// prerequisites to the generated stub : the callee Method* in Rmethod
// note you must save the caller bcp before call the generated stub
//
address InterpreterGenerator::generate_normal_entry(bool synchronized) {
  // determine code generation flags
  bool inc_counter  = UseCompiler || CountCompiledCalls;

  // Rmethod: Method*
  // Rsender: sender 's sp
  address entry_point = __ pc();

  const Address invocation_counter(Rmethod,
      in_bytes(MethodCounters::invocation_counter_offset() + InvocationCounter::counter_offset()));

  // get parameter size (always needed)
  __ ld_d(T3, Rmethod, in_bytes(Method::const_offset()));  //T3 --> Rmethod._constMethod
  __ ld_hu(V0, T3, in_bytes(ConstMethod::size_of_parameters_offset()));

  // Rmethod: Method*
  // V0: size of parameters
  // Rsender: sender 's sp ,could be different frome sp+ wordSize if we call via c2i
  // get size of locals in words to T2
  __ ld_hu(T2, T3, in_bytes(ConstMethod::size_of_locals_offset()));
  // T2 = no. of additional locals, locals include parameters
  __ sub_d(T2, T2, V0);

  // see if we've got enough room on the stack for locals plus overhead.
  // Layout of frame at this point
  //
  // [ argument word n-1  ] <--- sp
  //   ...
  // [ argument word 0    ]
  generate_stack_overflow_check();
  // after this function, the layout of frame does not change

  // compute beginning of parameters (LVP)
  __ slli_d(LVP, V0, LogBytesPerWord);
  __ addi_d(LVP, LVP, (-1) * wordSize);
  __ add_d(LVP, LVP, SP);

  // T2 - # of additional locals
  // allocate space for locals
  // explicitly initialize locals
  {
    Label exit, loop;
    __ beq(T2, R0, exit);

    __ bind(loop);
    __ addi_d(SP, SP, (-1) * wordSize);
    __ addi_d(T2, T2, -1);               // until everything initialized
    __ st_d(R0, SP, 0);                  // initialize local variables
    __ bne(T2, R0, loop);

    __ bind(exit);
  }

  //
  // [ local var m-1      ] <--- sp
  //   ...
  // [ local var 0        ]
  // [ argument word n-1  ] <--- T0?
  //   ...
  // [ argument word 0    ] <--- LVP

  // initialize fixed part of activation frame

  generate_fixed_frame(false);


  // after this function, the layout of frame is as following
  //
  // [ monitor block top        ] <--- sp ( the top monitor entry )
  // [ byte code pointer        ] (if native, bcp = 0)
  // [ constant pool cache      ]
  // [ Method*                  ]
  // [ locals offset            ]
  // [ sender's sp              ]
  // [ sender's fp              ] <--- fp
  // [ return address           ]
  // [ local var m-1            ]
  //   ...
  // [ local var 0              ]
  // [ argumnet word n-1        ] <--- ( sender's sp )
  //   ...
  // [ argument word 0          ] <--- LVP


  // make sure method is not native & not abstract
#ifdef ASSERT
  __ ld_d(AT, Rmethod, in_bytes(Method::access_flags_offset()));
  {
    Label L;
    __ andi(T2, AT, JVM_ACC_NATIVE);
    __ beq(T2, R0, L);
    __ stop("tried to execute native method as non-native");
    __ bind(L);
  }
  {
    Label L;
    __ andi(T2, AT, JVM_ACC_ABSTRACT);
    __ beq(T2, R0, L);
    __ stop("tried to execute abstract method in interpreter");
    __ bind(L);
  }
#endif

  // Since at this point in the method invocation the exception handler
  // would try to exit the monitor of synchronized methods which hasn't
  // been entered yet, we set the thread local variable
  // _do_not_unlock_if_synchronized to true. The remove_activation will
  // check this flag.

#ifndef OPT_THREAD
  Register thread = T8;
  __ get_thread(thread);
#else
  Register thread = TREG;
#endif
  __ li(AT, (int)true);
  __ st_b(AT, thread, in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));

#ifndef CORE

  // mdp : T8
  // tmp1: T4
  // tmp2: T2
   __ profile_parameters_type(T8, T4, T2);

  // increment invocation count & check for overflow
  Label invocation_counter_overflow;
  Label profile_method;
  Label profile_method_continue;
  if (inc_counter) {
    generate_counter_incr(&invocation_counter_overflow,
                          &profile_method,
                          &profile_method_continue);
    if (ProfileInterpreter) {
      __ bind(profile_method_continue);
    }
  }

  Label continue_after_compile;
  __ bind(continue_after_compile);

#endif // CORE

  bang_stack_shadow_pages(false);

  // reset the _do_not_unlock_if_synchronized flag
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ st_b(R0, thread, in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));

  // check for synchronized methods
  // Must happen AFTER invocation_counter check and stack overflow check,
  // so method is not locked if overflows.
  //
  if (synchronized) {
    // Allocate monitor and lock method
    lock_method();
  } else {
    // no synchronization necessary
#ifdef ASSERT
    { Label L;
      __ ld_w(AT, Rmethod, in_bytes(Method::access_flags_offset()));
      __ andi(T2, AT, JVM_ACC_SYNCHRONIZED);
      __ beq(T2, R0, L);
      __ stop("method needs synchronization");
      __ bind(L);
    }
#endif
  }

  // layout of frame after lock_method
  // [ monitor entry            ] <--- sp
  //   ...
  // [ monitor entry            ]
  // [ monitor block top        ] ( the top monitor entry )
  // [ byte code pointer        ] (if native, bcp = 0)
  // [ constant pool cache      ]
  // [ Method*                  ]
  // [ locals offset            ]
  // [ sender's sp              ]
  // [ sender's fp              ]
  // [ return address           ] <--- fp
  // [ local var m-1            ]
  //   ...
  // [ local var 0              ]
  // [ argumnet word n-1        ] <--- ( sender's sp )
  //   ...
  // [ argument word 0          ] <--- LVP


  // start execution
#ifdef ASSERT
  {
    Label L;
    __ ld_d(AT, FP, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    __ beq(AT, SP, L);
    __ stop("broken stack frame setup in interpreter in native");
    __ bind(L);
  }
#endif

  // jvmti/jvmpi support
  __ notify_method_entry();

  __ dispatch_next(vtos);

  // invocation counter overflow
  if (inc_counter) {
    if (ProfileInterpreter) {
      // We have decided to profile this method in the interpreter
      __ bind(profile_method);
      __ call_VM(noreg, CAST_FROM_FN_PTR(address,
                 InterpreterRuntime::profile_method));
      __ set_method_data_pointer_for_bcp();
      __ get_method(Rmethod);
      __ b(profile_method_continue);
    }
    // Handle overflow of counter and compile method
    __ bind(invocation_counter_overflow);
    generate_counter_overflow(&continue_after_compile);
  }

  return entry_point;
}

// Entry points
//
// Here we generate the various kind of entries into the interpreter.
// The two main entry type are generic bytecode methods and native
// call method.  These both come in synchronized and non-synchronized
// versions but the frame layout they create is very similar. The
// other method entry types are really just special purpose entries
// that are really entry and interpretation all in one. These are for
// trivial methods like accessor, empty, or special math methods.
//
// When control flow reaches any of the entry types for the interpreter
// the following holds ->
//
// Arguments:
//
// Rmethod: Method*
// V0: receiver
//
//
// Stack layout immediately at entry
//
// [ parameter n-1            ] <--- sp
//   ...
// [ parameter 0              ]
// [ expression stack         ] (caller's java expression stack)

// Assuming that we don't go to one of the trivial specialized entries
// the stack will look like below when we are ready to execute the
// first bytecode (or call the native routine). The register usage
// will be as the template based interpreter expects (see
// interpreter_loongarch_64.hpp).
//
// local variables follow incoming parameters immediately; i.e.
// the return address is moved to the end of the locals).
//
// [ monitor entry            ] <--- sp
//   ...
// [ monitor entry            ]
// [ monitor block top        ] ( the top monitor entry )
// [ byte code pointer        ] (if native, bcp = 0)
// [ constant pool cache      ]
// [ Method*                  ]
// [ locals offset            ]
// [ sender's sp              ]
// [ sender's fp              ]
// [ return address           ] <--- fp
// [ local var m-1            ]
//   ...
// [ local var 0              ]
// [ argumnet word n-1        ] <--- ( sender's sp )
//   ...
// [ argument word 0          ] <--- S7

address AbstractInterpreterGenerator::generate_method_entry(
                                        AbstractInterpreter::MethodKind kind) {
  // determine code generation flags
  bool synchronized = false;
  address entry_point = NULL;
  switch (kind) {
    case Interpreter::zerolocals             :
      break;
    case Interpreter::zerolocals_synchronized:
      synchronized = true;
      break;
    case Interpreter::native                 :
      entry_point = ((InterpreterGenerator*)this)->generate_native_entry(false);
      break;
    case Interpreter::native_synchronized    :
      entry_point = ((InterpreterGenerator*)this)->generate_native_entry(true);
      break;
    case Interpreter::empty                  :
      entry_point = ((InterpreterGenerator*)this)->generate_empty_entry();
      break;
    case Interpreter::accessor               :
      entry_point = ((InterpreterGenerator*)this)->generate_accessor_entry();
      break;
    case Interpreter::abstract               :
      entry_point = ((InterpreterGenerator*)this)->generate_abstract_entry();
      break;

    case Interpreter::java_lang_math_sin     : // fall thru
    case Interpreter::java_lang_math_cos     : // fall thru
    case Interpreter::java_lang_math_tan     : // fall thru
    case Interpreter::java_lang_math_log     : // fall thru
    case Interpreter::java_lang_math_log10   : // fall thru
    case Interpreter::java_lang_math_pow     : // fall thru
    case Interpreter::java_lang_math_exp     : break;
    case Interpreter::java_lang_math_abs     : // fall thru
    case Interpreter::java_lang_math_sqrt    :
      entry_point = ((InterpreterGenerator*)this)->generate_math_entry(kind);    break;
    case Interpreter::java_lang_ref_reference_get:
      entry_point = ((InterpreterGenerator*)this)->generate_Reference_get_entry(); break;
    case Interpreter::java_util_zip_CRC32_update:
      entry_point = ((InterpreterGenerator*)this)->generate_CRC32_update_entry();  break;
    case Interpreter::java_util_zip_CRC32_updateBytes: // fall thru
    case Interpreter::java_util_zip_CRC32_updateByteBuffer:
      entry_point = ((InterpreterGenerator*)this)->generate_CRC32_updateBytes_entry(kind); break;
    default:
      fatal(err_msg("unexpected method kind: %d", kind));
      break;
  }
  if (entry_point) return entry_point;

  return ((InterpreterGenerator*)this)->generate_normal_entry(synchronized);
}

// These should never be compiled since the interpreter will prefer
// the compiled version to the intrinsic version.
bool AbstractInterpreter::can_be_compiled(methodHandle m) {
  switch (method_kind(m)) {
    case Interpreter::java_lang_math_sin     : // fall thru
    case Interpreter::java_lang_math_cos     : // fall thru
    case Interpreter::java_lang_math_tan     : // fall thru
    case Interpreter::java_lang_math_abs     : // fall thru
    case Interpreter::java_lang_math_log     : // fall thru
    case Interpreter::java_lang_math_log10   : // fall thru
    case Interpreter::java_lang_math_sqrt    : // fall thru
    case Interpreter::java_lang_math_pow     : // fall thru
    case Interpreter::java_lang_math_exp     :
      return false;
    default:
      return true;
  }
}

// How much stack a method activation needs in words.
int AbstractInterpreter::size_top_interpreter_activation(Method* method) {

  const int entry_size    = frame::interpreter_frame_monitor_size();

  // total overhead size: entry_size + (saved fp thru expr stack bottom).
  // be sure to change this if you add/subtract anything to/from the overhead area
  const int overhead_size = -(frame::interpreter_frame_initial_sp_offset) + entry_size;

  const int stub_code = 6;  // see generate_call_stub
  // return overhead_size + method->max_locals() + method->max_stack() + stub_code;
  const int method_stack = (method->max_locals() + method->max_stack()) *
          Interpreter::stackElementWords;
  return overhead_size + method_stack + stub_code;
}

void AbstractInterpreter::layout_activation(Method* method,
                                           int tempcount,
                                           int popframe_extra_args,
                                           int moncount,
                                           int caller_actual_parameters,
                                           int callee_param_count,
                                           int callee_locals,
                                           frame* caller,
                                           frame* interpreter_frame,
                                           bool is_top_frame,
                                           bool is_bottom_frame) {
  // Note: This calculation must exactly parallel the frame setup
  // in AbstractInterpreterGenerator::generate_method_entry.
  // If interpreter_frame!=NULL, set up the method, locals, and monitors.
  // The frame interpreter_frame, if not NULL, is guaranteed to be the
  // right size, as determined by a previous call to this method.
  // It is also guaranteed to be walkable even though it is in a skeletal state

  // fixed size of an interpreter frame:

  int max_locals = method->max_locals() * Interpreter::stackElementWords;
  int extra_locals = (method->max_locals() - method->size_of_parameters()) * Interpreter::stackElementWords;

#ifdef ASSERT
  if (!EnableInvokeDynamic) {
    // @@@ FIXME: Should we correct interpreter_frame_sender_sp in the calling sequences?
    // Probably, since deoptimization doesn't work yet.
    assert(caller->unextended_sp() == interpreter_frame->interpreter_frame_sender_sp(), "Frame not properly walkable");
  }
  assert(caller->sp() == interpreter_frame->sender_sp(), "Frame not properly walkable(2)");
#endif

  interpreter_frame->interpreter_frame_set_method(method);
  // NOTE the difference in using sender_sp and interpreter_frame_sender_sp
  // interpreter_frame_sender_sp is the original sp of the caller (the unextended_sp)
  // and sender_sp is fp+8
  intptr_t* locals = interpreter_frame->sender_sp() + max_locals - 1;

#ifdef ASSERT
  if (caller->is_interpreted_frame()) {
    assert(locals < caller->fp() + frame::interpreter_frame_initial_sp_offset, "bad placement");
  }
#endif

  interpreter_frame->interpreter_frame_set_locals(locals);
  BasicObjectLock* montop = interpreter_frame->interpreter_frame_monitor_begin();
  BasicObjectLock* monbot = montop - moncount;
  interpreter_frame->interpreter_frame_set_monitor_end(montop - moncount);

  //set last sp;
  intptr_t*  sp = (intptr_t*) monbot - tempcount*Interpreter::stackElementWords -
                      popframe_extra_args;
  interpreter_frame->interpreter_frame_set_last_sp(sp);
  // All frames but the initial interpreter frame we fill in have a
  // value for sender_sp that allows walking the stack but isn't
  // truly correct. Correct the value here.
  //
  if (extra_locals != 0 &&
      interpreter_frame->sender_sp() == interpreter_frame->interpreter_frame_sender_sp() ) {
    interpreter_frame->set_interpreter_frame_sender_sp(caller->sp() + extra_locals);
  }
  *interpreter_frame->interpreter_frame_cache_addr() = method->constants()->cache();
}

//-----------------------------------------------------------------------------
// Exceptions

void TemplateInterpreterGenerator::generate_throw_exception() {
  // Entry point in previous activation (i.e., if the caller was
  // interpreted)
  Interpreter::_rethrow_exception_entry = __ pc();
  // Restore sp to interpreter_frame_last_sp even though we are going
  // to empty the expression stack for the exception processing.
  __ st_d(R0,FP, frame::interpreter_frame_last_sp_offset * wordSize);

  // V0: exception
  // V1: return address/pc that threw exception
  __ restore_bcp();                              // BCP points to call/send
  __ restore_locals();

  //add for compressedoops
  __ reinit_heapbase();
  // Entry point for exceptions thrown within interpreter code
  Interpreter::_throw_exception_entry = __ pc();
  // expression stack is undefined here
  // V0: exception
  // BCP: exception bcp
  __ verify_oop(V0);

  // expression stack must be empty before entering the VM in case of an exception
  __ empty_expression_stack();
  // find exception handler address and preserve exception oop
  __ move(A1, V0);
  __ call_VM(V1, CAST_FROM_FN_PTR(address, InterpreterRuntime::exception_handler_for_exception), A1);
  // V0: exception handler entry point
  // V1: preserved exception oop
  // S0: bcp for exception handler
  __ push(V1);                                 // push exception which is now the only value on the stack
  __ jr(V0);                                   // jump to exception handler (may be _remove_activation_entry!)

  // If the exception is not handled in the current frame the frame is removed and
  // the exception is rethrown (i.e. exception continuation is _rethrow_exception).
  //
  // Note: At this point the bci is still the bxi for the instruction which caused
  //       the exception and the expression stack is empty. Thus, for any VM calls
  //       at this point, GC will find a legal oop map (with empty expression stack).

  // In current activation
  // V0: exception
  // BCP: exception bcp

  //
  // JVMTI PopFrame support
  //

  Interpreter::_remove_activation_preserving_args_entry = __ pc();
  __ empty_expression_stack();
  // Set the popframe_processing bit in pending_popframe_condition indicating that we are
  // currently handling popframe, so that call_VMs that may happen later do not trigger new
  // popframe handling cycles.
#ifndef OPT_THREAD
  Register thread = T2;
  __ get_thread(T2);
#else
  Register thread = TREG;
#endif
  __ ld_w(T3, thread, in_bytes(JavaThread::popframe_condition_offset()));
  __ ori(T3, T3, JavaThread::popframe_processing_bit);
  __ st_w(T3, thread, in_bytes(JavaThread::popframe_condition_offset()));

#ifndef CORE
  {
    // Check to see whether we are returning to a deoptimized frame.
    // (The PopFrame call ensures that the caller of the popped frame is
    // either interpreted or compiled and deoptimizes it if compiled.)
    // In this case, we can't call dispatch_next() after the frame is
    // popped, but instead must save the incoming arguments and restore
    // them after deoptimization has occurred.
    //
    // Note that we don't compare the return PC against the
    // deoptimization blob's unpack entry because of the presence of
    // adapter frames in C2.
    Label caller_not_deoptimized;
    __ ld_d(A0, FP, frame::return_addr_offset * wordSize);
    __ super_call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::interpreter_contains), A0);
    __ bne(V0, R0, caller_not_deoptimized);

    // Compute size of arguments for saving when returning to deoptimized caller
    __ get_method(A1);
    __ verify_oop(A1);
    __ ld_d(A1, A1, in_bytes(Method::const_offset()));
    __ ld_hu(A1, A1, in_bytes(ConstMethod::size_of_parameters_offset()));
    __ shl(A1, Interpreter::logStackElementSize);
    __ restore_locals();
    __ sub_d(A2, LVP, A1);
    __ addi_d(A2, A2, wordSize);
    // Save these arguments
#ifndef OPT_THREAD
    __ get_thread(A0);
#else
    __ move(A0, TREG);
#endif
    __ super_call_VM_leaf(CAST_FROM_FN_PTR(address, Deoptimization::popframe_preserve_args), A0, A1, A2);

    __ remove_activation(vtos, T4, false, false, false);

    // Inform deoptimization that it is responsible for restoring these arguments
#ifndef OPT_THREAD
    __ get_thread(thread);
#endif
    __ li(AT, JavaThread::popframe_force_deopt_reexecution_bit);
    __ st_w(AT, thread, in_bytes(JavaThread::popframe_condition_offset()));
    // Continue in deoptimization handler
    __ jr(T4);

    __ bind(caller_not_deoptimized);
  }
#endif /* !CORE */

  __ remove_activation(vtos, T3,
                       /* throw_monitor_exception */ false,
                       /* install_monitor_exception */ false,
                       /* notify_jvmdi */ false);

  // Clear the popframe condition flag
  // Finish with popframe handling
  // A previous I2C followed by a deoptimization might have moved the
  // outgoing arguments further up the stack. PopFrame expects the
  // mutations to those outgoing arguments to be preserved and other
  // constraints basically require this frame to look exactly as
  // though it had previously invoked an interpreted activation with
  // no space between the top of the expression stack (current
  // last_sp) and the top of stack. Rather than force deopt to
  // maintain this kind of invariant all the time we call a small
  // fixup routine to move the mutated arguments onto the top of our
  // expression stack if necessary.
  __ move(T8, SP);
  __ ld_d(A2, FP, frame::interpreter_frame_last_sp_offset * wordSize);
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  // PC must point into interpreter here
  Label L;
  __ bind(L);
  __ set_last_Java_frame(thread, noreg, FP, L);
  __ super_call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::popframe_move_outgoing_args), thread, T8, A2);
  __ reset_last_Java_frame(thread, true);
  // Restore the last_sp and null it out
  __ ld_d(SP, FP, frame::interpreter_frame_last_sp_offset * wordSize);
  __ st_d(R0, FP, frame::interpreter_frame_last_sp_offset * wordSize);



  __ li(AT, JavaThread::popframe_inactive);
  __ st_w(AT, thread, in_bytes(JavaThread::popframe_condition_offset()));

  // Finish with popframe handling
  __ restore_bcp();
  __ restore_locals();
#ifndef CORE
  // The method data pointer was incremented already during
  // call profiling. We have to restore the mdp for the current bcp.
  if (ProfileInterpreter) {
    __ set_method_data_pointer_for_bcp();
  }
#endif // !CORE
  // Clear the popframe condition flag
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ li(AT, JavaThread::popframe_inactive);
  __ st_w(AT, thread, in_bytes(JavaThread::popframe_condition_offset()));

#if INCLUDE_JVMTI
  {
    Label L_done;

    __ ld_bu(AT, BCP, 0);
    __ addi_d(AT, AT, -1 * Bytecodes::_invokestatic);
    __ bne(AT, R0, L_done);

    // The member name argument must be restored if _invokestatic is re-executed after a PopFrame call.
    // Detect such a case in the InterpreterRuntime function and return the member name argument, or NULL.

    __ get_method(T4);
    __ ld_d(T8, LVP, 0);
    __ call_VM(T8, CAST_FROM_FN_PTR(address, InterpreterRuntime::member_name_arg_or_null), T8, T4, BCP);

    __ beq(T8, R0, L_done);

    __ st_d(T8, SP, 0);
    __ bind(L_done);
  }
#endif // INCLUDE_JVMTI

  __ dispatch_next(vtos);
  // end of PopFrame support

  Interpreter::_remove_activation_entry = __ pc();

  // preserve exception over this code sequence
  __ pop(T0);
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ st_d(T0, thread, in_bytes(JavaThread::vm_result_offset()));
  // remove the activation (without doing throws on illegalMonitorExceptions)
  __ remove_activation(vtos, T3, false, true, false);
  // restore exception
  __ get_vm_result(T0, thread);
  __ verify_oop(T0);

  // In between activations - previous activation type unknown yet
  // compute continuation point - the continuation point expects
  // the following registers set up:
  //
  // T0: exception
  // T1: return address/pc that threw exception
  // SP: expression stack of caller
  // FP: fp of caller
  __ push2(T0, T3);             // save exception and return address
  __ move(A1, T3);
  __ super_call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::exception_handler_for_return_address), thread, A1);
  __ move(T4, V0);                             // save exception handler
  __ pop2(V0, V1);                   // restore return address and exception

  // Note that an "issuing PC" is actually the next PC after the call
  __ jr(T4);                                   // jump to exception handler of caller
}


//
// JVMTI ForceEarlyReturn support
//
address TemplateInterpreterGenerator::generate_earlyret_entry_for(TosState state) {
  address entry = __ pc();
  __ restore_bcp();
  __ restore_locals();
  __ empty_expression_stack();
  __ empty_FPU_stack();
  __ load_earlyret_value(state);

#ifndef OPT_THREAD
  __ get_thread(TREG);
#endif
  __ ld_ptr(T4, TREG, in_bytes(JavaThread::jvmti_thread_state_offset()));
  const Address cond_addr(T4, in_bytes(JvmtiThreadState::earlyret_state_offset()));
  // Clear the earlyret state
  __ li(AT, JvmtiThreadState::earlyret_inactive);
  __ st_w(AT, cond_addr);
  __ membar(__ AnyAny);//no membar here for aarch64


  __ remove_activation(state, T0,
                         false, /* throw_monitor_exception */
                         false, /* install_monitor_exception */
                         true); /* notify_jvmdi */
  __ membar(__ AnyAny);
  __ jr(T0);

  return entry;
} // end of ForceEarlyReturn support


//-----------------------------------------------------------------------------
// Helper for vtos entry point generation

void TemplateInterpreterGenerator::set_vtos_entry_points(Template* t,
                                                         address& bep,
                                                         address& cep,
                                                         address& sep,
                                                         address& aep,
                                                         address& iep,
                                                         address& lep,
                                                         address& fep,
                                                         address& dep,
                                                         address& vep) {
  assert(t->is_valid() && t->tos_in() == vtos, "illegal template");
  Label L;
  fep = __ pc(); __ push(ftos); __ b(L);
  dep = __ pc(); __ push(dtos); __ b(L);
  lep = __ pc(); __ push(ltos); __ b(L);
  aep  =__ pc(); __ push(atos); __ b(L);
  bep = cep = sep =
  iep = __ pc(); __ push(itos);
  vep = __ pc();
  __ bind(L);
  generate_and_dispatch(t);
}


//-----------------------------------------------------------------------------
// Generation of individual instructions

// helpers for generate_and_dispatch


InterpreterGenerator::InterpreterGenerator(StubQueue* code)
  : TemplateInterpreterGenerator(code) {
   generate_all(); // down here so it can be "virtual"
}

//-----------------------------------------------------------------------------

// Non-product code
#ifndef PRODUCT
address TemplateInterpreterGenerator::generate_trace_code(TosState state) {
  address entry = __ pc();

  // prepare expression stack
  __ push(state);       // save tosca

  // tos & tos2
  // trace_bytecode need actually 4 args, the last two is tos&tos2
  // this work fine for x86. but LA ABI calling convention will store A2-A3
  // to the stack position it think is the tos&tos2
  // when the expression stack have no more than 2 data, error occur.
  __ ld_d(A2, SP, 0);
  __ ld_d(A3, SP, 1 * wordSize);

  // pass arguments & call tracer
  __ call_VM(noreg, CAST_FROM_FN_PTR(address, SharedRuntime::trace_bytecode), RA, A2, A3);
  __ move(RA, V0);    // make sure return address is not destroyed by pop(state)

  // restore expression stack
  __ pop(state);        // restore tosca

  // return
  __ jr(RA);
  return entry;
}

void TemplateInterpreterGenerator::count_bytecode() {
  __ li(T8, (long)&BytecodeCounter::_counter_value);
  __ ld_w(AT, T8, 0);
  __ addi_d(AT, AT, 1);
  __ st_w(AT, T8, 0);
}

void TemplateInterpreterGenerator::histogram_bytecode(Template* t) {
  __ li(T8, (long)&BytecodeHistogram::_counters[t->bytecode()]);
  __ ld_w(AT, T8, 0);
  __ addi_d(AT, AT, 1);
  __ st_w(AT, T8, 0);
}

void TemplateInterpreterGenerator::histogram_bytecode_pair(Template* t) {
  __ li(T8, (long)&BytecodePairHistogram::_index);
  __ ld_w(T4, T8, 0);
  __ srli_d(T4, T4, BytecodePairHistogram::log2_number_of_codes);
  __ li(T8, ((long)t->bytecode()) << BytecodePairHistogram::log2_number_of_codes);
  __ orr(T4, T4, T8);
  __ li(T8, (long)&BytecodePairHistogram::_index);
  __ st_w(T4, T8, 0);
  __ slli_d(T4, T4, 2);
  __ li(T8, (long)BytecodePairHistogram::_counters);
  __ add_d(T8, T8, T4);
  __ ld_w(AT, T8, 0);
  __ addi_d(AT, AT, 1);
  __ st_w(AT, T8, 0);
}


void TemplateInterpreterGenerator::trace_bytecode(Template* t) {
  // Call a little run-time stub to avoid blow-up for each bytecode.
  // The run-time runtime saves the right registers, depending on
  // the tosca in-state for the given template.
  address entry = Interpreter::trace_code(t->tos_in());
  assert(entry != NULL, "entry must have been generated");
  __ call(entry, relocInfo::none);
  //add for compressedoops
  __ reinit_heapbase();
}


void TemplateInterpreterGenerator::stop_interpreter_at() {
  Label L;
  __ li(T8, long(&BytecodeCounter::_counter_value));
  __ ld_w(T8, T8, 0);
  __ li(AT, StopInterpreterAt);
  __ bne(T8, AT, L);
  __ brk(5);
  __ bind(L);
}
#endif // !PRODUCT
#endif // ! CC_INTERP
