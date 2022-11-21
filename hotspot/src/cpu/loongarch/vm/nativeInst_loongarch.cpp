/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "compiler/disassembler.hpp"
#include "memory/resourceArea.hpp"
#include "nativeInst_loongarch.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/ostream.hpp"
#ifdef COMPILER1
#include "c1/c1_Runtime1.hpp"
#endif

#include <sys/mman.h>

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

void NativeInstruction::wrote(int offset) {
  ICache::invalidate_word(addr_at(offset));
}

void NativeInstruction::set_long_at(int offset, long i) {
  address addr = addr_at(offset);
  *(long*)addr = i;
  ICache::invalidate_range(addr, 8);
}

bool NativeInstruction::is_int_branch() {
  int op = Assembler::high(insn_word(), 6);
  return op == Assembler::beqz_op || op == Assembler::bnez_op ||
         op == Assembler::beq_op  || op == Assembler::bne_op  ||
         op == Assembler::blt_op  || op == Assembler::bge_op  ||
         op == Assembler::bltu_op || op == Assembler::bgeu_op;
}

bool NativeInstruction::is_float_branch() {
  return Assembler::high(insn_word(), 6) == Assembler::bccondz_op;
}

bool NativeCall::is_bl() const {
  return Assembler::high(int_at(0), 6) == Assembler::bl_op;
}

void NativeCall::verify() {
  assert(is_bl(), "not a NativeCall");
}

address NativeCall::target_addr_for_bl(address orig_addr) const {
  address addr = orig_addr ? orig_addr : addr_at(0);

  // bl
  if (is_bl()) {
    return addr + (Assembler::simm26(((int_at(0) & 0x3ff) << 16) |
                              ((int_at(0) >> 10) & 0xffff)) << 2);
  }

  fatal("not a NativeCall");
  return NULL;
}

address NativeCall::destination() const {
  address addr = (address)this;
  address destination = target_addr_for_bl();
  // Do we use a trampoline stub for this call?
  // Trampoline stubs are located behind the main code.
  if (destination > addr) {
    // Filter out recursive method invocation (call to verified/unverified entry point).
    CodeBlob* cb = CodeCache::find_blob_unsafe(addr);   // Else we get assertion if nmethod is zombie.
    assert(cb && cb->is_nmethod(), "sanity");
    nmethod *nm = (nmethod *)cb;
    NativeInstruction* ni = nativeInstruction_at(destination);
    if (nm->stub_contains(destination) && ni->is_NativeCallTrampolineStub_at()) {
      // Yes we do, so get the destination from the trampoline stub.
      const address trampoline_stub_addr = destination;
      destination = nativeCallTrampolineStub_at(trampoline_stub_addr)->destination();
    }
  }
  return destination;
}

// Similar to replace_mt_safe, but just changes the destination. The
// important thing is that free-running threads are able to execute this
// call instruction at all times.
//
// Used in the runtime linkage of calls; see class CompiledIC.
//
// Add parameter assert_lock to switch off assertion
// during code generation, where no patching lock is needed.
void NativeCall::set_destination_mt_safe(address dest, bool assert_lock) {
  assert(!assert_lock ||
         (Patching_lock->is_locked() || SafepointSynchronize::is_at_safepoint()),
         "concurrent code patching");

  ResourceMark rm;
  address addr_call = addr_at(0);
  bool reachable = MacroAssembler::reachable_from_branch_short(dest - addr_call);
  assert(NativeCall::is_call_at(addr_call), "unexpected code at call site");

  // Patch the call.
  if (!reachable) {
    address trampoline_stub_addr = get_trampoline();
    assert (trampoline_stub_addr != NULL, "we need a trampoline");
    guarantee(Assembler::is_simm((trampoline_stub_addr - addr_call) >> 2, 26), "cannot reach trampoline stub");

    // Patch the constant in the call's trampoline stub.
    NativeInstruction* ni = nativeInstruction_at(dest);
    assert (! ni->is_NativeCallTrampolineStub_at(), "chained trampolines");
    nativeCallTrampolineStub_at(trampoline_stub_addr)->set_destination(dest);
    dest = trampoline_stub_addr;
  }
  set_destination(dest);
}

