/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "interp_masm_loongarch_64.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "oops/arrayOop.hpp"
#include "oops/markOop.hpp"
#include "oops/methodData.hpp"
#include "oops/method.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiRedefineClassesTrace.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "runtime/basicLock.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/thread.inline.hpp"

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

// Implementation of InterpreterMacroAssembler

#ifdef CC_INTERP
void InterpreterMacroAssembler::get_method(Register reg) {
}
#endif // CC_INTERP

void InterpreterMacroAssembler::get_2_byte_integer_at_bcp(Register reg, Register tmp, int offset) {
  if (UseUnalignedAccesses) {
    ld_hu(reg, BCP, offset);
  } else {
    ld_bu(reg, BCP, offset);
    ld_bu(tmp, BCP, offset + 1);
    bstrins_d(reg, tmp, 15, 8);
  }
}

void InterpreterMacroAssembler::get_4_byte_integer_at_bcp(Register reg, int offset) {
  if (UseUnalignedAccesses) {
    ld_wu(reg, BCP, offset);
  } else {
    ldr_w(reg, BCP, offset);
    ldl_w(reg, BCP, offset + 3);
    lu32i_d(reg, 0);
  }
}

#ifndef CC_INTERP

void InterpreterMacroAssembler::call_VM_leaf_base(address entry_point,
                                                  int number_of_arguments) {
  // interpreter specific
  //
  // Note: No need to save/restore bcp & locals pointer
  //       since these are callee saved registers and no blocking/
  //       GC can happen in leaf calls.
  // Further Note: DO NOT save/restore bcp/locals. If a caller has
  // already saved them so that it can use BCP/LVP as temporaries
  // then a save/restore here will DESTROY the copy the caller
  // saved! There used to be a save_bcp() that only happened in
  // the ASSERT path (no restore_bcp). Which caused bizarre failures
  // when jvm built with ASSERTs.
#ifdef ASSERT
  save_bcp();
  {
    Label L;
    ld_d(AT,FP,frame::interpreter_frame_last_sp_offset * wordSize);
    beq(AT,R0,L);
    stop("InterpreterMacroAssembler::call_VM_leaf_base: last_sp != NULL");
    bind(L);
  }
#endif
  // super call
  MacroAssembler::call_VM_leaf_base(entry_point, number_of_arguments);
  // interpreter specific
  // Used to ASSERT that BCP/LVP were equal to frame's bcp/locals
  // but since they may not have been saved (and we don't want to
  // save them here (see note above) the assert is invalid.
}

void InterpreterMacroAssembler::call_VM_base(Register oop_result,
                                             Register java_thread,
                                             Register last_java_sp,
                                             address  entry_point,
                                             int      number_of_arguments,
                                             bool     check_exceptions) {
  // interpreter specific
  //
  // Note: Could avoid restoring locals ptr (callee saved) - however doesn't
  //       really make a difference for these runtime calls, since they are
  //       slow anyway. Btw., bcp must be saved/restored since it may change
  //       due to GC.
  assert(java_thread == noreg , "not expecting a precomputed java thread");
  save_bcp();
#ifdef ASSERT
  {
    Label L;
    ld_d(AT, FP, frame::interpreter_frame_last_sp_offset * wordSize);
    beq(AT, R0, L);
    stop("InterpreterMacroAssembler::call_VM_base: last_sp != NULL");
    bind(L);
  }
#endif /* ASSERT */
  // super call
  MacroAssembler::call_VM_base(oop_result, java_thread, last_java_sp,
                               entry_point, number_of_arguments,
                               check_exceptions);
  // interpreter specific
  restore_bcp();
  restore_locals();
}


void InterpreterMacroAssembler::check_and_handle_popframe(Register java_thread) {
  if (JvmtiExport::can_pop_frame()) {
    Label L;
    // Initiate popframe handling only if it is not already being
    // processed.  If the flag has the popframe_processing bit set, it
    // means that this code is called *during* popframe handling - we
    // don't want to reenter.
    // This method is only called just after the call into the vm in
    // call_VM_base, so the arg registers are available.
    // Not clear if any other register is available, so load AT twice
    assert(AT != java_thread, "check");
    ld_w(AT, java_thread, in_bytes(JavaThread::popframe_condition_offset()));
    andi(AT, AT, JavaThread::popframe_pending_bit);
    beq(AT, R0, L);

    ld_w(AT, java_thread, in_bytes(JavaThread::popframe_condition_offset()));
    andi(AT, AT, JavaThread::popframe_processing_bit);
    bne(AT, R0, L);
    call_VM_leaf(CAST_FROM_FN_PTR(address, Interpreter::remove_activation_preserving_args_entry));
    jr(V0);
    bind(L);
  }
}


void InterpreterMacroAssembler::load_earlyret_value(TosState state) {
  Register thread = T8;
#ifndef OPT_THREAD
  get_thread(thread);
#else
  move(T8, TREG);
#endif
  ld_ptr(thread, thread, in_bytes(JavaThread::jvmti_thread_state_offset()));
  const Address tos_addr (thread, in_bytes(JvmtiThreadState::earlyret_tos_offset()));
  const Address oop_addr (thread, in_bytes(JvmtiThreadState::earlyret_oop_offset()));
  const Address val_addr (thread, in_bytes(JvmtiThreadState::earlyret_value_offset()));
  //V0, oop_addr,V1,val_addr
  switch (state) {
    case atos:
      ld_ptr(V0, oop_addr);
      st_ptr(R0, oop_addr);
      verify_oop(V0, state);
      break;
    case ltos:
      ld_ptr(V0, val_addr);               // fall through
      break;
    case btos:                                     // fall through
    case ztos:                                     // fall through
    case ctos:                                     // fall through
    case stos:                                     // fall through
    case itos:
      ld_w(V0, val_addr);
      break;
    case ftos:
      fld_s(F0, thread, in_bytes(JvmtiThreadState::earlyret_value_offset()));
      break;
    case dtos:
      fld_d(F0, thread, in_bytes(JvmtiThreadState::earlyret_value_offset()));
      break;
    case vtos: /* nothing to do */                    break;
    default  : ShouldNotReachHere();
  }
  // Clean up tos value in the thread object
  li(AT, (int)ilgl);
  st_w(AT, tos_addr);
  st_w(R0, thread, in_bytes(JvmtiThreadState::earlyret_value_offset()));
}


void InterpreterMacroAssembler::check_and_handle_earlyret(Register java_thread) {
  if (JvmtiExport::can_force_early_return()) {
    Label L;
    Register tmp = T4;

    assert(java_thread != AT, "check");
    assert(java_thread != tmp, "check");
    ld_ptr(AT, java_thread, in_bytes(JavaThread::jvmti_thread_state_offset()));
    beq(AT, R0, L);

    // Initiate earlyret handling only if it is not already being processed.
    // If the flag has the earlyret_processing bit set, it means that this code
    // is called *during* earlyret handling - we don't want to reenter.
    ld_w(AT, AT, in_bytes(JvmtiThreadState::earlyret_state_offset()));
    li(tmp, JvmtiThreadState::earlyret_pending);
    bne(tmp, AT, L);

    // Call Interpreter::remove_activation_early_entry() to get the address of the
    // same-named entrypoint in the generated interpreter code.
    ld_ptr(tmp, java_thread, in_bytes(JavaThread::jvmti_thread_state_offset()));
    ld_w(AT, tmp, in_bytes(JvmtiThreadState::earlyret_tos_offset()));
    move(A0, AT);
    call_VM_leaf(CAST_FROM_FN_PTR(address, Interpreter::remove_activation_early_entry), A0);
    jr(V0);
    bind(L);
  }
}


void InterpreterMacroAssembler::get_unsigned_2_byte_index_at_bcp(Register reg,
                                                                 int bcp_offset) {
  assert(bcp_offset >= 0, "bcp is still pointing to start of bytecode");
  ld_bu(AT, BCP, bcp_offset);
  ld_bu(reg, BCP, bcp_offset + 1);
  bstrins_w(reg, AT, 15, 8);
}


