/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2017, 2022, Loongson Technology. All rights reserved.
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
#include "asm/assembler.hpp"
#include "asm/assembler.inline.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "compiler/disassembler.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "interpreter/interpreter.hpp"
#include "memory/cardTableModRefBS.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/os.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/macros.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#include "gc_implementation/g1/heapRegion.hpp"
#endif // INCLUDE_ALL_GCS

#ifdef COMPILER2
#include "opto/compile.hpp"
#include "opto/node.hpp"
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

// Implementation of MacroAssembler

intptr_t MacroAssembler::i[32] = {0};
float MacroAssembler::f[32] = {0.0};

void MacroAssembler::print(outputStream *s) {
  unsigned int k;
  for(k=0; k<sizeof(i)/sizeof(i[0]); k++) {
    s->print_cr("i%d = 0x%.16lx", k, i[k]);
  }
  s->cr();

  for(k=0; k<sizeof(f)/sizeof(f[0]); k++) {
    s->print_cr("f%d = %f", k, f[k]);
  }
  s->cr();
}

int MacroAssembler::i_offset(unsigned int k) { return (intptr_t)&((MacroAssembler*)0)->i[k]; }
int MacroAssembler::f_offset(unsigned int k) { return (intptr_t)&((MacroAssembler*)0)->f[k]; }

void MacroAssembler::save_registers(MacroAssembler *masm) {
#define __ masm->
  for(int k=0; k<32; k++) {
    __ st_w (as_Register(k), A0, i_offset(k));
  }

  for(int k=0; k<32; k++) {
    __ fst_s (as_FloatRegister(k), A0, f_offset(k));
  }
#undef __
}

void MacroAssembler::restore_registers(MacroAssembler *masm) {
#define __ masm->
  for(int k=0; k<32; k++) {
    __ ld_w (as_Register(k), A0, i_offset(k));
  }

  for(int k=0; k<32; k++) {
    __ fld_s (as_FloatRegister(k), A0, f_offset(k));
  }
#undef __
}


void MacroAssembler::pd_patch_instruction(address branch, address target) {
  jint& stub_inst = *(jint*)branch;
  jint* pc = (jint*)branch;

  if (high(stub_inst, 7) == pcaddu18i_op) {
    // far:
    //   pcaddu18i reg, si20
    //   jirl  r0, reg, si18

    assert(high(pc[1], 6) == jirl_op, "Not a branch label patch");
    jlong offs = target - branch;
    CodeBuffer cb(branch, 2 * BytesPerInstWord);
    MacroAssembler masm(&cb);
    if (reachable_from_branch_short(offs)) {
      // convert far to short
#define __ masm.
      __ b(target);
      __ nop();
#undef __
    } else {
      masm.patchable_jump_far(R0, offs);
    }
    return;
  } else if (high(stub_inst, 7) == pcaddi_op) {
    // see MacroAssembler::set_last_Java_frame:
    //   pcaddi reg, si20

    jint offs = (target - branch) >> 2;
    guarantee(is_simm(offs, 20), "Not signed 20-bit offset");
    CodeBuffer cb(branch, 1 * BytesPerInstWord);
    MacroAssembler masm(&cb);
    masm.pcaddi(as_Register(low(stub_inst, 5)), offs);
    return;
  }

  stub_inst = patched_branch(target - branch, stub_inst, 0);
}

bool MacroAssembler::reachable_from_branch_short(jlong offs) {
  if (ForceUnreachable) {
    return false;
  }
  return is_simm(offs >> 2, 26);
}

void MacroAssembler::patchable_jump_far(Register ra, jlong offs) {
  jint si18, si20;
  guarantee(is_simm(offs, 38), "Not signed 38-bit offset");
  split_simm38(offs, si18, si20);
  pcaddu18i(T4, si20);
  jirl(ra, T4, si18);
}

void MacroAssembler::patchable_jump(address target, bool force_patchable) {
  assert(ReservedCodeCacheSize < 4*G, "branch out of range");
  assert(CodeCache::find_blob(target) != NULL,
         "destination of jump not found in code cache");
  if (force_patchable || patchable_branches()) {
    jlong offs = target - pc();
    if (reachable_from_branch_short(offs)) { // Short jump
      b(offset26(target));
      nop();
    } else {                                 // Far jump
      patchable_jump_far(R0, offs);
    }
  } else {                                   // Real short jump
    b(offset26(target));
  }
}

void MacroAssembler::patchable_call(address target, address call_site) {
  jlong offs = target - (call_site ? call_site : pc());
  if (reachable_from_branch_short(offs - BytesPerInstWord)) { // Short call
    nop();
    bl((offs - BytesPerInstWord) >> 2);
  } else {                                                    // Far call
    patchable_jump_far(RA, offs);
  }
}

// Maybe emit a call via a trampoline.  If the code cache is small
// trampolines won't be emitted.

address MacroAssembler::trampoline_call(AddressLiteral entry, CodeBuffer *cbuf) {
  assert(JavaThread::current()->is_Compiler_thread(), "just checking");
  assert(entry.rspec().type() == relocInfo::runtime_call_type
         || entry.rspec().type() == relocInfo::opt_virtual_call_type
         || entry.rspec().type() == relocInfo::static_call_type
         || entry.rspec().type() == relocInfo::virtual_call_type, "wrong reloc type");

  // We need a trampoline if branches are far.
  if (far_branches()) {
    bool in_scratch_emit_size = false;
#ifdef COMPILER2
    // We don't want to emit a trampoline if C2 is generating dummy
    // code during its branch shortening phase.
    CompileTask* task = ciEnv::current()->task();
    in_scratch_emit_size =
      (task != NULL && is_c2_compile(task->comp_level()) &&
       Compile::current()->in_scratch_emit_size());
#endif
    if (!in_scratch_emit_size) {
      address stub = emit_trampoline_stub(offset(), entry.target());
      if (stub == NULL) {
        return NULL; // CodeCache is full
      }
    }
  }

  if (cbuf) cbuf->set_insts_mark();
  relocate(entry.rspec());
  if (!far_branches()) {
    bl(entry.target());
  } else {
    bl(pc());
  }
  // just need to return a non-null address
  return pc();
}

// Emit a trampoline stub for a call to a target which is too far away.
//
// code sequences:
//
// call-site:
//   branch-and-link to <destination> or <trampoline stub>
//
// Related trampoline stub for this call site in the stub section:
//   load the call target from the constant pool
//   branch (RA still points to the call site above)

address MacroAssembler::emit_trampoline_stub(int insts_call_instruction_offset,
                                             address dest) {
  // Start the stub
  address stub = start_a_stub(NativeInstruction::nop_instruction_size
                   + NativeCallTrampolineStub::instruction_size);
  if (stub == NULL) {
    return NULL;  // CodeBuffer::expand failed
  }

  // Create a trampoline stub relocation which relates this trampoline stub
  // with the call instruction at insts_call_instruction_offset in the
  // instructions code-section.
  align(wordSize);
  relocate(trampoline_stub_Relocation::spec(code()->insts()->start()
                                            + insts_call_instruction_offset));
  const int stub_start_offset = offset();

  // Now, create the trampoline stub's code:
  // - load the call
  // - call
  pcaddi(T4, 0);
  ld_d(T4, T4, 16);
  jr(T4);
  nop();  //align
  assert(offset() - stub_start_offset == NativeCallTrampolineStub::data_offset,
         "should be");
  emit_int64((int64_t)dest);

  const address stub_start_addr = addr_at(stub_start_offset);

  NativeInstruction* ni = nativeInstruction_at(stub_start_addr);
  assert(ni->is_NativeCallTrampolineStub_at(), "doesn't look like a trampoline");

  end_a_stub();
  return stub_start_addr;
}

void MacroAssembler::beq_far(Register rs, Register rt, address entry) {
  if (is_simm16((entry - pc()) >> 2)) { // Short jump
    beq(rs, rt, offset16(entry));
  } else {                              // Far jump
    Label not_jump;
    bne(rs, rt, not_jump);
    b_far(entry);
    bind(not_jump);
  }
}

void MacroAssembler::beq_far(Register rs, Register rt, Label& L) {
  if (L.is_bound()) {
    beq_far(rs, rt, target(L));
  } else {
    Label not_jump;
    bne(rs, rt, not_jump);
    b_far(L);
    bind(not_jump);
  }
}

void MacroAssembler::bne_far(Register rs, Register rt, address entry) {
  if (is_simm16((entry - pc()) >> 2)) { // Short jump
    bne(rs, rt, offset16(entry));
  } else {                              // Far jump
    Label not_jump;
    beq(rs, rt, not_jump);
    b_far(entry);
    bind(not_jump);
  }
}

void MacroAssembler::bne_far(Register rs, Register rt, Label& L) {
  if (L.is_bound()) {
    bne_far(rs, rt, target(L));
  } else {
    Label not_jump;
    beq(rs, rt, not_jump);
    b_far(L);
    bind(not_jump);
  }
}

void MacroAssembler::blt_far(Register rs, Register rt, address entry, bool is_signed) {
  if (is_simm16((entry - pc()) >> 2)) { // Short jump
    if (is_signed) {
      blt(rs, rt, offset16(entry));
    } else {
      bltu(rs, rt, offset16(entry));
    }
  } else {                              // Far jump
    Label not_jump;
    if (is_signed) {
      bge(rs, rt, not_jump);
    } else {
      bgeu(rs, rt, not_jump);
    }
    b_far(entry);
    bind(not_jump);
  }
}

void MacroAssembler::blt_far(Register rs, Register rt, Label& L, bool is_signed) {
  if (L.is_bound()) {
    blt_far(rs, rt, target(L), is_signed);
  } else {
    Label not_jump;
    if (is_signed) {
      bge(rs, rt, not_jump);
    } else {
      bgeu(rs, rt, not_jump);
    }
    b_far(L);
    bind(not_jump);
  }
}

void MacroAssembler::bge_far(Register rs, Register rt, address entry, bool is_signed) {
  if (is_simm16((entry - pc()) >> 2)) { // Short jump
    if (is_signed) {
      bge(rs, rt, offset16(entry));
    } else {
      bgeu(rs, rt, offset16(entry));
    }
  } else {                              // Far jump
    Label not_jump;
    if (is_signed) {
      blt(rs, rt, not_jump);
    } else {
      bltu(rs, rt, not_jump);
    }
    b_far(entry);
    bind(not_jump);
  }
}

void MacroAssembler::bge_far(Register rs, Register rt, Label& L, bool is_signed) {
  if (L.is_bound()) {
    bge_far(rs, rt, target(L), is_signed);
  } else {
    Label not_jump;
    if (is_signed) {
      blt(rs, rt, not_jump);
    } else {
      bltu(rs, rt, not_jump);
    }
    b_far(L);
    bind(not_jump);
  }
}

void MacroAssembler::beq_long(Register rs, Register rt, Label& L) {
  Label not_taken;
  bne(rs, rt, not_taken);
  jmp_far(L);
  bind(not_taken);
}

void MacroAssembler::bne_long(Register rs, Register rt, Label& L) {
  Label not_taken;
  beq(rs, rt, not_taken);
  jmp_far(L);
  bind(not_taken);
}

void MacroAssembler::blt_long(Register rs, Register rt, Label& L, bool is_signed) {
  Label not_taken;
  if (is_signed) {
    bge(rs, rt, not_taken);
  } else {
    bgeu(rs, rt, not_taken);
  }
  jmp_far(L);
  bind(not_taken);
}

void MacroAssembler::bge_long(Register rs, Register rt, Label& L, bool is_signed) {
  Label not_taken;
  if (is_signed) {
    blt(rs, rt, not_taken);
  } else {
    bltu(rs, rt, not_taken);
  }
  jmp_far(L);
  bind(not_taken);
}

void MacroAssembler::bc1t_long(Label& L) {
  Label not_taken;
  bceqz(FCC0, not_taken);
  jmp_far(L);
  bind(not_taken);
}

void MacroAssembler::bc1f_long(Label& L) {
  Label not_taken;
  bcnez(FCC0, not_taken);
  jmp_far(L);
  bind(not_taken);
}

void MacroAssembler::b_far(Label& L) {
  if (L.is_bound()) {
    b_far(target(L));
  } else {
    L.add_patch_at(code(), locator());
    if (ForceUnreachable) {
      patchable_jump_far(R0, 0);
    } else {
      b(0);
    }
  }
}

void MacroAssembler::b_far(address entry) {
  jlong offs = entry - pc();
  if (reachable_from_branch_short(offs)) { // Short jump
    b(offset26(entry));
  } else {                                 // Far jump
    patchable_jump_far(R0, offs);
  }
}

void MacroAssembler::ld_ptr(Register rt, Register base, Register offset) {
  ldx_d(rt, base, offset);
}

void MacroAssembler::st_ptr(Register rt, Register base, Register offset) {
  stx_d(rt, base, offset);
}

void MacroAssembler::ld_long(Register rt, Register offset, Register base) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
#if 0
  add_d(AT, base, offset);
  ld_long(rt, 0, AT);
#endif
}

void MacroAssembler::st_long(Register rt, Register offset, Register base) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
#if 0
  add_d(AT, base, offset);
  st_long(rt, 0, AT);
#endif
}

Address MacroAssembler::as_Address(AddressLiteral adr) {
  return Address(adr.target(), adr.rspec());
}

Address MacroAssembler::as_Address(ArrayAddress adr) {
  return Address::make_array(adr);
}

// tmp_reg1 and tmp_reg2 should be saved outside of atomic_inc32 (caller saved).
void MacroAssembler::atomic_inc32(address counter_addr, int inc, Register tmp_reg1, Register tmp_reg2) {
  li(tmp_reg1, inc);
  li(tmp_reg2, counter_addr);
  amadd_w(R0, tmp_reg1, tmp_reg2);
}

