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
#include "code/codeBlob.hpp"
#include "code/codeCache.hpp"
#include "compiler/disassembler.hpp"
#include "memory/resourceArea.hpp"
#include "nativeInst_mips.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/ostream.hpp"

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
#define T8 RT8
#define T9 RT9

void NativeInstruction::wrote(int offset) {
  ICache::invalidate_word(addr_at(offset));
}

void NativeInstruction::set_long_at(int offset, long i) {
  address addr = addr_at(offset);
  *(long*)addr = i;
  ICache::invalidate_range(addr, 8);
}

static int illegal_instruction_bits = 0;

int NativeInstruction::illegal_instruction() {
  if (illegal_instruction_bits == 0) {
    ResourceMark rm;
    char buf[40];
    CodeBuffer cbuf((address)&buf[0], 20);
    MacroAssembler* a = new MacroAssembler(&cbuf);
    address ia = a->pc();
    a->brk(11);
    int bits = *(int*)ia;
    illegal_instruction_bits = bits;
  }
  return illegal_instruction_bits;
}

bool NativeInstruction::is_int_branch() {
  switch(Assembler::opcode(insn_word())) {
    case Assembler::beq_op:
    case Assembler::beql_op:
    case Assembler::bgtz_op:
    case Assembler::bgtzl_op:
    case Assembler::blez_op:
    case Assembler::blezl_op:
    case Assembler::bne_op:
    case Assembler::bnel_op:
      return true;
    case Assembler::regimm_op:
      switch(Assembler::rt(insn_word())) {
        case Assembler::bgez_op:
        case Assembler::bgezal_op:
        case Assembler::bgezall_op:
        case Assembler::bgezl_op:
        case Assembler::bltz_op:
        case Assembler::bltzal_op:
        case Assembler::bltzall_op:
        case Assembler::bltzl_op:
          return true;
      }
  }

  return false;
}

bool NativeInstruction::is_float_branch() {
  if (!is_op(Assembler::cop1_op) ||
      !is_rs((Register)Assembler::bc1f_op)) return false;

  switch(Assembler::rt(insn_word())) {
    case Assembler::bcf_op:
    case Assembler::bcfl_op:
    case Assembler::bct_op:
    case Assembler::bctl_op:
      return true;
  }

  return false;
}