void InterpreterMacroAssembler::get_cache_index_at_bcp(Register index,
                                                       int bcp_offset,
                                                       size_t index_size) {
  assert(bcp_offset > 0, "bcp is still pointing to start of bytecode");
  if (index_size == sizeof(u2)) {
    get_2_byte_integer_at_bcp(index, AT, bcp_offset);
  } else if (index_size == sizeof(u4)) {
    assert(EnableInvokeDynamic, "giant index used only for JSR 292");
    get_4_byte_integer_at_bcp(index, bcp_offset);
    // Check if the secondary index definition is still ~x, otherwise
    // we have to change the following assembler code to calculate the
    // plain index.
    assert(ConstantPool::decode_invokedynamic_index(~123) == 123, "else change next line");
    nor(index, index, R0);
    slli_w(index, index, 0);
  } else if (index_size == sizeof(u1)) {
    ld_bu(index, BCP, bcp_offset);
  } else {
    ShouldNotReachHere();
  }
}


void InterpreterMacroAssembler::get_cache_and_index_at_bcp(Register cache,
                                                           Register index,
                                                           int bcp_offset,
                                                           size_t index_size) {
  assert_different_registers(cache, index);
  get_cache_index_at_bcp(index, bcp_offset, index_size);
  ld_d(cache, FP, frame::interpreter_frame_cache_offset * wordSize);
  assert(sizeof(ConstantPoolCacheEntry) == 4 * wordSize, "adjust code below");
  assert(exact_log2(in_words(ConstantPoolCacheEntry::size())) == 2, "else change next line");
  shl(index, 2);
}


void InterpreterMacroAssembler::get_cache_and_index_and_bytecode_at_bcp(Register cache,
                                                                        Register index,
                                                                        Register bytecode,
                                                                        int byte_no,
                                                                        int bcp_offset,
                                                                        size_t index_size) {
  get_cache_and_index_at_bcp(cache, index, bcp_offset, index_size);
  // We use a 32-bit load here since the layout of 64-bit words on
  // little-endian machines allow us that.
  alsl_d(AT, index, cache, Address::times_ptr - 1);
  ld_w(bytecode, AT, in_bytes(ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::indices_offset()));
  if(os::is_MP()) {
    membar(Assembler::Membar_mask_bits(LoadLoad|LoadStore));
  }

  const int shift_count = (1 + byte_no) * BitsPerByte;
  assert((byte_no == TemplateTable::f1_byte && shift_count == ConstantPoolCacheEntry::bytecode_1_shift) ||
         (byte_no == TemplateTable::f2_byte && shift_count == ConstantPoolCacheEntry::bytecode_2_shift),
         "correct shift count");
  srli_d(bytecode, bytecode, shift_count);
  assert(ConstantPoolCacheEntry::bytecode_1_mask == ConstantPoolCacheEntry::bytecode_2_mask, "common mask");
  li(AT, ConstantPoolCacheEntry::bytecode_1_mask);
  andr(bytecode, bytecode, AT);
}

void InterpreterMacroAssembler::get_cache_entry_pointer_at_bcp(Register cache,
                                                               Register tmp,
                                                               int bcp_offset,
                                                               size_t index_size) {
  assert(bcp_offset > 0, "bcp is still pointing to start of bytecode");
  assert(cache != tmp, "must use different register");
  get_cache_index_at_bcp(tmp, bcp_offset, index_size);
  assert(sizeof(ConstantPoolCacheEntry) == 4 * wordSize, "adjust code below");
  // convert from field index to ConstantPoolCacheEntry index
  // and from word offset to byte offset
  assert(exact_log2(in_bytes(ConstantPoolCacheEntry::size_in_bytes())) == 2 + LogBytesPerWord, "else change next line");
  shl(tmp, 2 + LogBytesPerWord);
  ld_d(cache, FP, frame::interpreter_frame_cache_offset * wordSize);
  // skip past the header
  addi_d(cache, cache, in_bytes(ConstantPoolCache::base_offset()));
  add_d(cache, cache, tmp);
}

void InterpreterMacroAssembler::get_method_counters(Register method,
                                                    Register mcs, Label& skip) {
  Label has_counters;
  ld_d(mcs, method, in_bytes(Method::method_counters_offset()));
  bne(mcs, R0, has_counters);
  call_VM(noreg, CAST_FROM_FN_PTR(address,
          InterpreterRuntime::build_method_counters), method);
  ld_d(mcs, method, in_bytes(Method::method_counters_offset()));
  beq(mcs, R0, skip);   // No MethodCounters allocated, OutOfMemory
  bind(has_counters);
}