int MacroAssembler::biased_locking_enter(Register lock_reg,
                                         Register obj_reg,
                                         Register swap_reg,
                                         Register tmp_reg,
                                         bool swap_reg_contains_mark,
                                         Label& done,
                                         Label* slow_case,
                                         BiasedLockingCounters* counters) {
  assert(UseBiasedLocking, "why call this otherwise?");
  bool need_tmp_reg = false;
  if (tmp_reg == noreg) {
    need_tmp_reg = true;
    tmp_reg = T4;
  }
  assert_different_registers(lock_reg, obj_reg, swap_reg, tmp_reg, AT);
  assert(markOopDesc::age_shift == markOopDesc::lock_bits + markOopDesc::biased_lock_bits, "biased locking makes assumptions about bit layout");
  Address mark_addr      (obj_reg, oopDesc::mark_offset_in_bytes());
  Address saved_mark_addr(lock_reg, 0);

  // Biased locking
  // See whether the lock is currently biased toward our thread and
  // whether the epoch is still valid
  // Note that the runtime guarantees sufficient alignment of JavaThread
  // pointers to allow age to be placed into low bits
  // First check to see whether biasing is even enabled for this object
  Label cas_label;
  int null_check_offset = -1;
  if (!swap_reg_contains_mark) {
    null_check_offset = offset();
    ld_ptr(swap_reg, mark_addr);
  }

  if (need_tmp_reg) {
    push(tmp_reg);
  }
  move(tmp_reg, swap_reg);
  andi(tmp_reg, tmp_reg, markOopDesc::biased_lock_mask_in_place);
  addi_d(AT, R0, markOopDesc::biased_lock_pattern);
  sub_d(AT, AT, tmp_reg);
  if (need_tmp_reg) {
    pop(tmp_reg);
  }

  bne(AT, R0, cas_label);


  // The bias pattern is present in the object's header. Need to check
  // whether the bias owner and the epoch are both still current.
  // Note that because there is no current thread register on LA we
  // need to store off the mark word we read out of the object to
  // avoid reloading it and needing to recheck invariants below. This
  // store is unfortunate but it makes the overall code shorter and
  // simpler.
  st_ptr(swap_reg, saved_mark_addr);
  if (need_tmp_reg) {
    push(tmp_reg);
  }
  if (swap_reg_contains_mark) {
    null_check_offset = offset();
  }
  load_prototype_header(tmp_reg, obj_reg);
  xorr(tmp_reg, tmp_reg, swap_reg);
  get_thread(swap_reg);
  xorr(swap_reg, swap_reg, tmp_reg);

  li(AT, ~((int) markOopDesc::age_mask_in_place));
  andr(swap_reg, swap_reg, AT);

  if (PrintBiasedLockingStatistics) {
    Label L;
    bne(swap_reg, R0, L);
    push(tmp_reg);
    push(A0);
    atomic_inc32((address)BiasedLocking::biased_lock_entry_count_addr(), 1, A0, tmp_reg);
    pop(A0);
    pop(tmp_reg);
    bind(L);
  }
  if (need_tmp_reg) {
    pop(tmp_reg);
  }
  beq(swap_reg, R0, done);
  Label try_revoke_bias;
  Label try_rebias;

  // At this point we know that the header has the bias pattern and
  // that we are not the bias owner in the current epoch. We need to
  // figure out more details about the state of the header in order to
  // know what operations can be legally performed on the object's
  // header.

  // If the low three bits in the xor result aren't clear, that means
  // the prototype header is no longer biased and we have to revoke
  // the bias on this object.

  li(AT, markOopDesc::biased_lock_mask_in_place);
  andr(AT, swap_reg, AT);
  bne(AT, R0, try_revoke_bias);
  // Biasing is still enabled for this data type. See whether the
  // epoch of the current bias is still valid, meaning that the epoch
  // bits of the mark word are equal to the epoch bits of the
  // prototype header. (Note that the prototype header's epoch bits
  // only change at a safepoint.) If not, attempt to rebias the object
  // toward the current thread. Note that we must be absolutely sure
  // that the current epoch is invalid in order to do this because
  // otherwise the manipulations it performs on the mark word are
  // illegal.

  li(AT, markOopDesc::epoch_mask_in_place);
  andr(AT,swap_reg, AT);
  bne(AT, R0, try_rebias);
  // The epoch of the current bias is still valid but we know nothing
  // about the owner; it might be set or it might be clear. Try to
  // acquire the bias of the object using an atomic operation. If this
  // fails we will go in to the runtime to revoke the object's bias.
  // Note that we first construct the presumed unbiased header so we
  // don't accidentally blow away another thread's valid bias.

  ld_ptr(swap_reg, saved_mark_addr);

  li(AT, markOopDesc::biased_lock_mask_in_place | markOopDesc::age_mask_in_place | markOopDesc::epoch_mask_in_place);
  andr(swap_reg, swap_reg, AT);

  if (need_tmp_reg) {
    push(tmp_reg);
  }
  get_thread(tmp_reg);
  orr(tmp_reg, tmp_reg, swap_reg);
  cmpxchg(Address(obj_reg, 0), swap_reg, tmp_reg, AT, false, false);
  if (need_tmp_reg) {
    pop(tmp_reg);
  }
  // If the biasing toward our thread failed, this means that
  // another thread succeeded in biasing it toward itself and we
  // need to revoke that bias. The revocation will occur in the
  // interpreter runtime in the slow case.
  if (PrintBiasedLockingStatistics) {
    Label L;
    bne(AT, R0, L);
    push(tmp_reg);
    push(A0);
    atomic_inc32((address)BiasedLocking::anonymously_biased_lock_entry_count_addr(), 1, A0, tmp_reg);
    pop(A0);
    pop(tmp_reg);
    bind(L);
  }
  if (slow_case != NULL) {
    beq_far(AT, R0, *slow_case);
  }
  b(done);

  bind(try_rebias);
  // At this point we know the epoch has expired, meaning that the
  // current "bias owner", if any, is actually invalid. Under these
  // circumstances _only_, we are allowed to use the current header's
  // value as the comparison value when doing the cas to acquire the
  // bias in the current epoch. In other words, we allow transfer of
  // the bias from one thread to another directly in this situation.
  //
  // FIXME: due to a lack of registers we currently blow away the age
  // bits in this situation. Should attempt to preserve them.
  if (need_tmp_reg) {
    push(tmp_reg);
  }
  load_prototype_header(tmp_reg, obj_reg);
  get_thread(swap_reg);
  orr(tmp_reg, tmp_reg, swap_reg);
  ld_ptr(swap_reg, saved_mark_addr);

  cmpxchg(Address(obj_reg, 0), swap_reg, tmp_reg, AT, false, false);
  if (need_tmp_reg) {
    pop(tmp_reg);
  }
  // If the biasing toward our thread failed, then another thread
  // succeeded in biasing it toward itself and we need to revoke that
  // bias. The revocation will occur in the runtime in the slow case.
  if (PrintBiasedLockingStatistics) {
    Label L;
    bne(AT, R0, L);
    push(AT);
    push(tmp_reg);
    atomic_inc32((address)BiasedLocking::rebiased_lock_entry_count_addr(), 1, AT, tmp_reg);
    pop(tmp_reg);
    pop(AT);
    bind(L);
  }
  if (slow_case != NULL) {
    beq_far(AT, R0, *slow_case);
  }

  b(done);
  bind(try_revoke_bias);
  // The prototype mark in the klass doesn't have the bias bit set any
  // more, indicating that objects of this data type are not supposed
  // to be biased any more. We are going to try to reset the mark of
  // this object to the prototype value and fall through to the
  // CAS-based locking scheme. Note that if our CAS fails, it means
  // that another thread raced us for the privilege of revoking the
  // bias of this particular object, so it's okay to continue in the
  // normal locking code.
  //
  // FIXME: due to a lack of registers we currently blow away the age
  // bits in this situation. Should attempt to preserve them.
  ld_ptr(swap_reg, saved_mark_addr);

  if (need_tmp_reg) {
    push(tmp_reg);
  }
  load_prototype_header(tmp_reg, obj_reg);
  cmpxchg(Address(obj_reg, 0), swap_reg, tmp_reg, AT, false, false);
  if (need_tmp_reg) {
    pop(tmp_reg);
  }
  // Fall through to the normal CAS-based lock, because no matter what
  // the result of the above CAS, some thread must have succeeded in
  // removing the bias bit from the object's header.
  if (PrintBiasedLockingStatistics) {
    Label L;
    bne(AT, R0, L);
    push(AT);
    push(tmp_reg);
    atomic_inc32((address)BiasedLocking::revoked_lock_entry_count_addr(), 1, AT, tmp_reg);
    pop(tmp_reg);
    pop(AT);
    bind(L);
  }

  bind(cas_label);
  return null_check_offset;
}

void MacroAssembler::biased_locking_exit(Register obj_reg, Register temp_reg, Label& done) {
  assert(UseBiasedLocking, "why call this otherwise?");

  // Check for biased locking unlock case, which is a no-op
  // Note: we do not have to check the thread ID for two reasons.
  // First, the interpreter checks for IllegalMonitorStateException at
  // a higher level. Second, if the bias was revoked while we held the
  // lock, the object could not be rebiased toward another thread, so
  // the bias bit would be clear.
  ld_d(temp_reg, Address(obj_reg, oopDesc::mark_offset_in_bytes()));
  andi(temp_reg, temp_reg, markOopDesc::biased_lock_mask_in_place);
  addi_d(AT, R0, markOopDesc::biased_lock_pattern);

  beq(AT, temp_reg, done);
}

// the stack pointer adjustment is needed. see InterpreterMacroAssembler::super_call_VM_leaf
// this method will handle the stack problem, you need not to preserve the stack space for the argument now
void MacroAssembler::call_VM_leaf_base(address entry_point, int number_of_arguments) {
  Label L, E;

  assert(number_of_arguments <= 4, "just check");

  andi(AT, SP, 0xf);
  beq(AT, R0, L);
  addi_d(SP, SP, -8);
  call(entry_point, relocInfo::runtime_call_type);
  addi_d(SP, SP, 8);
  b(E);

  bind(L);
  call(entry_point, relocInfo::runtime_call_type);
  bind(E);
}


void MacroAssembler::jmp(address entry) {
  jlong offs = entry - pc();
  if (reachable_from_branch_short(offs)) { // Short jump
    b(offset26(entry));
  } else {                                 // Far jump
    patchable_jump_far(R0, offs);
  }
}

void MacroAssembler::jmp(address entry, relocInfo::relocType rtype) {
  switch (rtype) {
    case relocInfo::none:
      jmp(entry);
      break;
    default:
      {
        InstructionMark im(this);
        relocate(rtype);
        patchable_jump(entry);
      }
      break;
  }
}

void MacroAssembler::jmp_far(Label& L) {
  if (L.is_bound()) {
    assert(target(L) != NULL, "jmp most probably wrong");
    patchable_jump(target(L), true /* force patchable */);
  } else {
    L.add_patch_at(code(), locator());
    patchable_jump_far(R0, 0);
  }
}

void MacroAssembler::mov_metadata(Address dst, Metadata* obj) {
  int oop_index;
  if (obj) {
    oop_index = oop_recorder()->find_index(obj);
  } else {
    oop_index = oop_recorder()->allocate_metadata_index(obj);
  }
  relocate(metadata_Relocation::spec(oop_index));
  patchable_li52(AT, (long)obj);
  st_d(AT, dst);
}

void MacroAssembler::mov_metadata(Register dst, Metadata* obj) {
  int oop_index;
  if (obj) {
    oop_index = oop_recorder()->find_index(obj);
  } else {
    oop_index = oop_recorder()->allocate_metadata_index(obj);
  }
  relocate(metadata_Relocation::spec(oop_index));
  patchable_li52(dst, (long)obj);
}

void MacroAssembler::call(address entry) {
  jlong offs = entry - pc();
  if (reachable_from_branch_short(offs)) { // Short call (pc-rel)
    bl(offset26(entry));
  } else if (is_simm(offs, 38)) {          // Far call (pc-rel)
    patchable_jump_far(RA, offs);
  } else {                                 // Long call (absolute)
    call_long(entry);
  }
}

void MacroAssembler::call(address entry, relocInfo::relocType rtype) {
  switch (rtype) {
    case relocInfo::none:
      call(entry);
      break;
    case relocInfo::runtime_call_type:
      if (!is_simm(entry - pc(), 38)) {
        call_long(entry);
        break;
      }
      // fallthrough
    default:
      {
        InstructionMark im(this);
        relocate(rtype);
        patchable_call(entry);
      }
      break;
  }
}

void MacroAssembler::call(address entry, RelocationHolder& rh) {
  switch (rh.type()) {
    case relocInfo::none:
      call(entry);
      break;
    case relocInfo::runtime_call_type:
      if (!is_simm(entry - pc(), 38)) {
        call_long(entry);
        break;
      }
      // fallthrough
    default:
      {
        InstructionMark im(this);
        relocate(rh);
        patchable_call(entry);
      }
      break;
  }
}

void MacroAssembler::call_long(address entry) {
  jlong value = (jlong)entry;
  lu12i_w(T4, split_low20(value >> 12));
  lu32i_d(T4, split_low20(value >> 32));
  jirl(RA, T4, split_low12(value));
}

address MacroAssembler::ic_call(address entry) {
  RelocationHolder rh = virtual_call_Relocation::spec(pc());
  patchable_li52(IC_Klass, (long)Universe::non_oop_word());
  assert(entry != NULL, "call most probably wrong");
  InstructionMark im(this);
  return trampoline_call(AddressLiteral(entry, rh));
}

void MacroAssembler::c2bool(Register r) {
  sltu(r, R0, r);
}

#ifndef PRODUCT
extern "C" void findpc(intptr_t x);
#endif

void MacroAssembler::debug(char* msg/*, RegistersForDebugging* regs*/) {
  if ( ShowMessageBoxOnError ) {
    JavaThreadState saved_state = JavaThread::current()->thread_state();
    JavaThread::current()->set_thread_state(_thread_in_vm);
    {
      // In order to get locks work, we need to fake a in_VM state
      ttyLocker ttyl;
      ::tty->print_cr("EXECUTION STOPPED: %s\n", msg);
      if (CountBytecodes || TraceBytecodes || StopInterpreterAt) {
  BytecodeCounter::print();
      }

    }
    ThreadStateTransition::transition(JavaThread::current(), _thread_in_vm, saved_state);
  }
  else
    ::tty->print_cr("=============== DEBUG MESSAGE: %s ================\n", msg);
}


void MacroAssembler::stop(const char* msg) {
  li(A0, (long)msg);
  call(CAST_FROM_FN_PTR(address, MacroAssembler::debug), relocInfo::runtime_call_type);
  brk(17);
}

void MacroAssembler::warn(const char* msg) {
  pushad();
  li(A0, (long)msg);
  push(S2);
  li(AT, -(StackAlignmentInBytes));
  move(S2, SP);     // use S2 as a sender SP holder
  andr(SP, SP, AT); // align stack as required by ABI
  call(CAST_FROM_FN_PTR(address, MacroAssembler::debug), relocInfo::runtime_call_type);
  move(SP, S2);     // use S2 as a sender SP holder
  pop(S2);
  popad();
}

void MacroAssembler::increment(Register reg, int imm) {
  if (!imm) return;
  if (is_simm(imm, 12)) {
    addi_d(reg, reg, imm);
  } else {
    li(AT, imm);
    add_d(reg, reg, AT);
  }
}

void MacroAssembler::decrement(Register reg, int imm) {
  increment(reg, -imm);
}

void MacroAssembler::increment(Address addr, int imm) {
  if (!imm) return;
  assert(is_simm(imm, 12), "must be");
  ld_ptr(AT, addr);
  addi_d(AT, AT, imm);
  st_ptr(AT, addr);
}

