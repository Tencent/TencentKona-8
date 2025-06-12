/*
 * Copyright (c) 2000, 2021, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "asm/assembler.hpp"
#include "c1/c1_CodeStubs.hpp"
#include "c1/c1_Compilation.hpp"
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "c1/c1_ValueStack.hpp"
#include "ci/ciArrayKlass.hpp"
#include "ci/ciInstance.hpp"
#include "code/compiledIC.hpp"
#include "nativeInst_loongarch.hpp"
#include "oops/objArrayKlass.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "vmreg_loongarch.inline.hpp"

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
#define T5 RT5
#define T6 RT6
#define T7 RT7
#define T8 RT8

#ifndef PRODUCT
#define COMMENT(x) do { __ block_comment(x); } while (0)
#else
#define COMMENT(x)
#endif

NEEDS_CLEANUP // remove this definitions?

#define __ _masm->

static void select_different_registers(Register preserve, Register extra,
                                       Register &tmp1, Register &tmp2) {
  if (tmp1 == preserve) {
    assert_different_registers(tmp1, tmp2, extra);
    tmp1 = extra;
  } else if (tmp2 == preserve) {
    assert_different_registers(tmp1, tmp2, extra);
    tmp2 = extra;
  }
  assert_different_registers(preserve, tmp1, tmp2);
}

static void select_different_registers(Register preserve, Register extra,
                                       Register &tmp1, Register &tmp2,
                                       Register &tmp3) {
  if (tmp1 == preserve) {
    assert_different_registers(tmp1, tmp2, tmp3, extra);
    tmp1 = extra;
  } else if (tmp2 == preserve) {
    assert_different_registers(tmp1, tmp2, tmp3, extra);
    tmp2 = extra;
  } else if (tmp3 == preserve) {
    assert_different_registers(tmp1, tmp2, tmp3, extra);
    tmp3 = extra;
  }
  assert_different_registers(preserve, tmp1, tmp2, tmp3);
}

bool LIR_Assembler::is_small_constant(LIR_Opr opr) { Unimplemented(); return false; }

LIR_Opr LIR_Assembler::receiverOpr() {
  return FrameMap::receiver_opr;
}

LIR_Opr LIR_Assembler::osrBufferPointer() {
  return FrameMap::as_pointer_opr(receiverOpr()->as_register());
}

//--------------fpu register translations-----------------------

address LIR_Assembler::float_constant(float f) {
  address const_addr = __ float_constant(f);
  if (const_addr == NULL) {
    bailout("const section overflow");
    return __ code()->consts()->start();
  } else {
    return const_addr;
  }
}

address LIR_Assembler::double_constant(double d) {
  address const_addr = __ double_constant(d);
  if (const_addr == NULL) {
    bailout("const section overflow");
    return __ code()->consts()->start();
  } else {
    return const_addr;
  }
}

void LIR_Assembler::set_24bit_FPU() { Unimplemented(); }

void LIR_Assembler::reset_FPU() { Unimplemented(); }

void LIR_Assembler::fpop() { Unimplemented(); }

void LIR_Assembler::fxch(int i) { Unimplemented(); }

void LIR_Assembler::fld(int i) { Unimplemented(); }

void LIR_Assembler::ffree(int i) { Unimplemented(); }

void LIR_Assembler::breakpoint() { Unimplemented(); }

void LIR_Assembler::push(LIR_Opr opr) { Unimplemented(); }

void LIR_Assembler::pop(LIR_Opr opr) { Unimplemented(); }

bool LIR_Assembler::is_literal_address(LIR_Address* addr) { Unimplemented(); return false; }

static Register as_reg(LIR_Opr op) {
  return op->is_double_cpu() ? op->as_register_lo() : op->as_register();
}

static jlong as_long(LIR_Opr data) {
  jlong result;
  switch (data->type()) {
  case T_INT:
    result = (data->as_jint());
    break;
  case T_LONG:
    result = (data->as_jlong());
    break;
  default:
    ShouldNotReachHere();
    result = 0; // unreachable
  }
  return result;
}

Address LIR_Assembler::as_Address(LIR_Address* addr) {
  Register base = addr->base()->as_pointer_register();
  LIR_Opr opr = addr->index();
  if (opr->is_cpu_register()) {
    Register index;
    if (opr->is_single_cpu())
      index = opr->as_register();
    else
      index = opr->as_register_lo();
    assert(addr->disp() == 0, "must be");
    return Address(base, index, Address::ScaleFactor(addr->scale()));
  } else {
    assert(addr->scale() == 0, "must be");
    return Address(base, addr->disp());
  }
  return Address();
}

Address LIR_Assembler::as_Address_hi(LIR_Address* addr) {
  ShouldNotReachHere();
  return Address();
}

Address LIR_Assembler::as_Address_lo(LIR_Address* addr) {
  return as_Address(addr); // Ouch
  // FIXME: This needs to be much more clever. See x86.
}

// Ensure a valid Address (base + offset) to a stack-slot. If stack access is
// not encodable as a base + (immediate) offset, generate an explicit address
// calculation to hold the address in a temporary register.
Address LIR_Assembler::stack_slot_address(int index, uint size, int adjust) {
  precond(size == 4 || size == 8);
  Address addr = frame_map()->address_for_slot(index, adjust);
  precond(addr.index() == noreg);
  precond(addr.base() == SP);
  precond(addr.disp() > 0);
  uint mask = size - 1;
  assert((addr.disp() & mask) == 0, "scaled offsets only");
  return addr;
}

void LIR_Assembler::osr_entry() {
  offsets()->set_value(CodeOffsets::OSR_Entry, code_offset());
  BlockBegin* osr_entry = compilation()->hir()->osr_entry();
  ValueStack* entry_state = osr_entry->state();
  int number_of_locks = entry_state->locks_size();

  // we jump here if osr happens with the interpreter
  // state set up to continue at the beginning of the
  // loop that triggered osr - in particular, we have
  // the following registers setup:
  //
  // A2: osr buffer
  //

  // build frame
  ciMethod* m = compilation()->method();
  __ build_frame(initial_frame_size_in_bytes(), bang_size_in_bytes());

  // OSR buffer is
  //
  // locals[nlocals-1..0]
  // monitors[0..number_of_locks]
  //
  // locals is a direct copy of the interpreter frame so in the osr buffer
  // so first slot in the local array is the last local from the interpreter
  // and last slot is local[0] (receiver) from the interpreter
  //
  // Similarly with locks. The first lock slot in the osr buffer is the nth lock
  // from the interpreter frame, the nth lock slot in the osr buffer is 0th lock
  // in the interpreter frame (the method lock if a sync method)

  // Initialize monitors in the compiled activation.
  //   A2: pointer to osr buffer
  //
  // All other registers are dead at this point and the locals will be
  // copied into place by code emitted in the IR.

  Register OSR_buf = osrBufferPointer()->as_pointer_register();
  {
    assert(frame::interpreter_frame_monitor_size() == BasicObjectLock::size(), "adjust code below");
    int monitor_offset = BytesPerWord * method()->max_locals() + (2 * BytesPerWord) * (number_of_locks - 1);
    // SharedRuntime::OSR_migration_begin() packs BasicObjectLocks in
    // the OSR buffer using 2 word entries: first the lock and then
    // the oop.
    for (int i = 0; i < number_of_locks; i++) {
      int slot_offset = monitor_offset - ((i * 2) * BytesPerWord);
#ifdef ASSERT
      // verify the interpreter's monitor has a non-null object
      {
        Label L;
        __ ld_ptr(SCR1, Address(OSR_buf, slot_offset + 1 * BytesPerWord));
        __ bnez(SCR1, L);
        __ stop("locked object is NULL");
        __ bind(L);
      }
#endif
      __ ld_ptr(S0, Address(OSR_buf, slot_offset + 0));
      __ st_ptr(S0, frame_map()->address_for_monitor_lock(i));
      __ ld_ptr(S0, Address(OSR_buf, slot_offset + 1*BytesPerWord));
      __ st_ptr(S0, frame_map()->address_for_monitor_object(i));
    }
  }
}

// inline cache check; done before the frame is built.
int LIR_Assembler::check_icache() {
  Register receiver = FrameMap::receiver_opr->as_register();
  Register ic_klass = IC_Klass;
  int start_offset = __ offset();
  Label dont;

  __ verify_oop(receiver);

  // explicit NULL check not needed since load from [klass_offset] causes a trap
  // check against inline cache
  assert(!MacroAssembler::needs_explicit_null_check(oopDesc::klass_offset_in_bytes()),
         "must add explicit null check");

  __ load_klass(SCR2, receiver);
  __ beq(SCR2, ic_klass, dont);

  // if icache check fails, then jump to runtime routine
  // Note: RECEIVER must still contain the receiver!
  __ jmp(SharedRuntime::get_ic_miss_stub(), relocInfo::runtime_call_type);

  // We align the verified entry point unless the method body
  // (including its inline cache check) will fit in a single 64-byte
  // icache line.
  if (!method()->is_accessor() || __ offset() - start_offset > 4 * 4) {
    // force alignment after the cache check.
    __ align(CodeEntryAlignment);
  }

  __ bind(dont);
  return start_offset;
}

void LIR_Assembler::jobject2reg(jobject o, Register reg) {
  if (o == NULL) {
    __ move(reg, R0);
  } else {
    int oop_index = __ oop_recorder()->find_index(o);
    RelocationHolder rspec = oop_Relocation::spec(oop_index);
    __ relocate(rspec);
    __ patchable_li52(reg, (long)o);
  }
}

void LIR_Assembler::deoptimize_trap(CodeEmitInfo *info) {
  address target = NULL;

  switch (patching_id(info)) {
  case PatchingStub::access_field_id:
    target = Runtime1::entry_for(Runtime1::access_field_patching_id);
    break;
  case PatchingStub::load_klass_id:
    target = Runtime1::entry_for(Runtime1::load_klass_patching_id);
    break;
  case PatchingStub::load_mirror_id:
    target = Runtime1::entry_for(Runtime1::load_mirror_patching_id);
    break;
  case PatchingStub::load_appendix_id:
    target = Runtime1::entry_for(Runtime1::load_appendix_patching_id);
    break;
  default: ShouldNotReachHere();
  }

  __ call(target, relocInfo::runtime_call_type);
  add_call_info_here(info);
}

void LIR_Assembler::jobject2reg_with_patching(Register reg, CodeEmitInfo *info) {
  deoptimize_trap(info);
}

// This specifies the rsp decrement needed to build the frame
int LIR_Assembler::initial_frame_size_in_bytes() const {
  // if rounding, must let FrameMap know!
  return in_bytes(frame_map()->framesize_in_bytes());
}

int LIR_Assembler::emit_exception_handler() {
  // if the last instruction is a call (typically to do a throw which
  // is coming at the end after block reordering) the return address
  // must still point into the code area in order to avoid assertion
  // failures when searching for the corresponding bci => add a nop
  // (was bug 5/14/1999 - gri)
  __ nop();

  // generate code for exception handler
  address handler_base = __ start_a_stub(exception_handler_size);
  if (handler_base == NULL) {
    // not enough space left for the handler
    bailout("exception handler overflow");
    return -1;
  }

  int offset = code_offset();

  // the exception oop and pc are in A0, and A1
  // no other registers need to be preserved, so invalidate them
  __ invalidate_registers(false, true, true, true, true, true);

  // check that there is really an exception
  __ verify_not_null_oop(A0);

  // search an exception handler (A0: exception oop, A1: throwing pc)
  __ call(Runtime1::entry_for(Runtime1::handle_exception_from_callee_id), relocInfo::runtime_call_type);
  __ should_not_reach_here();
  guarantee(code_offset() - offset <= exception_handler_size, "overflow");
  __ end_a_stub();

  return offset;
}

// Emit the code to remove the frame from the stack in the exception unwind path.
int LIR_Assembler::emit_unwind_handler() {
#ifndef PRODUCT
  if (CommentedAssembly) {
    _masm->block_comment("Unwind handler");
  }
#endif

  int offset = code_offset();

  // Fetch the exception from TLS and clear out exception related thread state
  __ ld_ptr(A0, Address(TREG, JavaThread::exception_oop_offset()));
  __ st_ptr(R0, Address(TREG, JavaThread::exception_oop_offset()));
  __ st_ptr(R0, Address(TREG, JavaThread::exception_pc_offset()));

  __ bind(_unwind_handler_entry);
  __ verify_not_null_oop(V0);
  if (method()->is_synchronized() || compilation()->env()->dtrace_method_probes()) {
    __ move(S0, V0);  // Preserve the exception
  }

  // Perform needed unlocking
  MonitorExitStub* stub = NULL;
  if (method()->is_synchronized()) {
    monitor_address(0, FrameMap::a0_opr);
    stub = new MonitorExitStub(FrameMap::a0_opr, true, 0);
    __ unlock_object(A5, A4, A0, *stub->entry());
    __ bind(*stub->continuation());
  }

  if (compilation()->env()->dtrace_method_probes()) {
    __ mov_metadata(A1, method()->constant_encoding());
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_exit), TREG, A1);
  }

  if (method()->is_synchronized() || compilation()->env()->dtrace_method_probes()) {
    __ move(A0, S0);  // Restore the exception
  }

  // remove the activation and dispatch to the unwind handler
  __ block_comment("remove_frame and dispatch to the unwind handler");
  __ remove_frame(initial_frame_size_in_bytes());
  __ jmp(Runtime1::entry_for(Runtime1::unwind_exception_id), relocInfo::runtime_call_type);

  // Emit the slow path assembly
  if (stub != NULL) {
    stub->emit_code(this);
  }

  return offset;
}

int LIR_Assembler::emit_deopt_handler() {
  // if the last instruction is a call (typically to do a throw which
  // is coming at the end after block reordering) the return address
  // must still point into the code area in order to avoid assertion
  // failures when searching for the corresponding bci => add a nop
  // (was bug 5/14/1999 - gri)
  __ nop();

  // generate code for exception handler
  address handler_base = __ start_a_stub(deopt_handler_size);
  if (handler_base == NULL) {
    // not enough space left for the handler
    bailout("deopt handler overflow");
    return -1;
  }

  int offset = code_offset();

  __ call(SharedRuntime::deopt_blob()->unpack(), relocInfo::runtime_call_type);
  guarantee(code_offset() - offset <= deopt_handler_size, "overflow");
  __ end_a_stub();

  return offset;
}

void LIR_Assembler::add_debug_info_for_branch(address adr, CodeEmitInfo* info) {
  _masm->code_section()->relocate(adr, relocInfo::poll_type);
  int pc_offset = code_offset();
  flush_debug_info(pc_offset);
  info->record_debug_info(compilation()->debug_info_recorder(), pc_offset);
  if (info->exception_handlers() != NULL) {
    compilation()->add_exception_handlers_for_pco(pc_offset, info->exception_handlers());
  }
}

void LIR_Assembler::return_op(LIR_Opr result) {
  assert(result->is_illegal() || !result->is_single_cpu() || result->as_register() == V0,
         "word returns are in V0,");

  // Pop the stack before the safepoint code
  __ remove_frame(initial_frame_size_in_bytes());

  __ li(SCR2, os::get_polling_page());
  __ relocate(relocInfo::poll_return_type);
  __ ld_w(SCR1, SCR2, 0);
  __ jr(RA);
}

int LIR_Assembler::safepoint_poll(LIR_Opr tmp, CodeEmitInfo* info) {
  guarantee(info != NULL, "Shouldn't be NULL");
  __ li(SCR2, os::get_polling_page());
  add_debug_info_for_branch(info); // This isn't just debug info: it's the oop map
  __ relocate(relocInfo::poll_type);
  __ ld_w(SCR1, SCR2, 0);
  return __ offset();
}

void LIR_Assembler::move_regs(Register from_reg, Register to_reg) {
  __ move(to_reg, from_reg);
}

void LIR_Assembler::swap_reg(Register a, Register b) { Unimplemented(); }

void LIR_Assembler::const2reg(LIR_Opr src, LIR_Opr dest, LIR_PatchCode patch_code, CodeEmitInfo* info) {
  assert(src->is_constant(), "should not call otherwise");
  assert(dest->is_register(), "should not call otherwise");
  LIR_Const* c = src->as_constant_ptr();

  switch (c->type()) {
    case T_INT:
      assert(patch_code == lir_patch_none, "no patching handled here");
      __ li(dest->as_register(), c->as_jint());
      break;
    case T_ADDRESS:
      assert(patch_code == lir_patch_none, "no patching handled here");
      __ li(dest->as_register(), c->as_jint());
      break;
    case T_LONG:
      assert(patch_code == lir_patch_none, "no patching handled here");
      __ li(dest->as_register_lo(), (intptr_t)c->as_jlong());
      break;
    case T_OBJECT:
      if (patch_code == lir_patch_none) {
        jobject2reg(c->as_jobject(), dest->as_register());
      } else {
        jobject2reg_with_patching(dest->as_register(), info);
      }
      break;
    case T_METADATA:
      if (patch_code != lir_patch_none) {
        klass2reg_with_patching(dest->as_register(), info);
      } else {
        __ mov_metadata(dest->as_register(), c->as_metadata());
      }
      break;
    case T_FLOAT:
      __ relocate(relocInfo::internal_word_type);
      __ patchable_li52(SCR1, (jlong) float_constant(c->as_jfloat()));
      __ fld_s(dest->as_float_reg(), SCR1, 0);
      break;
    case T_DOUBLE:
      __ relocate(relocInfo::internal_word_type);
      __ patchable_li52(SCR1, (jlong) double_constant(c->as_jdouble()));
      __ fld_d(dest->as_double_reg(), SCR1, 0);
      break;
    default:
      ShouldNotReachHere();
  }
}

void LIR_Assembler::const2stack(LIR_Opr src, LIR_Opr dest) {
  LIR_Const* c = src->as_constant_ptr();
  switch (c->type()) {
  case T_OBJECT:
    if (!c->as_jobject())
      __ st_ptr(R0, frame_map()->address_for_slot(dest->single_stack_ix()));
    else {
      const2reg(src, FrameMap::scr1_opr, lir_patch_none, NULL);
      reg2stack(FrameMap::scr1_opr, dest, c->type(), false);
    }
    break;
  case T_ADDRESS:
    const2reg(src, FrameMap::scr1_opr, lir_patch_none, NULL);
    reg2stack(FrameMap::scr1_opr, dest, c->type(), false);
  case T_INT:
  case T_FLOAT:
    if (c->as_jint_bits() == 0)
      __ st_w(R0, frame_map()->address_for_slot(dest->single_stack_ix()));
    else {
      __ li(SCR2, c->as_jint_bits());
      __ st_w(SCR2, frame_map()->address_for_slot(dest->single_stack_ix()));
    }
    break;
  case T_LONG:
  case T_DOUBLE:
    if (c->as_jlong_bits() == 0)
      __ st_ptr(R0, frame_map()->address_for_slot(dest->double_stack_ix(),
                lo_word_offset_in_bytes));
    else {
      __ li(SCR2, (intptr_t)c->as_jlong_bits());
      __ st_ptr(SCR2, frame_map()->address_for_slot(dest->double_stack_ix(),
                lo_word_offset_in_bytes));
    }
    break;
  default:
    ShouldNotReachHere();
  }
}

void LIR_Assembler::const2mem(LIR_Opr src, LIR_Opr dest, BasicType type,
                              CodeEmitInfo* info, bool wide) {
  assert(src->is_constant(), "should not call otherwise");
  LIR_Const* c = src->as_constant_ptr();
  LIR_Address* to_addr = dest->as_address_ptr();

  void (Assembler::* insn)(Register Rt, Address adr);

  switch (type) {
  case T_ADDRESS:
    assert(c->as_jint() == 0, "should be");
    insn = &Assembler::st_d;
    break;
  case T_LONG:
    assert(c->as_jlong() == 0, "should be");
    insn = &Assembler::st_d;
    break;
  case T_INT:
    assert(c->as_jint() == 0, "should be");
    insn = &Assembler::st_w;
    break;
  case T_OBJECT:
  case T_ARRAY:
    assert(c->as_jobject() == 0, "should be");
    if (UseCompressedOops && !wide) {
      insn = &Assembler::st_w;
    } else {
      insn = &Assembler::st_d;
    }
    break;
  case T_CHAR:
  case T_SHORT:
    assert(c->as_jint() == 0, "should be");
    insn = &Assembler::st_h;
    break;
  case T_BOOLEAN:
  case T_BYTE:
    assert(c->as_jint() == 0, "should be");
    insn = &Assembler::st_b;
    break;
  default:
    ShouldNotReachHere();
    insn = &Assembler::st_d;  // unreachable
  }

  if (info) add_debug_info_for_null_check_here(info);
  (_masm->*insn)(R0, as_Address(to_addr));
}

void LIR_Assembler::reg2reg(LIR_Opr src, LIR_Opr dest) {
  assert(src->is_register(), "should not call otherwise");
  assert(dest->is_register(), "should not call otherwise");

  // move between cpu-registers
  if (dest->is_single_cpu()) {
    if (src->type() == T_LONG) {
      // Can do LONG -> OBJECT
      move_regs(src->as_register_lo(), dest->as_register());
      return;
    }
    assert(src->is_single_cpu(), "must match");
    if (src->type() == T_OBJECT) {
      __ verify_oop(src->as_register());
    }
    move_regs(src->as_register(), dest->as_register());
  } else if (dest->is_double_cpu()) {
    if (is_reference_type(src->type())) {
      // Surprising to me but we can see move of a long to t_object
      __ verify_oop(src->as_register());
      move_regs(src->as_register(), dest->as_register_lo());
      return;
    }
    assert(src->is_double_cpu(), "must match");
    Register f_lo = src->as_register_lo();
    Register f_hi = src->as_register_hi();
    Register t_lo = dest->as_register_lo();
    Register t_hi = dest->as_register_hi();
    assert(f_hi == f_lo, "must be same");
    assert(t_hi == t_lo, "must be same");
    move_regs(f_lo, t_lo);
  } else if (dest->is_single_fpu()) {
    __ fmov_s(dest->as_float_reg(), src->as_float_reg());
  } else if (dest->is_double_fpu()) {
    __ fmov_d(dest->as_double_reg(), src->as_double_reg());
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::reg2stack(LIR_Opr src, LIR_Opr dest, BasicType type, bool pop_fpu_stack) {
  precond(src->is_register() && dest->is_stack());

  uint const c_sz32 = sizeof(uint32_t);
  uint const c_sz64 = sizeof(uint64_t);

  if (src->is_single_cpu()) {
    int index = dest->single_stack_ix();
    if (is_reference_type(type)) {
      __ st_ptr(src->as_register(), stack_slot_address(index, c_sz64));
      __ verify_oop(src->as_register());
    } else if (type == T_METADATA || type == T_DOUBLE || type == T_ADDRESS) {
      __ st_ptr(src->as_register(), stack_slot_address(index, c_sz64));
    } else {
      __ st_w(src->as_register(), stack_slot_address(index, c_sz32));
    }
  } else if (src->is_double_cpu()) {
    int index = dest->double_stack_ix();
    Address dest_addr_LO = stack_slot_address(index, c_sz64, lo_word_offset_in_bytes);
    __ st_ptr(src->as_register_lo(), dest_addr_LO);
  } else if (src->is_single_fpu()) {
    int index = dest->single_stack_ix();
    __ fst_s(src->as_float_reg(), stack_slot_address(index, c_sz32));
  } else if (src->is_double_fpu()) {
    int index = dest->double_stack_ix();
    __ fst_d(src->as_double_reg(), stack_slot_address(index, c_sz64));
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::reg2mem(LIR_Opr src, LIR_Opr dest, BasicType type, LIR_PatchCode patch_code,
                            CodeEmitInfo* info, bool pop_fpu_stack, bool wide, bool /* unaligned */) {
  LIR_Address* to_addr = dest->as_address_ptr();
  PatchingStub* patch = NULL;
  Register compressed_src = SCR2;

  if (patch_code != lir_patch_none) {
    deoptimize_trap(info);
    return;
  }

  if (is_reference_type(type)) {
    __ verify_oop(src->as_register());

    if (UseCompressedOops && !wide) {
      __ encode_heap_oop(compressed_src, src->as_register());
    } else {
      compressed_src = src->as_register();
    }
  }

  int null_check_here = code_offset();
  switch (type) {
    case T_FLOAT:
      __ fst_s(src->as_float_reg(), as_Address(to_addr));
      break;
    case T_DOUBLE:
      __ fst_d(src->as_double_reg(), as_Address(to_addr));
      break;
    case T_ARRAY:  // fall through
    case T_OBJECT: // fall through
      if (UseCompressedOops && !wide) {
        __ st_w(compressed_src, as_Address(to_addr));
      } else {
         __ st_ptr(compressed_src, as_Address(to_addr));
      }
      break;
    case T_METADATA:
      // We get here to store a method pointer to the stack to pass to
      // a dtrace runtime call. This can't work on 64 bit with
      // compressed klass ptrs: T_METADATA can be a compressed klass
      // ptr or a 64 bit method pointer.
      ShouldNotReachHere();
      __ st_ptr(src->as_register(), as_Address(to_addr));
      break;
    case T_ADDRESS:
      __ st_ptr(src->as_register(), as_Address(to_addr));
      break;
    case T_INT:
      __ st_w(src->as_register(), as_Address(to_addr));
      break;
    case T_LONG:
      __ st_ptr(src->as_register_lo(), as_Address_lo(to_addr));
      break;
    case T_BYTE: // fall through
    case T_BOOLEAN:
      __ st_b(src->as_register(), as_Address(to_addr));
      break;
    case T_CHAR: // fall through
    case T_SHORT:
      __ st_h(src->as_register(), as_Address(to_addr));
      break;
    default:
      ShouldNotReachHere();
  }
  if (info != NULL) {
    add_debug_info_for_null_check(null_check_here, info);
  }
}