// Load object from cpool->resolved_references(index)
void InterpreterMacroAssembler::load_resolved_reference_at_index(
                                           Register result, Register index) {
  assert_different_registers(result, index);
  // convert from field index to resolved_references() index and from
  // word index to byte offset. Since this is a java object, it can be compressed
  Register tmp = index;  // reuse
  shl(tmp, LogBytesPerHeapOop);

  get_constant_pool(result);
  // load pointer for resolved_references[] objArray
  ld_d(result, result, ConstantPool::resolved_references_offset_in_bytes());
  // JNIHandles::resolve(obj);
  ld_d(result, result, 0); //? is needed?
  // Add in the index
  add_d(result, result, tmp);
  load_heap_oop(result, Address(result, arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}

// Resets LVP to locals.  Register sub_klass cannot be any of the above.
void InterpreterMacroAssembler::gen_subtype_check( Register Rsup_klass, Register Rsub_klass, Label &ok_is_subtype ) {

  assert( Rsub_klass != Rsup_klass, "Rsup_klass holds superklass" );
  assert( Rsub_klass != T1, "T1 holds 2ndary super array length" );
  assert( Rsub_klass != T0, "T0 holds 2ndary super array scan ptr" );
  // Profile the not-null value's klass.
  // Here T4 and T1 are used as temporary registers.
  profile_typecheck(T4, Rsub_klass, T1); // blows T4, reloads T1

  // Do the check.
  check_klass_subtype(Rsub_klass, Rsup_klass, T1, ok_is_subtype); // blows T1

  // Profile the failure of the check.
  profile_typecheck_failed(T4); // blows T4

}



// Java Expression Stack

void InterpreterMacroAssembler::pop_ptr(Register r) {
  ld_d(r, SP, 0);
  addi_d(SP, SP, Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::pop_i(Register r) {
  ld_w(r, SP, 0);
  addi_d(SP, SP, Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::pop_l(Register r) {
  ld_d(r, SP, 0);
  addi_d(SP, SP, 2 * Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::pop_f(FloatRegister r) {
  fld_s(r, SP, 0);
  addi_d(SP, SP, Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::pop_d(FloatRegister r) {
  fld_d(r, SP, 0);
  addi_d(SP, SP, 2 * Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::push_ptr(Register r) {
  addi_d(SP, SP, - Interpreter::stackElementSize);
  st_d(r, SP, 0);
}

void InterpreterMacroAssembler::push_i(Register r) {
  // For compatibility reason, don't change to sw.
  addi_d(SP, SP, - Interpreter::stackElementSize);
  st_d(r, SP, 0);
}

void InterpreterMacroAssembler::push_l(Register r) {
  addi_d(SP, SP, -2 * Interpreter::stackElementSize);
  st_d(r, SP, 0);
  st_d(R0, SP, Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::push_f(FloatRegister r) {
  addi_d(SP, SP, - Interpreter::stackElementSize);
  fst_s(r, SP, 0);
}

void InterpreterMacroAssembler::push_d(FloatRegister r) {
  addi_d(SP, SP, -2 * Interpreter::stackElementSize);
  fst_d(r, SP, 0);
  st_d(R0, SP, Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::pop(TosState state) {
  switch (state) {
    case atos: pop_ptr();           break;
    case btos:
    case ztos:
    case ctos:
    case stos:
    case itos: pop_i();             break;
    case ltos: pop_l();             break;
    case ftos: pop_f();             break;
    case dtos: pop_d();             break;
    case vtos: /* nothing to do */  break;
    default:   ShouldNotReachHere();
  }
  verify_oop(FSR, state);
}

//FSR=V0,SSR=V1
void InterpreterMacroAssembler::push(TosState state) {
  verify_oop(FSR, state);
  switch (state) {
    case atos: push_ptr();          break;
    case btos:
    case ztos:
    case ctos:
    case stos:
    case itos: push_i();            break;
    case ltos: push_l();            break;
    case ftos: push_f();            break;
    case dtos: push_d();            break;
    case vtos: /* nothing to do */  break;
    default  : ShouldNotReachHere();
  }
}

void InterpreterMacroAssembler::load_ptr(int n, Register val) {
  ld_d(val, SP, Interpreter::expr_offset_in_bytes(n));
}

void InterpreterMacroAssembler::store_ptr(int n, Register val) {
  st_d(val, SP, Interpreter::expr_offset_in_bytes(n));
}

// Jump to from_interpreted entry of a call unless single stepping is possible
// in this thread in which case we must call the i2i entry
void InterpreterMacroAssembler::jump_from_interpreted(Register method, Register temp) {
  // record last_sp
  move(Rsender, SP);
  st_d(SP, FP, frame::interpreter_frame_last_sp_offset * wordSize);

  if (JvmtiExport::can_post_interpreter_events()) {
    Label run_compiled_code;
    // JVMTI events, such as single-stepping, are implemented partly by avoiding running
    // compiled code in threads for which the event is enabled.  Check here for
    // interp_only_mode if these events CAN be enabled.
#ifndef OPT_THREAD
    get_thread(temp);
#else
    move(temp, TREG);
#endif
    // interp_only is an int, on little endian it is sufficient to test the byte only
    // Is a cmpl faster?
    ld_w(AT, temp, in_bytes(JavaThread::interp_only_mode_offset()));
    beq(AT, R0, run_compiled_code);
    ld_d(AT, method, in_bytes(Method::interpreter_entry_offset()));
    jr(AT);
    bind(run_compiled_code);
  }

  ld_d(AT, method, in_bytes(Method::from_interpreted_offset()));
  jr(AT);
}


// The following two routines provide a hook so that an implementation
// can schedule the dispatch in two parts. LoongArch64 does not do this.
void InterpreterMacroAssembler::dispatch_prolog(TosState state, int step) {
  // Nothing LoongArch64 specific to be done here
}

void InterpreterMacroAssembler::dispatch_epilog(TosState state, int step) {
  dispatch_next(state, step);
}

// assume the next bytecode in T8.
void InterpreterMacroAssembler::dispatch_base(TosState state,
                                              address* table,
                                              bool verifyoop) {
  if (VerifyActivationFrameSize) {
    Label L;

    sub_d(T2, FP, SP);
    int min_frame_size = (frame::link_offset -
      frame::interpreter_frame_initial_sp_offset) * wordSize;
    addi_d(T2, T2, -min_frame_size);
    bge(T2, R0, L);
    stop("broken stack frame");
    bind(L);
  }
  // FIXME: I do not know which register should pass to verify_oop
  if (verifyoop) verify_oop(FSR, state);

  if((long)table >= (long)Interpreter::dispatch_table(btos) &&
     (long)table <= (long)Interpreter::dispatch_table(vtos)) {
    int table_size = (long)Interpreter::dispatch_table(itos) -
                     (long)Interpreter::dispatch_table(stos);
    int table_offset = ((int)state - (int)itos) * table_size;

    // S8 points to the starting address of Interpreter::dispatch_table(itos).
    // See StubGenerator::generate_call_stub(address& return_address) for the initialization of S8.
    if (table_offset != 0) {
      if (is_simm(table_offset, 12)) {
        alsl_d(T3, Rnext, S8, LogBytesPerWord - 1);
        ld_d(T3, T3, table_offset);
      } else {
        li(T2, table_offset);
        alsl_d(T3, Rnext, S8, LogBytesPerWord - 1);
        ldx_d(T3, T2, T3);
      }
    } else {
      slli_d(T2, Rnext, LogBytesPerWord);
      ldx_d(T3, S8, T2);
    }
  } else {
    li(T3, (long)table);
    slli_d(T2, Rnext, LogBytesPerWord);
    ldx_d(T3, T2, T3);
  }
  jr(T3);
}

void InterpreterMacroAssembler::dispatch_only(TosState state) {
  dispatch_base(state, Interpreter::dispatch_table(state));
}

void InterpreterMacroAssembler::dispatch_only_normal(TosState state) {
  dispatch_base(state, Interpreter::normal_table(state));
}

void InterpreterMacroAssembler::dispatch_only_noverify(TosState state) {
  dispatch_base(state, Interpreter::normal_table(state), false);
}


void InterpreterMacroAssembler::dispatch_next(TosState state, int step) {
  // load next bytecode
  ld_bu(Rnext, BCP, step);
  increment(BCP, step);
  dispatch_base(state, Interpreter::dispatch_table(state));
}

void InterpreterMacroAssembler::dispatch_via(TosState state, address* table) {
  // load current bytecode
  ld_bu(Rnext, BCP, 0);
  dispatch_base(state, table);
}

// remove activation
//
// Unlock the receiver if this is a synchronized method.
// Unlock any Java monitors from syncronized blocks.
// Remove the activation from the stack.
//
// If there are locked Java monitors
//    If throw_monitor_exception
//       throws IllegalMonitorStateException
//    Else if install_monitor_exception
//       installs IllegalMonitorStateException
//    Else
//       no error processing
// used registers : T1, T2, T3, T8
// T1 : thread, method access flags
// T2 : monitor entry pointer
// T3 : method, monitor top
// T8 : unlock flag
void InterpreterMacroAssembler::remove_activation(
        TosState state,
        Register ret_addr,
        bool throw_monitor_exception,
        bool install_monitor_exception,
  bool notify_jvmdi) {
  // Note: Registers V0, V1 and F0, F1 may be in use for the result
  // check if synchronized method
  Label unlocked, unlock, no_unlock;

  // get the value of _do_not_unlock_if_synchronized into T8
#ifndef OPT_THREAD
  Register thread = T1;
  get_thread(thread);
#else
  Register thread = TREG;
#endif
  ld_b(T8, thread, in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  // reset the flag
  st_b(R0, thread, in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  // get method access flags
  ld_d(T3, FP, frame::interpreter_frame_method_offset * wordSize);
  ld_w(T1, T3, in_bytes(Method::access_flags_offset()));
  andi(T1, T1, JVM_ACC_SYNCHRONIZED);
  beq(T1, R0, unlocked);

  // Don't unlock anything if the _do_not_unlock_if_synchronized flag is set.
  bne(T8, R0, no_unlock);
  // unlock monitor
  push(state); // save result

  // BasicObjectLock will be first in list, since this is a
  // synchronized method. However, need to check that the object has
  // not been unlocked by an explicit monitorexit bytecode.
  addi_d(c_rarg0, FP, frame::interpreter_frame_initial_sp_offset * wordSize
      - (int)sizeof(BasicObjectLock));
  // address of first monitor
  ld_d(T1, c_rarg0, BasicObjectLock::obj_offset_in_bytes());
  bne(T1, R0, unlock);
  pop(state);
  if (throw_monitor_exception) {
    // Entry already unlocked, need to throw exception
    // I think LA do not need empty_FPU_stack
    // remove possible return value from FPU-stack, otherwise stack could overflow
    empty_FPU_stack();
    call_VM(NOREG, CAST_FROM_FN_PTR(address,
    InterpreterRuntime::throw_illegal_monitor_state_exception));
    should_not_reach_here();
  } else {
    // Monitor already unlocked during a stack unroll. If requested,
    // install an illegal_monitor_state_exception.  Continue with
    // stack unrolling.
    if (install_monitor_exception) {
      // remove possible return value from FPU-stack,
      // otherwise stack could overflow
      empty_FPU_stack();
      call_VM(NOREG, CAST_FROM_FN_PTR(address,
      InterpreterRuntime::new_illegal_monitor_state_exception));

    }

    b(unlocked);
  }

  bind(unlock);
  unlock_object(c_rarg0);
  pop(state);

  // Check that for block-structured locking (i.e., that all locked
  // objects has been unlocked)
  bind(unlocked);

  // V0, V1: Might contain return value

  // Check that all monitors are unlocked
  {
    Label loop, exception, entry, restart;
    const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;
    const Address monitor_block_top(FP,
        frame::interpreter_frame_monitor_block_top_offset * wordSize);

    bind(restart);
    // points to current entry, starting with top-most entry
    ld_d(c_rarg0, monitor_block_top);
    // points to word before bottom of monitor block
    addi_d(T3, FP, frame::interpreter_frame_initial_sp_offset * wordSize);
    b(entry);

    // Entry already locked, need to throw exception
    bind(exception);

    if (throw_monitor_exception) {
      // Throw exception
      // remove possible return value from FPU-stack,
      // otherwise stack could overflow
      empty_FPU_stack();
      MacroAssembler::call_VM(NOREG, CAST_FROM_FN_PTR(address,
                              InterpreterRuntime::throw_illegal_monitor_state_exception));
      should_not_reach_here();
    } else {
      // Stack unrolling. Unlock object and install illegal_monitor_exception
      // Unlock does not block, so don't have to worry about the frame
      // We don't have to preserve c_rarg0, since we are going to
      // throw an exception

      push(state);
      unlock_object(c_rarg0);
      pop(state);

      if (install_monitor_exception) {
        empty_FPU_stack();
        call_VM(NOREG, CAST_FROM_FN_PTR(address,
                                        InterpreterRuntime::new_illegal_monitor_state_exception));
      }

      b(restart);
    }

    bind(loop);
    ld_d(T1, c_rarg0, BasicObjectLock::obj_offset_in_bytes());
    bne(T1, R0, exception);// check if current entry is used

    addi_d(c_rarg0, c_rarg0, entry_size);// otherwise advance to next entry
    bind(entry);
    bne(c_rarg0, T3, loop);  // check if bottom reached
  }

  bind(no_unlock);

  // jvmpi support (jvmdi does not generate MethodExit on exception / popFrame)
  if (notify_jvmdi) {
    notify_method_exit(state, NotifyJVMTI); // preserve TOSCA
  } else {
    notify_method_exit(state, SkipNotifyJVMTI); // preserve TOSCA
  }

  // remove activation
  ld_d(SP, FP, frame::interpreter_frame_sender_sp_offset * wordSize);
  ld_d(ret_addr, FP, frame::interpreter_frame_return_addr_offset * wordSize);
  ld_d(FP, FP, frame::interpreter_frame_sender_fp_offset * wordSize);
}

#endif // C_INTERP

// Lock object
//
// Args:
//      c_rarg0: BasicObjectLock to be used for locking
//
// Kills:
//      T1
//      T2
void InterpreterMacroAssembler::lock_object(Register lock_reg) {
  assert(lock_reg == c_rarg0, "The argument is only for looks. It must be c_rarg0");

  if (UseHeavyMonitors) {
    call_VM(NOREG, CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorenter), lock_reg);
  } else {
    Label done, slow_case;
    const Register tmp_reg = T2;
    const Register scr_reg = T1;
    const int obj_offset = BasicObjectLock::obj_offset_in_bytes();
    const int lock_offset = BasicObjectLock::lock_offset_in_bytes ();
    const int mark_offset = lock_offset + BasicLock::displaced_header_offset_in_bytes();

    // Load object pointer into scr_reg
    ld_d(scr_reg, lock_reg, obj_offset);

    if (UseBiasedLocking) {
      // Note: we use noreg for the temporary register since it's hard
      // to come up with a free register on all incoming code paths
      biased_locking_enter(lock_reg, scr_reg, tmp_reg, noreg, false, done, &slow_case);
    }

    // Load (object->mark() | 1) into tmp_reg
    ld_d(AT, scr_reg, 0);
    ori(tmp_reg, AT, 1);

    // Save (object->mark() | 1) into BasicLock's displaced header
    st_d(tmp_reg, lock_reg, mark_offset);

    assert(lock_offset == 0, "displached header must be first word in BasicObjectLock");

    if (PrintBiasedLockingStatistics) {
      Label succ, fail;
      cmpxchg(Address(scr_reg, 0), tmp_reg, lock_reg, AT, true, false, succ, &fail);
      bind(succ);
      atomic_inc32((address)BiasedLocking::fast_path_entry_count_addr(), 1, AT, scr_reg);
      b(done);
      bind(fail);
    } else {
      cmpxchg(Address(scr_reg, 0), tmp_reg, lock_reg, AT, true, false, done);
    }

    // Test if the oopMark is an obvious stack pointer, i.e.,
    //  1) (mark & 3) == 0, and
    //  2) SP <= mark < SP + os::pagesize()
    //
    // These 3 tests can be done by evaluating the following
    // expression: ((mark - sp) & (3 - os::vm_page_size())),
    // assuming both stack pointer and pagesize have their
    // least significant 2 bits clear.
    // NOTE: the oopMark is in tmp_reg as the result of cmpxchg
    sub_d(tmp_reg, tmp_reg, SP);
    li(AT, 7 - os::vm_page_size());
    andr(tmp_reg, tmp_reg, AT);
    // Save the test result, for recursive case, the result is zero
    st_d(tmp_reg, lock_reg, mark_offset);
    if (PrintBiasedLockingStatistics) {
      bnez(tmp_reg, slow_case);
      atomic_inc32((address)BiasedLocking::fast_path_entry_count_addr(), 1, AT, scr_reg);
    }
    beqz(tmp_reg, done);

    bind(slow_case);
    // Call the runtime routine for slow case
    call_VM(NOREG, CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorenter), lock_reg);

    bind(done);
  }
}

// Unlocks an object. Used in monitorexit bytecode and
// remove_activation.  Throws an IllegalMonitorException if object is
// not locked by current thread.
//
// Args:
//      c_rarg0: BasicObjectLock for lock
//
// Kills:
//      T1
//      T2
//      T3
// Throw an IllegalMonitorException if object is not locked by current thread
void InterpreterMacroAssembler::unlock_object(Register lock_reg) {
  assert(lock_reg == c_rarg0, "The argument is only for looks. It must be c_rarg0");

  if (UseHeavyMonitors) {
    call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorexit), lock_reg);
  } else {
    Label done;
    const Register tmp_reg = T1;
    const Register scr_reg = T2;
    const Register hdr_reg = T3;

    save_bcp(); // Save in case of exception

    // Convert from BasicObjectLock structure to object and BasicLock structure
    // Store the BasicLock address into tmp_reg
    addi_d(tmp_reg, lock_reg, BasicObjectLock::lock_offset_in_bytes());

    // Load oop into scr_reg
    ld_d(scr_reg, lock_reg, BasicObjectLock::obj_offset_in_bytes());
    // free entry
    st_d(R0, lock_reg, BasicObjectLock::obj_offset_in_bytes());
    if (UseBiasedLocking) {
      biased_locking_exit(scr_reg, hdr_reg, done);
    }

    // Load the old header from BasicLock structure
    ld_d(hdr_reg, tmp_reg, BasicLock::displaced_header_offset_in_bytes());
    // zero for recursive case
    beqz(hdr_reg, done);

    // Atomic swap back the old header
    cmpxchg(Address(scr_reg, 0), tmp_reg, hdr_reg, AT, false, false, done);

    // Call the runtime routine for slow case.
    st_d(scr_reg, lock_reg, BasicObjectLock::obj_offset_in_bytes()); // restore obj
    call_VM(NOREG,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorexit),
            lock_reg);

    bind(done);

    restore_bcp();
  }
}

#ifndef CC_INTERP

void InterpreterMacroAssembler::test_method_data_pointer(Register mdp,
                                                         Label& zero_continue) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  ld_d(mdp, Address(FP, frame::interpreter_frame_mdx_offset * wordSize));
  beq(mdp, R0, zero_continue);
}


// Set the method data pointer for the current bcp.
void InterpreterMacroAssembler::set_method_data_pointer_for_bcp() {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Label set_mdp;

  // V0 and T0 will be used as two temporary registers.
  push2(V0, T0);

  get_method(T0);
  // Test MDO to avoid the call if it is NULL.
  ld_d(V0, T0, in_bytes(Method::method_data_offset()));
  beq(V0, R0, set_mdp);

  // method: T0
  // bcp: BCP --> S0
  call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::bcp_to_di), T0, BCP);
  // mdi: V0
  // mdo is guaranteed to be non-zero here, we checked for it before the call.
  get_method(T0);
  ld_d(T0, T0, in_bytes(Method::method_data_offset()));
  addi_d(T0, T0, in_bytes(MethodData::data_offset()));
  add_d(V0, T0, V0);
  bind(set_mdp);
  st_d(V0, FP, frame::interpreter_frame_mdx_offset * wordSize);
  pop2(T0, V0);
}

void InterpreterMacroAssembler::verify_method_data_pointer() {
  assert(ProfileInterpreter, "must be profiling interpreter");
#ifdef ASSERT
  Label verify_continue;
  Register method = T5;
  Register mdp = T6;
  Register tmp = A0;
  push(method);
  push(mdp);
  push(tmp);
  test_method_data_pointer(mdp, verify_continue); // If mdp is zero, continue
  get_method(method);

  // If the mdp is valid, it will point to a DataLayout header which is
  // consistent with the bcp.  The converse is highly probable also.
  ld_hu(tmp, mdp, in_bytes(DataLayout::bci_offset()));
  ld_d(AT, method, in_bytes(Method::const_offset()));
  add_d(tmp, tmp, AT);
  addi_d(tmp, tmp, in_bytes(ConstMethod::codes_offset()));
  beq(tmp, BCP, verify_continue);
  call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::verify_mdp), method, BCP, mdp);
  bind(verify_continue);
  pop(tmp);
  pop(mdp);
  pop(method);
#endif // ASSERT
}


void InterpreterMacroAssembler::set_mdp_data_at(Register mdp_in,
                                                int constant,
                                                Register value) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Address data(mdp_in, constant);
  st_d(value, data);
}


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      int constant,
                                                      bool decrement) {
  // Counter address
  Address data(mdp_in, constant);

  increment_mdp_data_at(data, decrement);
}

void InterpreterMacroAssembler::increment_mdp_data_at(Address data,
                                                      bool decrement) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  // %%% this does 64bit counters at best it is wasting space
  // at worst it is a rare bug when counters overflow
  Register tmp = S0;
  push(tmp);
  if (decrement) {
    // Decrement the register.
    ld_d(AT, data);
    addi_d(tmp, AT, (int32_t) -DataLayout::counter_increment);
    // If the decrement causes the counter to overflow, stay negative
    Label L;
    blt(tmp, R0, L);
    addi_d(tmp, tmp, (int32_t) DataLayout::counter_increment);
    bind(L);
    st_d(tmp, data);
  } else {
    assert(DataLayout::counter_increment == 1,
           "flow-free idiom only works with 1");
    ld_d(AT, data);
    // Increment the register.
    addi_d(tmp, AT, DataLayout::counter_increment);
    // If the increment causes the counter to overflow, pull back by 1.
    slt(AT, tmp, R0);
    sub_d(tmp, tmp, AT);
    st_d(tmp, data);
  }
  pop(tmp);
}


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      Register reg,
                                                      int constant,
                                                      bool decrement) {
  Register tmp = S0;
  push(S0);
  if (decrement) {
    // Decrement the register.
    add_d(AT, mdp_in, reg);
    assert(Assembler::is_simm(constant, 12), "constant is not a simm12 !");
    ld_d(AT, AT, constant);

    addi_d(tmp, AT, (int32_t) -DataLayout::counter_increment);
    // If the decrement causes the counter to overflow, stay negative
    Label L;
    blt(tmp, R0, L);
    addi_d(tmp, tmp, (int32_t) DataLayout::counter_increment);
    bind(L);

    add_d(AT, mdp_in, reg);
    st_d(tmp, AT, constant);
  } else {
    add_d(AT, mdp_in, reg);
    assert(Assembler::is_simm(constant, 12), "constant is not a simm12 !");
    ld_d(AT, AT, constant);

    // Increment the register.
    addi_d(tmp, AT, DataLayout::counter_increment);
    // If the increment causes the counter to overflow, pull back by 1.
    slt(AT, tmp, R0);
    sub_d(tmp, tmp, AT);

    add_d(AT, mdp_in, reg);
    st_d(tmp, AT, constant);
  }
  pop(S0);
}

void InterpreterMacroAssembler::set_mdp_flag_at(Register mdp_in,
                                                int flag_byte_constant) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  int header_offset = in_bytes(DataLayout::header_offset());
  int header_bits = DataLayout::flag_mask_to_header_mask(flag_byte_constant);
  // Set the flag
  ld_w(AT, Address(mdp_in, header_offset));
  if(Assembler::is_simm(header_bits, 12)) {
    ori(AT, AT, header_bits);
  } else {
    push(T8);
    // T8 is used as a temporary register.
    li(T8, header_bits);
    orr(AT, AT, T8);
    pop(T8);
  }
  st_w(AT, Address(mdp_in, header_offset));
}


void InterpreterMacroAssembler::test_mdp_data_at(Register mdp_in,
                                                 int offset,
                                                 Register value,
                                                 Register test_value_out,
                                                 Label& not_equal_continue) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  if (test_value_out == noreg) {
    ld_d(AT, Address(mdp_in, offset));
    bne(AT, value, not_equal_continue);
  } else {
    // Put the test value into a register, so caller can use it:
    ld_d(test_value_out, Address(mdp_in, offset));
    bne(value, test_value_out, not_equal_continue);
  }
}


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     int offset_of_disp) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  assert(Assembler::is_simm(offset_of_disp, 12), "offset is not an simm12");
  ld_d(AT, mdp_in, offset_of_disp);
  add_d(mdp_in, mdp_in, AT);
  st_d(mdp_in, Address(FP, frame::interpreter_frame_mdx_offset * wordSize));
}


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     Register reg,
                                                     int offset_of_disp) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  add_d(AT, reg, mdp_in);
  assert(Assembler::is_simm(offset_of_disp, 12), "offset is not an simm12");
  ld_d(AT, AT, offset_of_disp);
  add_d(mdp_in, mdp_in, AT);
  st_d(mdp_in, Address(FP, frame::interpreter_frame_mdx_offset * wordSize));
}