void MacroAssembler::decrement(Address addr, int imm) {
  increment(addr, -imm);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             bool check_exceptions) {
  call_VM_helper(oop_result, entry_point, 0, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             bool check_exceptions) {
  if (arg_1!=A1) move(A1, arg_1);
  call_VM_helper(oop_result, entry_point, 1, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             bool check_exceptions) {
  if (arg_1!=A1) move(A1, arg_1);
  if (arg_2!=A2) move(A2, arg_2);
  assert(arg_2 != A1, "smashed argument");
  call_VM_helper(oop_result, entry_point, 2, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             Register arg_3,
                             bool check_exceptions) {
  if (arg_1!=A1) move(A1, arg_1);
  if (arg_2!=A2) move(A2, arg_2); assert(arg_2 != A1, "smashed argument");
  if (arg_3!=A3) move(A3, arg_3); assert(arg_3 != A1 && arg_3 != A2, "smashed argument");
  call_VM_helper(oop_result, entry_point, 3, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             int number_of_arguments,
                             bool check_exceptions) {
  call_VM_base(oop_result, NOREG, last_java_sp, entry_point, number_of_arguments, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             bool check_exceptions) {
  if (arg_1 != A1) move(A1, arg_1);
  call_VM(oop_result, last_java_sp, entry_point, 1, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             bool check_exceptions) {
  if (arg_1 != A1) move(A1, arg_1);
  if (arg_2 != A2) move(A2, arg_2); assert(arg_2 != A1, "smashed argument");
  call_VM(oop_result, last_java_sp, entry_point, 2, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             Register arg_3,
                             bool check_exceptions) {
  if (arg_1 != A1) move(A1, arg_1);
  if (arg_2 != A2) move(A2, arg_2); assert(arg_2 != A1, "smashed argument");
  if (arg_3 != A3) move(A3, arg_3); assert(arg_3 != A1 && arg_3 != A2, "smashed argument");
  call_VM(oop_result, last_java_sp, entry_point, 3, check_exceptions);
}

void MacroAssembler::call_VM_base(Register oop_result,
                                  Register java_thread,
                                  Register last_java_sp,
                                  address  entry_point,
                                  int      number_of_arguments,
                                  bool     check_exceptions) {
  // determine java_thread register
  if (!java_thread->is_valid()) {
#ifndef OPT_THREAD
    java_thread = T2;
    get_thread(java_thread);
#else
    java_thread = TREG;
#endif
  }
  // determine last_java_sp register
  if (!last_java_sp->is_valid()) {
    last_java_sp = SP;
  }
  // debugging support
  assert(number_of_arguments >= 0   , "cannot have negative number of arguments");
  assert(number_of_arguments <= 4   , "cannot have negative number of arguments");
  assert(java_thread != oop_result  , "cannot use the same register for java_thread & oop_result");
  assert(java_thread != last_java_sp, "cannot use the same register for java_thread & last_java_sp");

  assert(last_java_sp != FP, "this code doesn't work for last_java_sp == fp, which currently can't portably work anyway since C2 doesn't save fp");

  // set last Java frame before call
  Label before_call;
  bind(before_call);
  set_last_Java_frame(java_thread, last_java_sp, FP, before_call);

  // do the call
  move(A0, java_thread);
  call(entry_point, relocInfo::runtime_call_type);

  // restore the thread (cannot use the pushed argument since arguments
  // may be overwritten by C code generated by an optimizing compiler);
  // however can use the register value directly if it is callee saved.
#ifndef OPT_THREAD
  get_thread(java_thread);
#else
#ifdef ASSERT
  {
    Label L;
    get_thread(AT);
    beq(java_thread, AT, L);
    stop("MacroAssembler::call_VM_base: TREG not callee saved?");
    bind(L);
  }
#endif
#endif

  // discard thread and arguments
  ld_ptr(SP, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
  // reset last Java frame
  reset_last_Java_frame(java_thread, false);

  check_and_handle_popframe(java_thread);
  check_and_handle_earlyret(java_thread);
  if (check_exceptions) {
    // check for pending exceptions (java_thread is set upon return)
    Label L;
    ld_d(AT, java_thread, in_bytes(Thread::pending_exception_offset()));
    beq(AT, R0, L);
    li(AT, target(before_call));
    push(AT);
    jmp(StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);
    bind(L);
  }

  // get oop result if there is one and reset the value in the thread
  if (oop_result->is_valid()) {
    ld_d(oop_result, java_thread, in_bytes(JavaThread::vm_result_offset()));
    st_d(R0, java_thread, in_bytes(JavaThread::vm_result_offset()));
    verify_oop(oop_result);
  }
}

void MacroAssembler::call_VM_helper(Register oop_result, address entry_point, int number_of_arguments, bool check_exceptions) {
  move(V0, SP);
  //we also reserve space for java_thread here
  li(AT, -(StackAlignmentInBytes));
  andr(SP, SP, AT);
  call_VM_base(oop_result, NOREG, V0, entry_point, number_of_arguments, check_exceptions);
}

void MacroAssembler::call_VM_leaf(address entry_point, int number_of_arguments) {
  call_VM_leaf_base(entry_point, number_of_arguments);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0) {
  if (arg_0 != A0) move(A0, arg_0);
  call_VM_leaf(entry_point, 1);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0, Register arg_1) {
  if (arg_0 != A0) move(A0, arg_0);
  if (arg_1 != A1) move(A1, arg_1); assert(arg_1 != A0, "smashed argument");
  call_VM_leaf(entry_point, 2);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0, Register arg_1, Register arg_2) {
  if (arg_0 != A0) move(A0, arg_0);
  if (arg_1 != A1) move(A1, arg_1); assert(arg_1 != A0, "smashed argument");
  if (arg_2 != A2) move(A2, arg_2); assert(arg_2 != A0 && arg_2 != A1, "smashed argument");
  call_VM_leaf(entry_point, 3);
}

void MacroAssembler::super_call_VM_leaf(address entry_point) {
  MacroAssembler::call_VM_leaf_base(entry_point, 0);
}

void MacroAssembler::super_call_VM_leaf(address entry_point,
                                                   Register arg_1) {
  if (arg_1 != A0) move(A0, arg_1);
  MacroAssembler::call_VM_leaf_base(entry_point, 1);
}

void MacroAssembler::super_call_VM_leaf(address entry_point,
                                                   Register arg_1,
                                                   Register arg_2) {
  if (arg_1 != A0) move(A0, arg_1);
  if (arg_2 != A1) move(A1, arg_2); assert(arg_2 != A0, "smashed argument");
  MacroAssembler::call_VM_leaf_base(entry_point, 2);
}

void MacroAssembler::super_call_VM_leaf(address entry_point,
                                                   Register arg_1,
                                                   Register arg_2,
                                                   Register arg_3) {
  if (arg_1 != A0) move(A0, arg_1);
  if (arg_2 != A1) move(A1, arg_2); assert(arg_2 != A0, "smashed argument");
  if (arg_3 != A2) move(A2, arg_3); assert(arg_3 != A0 && arg_3 != A1, "smashed argument");
  MacroAssembler::call_VM_leaf_base(entry_point, 3);
}

void MacroAssembler::check_and_handle_earlyret(Register java_thread) {
}

void MacroAssembler::check_and_handle_popframe(Register java_thread) {
}

void MacroAssembler::null_check(Register reg, int offset) {
  if (needs_explicit_null_check(offset)) {
    // provoke OS NULL exception if reg = NULL by
    // accessing M[reg] w/o changing any (non-CC) registers
    // NOTE: cmpl is plenty here to provoke a segv
    ld_w(AT, reg, 0);
  } else {
    // nothing to do, (later) access of M[reg + offset]
    // will provoke OS NULL exception if reg = NULL
  }
}

void MacroAssembler::enter() {
  push2(RA, FP);
  move(FP, SP);
}

void MacroAssembler::leave() {
  move(SP, FP);
  pop2(RA, FP);
}

void MacroAssembler::build_frame(int framesize) {
  assert(framesize >= 2 * wordSize, "framesize must include space for FP/RA");
  assert(framesize % (2 * wordSize) == 0, "must preserve 2 * wordSize alignment");
  if (Assembler::is_simm(-framesize, 12)) {
    addi_d(SP, SP, -framesize);
    st_ptr(FP, Address(SP, framesize - 2 * wordSize));
    st_ptr(RA, Address(SP, framesize - 1 * wordSize));
    if (PreserveFramePointer)
      addi_d(FP, SP, framesize - 2 * wordSize);
  } else {
    addi_d(SP, SP, -2 * wordSize);
    st_ptr(FP, Address(SP, 0 * wordSize));
    st_ptr(RA, Address(SP, 1 * wordSize));
    if (PreserveFramePointer)
      move(FP, SP);
    li(SCR1, framesize - 2 * wordSize);
    sub_d(SP, SP, SCR1);
  }
}

void MacroAssembler::remove_frame(int framesize) {
  assert(framesize >= 2 * wordSize, "framesize must include space for FP/RA");
  assert(framesize % (2*wordSize) == 0, "must preserve 2*wordSize alignment");
  if (Assembler::is_simm(framesize, 12)) {
    ld_ptr(FP, Address(SP, framesize - 2 * wordSize));
    ld_ptr(RA, Address(SP, framesize - 1 * wordSize));
    addi_d(SP, SP, framesize);
  } else {
    li(SCR1, framesize - 2 * wordSize);
    add_d(SP, SP, SCR1);
    ld_ptr(FP, Address(SP, 0 * wordSize));
    ld_ptr(RA, Address(SP, 1 * wordSize));
    addi_d(SP, SP, 2 * wordSize);
  }
}

void MacroAssembler::reset_last_Java_frame(Register java_thread, bool clear_fp) {
  // determine java_thread register
  if (!java_thread->is_valid()) {
#ifndef OPT_THREAD
    java_thread = T1;
    get_thread(java_thread);
#else
    java_thread = TREG;
#endif
  }
  // we must set sp to zero to clear frame
  st_ptr(R0, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
  // must clear fp, so that compiled frames are not confused; it is possible
  // that we need it only for debugging
  if(clear_fp) {
    st_ptr(R0, java_thread, in_bytes(JavaThread::last_Java_fp_offset()));
  }

  // Always clear the pc because it could have been set by make_walkable()
  st_ptr(R0, java_thread, in_bytes(JavaThread::last_Java_pc_offset()));
}

void MacroAssembler::reset_last_Java_frame(bool clear_fp) {
  Register thread = TREG;
#ifndef OPT_THREAD
  get_thread(thread);
#endif
  // we must set sp to zero to clear frame
  st_d(R0, thread, in_bytes(JavaThread::last_Java_sp_offset()));
  // must clear fp, so that compiled frames are not confused; it is
  // possible that we need it only for debugging
  if (clear_fp) {
    st_d(R0, thread, in_bytes(JavaThread::last_Java_fp_offset()));
  }

  // Always clear the pc because it could have been set by make_walkable()
  st_d(R0, thread, in_bytes(JavaThread::last_Java_pc_offset()));
}

// Write serialization page so VM thread can do a pseudo remote membar.
// We use the current thread pointer to calculate a thread specific
// offset to write to within the page. This minimizes bus traffic
// due to cache line collision.
void MacroAssembler::serialize_memory(Register thread, Register tmp) {
  assert_different_registers(AT, tmp);
  juint sps = os::get_serialize_page_shift_count();
  juint lsb = sps + 2;
  juint msb = sps + log2_uint(os::vm_page_size()) - 1;
  bstrpick_w(AT, thread, msb, lsb);
  li(tmp, os::get_memory_serialize_page());
  alsl_d(tmp, AT, tmp, Address::times_2 - 1);
  st_w(R0, tmp, 0);
}

// Calls to C land
//
// When entering C land, the fp, & sp of the last Java frame have to be recorded
// in the (thread-local) JavaThread object. When leaving C land, the last Java fp
// has to be reset to 0. This is required to allow proper stack traversal.
void MacroAssembler::set_last_Java_frame(Register java_thread,
                                         Register last_java_sp,
                                         Register last_java_fp,
                                         Label& last_java_pc) {
  // determine java_thread register
  if (!java_thread->is_valid()) {
#ifndef OPT_THREAD
    java_thread = T2;
    get_thread(java_thread);
#else
    java_thread = TREG;
#endif
  }

  // determine last_java_sp register
  if (!last_java_sp->is_valid()) {
    last_java_sp = SP;
  }

  // last_java_fp is optional
  if (last_java_fp->is_valid()) {
    st_ptr(last_java_fp, java_thread, in_bytes(JavaThread::last_Java_fp_offset()));
  }

  // last_java_pc
  lipc(AT, last_java_pc);
  st_ptr(AT, java_thread, in_bytes(JavaThread::frame_anchor_offset() +
                                   JavaFrameAnchor::last_Java_pc_offset()));

  st_ptr(last_java_sp, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
}

void MacroAssembler::set_last_Java_frame(Register last_java_sp,
                                         Register last_java_fp,
                                         Label& last_java_pc) {
  set_last_Java_frame(NOREG, last_java_sp, last_java_fp, last_java_pc);
}

//////////////////////////////////////////////////////////////////////////////////
#if INCLUDE_ALL_GCS

void MacroAssembler::g1_write_barrier_pre(Register obj,
                                          Register pre_val,
                                          Register thread,
                                          Register tmp,
                                          bool tosca_live,
                                          bool expand_call) {

  // If expand_call is true then we expand the call_VM_leaf macro
  // directly to skip generating the check by
  // InterpreterMacroAssembler::call_VM_leaf_base that checks _last_sp.

  assert(thread == TREG, "must be");

  Label done;
  Label runtime;

  assert(pre_val != noreg, "check this code");

  if (obj != noreg) {
    assert_different_registers(obj, pre_val, tmp);
    assert(pre_val != V0, "check this code");
  }

  Address in_progress(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                       PtrQueue::byte_offset_of_active()));
  Address index(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                       PtrQueue::byte_offset_of_index()));
  Address buffer(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                       PtrQueue::byte_offset_of_buf()));

  // Is marking active?
  if (in_bytes(PtrQueue::byte_width_of_active()) == 4) {
    ld_w(AT, in_progress);
  } else {
    assert(in_bytes(PtrQueue::byte_width_of_active()) == 1, "Assumption");
    ld_b(AT, in_progress);
  }
  beqz(AT, done);

  // Do we need to load the previous value?
  if (obj != noreg) {
    load_heap_oop(pre_val, Address(obj, 0));
  }

  // Is the previous value null?
  beqz(pre_val, done);

  // Can we store original value in the thread's buffer?
  // Is index == 0?
  // (The index field is typed as size_t.)

  ld_d(tmp, index);
  beqz(tmp, runtime);

  addi_d(tmp, tmp, -1 * wordSize);
  st_d(tmp, index);
  ld_d(AT, buffer);

  // Record the previous value
  stx_d(pre_val, tmp, AT);
  b(done);

  bind(runtime);
  // save the live input values
  if (tosca_live) push(V0);

  if (obj != noreg && obj != V0) push(obj);

  if (pre_val != V0) push(pre_val);

  // Calling the runtime using the regular call_VM_leaf mechanism generates
  // code (generated by InterpreterMacroAssember::call_VM_leaf_base)
  // that checks that the *(fp+frame::interpreter_frame_last_sp) == NULL.
  //
  // If we care generating the pre-barrier without a frame (e.g. in the
  // intrinsified Reference.get() routine) then fp might be pointing to
  // the caller frame and so this check will most likely fail at runtime.
  //
  // Expanding the call directly bypasses the generation of the check.
  // So when we do not have have a full interpreter frame on the stack
  // expand_call should be passed true.

  if (expand_call) {
    assert(pre_val != A1, "smashed arg");
    if (thread != A1) move(A1, thread);
    if (pre_val != A0) move(A0, pre_val);
    MacroAssembler::call_VM_leaf_base(CAST_FROM_FN_PTR(address, SharedRuntime::g1_wb_pre), 2);
  } else {
    call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::g1_wb_pre), pre_val, thread);
  }

  // save the live input values
  if (pre_val != V0)
    pop(pre_val);

  if (obj != noreg && obj != V0)
    pop(obj);

  if(tosca_live) pop(V0);

  bind(done);
}

void MacroAssembler::g1_write_barrier_post(Register store_addr,
                                           Register new_val,
                                           Register thread,
                                           Register tmp,
                                           Register tmp2) {
  assert(tmp  != AT, "must be");
  assert(tmp2 != AT, "must be");
  assert(thread == TREG, "must be");

  Address queue_index(thread, in_bytes(JavaThread::dirty_card_queue_offset() +
                                       PtrQueue::byte_offset_of_index()));
  Address buffer(thread, in_bytes(JavaThread::dirty_card_queue_offset() +
                                       PtrQueue::byte_offset_of_buf()));

  BarrierSet* bs = Universe::heap()->barrier_set();
  CardTableModRefBS* ct = (CardTableModRefBS*)bs;
  assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");
  Label done;
  Label runtime;

  // Does store cross heap regions?
  xorr(AT, store_addr, new_val);
  srli_d(AT, AT, HeapRegion::LogOfHRGrainBytes);
  beqz(AT, done);


  // crosses regions, storing NULL?
  beq(new_val, R0, done);

  // storing region crossing non-NULL, is card already dirty?
  const Register card_addr = tmp;
  const Register cardtable = tmp2;

  move(card_addr, store_addr);
  srli_d(card_addr, card_addr, CardTableModRefBS::card_shift);
  // Do not use ExternalAddress to load 'byte_map_base', since 'byte_map_base' is NOT
  // a valid address and therefore is not properly handled by the relocation code.
  li(cardtable, (intptr_t)ct->byte_map_base);
  add_d(card_addr, card_addr, cardtable);

  ld_b(AT, card_addr, 0);
  addi_d(AT, AT, -1 * (int)G1SATBCardTableModRefBS::g1_young_card_val());
  beqz(AT, done);

  membar(StoreLoad);
  ld_b(AT, card_addr, 0);
  addi_d(AT, AT, -1 * (int)(int)CardTableModRefBS::dirty_card_val());
  beqz(AT, done);


  // storing a region crossing, non-NULL oop, card is clean.
  // dirty card and log.
  li(AT, (int)CardTableModRefBS::dirty_card_val());
  st_b(AT, card_addr, 0);

  ld_w(AT, queue_index);
  beqz(AT, runtime);
  addi_d(AT, AT, -1 * wordSize);
  st_w(AT, queue_index);
  ld_d(tmp2, buffer);
  ld_d(AT, queue_index);
  stx_d(card_addr, tmp2, AT);
  b(done);

  bind(runtime);
  // save the live input values
  push(store_addr);
  push(new_val);
  call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::g1_wb_post), card_addr, TREG);
  pop(new_val);
  pop(store_addr);

  bind(done);
}

#endif // INCLUDE_ALL_GCS
//////////////////////////////////////////////////////////////////////////////////


void MacroAssembler::store_check(Register obj) {
  // Does a store check for the oop in register obj. The content of
  // register obj is destroyed afterwards.
  store_check_part_1(obj);
  store_check_part_2(obj);
}

void MacroAssembler::store_check(Register obj, Address dst) {
  store_check(obj);
}


// split the store check operation so that other instructions can be scheduled inbetween
void MacroAssembler::store_check_part_1(Register obj) {
  BarrierSet* bs = Universe::heap()->barrier_set();
  assert(bs->kind() == BarrierSet::CardTableModRef, "Wrong barrier set kind");
  srli_d(obj, obj, CardTableModRefBS::card_shift);
}

void MacroAssembler::store_check_part_2(Register obj) {
  BarrierSet* bs = Universe::heap()->barrier_set();
  assert(bs->kind() == BarrierSet::CardTableModRef, "Wrong barrier set kind");
  CardTableModRefBS* ct = (CardTableModRefBS*)bs;
  assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");
  li(AT, (long)ct->byte_map_base);
  add_d(AT, AT, obj);
  if (UseConcMarkSweepGC) membar(StoreStore);
  st_b(R0, AT, 0);
}

// Defines obj, preserves var_size_in_bytes, okay for t2 == var_size_in_bytes.
void MacroAssembler::tlab_allocate(Register obj, Register var_size_in_bytes, int con_size_in_bytes,
                                   Register t1, Register t2, Label& slow_case) {
  assert_different_registers(obj, t2);
  assert_different_registers(obj, var_size_in_bytes);

  Register end = t2;
  // verify_tlab();

  ld_ptr(obj, Address(TREG, JavaThread::tlab_top_offset()));
  if (var_size_in_bytes == noreg) {
    lea(end, Address(obj, con_size_in_bytes));
  } else {
    lea(end, Address(obj, var_size_in_bytes, Address::times_1, 0));
  }

  ld_ptr(SCR1, Address(TREG, JavaThread::tlab_end_offset()));
  blt_far(SCR1, end, slow_case, false);

  // update the tlab top pointer
  st_ptr(end, Address(TREG, JavaThread::tlab_top_offset()));

  // recover var_size_in_bytes if necessary
  if (var_size_in_bytes == end) {
    sub_d(var_size_in_bytes, var_size_in_bytes, obj);
  }
  // verify_tlab();
}

// Defines obj, preserves var_size_in_bytes
void MacroAssembler::eden_allocate(Register obj, Register var_size_in_bytes, int con_size_in_bytes,
                                   Register t1, Label& slow_case) {
  assert_different_registers(obj, var_size_in_bytes, t1, AT);
  if (CMSIncrementalMode || !Universe::heap()->supports_inline_contig_alloc()) {
    // No allocation in the shared eden.
    b_far(slow_case);
  } else {
    Register end = t1;
    Register heap_end = SCR2;
    Label retry;
    bind(retry);

    li(SCR1, (address)Universe::heap()->end_addr());
    ld_d(heap_end, SCR1, 0);

    // Get the current top of the heap
    li(SCR1, (address) Universe::heap()->top_addr());
    ll_d(obj, SCR1, 0);

    // Adjust it my the size of our new object
    if (var_size_in_bytes == noreg)
      addi_d(end, obj, con_size_in_bytes);
    else
      add_d(end, obj, var_size_in_bytes);

    // if end < obj then we wrapped around high memory
    blt_far(end, obj, slow_case, false);
    blt_far(heap_end, end, slow_case, false);

    // If heap top hasn't been changed by some other thread, update it.
    sc_d(end, SCR1, 0);
    beqz(end, retry);

    incr_allocated_bytes(TREG, var_size_in_bytes, con_size_in_bytes, t1);
  }
}

void MacroAssembler::incr_allocated_bytes(Register thread,
                                          Register var_size_in_bytes,
                                          int con_size_in_bytes,
                                          Register t1) {
  if (!thread->is_valid()) {
#ifndef OPT_THREAD
    assert(t1->is_valid(), "need temp reg");
    thread = t1;
    get_thread(thread);
#else
    thread = TREG;
#endif
  }

  ld_ptr(AT, thread, in_bytes(JavaThread::allocated_bytes_offset()));
  if (var_size_in_bytes->is_valid()) {
    add_d(AT, AT, var_size_in_bytes);
  } else {
    addi_d(AT, AT, con_size_in_bytes);
  }
  st_ptr(AT, thread, in_bytes(JavaThread::allocated_bytes_offset()));
}

static const double     pi_4 =  0.7853981633974483;

// must get argument(a double) in FA0/FA1
//void MacroAssembler::trigfunc(char trig, bool preserve_cpu_regs, int num_fpu_regs_in_use) {
//We need to preseve the register which maybe modified during the Call
void MacroAssembler::trigfunc(char trig, int num_fpu_regs_in_use) {
  // save all modified register here
  // FIXME, in the disassembly of tirgfunc, only used V0, V1, T4, SP, RA, so we ony save V0, V1, T4
  guarantee(0, "LA not implemented yet");
#if 0
  pushad();
  // we should preserve the stack space before we call
  addi_d(SP, SP, -wordSize * 2);
  switch (trig){
    case 's' :
      call( CAST_FROM_FN_PTR(address, SharedRuntime::dsin), relocInfo::runtime_call_type );
      break;
    case 'c':
      call( CAST_FROM_FN_PTR(address, SharedRuntime::dcos), relocInfo::runtime_call_type );
      break;
    case 't':
      call( CAST_FROM_FN_PTR(address, SharedRuntime::dtan), relocInfo::runtime_call_type );
      break;
    default:assert (false, "bad intrinsic");
    break;

  }

  addi_d(SP, SP, wordSize * 2);
  popad();
#endif
}

void MacroAssembler::li(Register rd, jlong value) {
  jlong hi12 = bitfield(value, 52, 12);
  jlong lo52 = bitfield(value,  0, 52);

  if (hi12 != 0 && lo52 == 0) {
    lu52i_d(rd, R0, hi12);
  } else {
    jlong hi20 = bitfield(value, 32, 20);
    jlong lo20 = bitfield(value, 12, 20);
    jlong lo12 = bitfield(value,  0, 12);

    if (lo20 == 0) {
      ori(rd, R0, lo12);
    } else if (bitfield(simm12(lo12), 12, 20) == lo20) {
      addi_w(rd, R0, simm12(lo12));
    } else {
      lu12i_w(rd, lo20);
      if (lo12 != 0)
        ori(rd, rd, lo12);
    }
    if (hi20 != bitfield(simm20(lo20), 20, 20))
      lu32i_d(rd, hi20);
    if (hi12 != bitfield(simm20(hi20), 20, 12))
      lu52i_d(rd, rd, hi12);
  }
}

void MacroAssembler::patchable_li52(Register rd, jlong value) {
  int count = 0;

  if (value <= max_jint && value >= min_jint) {
    if (is_simm(value, 12)) {
      addi_d(rd, R0, value);
      count++;
    } else {
      lu12i_w(rd, split_low20(value >> 12));
      count++;
      if (split_low12(value)) {
        ori(rd, rd, split_low12(value));
        count++;
      }
    }
  } else if (is_simm(value, 52)) {
    lu12i_w(rd, split_low20(value >> 12));
    count++;
    if (split_low12(value)) {
      ori(rd, rd, split_low12(value));
      count++;
    }
    lu32i_d(rd, split_low20(value >> 32));
    count++;
  } else {
    tty->print_cr("value = 0x%lx", value);
    guarantee(false, "Not supported yet !");
  }

  while (count < 3) {
    nop();
    count++;
  }
}

void MacroAssembler::set_narrow_klass(Register dst, Klass* k) {
  assert(UseCompressedClassPointers, "should only be used for compressed header");
  assert(oop_recorder() != NULL, "this assembler needs an OopRecorder");

  int klass_index = oop_recorder()->find_index(k);
  RelocationHolder rspec = metadata_Relocation::spec(klass_index);
  long narrowKlass = (long)Klass::encode_klass(k);

  relocate(rspec, Assembler::narrow_oop_operand);
  patchable_li52(dst, narrowKlass);
}

void MacroAssembler::set_narrow_oop(Register dst, jobject obj) {
  assert(UseCompressedOops, "should only be used for compressed header");
  assert(oop_recorder() != NULL, "this assembler needs an OopRecorder");

  int oop_index = oop_recorder()->find_index(obj);
  RelocationHolder rspec = oop_Relocation::spec(oop_index);

  relocate(rspec, Assembler::narrow_oop_operand);
  patchable_li52(dst, oop_index);
}

void MacroAssembler::lipc(Register rd, Label& L) {
  if (L.is_bound()) {
    jint offs = (target(L) - pc()) >> 2;
    guarantee(is_simm(offs, 20), "Not signed 20-bit offset");
    pcaddi(rd, offs);
  } else {
    InstructionMark im(this);
    L.add_patch_at(code(), locator());
    pcaddi(rd, 0);
  }
}

void MacroAssembler::verify_oop(Register reg, const char* s) {
  if (!VerifyOops) return;
  const char * b = NULL;
  stringStream ss;
  ss.print("verify_oop: %s: %s", reg->name(), s);
  b = code_string(ss.as_string());
  pushad();
  move(A1, reg);
  patchable_li52(A0, (long)b);
  li(AT, (long)StubRoutines::verify_oop_subroutine_entry_address());
  ld_d(T4, AT, 0);
  jalr(T4);
  popad();
}

void MacroAssembler::verify_oop_addr(Address addr, const char* s) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
#if 0
  if (!VerifyOops) {
    nop();
    return;
  }
  // Pass register number to verify_oop_subroutine
  const char * b = NULL;
  stringStream ss;
  ss.print("verify_oop_addr: %s",  s);
  b = code_string(ss.as_string());

  st_ptr(T0, SP, - wordSize);
  st_ptr(T1, SP, - 2*wordSize);
  st_ptr(RA, SP, - 3*wordSize);
  st_ptr(A0, SP, - 4*wordSize);
  st_ptr(A1, SP, - 5*wordSize);
  st_ptr(AT, SP, - 6*wordSize);
  st_ptr(T9, SP, - 7*wordSize);
  ld_ptr(A1, addr);   // addr may use SP, so load from it before change SP
  addiu(SP, SP, - 7 * wordSize);

  patchable_li52(A0, (long)b);
  // call indirectly to solve generation ordering problem
  li(AT, (long)StubRoutines::verify_oop_subroutine_entry_address());
  ld_ptr(T9, AT, 0);
  jalr(T9);
  delayed()->nop();
  ld_ptr(T0, SP, 6* wordSize);
  ld_ptr(T1, SP, 5* wordSize);
  ld_ptr(RA, SP, 4* wordSize);
  ld_ptr(A0, SP, 3* wordSize);
  ld_ptr(A1, SP, 2* wordSize);
  ld_ptr(AT, SP, 1* wordSize);
  ld_ptr(T9, SP, 0* wordSize);
  addiu(SP, SP, 7 * wordSize);
#endif
}

// used registers :  T0, T1
void MacroAssembler::verify_oop_subroutine() {
  // RA: ra
  // A0: char* error message
  // A1: oop   object to verify
  Label exit, error;
  // increment counter
  li(T0, (long)StubRoutines::verify_oop_count_addr());
  ld_w(AT, T0, 0);
  addi_d(AT, AT, 1);
  st_w(AT, T0, 0);

  // make sure object is 'reasonable'
  beq(A1, R0, exit);         // if obj is NULL it is ok

  // Check if the oop is in the right area of memory
  // const int oop_mask = Universe::verify_oop_mask();
  // const int oop_bits = Universe::verify_oop_bits();
  const uintptr_t oop_mask = Universe::verify_oop_mask();
  const uintptr_t oop_bits = Universe::verify_oop_bits();
  li(AT, oop_mask);
  andr(T0, A1, AT);
  li(AT, oop_bits);
  bne(T0, AT, error);

  // make sure klass is 'reasonable'
  // add for compressedoops
  reinit_heapbase();
  // add for compressedoops
  load_klass(T0, A1);
  beq(T0, R0, error);                        // if klass is NULL it is broken
  // return if everything seems ok
  bind(exit);

  jr(RA);

  // handle errors
  bind(error);
  pushad();
  call(CAST_FROM_FN_PTR(address, MacroAssembler::debug), relocInfo::runtime_call_type);
  popad();
  jr(RA);
}

void MacroAssembler::verify_tlab(Register t1, Register t2) {
#ifdef ASSERT
  assert_different_registers(t1, t2, AT);
  if (UseTLAB && VerifyOops) {
    Label next, ok;

    get_thread(t1);

    ld_ptr(t2, t1, in_bytes(JavaThread::tlab_top_offset()));
    ld_ptr(AT, t1, in_bytes(JavaThread::tlab_start_offset()));
    bgeu(t2, AT, next);

    stop("assert(top >= start)");

    bind(next);
    ld_ptr(AT, t1, in_bytes(JavaThread::tlab_end_offset()));
    bgeu(AT, t2, ok);

    stop("assert(top <= end)");

    bind(ok);

  }
#endif
}

RegisterOrConstant MacroAssembler::delayed_value_impl(intptr_t* delayed_value_addr,
                                                      Register tmp,
                                                      int offset) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
  return RegisterOrConstant(tmp);
}

void MacroAssembler::hswap(Register reg) {
  // TODO LA opt
  //short
  srli_w(AT, reg, 8);
  slli_w(reg, reg, 24);
  srai_w(reg, reg, 16);
  orr(reg, reg, AT);
}

void MacroAssembler::huswap(Register reg) {
  // TODO LA opt
  srli_d(AT, reg, 8);
  slli_d(reg, reg, 24);
  srli_d(reg, reg, 16);
  orr(reg, reg, AT);
  bstrpick_d(reg, reg, 15, 0);
}

// something funny to do this will only one more register AT
// 32 bits
void MacroAssembler::swap(Register reg) {
  //TODO: LA opt
  srli_w(AT, reg, 8);
  slli_w(reg, reg, 24);
  orr(reg, reg, AT);
  //reg : 4 1 2 3
  srli_w(AT, AT, 16);
  xorr(AT, AT, reg);
  andi(AT, AT, 0xff);
  //AT : 0 0 0 1^3);
  xorr(reg, reg, AT);
  //reg : 4 1 2 1
  slli_w(AT, AT, 16);
  xorr(reg, reg, AT);
  //reg : 4 3 2 1
}

void MacroAssembler::cmpxchg(Address addr, Register oldval, Register newval,
                             Register resflag, bool retold, bool barrier) {
  assert(oldval != resflag, "oldval != resflag");
  assert(newval != resflag, "newval != resflag");
  Label again, succ, fail;

  bind(again);
  ll_d(resflag, addr);
  bne(resflag, oldval, fail);
  move(resflag, newval);
  sc_d(resflag, addr);
  beqz(resflag, again);
  b(succ);

  bind(fail);
  if (barrier)
    membar(LoadLoad);
  if (retold && oldval != R0)
    move(oldval, resflag);
  move(resflag, R0);
  bind(succ);
}

void MacroAssembler::cmpxchg(Address addr, Register oldval, Register newval,
                             Register tmp, bool retold, bool barrier, Label& succ, Label* fail) {
  assert(oldval != tmp, "oldval != tmp");
  assert(newval != tmp, "newval != tmp");
  Label again, neq;

  bind(again);
  ll_d(tmp, addr);
  bne(tmp, oldval, neq);
  move(tmp, newval);
  sc_d(tmp, addr);
  beqz(tmp, again);
  b(succ);

  bind(neq);
  if (barrier)
    membar(LoadLoad);
  if (retold && oldval != R0)
    move(oldval, tmp);
  if (fail)
    b(*fail);
}

void MacroAssembler::cmpxchg32(Address addr, Register oldval, Register newval,
                               Register resflag, bool sign, bool retold, bool barrier) {
  assert(oldval != resflag, "oldval != resflag");
  assert(newval != resflag, "newval != resflag");
  Label again, succ, fail;

  bind(again);
  ll_w(resflag, addr);
  if (!sign)
    lu32i_d(resflag, 0);
  bne(resflag, oldval, fail);
  move(resflag, newval);
  sc_w(resflag, addr);
  beqz(resflag, again);
  b(succ);

  bind(fail);
  if (barrier)
    membar(LoadLoad);
  if (retold && oldval != R0)
    move(oldval, resflag);
  move(resflag, R0);
  bind(succ);
}

void MacroAssembler::cmpxchg32(Address addr, Register oldval, Register newval, Register tmp,
                               bool sign, bool retold, bool barrier, Label& succ, Label* fail) {
  assert(oldval != tmp, "oldval != tmp");
  assert(newval != tmp, "newval != tmp");
  Label again, neq;

  bind(again);
  ll_w(tmp, addr);
  if (!sign)
    lu32i_d(tmp, 0);
  bne(tmp, oldval, neq);
  move(tmp, newval);
  sc_w(tmp, addr);
  beqz(tmp, again);
  b(succ);

  bind(neq);
  if (barrier)
    membar(LoadLoad);
  if (retold && oldval != R0)
    move(oldval, tmp);
  if (fail)
    b(*fail);
}

// be sure the three register is different
void MacroAssembler::rem_s(FloatRegister fd, FloatRegister fs, FloatRegister ft, FloatRegister tmp) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

// be sure the three register is different
void MacroAssembler::rem_d(FloatRegister fd, FloatRegister fs, FloatRegister ft, FloatRegister tmp) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

// Fast_Lock and Fast_Unlock used by C2

// Because the transitions from emitted code to the runtime
// monitorenter/exit helper stubs are so slow it's critical that
// we inline both the stack-locking fast-path and the inflated fast path.
//
// See also: cmpFastLock and cmpFastUnlock.
//
// What follows is a specialized inline transliteration of the code
// in slow_enter() and slow_exit().  If we're concerned about I$ bloat
// another option would be to emit TrySlowEnter and TrySlowExit methods
// at startup-time.  These methods would accept arguments as
// (Obj, Self, box, Scratch) and return success-failure
// indications in the icc.ZFlag.  Fast_Lock and Fast_Unlock would simply
// marshal the arguments and emit calls to TrySlowEnter and TrySlowExit.
// In practice, however, the # of lock sites is bounded and is usually small.
// Besides the call overhead, TrySlowEnter and TrySlowExit might suffer
// if the processor uses simple bimodal branch predictors keyed by EIP
// Since the helper routines would be called from multiple synchronization
// sites.
//
// An even better approach would be write "MonitorEnter()" and "MonitorExit()"
// in java - using j.u.c and unsafe - and just bind the lock and unlock sites
// to those specialized methods.  That'd give us a mostly platform-independent
// implementation that the JITs could optimize and inline at their pleasure.
// Done correctly, the only time we'd need to cross to native could would be
// to park() or unpark() threads.  We'd also need a few more unsafe operators
// to (a) prevent compiler-JIT reordering of non-volatile accesses, and
// (b) explicit barriers or fence operations.
//
// TODO:
//
// *  Arrange for C2 to pass "Self" into Fast_Lock and Fast_Unlock in one of the registers (scr).
//    This avoids manifesting the Self pointer in the Fast_Lock and Fast_Unlock terminals.
//    Given TLAB allocation, Self is usually manifested in a register, so passing it into
//    the lock operators would typically be faster than reifying Self.
//
// *  Ideally I'd define the primitives as:
//       fast_lock   (nax Obj, nax box, res, tmp, nax scr) where tmp and scr are KILLED.
//       fast_unlock (nax Obj, box, res, nax tmp) where tmp are KILLED
//    Unfortunately ADLC bugs prevent us from expressing the ideal form.
//    Instead, we're stuck with a rather awkward and brittle register assignments below.
//    Furthermore the register assignments are overconstrained, possibly resulting in
//    sub-optimal code near the synchronization site.
//
// *  Eliminate the sp-proximity tests and just use "== Self" tests instead.
//    Alternately, use a better sp-proximity test.
//
// *  Currently ObjectMonitor._Owner can hold either an sp value or a (THREAD *) value.
//    Either one is sufficient to uniquely identify a thread.
//    TODO: eliminate use of sp in _owner and use get_thread(tr) instead.
//
// *  Intrinsify notify() and notifyAll() for the common cases where the
//    object is locked by the calling thread but the waitlist is empty.
//    avoid the expensive JNI call to JVM_Notify() and JVM_NotifyAll().
//
// *  use jccb and jmpb instead of jcc and jmp to improve code density.
//    But beware of excessive branch density on AMD Opterons.
//
// *  Both Fast_Lock and Fast_Unlock set the ICC.ZF to indicate success
//    or failure of the fast-path.  If the fast-path fails then we pass
//    control to the slow-path, typically in C.  In Fast_Lock and
//    Fast_Unlock we often branch to DONE_LABEL, just to find that C2
//    will emit a conditional branch immediately after the node.
//    So we have branches to branches and lots of ICC.ZF games.
//    Instead, it might be better to have C2 pass a "FailureLabel"
//    into Fast_Lock and Fast_Unlock.  In the case of success, control
//    will drop through the node.  ICC.ZF is undefined at exit.
//    In the case of failure, the node will branch directly to the
//    FailureLabel

// obj: object to lock
// box: on-stack box address (displaced header location)
// tmp: tmp -- KILLED
// scr: tmp -- KILLED
void MacroAssembler::fast_lock(Register objReg, Register boxReg, Register resReg,
                               Register tmpReg, Register scrReg) {
  Label IsInflated, DONE, DONE_SET;

  // Ensure the register assignents are disjoint
  guarantee(objReg != boxReg, "");
  guarantee(objReg != tmpReg, "");
  guarantee(objReg != scrReg, "");
  guarantee(boxReg != tmpReg, "");
  guarantee(boxReg != scrReg, "");

  block_comment("FastLock");

  if (PrintBiasedLockingStatistics) {
    atomic_inc32((address)BiasedLocking::total_entry_count_addr(), 1, tmpReg, scrReg);
  }

  if (EmitSync & 1) {
    move(AT, R0);
    return;
  } else
    if (EmitSync & 2) {
      Label DONE_LABEL ;
      if (UseBiasedLocking) {
        // Note: tmpReg maps to the swap_reg argument and scrReg to the tmp_reg argument.
        biased_locking_enter(boxReg, objReg, tmpReg, scrReg, false, DONE_LABEL, NULL);
      }

      ld_d(tmpReg, Address(objReg, 0)) ;          // fetch markword
      ori(tmpReg, tmpReg, 0x1);
      st_d(tmpReg, Address(boxReg, 0));           // Anticipate successful CAS

      cmpxchg(Address(objReg, 0), tmpReg, boxReg, scrReg, true, false, DONE_LABEL); // Updates tmpReg

      // Recursive locking
      sub_d(tmpReg, tmpReg, SP);
      li(AT, (7 - os::vm_page_size() ));
      andr(tmpReg, tmpReg, AT);
      st_d(tmpReg, Address(boxReg, 0));
      bind(DONE_LABEL) ;
    } else {
      // Possible cases that we'll encounter in fast_lock
      // ------------------------------------------------
      // * Inflated
      //    -- unlocked
      //    -- Locked
      //       = by self
      //       = by other
      // * biased
      //    -- by Self
      //    -- by other
      // * neutral
      // * stack-locked
      //    -- by self
      //       = sp-proximity test hits
      //       = sp-proximity test generates false-negative
      //    -- by other
      //

      // TODO: optimize away redundant LDs of obj->mark and improve the markword triage
      // order to reduce the number of conditional branches in the most common cases.
      // Beware -- there's a subtle invariant that fetch of the markword
      // at [FETCH], below, will never observe a biased encoding (*101b).
      // If this invariant is not held we risk exclusion (safety) failure.
      if (UseBiasedLocking && !UseOptoBiasInlining) {
        Label succ, fail;
        biased_locking_enter(boxReg, objReg, tmpReg, scrReg, false, succ, NULL);
        b(fail);
        bind(succ);
        li(resReg, 1);
        b(DONE);
        bind(fail);
      }

      ld_d(tmpReg, Address(objReg, 0)); //Fetch the markword of the object.
      andi(AT, tmpReg, markOopDesc::monitor_value);
      bnez(AT, IsInflated); // inflated vs stack-locked|neutral|bias

      // Attempt stack-locking ...
      ori(tmpReg, tmpReg, markOopDesc::unlocked_value);
      st_d(tmpReg, Address(boxReg, 0)); // Anticipate successful CAS

      if (PrintBiasedLockingStatistics) {
        Label SUCC, FAIL;
        cmpxchg(Address(objReg, 0), tmpReg, boxReg, scrReg, true, false, SUCC, &FAIL); // Updates tmpReg
        bind(SUCC);
        atomic_inc32((address)BiasedLocking::fast_path_entry_count_addr(), 1, AT, scrReg);
        li(resReg, 1);
        b(DONE);
        bind(FAIL);
      } else {
        // If cmpxchg is succ, then scrReg = 1
        cmpxchg(Address(objReg, 0), tmpReg, boxReg, scrReg, true, false, DONE_SET); // Updates tmpReg
      }

      // Recursive locking
      // The object is stack-locked: markword contains stack pointer to BasicLock.
      // Locked by current thread if difference with current SP is less than one page.
      sub_d(tmpReg, tmpReg, SP);
      li(AT, 7 - os::vm_page_size());
      andr(tmpReg, tmpReg, AT);
      st_d(tmpReg, Address(boxReg, 0));

      if (PrintBiasedLockingStatistics) {
        Label L;
        // tmpReg == 0 => BiasedLocking::_fast_path_entry_count++
        bnez(tmpReg, L);
        atomic_inc32((address)BiasedLocking::fast_path_entry_count_addr(), 1, AT, scrReg);
        bind(L);
      }

      sltui(resReg, tmpReg, 1); // resReg = (tmpReg == 0) ? 1 : 0
      b(DONE);

      bind(IsInflated);
      // The object's monitor m is unlocked iff m->owner == NULL,
      // otherwise m->owner may contain a thread or a stack address.

      // TODO: someday avoid the ST-before-CAS penalty by
      // relocating (deferring) the following ST.
      // We should also think about trying a CAS without having
      // fetched _owner.  If the CAS is successful we may
      // avoid an RTO->RTS upgrade on the $line.
      // Without cast to int32_t a movptr will destroy r10 which is typically obj
      li(AT, (int32_t)intptr_t(markOopDesc::unused_mark()));
      st_d(AT, Address(boxReg, 0));

      ld_d(AT, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2));
      // if (m->owner != 0) => AT = 0, goto slow path.
      move(scrReg, R0);
      bnez(AT, DONE_SET);

#ifndef OPT_THREAD
      get_thread(TREG) ;
#endif
      // It's inflated and appears unlocked
      addi_d(tmpReg, tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2);
      cmpxchg(Address(tmpReg, 0), R0, TREG, scrReg, false, false);
      // Intentional fall-through into DONE ...

      bind(DONE_SET);
      move(resReg, scrReg);

      // DONE is a hot target - we'd really like to place it at the
      // start of cache line by padding with NOPs.
      // See the AMD and Intel software optimization manuals for the
      // most efficient "long" NOP encodings.
      // Unfortunately none of our alignment mechanisms suffice.
      bind(DONE);
      // At DONE the resReg is set as follows ...
      // Fast_Unlock uses the same protocol.
      // resReg == 1 -> Success
      // resREg == 0 -> Failure - force control through the slow-path

      // Avoid branch-to-branch on AMD processors
      // This appears to be superstition.
      if (EmitSync & 32) nop() ;

    }
}

