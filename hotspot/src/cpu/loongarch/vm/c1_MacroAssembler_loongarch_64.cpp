/*
 * Copyright (c) 1999, 2021, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, 2023, Loongson Technology. All rights reserved.
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
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "interpreter/interpreter.hpp"
#include "oops/arrayOop.hpp"
#include "runtime/basicLock.hpp"
#include "runtime/os.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"

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
#define T4 RT4

int C1_MacroAssembler::lock_object(Register hdr, Register obj, Register disp_hdr, Register scratch, Label& slow_case) {
  const int aligned_mask = BytesPerWord -1;
  const int hdr_offset = oopDesc::mark_offset_in_bytes();
  assert(hdr != obj && hdr != disp_hdr && obj != disp_hdr, "registers must be different");
  int null_check_offset = -1;
  Label done;

  verify_oop(obj);

  // save object being locked into the BasicObjectLock
  st_ptr(obj, Address(disp_hdr, BasicObjectLock::obj_offset_in_bytes()));

  if (UseBiasedLocking) {
    assert(scratch != noreg, "should have scratch register at this point");
    null_check_offset = biased_locking_enter(disp_hdr, obj, hdr, scratch, false, done, &slow_case);
  } else {
    null_check_offset = offset();
  }

  // Load object header
  ld_ptr(hdr, Address(obj, hdr_offset));
  // and mark it as unlocked
  ori(hdr, hdr, markOopDesc::unlocked_value);
  // save unlocked object header into the displaced header location on the stack
  st_ptr(hdr, Address(disp_hdr, 0));
  // test if object header is still the same (i.e. unlocked), and if so, store the
  // displaced header address in the object header - if it is not the same, get the
  // object header instead
  lea(SCR2, Address(obj, hdr_offset));
  cmpxchg(Address(SCR2, 0), hdr, disp_hdr, SCR1, true, false, done);
  // if the object header was the same, we're done
  // if the object header was not the same, it is now in the hdr register
  // => test if it is a stack pointer into the same stack (recursive locking), i.e.:
  //
  // 1) (hdr & aligned_mask) == 0
  // 2) sp <= hdr
  // 3) hdr <= sp + page_size
  //
  // these 3 tests can be done by evaluating the following expression:
  //
  // (hdr - sp) & (aligned_mask - page_size)
  //
  // assuming both the stack pointer and page_size have their least
  // significant 2 bits cleared and page_size is a power of 2
  sub_d(hdr, hdr, SP);
  li(SCR1, aligned_mask - os::vm_page_size());
  andr(hdr, hdr, SCR1);
  // for recursive locking, the result is zero => save it in the displaced header
  // location (NULL in the displaced hdr location indicates recursive locking)
  st_ptr(hdr, Address(disp_hdr, 0));
  // otherwise we don't care about the result and handle locking via runtime call
  bnez(hdr, slow_case);
  // done
  bind(done);
  return null_check_offset;
}

void C1_MacroAssembler::unlock_object(Register hdr, Register obj, Register disp_hdr, Label& slow_case) {
  const int aligned_mask = BytesPerWord -1;
  const int hdr_offset = oopDesc::mark_offset_in_bytes();
  assert(hdr != obj && hdr != disp_hdr && obj != disp_hdr, "registers must be different");
  Label done;

  if (UseBiasedLocking) {
    // load object
    ld_ptr(obj, Address(disp_hdr, BasicObjectLock::obj_offset_in_bytes()));
    biased_locking_exit(obj, hdr, done);
  }

  // load displaced header
  ld_ptr(hdr, Address(disp_hdr, 0));
  // if the loaded hdr is NULL we had recursive locking
  // if we had recursive locking, we are done
  beqz(hdr, done);
  if (!UseBiasedLocking) {
    // load object
    ld_ptr(obj, Address(disp_hdr, BasicObjectLock::obj_offset_in_bytes()));
  }
  verify_oop(obj);
  // test if object header is pointing to the displaced header, and if so, restore
  // the displaced header in the object - if the object header is not pointing to
  // the displaced header, get the object header instead
  // if the object header was not pointing to the displaced header,
  // we do unlocking via runtime call
  if (hdr_offset) {
    lea(SCR1, Address(obj, hdr_offset));
    cmpxchg(Address(SCR1, 0), disp_hdr, hdr, SCR2, false, false, done, &slow_case);
  } else {
    cmpxchg(Address(obj, 0), disp_hdr, hdr, SCR2, false, false, done, &slow_case);
  }
  // done
  bind(done);
}

// Defines obj, preserves var_size_in_bytes
void C1_MacroAssembler::try_allocate(Register obj, Register var_size_in_bytes,
                                     int con_size_in_bytes, Register t1, Register t2,
                                     Label& slow_case) {
  if (UseTLAB) {
    tlab_allocate(obj, var_size_in_bytes, con_size_in_bytes, t1, t2, slow_case);
  } else {
    eden_allocate(obj, var_size_in_bytes, con_size_in_bytes, t1, slow_case);
  }
}

void C1_MacroAssembler::initialize_header(Register obj, Register klass, Register len,
                                          Register t1, Register t2) {
  assert_different_registers(obj, klass, len);
  if (UseBiasedLocking && !len->is_valid()) {
    assert_different_registers(obj, klass, len, t1, t2);
    ld_ptr(t1, Address(klass, Klass::prototype_header_offset()));
  } else {
    // This assumes that all prototype bits fit in an int32_t
    li(t1, (int32_t)(intptr_t)markOopDesc::prototype());
  }
  st_ptr(t1, Address(obj, oopDesc::mark_offset_in_bytes()));

  if (UseCompressedClassPointers) { // Take care not to kill klass
    encode_klass_not_null(t1, klass);
    st_w(t1, Address(obj, oopDesc::klass_offset_in_bytes()));
  } else {
    st_ptr(klass, Address(obj, oopDesc::klass_offset_in_bytes()));
  }

  if (len->is_valid()) {
    st_w(len, Address(obj, arrayOopDesc::length_offset_in_bytes()));
  } else if (UseCompressedClassPointers) {
    store_klass_gap(obj, R0);
  }
}

// preserves obj, destroys len_in_bytes
//
// Scratch registers: t1 = T0, t2 = T1
//
void C1_MacroAssembler::initialize_body(Register obj, Register len_in_bytes,
                                        int hdr_size_in_bytes, Register t1, Register t2) {
  assert(hdr_size_in_bytes >= 0, "header size must be positive or 0");
  assert(t1 == T0 && t2 == T1, "must be");
  Label done;

  // len_in_bytes is positive and ptr sized
  addi_d(len_in_bytes, len_in_bytes, -hdr_size_in_bytes);
  beqz(len_in_bytes, done);

  // zero_words() takes ptr in t1 and count in bytes in t2
  lea(t1, Address(obj, hdr_size_in_bytes));
  addi_d(t2, len_in_bytes, -BytesPerWord);

  Label loop;
  bind(loop);
  stx_d(R0, t1, t2);
  addi_d(t2, t2, -BytesPerWord);
  bge(t2, R0, loop);

  bind(done);
}

void C1_MacroAssembler::allocate_object(Register obj, Register t1, Register t2, int header_size,
                                        int object_size, Register klass, Label& slow_case) {
  assert_different_registers(obj, t1, t2);
  assert(header_size >= 0 && object_size >= header_size, "illegal sizes");

  try_allocate(obj, noreg, object_size * BytesPerWord, t1, t2, slow_case);

  initialize_object(obj, klass, noreg, object_size * HeapWordSize, t1, t2, UseTLAB);
}

// Scratch registers: t1 = T0, t2 = T1
void C1_MacroAssembler::initialize_object(Register obj, Register klass, Register var_size_in_bytes,
                                          int con_size_in_bytes, Register t1, Register t2,
                                          bool is_tlab_allocated) {
  assert((con_size_in_bytes & MinObjAlignmentInBytesMask) == 0,
         "con_size_in_bytes is not multiple of alignment");
  const int hdr_size_in_bytes = instanceOopDesc::header_size() * HeapWordSize;

  initialize_header(obj, klass, noreg, t1, t2);

  if (!(UseTLAB && ZeroTLAB && is_tlab_allocated)) {
     // clear rest of allocated space
     const Register index = t2;
     if (var_size_in_bytes != noreg) {
       move(index, var_size_in_bytes);
       initialize_body(obj, index, hdr_size_in_bytes, t1, t2);
     } else if (con_size_in_bytes > hdr_size_in_bytes) {
       con_size_in_bytes -= hdr_size_in_bytes;
       lea(t1, Address(obj, hdr_size_in_bytes));
       Label loop;
       li(SCR1, con_size_in_bytes - BytesPerWord);
       bind(loop);
       stx_d(R0, t1, SCR1);
       addi_d(SCR1, SCR1, -BytesPerWord);
       bge(SCR1, R0, loop);
     }
  }

  membar(StoreStore);

  if (CURRENT_ENV->dtrace_alloc_probes()) {
    assert(obj == A0, "must be");
    call(Runtime1::entry_for(Runtime1::dtrace_object_alloc_id), relocInfo::runtime_call_type);
  }

  verify_oop(obj);
}

void C1_MacroAssembler::allocate_array(Register obj, Register len, Register t1, Register t2,
                                       int header_size, int f, Register klass, Label& slow_case) {
  assert_different_registers(obj, len, t1, t2, klass);

  // determine alignment mask
  assert(!(BytesPerWord & 1), "must be a multiple of 2 for masking code to work");

  // check for negative or excessive length
  li(SCR1, (int32_t)max_array_allocation_length);
  bge_far(len, SCR1, slow_case, false);

  const Register arr_size = t2; // okay to be the same
  // align object end
  li(arr_size, (int32_t)header_size * BytesPerWord + MinObjAlignmentInBytesMask);
  slli_w(SCR1, len, f);
  add_d(arr_size, arr_size, SCR1);
  bstrins_d(arr_size, R0, exact_log2(MinObjAlignmentInBytesMask + 1) - 1, 0);

  try_allocate(obj, arr_size, 0, t1, t2, slow_case);

  initialize_header(obj, klass, len, t1, t2);

  // clear rest of allocated space
  initialize_body(obj, arr_size, header_size * BytesPerWord, t1, t2);

  membar(StoreStore);

  if (CURRENT_ENV->dtrace_alloc_probes()) {
    assert(obj == A0, "must be");
    call(Runtime1::entry_for(Runtime1::dtrace_object_alloc_id), relocInfo::runtime_call_type);
  }

  verify_oop(obj);
}

void C1_MacroAssembler::build_frame(int framesize, int bang_size_in_bytes) {
  assert(bang_size_in_bytes >= framesize, "stack bang size incorrect");
  // Make sure there is enough stack space for this method's activation.
  // Note that we do this before creating a frame.
  generate_stack_overflow_check(bang_size_in_bytes);
  MacroAssembler::build_frame(framesize);
}

void C1_MacroAssembler::remove_frame(int framesize) {
  MacroAssembler::remove_frame(framesize);
}

void C1_MacroAssembler::verified_entry() {
  // If we have to make this method not-entrant we'll overwrite its
  // first instruction with a jump.  For this action to be legal we
  // must ensure that this first instruction is a b, bl, nop, break.
  // Make it a NOP.
  nop();
}

void C1_MacroAssembler::load_parameter(int offset_in_words, Register reg) {
  // rbp, + 0: link
  //      + 1: return address
  //      + 2: argument with offset 0
  //      + 3: argument with offset 1
  //      + 4: ...

  ld_ptr(reg, Address(FP, (offset_in_words + 2) * BytesPerWord));
}

#ifndef PRODUCT
void C1_MacroAssembler::verify_stack_oop(int stack_offset) {
  if (!VerifyOops) return;
  verify_oop_addr(Address(SP, stack_offset), "oop");
}

void C1_MacroAssembler::verify_not_null_oop(Register r) {
  if (!VerifyOops) return;
  Label not_null;
  bnez(r, not_null);
  stop("non-null oop required");
  bind(not_null);
  verify_oop(r);
}

void C1_MacroAssembler::invalidate_registers(bool inv_a0, bool inv_s0, bool inv_a2,
                                             bool inv_a3, bool inv_a4, bool inv_a5) {
#ifdef ASSERT
  static int nn;
  if (inv_a0) li(A0, 0xDEAD);
  if (inv_s0) li(S0, 0xDEAD);
  if (inv_a2) li(A2, nn++);
  if (inv_a3) li(A3, 0xDEAD);
  if (inv_a4) li(A4, 0xDEAD);
  if (inv_a5) li(A5, 0xDEAD);
#endif
}
#endif // ifndef PRODUCT