void InterpreterMacroAssembler::update_mdp_by_constant(Register mdp_in,
                                                       int constant) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  if(Assembler::is_simm(constant, 12)) {
    addi_d(mdp_in, mdp_in, constant);
  } else {
    li(AT, constant);
    add_d(mdp_in, mdp_in, AT);
  }
  st_d(mdp_in, Address(FP, frame::interpreter_frame_mdx_offset * wordSize));
}


void InterpreterMacroAssembler::update_mdp_for_ret(Register return_bci) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  push(return_bci); // save/restore across call_VM
  call_VM(noreg,
          CAST_FROM_FN_PTR(address, InterpreterRuntime::update_mdp_for_ret),
          return_bci);
  pop(return_bci);
}


void InterpreterMacroAssembler::profile_taken_branch(Register mdp,
                                                     Register bumped_count) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    // Otherwise, assign to mdp
    test_method_data_pointer(mdp, profile_continue);

    // We are taking a branch.  Increment the taken count.
    // We inline increment_mdp_data_at to return bumped_count in a register
    //increment_mdp_data_at(mdp, in_bytes(JumpData::taken_offset()));
    ld_d(bumped_count, mdp, in_bytes(JumpData::taken_offset()));
    assert(DataLayout::counter_increment == 1,
           "flow-free idiom only works with 1");
    push(T8);
    // T8 is used as a temporary register.
    addi_d(T8, bumped_count, DataLayout::counter_increment);
    slt(AT, T8, R0);
    sub_d(bumped_count, T8, AT);
    pop(T8);
    st_d(bumped_count, mdp, in_bytes(JumpData::taken_offset())); // Store back out
    // The method data pointer needs to be updated to reflect the new target.
    update_mdp_by_offset(mdp, in_bytes(JumpData::displacement_offset()));
    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_not_taken_branch(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // We are taking a branch.  Increment the not taken count.
    increment_mdp_data_at(mdp, in_bytes(BranchData::not_taken_offset()));

    // The method data pointer needs to be updated to correspond to
    // the next bytecode
    update_mdp_by_constant(mdp, in_bytes(BranchData::branch_data_size()));
    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_call(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // We are making a call.  Increment the count.
    increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));

    // The method data pointer needs to be updated to reflect the new target.
    update_mdp_by_constant(mdp, in_bytes(CounterData::counter_data_size()));
    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_final_call(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // We are making a call.  Increment the count.
    increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));

    // The method data pointer needs to be updated to reflect the new target.
    update_mdp_by_constant(mdp,
                           in_bytes(VirtualCallData::
                                    virtual_call_data_size()));
    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_virtual_call(Register receiver,
                                                     Register mdp,
                                                     Register reg2,
                                                     bool receiver_can_be_null) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    Label skip_receiver_profile;
    if (receiver_can_be_null) {
      Label not_null;
      bnez(receiver, not_null);
      // We are making a call.  Increment the count.
      increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));
      b(skip_receiver_profile);
      bind(not_null);
    }

    // Record the receiver type.
    record_klass_in_profile(receiver, mdp, reg2, true);
    bind(skip_receiver_profile);

    // The method data pointer needs to be updated to reflect the new target.
    update_mdp_by_constant(mdp,
                           in_bytes(VirtualCallData::
                                    virtual_call_data_size()));
    bind(profile_continue);
  }
}