address NativeCall::get_trampoline() {
  address call_addr = addr_at(0);

  CodeBlob *code = CodeCache::find_blob(call_addr);
  assert(code != NULL, "Could not find the containing code blob");

  address bl_destination
    = nativeCall_at(call_addr)->target_addr_for_bl();
  NativeInstruction* ni = nativeInstruction_at(bl_destination);
  if (code->contains(bl_destination) &&
      ni->is_NativeCallTrampolineStub_at())
    return bl_destination;

  // If the codeBlob is not a nmethod, this is because we get here from the
  // CodeBlob constructor, which is called within the nmethod constructor.
  return trampoline_stub_Relocation::get_trampoline_for(call_addr, (nmethod*)code);
}

void NativeCall::set_destination(address dest) {
  address addr_call = addr_at(0);
  CodeBuffer cb(addr_call, instruction_size);
  MacroAssembler masm(&cb);
  assert(is_call_at(addr_call), "unexpected call type");
  jlong offs = dest - addr_call;
  masm.bl(offs >> 2);
  ICache::invalidate_range(addr_call, instruction_size);
}

void NativeCall::print() {
  tty->print_cr(PTR_FORMAT ": call " PTR_FORMAT,
                p2i(instruction_address()), p2i(destination()));
}

// Inserts a native call instruction at a given pc
void NativeCall::insert(address code_pos, address entry) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

// MT-safe patching of a call instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
void NativeCall::replace_mt_safe(address instr_addr, address code_buffer) {
  Unimplemented();
}

bool NativeFarCall::is_short() const {
  return Assembler::high(int_at(0), 10) == Assembler::andi_op &&
         Assembler::low(int_at(0), 22) == 0 &&
         Assembler::high(int_at(4), 6) == Assembler::bl_op;
}

bool NativeFarCall::is_far() const {
  return Assembler::high(int_at(0), 7) == Assembler::pcaddu18i_op &&
         Assembler::high(int_at(4), 6) == Assembler::jirl_op      &&
         Assembler::low(int_at(4), 5)  == RA->encoding();
}

address NativeFarCall::destination(address orig_addr) const {
  address addr = orig_addr ? orig_addr : addr_at(0);

  if (is_short()) {
  // short
    return addr + BytesPerInstWord +
           (Assembler::simm26(((int_at(4) & 0x3ff) << 16) |
                              ((int_at(4) >> 10) & 0xffff)) << 2);
  }

  if (is_far()) {
  // far
    return addr + ((intptr_t)Assembler::simm20(int_at(0) >> 5 & 0xfffff) << 18) +
           (Assembler::simm16(int_at(4) >> 10 & 0xffff) << 2);
  }

  fatal("not a NativeFarCall");
  return NULL;
}

void NativeFarCall::set_destination(address dest) {
  address addr_call = addr_at(0);
  CodeBuffer cb(addr_call, instruction_size);
  MacroAssembler masm(&cb);
  assert(is_far_call_at(addr_call), "unexpected call type");
  masm.patchable_call(dest, addr_call);
  ICache::invalidate_range(addr_call, instruction_size);
}

void NativeFarCall::verify() {
  assert(is_short() || is_far(), "not a NativeFarcall");
}

//-------------------------------------------------------------------

bool NativeMovConstReg::is_lu12iw_ori_lu32id() const {
  return Assembler::high(int_at(0), 7)   == Assembler::lu12i_w_op &&
         Assembler::high(int_at(4), 10)  == Assembler::ori_op     &&
         Assembler::high(int_at(8), 7)   == Assembler::lu32i_d_op;
}

bool NativeMovConstReg::is_lu12iw_lu32id_nop() const {
  return Assembler::high(int_at(0), 7)   == Assembler::lu12i_w_op &&
         Assembler::high(int_at(4), 7)   == Assembler::lu32i_d_op &&
         Assembler::high(int_at(8), 10)  == Assembler::andi_op;
}