// obj: object to unlock
// box: box address (displaced header location), killed.
// tmp: killed tmp; cannot be obj nor box.
//
// Some commentary on balanced locking:
//
// Fast_Lock and Fast_Unlock are emitted only for provably balanced lock sites.
// Methods that don't have provably balanced locking are forced to run in the
// interpreter - such methods won't be compiled to use fast_lock and fast_unlock.
// The interpreter provides two properties:
// I1:  At return-time the interpreter automatically and quietly unlocks any
//      objects acquired the current activation (frame).  Recall that the
//      interpreter maintains an on-stack list of locks currently held by
//      a frame.
// I2:  If a method attempts to unlock an object that is not held by the
//      the frame the interpreter throws IMSX.
//
// Lets say A(), which has provably balanced locking, acquires O and then calls B().
// B() doesn't have provably balanced locking so it runs in the interpreter.
// Control returns to A() and A() unlocks O.  By I1 and I2, above, we know that O
// is still locked by A().
//
// The only other source of unbalanced locking would be JNI.  The "Java Native Interface:
// Programmer's Guide and Specification" claims that an object locked by jni_monitorenter
// should not be unlocked by "normal" java-level locking and vice-versa.  The specification
// doesn't specify what will occur if a program engages in such mixed-mode locking, however.