void NativeCall::verify() {
  // make sure code pattern is actually a call instruction

  // nop
  // nop
  // nop
  // nop
  // jal target
  // nop
  if ( is_nop() &&
  nativeInstruction_at(addr_at(4))->is_nop()   &&
  nativeInstruction_at(addr_at(8))->is_nop()   &&
  nativeInstruction_at(addr_at(12))->is_nop()  &&
  is_op(int_at(16), Assembler::jal_op)  &&
  nativeInstruction_at(addr_at(20))->is_nop() ) {
      return;
  }

  // jal targe
  // nop
  if ( is_op(int_at(0), Assembler::jal_op)  &&
  nativeInstruction_at(addr_at(4))->is_nop() ) {
      return;
  }

  // li64
  if ( is_op(Assembler::lui_op) &&
  is_op(int_at(4), Assembler::ori_op) &&
  is_special_op(int_at(8), Assembler::dsll_op) &&
  is_op(int_at(12), Assembler::ori_op) &&
  is_special_op(int_at(16), Assembler::dsll_op) &&
  is_op(int_at(20), Assembler::ori_op) &&
        is_special_op(int_at(24), Assembler::jalr_op) ) {
      return;
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  if (  is_op(Assembler::lui_op) &&
    is_op  (int_at(4), Assembler::ori_op) &&
    is_special_op(int_at(8), Assembler::dsll_op) &&
    is_op  (int_at(12), Assembler::ori_op) &&
          is_special_op(int_at(16), Assembler::jalr_op) ) {
      return;
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  //nop
  if (  is_op(Assembler::ori_op) &&
    is_special_op(int_at(4), Assembler::dsll_op) &&
    is_op  (int_at(8), Assembler::ori_op) &&
          nativeInstruction_at(addr_at(12))->is_nop() &&
          is_special_op(int_at(16), Assembler::jalr_op) ) {
      return;
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //nop
  //nop
  if (  is_op(Assembler::ori_op) &&
    is_special_op(int_at(4), Assembler::dsll_op) &&
    nativeInstruction_at(addr_at(8))->is_nop()   &&
          nativeInstruction_at(addr_at(12))->is_nop() &&
          is_special_op(int_at(16), Assembler::jalr_op) ) {
      return;
  }

  //daddiu dst, R0, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::daddiu_op) &&
    nativeInstruction_at(addr_at(4))->is_nop() &&
    nativeInstruction_at(addr_at(8))->is_nop() &&
    nativeInstruction_at(addr_at(12))->is_nop() &&
          is_special_op(int_at(16), Assembler::jalr_op) ) {
      return;
  }

  // FIXME: why add jr_op here?
  //daddiu dst, R0, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::daddiu_op) &&
    nativeInstruction_at(addr_at(4))->is_nop() &&
    nativeInstruction_at(addr_at(8))->is_nop() &&
    nativeInstruction_at(addr_at(12))->is_nop() &&
          is_special_op(int_at(16), Assembler::jr_op) ) {
      return;
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
    is_op  (int_at(4), Assembler::ori_op) &&
    nativeInstruction_at(addr_at(8))->is_nop() &&
    nativeInstruction_at(addr_at(12))->is_nop() &&
          is_special_op(int_at(16), Assembler::jalr_op) ) {
      return;
  }

  //lui dst, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
    nativeInstruction_at(addr_at(4))->is_nop() &&
    nativeInstruction_at(addr_at(8))->is_nop() &&
    nativeInstruction_at(addr_at(12))->is_nop() &&
          is_special_op(int_at(16), Assembler::jalr_op) ) {
      return;
  }

  //daddiu dst, R0, imm16
  //nop
  if (  is_op(Assembler::daddiu_op) &&
          nativeInstruction_at(addr_at(4))->is_nop() &&
          is_special_op(int_at(8), Assembler::jalr_op) ) {
      return;
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  if (  is_op(Assembler::lui_op) &&
          is_op (int_at(4), Assembler::ori_op) &&
          is_special_op(int_at(8), Assembler::jalr_op) ) {
      return;
  }

  //lui dst, imm16
  //nop
  if (  is_op(Assembler::lui_op) &&
          nativeInstruction_at(addr_at(4))->is_nop() &&
          is_special_op(int_at(8), Assembler::jalr_op) ) {
      return;
  }

  if (nativeInstruction_at(addr_at(0))->is_trampoline_call())
    return;

  fatal("not a call");
}

address NativeCall::target_addr_for_insn() const {
  // jal target
  // nop
  if ( is_op(int_at(0), Assembler::jal_op)         &&
  nativeInstruction_at(addr_at(4))->is_nop()) {
      int instr_index = int_at(0) & 0x3ffffff;
      intptr_t target_high = ((intptr_t)addr_at(4)) & 0xfffffffff0000000;
      intptr_t target = target_high | (instr_index << 2);
      return (address)target;
  }

  // nop
  // nop
  // nop
  // nop
  // jal target
  // nop
  if ( nativeInstruction_at(addr_at(0))->is_nop() &&
  nativeInstruction_at(addr_at(4))->is_nop()   &&
  nativeInstruction_at(addr_at(8))->is_nop()   &&
  nativeInstruction_at(addr_at(12))->is_nop()  &&
  is_op(int_at(16), Assembler::jal_op)         &&
  nativeInstruction_at(addr_at(20))->is_nop()) {
      int instr_index = int_at(16) & 0x3ffffff;
      intptr_t target_high = ((intptr_t)addr_at(20)) & 0xfffffffff0000000;
      intptr_t target = target_high | (instr_index << 2);
      return (address)target;
  }

  // li64
  if ( is_op(Assembler::lui_op) &&
        is_op(int_at(4), Assembler::ori_op) &&
        is_special_op(int_at(8), Assembler::dsll_op) &&
        is_op(int_at(12), Assembler::ori_op) &&
        is_special_op(int_at(16), Assembler::dsll_op) &&
        is_op(int_at(20), Assembler::ori_op) ) {

      return (address)Assembler::merge( (intptr_t)(int_at(20) & 0xffff),
                               (intptr_t)(int_at(12) & 0xffff),
                               (intptr_t)(int_at(4) & 0xffff),
                               (intptr_t)(int_at(0) & 0xffff));
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  if (  is_op(Assembler::lui_op) &&
          is_op (int_at(4), Assembler::ori_op) &&
          is_special_op(int_at(8), Assembler::dsll_op) &&
          is_op (int_at(12), Assembler::ori_op) ) {

      return (address)Assembler::merge( (intptr_t)(int_at(12) & 0xffff),
                               (intptr_t)(int_at(4) & 0xffff),
                               (intptr_t)(int_at(0) & 0xffff),
                               (intptr_t)0);
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //dsll dst, dst, 16
  //ld dst, dst, imm16
  if (  is_op(Assembler::lui_op) &&
          is_op (int_at(4), Assembler::ori_op) &&
          is_special_op(int_at(8), Assembler::dsll_op) &&
          is_op (int_at(12), Assembler::ld_op) ) {

      address dest = (address)Assembler::merge( (intptr_t)0,
                               (intptr_t)(int_at(4) & 0xffff),
                               (intptr_t)(int_at(0) & 0xffff),
                               (intptr_t)0);
      return dest + Assembler::simm16((intptr_t)int_at(12) & 0xffff);
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  //nop
  if (  is_op(Assembler::ori_op) &&
          is_special_op(int_at(4), Assembler::dsll_op) &&
          is_op (int_at(8), Assembler::ori_op) &&
          nativeInstruction_at(addr_at(12))->is_nop()) {

      return (address)Assembler::merge( (intptr_t)(int_at(8) & 0xffff),
                               (intptr_t)(int_at(0) & 0xffff),
                               (intptr_t)0,
                               (intptr_t)0);
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //nop
  //nop
  if (  is_op(Assembler::ori_op) &&
          is_special_op(int_at(4), Assembler::dsll_op) &&
          nativeInstruction_at(addr_at(8))->is_nop()   &&
          nativeInstruction_at(addr_at(12))->is_nop()) {

      return (address)Assembler::merge( (intptr_t)(0),
                               (intptr_t)(int_at(0) & 0xffff),
                               (intptr_t)0,
                               (intptr_t)0);
  }

  //daddiu dst, R0, imm16
  //nop
  //nop  <-- optional
  //nop  <-- optional
  if (  is_op(Assembler::daddiu_op) &&
          nativeInstruction_at(addr_at(4))->is_nop() ) {

      int sign = int_at(0) & 0x8000;
      if (sign == 0) {
         return (address)Assembler::merge( (intptr_t)(int_at(0) & 0xffff),
                                  (intptr_t)0,
                                  (intptr_t)0,
                                  (intptr_t)0);
      } else {
         return (address)Assembler::merge( (intptr_t)(int_at(0) & 0xffff),
                                  (intptr_t)(0xffff),
                                  (intptr_t)(0xffff),
                                  (intptr_t)(0xffff));
      }
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //nop  <-- optional
  //nop  <-- optional
  if (  is_op(Assembler::lui_op) &&
          is_op (int_at(4), Assembler::ori_op) ) {

      int sign = int_at(0) & 0x8000;
      if (sign == 0) {
         return (address)Assembler::merge( (intptr_t)(int_at(4) & 0xffff),
                                  (intptr_t)(int_at(0) & 0xffff),
                                  (intptr_t)0,
                                  (intptr_t)0);
      } else {
         return (address)Assembler::merge( (intptr_t)(int_at(4) & 0xffff),
                                  (intptr_t)(int_at(0) & 0xffff),
                                  (intptr_t)(0xffff),
                                  (intptr_t)(0xffff));
      }
  }

  //lui dst, imm16
  //nop
  //nop  <-- optional
  //nop  <-- optional
  if (  is_op(Assembler::lui_op) &&
          nativeInstruction_at(addr_at(4))->is_nop() ) {

      int sign = int_at(0) & 0x8000;
      if (sign == 0) {
         return (address)Assembler::merge( (intptr_t)0,
                                  (intptr_t)(int_at(0) & 0xffff),
                                  (intptr_t)0,
                                  (intptr_t)0);
      } else {
         return (address)Assembler::merge( (intptr_t)0,
                                  (intptr_t)(int_at(0) & 0xffff),
                                  (intptr_t)(0xffff),
                                  (intptr_t)(0xffff));
      }
  }

  tty->print_cr("not a call: addr = " INTPTR_FORMAT , p2i(addr_at(0)));
  tty->print_cr("======= Start decoding at addr = " INTPTR_FORMAT " =======", p2i(addr_at(0)));
  Disassembler::decode(addr_at(0) - 2 * 4, addr_at(0) + 8 * 4, tty);
  tty->print_cr("======= End of decoding =======");
  fatal("not a call");
  return NULL;
}

// Extract call destination from a NativeCall. The call might use a trampoline stub.
address NativeCall::destination() const {
  address addr = (address)this;
  address destination = target_addr_for_insn();
  // Do we use a trampoline stub for this call?
  // Trampoline stubs are located behind the main code.
  if (destination > addr) {
    // Filter out recursive method invocation (call to verified/unverified entry point).
    CodeBlob* cb = CodeCache::find_blob_unsafe(addr);   // Else we get assertion if nmethod is zombie.
    assert(cb && cb->is_nmethod(), "sanity");
    nmethod *nm = (nmethod *)cb;
    NativeInstruction* ni = nativeInstruction_at(addr);
    if (nm->stub_contains(destination) && ni->is_trampoline_call()) {
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
  assert(NativeCall::is_call_at(addr_call), "unexpected code at call site");
  // Patch the constant in the call's trampoline stub.
  if (MacroAssembler::reachable_from_cache()) {
    set_destination(dest);
  } else {
    address trampoline_stub_addr = nativeCall_at(addr_call)->target_addr_for_insn();
    assert (get_trampoline() != NULL && trampoline_stub_addr == get_trampoline(), "we need a trampoline");
    nativeCallTrampolineStub_at(trampoline_stub_addr)->set_destination(dest);
  }
}


address NativeCall::get_trampoline() {
  address call_addr = addr_at(0);

  CodeBlob *code = CodeCache::find_blob(call_addr);
  assert(code != NULL, "Could not find the containing code blob");

  // If the codeBlob is not a nmethod, this is because we get here from the
  // CodeBlob constructor, which is called within the nmethod constructor.
  return trampoline_stub_Relocation::get_trampoline_for(call_addr, (nmethod*)code);
}

// manual implementation of GSSQ
//
//  00000001200009c0 <atomic_store128>:
//     1200009c0:   0085202d        daddu   a0, a0, a1
//     1200009c4:   e8860027        gssq    a2, a3, 0(a0)
//     1200009c8:   03e00008        jr      ra
//     1200009cc:   00000000        nop
//
typedef void (* atomic_store128_ptr)(long *addr, int offset, long low64, long hi64);

static int *buf;

static atomic_store128_ptr get_atomic_store128_func() {
  assert(UseLEXT1, "UseLEXT1 must be true");
  static atomic_store128_ptr p = NULL;
  if (p != NULL)
    return p;

  buf = (int *)mmap(NULL, 1024, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS,
                       -1, 0);
  buf[0] = 0x0085202d;
  buf[1] = (0x3a << 26) | (4 << 21) | (6 << 16) | 0x27;   /* gssq $a2, $a3, 0($a0) */
  buf[2] = 0x03e00008;
  buf[3] = 0;

  asm("sync");
  p = (atomic_store128_ptr)buf;
  return p;
}

void  NativeCall::patch_on_jal_only(address dst) {
  long dest = ((long)dst - (((long)addr_at(4)) & 0xfffffffff0000000))>>2;
  if ((dest >= 0) && (dest < (1<<26))) {
    jint jal_inst = (Assembler::jal_op << 26) | dest;
    set_int_at(0, jal_inst);
    ICache::invalidate_range(addr_at(0), 4);
  } else {
    ShouldNotReachHere();
  }
}

void  NativeCall::patch_on_trampoline(address dest) {
  assert(nativeInstruction_at(addr_at(0))->is_trampoline_call(), "unexpected code at call site");
  jlong dst = (jlong) dest;
  //lui dst, imm16
  //ori dst, dst, imm16
  //dsll dst, dst, 16
  //ld dst, dst, imm16
  if ((dst> 0) && Assembler::is_simm16(dst >> 32)) {
    dst += (dst & 0x8000) << 1;
    set_int_at(0, (int_at(0) & 0xffff0000) | (Assembler::split_low(dst >> 32) & 0xffff));
    set_int_at(4, (int_at(4) & 0xffff0000) | (Assembler::split_low(dst >> 16) & 0xffff));
    set_int_at(12, (int_at(12) & 0xffff0000) | (Assembler::split_low(dst) & 0xffff));

    ICache::invalidate_range(addr_at(0), 24);
  } else {
    ShouldNotReachHere();
  }
}

void  NativeCall::patch_on_jal_gs(address dst) {
  long dest = ((long)dst - (((long)addr_at(20)) & 0xfffffffff0000000))>>2;
  if ((dest >= 0) && (dest < (1<<26))) {
    jint jal_inst = (Assembler::jal_op << 26) | dest;
    set_int_at(16, jal_inst);
    ICache::invalidate_range(addr_at(16), 4);
  } else {
    ShouldNotReachHere();
  }
}

void  NativeCall::patch_on_jal(address dst) {
  patch_on_jal_gs(dst);
}

void  NativeCall::patch_on_jalr_gs(address dst) {
  patch_set48_gs(dst);
}

void  NativeCall::patch_on_jalr(address dst) {
  patch_set48(dst);
}

void  NativeCall::patch_set48_gs(address dest) {
  jlong value = (jlong) dest;
  int  rt_reg = (int_at(0) & (0x1f << 16));

  if (rt_reg == 0) rt_reg = 25 << 16; // r25 is T9

  int  rs_reg = rt_reg << 5;
  int  rd_reg = rt_reg >> 5;

  int hi = (int)(value >> 32);
  int lo = (int)(value & ~0);
  int count = 0;
  int insts[4] = {0, 0, 0, 0};

  if (value == lo) {  // 32-bit integer
    if (Assembler::is_simm16(value)) {
      insts[count] = (Assembler::daddiu_op << 26) | rt_reg | Assembler::split_low(value);
      count += 1;
    } else {
      insts[count] = (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 16);
      count += 1;
      if (Assembler::split_low(value)) {
        insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value);
        count += 1;
      }
    }
  } else if (hi == 0) {  // hardware zero-extends to upper 32
    insts[count] = (Assembler::ori_op << 26) | rt_reg | Assembler::split_low(julong(value) >> 16);
    count += 1;
    insts[count] = (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6);
    count += 1;
    if (Assembler::split_low(value)) {
      insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value);
      count += 1;
    }
  } else if ((value> 0) && Assembler::is_simm16(value >> 32)) {
    insts[count] = (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 32);
    count += 1;
    insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value >> 16);
    count += 1;
    insts[count] = (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6);
    count += 1;
    insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value);
    count += 1;
  } else {
    tty->print_cr("dest = 0x%lx", value);
    guarantee(false, "Not supported yet !");
  }

  while (count < 4) {
    insts[count] = 0;
    count++;
  }

  guarantee(((long)addr_at(0) % (BytesPerWord * 2)) == 0, "must be aligned");
  atomic_store128_ptr func = get_atomic_store128_func();
  (*func)((long *)addr_at(0), 0, *(long *)&insts[0], *(long *)&insts[2]);

  ICache::invalidate_range(addr_at(0), 16);
}

void NativeCall::patch_set32_gs(address dest) {
  jlong value = (jlong) dest;
  int  rt_reg = (int_at(0) & (0x1f << 16));

  if (rt_reg == 0) rt_reg = 25 << 16; // r25 is T9

  int  rs_reg = rt_reg << 5;
  int  rd_reg = rt_reg >> 5;

  int hi = (int)(value >> 32);
  int lo = (int)(value & ~0);

  int count = 0;

  int insts[2] = {0, 0};

  if (value == lo) {  // 32-bit integer
    if (Assembler::is_simm16(value)) {
      //daddiu(d, R0, value);
      //set_int_at(count << 2, (Assembler::daddiu_op << 26) | rt_reg | Assembler::split_low(value));
      insts[count] = (Assembler::daddiu_op << 26) | rt_reg | Assembler::split_low(value);
      count += 1;
    } else {
      //lui(d, split_low(value >> 16));
      //set_int_at(count << 2, (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 16));
      insts[count] = (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 16);
      count += 1;
      if (Assembler::split_low(value)) {
        //ori(d, d, split_low(value));
        //set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
        insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value);
        count += 1;
      }
    }
  } else {
    tty->print_cr("dest = 0x%lx", value);
    guarantee(false, "Not supported yet !");
  }

  while (count < 2) {
    //nop();
    //set_int_at(count << 2, 0);
    insts[count] = 0;
    count++;
  }

  long inst = insts[1];
  inst = inst << 32;
  inst = inst + insts[0];

  set_long_at(0, inst);
}

void NativeCall::patch_set48(address dest) {
  jlong value = (jlong) dest;
  int  rt_reg = (int_at(0) & (0x1f << 16));

  if (rt_reg == 0) rt_reg = 25 << 16; // r25 is T9

  int  rs_reg = rt_reg << 5;
  int  rd_reg = rt_reg >> 5;

  int hi = (int)(value >> 32);
  int lo = (int)(value & ~0);

  int count = 0;

  if (value == lo) {  // 32-bit integer
    if (Assembler::is_simm16(value)) {
      //daddiu(d, R0, value);
      set_int_at(count << 2, (Assembler::daddiu_op << 26) | rt_reg | Assembler::split_low(value));
      count += 1;
    } else {
      //lui(d, split_low(value >> 16));
      set_int_at(count << 2, (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 16));
      count += 1;
      if (Assembler::split_low(value)) {
        //ori(d, d, split_low(value));
        set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
        count += 1;
      }
    }
  } else if (hi == 0) {  // hardware zero-extends to upper 32
      //ori(d, R0, julong(value) >> 16);
      set_int_at(count << 2, (Assembler::ori_op << 26) | rt_reg | Assembler::split_low(julong(value) >> 16));
      count += 1;
      //dsll(d, d, 16);
      set_int_at(count << 2, (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6));
      count += 1;
      if (Assembler::split_low(value)) {
        //ori(d, d, split_low(value));
        set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
        count += 1;
      }
  } else if ((value> 0) && Assembler::is_simm16(value >> 32)) {
    //lui(d, value >> 32);
    set_int_at(count << 2, (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 32));
    count += 1;
    //ori(d, d, split_low(value >> 16));
    set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value >> 16));
    count += 1;
    //dsll(d, d, 16);
    set_int_at(count << 2, (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6));
    count += 1;
    //ori(d, d, split_low(value));
    set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
    count += 1;
  } else {
    tty->print_cr("dest = 0x%lx", value);
    guarantee(false, "Not supported yet !");
  }

  while (count < 4) {
    //nop();
    set_int_at(count << 2, 0);
    count++;
  }

  ICache::invalidate_range(addr_at(0), 16);
}

void  NativeCall::patch_set32(address dest) {
  patch_set32_gs(dest);
}

void  NativeCall::set_destination(address dest) {
  OrderAccess::fence();

  // li64
  if (is_special_op(int_at(16), Assembler::dsll_op)) {
    int first_word = int_at(0);
    set_int_at(0, 0x1000ffff); /* .1: b .1 */
    set_int_at(4, (int_at(4) & 0xffff0000) | (Assembler::split_low((intptr_t)dest >> 32) & 0xffff));
    set_int_at(12, (int_at(12) & 0xffff0000) | (Assembler::split_low((intptr_t)dest >> 16) & 0xffff));
    set_int_at(20, (int_at(20) & 0xffff0000) | (Assembler::split_low((intptr_t)dest) & 0xffff));
    set_int_at(0, (first_word & 0xffff0000) | (Assembler::split_low((intptr_t)dest >> 48) & 0xffff));
    ICache::invalidate_range(addr_at(0), 24);
  } else if (is_op(int_at(16), Assembler::jal_op)) {
    if (UseLEXT1) {
      patch_on_jal_gs(dest);
    } else {
      patch_on_jal(dest);
    }
  } else if (is_op(int_at(0), Assembler::jal_op)) {
    patch_on_jal_only(dest);
  } else if (is_special_op(int_at(16), Assembler::jalr_op)) {
    if (UseLEXT1) {
      patch_on_jalr_gs(dest);
    } else {
      patch_on_jalr(dest);
    }
  } else if (is_special_op(int_at(8), Assembler::jalr_op)) {
    guarantee(!os::is_MP() || (((long)addr_at(0) % 8) == 0), "destination must be aligned by 8");
    if (UseLEXT1) {
      patch_set32_gs(dest);
    } else {
      patch_set32(dest);
    }
    ICache::invalidate_range(addr_at(0), 8);
  } else {
      fatal("not a call");
  }
}

void NativeCall::print() {
  tty->print_cr(PTR_FORMAT ": call " PTR_FORMAT,
                p2i(instruction_address()), p2i(destination()));
}

// Inserts a native call instruction at a given pc
void NativeCall::insert(address code_pos, address entry) {
  NativeCall *call = nativeCall_at(code_pos);
  CodeBuffer cb(call->addr_at(0), instruction_size);
  MacroAssembler masm(&cb);
#define __ masm.
  __ li48(T9, (long)entry);
  __ jalr ();
  __ delayed()->nop();
#undef __

  ICache::invalidate_range(call->addr_at(0), instruction_size);
}

// MT-safe patching of a call instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
void NativeCall::replace_mt_safe(address instr_addr, address code_buffer) {
  Unimplemented();
}

//-------------------------------------------------------------------

void NativeMovConstReg::verify() {
  // li64
  if ( is_op(Assembler::lui_op) &&
       is_op(int_at(4), Assembler::ori_op) &&
       is_special_op(int_at(8), Assembler::dsll_op) &&
       is_op(int_at(12), Assembler::ori_op) &&
       is_special_op(int_at(16), Assembler::dsll_op) &&
       is_op(int_at(20), Assembler::ori_op) ) {
    return;
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  if (  is_op(Assembler::lui_op) &&
        is_op  (int_at(4), Assembler::ori_op) &&
        is_special_op(int_at(8), Assembler::dsll_op) &&
        is_op  (int_at(12), Assembler::ori_op) ) {
    return;
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  //nop
  if (  is_op(Assembler::ori_op) &&
        is_special_op(int_at(4), Assembler::dsll_op) &&
        is_op  (int_at(8), Assembler::ori_op) &&
        nativeInstruction_at(addr_at(12))->is_nop()) {
    return;
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //nop
  //nop
  if (  is_op(Assembler::ori_op) &&
        is_special_op(int_at(4), Assembler::dsll_op) &&
        nativeInstruction_at(addr_at(8))->is_nop()   &&
        nativeInstruction_at(addr_at(12))->is_nop()) {
    return;
  }

  //daddiu dst, R0, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::daddiu_op) &&
        nativeInstruction_at(addr_at(4))->is_nop() &&
        nativeInstruction_at(addr_at(8))->is_nop() &&
        nativeInstruction_at(addr_at(12))->is_nop() ) {
    return;
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
        is_op  (int_at(4), Assembler::ori_op) &&
        nativeInstruction_at(addr_at(8))->is_nop() &&
        nativeInstruction_at(addr_at(12))->is_nop() ) {
    return;
  }

  //lui dst, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
        nativeInstruction_at(addr_at(4))->is_nop() &&
        nativeInstruction_at(addr_at(8))->is_nop() &&
        nativeInstruction_at(addr_at(12))->is_nop() ) {
    return;
  }

  fatal("not a mov reg, imm64/imm48");
}

void NativeMovConstReg::print() {
  tty->print_cr(PTR_FORMAT ": mov reg, " INTPTR_FORMAT,
                p2i(instruction_address()), data());
}

intptr_t NativeMovConstReg::data() const {
  // li64
  if ( is_op(Assembler::lui_op) &&
        is_op(int_at(4), Assembler::ori_op) &&
        is_special_op(int_at(8), Assembler::dsll_op) &&
        is_op(int_at(12), Assembler::ori_op) &&
        is_special_op(int_at(16), Assembler::dsll_op) &&
        is_op(int_at(20), Assembler::ori_op) ) {

    return Assembler::merge( (intptr_t)(int_at(20) & 0xffff),
                             (intptr_t)(int_at(12) & 0xffff),
                             (intptr_t)(int_at(4) & 0xffff),
                             (intptr_t)(int_at(0) & 0xffff));
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  if (  is_op(Assembler::lui_op) &&
          is_op (int_at(4), Assembler::ori_op) &&
          is_special_op(int_at(8), Assembler::dsll_op) &&
          is_op (int_at(12), Assembler::ori_op) ) {

    return Assembler::merge( (intptr_t)(int_at(12) & 0xffff),
                 (intptr_t)(int_at(4) & 0xffff),
           (intptr_t)(int_at(0) & 0xffff),
           (intptr_t)0);
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  //nop
  if (  is_op(Assembler::ori_op) &&
          is_special_op(int_at(4), Assembler::dsll_op) &&
          is_op (int_at(8), Assembler::ori_op) &&
          nativeInstruction_at(addr_at(12))->is_nop()) {

    return Assembler::merge( (intptr_t)(int_at(8) & 0xffff),
                             (intptr_t)(int_at(0) & 0xffff),
                             (intptr_t)0,
                             (intptr_t)0);
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //nop
  //nop
  if (  is_op(Assembler::ori_op) &&
          is_special_op(int_at(4), Assembler::dsll_op) &&
          nativeInstruction_at(addr_at(8))->is_nop()   &&
          nativeInstruction_at(addr_at(12))->is_nop()) {

    return Assembler::merge( (intptr_t)(0),
                             (intptr_t)(int_at(0) & 0xffff),
                             (intptr_t)0,
                             (intptr_t)0);
  }

  //daddiu dst, R0, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::daddiu_op) &&
          nativeInstruction_at(addr_at(4))->is_nop() &&
          nativeInstruction_at(addr_at(8))->is_nop() &&
          nativeInstruction_at(addr_at(12))->is_nop() ) {

    int sign = int_at(0) & 0x8000;
    if (sign == 0) {
     return Assembler::merge( (intptr_t)(int_at(0) & 0xffff),
                              (intptr_t)0,
                              (intptr_t)0,
                              (intptr_t)0);
    } else {
     return Assembler::merge( (intptr_t)(int_at(0) & 0xffff),
                              (intptr_t)(0xffff),
                              (intptr_t)(0xffff),
                              (intptr_t)(0xffff));
    }
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
        is_op (int_at(4), Assembler::ori_op) &&
        nativeInstruction_at(addr_at(8))->is_nop() &&
        nativeInstruction_at(addr_at(12))->is_nop() ) {

    int sign = int_at(0) & 0x8000;
    if (sign == 0) {
      return Assembler::merge( (intptr_t)(int_at(4) & 0xffff),
                               (intptr_t)(int_at(0) & 0xffff),
                               (intptr_t)0,
                               (intptr_t)0);
    } else {
      return Assembler::merge( (intptr_t)(int_at(4) & 0xffff),
                               (intptr_t)(int_at(0) & 0xffff),
                               (intptr_t)(0xffff),
                               (intptr_t)(0xffff));
    }
  }

  //lui dst, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
        nativeInstruction_at(addr_at(4))->is_nop() &&
        nativeInstruction_at(addr_at(8))->is_nop() &&
        nativeInstruction_at(addr_at(12))->is_nop() ) {

    int sign = int_at(0) & 0x8000;
    if (sign == 0) {
      return Assembler::merge( (intptr_t)0,
                               (intptr_t)(int_at(0) & 0xffff),
                               (intptr_t)0,
                               (intptr_t)0);
    } else {
      return Assembler::merge( (intptr_t)0,
                               (intptr_t)(int_at(0) & 0xffff),
                               (intptr_t)(0xffff),
                               (intptr_t)(0xffff));
    }
  }

  fatal("not a mov reg, imm64/imm48");
  return 0; // unreachable
}

void NativeMovConstReg::patch_set48(intptr_t x) {
  jlong value = (jlong) x;
  int  rt_reg = (int_at(0) & (0x1f << 16));
  int  rs_reg = rt_reg << 5;
  int  rd_reg = rt_reg >> 5;

  int hi = (int)(value >> 32);
  int lo = (int)(value & ~0);

  int count = 0;

  if (value == lo) {  // 32-bit integer
    if (Assembler::is_simm16(value)) {
      //daddiu(d, R0, value);
      set_int_at(count << 2, (Assembler::daddiu_op << 26) | rt_reg | Assembler::split_low(value));
      count += 1;
    } else {
      //lui(d, split_low(value >> 16));
      set_int_at(count << 2, (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 16));
      count += 1;
      if (Assembler::split_low(value)) {
        //ori(d, d, split_low(value));
        set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
        count += 1;
      }
    }
  } else if (hi == 0) {  // hardware zero-extends to upper 32
    set_int_at(count << 2, (Assembler::ori_op << 26) | rt_reg | Assembler::split_low(julong(value) >> 16));
    count += 1;
    set_int_at(count << 2, (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6));
    count += 1;
    if (Assembler::split_low(value)) {
      set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
      count += 1;
    }
  } else if ((value> 0) && Assembler::is_simm16(value >> 32)) {
    set_int_at(count << 2, (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 32));
    count += 1;
    set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value >> 16));
    count += 1;
    set_int_at(count << 2, (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6));
    count += 1;
    set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
    count += 1;
  } else {
    tty->print_cr("value = 0x%lx", value);
    guarantee(false, "Not supported yet !");
  }

  while (count < 4) {
    set_int_at(count << 2, 0);
    count++;
  }
}

void NativeMovConstReg::set_data(intptr_t x, intptr_t o) {
  // li64 or li48
  if ((!nativeInstruction_at(addr_at(12))->is_nop()) && is_special_op(int_at(16), Assembler::dsll_op) && is_op(long_at(20), Assembler::ori_op)) {
    set_int_at(0, (int_at(0) & 0xffff0000) | (Assembler::split_low((intptr_t)x >> 48) & 0xffff));
    set_int_at(4, (int_at(4) & 0xffff0000) | (Assembler::split_low((intptr_t)x >> 32) & 0xffff));
    set_int_at(12, (int_at(12) & 0xffff0000) | (Assembler::split_low((intptr_t)x >> 16) & 0xffff));
    set_int_at(20, (int_at(20) & 0xffff0000) | (Assembler::split_low((intptr_t)x) & 0xffff));
  } else {
    patch_set48(x);
  }

  ICache::invalidate_range(addr_at(0), 24);

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
  if (is_immediate())
    return (short)(int_at(instruction_offset)&0xffff);
  else
    return Assembler::merge(int_at(hiword_offset)&0xffff, long_at(instruction_offset)&0xffff);
}

void NativeMovRegMem::set_offset(int x) {
  if (is_immediate()) {
    assert(Assembler::is_simm16(x), "just check");
    set_int_at(0, (int_at(0)&0xffff0000) | (x&0xffff) );
    if (is_64ldst()) {
      assert(Assembler::is_simm16(x+4), "just check");
      set_int_at(4, (int_at(4)&0xffff0000) | ((x+4)&0xffff) );
    }
  } else {
    set_int_at(0, (int_at(0) & 0xffff0000) | (Assembler::split_high(x) & 0xffff));
    set_int_at(4, (int_at(4) & 0xffff0000) | (Assembler::split_low(x) & 0xffff));
  }
  ICache::invalidate_range(addr_at(0), 8);
}

void NativeMovRegMem::verify() {
  int offset = 0;

  if ( Assembler::opcode(int_at(0)) == Assembler::lui_op ) {

    if ( Assembler::opcode(int_at(4)) != Assembler::ori_op ) {
      fatal ("not a mov [reg+offs], reg instruction");
    }

    offset += 12;
  }

  switch(Assembler::opcode(int_at(offset))) {
    case Assembler::lb_op:
    case Assembler::lbu_op:
    case Assembler::lh_op:
    case Assembler::lhu_op:
    case Assembler::lw_op:
    case Assembler::lwu_op:
    case Assembler::ld_op:
    case Assembler::lwc1_op:
    case Assembler::ldc1_op:
    case Assembler::sb_op:
    case Assembler::sh_op:
    case Assembler::sw_op:
    case Assembler::sd_op:
    case Assembler::swc1_op:
    case Assembler::sdc1_op:
      break;
    default:
      fatal ("not a mov [reg+offs], reg instruction");
  }
}


void NativeMovRegMem::print() {
  tty->print_cr(PTR_FORMAT ": mov reg, [reg + %x]", p2i(instruction_address()), offset());
}

bool NativeInstruction::is_sigill_zombie_not_entrant() {
  return uint_at(0) == NativeIllegalInstruction::instruction_code;
}

void NativeIllegalInstruction::insert(address code_pos) {
  *(juint*)code_pos = instruction_code;
  ICache::invalidate_range(code_pos, instruction_size);
}

void NativeJump::verify() {
  assert(((NativeInstruction *)this)->is_jump() ||
         ((NativeInstruction *)this)->is_cond_jump(), "not a general jump instruction");
}

void  NativeJump::patch_set48_gs(address dest) {
  jlong value = (jlong) dest;
  int  rt_reg = (int_at(0) & (0x1f << 16));

  if (rt_reg == 0) rt_reg = 25 << 16; // r25 is T9

  int  rs_reg = rt_reg << 5;
  int  rd_reg = rt_reg >> 5;

  int hi = (int)(value >> 32);
  int lo = (int)(value & ~0);

  int count = 0;

  int insts[4] = {0, 0, 0, 0};

  if (value == lo) {  // 32-bit integer
    if (Assembler::is_simm16(value)) {
      insts[count] = (Assembler::daddiu_op << 26) | rt_reg | Assembler::split_low(value);
      count += 1;
    } else {
      insts[count] = (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 16);
      count += 1;
      if (Assembler::split_low(value)) {
        insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value);
        count += 1;
      }
    }
  } else if (hi == 0) {  // hardware zero-extends to upper 32
      insts[count] = (Assembler::ori_op << 26) | rt_reg | Assembler::split_low(julong(value) >> 16);
      count += 1;
      insts[count] = (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6);
      count += 1;
      if (Assembler::split_low(value)) {
        insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value);
        count += 1;
      }
  } else if ((value> 0) && Assembler::is_simm16(value >> 32)) {
    insts[count] = (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 32);
    count += 1;
    insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value >> 16);
    count += 1;
    insts[count] = (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6);
    count += 1;
    insts[count] = (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value);
    count += 1;
  } else {
    tty->print_cr("dest = 0x%lx", value);
    guarantee(false, "Not supported yet !");
  }

  while (count < 4) {
    insts[count] = 0;
    count++;
  }

  guarantee(((long)addr_at(0) % (BytesPerWord * 2)) == 0, "must be aligned");
  atomic_store128_ptr func = get_atomic_store128_func();
  (*func)((long *)addr_at(0), 0, *(long *)&insts[0], *(long *)&insts[2]);

  ICache::invalidate_range(addr_at(0), 16);
}