bool NativeMovConstReg::is_lu12iw_2nop() const {
  return Assembler::high(int_at(0), 7)   == Assembler::lu12i_w_op &&
         Assembler::high(int_at(4), 10)  == Assembler::andi_op    &&
         Assembler::high(int_at(8), 10)  == Assembler::andi_op;
}

bool NativeMovConstReg::is_lu12iw_ori_nop() const {
  return Assembler::high(int_at(0), 7)   == Assembler::lu12i_w_op &&
         Assembler::high(int_at(4), 10)  == Assembler::ori_op     &&
         Assembler::high(int_at(8), 10)  == Assembler::andi_op;
}

bool NativeMovConstReg::is_addid_2nop() const {
  return Assembler::high(int_at(0), 10)  == Assembler::addi_d_op &&
         Assembler::high(int_at(4), 10)  == Assembler::andi_op   &&
         Assembler::high(int_at(8), 10)  == Assembler::andi_op;
}

void NativeMovConstReg::verify() {
  assert(is_li52(), "not a mov reg, imm52");
}

void NativeMovConstReg::print() {
  tty->print_cr(PTR_FORMAT ": mov reg, " INTPTR_FORMAT,
                p2i(instruction_address()), data());
}

intptr_t NativeMovConstReg::data() const {
  if (is_lu12iw_ori_lu32id()) {
    return Assembler::merge((intptr_t)((int_at(4)  >> 10) & 0xfff),
                            (intptr_t)((int_at(0)  >> 5)  & 0xfffff),
                            (intptr_t)((int_at(8)  >> 5)  & 0xfffff));
  }

  if (is_lu12iw_lu32id_nop()) {
    return Assembler::merge((intptr_t)0,
                            (intptr_t)((int_at(0)  >> 5)  & 0xfffff),
                            (intptr_t)((int_at(4)  >> 5)  & 0xfffff));
  }

  if (is_lu12iw_2nop()) {
    return Assembler::merge((intptr_t)0,
                            (intptr_t)((int_at(0)  >> 5)  & 0xfffff));
  }

  if (is_lu12iw_ori_nop()) {
    return Assembler::merge((intptr_t)((int_at(4)  >> 10) & 0xfff),
                            (intptr_t)((int_at(0)  >> 5)  & 0xfffff));
  }

  if (is_addid_2nop()) {
    return Assembler::simm12((int_at(0) >> 10) & 0xfff);
  }

  Disassembler::decode(addr_at(0), addr_at(0) + 16, tty);
  fatal("not a mov reg, imm52");
  return 0; // unreachable
}

void NativeMovConstReg::set_data(intptr_t x, intptr_t o) {
  CodeBuffer cb(addr_at(0), instruction_size);
  MacroAssembler masm(&cb);
  masm.patchable_li52(as_Register(int_at(0) & 0x1f), x);
  ICache::invalidate_range(addr_at(0), instruction_size);

  // Find and replace the oop/metadata corresponding to this
  // instruction in oops section.
  CodeBlob* blob = CodeCache::find_blob_unsafe(instruction_address());
  nmethod* nm = blob->as_nmethod_or_null();
  if (nm != NULL) {
    o = o ? o : x;
    RelocIterator iter(nm, instruction_address(), next_instruction_address());
    while (iter.next()) {
      if (iter.type() == relocInfo::oop_type) {
        oop* oop_addr = iter.oop_reloc()->oop_addr();
        *oop_addr = cast_to_oop(o);
        break;
      } else if (iter.type() == relocInfo::metadata_type) {
        Metadata** metadata_addr = iter.metadata_reloc()->metadata_addr();
        *metadata_addr = (Metadata*)o;
        break;
      }
    }
  }
}

//-------------------------------------------------------------------

int NativeMovRegMem::offset() const{
  //TODO: LA
  guarantee(0, "LA not implemented yet");
  return 0; // mute compiler
}

void NativeMovRegMem::set_offset(int x) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

void NativeMovRegMem::verify() {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}