void MacroAssembler::fast_unlock(Register objReg, Register boxReg, Register resReg,
                                 Register tmpReg, Register scrReg) {
  Label DONE, DONE_SET, Stacked, Inflated;

  guarantee(objReg != boxReg, "");
  guarantee(objReg != tmpReg, "");
  guarantee(objReg != scrReg, "");
  guarantee(boxReg != tmpReg, "");
  guarantee(boxReg != scrReg, "");

  block_comment("FastUnlock");

  if (EmitSync & 4) {
    // Disable - inhibit all inlining.  Force control through the slow-path
    move(AT, R0);
    return;
  } else
    if (EmitSync & 8) {
      Label DONE_LABEL ;
      if (UseBiasedLocking) {
        biased_locking_exit(objReg, tmpReg, DONE_LABEL);
      }
      // classic stack-locking code ...
      ld_d(tmpReg, Address(boxReg, 0)) ;
      assert_different_registers(AT, tmpReg);
      li(AT, 0x1);
      beq(tmpReg, R0, DONE_LABEL) ;

      cmpxchg(Address(objReg, 0), boxReg, tmpReg, AT, false, false);
      bind(DONE_LABEL);
    } else {
      Label CheckSucc;

      // Critically, the biased locking test must have precedence over
      // and appear before the (box->dhw == 0) recursive stack-lock test.
      if (UseBiasedLocking && !UseOptoBiasInlining) {
        Label succ, fail;
        biased_locking_exit(objReg, tmpReg, succ);
        b(fail);
        bind(succ);
        li(resReg, 1);
        b(DONE);
        bind(fail);
      }

      ld_d(tmpReg, Address(boxReg, 0)); // Examine the displaced header
      sltui(AT, tmpReg, 1);
      beqz(tmpReg, DONE_SET); // 0 indicates recursive stack-lock

      ld_d(tmpReg, Address(objReg, 0)); // Examine the object's markword
      andi(AT, tmpReg, markOopDesc::monitor_value);
      beqz(AT, Stacked); // Inflated?

      bind(Inflated);
      // It's inflated.
      // Despite our balanced locking property we still check that m->_owner == Self
      // as java routines or native JNI code called by this thread might
      // have released the lock.
      // Refer to the comments in synchronizer.cpp for how we might encode extra
      // state in _succ so we can avoid fetching EntryList|cxq.
      //
      // I'd like to add more cases in fast_lock() and fast_unlock() --
      // such as recursive enter and exit -- but we have to be wary of
      // I$ bloat, T$ effects and BP$ effects.
      //
      // If there's no contention try a 1-0 exit.  That is, exit without
      // a costly MEMBAR or CAS.  See synchronizer.cpp for details on how
      // we detect and recover from the race that the 1-0 exit admits.
      //
      // Conceptually Fast_Unlock() must execute a STST|LDST "release" barrier
      // before it STs null into _owner, releasing the lock.  Updates
      // to data protected by the critical section must be visible before
      // we drop the lock (and thus before any other thread could acquire
      // the lock and observe the fields protected by the lock).
#ifndef OPT_THREAD
      get_thread(TREG);
#endif

      // It's inflated
      ld_d(scrReg, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2));
      xorr(scrReg, scrReg, TREG);

      ld_d(AT, Address(tmpReg, ObjectMonitor::recursions_offset_in_bytes() - 2));
      orr(scrReg, scrReg, AT);

      move(AT, R0);
      bnez(scrReg, DONE_SET);

      ld_d(scrReg, Address(tmpReg, ObjectMonitor::cxq_offset_in_bytes() - 2));
      ld_d(AT, Address(tmpReg, ObjectMonitor::EntryList_offset_in_bytes() - 2));
      orr(scrReg, scrReg, AT);

      move(AT, R0);
      bnez(scrReg, DONE_SET);

      membar(Assembler::Membar_mask_bits(LoadLoad|LoadStore));
      st_d(R0, Address(tmpReg, ObjectMonitor::owner_offset_in_bytes() - 2));
      li(resReg, 1);
      b(DONE);

      bind(Stacked);
      ld_d(tmpReg, Address(boxReg, 0));
      cmpxchg(Address(objReg, 0), boxReg, tmpReg, AT, false, false);

      bind(DONE_SET);
      move(resReg, AT);

      if (EmitSync & 65536) {
        bind (CheckSucc);
      }

      bind(DONE);

      // Avoid branch to branch on AMD processors
      if (EmitSync & 32768) { nop() ; }
    }
}