void LIR_Assembler::stack2reg(LIR_Opr src, LIR_Opr dest, BasicType type) {
  precond(src->is_stack() && dest->is_register());

  uint const c_sz32 = sizeof(uint32_t);
  uint const c_sz64 = sizeof(uint64_t);

  if (dest->is_single_cpu()) {
    int index = src->single_stack_ix();
    if (is_reference_type(type)) {
      __ ld_ptr(dest->as_register(), stack_slot_address(index, c_sz64));
      __ verify_oop(dest->as_register());
    } else if (type == T_METADATA || type == T_ADDRESS) {
      __ ld_ptr(dest->as_register(), stack_slot_address(index, c_sz64));
    } else {
      __ ld_w(dest->as_register(), stack_slot_address(index, c_sz32));
    }
  } else if (dest->is_double_cpu()) {
    int index = src->double_stack_ix();
    Address src_addr_LO = stack_slot_address(index, c_sz64, lo_word_offset_in_bytes);
    __ ld_ptr(dest->as_register_lo(), src_addr_LO);
  } else if (dest->is_single_fpu()) {
    int index = src->single_stack_ix();
    __ fld_s(dest->as_float_reg(), stack_slot_address(index, c_sz32));
  } else if (dest->is_double_fpu()) {
    int index = src->double_stack_ix();
    __ fld_d(dest->as_double_reg(), stack_slot_address(index, c_sz64));
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::klass2reg_with_patching(Register reg, CodeEmitInfo* info) {
  address target = NULL;

  switch (patching_id(info)) {
  case PatchingStub::access_field_id:
    target = Runtime1::entry_for(Runtime1::access_field_patching_id);
    break;
  case PatchingStub::load_klass_id:
    target = Runtime1::entry_for(Runtime1::load_klass_patching_id);
    break;
  case PatchingStub::load_mirror_id:
    target = Runtime1::entry_for(Runtime1::load_mirror_patching_id);
    break;
  case PatchingStub::load_appendix_id:
    target = Runtime1::entry_for(Runtime1::load_appendix_patching_id);
    break;
  default: ShouldNotReachHere();
  }

  __ call(target, relocInfo::runtime_call_type);
  add_call_info_here(info);
}

void LIR_Assembler::stack2stack(LIR_Opr src, LIR_Opr dest, BasicType type) {
  LIR_Opr temp;

  if (type == T_LONG || type == T_DOUBLE)
    temp = FrameMap::scr1_long_opr;
  else
    temp = FrameMap::scr1_opr;

  stack2reg(src, temp, src->type());
  reg2stack(temp, dest, dest->type(), false);
}

void LIR_Assembler::mem2reg(LIR_Opr src, LIR_Opr dest, BasicType type, LIR_PatchCode patch_code, CodeEmitInfo* info, bool wide, bool /* unaligned */) {
  LIR_Address* addr = src->as_address_ptr();
  LIR_Address* from_addr = src->as_address_ptr();

  if (addr->base()->type() == T_OBJECT) {
    __ verify_oop(addr->base()->as_pointer_register());
  }

  if (patch_code != lir_patch_none) {
    deoptimize_trap(info);
    return;
  }

  if (info != NULL) {
    add_debug_info_for_null_check_here(info);
  }
  int null_check_here = code_offset();
  switch (type) {
    case T_FLOAT:
      __ fld_s(dest->as_float_reg(), as_Address(from_addr));
      break;
    case T_DOUBLE:
      __ fld_d(dest->as_double_reg(), as_Address(from_addr));
      break;
    case T_ARRAY:  // fall through
    case T_OBJECT: // fall through
      if (UseCompressedOops && !wide) {
        __ ld_wu(dest->as_register(), as_Address(from_addr));
      } else {
         __ ld_ptr(dest->as_register(), as_Address(from_addr));
      }
      break;
    case T_METADATA:
      // We get here to store a method pointer to the stack to pass to
      // a dtrace runtime call. This can't work on 64 bit with
      // compressed klass ptrs: T_METADATA can be a compressed klass
      // ptr or a 64 bit method pointer.
      ShouldNotReachHere();
      __ ld_ptr(dest->as_register(), as_Address(from_addr));
      break;
    case T_ADDRESS:
      // FIXME: OMG this is a horrible kludge.  Any offset from an
      // address that matches klass_offset_in_bytes() will be loaded
      // as a word, not a long.
      if (UseCompressedClassPointers && addr->disp() == oopDesc::klass_offset_in_bytes()) {
        __ ld_wu(dest->as_register(), as_Address(from_addr));
      } else {
        __ ld_ptr(dest->as_register(), as_Address(from_addr));
      }
      break;
    case T_INT:
      __ ld_w(dest->as_register(), as_Address(from_addr));
      break;
    case T_LONG:
      __ ld_ptr(dest->as_register_lo(), as_Address_lo(from_addr));
      break;
    case T_BYTE:
      __ ld_b(dest->as_register(), as_Address(from_addr));
      break;
    case T_BOOLEAN:
      __ ld_bu(dest->as_register(), as_Address(from_addr));
      break;
    case T_CHAR:
      __ ld_hu(dest->as_register(), as_Address(from_addr));
      break;
    case T_SHORT:
      __ ld_h(dest->as_register(), as_Address(from_addr));
      break;
    default:
      ShouldNotReachHere();
  }

  if (is_reference_type(type)) {
    if (UseCompressedOops && !wide) {
      __ decode_heap_oop(dest->as_register());
    }

    // Load barrier has not yet been applied, so ZGC can't verify the oop here
    __ verify_oop(dest->as_register());
  } else if (type == T_ADDRESS && addr->disp() == oopDesc::klass_offset_in_bytes()) {
    if (UseCompressedClassPointers) {
      __ decode_klass_not_null(dest->as_register());
    }
  }
}

void LIR_Assembler::prefetchr(LIR_Opr src) { Unimplemented(); }

void LIR_Assembler::prefetchw(LIR_Opr src) { Unimplemented(); }

int LIR_Assembler::array_element_size(BasicType type) const {
  int elem_size = type2aelembytes(type);
  return exact_log2(elem_size);
}

void LIR_Assembler::emit_op3(LIR_Op3* op) {
  switch (op->code()) {
  case lir_idiv:
  case lir_irem:
    arithmetic_idiv(op->code(), op->in_opr1(), op->in_opr2(), op->in_opr3(),
                    op->result_opr(), op->info());
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}

void LIR_Assembler::emit_opBranch(LIR_OpBranch* op) {
#ifdef ASSERT
  assert(op->block() == NULL || op->block()->label() == op->label(), "wrong label");
  if (op->block() != NULL)  _branch_target_blocks.append(op->block());
  assert(op->cond() == lir_cond_always, "must be");
#endif

  if (op->info() != NULL)
    add_debug_info_for_branch(op->info());

  __ b_far(*(op->label()));
}

void LIR_Assembler::emit_opCmpBranch(LIR_OpCmpBranch* op) {
#ifdef ASSERT
  assert(op->block() == NULL || op->block()->label() == op->label(), "wrong label");
  if (op->block() != NULL)  _branch_target_blocks.append(op->block());
  if (op->ublock() != NULL) _branch_target_blocks.append(op->ublock());
#endif

  if (op->info() != NULL) {
    assert(op->in_opr1()->is_address() || op->in_opr2()->is_address(),
           "shouldn't be codeemitinfo for non-address operands");
    add_debug_info_for_null_check_here(op->info()); // exception possible
  }

  Label& L = *(op->label());
  Assembler::Condition acond;
  LIR_Opr opr1 = op->in_opr1();
  LIR_Opr opr2 = op->in_opr2();
  assert(op->condition() != lir_cond_always, "must be");

  if (op->code() == lir_cmp_float_branch) {
    bool is_unordered = (op->ublock() == op->block());
    if (opr1->is_single_fpu()) {
      FloatRegister reg1 = opr1->as_float_reg();
      assert(opr2->is_single_fpu(), "expect single float register");
      FloatRegister reg2 = opr2->as_float_reg();
      switch(op->condition()) {
      case lir_cond_equal:
        if (is_unordered)
          __ fcmp_cueq_s(FCC0, reg1, reg2);
        else
          __ fcmp_ceq_s(FCC0, reg1, reg2);
        break;
      case lir_cond_notEqual:
        if (is_unordered)
          __ fcmp_cune_s(FCC0, reg1, reg2);
        else
          __ fcmp_cne_s(FCC0, reg1, reg2);
        break;
      case lir_cond_less:
        if (is_unordered)
          __ fcmp_cult_s(FCC0, reg1, reg2);
        else
          __ fcmp_clt_s(FCC0, reg1, reg2);
        break;
      case lir_cond_lessEqual:
        if (is_unordered)
          __ fcmp_cule_s(FCC0, reg1, reg2);
        else
          __ fcmp_cle_s(FCC0, reg1, reg2);
        break;
      case lir_cond_greaterEqual:
        if (is_unordered)
          __ fcmp_cule_s(FCC0, reg2, reg1);
        else
          __ fcmp_cle_s(FCC0, reg2, reg1);
        break;
      case lir_cond_greater:
        if (is_unordered)
          __ fcmp_cult_s(FCC0, reg2, reg1);
        else
          __ fcmp_clt_s(FCC0, reg2, reg1);
        break;
      default:
        ShouldNotReachHere();
      }
    } else if (opr1->is_double_fpu()) {
      FloatRegister reg1 = opr1->as_double_reg();
      assert(opr2->is_double_fpu(), "expect double float register");
      FloatRegister reg2 = opr2->as_double_reg();
      switch(op->condition()) {
      case lir_cond_equal:
        if (is_unordered)
          __ fcmp_cueq_d(FCC0, reg1, reg2);
        else
          __ fcmp_ceq_d(FCC0, reg1, reg2);
        break;
      case lir_cond_notEqual:
        if (is_unordered)
          __ fcmp_cune_d(FCC0, reg1, reg2);
        else
          __ fcmp_cne_d(FCC0, reg1, reg2);
        break;
      case lir_cond_less:
        if (is_unordered)
          __ fcmp_cult_d(FCC0, reg1, reg2);
        else
          __ fcmp_clt_d(FCC0, reg1, reg2);
        break;
      case lir_cond_lessEqual:
        if (is_unordered)
          __ fcmp_cule_d(FCC0, reg1, reg2);
        else
          __ fcmp_cle_d(FCC0, reg1, reg2);
        break;
      case lir_cond_greaterEqual:
        if (is_unordered)
          __ fcmp_cule_d(FCC0, reg2, reg1);
        else
          __ fcmp_cle_d(FCC0, reg2, reg1);
        break;
      case lir_cond_greater:
        if (is_unordered)
          __ fcmp_cult_d(FCC0, reg2, reg1);
        else
          __ fcmp_clt_d(FCC0, reg2, reg1);
        break;
      default:
        ShouldNotReachHere();
      }
    } else {
      ShouldNotReachHere();
    }
    __ bcnez(FCC0, L);
  } else {
    if (opr1->is_constant() && opr2->is_single_cpu()) {
      // tableswitch
      Unimplemented();
    } else if (opr1->is_single_cpu() || opr1->is_double_cpu()) {
      Register reg1 = as_reg(opr1);
      Register reg2 = noreg;
      jlong imm2 = 0;
      if (opr2->is_single_cpu()) {
        // cpu register - cpu register
        reg2 = opr2->as_register();
      } else if (opr2->is_double_cpu()) {
        // cpu register - cpu register
        reg2 = opr2->as_register_lo();
      } else if (opr2->is_constant()) {
        switch(opr2->type()) {
        case T_INT:
        case T_ADDRESS:
          imm2 = opr2->as_constant_ptr()->as_jint();
          break;
        case T_LONG:
          imm2 = opr2->as_constant_ptr()->as_jlong();
          break;
        case T_METADATA:
          imm2 = (intptr_t)opr2->as_constant_ptr()->as_metadata();
          break;
        case T_OBJECT:
        case T_ARRAY:
          if (opr2->as_constant_ptr()->as_jobject() != NULL) {
            reg2 = SCR1;
            jobject2reg(opr2->as_constant_ptr()->as_jobject(), reg2);
          } else {
            reg2 = R0;
          }
          break;
        default:
          ShouldNotReachHere();
          break;
        }
      } else {
        ShouldNotReachHere();
      }
      if (reg2 == noreg) {
        if (imm2 == 0) {
          reg2 = R0;
        } else {
          reg2 = SCR1;
          __ li(reg2, imm2);
        }
      }
      switch (op->condition()) {
        case lir_cond_equal:
          __ beq_far(reg1, reg2, L); break;
        case lir_cond_notEqual:
          __ bne_far(reg1, reg2, L); break;
        case lir_cond_less:
          __ blt_far(reg1, reg2, L, true); break;
        case lir_cond_lessEqual:
          __ bge_far(reg2, reg1, L, true); break;
        case lir_cond_greaterEqual:
          __ bge_far(reg1, reg2, L, true); break;
        case lir_cond_greater:
          __ blt_far(reg2, reg1, L, true); break;
        case lir_cond_belowEqual:
          __ bge_far(reg2, reg1, L, false); break;
        case lir_cond_aboveEqual:
          __ bge_far(reg1, reg2, L, false); break;
        default:
          ShouldNotReachHere();
      }
    }
  }
}

void LIR_Assembler::emit_opConvert(LIR_OpConvert* op) {
  LIR_Opr src  = op->in_opr();
  LIR_Opr dest = op->result_opr();
  LIR_Opr tmp  = op->tmp();

  switch (op->bytecode()) {
    case Bytecodes::_i2f:
      __ movgr2fr_w(dest->as_float_reg(), src->as_register());
      __ ffint_s_w(dest->as_float_reg(), dest->as_float_reg());
      break;
    case Bytecodes::_i2d:
      __ movgr2fr_w(dest->as_double_reg(), src->as_register());
      __ ffint_d_w(dest->as_double_reg(), dest->as_double_reg());
      break;
    case Bytecodes::_l2d:
      __ movgr2fr_d(dest->as_double_reg(), src->as_register_lo());
      __ ffint_d_l(dest->as_double_reg(), dest->as_double_reg());
      break;
    case Bytecodes::_l2f:
      __ movgr2fr_d(dest->as_float_reg(), src->as_register_lo());
      __ ffint_s_l(dest->as_float_reg(), dest->as_float_reg());
      break;
    case Bytecodes::_f2d:
      __ fcvt_d_s(dest->as_double_reg(), src->as_float_reg());
      break;
    case Bytecodes::_d2f:
      __ fcvt_s_d(dest->as_float_reg(), src->as_double_reg());
      break;
    case Bytecodes::_i2c:
      __ bstrpick_w(dest->as_register(), src->as_register(), 15, 0);
      break;
    case Bytecodes::_i2l:
      _masm->block_comment("FIXME: This could be a no-op");
      __ slli_w(dest->as_register_lo(), src->as_register(), 0);
      break;
    case Bytecodes::_i2s:
      __ ext_w_h(dest->as_register(), src->as_register());
      break;
    case Bytecodes::_i2b:
      __ ext_w_b(dest->as_register(), src->as_register());
      break;
    case Bytecodes::_l2i:
      __ slli_w(dest->as_register(), src->as_register_lo(), 0);
      break;
    case Bytecodes::_d2l:
      __ ftintrz_l_d(tmp->as_double_reg(), src->as_double_reg());
      __ movfr2gr_d(dest->as_register_lo(), tmp->as_double_reg());
      break;
    case Bytecodes::_f2i:
      __ ftintrz_w_s(tmp->as_float_reg(), src->as_float_reg());
      __ movfr2gr_s(dest->as_register(), tmp->as_float_reg());
      break;
    case Bytecodes::_f2l:
      __ ftintrz_l_s(tmp->as_float_reg(), src->as_float_reg());
      __ movfr2gr_d(dest->as_register_lo(), tmp->as_float_reg());
      break;
    case Bytecodes::_d2i:
      __ ftintrz_w_d(tmp->as_double_reg(), src->as_double_reg());
      __ movfr2gr_s(dest->as_register(), tmp->as_double_reg());
      break;
    default: ShouldNotReachHere();
  }
}

void LIR_Assembler::emit_alloc_obj(LIR_OpAllocObj* op) {
  if (op->init_check()) {
    __ ld_bu(SCR1, Address(op->klass()->as_register(), InstanceKlass::init_state_offset()));
    __ li(SCR2, InstanceKlass::fully_initialized);
    add_debug_info_for_null_check_here(op->stub()->info());
    __ bne_far(SCR1, SCR2, *op->stub()->entry());
  }
  __ allocate_object(op->obj()->as_register(), op->tmp1()->as_register(),
                     op->tmp2()->as_register(), op->header_size(),
                     op->object_size(), op->klass()->as_register(),
                     *op->stub()->entry());
  __ bind(*op->stub()->continuation());
}

void LIR_Assembler::emit_alloc_array(LIR_OpAllocArray* op) {
  Register len =  op->len()->as_register();
  if (UseSlowPath ||
      (!UseFastNewObjectArray && is_reference_type(op->type())) ||
      (!UseFastNewTypeArray   && !is_reference_type(op->type()))) {
    __ b(*op->stub()->entry());
  } else {
    Register tmp1 = op->tmp1()->as_register();
    Register tmp2 = op->tmp2()->as_register();
    Register tmp3 = op->tmp3()->as_register();
    if (len == tmp1) {
      tmp1 = tmp3;
    } else if (len == tmp2) {
      tmp2 = tmp3;
    } else if (len == tmp3) {
      // everything is ok
    } else {
      __ move(tmp3, len);
    }
    __ allocate_array(op->obj()->as_register(), len, tmp1, tmp2,
                      arrayOopDesc::header_size(op->type()),
                      array_element_size(op->type()),
                      op->klass()->as_register(),
                      *op->stub()->entry());
  }
  __ bind(*op->stub()->continuation());
}

void LIR_Assembler::type_profile_helper(Register mdo, ciMethodData *md, ciProfileData *data,
                                        Register recv, Label* update_done) {
  for (uint i = 0; i < ReceiverTypeData::row_limit(); i++) {
    Label next_test;
    // See if the receiver is receiver[n].
    __ lea(SCR2, Address(mdo, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_offset(i))));
    __ ld_ptr(SCR1, Address(SCR2));
    __ bne(recv, SCR1, next_test);
    Address data_addr(mdo, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_count_offset(i)));
    __ ld_ptr(SCR2, data_addr);
    __ addi_d(SCR2, SCR2, DataLayout::counter_increment);
    __ st_ptr(SCR2, data_addr);
    __ b(*update_done);
    __ bind(next_test);
  }

  // Didn't find receiver; find next empty slot and fill it in
  for (uint i = 0; i < ReceiverTypeData::row_limit(); i++) {
    Label next_test;
    __ lea(SCR2, Address(mdo, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_offset(i))));
    Address recv_addr(SCR2);
    __ ld_ptr(SCR1, recv_addr);
    __ bnez(SCR1, next_test);
    __ st_ptr(recv, recv_addr);
    __ li(SCR1, DataLayout::counter_increment);
    __ lea(SCR2, Address(mdo, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_count_offset(i))));
    __ st_ptr(SCR1, Address(SCR2));
    __ b(*update_done);
    __ bind(next_test);
  }
}