void NativeMovRegMem::print() {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

bool NativeInstruction::is_sigill_zombie_not_entrant() {
  return uint_at(0) == NativeIllegalInstruction::instruction_code;
}

void NativeIllegalInstruction::insert(address code_pos) {
  *(juint*)code_pos = instruction_code;
  ICache::invalidate_range(code_pos, instruction_size);
}

void NativeJump::verify() {
  assert(is_short() || is_far(), "not a general jump instruction");
}

bool NativeJump::is_short() {
  return Assembler::high(insn_word(), 6) == Assembler::b_op;
}

bool NativeJump::is_far() {
  return Assembler::high(int_at(0), 7) == Assembler::pcaddu18i_op &&
         Assembler::high(int_at(4), 6) == Assembler::jirl_op      &&
         Assembler::low(int_at(4), 5)  == R0->encoding();
}

address NativeJump::jump_destination(address orig_addr) {
  address addr = orig_addr ? orig_addr : addr_at(0);

  // short
  if (is_short()) {
    return addr + (Assembler::simm26(((int_at(0) & 0x3ff) << 16) |
                                     ((int_at(0) >> 10) & 0xffff)) << 2);
  }

  // far
  if (is_far()) {
    return addr + ((intptr_t)Assembler::simm20(int_at(0) >> 5 & 0xfffff) << 18) +
           (Assembler::simm16(int_at(4) >> 10 & 0xffff) << 2);
  }

  fatal("not a jump");
  return NULL;
}

void NativeJump::set_jump_destination(address dest) {
  OrderAccess::fence();

  CodeBuffer cb(addr_at(0), instruction_size);
  MacroAssembler masm(&cb);
  masm.patchable_jump(dest);
  ICache::invalidate_range(addr_at(0), instruction_size);
}

void NativeGeneralJump::insert_unconditional(address code_pos, address entry) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

// MT-safe patching of a long jump instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
void NativeGeneralJump::replace_mt_safe(address instr_addr, address code_buffer) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
}

// Must ensure atomicity
void NativeJump::patch_verified_entry(address entry, address verified_entry, address dest) {
  assert(dest == SharedRuntime::get_handle_wrong_method_stub(), "expected fixed destination of patch");
  jlong offs = dest - verified_entry;

  if (MacroAssembler::reachable_from_branch_short(offs)) {
    CodeBuffer cb(verified_entry, 1 * BytesPerInstWord);
    MacroAssembler masm(&cb);
    masm.b(dest);
  } else {
    // We use an illegal instruction for marking a method as
    // not_entrant or zombie
    NativeIllegalInstruction::insert(verified_entry);
  }
  ICache::invalidate_range(verified_entry, 1 * BytesPerInstWord);
}

bool NativeInstruction::is_dtrace_trap() {
  //return (*(int32_t*)this & 0xff) == 0xcc;
  Unimplemented();
  return false;
}

bool NativeInstruction::is_safepoint_poll() {
  //
  // 390     li   T2, 0x0000000000400000 #@loadConP
  // 394     st_w    [SP + #12], V1    # spill 9
  // 398     Safepoint @ [T2] : poll for GC @ safePoint_poll        # spec.benchmarks.compress.Decompressor::decompress @ bci:224  L[0]=A6 L[1]=_ L[2]=sp + #28 L[3]=_ L[4]=V1
  //
  //  0x000000ffe5815130: lu12i_w  t2, 0x400
  //  0x000000ffe5815134: st_w  v1, 0xc(sp)    ; OopMap{a6=Oop off=920}
  //                                           ;*goto
  //                                           ; - spec.benchmarks.compress.Decompressor::decompress@224 (line 584)
  //
  //  0x000000ffe5815138: ld_w  at, 0x0(t2)    ;*goto       <---  PC
  //                                           ; - spec.benchmarks.compress.Decompressor::decompress@224 (line 584)
  //

  // Since there may be some spill instructions between the safePoint_poll and loadConP,
  // we check the safepoint instruction like this.
  return Assembler::high(insn_word(), 10) == Assembler::ld_w_op &&
         Assembler::low(insn_word(), 5)   == AT->encoding();
}