void  NativeJump::patch_set48(address dest) {
  jlong value = (jlong) dest;
  int  rt_reg = (int_at(0) & (0x1f << 16));
  int  rs_reg = rt_reg << 5;
  int  rd_reg = rt_reg >> 5;

  int hi = (int)(value >> 32);
  int lo = (int)(value & ~0);

  int count = 0;

  if (value == lo) {  // 32-bit integer
    if (Assembler::is_simm16(value)) {
      set_int_at(count << 2, (Assembler::daddiu_op << 26) | rt_reg | Assembler::split_low(value));
      count += 1;
    } else {
      set_int_at(count << 2, (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 16));
      count += 1;
      if (Assembler::split_low(value)) {
        set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
        count += 1;
      }
    }
  } else if (hi == 0) {  // hardware zero-extends to upper 32
      set_int_at(count << 2, (Assembler::ori_op << 26) | rt_reg | Assembler::split_low(julong(value) >> 16));
      count += 1;
      set_int_at(count << 2, (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6));
      count += 1;
      if (Assembler::split_low(value)) {
        set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
        count += 1;
      }
  } else if ((value> 0) && Assembler::is_simm16(value >> 32)) {
    set_int_at(count << 2, (Assembler::lui_op << 26) | rt_reg | Assembler::split_low(value >> 32));
    count += 1;
    set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value >> 16));
    count += 1;
    set_int_at(count << 2, (Assembler::dsll_op) | rt_reg | rd_reg | (16 << 6));
    count += 1;
    set_int_at(count << 2, (Assembler::ori_op << 26) | rs_reg | rt_reg | Assembler::split_low(value));
    count += 1;
  } else {
    tty->print_cr("dest = 0x%lx", value);
    guarantee(false, "Not supported yet !");
  }

  while (count < 4) {
    set_int_at(count << 2, 0);
    count++;
  }

  ICache::invalidate_range(addr_at(0), 16);
}