void LIR_Assembler::emit_typecheck_helper(LIR_OpTypeCheck *op, Label* success,
                                          Label* failure, Label* obj_is_null) {
  // we always need a stub for the failure case.
  CodeStub* stub = op->stub();
  Register obj = op->object()->as_register();
  Register k_RInfo = op->tmp1()->as_register();
  Register klass_RInfo = op->tmp2()->as_register();
  Register dst = op->result_opr()->as_register();
  ciKlass* k = op->klass();
  Register Rtmp1 = noreg;

  // check if it needs to be profiled
  ciMethodData* md;
  ciProfileData* data;

  const bool should_profile = op->should_profile();

  if (should_profile) {
    ciMethod* method = op->profiled_method();
    assert(method != NULL, "Should have method");
    int bci = op->profiled_bci();
    md = method->method_data_or_null();
    assert(md != NULL, "Sanity");
    data = md->bci_to_data(bci);
    assert(data != NULL, "need data for type check");
    assert(data->is_ReceiverTypeData(), "need ReceiverTypeData for type check");
  }

  Label profile_cast_success, profile_cast_failure;
  Label *success_target = should_profile ? &profile_cast_success : success;
  Label *failure_target = should_profile ? &profile_cast_failure : failure;

  if (obj == k_RInfo) {
    k_RInfo = dst;
  } else if (obj == klass_RInfo) {
    klass_RInfo = dst;
  }
  if (k->is_loaded() && !UseCompressedClassPointers) {
    select_different_registers(obj, dst, k_RInfo, klass_RInfo);
  } else {
    Rtmp1 = op->tmp3()->as_register();
    select_different_registers(obj, dst, k_RInfo, klass_RInfo, Rtmp1);
  }

  assert_different_registers(obj, k_RInfo, klass_RInfo);

  if (should_profile) {
    Label not_null;
    __ bnez(obj, not_null);
    // Object is null; update MDO and exit
    Register mdo = klass_RInfo;
    __ mov_metadata(mdo, md->constant_encoding());
    Address data_addr = Address(mdo, md->byte_offset_of_slot(data, DataLayout::flags_offset()));
    __ ld_bu(SCR2, data_addr);
    __ ori(SCR2, SCR2, BitData::null_seen_byte_constant());
    __ st_b(SCR2, data_addr);
    __ b(*obj_is_null);
    __ bind(not_null);
  } else {
    __ beqz(obj, *obj_is_null);
  }

  if (!k->is_loaded()) {
    klass2reg_with_patching(k_RInfo, op->info_for_patch());
  } else {
    __ mov_metadata(k_RInfo, k->constant_encoding());
  }
  __ verify_oop(obj);

  if (op->fast_check()) {
    // get object class
    // not a safepoint as obj null check happens earlier
    __ load_klass(SCR2, obj);
    __ bne_far(SCR2, k_RInfo, *failure_target);
    // successful cast, fall through to profile or jump
  } else {
    // get object class
    // not a safepoint as obj null check happens earlier
    __ load_klass(klass_RInfo, obj);
    if (k->is_loaded()) {
      // See if we get an immediate positive hit
      __ ld_ptr(SCR1, Address(klass_RInfo, int64_t(k->super_check_offset())));
      if ((juint)in_bytes(Klass::secondary_super_cache_offset()) != k->super_check_offset()) {
        __ bne_far(k_RInfo, SCR1, *failure_target);
        // successful cast, fall through to profile or jump
      } else {
        // See if we get an immediate positive hit
        __ beq_far(k_RInfo, SCR1, *success_target);
        // check for self
        __ beq_far(klass_RInfo, k_RInfo, *success_target);

        __ addi_d(SP, SP, -2 * wordSize);
        __ st_ptr(k_RInfo, Address(SP, 0 * wordSize));
        __ st_ptr(klass_RInfo, Address(SP, 1 * wordSize));
        __ call(Runtime1::entry_for(Runtime1::slow_subtype_check_id), relocInfo::runtime_call_type);
        __ ld_ptr(klass_RInfo, Address(SP, 0 * wordSize));
        __ addi_d(SP, SP, 2 * wordSize);
        // result is a boolean
        __ beqz(klass_RInfo, *failure_target);
        // successful cast, fall through to profile or jump
      }
    } else {
      // perform the fast part of the checking logic
      __ check_klass_subtype_fast_path(klass_RInfo, k_RInfo, Rtmp1, success_target, failure_target, NULL);
      // call out-of-line instance of __ check_klass_subtype_slow_path(...):
      __ addi_d(SP, SP, -2 * wordSize);
      __ st_ptr(k_RInfo, Address(SP, 0 * wordSize));
      __ st_ptr(klass_RInfo, Address(SP, 1 * wordSize));
      __ call(Runtime1::entry_for(Runtime1::slow_subtype_check_id), relocInfo::runtime_call_type);
      __ ld_ptr(k_RInfo, Address(SP, 0 * wordSize));
      __ ld_ptr(klass_RInfo, Address(SP, 1 * wordSize));
      __ addi_d(SP, SP, 2 * wordSize);
      // result is a boolean
      __ beqz(k_RInfo, *failure_target);
      // successful cast, fall through to profile or jump
    }
  }
  if (should_profile) {
    Register mdo = klass_RInfo, recv = k_RInfo;
    __ bind(profile_cast_success);
    __ mov_metadata(mdo, md->constant_encoding());
    __ load_klass(recv, obj);
    Label update_done;
    type_profile_helper(mdo, md, data, recv, success);
    __ b(*success);

    __ bind(profile_cast_failure);
    __ mov_metadata(mdo, md->constant_encoding());
    Address counter_addr = Address(mdo, md->byte_offset_of_slot(data, CounterData::count_offset()));
    __ ld_ptr(SCR2, counter_addr);
    __ addi_d(SCR2, SCR2, -DataLayout::counter_increment);
    __ st_ptr(SCR2, counter_addr);
    __ b(*failure);
  }
  __ b(*success);
}