// This routine creates a state machine for updating the multi-row
// type profile at a virtual call site (or other type-sensitive bytecode).
// The machine visits each row (of receiver/count) until the receiver type
// is found, or until it runs out of rows.  At the same time, it remembers
// the location of the first empty row.  (An empty row records null for its
// receiver, and can be allocated for a newly-observed receiver type.)
// Because there are two degrees of freedom in the state, a simple linear
// search will not work; it must be a decision tree.  Hence this helper
// function is recursive, to generate the required tree structured code.
// It's the interpreter, so we are trading off code space for speed.
// See below for example code.
void InterpreterMacroAssembler::record_klass_in_profile_helper(
                                        Register receiver, Register mdp,
                                        Register reg2, int start_row,
                                        Label& done, bool is_virtual_call) {
  if (TypeProfileWidth == 0) {
    if (is_virtual_call) {
      increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));
    }
    return;
  }

  int last_row = VirtualCallData::row_limit() - 1;
  assert(start_row <= last_row, "must be work left to do");
  // Test this row for both the receiver and for null.
  // Take any of three different outcomes:
  //   1. found receiver => increment count and goto done
  //   2. found null => keep looking for case 1, maybe allocate this cell
  //   3. found something else => keep looking for cases 1 and 2
  // Case 3 is handled by a recursive call.
  for (int row = start_row; row <= last_row; row++) {
    Label next_test;
    bool test_for_null_also = (row == start_row);

    // See if the receiver is receiver[n].
    int recvr_offset = in_bytes(VirtualCallData::receiver_offset(row));
    test_mdp_data_at(mdp, recvr_offset, receiver,
                     (test_for_null_also ? reg2 : noreg),
                     next_test);
    // (Reg2 now contains the receiver from the CallData.)

    // The receiver is receiver[n].  Increment count[n].
    int count_offset = in_bytes(VirtualCallData::receiver_count_offset(row));
    increment_mdp_data_at(mdp, count_offset);
    beq(R0, R0, done);
    bind(next_test);

    if (test_for_null_also) {
      Label found_null;
      // Failed the equality check on receiver[n]...  Test for null.
      if (start_row == last_row) {
        // The only thing left to do is handle the null case.
        if (is_virtual_call) {
          beq(reg2, R0, found_null);
          // Receiver did not match any saved receiver and there is no empty row for it.
          // Increment total counter to indicate polymorphic case.
          increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));
          beq(R0, R0, done);
          bind(found_null);
        } else {
          bne(reg2, R0, done);
        }
        break;
      }
      // Since null is rare, make it be the branch-taken case.
      beq(reg2, R0, found_null);

      // Put all the "Case 3" tests here.
      record_klass_in_profile_helper(receiver, mdp, reg2, start_row + 1, done, is_virtual_call);

      // Found a null.  Keep searching for a matching receiver,
      // but remember that this is an empty (unused) slot.
      bind(found_null);
    }
  }

  // In the fall-through case, we found no matching receiver, but we
  // observed the receiver[start_row] is NULL.

  // Fill in the receiver field and increment the count.
  int recvr_offset = in_bytes(VirtualCallData::receiver_offset(start_row));
  set_mdp_data_at(mdp, recvr_offset, receiver);
  int count_offset = in_bytes(VirtualCallData::receiver_count_offset(start_row));
  li(reg2, DataLayout::counter_increment);
  set_mdp_data_at(mdp, count_offset, reg2);
  if (start_row > 0) {
    beq(R0, R0, done);
  }
}