void  NativeJump::patch_on_j_only(address dst) {
  long dest = ((long)dst - (((long)addr_at(4)) & 0xfffffffff0000000))>>2;
  if ((dest >= 0) && (dest < (1<<26))) {
    jint j_inst = (Assembler::j_op << 26) | dest;
    set_int_at(0, j_inst);
    ICache::invalidate_range(addr_at(0), 4);
  } else {
    ShouldNotReachHere();
  }
}


void  NativeJump::patch_on_j_gs(address dst) {
  long dest = ((long)dst - (((long)addr_at(20)) & 0xfffffffff0000000))>>2;
  if ((dest >= 0) && (dest < (1<<26))) {
    jint j_inst = (Assembler::j_op << 26) | dest;
    set_int_at(16, j_inst);
    ICache::invalidate_range(addr_at(16), 4);
  } else {
    ShouldNotReachHere();
  }
}

void  NativeJump::patch_on_j(address dst) {
  patch_on_j_gs(dst);
}

void  NativeJump::patch_on_jr_gs(address dst) {
  patch_set48_gs(dst);
  ICache::invalidate_range(addr_at(0), 16);
}

void  NativeJump::patch_on_jr(address dst) {
  patch_set48(dst);
  ICache::invalidate_range(addr_at(0), 16);
}


