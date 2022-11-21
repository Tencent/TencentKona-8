/*
 * Copyright (c) 1998, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "code/relocInfo.hpp"
#include "compiler/disassembler.hpp"
#include "nativeInst_mips.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/safepoint.hpp"


void Relocation::pd_set_data_value(address x, intptr_t o, bool verify_only) {
  x += o;
  typedef Assembler::WhichOperand WhichOperand;
  WhichOperand which = (WhichOperand) format(); // that is, disp32 or imm, call32, narrow oop
  assert(which == Assembler::disp32_operand ||
         which == Assembler::narrow_oop_operand ||
         which == Assembler::imm_operand, "format unpacks ok");
  if (which == Assembler::imm_operand) {
    if (verify_only) {
      assert(nativeMovConstReg_at(addr())->data() == (long)x, "instructions must match");
    } else {
      nativeMovConstReg_at(addr())->set_data((intptr_t)(x));
    }
  } else if (which == Assembler::narrow_oop_operand) {
    // both compressed oops and compressed classes look the same
    if (Universe::heap()->is_in_reserved((oop)x)) {
      if (verify_only) {
        assert(nativeMovConstReg_at(addr())->data() == (long)oopDesc::encode_heap_oop((oop)x), "instructions must match");
      } else {
        nativeMovConstReg_at(addr())->set_data((intptr_t)(oopDesc::encode_heap_oop((oop)x)), (intptr_t)(x));
      }
    } else {
      if (verify_only) {
        assert(nativeMovConstReg_at(addr())->data() == (long)Klass::encode_klass((Klass*)x), "instructions must match");
      } else {
        nativeMovConstReg_at(addr())->set_data((intptr_t)(Klass::encode_klass((Klass*)x)), (intptr_t)(x));
      }
    }
  } else {
    // Note:  Use runtime_call_type relocations for call32_operand.
    assert(0, "call32_operand not supported in MIPS64");
  }
}


//NOTICE HERE, this relocate is not need for MIPS, since MIPS USE abosolutly target,
//Maybe We should FORGET CALL RELOCATION
address Relocation::pd_call_destination(address orig_addr) {
  intptr_t adj = 0;
  NativeInstruction* ni = nativeInstruction_at(addr());
  if (ni->is_call()) {
    if (!ni->is_trampoline_call()) {
      return nativeCall_at(addr())->target_addr_for_insn();
    } else {
      address trampoline = nativeCall_at(addr())->get_trampoline();
      if (trampoline) {
        return nativeCallTrampolineStub_at(trampoline)->destination();
      } else {
        return (address) -1;
      }
    }
  } else if (ni->is_jump()) {
    return nativeGeneralJump_at(addr())->jump_destination() + adj;
  } else if (ni->is_cond_jump()) {
    return nativeCondJump_at(addr())->jump_destination() +adj;
  } else {
    tty->print_cr("\nError!\ncall destination: 0x%lx", p2i(addr()));
    Disassembler::decode(addr() - 10 * 4, addr() + 10 * 4, tty);
    ShouldNotReachHere();
    return NULL;
  }
}


void Relocation::pd_set_call_destination(address x) {
  NativeInstruction* ni = nativeInstruction_at(addr());
  if (ni->is_call()) {
    NativeCall* call = nativeCall_at(addr());
    if (!ni->is_trampoline_call()) {
      call->set_destination(x);
    } else {
      address trampoline_stub_addr = call->get_trampoline();
      if (trampoline_stub_addr != NULL) {
        address orig = call->target_addr_for_insn();
        if (orig != trampoline_stub_addr) {
          call->patch_on_trampoline(trampoline_stub_addr);
        }
        call->set_destination_mt_safe(x, false);
      }
    }
  } else if (ni->is_jump())
    nativeGeneralJump_at(addr())->set_jump_destination(x);
  else if (ni->is_cond_jump())
    nativeCondJump_at(addr())->set_jump_destination(x);
  else
    { ShouldNotReachHere(); }

    // Unresolved jumps are recognized by a destination of -1
    // However 64bit can't actually produce such an address
    // and encodes a jump to self but jump_destination will
    // return a -1 as the signal. We must not relocate this
    // jmp or the ic code will not see it as unresolved.
}


address* Relocation::pd_address_in_code() {
  return (address*)addr();
}


address Relocation::pd_get_address_from_code() {
  NativeMovConstReg* ni = nativeMovConstReg_at(addr());
  return (address)ni->data();
}



void poll_Relocation::fix_relocation_after_move(const CodeBuffer* src, CodeBuffer* dest) {
}

void poll_return_Relocation::fix_relocation_after_move(const CodeBuffer* src, CodeBuffer* dest) {
}

void internal_pc_Relocation::fix_relocation_after_move(const CodeBuffer* src, CodeBuffer* dest) {
  address target =0;
  NativeMovConstReg* ni = nativeMovConstReg_at(addr());
  target = new_addr_for((address)ni->data(), src, dest);
  ni->set_data((intptr_t)target);
}

void metadata_Relocation::pd_fix_value(address x) {
}