void MacroAssembler::align(int modulus) {
  while (offset() % modulus != 0) nop();
}


void MacroAssembler::verify_FPU(int stack_depth, const char* s) {
  //Unimplemented();
}

Register caller_saved_registers[]           = {T7, T5, T6, A0, A1, A2, A3, A4, A5, A6, A7, T0, T1, T2, T3, T8, T4, S8, RA, FP};
Register caller_saved_registers_except_v0[] = {T7, T5, T6,     A1, A2, A3, A4, A5, A6, A7, T0, T1, T2, T3, T8, T4, S8, RA, FP};

  //TODO: LA
//In LA, F0~23 are all caller-saved registers
FloatRegister caller_saved_fpu_registers[] = {F0, F12, F13};

// We preserve all caller-saved register
void  MacroAssembler::pushad(){
  int i;
  // Fixed-point registers
  int len = sizeof(caller_saved_registers) / sizeof(caller_saved_registers[0]);
  addi_d(SP, SP, -1 * len * wordSize);
  for (i = 0; i < len; i++) {
    st_d(caller_saved_registers[i], SP, (len - i - 1) * wordSize);
  }

  // Floating-point registers
  len = sizeof(caller_saved_fpu_registers) / sizeof(caller_saved_fpu_registers[0]);
  addi_d(SP, SP, -1 * len * wordSize);
  for (i = 0; i < len; i++) {
    fst_d(caller_saved_fpu_registers[i], SP, (len - i - 1) * wordSize);
  }
};

void  MacroAssembler::popad(){
  int i;
  // Floating-point registers
  int len = sizeof(caller_saved_fpu_registers) / sizeof(caller_saved_fpu_registers[0]);
  for (i = 0; i < len; i++)
  {
    fld_d(caller_saved_fpu_registers[i], SP, (len - i - 1) * wordSize);
  }
  addi_d(SP, SP, len * wordSize);

  // Fixed-point registers
  len = sizeof(caller_saved_registers) / sizeof(caller_saved_registers[0]);
  for (i = 0; i < len; i++)
  {
    ld_d(caller_saved_registers[i], SP, (len - i - 1) * wordSize);
  }
  addi_d(SP, SP, len * wordSize);
};

// We preserve all caller-saved register except V0
void MacroAssembler::pushad_except_v0() {
  int i;
  // Fixed-point registers
  int len = sizeof(caller_saved_registers_except_v0) / sizeof(caller_saved_registers_except_v0[0]);
  addi_d(SP, SP, -1 * len * wordSize);
  for (i = 0; i < len; i++) {
    st_d(caller_saved_registers_except_v0[i], SP, (len - i - 1) * wordSize);
  }

  // Floating-point registers
  len = sizeof(caller_saved_fpu_registers) / sizeof(caller_saved_fpu_registers[0]);
  addi_d(SP, SP, -1 * len * wordSize);
  for (i = 0; i < len; i++) {
    fst_d(caller_saved_fpu_registers[i], SP, (len - i - 1) * wordSize);
  }
}

void MacroAssembler::popad_except_v0() {
  int i;
  // Floating-point registers
  int len = sizeof(caller_saved_fpu_registers) / sizeof(caller_saved_fpu_registers[0]);
  for (i = 0; i < len; i++) {
    fld_d(caller_saved_fpu_registers[i], SP, (len - i - 1) * wordSize);
  }
  addi_d(SP, SP, len * wordSize);

  // Fixed-point registers
  len = sizeof(caller_saved_registers_except_v0) / sizeof(caller_saved_registers_except_v0[0]);
  for (i = 0; i < len; i++) {
    ld_d(caller_saved_registers_except_v0[i], SP, (len - i - 1) * wordSize);
  }
  addi_d(SP, SP, len * wordSize);
}

void MacroAssembler::push2(Register reg1, Register reg2) {
  addi_d(SP, SP, -16);
  st_d(reg1, SP, 8);
  st_d(reg2, SP, 0);
}

void MacroAssembler::pop2(Register reg1, Register reg2) {
  ld_d(reg1, SP, 8);
  ld_d(reg2, SP, 0);
  addi_d(SP, SP, 16);
}

// for UseCompressedOops Option
void MacroAssembler::load_klass(Register dst, Register src) {
  if(UseCompressedClassPointers){
    ld_wu(dst, Address(src, oopDesc::klass_offset_in_bytes()));
    decode_klass_not_null(dst);
  } else {
    ld_d(dst, src, oopDesc::klass_offset_in_bytes());
  }
}

void MacroAssembler::store_klass(Register dst, Register src) {
  if(UseCompressedClassPointers){
    encode_klass_not_null(src);
    st_w(src, dst, oopDesc::klass_offset_in_bytes());
  } else {
    st_d(src, dst, oopDesc::klass_offset_in_bytes());
  }
}

void MacroAssembler::load_prototype_header(Register dst, Register src) {
  load_klass(dst, src);
  ld_d(dst, Address(dst, Klass::prototype_header_offset()));
}

void MacroAssembler::store_klass_gap(Register dst, Register src) {
  if (UseCompressedClassPointers) {
    st_w(src, dst, oopDesc::klass_gap_offset_in_bytes());
  }
}

void MacroAssembler::load_heap_oop(Register dst, Address src) {
  if(UseCompressedOops){
    ld_wu(dst, src);
    decode_heap_oop(dst);
  } else {
    ld_d(dst, src);
  }
}

void MacroAssembler::store_heap_oop(Address dst, Register src){
  if(UseCompressedOops){
    assert(!dst.uses(src), "not enough registers");
    encode_heap_oop(src);
    st_w(src, dst);
  } else {
    st_d(src, dst);
  }
}

void MacroAssembler::store_heap_oop_null(Address dst){
  if(UseCompressedOops){
    st_w(R0, dst);
  } else {
    st_d(R0, dst);
  }
}

#ifdef ASSERT
void MacroAssembler::verify_heapbase(const char* msg) {
  assert (UseCompressedOops || UseCompressedClassPointers, "should be compressed");
  assert (Universe::heap() != NULL, "java heap should be initialized");
}
#endif

// Algorithm must match oop.inline.hpp encode_heap_oop.
void MacroAssembler::encode_heap_oop(Register r) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_heap_oop:heap base corrupted?");
#endif
  verify_oop(r, "broken oop in encode_heap_oop");
  if (Universe::narrow_oop_base() == NULL) {
    if (Universe::narrow_oop_shift() != 0) {
      assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
      shr(r, LogMinObjAlignmentInBytes);
    }
    return;
  }

  sub_d(AT, r, S5_heapbase);
  maskeqz(r, AT, r);
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    shr(r, LogMinObjAlignmentInBytes);
  }
}

void MacroAssembler::encode_heap_oop(Register dst, Register src) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_heap_oop:heap base corrupted?");
#endif
  verify_oop(src, "broken oop in encode_heap_oop");
  if (Universe::narrow_oop_base() == NULL) {
    if (Universe::narrow_oop_shift() != 0) {
      assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
      srli_d(dst, src, LogMinObjAlignmentInBytes);
    } else {
      if (dst != src) {
        move(dst, src);
      }
    }
    return;
  }

  sub_d(AT, src, S5_heapbase);
  maskeqz(dst, AT, src);
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    shr(dst, LogMinObjAlignmentInBytes);
  }
}

void MacroAssembler::encode_heap_oop_not_null(Register r) {
  assert (UseCompressedOops, "should be compressed");
#ifdef ASSERT
  if (CheckCompressedOops) {
    Label ok;
    bne(r, R0, ok);
    stop("null oop passed to encode_heap_oop_not_null");
    bind(ok);
  }
#endif
  verify_oop(r, "broken oop in encode_heap_oop_not_null");
  if (Universe::narrow_oop_base() != NULL) {
    sub_d(r, r, S5_heapbase);
  }
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    shr(r, LogMinObjAlignmentInBytes);
  }
}

void MacroAssembler::encode_heap_oop_not_null(Register dst, Register src) {
  assert (UseCompressedOops, "should be compressed");
#ifdef ASSERT
  if (CheckCompressedOops) {
    Label ok;
    bne(src, R0, ok);
    stop("null oop passed to encode_heap_oop_not_null2");
    bind(ok);
  }
#endif
  verify_oop(src, "broken oop in encode_heap_oop_not_null2");
  if (Universe::narrow_oop_base() == NULL) {
    if (Universe::narrow_oop_shift() != 0) {
      assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
      srli_d(dst, src, LogMinObjAlignmentInBytes);
    } else {
      if (dst != src) {
        move(dst, src);
      }
    }
    return;
  }
  sub_d(dst, src, S5_heapbase);
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    shr(dst, LogMinObjAlignmentInBytes);
  }
}

void MacroAssembler::decode_heap_oop(Register r) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::decode_heap_oop corrupted?");
#endif
  if (Universe::narrow_oop_base() == NULL) {
    if (Universe::narrow_oop_shift() != 0) {
      assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
      shl(r, LogMinObjAlignmentInBytes);
    }
    return;
  }

  move(AT, r);
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    if (LogMinObjAlignmentInBytes <= 4) {
      alsl_d(r, r, S5_heapbase, LogMinObjAlignmentInBytes - 1);
    } else {
      shl(r, LogMinObjAlignmentInBytes);
      add_d(r, r, S5_heapbase);
    }
  } else {
    add_d(r, r, S5_heapbase);
  }
  maskeqz(r, r, AT);
  verify_oop(r, "broken oop in decode_heap_oop");
}

void MacroAssembler::decode_heap_oop(Register dst, Register src) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::decode_heap_oop corrupted?");
#endif
  if (Universe::narrow_oop_base() == NULL) {
    if (Universe::narrow_oop_shift() != 0) {
      assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
      slli_d(dst, src, LogMinObjAlignmentInBytes);
    } else {
      if (dst != src) {
        move(dst, src);
      }
    }
    return;
  }

  Register cond;
  if (dst == src) {
    cond = AT;
    move(cond, src);
  } else {
    cond = src;
  }
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    if (LogMinObjAlignmentInBytes <= 4) {
      alsl_d(dst, src, S5_heapbase, LogMinObjAlignmentInBytes - 1);
    } else {
      slli_d(dst, src, LogMinObjAlignmentInBytes);
      add_d(dst, dst, S5_heapbase);
    }
  } else {
    add_d(dst, src, S5_heapbase);
  }
  maskeqz(dst, dst, cond);
  verify_oop(dst, "broken oop in decode_heap_oop");
}

void MacroAssembler::decode_heap_oop_not_null(Register r) {
  // Note: it will change flags
  assert(UseCompressedOops, "should only be used for compressed headers");
  assert(Universe::heap() != NULL, "java heap should be initialized");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    if (Universe::narrow_oop_base() != NULL) {
      if (LogMinObjAlignmentInBytes <= 4) {
        alsl_d(r, r, S5_heapbase, LogMinObjAlignmentInBytes - 1);
      } else {
        shl(r, LogMinObjAlignmentInBytes);
        add_d(r, r, S5_heapbase);
      }
    } else {
      shl(r, LogMinObjAlignmentInBytes);
    }
  } else {
    assert(Universe::narrow_oop_base() == NULL, "sanity");
  }
}

void MacroAssembler::decode_heap_oop_not_null(Register dst, Register src) {
  assert(UseCompressedOops, "should only be used for compressed headers");
  assert(Universe::heap() != NULL, "java heap should be initialized");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    if (Universe::narrow_oop_base() != NULL) {
      if (LogMinObjAlignmentInBytes <= 4) {
        alsl_d(dst, src, S5_heapbase, LogMinObjAlignmentInBytes - 1);
      } else {
        slli_d(dst, src, LogMinObjAlignmentInBytes);
        add_d(dst, dst, S5_heapbase);
      }
    } else {
      slli_d(dst, src, LogMinObjAlignmentInBytes);
    }
  } else {
    assert (Universe::narrow_oop_base() == NULL, "sanity");
    if (dst != src) {
      move(dst, src);
    }
  }
}