void  NativeJump::set_jump_destination(address dest) {
  OrderAccess::fence();

  if (is_short()) {
    assert(Assembler::is_simm16(dest-addr_at(4)), "change this code");
    set_int_at(0, (int_at(0) & 0xffff0000) | (dest - addr_at(4)) & 0xffff );
    ICache::invalidate_range(addr_at(0), 4);
  } else if (is_b_far()) {
    int offset = dest - addr_at(12);
    set_int_at(12, (int_at(12) & 0xffff0000) | (offset >> 16));
    set_int_at(16, (int_at(16) & 0xffff0000) | (offset & 0xffff));
  } else {
    if (is_op(int_at(16), Assembler::j_op)) {
      if (UseLEXT1) {
        patch_on_j_gs(dest);
      } else {
        patch_on_j(dest);
      }
    } else if (is_op(int_at(0), Assembler::j_op)) {
      patch_on_j_only(dest);
    } else if (is_special_op(int_at(16), Assembler::jr_op)) {
      if (UseLEXT1) {
        //guarantee(!os::is_MP() || (((long)addr_at(0) % 16) == 0), "destination must be aligned for GSSD");
        //patch_on_jr_gs(dest);
        patch_on_jr(dest);
      } else {
        patch_on_jr(dest);
      }
    } else {
      fatal("not a jump");
    }
  }
}

