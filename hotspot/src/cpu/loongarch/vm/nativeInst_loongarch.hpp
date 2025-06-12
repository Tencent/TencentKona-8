/*
 * Copyright (c) 1997, 2011, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_LOONGARCH_VM_NATIVEINST_LOONGARCH_HPP
#define CPU_LOONGARCH_VM_NATIVEINST_LOONGARCH_HPP

#include "asm/assembler.hpp"
#include "memory/allocation.hpp"
#include "runtime/icache.hpp"
#include "runtime/os.hpp"
#include "utilities/top.hpp"

// We have interfaces for the following instructions:
// - NativeInstruction
// - - NativeCall
// - - NativeMovConstReg
// - - NativeMovConstRegPatching
// - - NativeMovRegMem
// - - NativeMovRegMemPatching
// - - NativeIllegalOpCode
// - - NativeGeneralJump
// - - NativePushConst
// - - NativeTstRegMem

// The base class for different kinds of native instruction abstractions.
// Provides the primitive operations to manipulate code relative to this.

class NativeInstruction VALUE_OBJ_CLASS_SPEC {
  friend class Relocation;

 public:
  enum loongarch_specific_constants {
    nop_instruction_code        =    0,
    nop_instruction_size        =    4,
    sync_instruction_code       =    0xf
  };

  bool is_nop()                        { guarantee(0, "LA not implemented yet"); return long_at(0) == nop_instruction_code; }
  bool is_sync()                       { return Assembler::high(insn_word(), 17) == Assembler::dbar_op; }
  bool is_dtrace_trap();
  inline bool is_call();
  inline bool is_far_call();
  inline bool is_illegal();
  bool is_jump();
  bool is_safepoint_poll();

  // LoongArch has no instruction to generate a illegal instrucion exception?
  // But `break  11` is not illegal instruction for LoongArch.
  static int illegal_instruction();

  bool is_int_branch();
  bool is_float_branch();

  inline bool is_NativeCallTrampolineStub_at();
  //We use an illegal instruction for marking a method as not_entrant or zombie.
  bool is_sigill_zombie_not_entrant();

 protected:
  address addr_at(int offset) const    { return address(this) + offset; }
  address instruction_address() const       { return addr_at(0); }
  address next_instruction_address() const  { return addr_at(BytesPerInstWord); }
  address prev_instruction_address() const  { return addr_at(-BytesPerInstWord); }

  s_char sbyte_at(int offset) const    { return *(s_char*) addr_at(offset); }
  u_char ubyte_at(int offset) const    { return *(u_char*) addr_at(offset); }

  jint int_at(int offset) const         { return *(jint*) addr_at(offset); }
  juint uint_at(int offset) const       { return *(juint*) addr_at(offset); }

  intptr_t ptr_at(int offset) const    { return *(intptr_t*) addr_at(offset); }

  oop  oop_at (int offset) const       { return *(oop*) addr_at(offset); }
  int  long_at(int offset) const       { return *(jint*)addr_at(offset); }


  void set_char_at(int offset, char c)        { *addr_at(offset) = (u_char)c; wrote(offset); }
  void set_int_at(int offset, jint  i)        { *(jint*)addr_at(offset) = i;  wrote(offset); }
  void set_ptr_at (int offset, intptr_t  ptr) { *(intptr_t*) addr_at(offset) = ptr;  wrote(offset); }
  void set_oop_at (int offset, oop  o)        { *(oop*) addr_at(offset) = o;  wrote(offset); }
  void set_long_at(int offset, long  i);

  int  insn_word() const { return long_at(0); }

  void wrote(int offset);

 public:

  // unit test stuff
  static void test() {}                 // override for testing

  inline friend NativeInstruction* nativeInstruction_at(address address);
};

inline NativeInstruction* nativeInstruction_at(address address) {
  NativeInstruction* inst = (NativeInstruction*)address;
#ifdef ASSERT
  //inst->verify();
#endif
  return inst;
}

inline NativeCall* nativeCall_at(address address);

// The NativeCall is an abstraction for accessing/manipulating native call
// instructions (used to manipulate inline caches, primitive & dll calls, etc.).
class NativeCall: public NativeInstruction {
 public:
  enum loongarch_specific_constants {
    instruction_offset    = 0,
    instruction_size      = 1 * BytesPerInstWord,
    return_address_offset = 1 * BytesPerInstWord,
    displacement_offset   = 0
  };

  // We have only bl.
  bool is_bl() const;

  address instruction_address() const { return addr_at(instruction_offset); }

  address next_instruction_address() const {
    return addr_at(return_address_offset);
  }

  address return_address() const {
    return next_instruction_address();
  }

  address target_addr_for_bl(address orig_addr = 0) const;
  address destination() const;
  void set_destination(address dest);

  void verify_alignment() {}
  void verify();
  void print();

  // Creation
  inline friend NativeCall* nativeCall_at(address address);
  inline friend NativeCall* nativeCall_before(address return_address);

  static bool is_call_at(address instr) {
    return nativeInstruction_at(instr)->is_call();
  }

  static bool is_call_before(address return_address) {
    return is_call_at(return_address - return_address_offset);
  }

  // MT-safe patching of a call instruction.
  static void insert(address code_pos, address entry);
  static void replace_mt_safe(address instr_addr, address code_buffer);

  // Similar to replace_mt_safe, but just changes the destination.  The
  // important thing is that free-running threads are able to execute
  // this call instruction at all times.  If the call is an immediate bl
  // instruction we can simply rely on atomicity of 32-bit writes to
  // make sure other threads will see no intermediate states.

  // We cannot rely on locks here, since the free-running threads must run at
  // full speed.
  //
  // Used in the runtime linkage of calls; see class CompiledIC.

  // The parameter assert_lock disables the assertion during code generation.
  void set_destination_mt_safe(address dest, bool assert_lock = true);

  address get_trampoline();

};

inline NativeCall* nativeCall_at(address address) {
  NativeCall* call = (NativeCall*)(address - NativeCall::instruction_offset);
#ifdef ASSERT
  call->verify();
#endif
  return call;
}

inline NativeCall* nativeCall_before(address return_address) {
  NativeCall* call = (NativeCall*)(return_address - NativeCall::return_address_offset);
#ifdef ASSERT
  call->verify();
#endif
  return call;
}

// The NativeFarCall is an abstraction for accessing/manipulating native
// call-anywhere instructions.
// Used to call native methods which may be loaded anywhere in the address
// space, possibly out of reach of a call instruction.
class NativeFarCall: public NativeInstruction {
 public:
  enum loongarch_specific_constants {
    instruction_size      = 2 * BytesPerInstWord,
  };

  // We use MacroAssembler::patchable_call() for implementing a
  // call-anywhere instruction.
  bool is_short() const;
  bool is_far() const;

  // Checks whether instr points at a NativeFarCall instruction.
  static bool is_far_call_at(address address) {
    return nativeInstruction_at(address)->is_far_call();
  }

  // Returns the NativeFarCall's destination.
  address destination(address orig_addr = 0) const;

  // Sets the NativeFarCall's destination, not necessarily mt-safe.
  // Used when relocating code.
  void set_destination(address dest);

  void verify();
};

// Instantiates a NativeFarCall object starting at the given instruction
// address and returns the NativeFarCall object.
inline NativeFarCall* nativeFarCall_at(address address) {
  NativeFarCall* call = (NativeFarCall*)address;
#ifdef ASSERT
  call->verify();
#endif
  return call;
}

// An interface for accessing/manipulating native set_oop imm, reg instructions
// (used to manipulate inlined data references, etc.).
class NativeMovConstReg: public NativeInstruction {
 public:
  enum loongarch_specific_constants {
    instruction_offset    =    0,
    instruction_size          =    3 * BytesPerInstWord,
    next_instruction_offset   =    3 * BytesPerInstWord,
  };

  int     insn_word() const                 { return long_at(instruction_offset); }
  address instruction_address() const       { return addr_at(0); }
  address next_instruction_address() const  { return addr_at(next_instruction_offset); }
  intptr_t data() const;
  void    set_data(intptr_t x, intptr_t o = 0);

  bool is_li52() const {
    return is_lu12iw_ori_lu32id() ||
           is_lu12iw_lu32id_nop() ||
           is_lu12iw_2nop() ||
           is_lu12iw_ori_nop() ||
           is_addid_2nop();
  }
  bool is_lu12iw_ori_lu32id() const;
  bool is_lu12iw_lu32id_nop() const;
  bool is_lu12iw_2nop() const;
  bool is_lu12iw_ori_nop() const;
  bool is_addid_2nop() const;
  void  verify();
  void  print();

  // unit test stuff
  static void test() {}

  // Creation
  inline friend NativeMovConstReg* nativeMovConstReg_at(address address);
  inline friend NativeMovConstReg* nativeMovConstReg_before(address address);
};

inline NativeMovConstReg* nativeMovConstReg_at(address address) {
  NativeMovConstReg* test = (NativeMovConstReg*)(address - NativeMovConstReg::instruction_offset);
#ifdef ASSERT
  test->verify();
#endif
  return test;
}

inline NativeMovConstReg* nativeMovConstReg_before(address address) {
  NativeMovConstReg* test = (NativeMovConstReg*)(address - NativeMovConstReg::instruction_size - NativeMovConstReg::instruction_offset);
#ifdef ASSERT
  test->verify();
#endif
  return test;
}

class NativeMovConstRegPatching: public NativeMovConstReg {
 private:
    friend NativeMovConstRegPatching* nativeMovConstRegPatching_at(address address) {
    NativeMovConstRegPatching* test = (NativeMovConstRegPatching*)(address - instruction_offset);
    #ifdef ASSERT
      test->verify();
    #endif
    return test;
  }
};

class NativeMovRegMem: public NativeInstruction {
 public:
  enum loongarch_specific_constants {
    instruction_offset = 0,
    instruction_size = 4,
    hiword_offset   = 4,
    ldst_offset     = 12,
    immediate_size  = 4,
    ldst_size       = 16
  };

  address instruction_address() const       { return addr_at(instruction_offset); }

  int num_bytes_to_end_of_patch() const { return instruction_offset + instruction_size; }

  int   offset() const;

  void  set_offset(int x);

  void  add_offset_in_bytes(int add_offset)     { set_offset ( ( offset() + add_offset ) ); }

  void verify();
  void print ();

  // unit test stuff
  static void test() {}

 private:
  inline friend NativeMovRegMem* nativeMovRegMem_at (address address);
};

inline NativeMovRegMem* nativeMovRegMem_at (address address) {
  NativeMovRegMem* test = (NativeMovRegMem*)(address - NativeMovRegMem::instruction_offset);
#ifdef ASSERT
  test->verify();
#endif
  return test;
}

class NativeMovRegMemPatching: public NativeMovRegMem {
 private:
  friend NativeMovRegMemPatching* nativeMovRegMemPatching_at (address address) {
    NativeMovRegMemPatching* test = (NativeMovRegMemPatching*)(address - instruction_offset);
    #ifdef ASSERT
      test->verify();
    #endif
    return test;
  }
};


// Handles all kinds of jump on Loongson.
//   short:
//     b offs26
//     nop
//
//   far:
//     pcaddu18i reg, si20
//     jirl  r0, reg, si18
//
class NativeJump: public NativeInstruction {
 public:
  enum loongarch_specific_constants {
    instruction_offset = 0,
    instruction_size   = 2 * BytesPerInstWord
  };

  bool is_short();
  bool is_far();

  address instruction_address() const { return addr_at(instruction_offset); }
  address jump_destination(address orig_addr = 0);
  void  set_jump_destination(address dest);

  // Creation
  inline friend NativeJump* nativeJump_at(address address);

  // Insertion of native jump instruction
  static void insert(address code_pos, address entry) { Unimplemented(); }
  // MT-safe insertion of native jump at verified method entry
  static void check_verified_entry_alignment(address entry, address verified_entry){}
  static void patch_verified_entry(address entry, address verified_entry, address dest);

  void verify();
};

inline NativeJump* nativeJump_at(address address) {
  NativeJump* jump = (NativeJump*)(address - NativeJump::instruction_offset);
  debug_only(jump->verify();)
  return jump;
}

class NativeGeneralJump: public NativeJump {
 public:
  // Creation
  inline friend NativeGeneralJump* nativeGeneralJump_at(address address);

  // Insertion of native general jump instruction
  static void insert_unconditional(address code_pos, address entry);
  static void replace_mt_safe(address instr_addr, address code_buffer);
};

inline NativeGeneralJump* nativeGeneralJump_at(address address) {
  NativeGeneralJump* jump = (NativeGeneralJump*)(address);
  debug_only(jump->verify();)
  return jump;
}

class NativeIllegalInstruction: public NativeInstruction {
public:
  enum loongarch_specific_constants {
    instruction_code        = 0xbadc0de0, // TODO: LA
                                          // Temporary LoongArch reserved instruction
    instruction_size        = 4,
    instruction_offset      = 0,
    next_instruction_offset = 4
  };

  // Insert illegal opcode as specific address
  static void insert(address code_pos);
};

inline bool NativeInstruction::is_illegal() { return insn_word() == illegal_instruction(); }

inline bool NativeInstruction::is_call() {
  NativeCall *call = (NativeCall*)instruction_address();
  return call->is_bl();
}

inline bool NativeInstruction::is_far_call() {
  NativeFarCall *call = (NativeFarCall*)instruction_address();

  // short
  if (call->is_short()) {
    return true;
  }

  // far
  if (call->is_far()) {
    return true;
  }

  return false;
}

inline bool NativeInstruction::is_jump()
{
  NativeGeneralJump *jump = (NativeGeneralJump*)instruction_address();

  // short
  if (jump->is_short()) {
    return true;
  }

  // far
  if (jump->is_far()) {
    return true;
  }

  return false;
}

// Call trampoline stubs.
class NativeCallTrampolineStub : public NativeInstruction {
 public:

  enum la_specific_constants {
    instruction_size            =    6 * 4,
    instruction_offset          =    0,
    data_offset                 =    4 * 4,
    next_instruction_offset     =    6 * 4
  };

  address destination() const {
    return (address)ptr_at(data_offset);
  }

  void set_destination(address new_destination) {
    set_ptr_at(data_offset, (intptr_t)new_destination);
    OrderAccess::fence();
  }
};

// Note: Other stubs must not begin with this pattern.
inline bool NativeInstruction::is_NativeCallTrampolineStub_at() {
  // pcaddi
  // ld_d
  // jirl
  return Assembler::high(int_at(0), 7) == Assembler::pcaddi_op &&
         Assembler::high(int_at(4), 10) == Assembler::ld_d_op &&
         Assembler::high(int_at(8), 6) == Assembler::jirl_op      &&
         Assembler::low(int_at(8), 5)  == R0->encoding();
}

inline NativeCallTrampolineStub* nativeCallTrampolineStub_at(address addr) {
  NativeInstruction* ni = nativeInstruction_at(addr);
  assert(ni->is_NativeCallTrampolineStub_at(), "no call trampoline found");
  return (NativeCallTrampolineStub*)addr;
}
#endif // CPU_LOONGARCH_VM_NATIVEINST_LOONGARCH_HPP