void MacroAssembler::encode_klass_not_null(Register r) {
  if (Universe::narrow_klass_base() != NULL) {
    assert(r != AT, "Encoding a klass in AT");
    li(AT, (int64_t)Universe::narrow_klass_base());
    sub_d(r, r, AT);
  }
  if (Universe::narrow_klass_shift() != 0) {
    assert (LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
    shr(r, LogKlassAlignmentInBytes);
  }
}

void MacroAssembler::encode_klass_not_null(Register dst, Register src) {
  if (dst == src) {
    encode_klass_not_null(src);
  } else {
    if (Universe::narrow_klass_base() != NULL) {
      li(dst, (int64_t)Universe::narrow_klass_base());
      sub_d(dst, src, dst);
      if (Universe::narrow_klass_shift() != 0) {
        assert (LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
        shr(dst, LogKlassAlignmentInBytes);
      }
    } else {
      if (Universe::narrow_klass_shift() != 0) {
        assert (LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
        srli_d(dst, src, LogKlassAlignmentInBytes);
      } else {
        move(dst, src);
      }
    }
  }
}

// Function instr_size_for_decode_klass_not_null() counts the instructions
// generated by decode_klass_not_null(register r) and reinit_heapbase(),
// when (Universe::heap() != NULL).  Hence, if the instructions they
// generate change, then this method needs to be updated.
int MacroAssembler::instr_size_for_decode_klass_not_null() {
  assert (UseCompressedClassPointers, "only for compressed klass ptrs");
  if (Universe::narrow_klass_base() != NULL) {
    // mov64 + addq + shlq? + mov64  (for reinit_heapbase()).
    return (Universe::narrow_klass_shift() == 0 ? 4 * 9 : 4 * 10);
  } else {
    // longest load decode klass function, mov64, leaq
    return (Universe::narrow_klass_shift() == 0 ? 4 * 0 : 4 * 1);
  }
}

void MacroAssembler::decode_klass_not_null(Register r) {
  assert(UseCompressedClassPointers, "should only be used for compressed headers");
  assert(r != AT, "Decoding a klass in AT");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (Universe::narrow_klass_shift() != 0) {
    assert(LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
    shl(r, LogKlassAlignmentInBytes);
  }
  if (Universe::narrow_klass_base() != NULL) {
    li(AT, (int64_t)Universe::narrow_klass_base());
    add_d(r, r, AT);
  }
}

void MacroAssembler::decode_klass_not_null(Register dst, Register src) {
  assert(UseCompressedClassPointers, "should only be used for compressed headers");
  if (dst == src) {
    decode_klass_not_null(dst);
  } else {
    // Cannot assert, unverified entry point counts instructions (see .ad file)
    // vtableStubs also counts instructions in pd_code_size_limit.
    // Also do not verify_oop as this is called by verify_oop.
    li(dst, (int64_t)Universe::narrow_klass_base());
    if (Universe::narrow_klass_shift() != 0) {
      assert(LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
      assert(LogKlassAlignmentInBytes == Address::times_8, "klass not aligned on 64bits?");
      alsl_d(dst, src, dst, Address::times_8 - 1);
    } else {
      add_d(dst, src, dst);
    }
  }
}

void MacroAssembler::reinit_heapbase() {
  if (UseCompressedOops || UseCompressedClassPointers) {
    if (Universe::heap() != NULL) {
      if (Universe::narrow_oop_base() == NULL) {
        move(S5_heapbase, R0);
      } else {
        li(S5_heapbase, (int64_t)Universe::narrow_ptrs_base());
      }
    } else {
      li(S5_heapbase, (intptr_t)Universe::narrow_ptrs_base_addr());
      ld_d(S5_heapbase, S5_heapbase, 0);
    }
  }
}

void MacroAssembler::check_klass_subtype(Register sub_klass,
                           Register super_klass,
                           Register temp_reg,
                           Label& L_success) {
//implement ind   gen_subtype_check
  Label L_failure;
  check_klass_subtype_fast_path(sub_klass, super_klass, temp_reg,        &L_success, &L_failure, NULL);
  check_klass_subtype_slow_path(sub_klass, super_klass, temp_reg, noreg, &L_success, NULL);
  bind(L_failure);
}

SkipIfEqual::SkipIfEqual(
    MacroAssembler* masm, const bool* flag_addr, bool value) {
  _masm = masm;
  _masm->li(AT, (address)flag_addr);
  _masm->ld_b(AT, AT, 0);
  _masm->addi_d(AT, AT, -value);
  _masm->beq(AT, R0, _label);
}

void MacroAssembler::check_klass_subtype_fast_path(Register sub_klass,
                                                   Register super_klass,
                                                   Register temp_reg,
                                                   Label* L_success,
                                                   Label* L_failure,
                                                   Label* L_slow_path,
                                        RegisterOrConstant super_check_offset) {
  assert_different_registers(sub_klass, super_klass, temp_reg);
  bool must_load_sco = (super_check_offset.constant_or_zero() == -1);
  if (super_check_offset.is_register()) {
    assert_different_registers(sub_klass, super_klass,
                               super_check_offset.as_register());
  } else if (must_load_sco) {
    assert(temp_reg != noreg, "supply either a temp or a register offset");
  }

  Label L_fallthrough;
  int label_nulls = 0;
  if (L_success == NULL)   { L_success   = &L_fallthrough; label_nulls++; }
  if (L_failure == NULL)   { L_failure   = &L_fallthrough; label_nulls++; }
  if (L_slow_path == NULL) { L_slow_path = &L_fallthrough; label_nulls++; }
  assert(label_nulls <= 1, "at most one NULL in the batch");

  int sc_offset = in_bytes(Klass::secondary_super_cache_offset());
  int sco_offset = in_bytes(Klass::super_check_offset_offset());
  // If the pointers are equal, we are done (e.g., String[] elements).
  // This self-check enables sharing of secondary supertype arrays among
  // non-primary types such as array-of-interface.  Otherwise, each such
  // type would need its own customized SSA.
  // We move this check to the front of the fast path because many
  // type checks are in fact trivially successful in this manner,
  // so we get a nicely predicted branch right at the start of the check.
  beq(sub_klass, super_klass, *L_success);
  // Check the supertype display:
  if (must_load_sco) {
    ld_wu(temp_reg, super_klass, sco_offset);
    super_check_offset = RegisterOrConstant(temp_reg);
  }
  add_d(AT, sub_klass, super_check_offset.register_or_noreg());
  ld_d(AT, AT, super_check_offset.constant_or_zero());

  // This check has worked decisively for primary supers.
  // Secondary supers are sought in the super_cache ('super_cache_addr').
  // (Secondary supers are interfaces and very deeply nested subtypes.)
  // This works in the same check above because of a tricky aliasing
  // between the super_cache and the primary super display elements.
  // (The 'super_check_addr' can address either, as the case requires.)
  // Note that the cache is updated below if it does not help us find
  // what we need immediately.
  // So if it was a primary super, we can just fail immediately.
  // Otherwise, it's the slow path for us (no success at this point).

  if (super_check_offset.is_register()) {
    beq(super_klass, AT, *L_success);
    addi_d(AT, super_check_offset.as_register(), -sc_offset);
    if (L_failure == &L_fallthrough) {
      beq(AT, R0, *L_slow_path);
    } else {
      bne_far(AT, R0, *L_failure);
      b(*L_slow_path);
    }
  } else if (super_check_offset.as_constant() == sc_offset) {
    // Need a slow path; fast failure is impossible.
    if (L_slow_path == &L_fallthrough) {
      beq(super_klass, AT, *L_success);
    } else {
      bne(super_klass, AT, *L_slow_path);
      b(*L_success);
    }
  } else {
    // No slow path; it's a fast decision.
    if (L_failure == &L_fallthrough) {
      beq(super_klass, AT, *L_success);
    } else {
      bne_far(super_klass, AT, *L_failure);
      b(*L_success);
    }
  }

  bind(L_fallthrough);
}

void MacroAssembler::check_klass_subtype_slow_path(Register sub_klass,
                                                   Register super_klass,
                                                   Register temp_reg,
                                                   Register temp2_reg,
                                                   Label* L_success,
                                                   Label* L_failure,
                                                   bool set_cond_codes) {
  if (temp2_reg == noreg)
    temp2_reg = TSR;
  assert_different_registers(sub_klass, super_klass, temp_reg, temp2_reg);
#define IS_A_TEMP(reg) ((reg) == temp_reg || (reg) == temp2_reg)

  Label L_fallthrough;
  int label_nulls = 0;
  if (L_success == NULL)   { L_success   = &L_fallthrough; label_nulls++; }
  if (L_failure == NULL)   { L_failure   = &L_fallthrough; label_nulls++; }
  assert(label_nulls <= 1, "at most one NULL in the batch");

  // a couple of useful fields in sub_klass:
  int ss_offset = in_bytes(Klass::secondary_supers_offset());
  int sc_offset = in_bytes(Klass::secondary_super_cache_offset());
  Address secondary_supers_addr(sub_klass, ss_offset);
  Address super_cache_addr(     sub_klass, sc_offset);

  // Do a linear scan of the secondary super-klass chain.
  // This code is rarely used, so simplicity is a virtue here.
  // The repne_scan instruction uses fixed registers, which we must spill.
  // Don't worry too much about pre-existing connections with the input regs.

#ifndef PRODUCT
  int* pst_counter = &SharedRuntime::_partial_subtype_ctr;
  ExternalAddress pst_counter_addr((address) pst_counter);
#endif //PRODUCT

  // We will consult the secondary-super array.
  ld_d(temp_reg, secondary_supers_addr);
  // Load the array length.
  ld_w(temp2_reg, Address(temp_reg, Array<Klass*>::length_offset_in_bytes()));
  // Skip to start of data.
  addi_d(temp_reg, temp_reg, Array<Klass*>::base_offset_in_bytes());

  Label Loop, subtype;
  bind(Loop);
  beq(temp2_reg, R0, *L_failure);
  ld_d(AT, temp_reg, 0);
  addi_d(temp_reg, temp_reg, 1 * wordSize);
  beq(AT, super_klass, subtype);
  addi_d(temp2_reg, temp2_reg, -1);
  b(Loop);

  bind(subtype);
  st_d(super_klass, super_cache_addr);
  if (L_success != &L_fallthrough) {
    b(*L_success);
  }

  // Success.  Cache the super we found and proceed in triumph.
#undef IS_A_TEMP

  bind(L_fallthrough);
}

void MacroAssembler::get_vm_result(Register oop_result, Register java_thread) {
  ld_d(oop_result, Address(java_thread, JavaThread::vm_result_offset()));
  st_d(R0, Address(java_thread, JavaThread::vm_result_offset()));
  verify_oop(oop_result, "broken oop in call_VM_base");
}

void MacroAssembler::get_vm_result_2(Register metadata_result, Register java_thread) {
  ld_d(metadata_result, Address(java_thread, JavaThread::vm_result_2_offset()));
  st_d(R0, Address(java_thread, JavaThread::vm_result_2_offset()));
}

Address MacroAssembler::argument_address(RegisterOrConstant arg_slot,
                                         int extra_slot_offset) {
  // cf. TemplateTable::prepare_invoke(), if (load_receiver).
  int stackElementSize = Interpreter::stackElementSize;
  int offset = Interpreter::expr_offset_in_bytes(extra_slot_offset+0);
#ifdef ASSERT
  int offset1 = Interpreter::expr_offset_in_bytes(extra_slot_offset+1);
  assert(offset1 - offset == stackElementSize, "correct arithmetic");
#endif
  Register             scale_reg    = NOREG;
  Address::ScaleFactor scale_factor = Address::no_scale;
  if (arg_slot.is_constant()) {
    offset += arg_slot.as_constant() * stackElementSize;
  } else {
    scale_reg    = arg_slot.as_register();
    scale_factor = Address::times_8;
  }
  // We don't push RA on stack in prepare_invoke.
  //  offset += wordSize;           // return PC is on stack
  if(scale_reg==NOREG) return Address(SP, offset);
  else {
  alsl_d(scale_reg, scale_reg, SP, scale_factor - 1);
  return Address(scale_reg, offset);
  }
}

SkipIfEqual::~SkipIfEqual() {
  _masm->bind(_label);
}

void MacroAssembler::load_sized_value(Register dst, Address src, size_t size_in_bytes, bool is_signed, Register dst2) {
  switch (size_in_bytes) {
  case  8:  ld_d(dst, src); break;
  case  4:  ld_w(dst, src); break;
  case  2:  is_signed ? ld_h(dst, src) : ld_hu(dst, src); break;
  case  1:  is_signed ? ld_b( dst, src) : ld_bu( dst, src); break;
  default:  ShouldNotReachHere();
  }
}

void MacroAssembler::store_sized_value(Address dst, Register src, size_t size_in_bytes, Register src2) {
  switch (size_in_bytes) {
  case  8:  st_d(src, dst); break;
  case  4:  st_w(src, dst); break;
  case  2:  st_h(src, dst); break;
  case  1:  st_b(src, dst); break;
  default:  ShouldNotReachHere();
  }
}

// Look up the method for a megamorphic invokeinterface call.
// The target method is determined by <intf_klass, itable_index>.
// The receiver klass is in recv_klass.
// On success, the result will be in method_result, and execution falls through.
// On failure, execution transfers to the given label.
void MacroAssembler::lookup_interface_method(Register recv_klass,
                                             Register intf_klass,
                                             RegisterOrConstant itable_index,
                                             Register method_result,
                                             Register scan_temp,
                                             Label& L_no_such_interface,
                                             bool return_method) {
  assert_different_registers(recv_klass, intf_klass, scan_temp, AT);
  assert_different_registers(method_result, intf_klass, scan_temp, AT);
  assert(recv_klass != method_result || !return_method,
         "recv_klass can be destroyed when method isn't needed");

  assert(itable_index.is_constant() || itable_index.as_register() == method_result,
         "caller must use same register for non-constant itable index as for method");

  // Compute start of first itableOffsetEntry (which is at the end of the vtable)
  int vtable_base = InstanceKlass::vtable_start_offset() * wordSize;
  int itentry_off = itableMethodEntry::method_offset_in_bytes();
  int scan_step   = itableOffsetEntry::size() * wordSize;
  int vte_size    = vtableEntry::size() * wordSize;
  Address::ScaleFactor times_vte_scale = Address::times_ptr;
  assert(vte_size == wordSize, "else adjust times_vte_scale");

  ld_w(scan_temp, Address(recv_klass, InstanceKlass::vtable_length_offset() * wordSize));

  // %%% Could store the aligned, prescaled offset in the klassoop.
  alsl_d(scan_temp, scan_temp, recv_klass, times_vte_scale - 1);
  addi_d(scan_temp, scan_temp, vtable_base);
  if (HeapWordsPerLong > 1) {
    // Round up to align_object_offset boundary
    // see code for InstanceKlass::start_of_itable!
    round_to(scan_temp, BytesPerLong);
  }

  if (return_method) {
    // Adjust recv_klass by scaled itable_index, so we can free itable_index.
    assert(itableMethodEntry::size() * wordSize == wordSize, "adjust the scaling in the code below");
    if (itable_index.is_constant()) {
      li(AT, (int)itable_index.is_constant());
      alsl_d(AT, AT, recv_klass, (int)Address::times_ptr - 1);
    } else {
      alsl_d(AT, itable_index.as_register(), recv_klass, (int)Address::times_ptr - 1);
    }
    addi_d(recv_klass, AT, itentry_off);
  }

  Label search, found_method;

  for (int peel = 1; peel >= 0; peel--) {
    ld_d(method_result, Address(scan_temp, itableOffsetEntry::interface_offset_in_bytes()));

    if (peel) {
      beq(intf_klass, method_result, found_method);
    } else {
      bne(intf_klass, method_result, search);
      // (invert the test to fall through to found_method...)
    }

    if (!peel)  break;

    bind(search);

    // Check that the previous entry is non-null.  A null entry means that
    // the receiver class doesn't implement the interface, and wasn't the
    // same as when the caller was compiled.
    beq(method_result, R0, L_no_such_interface);
    addi_d(scan_temp, scan_temp, scan_step);
  }

  bind(found_method);

  if (return_method) {
    // Got a hit.
    ld_w(scan_temp, Address(scan_temp, itableOffsetEntry::offset_offset_in_bytes()));
    ldx_d(method_result, recv_klass, scan_temp);
  }
}

// virtual method calling
void MacroAssembler::lookup_virtual_method(Register recv_klass,
                                           RegisterOrConstant vtable_index,
                                           Register method_result) {
  Register tmp = S8;
  push(tmp);

  if (vtable_index.is_constant()) {
    assert_different_registers(recv_klass, method_result, tmp);
  } else {
    assert_different_registers(recv_klass, method_result, vtable_index.as_register(), tmp);
  }
  const int base = InstanceKlass::vtable_start_offset() * wordSize;
  assert(vtableEntry::size() * wordSize == wordSize, "else adjust the scaling in the code below");
  if (vtable_index.is_constant()) {
    li(AT, vtable_index.as_constant());
    slli_d(AT, AT, (int)Address::times_ptr);
  } else {
    slli_d(AT, vtable_index.as_register(), (int)Address::times_ptr);
  }
  li(tmp, base + vtableEntry::method_offset_in_bytes());
  add_d(tmp, tmp, AT);
  add_d(tmp, tmp, recv_klass);
  ld_d(method_result, tmp, 0);

  pop(tmp);
}

void MacroAssembler::load_byte_map_base(Register reg) {
  jbyte *byte_map_base =
    ((CardTableModRefBS*)(Universe::heap()->barrier_set()))->byte_map_base;

  // Strictly speaking the byte_map_base isn't an address at all, and it might
  // even be negative. It is thus materialised as a constant.
  li(reg, (uint64_t)byte_map_base);
}

void MacroAssembler::clear_jweak_tag(Register possibly_jweak) {
  const int32_t inverted_jweak_mask = ~static_cast<int32_t>(JNIHandles::weak_tag_mask);
  STATIC_ASSERT(inverted_jweak_mask == -2); // otherwise check this code
  // The inverted mask is sign-extended
  li(AT, inverted_jweak_mask);
  andr(possibly_jweak, AT, possibly_jweak);
}

void MacroAssembler::resolve_jobject(Register value,
                                     Register thread,
                                     Register tmp) {
  assert_different_registers(value, thread, tmp);
  Label done, not_weak;
  beq(value, R0, done);                // Use NULL as-is.
  li(AT, JNIHandles::weak_tag_mask); // Test for jweak tag.
  andr(AT, value, AT);
  beq(AT, R0, not_weak);
  // Resolve jweak.
  ld_d(value, value, -JNIHandles::weak_tag_value);
  verify_oop(value);
  #if INCLUDE_ALL_GCS
    if (UseG1GC) {
      g1_write_barrier_pre(noreg /* obj */,
                           value /* pre_val */,
                           thread /* thread */,
                           tmp /* tmp */,
                           true /* tosca_live */,
                           true /* expand_call */);
    }
  #endif // INCLUDE_ALL_GCS
  b(done);
  bind(not_weak);
  // Resolve (untagged) jobject.
  ld_d(value, value, 0);
  verify_oop(value);
  bind(done);
}

void MacroAssembler::lea(Register rd, Address src) {
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index == noreg) {
    if (is_simm(disp, 12)) {
      addi_d(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      add_d(dst, base, AT);
    }
  } else {
    if (scale == 0) {
      if (is_simm(disp, 12)) {
        add_d(AT, base, index);
        addi_d(dst, AT, disp);
      } else {
        lu12i_w(AT, split_low20(disp >> 12));
        if (split_low12(disp))
          ori(AT, AT, split_low12(disp));
        add_d(AT, base, AT);
        add_d(dst, AT, index);
      }
    } else {
      if (is_simm(disp, 12)) {
        alsl_d(AT, index, base, scale - 1);
        addi_d(dst, AT, disp);
      } else {
        lu12i_w(AT, split_low20(disp >> 12));
        if (split_low12(disp))
          ori(AT, AT, split_low12(disp));
        add_d(AT, AT, base);
        alsl_d(dst, index, AT, scale - 1);
      }
    }
  }
}

void MacroAssembler::lea(Register dst, AddressLiteral adr) {
  code_section()->relocate(pc(), adr.rspec());
  pcaddi(dst, (adr.target() - pc()) >> 2);
}

int MacroAssembler::patched_branch(int dest_pos, int inst, int inst_pos) {
  int v = (dest_pos - inst_pos) >> 2;
  switch(high(inst, 6)) {
  case beq_op:
  case bne_op:
  case blt_op:
  case bge_op:
  case bltu_op:
  case bgeu_op:
    assert(is_simm16(v), "must be simm16");
#ifndef PRODUCT
    if(!is_simm16(v))
    {
      tty->print_cr("must be simm16");
      tty->print_cr("Inst: %x", inst);
    }
#endif

    inst &= 0xfc0003ff;
    inst |= ((v & 0xffff) << 10);
    break;
  case beqz_op:
  case bnez_op:
  case bccondz_op:
    assert(is_simm(v, 21), "must be simm21");
#ifndef PRODUCT
    if(!is_simm(v, 21))
    {
      tty->print_cr("must be simm21");
      tty->print_cr("Inst: %x", inst);
    }
#endif

    inst &= 0xfc0003e0;
    inst |= ( ((v & 0xffff) << 10) | ((v >> 16) & 0x1f) );
    break;
  case b_op:
  case bl_op:
    assert(is_simm(v, 26), "must be simm26");
#ifndef PRODUCT
    if(!is_simm(v, 26))
    {
      tty->print_cr("must be simm26");
      tty->print_cr("Inst: %x", inst);
    }
#endif

    inst &= 0xfc000000;
    inst |= ( ((v & 0xffff) << 10) | ((v >> 16) & 0x3ff) );
    break;
  default:
    ShouldNotReachHere();
    break;
  }
  return inst;
}

void MacroAssembler::cmp_cmov(Register  op1,
                              Register  op2,
                              Register  dst,
                              Register  src,
                              CMCompare cmp,
                              bool      is_signed) {
  switch (cmp) {
    case EQ:
      sub_d(AT, op1, op2);
      maskeqz(dst, dst, AT);
      masknez(AT, src, AT);
      break;

    case NE:
      sub_d(AT, op1, op2);
      masknez(dst, dst, AT);
      maskeqz(AT, src, AT);
      break;

    case GT:
      if (is_signed) {
        slt(AT, op2, op1);
      } else {
        sltu(AT, op2, op1);
      }
      masknez(dst, dst, AT);
      maskeqz(AT, src, AT);
      break;

    case GE:
      if (is_signed) {
        slt(AT, op1, op2);
      } else {
        sltu(AT, op1, op2);
      }
      maskeqz(dst, dst, AT);
      masknez(AT, src, AT);
      break;

    case LT:
      if (is_signed) {
        slt(AT, op1, op2);
      } else {
        sltu(AT, op1, op2);
      }
      masknez(dst, dst, AT);
      maskeqz(AT, src, AT);
      break;

    case LE:
      if (is_signed) {
        slt(AT, op2, op1);
      } else {
        sltu(AT, op2, op1);
      }
      maskeqz(dst, dst, AT);
      masknez(AT, src, AT);
      break;

    default:
      Unimplemented();
  }
  OR(dst, dst, AT);
}


void MacroAssembler::cmp_cmov(FloatRegister op1,
                              FloatRegister op2,
                              Register      dst,
                              Register      src,
                              FloatRegister tmp1,
                              FloatRegister tmp2,
                              CMCompare     cmp,
                              bool          is_float) {
  movgr2fr_d(tmp1, dst);
  movgr2fr_d(tmp2, src);

  switch(cmp) {
    case EQ:
      if (is_float) {
        fcmp_ceq_s(FCC0, op1, op2);
      } else {
        fcmp_ceq_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp1, tmp2, FCC0);
      break;

    case NE:
      if (is_float) {
        fcmp_ceq_s(FCC0, op1, op2);
      } else {
        fcmp_ceq_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp2, tmp1, FCC0);
      break;

    case GT:
      if (is_float) {
        fcmp_cule_s(FCC0, op1, op2);
      } else {
        fcmp_cule_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp2, tmp1, FCC0);
      break;

    case GE:
      if (is_float) {
        fcmp_cult_s(FCC0, op1, op2);
      } else {
        fcmp_cult_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp2, tmp1, FCC0);
      break;

    case LT:
      if (is_float) {
        fcmp_cult_s(FCC0, op1, op2);
      } else {
        fcmp_cult_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp1, tmp2, FCC0);
      break;

    case LE:
      if (is_float) {
        fcmp_cule_s(FCC0, op1, op2);
      } else {
        fcmp_cule_d(FCC0, op1, op2);
      }
      fsel(tmp1, tmp1, tmp2, FCC0);
      break;

    default:
      Unimplemented();
  }

  movfr2gr_d(dst, tmp1);
}

void MacroAssembler::cmp_cmov(FloatRegister op1,
                              FloatRegister op2,
                              FloatRegister dst,
                              FloatRegister src,
                              CMCompare     cmp,
                              bool          is_float) {
  switch(cmp) {
    case EQ:
      if (!is_float) {
        fcmp_ceq_d(FCC0, op1, op2);
      } else {
        fcmp_ceq_s(FCC0, op1, op2);
      }
      fsel(dst, dst, src, FCC0);
      break;

    case NE:
      if (!is_float) {
        fcmp_ceq_d(FCC0, op1, op2);
      } else {
        fcmp_ceq_s(FCC0, op1, op2);
      }
      fsel(dst, src, dst, FCC0);
      break;

    case GT:
      if (!is_float) {
        fcmp_cule_d(FCC0, op1, op2);
      } else {
        fcmp_cule_s(FCC0, op1, op2);
      }
      fsel(dst, src, dst, FCC0);
      break;

    case GE:
      if (!is_float) {
        fcmp_cult_d(FCC0, op1, op2);
      } else {
        fcmp_cult_s(FCC0, op1, op2);
      }
      fsel(dst, src, dst, FCC0);
      break;

    case LT:
      if (!is_float) {
        fcmp_cult_d(FCC0, op1, op2);
      } else {
        fcmp_cult_s(FCC0, op1, op2);
      }
      fsel(dst, dst, src, FCC0);
      break;

    case LE:
      if (!is_float) {
        fcmp_cule_d(FCC0, op1, op2);
      } else {
        fcmp_cule_s(FCC0, op1, op2);
      }
      fsel(dst, dst, src, FCC0);
      break;

    default:
      Unimplemented();
  }
}

void MacroAssembler::cmp_cmov(Register      op1,
                              Register      op2,
                              FloatRegister dst,
                              FloatRegister src,
                              FloatRegister tmp1,
                              FloatRegister tmp2,
                              CMCompare     cmp) {
  movgr2fr_w(tmp1, R0);

  switch (cmp) {
    case EQ:
      sub_d(AT, op1, op2);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, dst, src, FCC0);
      break;

    case NE:
      sub_d(AT, op1, op2);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, src, dst, FCC0);
      break;

    case GT:
      slt(AT, op2, op1);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, src, dst, FCC0);
      break;

    case GE:
      slt(AT, op1, op2);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, dst, src, FCC0);
      break;

    case LT:
      slt(AT, op1, op2);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, src, dst, FCC0);
      break;

    case LE:
      slt(AT, op2, op1);
      movgr2fr_w(tmp2, AT);
      fcmp_ceq_s(FCC0, tmp1, tmp2);
      fsel(dst, dst, src, FCC0);
      break;

    default:
      Unimplemented();
  }
}