// Example state machine code for three profile rows:
//   // main copy of decision tree, rooted at row[1]
//   if (row[0].rec == rec) { row[0].incr(); goto done; }
//   if (row[0].rec != NULL) {
//     // inner copy of decision tree, rooted at row[1]
//     if (row[1].rec == rec) { row[1].incr(); goto done; }
//     if (row[1].rec != NULL) {
//       // degenerate decision tree, rooted at row[2]
//       if (row[2].rec == rec) { row[2].incr(); goto done; }
//       if (row[2].rec != NULL) { goto done; } // overflow
//       row[2].init(rec); goto done;
//     } else {
//       // remember row[1] is empty
//       if (row[2].rec == rec) { row[2].incr(); goto done; }
//       row[1].init(rec); goto done;
//     }
//   } else {
//     // remember row[0] is empty
//     if (row[1].rec == rec) { row[1].incr(); goto done; }
//     if (row[2].rec == rec) { row[2].incr(); goto done; }
//     row[0].init(rec); goto done;
//   }
//   done:

void InterpreterMacroAssembler::record_klass_in_profile(Register receiver,
                                                        Register mdp, Register reg2,
                                                        bool is_virtual_call) {
  assert(ProfileInterpreter, "must be profiling");
  Label done;

  record_klass_in_profile_helper(receiver, mdp, reg2, 0, done, is_virtual_call);

  bind (done);
}

void InterpreterMacroAssembler::profile_ret(Register return_bci,
                                            Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;
    uint row;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // Update the total ret count.
    increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));

    for (row = 0; row < RetData::row_limit(); row++) {
      Label next_test;

      // See if return_bci is equal to bci[n]:
      test_mdp_data_at(mdp,
                       in_bytes(RetData::bci_offset(row)),
                       return_bci, noreg,
                       next_test);

      // return_bci is equal to bci[n].  Increment the count.
      increment_mdp_data_at(mdp, in_bytes(RetData::bci_count_offset(row)));

      // The method data pointer needs to be updated to reflect the new target.
      update_mdp_by_offset(mdp,
                           in_bytes(RetData::bci_displacement_offset(row)));
      b(profile_continue);
      bind(next_test);
    }

    update_mdp_for_ret(return_bci);

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_null_seen(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    set_mdp_flag_at(mdp, BitData::null_seen_byte_constant());

    // The method data pointer needs to be updated.
    int mdp_delta = in_bytes(BitData::bit_data_size());
    if (TypeProfileCasts) {
      mdp_delta = in_bytes(VirtualCallData::virtual_call_data_size());
    }
    update_mdp_by_constant(mdp, mdp_delta);

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_typecheck_failed(Register mdp) {
  if (ProfileInterpreter && TypeProfileCasts) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    int count_offset = in_bytes(CounterData::count_offset());
    // Back up the address, since we have already bumped the mdp.
    count_offset -= in_bytes(VirtualCallData::virtual_call_data_size());

    // *Decrement* the counter.  We expect to see zero or small negatives.
    increment_mdp_data_at(mdp, count_offset, true);

    bind (profile_continue);
  }
}