void LIR_Assembler::emit_opTypeCheck(LIR_OpTypeCheck* op) {
  const bool should_profile = op->should_profile();

  LIR_Code code = op->code();
  if (code == lir_store_check) {
    Register value = op->object()->as_register();
    Register array = op->array()->as_register();
    Register k_RInfo = op->tmp1()->as_register();
    Register klass_RInfo = op->tmp2()->as_register();
    Register Rtmp1 = op->tmp3()->as_register();
    CodeStub* stub = op->stub();

    // check if it needs to be profiled
    ciMethodData* md;
    ciProfileData* data;

    if (should_profile) {
      ciMethod* method = op->profiled_method();
      assert(method != NULL, "Should have method");
      int bci = op->profiled_bci();
      md = method->method_data_or_null();
      assert(md != NULL, "Sanity");
      data = md->bci_to_data(bci);
      assert(data != NULL, "need data for type check");
      assert(data->is_ReceiverTypeData(), "need ReceiverTypeData for type check");
    }
    Label profile_cast_success, profile_cast_failure, done;
    Label *success_target = should_profile ? &profile_cast_success : &done;
    Label *failure_target = should_profile ? &profile_cast_failure : stub->entry();

    if (should_profile) {
      Label not_null;
      __ bnez(value, not_null);
      // Object is null; update MDO and exit
      Register mdo = klass_RInfo;
      __ mov_metadata(mdo, md->constant_encoding());
      Address data_addr = Address(mdo, md->byte_offset_of_slot(data, DataLayout::flags_offset()));
      __ ld_bu(SCR2, data_addr);
      __ ori(SCR2, SCR2, BitData::null_seen_byte_constant());
      __ st_b(SCR2, data_addr);
      __ b(done);
      __ bind(not_null);
    } else {
      __ beqz(value, done);
    }

    add_debug_info_for_null_check_here(op->info_for_exception());
    __ load_klass(k_RInfo, array);
    __ load_klass(klass_RInfo, value);

    // get instance klass (it's already uncompressed)
    __ ld_ptr(k_RInfo, Address(k_RInfo, ObjArrayKlass::element_klass_offset()));
    // perform the fast part of the checking logic
    __ check_klass_subtype_fast_path(klass_RInfo, k_RInfo, Rtmp1, success_target, failure_target, NULL);
    // call out-of-line instance of __ check_klass_subtype_slow_path(...):
    __ addi_d(SP, SP, -2 * wordSize);
    __ st_ptr(k_RInfo, Address(SP, 0 * wordSize));
    __ st_ptr(klass_RInfo, Address(SP, 1 * wordSize));
    __ call(Runtime1::entry_for(Runtime1::slow_subtype_check_id), relocInfo::runtime_call_type);
    __ ld_ptr(k_RInfo, Address(SP, 0 * wordSize));
    __ ld_ptr(klass_RInfo, Address(SP, 1 * wordSize));
    __ addi_d(SP, SP, 2 * wordSize);
    // result is a boolean
    __ beqz(k_RInfo, *failure_target);
    // fall through to the success case

    if (should_profile) {
      Register mdo = klass_RInfo, recv = k_RInfo;
      __ bind(profile_cast_success);
      __ mov_metadata(mdo, md->constant_encoding());
      __ load_klass(recv, value);
      Label update_done;
      type_profile_helper(mdo, md, data, recv, &done);
      __ b(done);

      __ bind(profile_cast_failure);
      __ mov_metadata(mdo, md->constant_encoding());
      Address counter_addr(mdo, md->byte_offset_of_slot(data, CounterData::count_offset()));
      __ lea(SCR2, counter_addr);
      __ ld_ptr(SCR1, Address(SCR2));
      __ addi_d(SCR1, SCR1, -DataLayout::counter_increment);
      __ st_ptr(SCR1, Address(SCR2));
      __ b(*stub->entry());
    }

    __ bind(done);
  } else if (code == lir_checkcast) {
    Register obj = op->object()->as_register();
    Register dst = op->result_opr()->as_register();
    Label success;
    emit_typecheck_helper(op, &success, op->stub()->entry(), &success);
    __ bind(success);
    if (dst != obj) {
      __ move(dst, obj);
    }
  } else if (code == lir_instanceof) {
    Register obj = op->object()->as_register();
    Register dst = op->result_opr()->as_register();
    Label success, failure, done;
    emit_typecheck_helper(op, &success, &failure, &failure);
    __ bind(failure);
    __ move(dst, R0);
    __ b(done);
    __ bind(success);
    __ li(dst, 1);
    __ bind(done);
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::casw(Register addr, Register newval, Register cmpval, bool sign) {
  __ cmpxchg32(Address(addr, 0), cmpval, newval, SCR1, sign,
               /* retold */ false, /* barrier */ true);
}

void LIR_Assembler::casl(Register addr, Register newval, Register cmpval) {
  __ cmpxchg(Address(addr, 0), cmpval, newval, SCR1,
             /* retold */ false, /* barrier */ true);
}

void LIR_Assembler::emit_compare_and_swap(LIR_OpCompareAndSwap* op) {
  assert(VM_Version::supports_cx8(), "wrong machine");
  Register addr;
  if (op->addr()->is_register()) {
    addr = as_reg(op->addr());
  } else {
    assert(op->addr()->is_address(), "what else?");
    LIR_Address* addr_ptr = op->addr()->as_address_ptr();
    assert(addr_ptr->disp() == 0, "need 0 disp");
    assert(addr_ptr->index() == LIR_OprDesc::illegalOpr(), "need 0 index");
    addr = as_reg(addr_ptr->base());
  }
  Register newval = as_reg(op->new_value());
  Register cmpval = as_reg(op->cmp_value());

  if (op->code() == lir_cas_obj) {
    if (UseCompressedOops) {
      Register t1 = op->tmp1()->as_register();
      assert(op->tmp1()->is_valid(), "must be");
      __ encode_heap_oop(t1, cmpval);
      cmpval = t1;
      __ encode_heap_oop(SCR2, newval);
      newval = SCR2;
      casw(addr, newval, cmpval, false);
    } else {
      casl(addr, newval, cmpval);
    }
  } else if (op->code() == lir_cas_int) {
    casw(addr, newval, cmpval, true);
  } else {
    casl(addr, newval, cmpval);
  }
}

void LIR_Assembler::cmove(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2,
                          LIR_Opr result, BasicType type) {
  Unimplemented();
}

void LIR_Assembler::cmp_cmove(LIR_Condition condition, LIR_Opr left, LIR_Opr right,
                              LIR_Opr src1, LIR_Opr src2, LIR_Opr result, BasicType type) {
  assert(result->is_single_cpu() || result->is_double_cpu(), "expect single register for result");
  assert(left->is_single_cpu() || left->is_double_cpu(), "must be");
  Register regd = (result->type() == T_LONG) ? result->as_register_lo() : result->as_register();
  Register regl = as_reg(left);
  Register regr = noreg;
  Register reg1 = noreg;
  Register reg2 = noreg;
  jlong immr = 0;

  // comparison operands
  if (right->is_single_cpu()) {
    // cpu register - cpu register
    regr = right->as_register();
  } else if (right->is_double_cpu()) {
    // cpu register - cpu register
    regr = right->as_register_lo();
  } else if (right->is_constant()) {
    switch(right->type()) {
    case T_INT:
    case T_ADDRESS:
      immr = right->as_constant_ptr()->as_jint();
      break;
    case T_LONG:
      immr = right->as_constant_ptr()->as_jlong();
      break;
    case T_METADATA:
      immr = (intptr_t)right->as_constant_ptr()->as_metadata();
      break;
    case T_OBJECT:
    case T_ARRAY:
      if (right->as_constant_ptr()->as_jobject() != NULL) {
        regr = SCR1;
        jobject2reg(right->as_constant_ptr()->as_jobject(), regr);
      } else {
        immr = 0;
      }
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  } else {
    ShouldNotReachHere();
  }

  if (regr == noreg) {
    switch (condition) {
    case lir_cond_equal:
    case lir_cond_notEqual:
      if (!Assembler::is_simm(-immr, 12)) {
        regr = SCR1;
        __ li(regr, immr);
      }
      break;
    default:
      if (!Assembler::is_simm(immr, 12)) {
        regr = SCR1;
        __ li(regr, immr);
      }
    }
  }

  // special cases
  if (src1->is_constant() && src2->is_constant()) {
    jlong val1 = 0, val2 = 0;
    if (src1->type() == T_INT && src2->type() == T_INT) {
      val1 = src1->as_jint();
      val2 = src2->as_jint();
    } else if (src1->type() == T_LONG && src2->type() == T_LONG) {
      val1 = src1->as_jlong();
      val2 = src2->as_jlong();
    }
    if (val1 == 0 && val2 == 1) {
      if (regr == noreg) {
        switch (condition) {
          case lir_cond_equal:
            if (immr == 0) {
              __ sltu(regd, R0, regl);
            } else {
              __ addi_d(SCR1, regl, -immr);
              __ li(regd, 1);
              __ maskeqz(regd, regd, SCR1);
            }
            break;
          case lir_cond_notEqual:
            if (immr == 0) {
              __ sltu(regd, R0, regl);
              __ xori(regd, regd, 1);
            } else {
              __ addi_d(SCR1, regl, -immr);
              __ li(regd, 1);
              __ masknez(regd, regd, SCR1);
            }
            break;
          case lir_cond_less:
            __ slti(regd, regl, immr);
            __ xori(regd, regd, 1);
            break;
          case lir_cond_lessEqual:
            if (immr == 0) {
              __ slt(regd, R0, regl);
            } else {
              __ li(SCR1, immr);
              __ slt(regd, SCR1, regl);
            }
            break;
          case lir_cond_greater:
            if (immr == 0) {
              __ slt(regd, R0, regl);
            } else {
              __ li(SCR1, immr);
              __ slt(regd, SCR1, regl);
            }
            __ xori(regd, regd, 1);
            break;
          case lir_cond_greaterEqual:
            __ slti(regd, regl, immr);
            break;
          case lir_cond_belowEqual:
            if (immr == 0) {
              __ sltu(regd, R0, regl);
            } else {
              __ li(SCR1, immr);
              __ sltu(regd, SCR1, regl);
            }
            break;
          case lir_cond_aboveEqual:
            __ sltui(regd, regl, immr);
            break;
          default:
            ShouldNotReachHere();
        }
      } else {
        switch (condition) {
          case lir_cond_equal:
            __ sub_d(SCR1, regl, regr);
            __ li(regd, 1);
            __ maskeqz(regd, regd, SCR1);
            break;
          case lir_cond_notEqual:
            __ sub_d(SCR1, regl, regr);
            __ li(regd, 1);
            __ masknez(regd, regd, SCR1);
            break;
          case lir_cond_less:
            __ slt(regd, regl, regr);
            __ xori(regd, regd, 1);
            break;
          case lir_cond_lessEqual:
            __ slt(regd, regr, regl);
            break;
          case lir_cond_greater:
            __ slt(regd, regr, regl);
            __ xori(regd, regd, 1);
            break;
          case lir_cond_greaterEqual:
            __ slt(regd, regl, regr);
            break;
          case lir_cond_belowEqual:
            __ sltu(regd, regr, regl);
            break;
          case lir_cond_aboveEqual:
            __ sltu(regd, regl, regr);
            break;
          default:
            ShouldNotReachHere();
        }
      }
      return;
    } else if (val1 == 1 && val2 == 0) {
      if (regr == noreg) {
        switch (condition) {
          case lir_cond_equal:
            if (immr == 0) {
              __ sltu(regd, R0, regl);
              __ xori(regd, regd, 1);
            } else {
              __ addi_d(SCR1, regl, -immr);
              __ li(regd, 1);
              __ masknez(regd, regd, SCR1);
            }
            break;
          case lir_cond_notEqual:
            if (immr == 0) {
              __ sltu(regd, R0, regl);
            } else {
              __ addi_d(SCR1, regl, -immr);
              __ li(regd, 1);
              __ maskeqz(regd, regd, SCR1);
            }
            break;
          case lir_cond_less:
            __ slti(regd, regl, immr);
            break;
          case lir_cond_lessEqual:
            if (immr == 0) {
              __ slt(regd, R0, regl);
            } else {
              __ li(SCR1, immr);
              __ slt(regd, SCR1, regl);
            }
            __ xori(regd, regd, 1);
            break;
          case lir_cond_greater:
            if (immr == 0) {
              __ slt(regd, R0, regl);
            } else {
              __ li(SCR1, immr);
              __ slt(regd, SCR1, regl);
            }
            break;
          case lir_cond_greaterEqual:
            __ slti(regd, regl, immr);
            __ xori(regd, regd, 1);
            break;
          case lir_cond_belowEqual:
            if (immr == 0) {
              __ sltu(regd, R0, regl);
            } else {
              __ li(SCR1, immr);
              __ sltu(regd, SCR1, regl);
            }
            __ xori(regd, regd, 1);
            break;
          case lir_cond_aboveEqual:
            __ sltui(regd, regl, immr);
            __ xori(regd, regd, 1);
            break;
          default:
            ShouldNotReachHere();
        }
      } else {
        switch (condition) {
          case lir_cond_equal:
            __ sub_d(SCR1, regl, regr);
            __ li(regd, 1);
            __ masknez(regd, regd, SCR1);
            break;
          case lir_cond_notEqual:
            __ sub_d(SCR1, regl, regr);
            __ li(regd, 1);
            __ maskeqz(regd, regd, SCR1);
            break;
          case lir_cond_less:
            __ slt(regd, regl, regr);
            break;
          case lir_cond_lessEqual:
            __ slt(regd, regr, regl);
            __ xori(regd, regd, 1);
            break;
          case lir_cond_greater:
            __ slt(regd, regr, regl);
            break;
          case lir_cond_greaterEqual:
            __ slt(regd, regl, regr);
            __ xori(regd, regd, 1);
            break;
          case lir_cond_belowEqual:
            __ sltu(regd, regr, regl);
            __ xori(regd, regd, 1);
            break;
          case lir_cond_aboveEqual:
            __ sltu(regd, regl, regr);
            __ xori(regd, regd, 1);
            break;
          default:
            ShouldNotReachHere();
        }
      }
      return;
    }
  }

  // cmp
  if (regr == noreg) {
    switch (condition) {
      case lir_cond_equal:
        __ addi_d(SCR2, regl, -immr);
        break;
      case lir_cond_notEqual:
        __ addi_d(SCR2, regl, -immr);
        break;
      case lir_cond_less:
        __ slti(SCR2, regl, immr);
        break;
      case lir_cond_lessEqual:
        __ li(SCR1, immr);
        __ slt(SCR2, SCR1, regl);
        break;
      case lir_cond_greater:
        __ li(SCR1, immr);
        __ slt(SCR2, SCR1, regl);
        break;
      case lir_cond_greaterEqual:
        __ slti(SCR2, regl, immr);
        break;
      case lir_cond_belowEqual:
        __ li(SCR1, immr);
        __ sltu(SCR2, SCR1, regl);
        break;
      case lir_cond_aboveEqual:
        __ sltui(SCR2, regl, immr);
        break;
      default:
        ShouldNotReachHere();
    }
  } else {
    switch (condition) {
      case lir_cond_equal:
        __ sub_d(SCR2, regl, regr);
        break;
      case lir_cond_notEqual:
        __ sub_d(SCR2, regl, regr);
        break;
      case lir_cond_less:
        __ slt(SCR2, regl, regr);
        break;
      case lir_cond_lessEqual:
        __ slt(SCR2, regr, regl);
        break;
      case lir_cond_greater:
        __ slt(SCR2, regr, regl);
        break;
      case lir_cond_greaterEqual:
        __ slt(SCR2, regl, regr);
        break;
      case lir_cond_belowEqual:
        __ sltu(SCR2, regr, regl);
        break;
      case lir_cond_aboveEqual:
        __ sltu(SCR2, regl, regr);
        break;
      default:
        ShouldNotReachHere();
    }
  }

  // value operands
  if (src1->is_stack()) {
    stack2reg(src1, result, result->type());
    reg1 = regd;
  } else if (src1->is_constant()) {
    const2reg(src1, result, lir_patch_none, NULL);
    reg1 = regd;
  } else {
    reg1 = (src1->type() == T_LONG) ? src1->as_register_lo() : src1->as_register();
  }

  if (src2->is_stack()) {
    stack2reg(src2, FrameMap::scr1_opr, result->type());
    reg2 = SCR1;
  } else if (src2->is_constant()) {
    LIR_Opr tmp = src2->type() == T_LONG ? FrameMap::scr1_long_opr : FrameMap::scr1_opr;
    const2reg(src2, tmp, lir_patch_none, NULL);
    reg2 = SCR1;
  } else {
    reg2 = (src2->type() == T_LONG) ? src2->as_register_lo() : src2->as_register();
  }

  // cmove
  switch (condition) {
    case lir_cond_equal:
      __ masknez(regd, reg1, SCR2);
      __ maskeqz(SCR2, reg2, SCR2);
      break;
    case lir_cond_notEqual:
      __ maskeqz(regd, reg1, SCR2);
      __ masknez(SCR2, reg2, SCR2);
      break;
    case lir_cond_less:
      __ maskeqz(regd, reg1, SCR2);
      __ masknez(SCR2, reg2, SCR2);
      break;
    case lir_cond_lessEqual:
      __ masknez(regd, reg1, SCR2);
      __ maskeqz(SCR2, reg2, SCR2);
      break;
    case lir_cond_greater:
      __ maskeqz(regd, reg1, SCR2);
      __ masknez(SCR2, reg2, SCR2);
      break;
    case lir_cond_greaterEqual:
      __ masknez(regd, reg1, SCR2);
      __ maskeqz(SCR2, reg2, SCR2);
      break;
    case lir_cond_belowEqual:
      __ masknez(regd, reg1, SCR2);
      __ maskeqz(SCR2, reg2, SCR2);
      break;
    case lir_cond_aboveEqual:
      __ masknez(regd, reg1, SCR2);
      __ maskeqz(SCR2, reg2, SCR2);
      break;
    default:
      ShouldNotReachHere();
  }

  __ OR(regd, regd, SCR2);
}

void LIR_Assembler::arith_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dest,
                             CodeEmitInfo* info, bool pop_fpu_stack) {
  assert(info == NULL, "should never be used, idiv/irem and ldiv/lrem not handled by this method");

  if (left->is_single_cpu()) {
    Register lreg = left->as_register();
    Register dreg = as_reg(dest);

    if (right->is_single_cpu()) {
      // cpu register - cpu register
      assert(left->type() == T_INT && right->type() == T_INT && dest->type() == T_INT, "should be");
      Register rreg = right->as_register();
      switch (code) {
        case lir_add: __ add_w (dest->as_register(), lreg, rreg); break;
        case lir_sub: __ sub_w (dest->as_register(), lreg, rreg); break;
        case lir_mul: __ mul_w (dest->as_register(), lreg, rreg); break;
        default:      ShouldNotReachHere();
      }
    } else if (right->is_double_cpu()) {
      Register rreg = right->as_register_lo();
      // single_cpu + double_cpu: can happen with obj+long
      assert(code == lir_add || code == lir_sub, "mismatched arithmetic op");
      switch (code) {
        case lir_add: __ add_d(dreg, lreg, rreg); break;
        case lir_sub: __ sub_d(dreg, lreg, rreg); break;
        default:      ShouldNotReachHere();
      }
    } else if (right->is_constant()) {
      // cpu register - constant
      jlong c;

      // FIXME: This is fugly: we really need to factor all this logic.
      switch(right->type()) {
        case T_LONG:
          c = right->as_constant_ptr()->as_jlong();
          break;
        case T_INT:
        case T_ADDRESS:
          c = right->as_constant_ptr()->as_jint();
          break;
        default:
          ShouldNotReachHere();
          c = 0; // unreachable
          break;
      }

      assert(code == lir_add || code == lir_sub, "mismatched arithmetic op");
      if (c == 0 && dreg == lreg) {
        COMMENT("effective nop elided");
        return;
      }

      switch(left->type()) {
        case T_INT:
          switch (code) {
            case lir_add: __ addi_w(dreg, lreg, c); break;
            case lir_sub: __ addi_w(dreg, lreg, -c); break;
            default:      ShouldNotReachHere();
          }
          break;
        case T_OBJECT:
        case T_ADDRESS:
          switch (code) {
          case lir_add: __ addi_d(dreg, lreg, c); break;
          case lir_sub: __ addi_d(dreg, lreg, -c); break;
          default:      ShouldNotReachHere();
          }
          break;
        default:
          ShouldNotReachHere();
      }
    } else {
      ShouldNotReachHere();
    }
  } else if (left->is_double_cpu()) {
    Register lreg_lo = left->as_register_lo();

    if (right->is_double_cpu()) {
      // cpu register - cpu register
      Register rreg_lo = right->as_register_lo();
      switch (code) {
        case lir_add: __ add_d(dest->as_register_lo(), lreg_lo, rreg_lo); break;
        case lir_sub: __ sub_d(dest->as_register_lo(), lreg_lo, rreg_lo); break;
        case lir_mul: __ mul_d(dest->as_register_lo(), lreg_lo, rreg_lo); break;
        case lir_div: __ div_d(dest->as_register_lo(), lreg_lo, rreg_lo); break;
        case lir_rem: __ mod_d(dest->as_register_lo(), lreg_lo, rreg_lo); break;
        default:      ShouldNotReachHere();
      }

    } else if (right->is_constant()) {
      jlong c = right->as_constant_ptr()->as_jlong();
      Register dreg = as_reg(dest);
      switch (code) {
        case lir_add:
        case lir_sub:
          if (c == 0 && dreg == lreg_lo) {
            COMMENT("effective nop elided");
            return;
          }
          code == lir_add ? __ addi_d(dreg, lreg_lo, c) : __ addi_d(dreg, lreg_lo, -c);
          break;
        case lir_div:
          assert(c > 0 && is_power_of_2(c), "divisor must be power-of-2 constant");
          if (c == 1) {
            // move lreg_lo to dreg if divisor is 1
            __ move(dreg, lreg_lo);
          } else {
            unsigned int shift = exact_log2(c);
            // use scr1 as intermediate result register
            __ srai_d(SCR1, lreg_lo, 63);
            __ srli_d(SCR1, SCR1, 64 - shift);
            __ add_d(SCR1, lreg_lo, SCR1);
            __ srai_d(dreg, SCR1, shift);
          }
          break;
        case lir_rem:
          assert(c > 0 && is_power_of_2(c), "divisor must be power-of-2 constant");
          if (c == 1) {
            // move 0 to dreg if divisor is 1
            __ move(dreg, R0);
          } else {
            // use scr1/2 as intermediate result register
            __ sub_d(SCR1, R0, lreg_lo);
            __ slt(SCR2, SCR1, R0);
            __ andi(dreg, lreg_lo, c - 1);
            __ andi(SCR1, SCR1, c - 1);
            __ sub_d(SCR1, R0, SCR1);
            __ maskeqz(dreg, dreg, SCR2);
            __ masknez(SCR1, SCR1, SCR2);
            __ OR(dreg, dreg, SCR1);
          }
          break;
        default:
          ShouldNotReachHere();
      }
    } else {
      ShouldNotReachHere();
    }
  } else if (left->is_single_fpu()) {
    assert(right->is_single_fpu(), "right hand side of float arithmetics needs to be float register");
    switch (code) {
      case lir_add: __ fadd_s (dest->as_float_reg(), left->as_float_reg(), right->as_float_reg()); break;
      case lir_sub: __ fsub_s (dest->as_float_reg(), left->as_float_reg(), right->as_float_reg()); break;
      case lir_mul_strictfp: // fall through
      case lir_mul: __ fmul_s (dest->as_float_reg(), left->as_float_reg(), right->as_float_reg()); break;
      case lir_div_strictfp: // fall through
      case lir_div: __ fdiv_s (dest->as_float_reg(), left->as_float_reg(), right->as_float_reg()); break;
      default:      ShouldNotReachHere();
    }
  } else if (left->is_double_fpu()) {
    if (right->is_double_fpu()) {
      // fpu register - fpu register
      switch (code) {
        case lir_add: __ fadd_d (dest->as_double_reg(), left->as_double_reg(), right->as_double_reg()); break;
        case lir_sub: __ fsub_d (dest->as_double_reg(), left->as_double_reg(), right->as_double_reg()); break;
        case lir_mul_strictfp: // fall through
        case lir_mul: __ fmul_d (dest->as_double_reg(), left->as_double_reg(), right->as_double_reg()); break;
        case lir_div_strictfp: // fall through
        case lir_div: __ fdiv_d (dest->as_double_reg(), left->as_double_reg(), right->as_double_reg()); break;
        default:      ShouldNotReachHere();
      }
    } else {
      if (right->is_constant()) {
        ShouldNotReachHere();
      }
      ShouldNotReachHere();
    }
  } else if (left->is_single_stack() || left->is_address()) {
    assert(left == dest, "left and dest must be equal");
    ShouldNotReachHere();
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::arith_fpu_implementation(LIR_Code code, int left_index, int right_index,
                                             int dest_index, bool pop_fpu_stack) {
  Unimplemented();
}

void LIR_Assembler::intrinsic_op(LIR_Code code, LIR_Opr value, LIR_Opr unused, LIR_Opr dest, LIR_Op* op) {
  switch(code) {
    case lir_abs : __ fabs_d(dest->as_double_reg(), value->as_double_reg()); break;
    case lir_sqrt: __ fsqrt_d(dest->as_double_reg(), value->as_double_reg()); break;
    default      : ShouldNotReachHere();
  }
}

void LIR_Assembler::logic_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dst) {
  assert(left->is_single_cpu() || left->is_double_cpu(), "expect single or double register");
  Register Rleft = left->is_single_cpu() ? left->as_register() : left->as_register_lo();

   if (dst->is_single_cpu()) {
     Register Rdst = dst->as_register();
     if (right->is_constant()) {
       switch (code) {
         case lir_logic_and:
           if (Assembler::is_uimm(right->as_jint(), 12)) {
             __ andi(Rdst, Rleft, right->as_jint());
           } else {
             __ li(AT, right->as_jint());
             __ AND(Rdst, Rleft, AT);
           }
           break;
         case lir_logic_or:  __  ori(Rdst, Rleft, right->as_jint()); break;
         case lir_logic_xor: __ xori(Rdst, Rleft, right->as_jint()); break;
         default:            ShouldNotReachHere(); break;
       }
     } else {
       Register Rright = right->is_single_cpu() ? right->as_register() : right->as_register_lo();
       switch (code) {
         case lir_logic_and: __ AND(Rdst, Rleft, Rright); break;
         case lir_logic_or:  __  OR(Rdst, Rleft, Rright); break;
         case lir_logic_xor: __ XOR(Rdst, Rleft, Rright); break;
         default:            ShouldNotReachHere(); break;
       }
     }
   } else {
     Register Rdst = dst->as_register_lo();
     if (right->is_constant()) {
       switch (code) {
         case lir_logic_and:
           if (Assembler::is_uimm(right->as_jlong(), 12)) {
             __ andi(Rdst, Rleft, right->as_jlong());
           } else {
             // We can guarantee that transform from HIR LogicOp is in range of
             // uimm(12), but the common code directly generates LIR LogicAnd,
             // and the right-operand is mask with all ones in the high bits.
             __ li(AT, right->as_jlong());
             __ AND(Rdst, Rleft, AT);
           }
           break;
         case lir_logic_or:  __  ori(Rdst, Rleft, right->as_jlong()); break;
         case lir_logic_xor: __ xori(Rdst, Rleft, right->as_jlong()); break;
         default:            ShouldNotReachHere(); break;
       }
     } else {
       Register Rright = right->is_single_cpu() ? right->as_register() : right->as_register_lo();
       switch (code) {
         case lir_logic_and: __ AND(Rdst, Rleft, Rright); break;
         case lir_logic_or:  __  OR(Rdst, Rleft, Rright); break;
         case lir_logic_xor: __ XOR(Rdst, Rleft, Rright); break;
         default:            ShouldNotReachHere(); break;
       }
     }
   }
}

void LIR_Assembler::arithmetic_idiv(LIR_Code code, LIR_Opr left, LIR_Opr right,
                                    LIR_Opr illegal, LIR_Opr result, CodeEmitInfo* info) {
  // opcode check
  assert((code == lir_idiv) || (code == lir_irem), "opcode must be idiv or irem");
  bool is_irem = (code == lir_irem);

  // operand check
  assert(left->is_single_cpu(), "left must be register");
  assert(right->is_single_cpu() || right->is_constant(), "right must be register or constant");
  assert(result->is_single_cpu(), "result must be register");
  Register lreg = left->as_register();
  Register dreg = result->as_register();

  // power-of-2 constant check and codegen
  if (right->is_constant()) {
    int c = right->as_constant_ptr()->as_jint();
    assert(c > 0 && is_power_of_2(c), "divisor must be power-of-2 constant");
    if (is_irem) {
      if (c == 1) {
        // move 0 to dreg if divisor is 1
        __ move(dreg, R0);
      } else {
        // use scr1/2 as intermediate result register
        __ sub_w(SCR1, R0, lreg);
        __ slt(SCR2, SCR1, R0);
        __ andi(dreg, lreg, c - 1);
        __ andi(SCR1, SCR1, c - 1);
        __ sub_w(SCR1, R0, SCR1);
        __ maskeqz(dreg, dreg, SCR2);
        __ masknez(SCR1, SCR1, SCR2);
        __ OR(dreg, dreg, SCR1);
      }
    } else {
      if (c == 1) {
        // move lreg to dreg if divisor is 1
        __ move(dreg, lreg);
      } else {
        unsigned int shift = exact_log2(c);
        // use scr1 as intermediate result register
        __ srai_w(SCR1, lreg, 31);
        __ srli_w(SCR1, SCR1, 32 - shift);
        __ add_w(SCR1, lreg, SCR1);
        __ srai_w(dreg, SCR1, shift);
      }
    }
  } else {
    Register rreg = right->as_register();
    if (is_irem)
      __ mod_w(dreg, lreg, rreg);
    else
      __ div_w(dreg, lreg, rreg);
  }
}

void LIR_Assembler::comp_op(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Op2* op) {
  Unimplemented();
}

void LIR_Assembler::comp_fl2i(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dst, LIR_Op2* op){
  if (code == lir_cmp_fd2i || code == lir_ucmp_fd2i) {
    bool is_unordered_less = (code == lir_ucmp_fd2i);
    if (left->is_single_fpu()) {
      if (is_unordered_less) {
        __ fcmp_clt_s(FCC0, right->as_float_reg(), left->as_float_reg());
        __ fcmp_cult_s(FCC1, left->as_float_reg(), right->as_float_reg());
      } else {
        __ fcmp_cult_s(FCC0, right->as_float_reg(), left->as_float_reg());
        __ fcmp_clt_s(FCC1, left->as_float_reg(), right->as_float_reg());
      }
    } else if (left->is_double_fpu()) {
      if (is_unordered_less) {
        __ fcmp_clt_d(FCC0, right->as_double_reg(), left->as_double_reg());
        __ fcmp_cult_d(FCC1, left->as_double_reg(), right->as_double_reg());
      } else {
        __ fcmp_cult_d(FCC0, right->as_double_reg(), left->as_double_reg());
        __ fcmp_clt_d(FCC1, left->as_double_reg(), right->as_double_reg());
      }
    } else {
      ShouldNotReachHere();
    }
    __ movcf2gr(dst->as_register(), FCC0);
    __ movcf2gr(SCR1, FCC1);
    __ sub_d(dst->as_register(), dst->as_register(), SCR1);
  } else if (code == lir_cmp_l2i) {
    __ slt(SCR1, left->as_register_lo(), right->as_register_lo());
    __ slt(dst->as_register(), right->as_register_lo(), left->as_register_lo());
    __ sub_d(dst->as_register(), dst->as_register(), SCR1);
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::align_call(LIR_Code code) {}

void LIR_Assembler::call(LIR_OpJavaCall* op, relocInfo::relocType rtype) {
  address call = __ trampoline_call(AddressLiteral(op->addr(), rtype));
  if (call == NULL) {
    bailout("trampoline stub overflow");
    return;
  }
  add_call_info(code_offset(), op->info());
}

void LIR_Assembler::ic_call(LIR_OpJavaCall* op) {
  address call = __ ic_call(op->addr());
  if (call == NULL) {
    bailout("trampoline stub overflow");
    return;
  }
  add_call_info(code_offset(), op->info());
}

/* Currently, vtable-dispatch is only enabled for sparc platforms */
void LIR_Assembler::vtable_call(LIR_OpJavaCall* op) {
  ShouldNotReachHere();
}

void LIR_Assembler::emit_static_call_stub() {
  address call_pc = __ pc();
  address stub = __ start_a_stub(call_stub_size);
  if (stub == NULL) {
    bailout("static call stub overflow");
    return;
  }

  int start = __ offset();

  __ relocate(static_stub_Relocation::spec(call_pc));

  // Code stream for loading method may be changed.
  __ ibar(0);

  // Rmethod contains Method*, it should be relocated for GC
  // static stub relocation also tags the Method* in the code-stream.
  __ mov_metadata(Rmethod, NULL);
  // This is recognized as unresolved by relocs/nativeInst/ic code
  __ patchable_jump(__ pc());

  assert(__ offset() - start <= call_stub_size, "stub too big");
  __ end_a_stub();
}

void LIR_Assembler::throw_op(LIR_Opr exceptionPC, LIR_Opr exceptionOop, CodeEmitInfo* info) {
  assert(exceptionOop->as_register() == A0, "must match");
  assert(exceptionPC->as_register() == A1, "must match");

  // exception object is not added to oop map by LinearScan
  // (LinearScan assumes that no oops are in fixed registers)
  info->add_register_oop(exceptionOop);
  Runtime1::StubID unwind_id;

  // get current pc information
  // pc is only needed if the method has an exception handler, the unwind code does not need it.
  if (compilation()->debug_info_recorder()->last_pc_offset() == __ offset()) {
    // As no instructions have been generated yet for this LIR node it's
    // possible that an oop map already exists for the current offset.
    // In that case insert an dummy NOP here to ensure all oop map PCs
    // are unique. See JDK-8237483.
    __ nop();
  }
  Label L;
  int pc_for_athrow_offset = __ offset();
  __ bind(L);
  __ lipc(exceptionPC->as_register(), L);
  add_call_info(pc_for_athrow_offset, info); // for exception handler

  __ verify_not_null_oop(A0);
  // search an exception handler (A0: exception oop, A1: throwing pc)
  if (compilation()->has_fpu_code()) {
    unwind_id = Runtime1::handle_exception_id;
  } else {
    unwind_id = Runtime1::handle_exception_nofpu_id;
  }
  __ call(Runtime1::entry_for(unwind_id), relocInfo::runtime_call_type);

  // FIXME: enough room for two byte trap   ????
  __ nop();
}

void LIR_Assembler::unwind_op(LIR_Opr exceptionOop) {
  assert(exceptionOop->as_register() == A0, "must match");
  __ b(_unwind_handler_entry);
}

void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, LIR_Opr count, LIR_Opr dest, LIR_Opr tmp) {
  Register lreg = left->is_single_cpu() ? left->as_register() : left->as_register_lo();
  Register dreg = dest->is_single_cpu() ? dest->as_register() : dest->as_register_lo();

  switch (left->type()) {
    case T_INT: {
      switch (code) {
        case lir_shl:  __ sll_w(dreg, lreg, count->as_register()); break;
        case lir_shr:  __ sra_w(dreg, lreg, count->as_register()); break;
        case lir_ushr: __ srl_w(dreg, lreg, count->as_register()); break;
        default:       ShouldNotReachHere(); break;
      }
      break;
    case T_LONG:
    case T_ADDRESS:
    case T_OBJECT:
      switch (code) {
        case lir_shl:  __ sll_d(dreg, lreg, count->as_register()); break;
        case lir_shr:  __ sra_d(dreg, lreg, count->as_register()); break;
        case lir_ushr: __ srl_d(dreg, lreg, count->as_register()); break;
        default:       ShouldNotReachHere(); break;
      }
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  }
}

void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, jint count, LIR_Opr dest) {
  Register dreg = dest->is_single_cpu() ? dest->as_register() : dest->as_register_lo();
  Register lreg = left->is_single_cpu() ? left->as_register() : left->as_register_lo();

  switch (left->type()) {
    case T_INT: {
      switch (code) {
        case lir_shl:  __ slli_w(dreg, lreg, count); break;
        case lir_shr:  __ srai_w(dreg, lreg, count); break;
        case lir_ushr: __ srli_w(dreg, lreg, count); break;
        default:       ShouldNotReachHere(); break;
      }
      break;
    case T_LONG:
    case T_ADDRESS:
    case T_OBJECT:
      switch (code) {
        case lir_shl:  __ slli_d(dreg, lreg, count); break;
        case lir_shr:  __ srai_d(dreg, lreg, count); break;
        case lir_ushr: __ srli_d(dreg, lreg, count); break;
        default:       ShouldNotReachHere(); break;
      }
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  }
}

void LIR_Assembler::store_parameter(Register r, int offset_from_sp_in_words) {
  assert(offset_from_sp_in_words >= 0, "invalid offset from sp");
  int offset_from_sp_in_bytes = offset_from_sp_in_words * BytesPerWord;
  assert(offset_from_sp_in_bytes < frame_map()->reserved_argument_area_size(), "invalid offset");
  __ st_ptr(r, Address(SP, offset_from_sp_in_bytes));
}

void LIR_Assembler::store_parameter(jint c,     int offset_from_sp_in_words) {
  assert(offset_from_sp_in_words >= 0, "invalid offset from sp");
  int offset_from_sp_in_bytes = offset_from_sp_in_words * BytesPerWord;
  assert(offset_from_sp_in_bytes < frame_map()->reserved_argument_area_size(), "invalid offset");
  __ li(SCR2, c);
  __ st_ptr(SCR2, Address(SP, offset_from_sp_in_bytes));
}

void LIR_Assembler::store_parameter(jobject o,  int offset_from_sp_in_words) {
  ShouldNotReachHere();
}

// This code replaces a call to arraycopy; no exception may
// be thrown in this code, they must be thrown in the System.arraycopy
// activation frame; we could save some checks if this would not be the case
void LIR_Assembler::emit_arraycopy(LIR_OpArrayCopy* op) {
  Register j_rarg0 = T0;
  Register j_rarg1 = A0;
  Register j_rarg2 = A1;
  Register j_rarg3 = A2;
  Register j_rarg4 = A3;

  ciArrayKlass* default_type = op->expected_type();
  Register src = op->src()->as_register();
  Register dst = op->dst()->as_register();
  Register src_pos = op->src_pos()->as_register();
  Register dst_pos = op->dst_pos()->as_register();
  Register length  = op->length()->as_register();
  Register tmp = op->tmp()->as_register();

  CodeStub* stub = op->stub();
  int flags = op->flags();
  BasicType basic_type = default_type != NULL ? default_type->element_type()->basic_type() : T_ILLEGAL;
  if (is_reference_type(basic_type))
    basic_type = T_OBJECT;

  // if we don't know anything, just go through the generic arraycopy
  if (default_type == NULL) {
    Label done;
    assert(src == T0 && src_pos == A0, "mismatch in calling convention");

    // Save the arguments in case the generic arraycopy fails and we
    // have to fall back to the JNI stub
    __ st_ptr(dst, Address(SP, 0 * BytesPerWord));
    __ st_ptr(dst_pos, Address(SP, 1 * BytesPerWord));
    __ st_ptr(length, Address(SP, 2 * BytesPerWord));
    __ st_ptr(src_pos, Address(SP, 3 * BytesPerWord));
    __ st_ptr(src, Address(SP, 4 * BytesPerWord));

    address copyfunc_addr = StubRoutines::generic_arraycopy();

    // FIXME: LA
    if (copyfunc_addr == NULL) {
      // Take a slow path for generic arraycopy.
      __ b(*stub->entry());
      __ bind(*stub->continuation());
      return;
    }

    // The arguments are in java calling convention so we shift them
    // to C convention
    assert_different_registers(A0, j_rarg1, j_rarg2, j_rarg3, j_rarg4);
    __ move(A0, j_rarg0);
    assert_different_registers(A1, j_rarg2, j_rarg3, j_rarg4);
    __ move(A1, j_rarg1);
    assert_different_registers(A2, j_rarg3, j_rarg4);
    __ move(A2, j_rarg2);
    assert_different_registers(A3, j_rarg4);
    __ move(A3, j_rarg3);
    __ move(A4, j_rarg4);
#ifndef PRODUCT
    if (PrintC1Statistics) {
      __ li(SCR2, (address)&Runtime1::_generic_arraycopystub_cnt);
      __ increment(SCR2, 1);
    }
#endif
    __ call(copyfunc_addr, relocInfo::runtime_call_type);

    __ beqz(A0, *stub->continuation());

    // Reload values from the stack so they are where the stub
    // expects them.
    __ ld_ptr(dst, Address(SP, 0 * BytesPerWord));
    __ ld_ptr(dst_pos, Address(SP, 1 * BytesPerWord));
    __ ld_ptr(length, Address(SP, 2 * BytesPerWord));
    __ ld_ptr(src_pos, Address(SP, 3 * BytesPerWord));
    __ ld_ptr(src, Address(SP, 4 * BytesPerWord));

    // A0 is -1^K where K == partial copied count
    __ nor(SCR1, A0, R0);
    __ slli_w(SCR1, SCR1, 0);
    // adjust length down and src/end pos up by partial copied count
    __ sub_w(length, length, SCR1);
    __ add_w(src_pos, src_pos, SCR1);
    __ add_w(dst_pos, dst_pos, SCR1);
    __ b(*stub->entry());

    __ bind(*stub->continuation());
    return;
  }

  assert(default_type != NULL && default_type->is_array_klass() && default_type->is_loaded(),
         "must be true at this point");

  int elem_size = type2aelembytes(basic_type);
  Address::ScaleFactor scale = Address::times(elem_size);

  Address src_length_addr = Address(src, arrayOopDesc::length_offset_in_bytes());
  Address dst_length_addr = Address(dst, arrayOopDesc::length_offset_in_bytes());
  Address src_klass_addr = Address(src, oopDesc::klass_offset_in_bytes());
  Address dst_klass_addr = Address(dst, oopDesc::klass_offset_in_bytes());

  // test for NULL
  if (flags & LIR_OpArrayCopy::src_null_check) {
    __ beqz(src, *stub->entry());
  }
  if (flags & LIR_OpArrayCopy::dst_null_check) {
    __ beqz(dst, *stub->entry());
  }

  // If the compiler was not able to prove that exact type of the source or the destination
  // of the arraycopy is an array type, check at runtime if the source or the destination is
  // an instance type.
  if (flags & LIR_OpArrayCopy::type_check) {
    if (!(flags & LIR_OpArrayCopy::LIR_OpArrayCopy::dst_objarray)) {
      __ load_klass(tmp, dst);
      __ ld_w(SCR1, Address(tmp, in_bytes(Klass::layout_helper_offset())));
      __ li(SCR2, Klass::_lh_neutral_value);
      __ bge_far(SCR1, SCR2, *stub->entry(), true);
    }

    if (!(flags & LIR_OpArrayCopy::LIR_OpArrayCopy::src_objarray)) {
      __ load_klass(tmp, src);
      __ ld_w(SCR1, Address(tmp, in_bytes(Klass::layout_helper_offset())));
      __ li(SCR2, Klass::_lh_neutral_value);
      __ bge_far(SCR1, SCR2, *stub->entry(), true);
    }
  }

  // check if negative
  if (flags & LIR_OpArrayCopy::src_pos_positive_check) {
    __ blt_far(src_pos, R0, *stub->entry(), true);
  }
  if (flags & LIR_OpArrayCopy::dst_pos_positive_check) {
    __ blt_far(dst_pos, R0, *stub->entry(), true);
  }

  if (flags & LIR_OpArrayCopy::length_positive_check) {
    __ blt_far(length, R0, *stub->entry(), true);
  }

  if (flags & LIR_OpArrayCopy::src_range_check) {
    __ add_w(tmp, src_pos, length);
    __ ld_wu(SCR1, src_length_addr);
    __ blt_far(SCR1, tmp, *stub->entry(), false);
  }
  if (flags & LIR_OpArrayCopy::dst_range_check) {
    __ add_w(tmp, dst_pos, length);
    __ ld_wu(SCR1, dst_length_addr);
    __ blt_far(SCR1, tmp, *stub->entry(), false);
  }

  if (flags & LIR_OpArrayCopy::type_check) {
    // We don't know the array types are compatible
    if (basic_type != T_OBJECT) {
      // Simple test for basic type arrays
      if (UseCompressedClassPointers) {
        __ ld_wu(tmp, src_klass_addr);
        __ ld_wu(SCR1, dst_klass_addr);
      } else {
        __ ld_ptr(tmp, src_klass_addr);
        __ ld_ptr(SCR1, dst_klass_addr);
      }
      __ bne_far(tmp, SCR1, *stub->entry());
    } else {
      // For object arrays, if src is a sub class of dst then we can
      // safely do the copy.
      Label cont, slow;

      __ addi_d(SP, SP, -2 * wordSize);
      __ st_ptr(dst, Address(SP, 0 * wordSize));
      __ st_ptr(src, Address(SP, 1 * wordSize));

      __ load_klass(src, src);
      __ load_klass(dst, dst);

      __ check_klass_subtype_fast_path(src, dst, tmp, &cont, &slow, NULL);

      __ addi_d(SP, SP, -2 * wordSize);
      __ st_ptr(dst, Address(SP, 0 * wordSize));
      __ st_ptr(src, Address(SP, 1 * wordSize));
      __ call(Runtime1::entry_for(Runtime1::slow_subtype_check_id), relocInfo::runtime_call_type);
      __ ld_ptr(dst, Address(SP, 0 * wordSize));
      __ ld_ptr(src, Address(SP, 1 * wordSize));
      __ addi_d(SP, SP, 2 * wordSize);

      __ bnez(dst, cont);

      __ bind(slow);
      __ ld_ptr(dst, Address(SP, 0 * wordSize));
      __ ld_ptr(src, Address(SP, 1 * wordSize));
      __ addi_d(SP, SP, 2 * wordSize);

      address copyfunc_addr = StubRoutines::checkcast_arraycopy();
      if (copyfunc_addr != NULL) { // use stub if available
        // src is not a sub class of dst so we have to do a
        // per-element check.

        int mask = LIR_OpArrayCopy::src_objarray|LIR_OpArrayCopy::dst_objarray;
        if ((flags & mask) != mask) {
          // Check that at least both of them object arrays.
          assert(flags & mask, "one of the two should be known to be an object array");

          if (!(flags & LIR_OpArrayCopy::src_objarray)) {
            __ load_klass(tmp, src);
          } else if (!(flags & LIR_OpArrayCopy::dst_objarray)) {
            __ load_klass(tmp, dst);
          }
          int lh_offset = in_bytes(Klass::layout_helper_offset());
          Address klass_lh_addr(tmp, lh_offset);
          jint objArray_lh = Klass::array_layout_helper(T_OBJECT);
          __ ld_w(SCR1, klass_lh_addr);
          __ li(SCR2, objArray_lh);
          __ XOR(SCR1, SCR1, SCR2);
          __ bnez(SCR1, *stub->entry());
        }

        // Spill because stubs can use any register they like and it's
        // easier to restore just those that we care about.
        __ st_ptr(dst, Address(SP, 0 * BytesPerWord));
        __ st_ptr(dst_pos, Address(SP, 1 * BytesPerWord));
        __ st_ptr(length, Address(SP, 2 * BytesPerWord));
        __ st_ptr(src_pos, Address(SP, 3 * BytesPerWord));
        __ st_ptr(src, Address(SP, 4 * BytesPerWord));

        __ lea(A0, Address(src, src_pos, scale));
        __ addi_d(A0, A0, arrayOopDesc::base_offset_in_bytes(basic_type));
        assert_different_registers(A0, dst, dst_pos, length);
        __ lea(A1, Address(dst, dst_pos, scale));
        __ addi_d(A1, A1, arrayOopDesc::base_offset_in_bytes(basic_type));
        assert_different_registers(A1, dst, length);
        __ bstrpick_d(A2, length, 31, 0);
        assert_different_registers(A2, dst);

        __ load_klass(A4, dst);
        __ ld_ptr(A4, Address(A4, ObjArrayKlass::element_klass_offset()));
        __ ld_w(A3, Address(A4, Klass::super_check_offset_offset()));
        __ call(copyfunc_addr, relocInfo::runtime_call_type);

#ifndef PRODUCT
        if (PrintC1Statistics) {
          Label failed;
          __ bnez(A0, failed);
          __ li(SCR2, (address)&Runtime1::_arraycopy_checkcast_cnt);
          __ increment(SCR2, 1);
          __ bind(failed);
        }
#endif

        __ beqz(A0, *stub->continuation());

#ifndef PRODUCT
        if (PrintC1Statistics) {
          __ li(SCR2, (address)&Runtime1::_arraycopy_checkcast_attempt_cnt);
          __ increment(SCR2, 1);
        }
#endif
        assert_different_registers(dst, dst_pos, length, src_pos, src, A0, SCR1);

        // Restore previously spilled arguments
        __ ld_ptr(dst, Address(SP, 0 * BytesPerWord));
        __ ld_ptr(dst_pos, Address(SP, 1 * BytesPerWord));
        __ ld_ptr(length, Address(SP, 2 * BytesPerWord));
        __ ld_ptr(src_pos, Address(SP, 3 * BytesPerWord));
        __ ld_ptr(src, Address(SP, 4 * BytesPerWord));

        // return value is -1^K where K is partial copied count
        __ nor(SCR1, A0, R0);
        __ slli_w(SCR1, SCR1, 0);
        // adjust length down and src/end pos up by partial copied count
        __ sub_w(length, length, SCR1);
        __ add_w(src_pos, src_pos, SCR1);
        __ add_w(dst_pos, dst_pos, SCR1);
      }

      __ b(*stub->entry());

      __ bind(cont);
      __ ld_ptr(dst, Address(SP, 0 * wordSize));
      __ ld_ptr(src, Address(SP, 1 * wordSize));
      __ addi_d(SP, SP, 2 * wordSize);
    }
  }

#ifdef ASSERT
  if (basic_type != T_OBJECT || !(flags & LIR_OpArrayCopy::type_check)) {
    // Sanity check the known type with the incoming class.  For the
    // primitive case the types must match exactly with src.klass and
    // dst.klass each exactly matching the default type.  For the
    // object array case, if no type check is needed then either the
    // dst type is exactly the expected type and the src type is a
    // subtype which we can't check or src is the same array as dst
    // but not necessarily exactly of type default_type.
    Label known_ok, halt;
    __ mov_metadata(tmp, default_type->constant_encoding());
    if (UseCompressedClassPointers) {
      __ encode_klass_not_null(tmp);
    }

    if (basic_type != T_OBJECT) {

      if (UseCompressedClassPointers) {
        __ ld_wu(SCR1, dst_klass_addr);
      } else {
        __ ld_ptr(SCR1, dst_klass_addr);
      }
      __ bne(tmp, SCR1, halt);
      if (UseCompressedClassPointers) {
        __ ld_wu(SCR1, src_klass_addr);
      } else {
        __ ld_ptr(SCR1, src_klass_addr);
      }
      __ beq(tmp, SCR1, known_ok);
    } else {
      if (UseCompressedClassPointers) {
        __ ld_wu(SCR1, dst_klass_addr);
      } else {
        __ ld_ptr(SCR1, dst_klass_addr);
      }
      __ beq(tmp, SCR1, known_ok);
      __ beq(src, dst, known_ok);
    }
    __ bind(halt);
    __ stop("incorrect type information in arraycopy");
    __ bind(known_ok);
  }
#endif

#ifndef PRODUCT
  if (PrintC1Statistics) {
    __ li(SCR2, Runtime1::arraycopy_count_address(basic_type));
    __ increment(SCR2, 1);
  }
#endif

  __ lea(A0, Address(src, src_pos, scale));
  __ addi_d(A0, A0, arrayOopDesc::base_offset_in_bytes(basic_type));
  assert_different_registers(A0, dst, dst_pos, length);
  __ lea(A1, Address(dst, dst_pos, scale));
  __ addi_d(A1, A1, arrayOopDesc::base_offset_in_bytes(basic_type));
  assert_different_registers(A1, length);
  __ bstrpick_d(A2, length, 31, 0);

  bool disjoint = (flags & LIR_OpArrayCopy::overlapping) == 0;
  bool aligned = (flags & LIR_OpArrayCopy::unaligned) == 0;
  const char *name;
  address entry = StubRoutines::select_arraycopy_function(basic_type, aligned, disjoint, name, false);

  CodeBlob *cb = CodeCache::find_blob(entry);
  if (cb) {
    __ call(entry, relocInfo::runtime_call_type);
  } else {
    __ call_VM_leaf(entry, 3);
  }

  __ bind(*stub->continuation());
}

void LIR_Assembler::emit_lock(LIR_OpLock* op) {
  Register obj = op->obj_opr()->as_register(); // may not be an oop
  Register hdr = op->hdr_opr()->as_register();
  Register lock = op->lock_opr()->as_register();
  if (!UseFastLocking) {
    __ b(*op->stub()->entry());
  } else if (op->code() == lir_lock) {
    Register scratch = noreg;
    if (UseBiasedLocking) {
      scratch = op->scratch_opr()->as_register();
    }
    assert(BasicLock::displaced_header_offset_in_bytes() == 0,
           "lock_reg must point to the displaced header");
    // add debug info for NullPointerException only if one is possible
    int null_check_offset = __ lock_object(hdr, obj, lock, scratch, *op->stub()->entry());
    if (op->info() != NULL) {
      add_debug_info_for_null_check(null_check_offset, op->info());
    }
    // done
  } else if (op->code() == lir_unlock) {
    assert(BasicLock::displaced_header_offset_in_bytes() == 0,
           "lock_reg must point to the displaced header");
    __ unlock_object(hdr, obj, lock, *op->stub()->entry());
  } else {
    Unimplemented();
  }
  __ bind(*op->stub()->continuation());
}

void LIR_Assembler::emit_profile_call(LIR_OpProfileCall* op) {
  ciMethod* method = op->profiled_method();
  ciMethod* callee = op->profiled_callee();
  int bci = op->profiled_bci();

  // Update counter for all call types
  ciMethodData* md = method->method_data_or_null();
  assert(md != NULL, "Sanity");
  ciProfileData* data = md->bci_to_data(bci);
  assert(data != NULL && data->is_CounterData(), "need CounterData for calls");
  assert(op->mdo()->is_single_cpu(),  "mdo must be allocated");
  Register mdo  = op->mdo()->as_register();
  __ mov_metadata(mdo, md->constant_encoding());
  Address counter_addr(mdo, md->byte_offset_of_slot(data, CounterData::count_offset()));
  Bytecodes::Code bc = method->java_code_at_bci(bci);
  const bool callee_is_static = callee->is_loaded() && callee->is_static();
  // Perform additional virtual call profiling for invokevirtual and
  // invokeinterface bytecodes
  if ((bc == Bytecodes::_invokevirtual || bc == Bytecodes::_invokeinterface) &&
      !callee_is_static &&  // required for optimized MH invokes
      C1ProfileVirtualCalls) {
    assert(op->recv()->is_single_cpu(), "recv must be allocated");
    Register recv = op->recv()->as_register();
    assert_different_registers(mdo, recv);
    assert(data->is_VirtualCallData(), "need VirtualCallData for virtual calls");
    ciKlass* known_klass = op->known_holder();
    if (C1OptimizeVirtualCallProfiling && known_klass != NULL) {
      // We know the type that will be seen at this call site; we can
      // statically update the MethodData* rather than needing to do
      // dynamic tests on the receiver type

      // NOTE: we should probably put a lock around this search to
      // avoid collisions by concurrent compilations
      ciVirtualCallData* vc_data = (ciVirtualCallData*) data;
      uint i;
      for (i = 0; i < VirtualCallData::row_limit(); i++) {
        ciKlass* receiver = vc_data->receiver(i);
        if (known_klass->equals(receiver)) {
          Address data_addr(mdo, md->byte_offset_of_slot(data, VirtualCallData::receiver_count_offset(i)));
          __ ld_ptr(SCR2, data_addr);
          __ addi_d(SCR2, SCR2, DataLayout::counter_increment);
          __ st_ptr(SCR2, data_addr);
          return;
        }
      }

      // Receiver type not found in profile data; select an empty slot

      // Note that this is less efficient than it should be because it
      // always does a write to the receiver part of the
      // VirtualCallData rather than just the first time
      for (i = 0; i < VirtualCallData::row_limit(); i++) {
        ciKlass* receiver = vc_data->receiver(i);
        if (receiver == NULL) {
          Address recv_addr(mdo, md->byte_offset_of_slot(data, VirtualCallData::receiver_offset(i)));
          __ mov_metadata(SCR2, known_klass->constant_encoding());
          __ lea(SCR1, recv_addr);
          __ st_ptr(SCR2, SCR1, 0);
          Address data_addr(mdo, md->byte_offset_of_slot(data, VirtualCallData::receiver_count_offset(i)));
          __ ld_ptr(SCR2, data_addr);
          __ addi_d(SCR2, SCR1, DataLayout::counter_increment);
          __ st_ptr(SCR2, data_addr);
          return;
        }
      }
    } else {
      __ load_klass(recv, recv);
      Label update_done;
      type_profile_helper(mdo, md, data, recv, &update_done);
      // Receiver did not match any saved receiver and there is no empty row for it.
      // Increment total counter to indicate polymorphic case.
      __ ld_ptr(SCR2, counter_addr);
      __ addi_d(SCR2, SCR2, DataLayout::counter_increment);
      __ st_ptr(SCR2, counter_addr);

      __ bind(update_done);
    }
  } else {
    // Static call
    __ ld_ptr(SCR2, counter_addr);
    __ addi_d(SCR2, SCR2, DataLayout::counter_increment);
    __ st_ptr(SCR2, counter_addr);
  }
}

void LIR_Assembler::emit_delay(LIR_OpDelay*) {
  Unimplemented();
}

void LIR_Assembler::monitor_address(int monitor_no, LIR_Opr dst) {
  __ lea(dst->as_register(), frame_map()->address_for_monitor_lock(monitor_no));
}

void LIR_Assembler::emit_updatecrc32(LIR_OpUpdateCRC32* op) {
  assert(op->crc()->is_single_cpu(), "crc must be register");
  assert(op->val()->is_single_cpu(), "byte value must be register");
  assert(op->result_opr()->is_single_cpu(), "result must be register");
  Register crc = op->crc()->as_register();
  Register val = op->val()->as_register();
  Register res = op->result_opr()->as_register();

  assert_different_registers(val, crc, res);
  __ li(res, StubRoutines::crc_table_addr());
  __ nor(crc, crc, R0); // ~crc
  __ update_byte_crc32(crc, val, res);
  __ nor(res, crc, R0); // ~crc
}

void LIR_Assembler::emit_profile_type(LIR_OpProfileType* op) {
  COMMENT("emit_profile_type {");
  Register obj = op->obj()->as_register();
  Register tmp = op->tmp()->as_pointer_register();
  Address mdo_addr = as_Address(op->mdp()->as_address_ptr());
  ciKlass* exact_klass = op->exact_klass();
  intptr_t current_klass = op->current_klass();
  bool not_null = op->not_null();
  bool no_conflict = op->no_conflict();

  Label update, next, none;

  bool do_null = !not_null;
  bool exact_klass_set = exact_klass != NULL && ciTypeEntries::valid_ciklass(current_klass) == exact_klass;
  bool do_update = !TypeEntries::is_type_unknown(current_klass) && !exact_klass_set;

  assert(do_null || do_update, "why are we here?");
  assert(!TypeEntries::was_null_seen(current_klass) || do_update, "why are we here?");
  assert(mdo_addr.base() != SCR1, "wrong register");

  __ verify_oop(obj);

  if (tmp != obj) {
    __ move(tmp, obj);
  }
  if (do_null) {
    __ bnez(tmp, update);
    if (!TypeEntries::was_null_seen(current_klass)) {
      __ ld_ptr(SCR2, mdo_addr);
      __ ori(SCR2, SCR2, TypeEntries::null_seen);
      __ st_ptr(SCR2, mdo_addr);
    }
    if (do_update) {
#ifndef ASSERT
      __ b(next);
    }
#else
      __ b(next);
    }
  } else {
    __ bnez(tmp, update);
    __ stop("unexpected null obj");
#endif
  }

  __ bind(update);

  if (do_update) {
#ifdef ASSERT
    if (exact_klass != NULL) {
      Label ok;
      __ load_klass(tmp, tmp);
      __ mov_metadata(SCR1, exact_klass->constant_encoding());
      __ XOR(SCR1, tmp, SCR1);
      __ beqz(SCR1, ok);
      __ stop("exact klass and actual klass differ");
      __ bind(ok);
    }
#endif
    if (!no_conflict) {
      if (exact_klass == NULL || TypeEntries::is_type_none(current_klass)) {
        if (exact_klass != NULL) {
          __ mov_metadata(tmp, exact_klass->constant_encoding());
        } else {
          __ load_klass(tmp, tmp);
        }

        __ ld_ptr(SCR2, mdo_addr);
        __ XOR(tmp, tmp, SCR2);
        assert(TypeEntries::type_klass_mask == -4, "must be");
        __ bstrpick_d(SCR1, tmp, 63, 2);
        // klass seen before, nothing to do. The unknown bit may have been
        // set already but no need to check.
        __ beqz(SCR1, next);

        __ andi(SCR1, tmp, TypeEntries::type_unknown);
        __ bnez(SCR1, next); // already unknown. Nothing to do anymore.

        if (TypeEntries::is_type_none(current_klass)) {
          __ beqz(SCR2, none);
          __ li(SCR1, (u1)TypeEntries::null_seen);
          __ beq(SCR2, SCR1, none);
          // There is a chance that the checks above (re-reading profiling
          // data from memory) fail if another thread has just set the
          // profiling to this obj's klass
          membar_acquire();
          __ ld_ptr(SCR2, mdo_addr);
          __ XOR(tmp, tmp, SCR2);
          assert(TypeEntries::type_klass_mask == -4, "must be");
          __ bstrpick_d(SCR1, tmp, 63, 2);
          __ beqz(SCR1, next);
        }
      } else {
        assert(ciTypeEntries::valid_ciklass(current_klass) != NULL &&
               ciTypeEntries::valid_ciklass(current_klass) != exact_klass, "conflict only");

        __ ld_ptr(tmp, mdo_addr);
        __ andi(SCR2, tmp, TypeEntries::type_unknown);
        __ bnez(SCR2, next); // already unknown. Nothing to do anymore.
      }

      // different than before. Cannot keep accurate profile.
      __ ld_ptr(SCR2, mdo_addr);
      __ ori(SCR2, SCR2, TypeEntries::type_unknown);
      __ st_ptr(SCR2, mdo_addr);

      if (TypeEntries::is_type_none(current_klass)) {
        __ b(next);

        __ bind(none);
        // first time here. Set profile type.
        __ st_ptr(tmp, mdo_addr);
      }
    } else {
      // There's a single possible klass at this profile point
      assert(exact_klass != NULL, "should be");
      if (TypeEntries::is_type_none(current_klass)) {
        __ mov_metadata(tmp, exact_klass->constant_encoding());
        __ ld_ptr(SCR2, mdo_addr);
        __ XOR(tmp, tmp, SCR2);
        assert(TypeEntries::type_klass_mask == -4, "must be");
        __ bstrpick_d(SCR1, tmp, 63, 2);
        __ beqz(SCR1, next);
#ifdef ASSERT
        {
          Label ok;
          __ ld_ptr(SCR1, mdo_addr);
          __ beqz(SCR1, ok);
          __ li(SCR2, (u1)TypeEntries::null_seen);
          __ beq(SCR1, SCR2, ok);
          // may have been set by another thread
          membar_acquire();
          __ mov_metadata(SCR1, exact_klass->constant_encoding());
          __ ld_ptr(SCR2, mdo_addr);
          __ XOR(SCR2, SCR1, SCR2);
          assert(TypeEntries::type_mask == -2, "must be");
          __ bstrpick_d(SCR2, SCR2, 63, 1);
          __ beqz(SCR2, ok);

          __ stop("unexpected profiling mismatch");
          __ bind(ok);
        }
#endif
        // first time here. Set profile type.
        __ st_ptr(tmp, mdo_addr);
      } else {
        assert(ciTypeEntries::valid_ciklass(current_klass) != NULL &&
               ciTypeEntries::valid_ciklass(current_klass) != exact_klass, "inconsistent");

        __ ld_ptr(tmp, mdo_addr);
        __ andi(SCR1, tmp, TypeEntries::type_unknown);
        __ bnez(SCR1, next); // already unknown. Nothing to do anymore.

        __ ori(tmp, tmp, TypeEntries::type_unknown);
        __ st_ptr(tmp, mdo_addr);
        // FIXME: Write barrier needed here?
      }
    }

    __ bind(next);
  }
  COMMENT("} emit_profile_type");
}

void LIR_Assembler::align_backward_branch_target() {}

void LIR_Assembler::negate(LIR_Opr left, LIR_Opr dest) {
  if (left->is_single_cpu()) {
    assert(dest->is_single_cpu(), "expect single result reg");
    __ sub_w(dest->as_register(), R0, left->as_register());
  } else if (left->is_double_cpu()) {
    assert(dest->is_double_cpu(), "expect double result reg");
    __ sub_d(dest->as_register_lo(), R0, left->as_register_lo());
  } else if (left->is_single_fpu()) {
    assert(dest->is_single_fpu(), "expect single float result reg");
    __ fneg_s(dest->as_float_reg(), left->as_float_reg());
  } else {
    assert(left->is_double_fpu(), "expect double float operand reg");
    assert(dest->is_double_fpu(), "expect double float result reg");
    __ fneg_d(dest->as_double_reg(), left->as_double_reg());
  }
}

void LIR_Assembler::leal(LIR_Opr addr, LIR_Opr dest) {
  __ lea(dest->as_register_lo(), as_Address(addr->as_address_ptr()));
}

void LIR_Assembler::rt_call(LIR_Opr result, address dest, const LIR_OprList* args,
                            LIR_Opr tmp, CodeEmitInfo* info) {
  assert(!tmp->is_valid(), "don't need temporary");
  __ call(dest, relocInfo::runtime_call_type);
  if (info != NULL) {
    add_call_info_here(info);
  }
}

void LIR_Assembler::volatile_move_op(LIR_Opr src, LIR_Opr dest, BasicType type,
                                     CodeEmitInfo* info) {
  if (dest->is_address() || src->is_address()) {
    move_op(src, dest, type, lir_patch_none, info,
            /*pop_fpu_stack*/false, /*unaligned*/false, /*wide*/false);
  } else {
    ShouldNotReachHere();
  }
}

#ifdef ASSERT
// emit run-time assertion
void LIR_Assembler::emit_assert(LIR_OpAssert* op) {
  assert(op->code() == lir_assert, "must be");
  Label ok;

  if (op->in_opr1()->is_valid()) {
    assert(op->in_opr2()->is_valid(), "both operands must be valid");
    assert(op->in_opr1()->is_cpu_register() || op->in_opr2()->is_cpu_register(), "must be");
    Register reg1 = as_reg(op->in_opr1());
    Register reg2 = as_reg(op->in_opr2());
    switch (op->condition()) {
      case lir_cond_equal:        __  beq(reg1, reg2, ok); break;
      case lir_cond_notEqual:     __  bne(reg1, reg2, ok); break;
      case lir_cond_less:         __  blt(reg1, reg2, ok); break;
      case lir_cond_lessEqual:    __  bge(reg2, reg1, ok); break;
      case lir_cond_greaterEqual: __  bge(reg1, reg2, ok); break;
      case lir_cond_greater:      __  blt(reg2, reg1, ok); break;
      case lir_cond_belowEqual:   __ bgeu(reg2, reg1, ok); break;
      case lir_cond_aboveEqual:   __ bgeu(reg1, reg2, ok); break;
      default:                    ShouldNotReachHere();
    }
  } else {
    assert(op->in_opr2()->is_illegal(), "both operands must be illegal");
    assert(op->condition() == lir_cond_always, "no other conditions allowed");
  }
  if (op->halt()) {
    const char* str = __ code_string(op->msg());
    __ stop(str);
  } else {
    breakpoint();
  }
  __ bind(ok);
}
#endif

#ifndef PRODUCT
#define COMMENT(x) do { __ block_comment(x); } while (0)
#else
#define COMMENT(x)
#endif

void LIR_Assembler::membar() {
  COMMENT("membar");
  __ membar(Assembler::AnyAny);
}

void LIR_Assembler::membar_acquire() {
  __ membar(Assembler::Membar_mask_bits(Assembler::LoadLoad | Assembler::LoadStore));
}

void LIR_Assembler::membar_release() {
  __ membar(Assembler::Membar_mask_bits(Assembler::LoadStore|Assembler::StoreStore));
}

void LIR_Assembler::membar_loadload() {
  __ membar(Assembler::LoadLoad);
}

void LIR_Assembler::membar_storestore() {
  __ membar(MacroAssembler::StoreStore);
}

void LIR_Assembler::membar_loadstore() {
  __ membar(MacroAssembler::LoadStore);
}

void LIR_Assembler::membar_storeload() {
  __ membar(MacroAssembler::StoreLoad);
}

void LIR_Assembler::get_thread(LIR_Opr result_reg) {
  __ move(result_reg->as_register(), TREG);
}

void LIR_Assembler::peephole(LIR_List *lir) {
}

void LIR_Assembler::atomic_op(LIR_Code code, LIR_Opr src, LIR_Opr data,
                              LIR_Opr dest, LIR_Opr tmp_op) {
  Address addr = as_Address(src->as_address_ptr());
  BasicType type = src->type();
  Register dst = as_reg(dest);
  Register tmp = as_reg(tmp_op);
  bool is_oop = is_reference_type(type);

  if (Assembler::is_simm(addr.disp(), 12)) {
    __ addi_d(tmp, addr.base(), addr.disp());
  } else {
    __ li(tmp, addr.disp());
    __ add_d(tmp, addr.base(), tmp);
  }
  if (addr.index() != noreg) {
    if (addr.scale() > Address::times_1)
      __ alsl_d(tmp, addr.index(), tmp, addr.scale() - 1);
    else
      __ add_d(tmp, tmp, addr.index());
  }

  switch(type) {
  case T_INT:
    break;
  case T_LONG:
    break;
  case T_OBJECT:
  case T_ARRAY:
    if (UseCompressedOops) {
      // unsigned int
    } else {
      // long
    }
    break;
  default:
    ShouldNotReachHere();
  }

  if (code == lir_xadd) {
    Register inc = noreg;
    if (data->is_constant()) {
      inc = SCR1;
      __ li(inc, as_long(data));
    } else {
      inc = as_reg(data);
    }
    switch(type) {
    case T_INT:
      __ amadd_db_w(dst, inc, tmp);
      break;
    case T_LONG:
      __ amadd_db_d(dst, inc, tmp);
      break;
    case T_OBJECT:
    case T_ARRAY:
      if (UseCompressedOops) {
        __ amadd_db_w(dst, inc, tmp);
        __ lu32i_d(dst, 0);
      } else {
        __ amadd_db_d(dst, inc, tmp);
      }
      break;
    default:
      ShouldNotReachHere();
    }
  } else if (code == lir_xchg) {
    Register obj = as_reg(data);
    if (is_oop && UseCompressedOops) {
      __ encode_heap_oop(SCR2, obj);
      obj = SCR2;
    }
    switch(type) {
    case T_INT:
      __ amswap_db_w(dst, obj, tmp);
      break;
    case T_LONG:
      __ amswap_db_d(dst, obj, tmp);
      break;
    case T_OBJECT:
    case T_ARRAY:
      if (UseCompressedOops) {
        __ amswap_db_w(dst, obj, tmp);
        __ lu32i_d(dst, 0);
      } else {
        __ amswap_db_d(dst, obj, tmp);
      }
      break;
    default:
      ShouldNotReachHere();
    }
    if (is_oop && UseCompressedOops) {
      __ decode_heap_oop(dst);
    }
  } else {
    ShouldNotReachHere();
  }
}

#undef __