void MacroAssembler::loadstore(Register reg, Register base, int disp, int type) {
  switch (type) {
    case STORE_BYTE:   st_b (reg, base, disp); break;
    case STORE_CHAR:
    case STORE_SHORT:  st_h (reg, base, disp); break;
    case STORE_INT:    st_w (reg, base, disp); break;
    case STORE_LONG:   st_d (reg, base, disp); break;
    case LOAD_BYTE:    ld_b (reg, base, disp); break;
    case LOAD_U_BYTE:  ld_bu(reg, base, disp); break;
    case LOAD_SHORT:   ld_h (reg, base, disp); break;
    case LOAD_U_SHORT: ld_hu(reg, base, disp); break;
    case LOAD_INT:     ld_w (reg, base, disp); break;
    case LOAD_U_INT:   ld_wu(reg, base, disp); break;
    case LOAD_LONG:    ld_d (reg, base, disp); break;
    case LOAD_LINKED_LONG:
      ll_d(reg, base, disp);
      break;
    default:
      ShouldNotReachHere();
  }
}

void MacroAssembler::loadstore(Register reg, Register base, Register disp, int type) {
  switch (type) {
    case STORE_BYTE:   stx_b (reg, base, disp); break;
    case STORE_CHAR:
    case STORE_SHORT:  stx_h (reg, base, disp); break;
    case STORE_INT:    stx_w (reg, base, disp); break;
    case STORE_LONG:   stx_d (reg, base, disp); break;
    case LOAD_BYTE:    ldx_b (reg, base, disp); break;
    case LOAD_U_BYTE:  ldx_bu(reg, base, disp); break;
    case LOAD_SHORT:   ldx_h (reg, base, disp); break;
    case LOAD_U_SHORT: ldx_hu(reg, base, disp); break;
    case LOAD_INT:     ldx_w (reg, base, disp); break;
    case LOAD_U_INT:   ldx_wu(reg, base, disp); break;
    case LOAD_LONG:    ldx_d (reg, base, disp); break;
    case LOAD_LINKED_LONG:
      add_d(AT, base, disp);
      ll_d(reg, AT, 0);
      break;
    default:
      ShouldNotReachHere();
  }
}

void MacroAssembler::loadstore(FloatRegister reg, Register base, int disp, int type) {
  switch (type) {
    case STORE_FLOAT:    fst_s(reg, base, disp); break;
    case STORE_DOUBLE:   fst_d(reg, base, disp); break;
    case STORE_VECTORX:  vst  (reg, base, disp); break;
    case STORE_VECTORY: xvst  (reg, base, disp); break;
    case LOAD_FLOAT:     fld_s(reg, base, disp); break;
    case LOAD_DOUBLE:    fld_d(reg, base, disp); break;
    case LOAD_VECTORX:   vld  (reg, base, disp); break;
    case LOAD_VECTORY:  xvld  (reg, base, disp); break;
    default:
      ShouldNotReachHere();
  }
}

void MacroAssembler::loadstore(FloatRegister reg, Register base, Register disp, int type) {
  switch (type) {
    case STORE_FLOAT:    fstx_s(reg, base, disp); break;
    case STORE_DOUBLE:   fstx_d(reg, base, disp); break;
    case STORE_VECTORX:  vstx  (reg, base, disp); break;
    case STORE_VECTORY: xvstx  (reg, base, disp); break;
    case LOAD_FLOAT:     fldx_s(reg, base, disp); break;
    case LOAD_DOUBLE:    fldx_d(reg, base, disp); break;
    case LOAD_VECTORX:   vldx  (reg, base, disp); break;
    case LOAD_VECTORY:  xvldx  (reg, base, disp); break;
    default:
      ShouldNotReachHere();
  }
}

/**
 * Emits code to update CRC-32 with a byte value according to constants in table
 *
 * @param [in,out]crc   Register containing the crc.
 * @param [in]val       Register containing the byte to fold into the CRC.
 * @param [in]table     Register containing the table of crc constants.
 *
 * uint32_t crc;
 * val = crc_table[(val ^ crc) & 0xFF];
 * crc = val ^ (crc >> 8);
**/
void MacroAssembler::update_byte_crc32(Register crc, Register val, Register table) {
  xorr(val, val, crc);
  andi(val, val, 0xff);
  ld_w(val, Address(table, val, Address::times_4, 0));
  srli_w(crc, crc, 8);
  xorr(crc, val, crc);
}

/**
 * @param crc   register containing existing CRC (32-bit)
 * @param buf   register pointing to input byte buffer (byte*)
 * @param len   register containing number of bytes
 * @param tmp   scratch register
**/
void MacroAssembler::kernel_crc32(Register crc, Register buf, Register len, Register tmp) {
  Label CRC_by64_loop, CRC_by4_loop, CRC_by1_loop, CRC_less64, CRC_by64_pre, CRC_by32_loop, CRC_less32, L_exit;
  assert_different_registers(crc, buf, len, tmp);

    nor(crc, crc, R0);

    addi_d(len, len, -64);
    bge(len, R0, CRC_by64_loop);
    addi_d(len, len, 64-4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    blt(R0, len, CRC_by1_loop);
    b(L_exit);

  bind(CRC_by64_loop);
    ld_d(tmp, buf, 0);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 8);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 16);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 24);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 32);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 40);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 48);
    crc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 56);
    crc_w_d_w(crc, tmp, crc);
    addi_d(buf, buf, 64);
    addi_d(len, len, -64);
    bge(len, R0, CRC_by64_loop);
    addi_d(len, len, 64-4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    blt(R0, len, CRC_by1_loop);
    b(L_exit);

  bind(CRC_by4_loop);
    ld_w(tmp, buf, 0);
    crc_w_w_w(crc, tmp, crc);
    addi_d(buf, buf, 4);
    addi_d(len, len, -4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    bge(R0, len, L_exit);

  bind(CRC_by1_loop);
    ld_b(tmp, buf, 0);
    crc_w_b_w(crc, tmp, crc);
    addi_d(buf, buf, 1);
    addi_d(len, len, -1);
    blt(R0, len, CRC_by1_loop);

  bind(L_exit);
    nor(crc, crc, R0);
}

/**
 * @param crc   register containing existing CRC (32-bit)
 * @param buf   register pointing to input byte buffer (byte*)
 * @param len   register containing number of bytes
 * @param tmp   scratch register
**/
void MacroAssembler::kernel_crc32c(Register crc, Register buf, Register len, Register tmp) {
  Label CRC_by64_loop, CRC_by4_loop, CRC_by1_loop, CRC_less64, CRC_by64_pre, CRC_by32_loop, CRC_less32, L_exit;
  assert_different_registers(crc, buf, len, tmp);

    addi_d(len, len, -64);
    bge(len, R0, CRC_by64_loop);
    addi_d(len, len, 64-4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    blt(R0, len, CRC_by1_loop);
    b(L_exit);

  bind(CRC_by64_loop);
    ld_d(tmp, buf, 0);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 8);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 16);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 24);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 32);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 40);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 48);
    crcc_w_d_w(crc, tmp, crc);
    ld_d(tmp, buf, 56);
    crcc_w_d_w(crc, tmp, crc);
    addi_d(buf, buf, 64);
    addi_d(len, len, -64);
    bge(len, R0, CRC_by64_loop);
    addi_d(len, len, 64-4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    blt(R0, len, CRC_by1_loop);
    b(L_exit);

  bind(CRC_by4_loop);
    ld_w(tmp, buf, 0);
    crcc_w_w_w(crc, tmp, crc);
    addi_d(buf, buf, 4);
    addi_d(len, len, -4);
    bge(len, R0, CRC_by4_loop);
    addi_d(len, len, 4);
    bge(R0, len, L_exit);

  bind(CRC_by1_loop);
    ld_b(tmp, buf, 0);
    crcc_w_b_w(crc, tmp, crc);
    addi_d(buf, buf, 1);
    addi_d(len, len, -1);
    blt(R0, len, CRC_by1_loop);

  bind(L_exit);
}