void NativeGeneralJump::insert_unconditional(address code_pos, address entry) {
  CodeBuffer cb(code_pos, instruction_size);
  MacroAssembler masm(&cb);
#define __ masm.
  if (Assembler::is_simm16((entry - code_pos - 4) / 4)) {
    __ b(entry);
    __ delayed()->nop();
  } else {
    // Attention: We have to use a relative jump here since PC reloc-operation isn't allowed here.
    int offset = entry - code_pos;

    Label L;
    __ bgezal(R0, L);
    __ delayed()->lui(T9, (offset - 8) >> 16);
    __ bind(L);
    __ ori(T9, T9, (offset - 8) & 0xffff);
    __ daddu(T9, T9, RA);
    __ jr(T9);
    __ delayed()->nop();
  }

#undef __

  ICache::invalidate_range(code_pos, instruction_size);
}

bool NativeJump::is_b_far() {
//
//   0x000000556809f198: daddu at, ra, zero
//   0x000000556809f19c: [4110001]bgezal zero, 0x000000556809f1a4
//
//   0x000000556809f1a0: nop
//   0x000000556809f1a4: lui t9, 0xfffffffd
//   0x000000556809f1a8: ori t9, t9, 0x14dc
//   0x000000556809f1ac: daddu t9, t9, ra
//   0x000000556809f1b0: daddu ra, at, zero
//   0x000000556809f1b4: jr t9
//   0x000000556809f1b8: nop
//  ;; ImplicitNullCheckStub slow case
//   0x000000556809f1bc: lui t9, 0x55
//
  return is_op(int_at(12), Assembler::lui_op);
}