void InterpreterMacroAssembler::profile_typecheck(Register mdp, Register klass, Register reg2) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // The method data pointer needs to be updated.
    int mdp_delta = in_bytes(BitData::bit_data_size());
    if (TypeProfileCasts) {
      mdp_delta = in_bytes(VirtualCallData::virtual_call_data_size());

      // Record the object type.
      record_klass_in_profile(klass, mdp, reg2, false);
    }
    update_mdp_by_constant(mdp, mdp_delta);

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_switch_default(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // Update the default case count
    increment_mdp_data_at(mdp,
                          in_bytes(MultiBranchData::default_count_offset()));

    // The method data pointer needs to be updated.
    update_mdp_by_offset(mdp,
                         in_bytes(MultiBranchData::
                                  default_displacement_offset()));

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_switch_case(Register index,
                                                    Register mdp,
                                                    Register reg2) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // Build the base (index * per_case_size_in_bytes()) +
    // case_array_offset_in_bytes()
    li(reg2, in_bytes(MultiBranchData::per_case_size()));
    mul_d(index, index, reg2);
    addi_d(index, index, in_bytes(MultiBranchData::case_array_offset()));

    // Update the case count
    increment_mdp_data_at(mdp,
                          index,
                          in_bytes(MultiBranchData::relative_count_offset()));

    // The method data pointer needs to be updated.
    update_mdp_by_offset(mdp,
                         index,
                         in_bytes(MultiBranchData::
                                  relative_displacement_offset()));

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::narrow(Register result) {
  // Get method->_constMethod->_result_type
  ld_d(T4, FP, frame::interpreter_frame_method_offset * wordSize);
  ld_d(T4, T4, in_bytes(Method::const_offset()));
  ld_bu(T4, T4, in_bytes(ConstMethod::result_type_offset()));

  Label done, notBool, notByte, notChar;

  // common case first
  addi_d(AT, T4, -T_INT);
  beq(AT, R0, done);

  // mask integer result to narrower return type.
  addi_d(AT, T4, -T_BOOLEAN);
  bne(AT, R0, notBool);
  andi(result, result, 0x1);
  beq(R0, R0, done);

  bind(notBool);
  addi_d(AT, T4, -T_BYTE);
  bne(AT, R0, notByte);
  ext_w_b(result, result);
  beq(R0, R0, done);

  bind(notByte);
  addi_d(AT, T4, -T_CHAR);
  bne(AT, R0, notChar);
  bstrpick_d(result, result, 15, 0);
  beq(R0, R0, done);

  bind(notChar);
  ext_w_h(result, result);

  // Nothing to do for T_INT
  bind(done);
}


void InterpreterMacroAssembler::profile_obj_type(Register obj, const Address& mdo_addr) {
  Label update, next, none;

  verify_oop(obj);

  if (mdo_addr.index() != noreg) {
    guarantee(T0 != mdo_addr.base(), "The base register will be corrupted !");
    guarantee(T0 != mdo_addr.index(), "The index register will be corrupted !");
    push(T0);
    alsl_d(T0, mdo_addr.index(), mdo_addr.base(), mdo_addr.scale() - 1);
  }

  bnez(obj, update);

  if (mdo_addr.index() == noreg) {
    ld_d(AT, mdo_addr);
  } else {
    ld_d(AT, T0, mdo_addr.disp());
  }
  ori(AT, AT, TypeEntries::null_seen);
  if (mdo_addr.index() == noreg) {
    st_d(AT, mdo_addr);
  } else {
    st_d(AT, T0, mdo_addr.disp());
  }

  b(next);

  bind(update);
  load_klass(obj, obj);

  if (mdo_addr.index() == noreg) {
    ld_d(AT, mdo_addr);
  } else {
    ld_d(AT, T0, mdo_addr.disp());
  }
  xorr(obj, obj, AT);

  assert(TypeEntries::type_klass_mask == -4, "must be");
  bstrpick_d(AT, obj, 63, 2);
  beqz(AT, next);

  andi(AT, obj, TypeEntries::type_unknown);
  bnez(AT, next);

  if (mdo_addr.index() == noreg) {
    ld_d(AT, mdo_addr);
  } else {
    ld_d(AT, T0, mdo_addr.disp());
  }
  beqz(AT, none);

  addi_d(AT, AT, -(TypeEntries::null_seen));
  beqz(AT, none);

  // There is a chance that the checks above (re-reading profiling
  // data from memory) fail if another thread has just set the
  // profiling to this obj's klass
  if (mdo_addr.index() == noreg) {
    ld_d(AT, mdo_addr);
  } else {
    ld_d(AT, T0, mdo_addr.disp());
  }
  xorr(obj, obj, AT);
  assert(TypeEntries::type_klass_mask == -4, "must be");
  bstrpick_d(AT, obj, 63, 2);
  beqz(AT, next);

  // different than before. Cannot keep accurate profile.
  if (mdo_addr.index() == noreg) {
    ld_d(AT, mdo_addr);
  } else {
    ld_d(AT, T0, mdo_addr.disp());
  }
  ori(AT, AT, TypeEntries::type_unknown);
  if (mdo_addr.index() == noreg) {
    st_d(AT, mdo_addr);
  } else {
    st_d(AT, T0, mdo_addr.disp());
  }
  b(next);

  bind(none);
  // first time here. Set profile type.
  if (mdo_addr.index() == noreg) {
    st_d(obj, mdo_addr);
  } else {
    st_d(obj, T0, mdo_addr.disp());
  }

  bind(next);
  if (mdo_addr.index() != noreg) {
    pop(T0);
  }
}

void InterpreterMacroAssembler::profile_arguments_type(Register mdp, Register callee, Register tmp, bool is_virtual) {
  if (!ProfileInterpreter) {
    return;
  }

  if (MethodData::profile_arguments() || MethodData::profile_return()) {
    Label profile_continue;

    test_method_data_pointer(mdp, profile_continue);

    int off_to_start = is_virtual ? in_bytes(VirtualCallData::virtual_call_data_size()) : in_bytes(CounterData::counter_data_size());

    ld_b(AT, mdp, in_bytes(DataLayout::tag_offset()) - off_to_start);
    li(tmp, is_virtual ? DataLayout::virtual_call_type_data_tag : DataLayout::call_type_data_tag);
    bne(tmp, AT, profile_continue);


    if (MethodData::profile_arguments()) {
      Label done;
      int off_to_args = in_bytes(TypeEntriesAtCall::args_data_offset());
      if (Assembler::is_simm(off_to_args, 12)) {
        addi_d(mdp, mdp, off_to_args);
      } else {
        li(AT, off_to_args);
        add_d(mdp, mdp, AT);
      }


      for (int i = 0; i < TypeProfileArgsLimit; i++) {
        if (i > 0 || MethodData::profile_return()) {
          // If return value type is profiled we may have no argument to profile
          ld_d(tmp, mdp, in_bytes(TypeEntriesAtCall::cell_count_offset())-off_to_args);

          if (Assembler::is_simm(-1 * i * TypeStackSlotEntries::per_arg_count(), 12)) {
            addi_w(tmp, tmp, -1 * i * TypeStackSlotEntries::per_arg_count());
          } else {
            li(AT, i*TypeStackSlotEntries::per_arg_count());
            sub_w(tmp, tmp, AT);
          }

          li(AT, TypeStackSlotEntries::per_arg_count());
          blt(tmp, AT, done);
        }
        ld_d(tmp, callee, in_bytes(Method::const_offset()));

        ld_hu(tmp, tmp, in_bytes(ConstMethod::size_of_parameters_offset()));

        // stack offset o (zero based) from the start of the argument
        // list, for n arguments translates into offset n - o - 1 from
        // the end of the argument list
        ld_d(AT, mdp, in_bytes(TypeEntriesAtCall::stack_slot_offset(i))-off_to_args);
        sub_d(tmp, tmp, AT);

        addi_w(tmp, tmp, -1);

        Address arg_addr = argument_address(tmp);
        ld_d(tmp, arg_addr);

        Address mdo_arg_addr(mdp, in_bytes(TypeEntriesAtCall::argument_type_offset(i))-off_to_args);
        profile_obj_type(tmp, mdo_arg_addr);

        int to_add = in_bytes(TypeStackSlotEntries::per_arg_size());
        if (Assembler::is_simm(to_add, 12)) {
          addi_d(mdp, mdp, to_add);
        } else {
          li(AT, to_add);
          add_d(mdp, mdp, AT);
        }

        off_to_args += to_add;
      }

      if (MethodData::profile_return()) {
        ld_d(tmp, mdp, in_bytes(TypeEntriesAtCall::cell_count_offset())-off_to_args);

        int tmp_arg_counts = TypeProfileArgsLimit*TypeStackSlotEntries::per_arg_count();
        if (Assembler::is_simm(-1 * tmp_arg_counts, 12)) {
          addi_w(tmp, tmp, -1 * tmp_arg_counts);
        } else {
          li(AT, tmp_arg_counts);
          sub_w(mdp, mdp, AT);
        }
      }

      bind(done);

      if (MethodData::profile_return()) {
        // We're right after the type profile for the last
        // argument. tmp is the number of cells left in the
        // CallTypeData/VirtualCallTypeData to reach its end. Non null
        // if there's a return to profile.
        assert(ReturnTypeEntry::static_cell_count() < TypeStackSlotEntries::per_arg_count(), "can't move past ret type");
        slli_w(tmp, tmp, exact_log2(DataLayout::cell_size));
        add_d(mdp, mdp, tmp);
      }
      st_d(mdp, FP, frame::interpreter_frame_mdx_offset * wordSize);
    } else {
      assert(MethodData::profile_return(), "either profile call args or call ret");
      update_mdp_by_constant(mdp, in_bytes(TypeEntriesAtCall::return_only_size()));
    }

    // mdp points right after the end of the
    // CallTypeData/VirtualCallTypeData, right after the cells for the
    // return value type if there's one

    bind(profile_continue);
  }
}

void InterpreterMacroAssembler::profile_return_type(Register mdp, Register ret, Register tmp) {
  assert_different_registers(mdp, ret, tmp, _bcp_register);
  if (ProfileInterpreter && MethodData::profile_return()) {
    Label profile_continue, done;

    test_method_data_pointer(mdp, profile_continue);

    if (MethodData::profile_return_jsr292_only()) {
      // If we don't profile all invoke bytecodes we must make sure
      // it's a bytecode we indeed profile. We can't go back to the
      // begining of the ProfileData we intend to update to check its
      // type because we're right after it and we don't known its
      // length
      Label do_profile;
      ld_b(tmp, _bcp_register, 0);
      addi_d(AT, tmp, -1 * Bytecodes::_invokedynamic);
      beqz(AT, do_profile);
      addi_d(AT, tmp, -1 * Bytecodes::_invokehandle);
      beqz(AT, do_profile);

      get_method(tmp);
      ld_b(tmp, tmp, Method::intrinsic_id_offset_in_bytes());
      li(AT, vmIntrinsics::_compiledLambdaForm);
      bne(tmp, AT, profile_continue);

      bind(do_profile);
    }

    Address mdo_ret_addr(mdp, -in_bytes(ReturnTypeEntry::size()));
    add_d(tmp, ret, R0);
    profile_obj_type(tmp, mdo_ret_addr);

    bind(profile_continue);
  }
}

void InterpreterMacroAssembler::profile_parameters_type(Register mdp, Register tmp1, Register tmp2) {
  guarantee(T4 == tmp1, "You are reqired to use T4 as the index register for LoongArch !");

  if (ProfileInterpreter && MethodData::profile_parameters()) {
    Label profile_continue, done;

    test_method_data_pointer(mdp, profile_continue);

    // Load the offset of the area within the MDO used for
    // parameters. If it's negative we're not profiling any parameters
    ld_w(tmp1, mdp, in_bytes(MethodData::parameters_type_data_di_offset()) - in_bytes(MethodData::data_offset()));
    blt(tmp1, R0, profile_continue);

    // Compute a pointer to the area for parameters from the offset
    // and move the pointer to the slot for the last
    // parameters. Collect profiling from last parameter down.
    // mdo start + parameters offset + array length - 1
    add_d(mdp, mdp, tmp1);
    ld_d(tmp1, mdp, in_bytes(ArrayData::array_len_offset()));
    decrement(tmp1, TypeStackSlotEntries::per_arg_count());


    Label loop;
    bind(loop);

    int off_base = in_bytes(ParametersTypeData::stack_slot_offset(0));
    int type_base = in_bytes(ParametersTypeData::type_offset(0));
    Address::ScaleFactor per_arg_scale = Address::times(DataLayout::cell_size);
    Address arg_type(mdp, tmp1, per_arg_scale, type_base);

    // load offset on the stack from the slot for this parameter
    alsl_d(AT, tmp1, mdp, per_arg_scale - 1);
    ld_d(tmp2, AT, off_base);

    sub_d(tmp2, R0, tmp2);

    // read the parameter from the local area
    slli_d(AT, tmp2, Interpreter::stackElementScale());
    ldx_d(tmp2, AT, _locals_register);

    // profile the parameter
    profile_obj_type(tmp2, arg_type);

    // go to next parameter
    decrement(tmp1, TypeStackSlotEntries::per_arg_count());
    blt(R0, tmp1, loop);

    bind(profile_continue);
  }
}

void InterpreterMacroAssembler::verify_oop(Register reg, TosState state) {
  if (state == atos) {
    MacroAssembler::verify_oop(reg);
  }
}

void InterpreterMacroAssembler::verify_FPU(int stack_depth, TosState state) {
}
#endif // !CC_INTERP


void InterpreterMacroAssembler::notify_method_entry() {
  // Whenever JVMTI is interp_only_mode, method entry/exit events are sent to
  // track stack depth.  If it is possible to enter interp_only_mode we add
  // the code to check if the event should be sent.
  Register tempreg = T0;
#ifndef OPT_THREAD
    get_thread(T8);
#else
    move(T8, TREG);
#endif
  if (JvmtiExport::can_post_interpreter_events()) {
    Label L;
    ld_w(tempreg, T8, in_bytes(JavaThread::interp_only_mode_offset()));
    beq(tempreg, R0, L);
    call_VM(noreg, CAST_FROM_FN_PTR(address,
                                    InterpreterRuntime::post_method_entry));
    bind(L);
  }

  {
    SkipIfEqual skip_if(this, &DTraceMethodProbes, 0);
    get_method(S3);
    call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_entry),
                                  //Rthread,
                                  T8,
                                  //Rmethod);
                                  S3);
  }
}

void InterpreterMacroAssembler::notify_method_exit(
    TosState state, NotifyMethodExitMode mode) {
  Register tempreg = T0;
#ifndef OPT_THREAD
    get_thread(T8);
#else
    move(T8, TREG);
#endif
  // Whenever JVMTI is interp_only_mode, method entry/exit events are sent to
  // track stack depth.  If it is possible to enter interp_only_mode we add
  // the code to check if the event should be sent.
  if (mode == NotifyJVMTI && JvmtiExport::can_post_interpreter_events()) {
    Label skip;
    // Note: frame::interpreter_frame_result has a dependency on how the
    // method result is saved across the call to post_method_exit. If this
    // is changed then the interpreter_frame_result implementation will
    // need to be updated too.

    // For c++ interpreter the result is always stored at a known location in the frame
    // template interpreter will leave it on the top of the stack.
    NOT_CC_INTERP(push(state);)
    ld_w(tempreg, T8, in_bytes(JavaThread::interp_only_mode_offset()));
    beq(tempreg, R0, skip);
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::post_method_exit));
    bind(skip);
    NOT_CC_INTERP(pop(state));
  }

  {
    // Dtrace notification
    SkipIfEqual skip_if(this, &DTraceMethodProbes, 0);
    NOT_CC_INTERP(push(state));
    get_method(S3);
    call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_exit),
                 //Rthread, Rmethod);
                 T8, S3);
    NOT_CC_INTERP(pop(state));
  }
}

// Jump if ((*counter_addr += increment) & mask) satisfies the condition.
void InterpreterMacroAssembler::increment_mask_and_jump(Address counter_addr,
                                                        int increment, int mask,
                                                        Register scratch, bool preloaded,
                                                        Condition cond, Label* where) {
  assert_different_registers(scratch, AT);

  if (!preloaded) {
    ld_w(scratch, counter_addr);
  }
  addi_w(scratch, scratch, increment);
  st_w(scratch, counter_addr);

  li(AT, mask);
  andr(scratch, scratch, AT);

  if (cond == Assembler::zero) {
    beq(scratch, R0, *where);
  } else {
    unimplemented();
  }
}