address NativeJump::jump_destination() {
  if ( is_short() ) {
    return addr_at(4) + Assembler::imm_off(int_at(instruction_offset)) * 4;
  }
  // Assembler::merge() is not correct in MIPS_64!
  //
  //   Example:
  //     hi16 = 0xfffd,
  //     lo16 = f7a4,
  //
  //     offset=0xfffdf7a4 (Right)
  //     Assembler::merge = 0xfffcf7a4 (Wrong)
  //
  if ( is_b_far() ) {
    int hi16 = int_at(12)&0xffff;
    int low16 = int_at(16)&0xffff;
    address target = addr_at(12) + (hi16 << 16) + low16;
    return target;
  }

  // nop
  // nop
  // nop
  // nop
  // j target
  // nop
  if ( nativeInstruction_at(addr_at(0))->is_nop() &&
        nativeInstruction_at(addr_at(4))->is_nop()   &&
        nativeInstruction_at(addr_at(8))->is_nop()   &&
        nativeInstruction_at(addr_at(12))->is_nop()  &&
        is_op(int_at(16), Assembler::j_op)         &&
        nativeInstruction_at(addr_at(20))->is_nop()) {
    int instr_index = int_at(16) & 0x3ffffff;
    intptr_t target_high = ((intptr_t)addr_at(20)) & 0xfffffffff0000000;
    intptr_t target = target_high | (instr_index << 2);
    return (address)target;
  }

  // j target
  // nop
  if ( is_op(int_at(0), Assembler::j_op)         &&
        nativeInstruction_at(addr_at(4))->is_nop()) {
    int instr_index = int_at(0) & 0x3ffffff;
    intptr_t target_high = ((intptr_t)addr_at(4)) & 0xfffffffff0000000;
    intptr_t target = target_high | (instr_index << 2);
    return (address)target;
  }

  // li64
  if ( is_op(Assembler::lui_op) &&
        is_op(int_at(4), Assembler::ori_op) &&
        is_special_op(int_at(8), Assembler::dsll_op) &&
        is_op(int_at(12), Assembler::ori_op) &&
        is_special_op(int_at(16), Assembler::dsll_op) &&
        is_op(int_at(20), Assembler::ori_op) ) {

    return (address)Assembler::merge( (intptr_t)(int_at(20) & 0xffff),
                             (intptr_t)(int_at(12) & 0xffff),
                             (intptr_t)(int_at(4) & 0xffff),
                             (intptr_t)(int_at(0) & 0xffff));
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  if (  is_op(Assembler::lui_op) &&
          is_op (int_at(4), Assembler::ori_op) &&
          is_special_op(int_at(8), Assembler::dsll_op) &&
          is_op (int_at(12), Assembler::ori_op) ) {

    return (address)Assembler::merge( (intptr_t)(int_at(12) & 0xffff),
                 (intptr_t)(int_at(4) & 0xffff),
           (intptr_t)(int_at(0) & 0xffff),
           (intptr_t)0);
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  //nop
  if (  is_op(Assembler::ori_op) &&
          is_special_op(int_at(4), Assembler::dsll_op) &&
          is_op (int_at(8), Assembler::ori_op) &&
          nativeInstruction_at(addr_at(12))->is_nop()) {

    return (address)Assembler::merge( (intptr_t)(int_at(8) & 0xffff),
                             (intptr_t)(int_at(0) & 0xffff),
                             (intptr_t)0,
                             (intptr_t)0);
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //nop
  //nop
  if (  is_op(Assembler::ori_op) &&
          is_special_op(int_at(4), Assembler::dsll_op) &&
          nativeInstruction_at(addr_at(8))->is_nop()   &&
          nativeInstruction_at(addr_at(12))->is_nop()) {

    return (address)Assembler::merge( (intptr_t)(0),
                             (intptr_t)(int_at(0) & 0xffff),
                             (intptr_t)0,
                             (intptr_t)0);
  }

  //daddiu dst, R0, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::daddiu_op) &&
          nativeInstruction_at(addr_at(4))->is_nop() &&
          nativeInstruction_at(addr_at(8))->is_nop() &&
          nativeInstruction_at(addr_at(12))->is_nop() ) {

    int sign = int_at(0) & 0x8000;
    if (sign == 0) {
      return (address)Assembler::merge( (intptr_t)(int_at(0) & 0xffff),
                                        (intptr_t)0,
                                        (intptr_t)0,
                                        (intptr_t)0);
    } else {
      return (address)Assembler::merge( (intptr_t)(int_at(0) & 0xffff),
                                        (intptr_t)(0xffff),
                                        (intptr_t)(0xffff),
                                        (intptr_t)(0xffff));
    }
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
          is_op (int_at(4), Assembler::ori_op) &&
          nativeInstruction_at(addr_at(8))->is_nop() &&
          nativeInstruction_at(addr_at(12))->is_nop() ) {

    int sign = int_at(0) & 0x8000;
    if (sign == 0) {
      return (address)Assembler::merge( (intptr_t)(int_at(4) & 0xffff),
                                        (intptr_t)(int_at(0) & 0xffff),
                                        (intptr_t)0,
                                        (intptr_t)0);
    } else {
      return (address)Assembler::merge( (intptr_t)(int_at(4) & 0xffff),
                                        (intptr_t)(int_at(0) & 0xffff),
                                        (intptr_t)(0xffff),
                                        (intptr_t)(0xffff));
    }
  }

  //lui dst, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
          nativeInstruction_at(addr_at(4))->is_nop() &&
          nativeInstruction_at(addr_at(8))->is_nop() &&
          nativeInstruction_at(addr_at(12))->is_nop() ) {

    int sign = int_at(0) & 0x8000;
    if (sign == 0) {
      return (address)Assembler::merge( (intptr_t)0,
                                        (intptr_t)(int_at(0) & 0xffff),
                                        (intptr_t)0,
                                        (intptr_t)0);
    } else {
      return (address)Assembler::merge( (intptr_t)0,
                                        (intptr_t)(int_at(0) & 0xffff),
                                        (intptr_t)(0xffff),
                                        (intptr_t)(0xffff));
    }
  }

  fatal("not a jump");
  return NULL; // unreachable
}

// MT-safe patching of a long jump instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
void NativeGeneralJump::replace_mt_safe(address instr_addr, address code_buffer) {
  NativeGeneralJump* h_jump =  nativeGeneralJump_at (instr_addr);
  assert((int)instruction_size == (int)NativeCall::instruction_size,
          "note::Runtime1::patch_code uses NativeCall::instruction_size");

  // ensure 100% atomicity
  guarantee(!os::is_MP() || (((long)instr_addr % BytesPerWord) == 0), "destination must be aligned for SD");

  int *p = (int *)instr_addr;
  int jr_word = p[4];

  p[4] = 0x1000fffb;   /* .1: --; --; --; --; b .1; nop */
  memcpy(instr_addr, code_buffer, NativeCall::instruction_size - 8);
  *(long *)(instr_addr + 16) = *(long *)(code_buffer + 16);
}

// Must ensure atomicity
void NativeJump::patch_verified_entry(address entry, address verified_entry, address dest) {
  assert(dest == SharedRuntime::get_handle_wrong_method_stub(), "expected fixed destination of patch");
  assert(nativeInstruction_at(verified_entry + BytesPerInstWord)->is_nop(), "mips64 cannot replace non-nop with jump");

  if (MacroAssembler::reachable_from_cache(dest)) {
    CodeBuffer cb(verified_entry, 1 * BytesPerInstWord);
    MacroAssembler masm(&cb);
    masm.j(dest);
  } else {
    // We use an illegal instruction for marking a method as
    // not_entrant or zombie
    NativeIllegalInstruction::insert(verified_entry);
  }

  ICache::invalidate_range(verified_entry, 1 * BytesPerInstWord);
}

bool NativeInstruction::is_jump()
{
  if ((int_at(0) & NativeGeneralJump::b_mask) == NativeGeneralJump::beq_opcode)
    return true;
  if (is_op(int_at(4), Assembler::lui_op)) // simplified b_far
    return true;
  if (is_op(int_at(12), Assembler::lui_op)) // original b_far
    return true;

  // nop
  // nop
  // nop
  // nop
  // j target
  // nop
  if ( is_nop() &&
         nativeInstruction_at(addr_at(4))->is_nop()  &&
         nativeInstruction_at(addr_at(8))->is_nop()  &&
         nativeInstruction_at(addr_at(12))->is_nop() &&
         nativeInstruction_at(addr_at(16))->is_op(Assembler::j_op) &&
         nativeInstruction_at(addr_at(20))->is_nop() ) {
    return true;
  }

  if ( nativeInstruction_at(addr_at(0))->is_op(Assembler::j_op) &&
         nativeInstruction_at(addr_at(4))->is_nop() ) {
    return true;
  }

  // lui   rd, imm(63...48);
  // ori   rd, rd, imm(47...32);
  // dsll  rd, rd, 16;
  // ori   rd, rd, imm(31...16);
  // dsll  rd, rd, 16;
  // ori   rd, rd, imm(15...0);
  // jr    rd
  // nop
  if (is_op(int_at(0), Assembler::lui_op) &&
          is_op(int_at(4), Assembler::ori_op) &&
          is_special_op(int_at(8), Assembler::dsll_op) &&
          is_op(int_at(12), Assembler::ori_op) &&
          is_special_op(int_at(16), Assembler::dsll_op) &&
          is_op(int_at(20), Assembler::ori_op) &&
          is_special_op(int_at(24), Assembler::jr_op)) {
    return true;
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  if (is_op(int_at(0), Assembler::lui_op) &&
          is_op(int_at(4), Assembler::ori_op) &&
          is_special_op(int_at(8), Assembler::dsll_op) &&
          is_op(int_at(12), Assembler::ori_op) &&
          is_special_op(int_at(16), Assembler::jr_op)) {
    return true;
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //ori dst, dst, imm16
  //nop
  if (  is_op(Assembler::ori_op) &&
        is_special_op(int_at(4), Assembler::dsll_op) &&
        is_op  (int_at(8), Assembler::ori_op) &&
        nativeInstruction_at(addr_at(12))->is_nop() &&
        is_special_op(int_at(16), Assembler::jr_op)) {
    return true;
  }

  //ori dst, R0, imm16
  //dsll dst, dst, 16
  //nop
  //nop
  if (  is_op(Assembler::ori_op) &&
        is_special_op(int_at(4), Assembler::dsll_op) &&
        nativeInstruction_at(addr_at(8))->is_nop()   &&
        nativeInstruction_at(addr_at(12))->is_nop() &&
        is_special_op(int_at(16), Assembler::jr_op)) {
      return true;
  }

  //daddiu dst, R0, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::daddiu_op) &&
        nativeInstruction_at(addr_at(4))->is_nop() &&
        nativeInstruction_at(addr_at(8))->is_nop() &&
        nativeInstruction_at(addr_at(12))->is_nop() &&
        is_special_op(int_at(16), Assembler::jr_op)) {
    return true;
  }

  //lui dst, imm16
  //ori dst, dst, imm16
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
        is_op  (int_at(4), Assembler::ori_op) &&
        nativeInstruction_at(addr_at(8))->is_nop() &&
        nativeInstruction_at(addr_at(12))->is_nop() &&
        is_special_op(int_at(16), Assembler::jr_op)) {
    return true;
  }

  //lui dst, imm16
  //nop
  //nop
  //nop
  if (  is_op(Assembler::lui_op) &&
        nativeInstruction_at(addr_at(4))->is_nop() &&
        nativeInstruction_at(addr_at(8))->is_nop() &&
        nativeInstruction_at(addr_at(12))->is_nop() &&
        is_special_op(int_at(16), Assembler::jr_op)) {
    return true;
  }

  return false;
}

bool NativeInstruction::is_dtrace_trap() {
  //return (*(int32_t*)this & 0xff) == 0xcc;
  Unimplemented();
  return false;
}

bool NativeInstruction::is_safepoint_poll() {
  //
  // 390     li   T2, 0x0000000000400000 #@loadConP
  // 394     sw    [SP + #12], V1    # spill 9
  // 398     Safepoint @ [T2] : poll for GC @ safePoint_poll        # spec.benchmarks.compress.Decompressor::decompress @ bci:224  L[0]=A6 L[1]=_ L[2]=sp + #28 L[3]=_ L[4]=V1
  //
  //  0x000000ffe5815130: lui t2, 0x40
  //  0x000000ffe5815134: sw v1, 0xc(sp)    ; OopMap{a6=Oop off=920}
  //                                        ;*goto
  //                                        ; - spec.benchmarks.compress.Decompressor::decompress@224 (line 584)
  //
  //  0x000000ffe5815138: lw at, 0x0(t2)    ;*goto       <---  PC
  //                                        ; - spec.benchmarks.compress.Decompressor::decompress@224 (line 584)
  //

  // Since there may be some spill instructions between the safePoint_poll and loadConP,
  // we check the safepoint instruction like the this.
  return is_op(Assembler::lw_op) && is_rt(AT);
}
