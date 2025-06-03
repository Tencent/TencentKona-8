/*
 * Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/macroAssembler.hpp"
#include "code/vtableStubs.hpp"
#include "interp_masm_loongarch_64.hpp"
#include "memory/resourceArea.hpp"
#include "oops/compiledICHolder.hpp"
#include "oops/klassVtable.hpp"
#include "runtime/sharedRuntime.hpp"
#include "vmreg_loongarch.inline.hpp"
#ifdef COMPILER2
#include "opto/runtime.hpp"
#endif


// machine-dependent part of VtableStubs: create VtableStub of correct size and
// initialize its code

#define __ masm->

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

#ifndef PRODUCT
extern "C" void bad_compiled_vtable_index(JavaThread* thread,
                                          oop receiver,
                                          int index);
#endif

// used by compiler only;  reciever in T0.
// used registers :
// Rmethod : receiver klass & method
// NOTE: If this code is used by the C1, the receiver_location is always 0.
// when reach here, receiver in T0, klass in T8
VtableStub* VtableStubs::create_vtable_stub(int vtable_index) {
  const int la_code_length = VtableStub::pd_code_size_limit(true);
  VtableStub* s = new(la_code_length) VtableStub(true, vtable_index);
  ResourceMark rm;
  CodeBuffer cb(s->entry_point(), la_code_length);
  MacroAssembler* masm = new MacroAssembler(&cb);
  Register t1 = T8, t2 = Rmethod;
#ifndef PRODUCT
  if (CountCompiledCalls) {
    __ li(AT, SharedRuntime::nof_megamorphic_calls_addr());
    __ ld_w(t1, AT , 0);
    __ addi_w(t1, t1, 1);
    __ st_w(t1, AT,0);
  }
#endif

  // get receiver (need to skip return address on top of stack)
  //assert(receiver_location == T0->as_VMReg(), "receiver expected in T0");

  // get receiver klass
  address npe_addr = __ pc();
  __ load_klass(t1, T0);
  // compute entry offset (in words)
  int entry_offset = InstanceKlass::vtable_start_offset() + vtable_index*vtableEntry::size();
#ifndef PRODUCT
  if (DebugVtables) {
    Label L;
    // check offset vs vtable length
    __ ld_w(t2, t1, InstanceKlass::vtable_length_offset()*wordSize);
    assert(Assembler::is_simm16(vtable_index*vtableEntry::size()), "change this code");
    __ li(AT, vtable_index*vtableEntry::size());
    __ blt(AT, t2, L);
    __ li(A2, vtable_index);
    __ move(A1, A0);
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, bad_compiled_vtable_index), A1, A2);
    __ bind(L);
  }
#endif // PRODUCT
  // load methodOop and target address
  const Register method = Rmethod;
  int offset = entry_offset*wordSize + vtableEntry::method_offset_in_bytes();
  if (Assembler::is_simm(offset, 12)) {
    __ ld_ptr(method, t1, offset);
  } else {
    __ li(AT, offset);
    __ ld_ptr(method, t1, AT);
  }
  if (DebugVtables) {
    Label L;
    __ beq(method, R0, L);
    __ ld_d(AT, method,in_bytes(Method::from_compiled_offset()));
    __ bne(AT, R0, L);
    __ stop("Vtable entry is NULL");
    __ bind(L);
  }
  // T8: receiver klass
  // T0: receiver
  // Rmethod: methodOop
  // T4: entry
  address ame_addr = __ pc();
  __ ld_ptr(T4, method,in_bytes(Method::from_compiled_offset()));
  __ jr(T4);
  masm->flush();
  s->set_exception_points(npe_addr, ame_addr);
  return s;
}


// used registers :
//  T1 T2
// when reach here, the receiver in T0, klass in T1
VtableStub* VtableStubs::create_itable_stub(int itable_index) {
  // Note well: pd_code_size_limit is the absolute minimum we can get
  // away with.  If you add code here, bump the code stub size
  // returned by pd_code_size_limit!
  const int la_code_length = VtableStub::pd_code_size_limit(false);
  VtableStub* s = new(la_code_length) VtableStub(false, itable_index);
  ResourceMark rm;
  CodeBuffer cb(s->entry_point(), la_code_length);
  MacroAssembler* masm = new MacroAssembler(&cb);
  // we T8,T4 as temparary register, they are free from register allocator
  Register t1 = T8, t2 = T2;
  // Entry arguments:
  //  T1: Interface
  //  T0: Receiver

#ifndef PRODUCT
  if (CountCompiledCalls) {
    __ li(AT, SharedRuntime::nof_megamorphic_calls_addr());
    __ ld_w(T8, AT, 0);
    __ addi_w(T8, T8, 1);
    __ st_w(T8, AT, 0);
  }
#endif /* PRODUCT */
  const Register holder_klass_reg   = T1; // declaring interface klass (DECC)
  const Register resolved_klass_reg = Rmethod; // resolved interface klass (REFC)
  const Register icholder_reg = T1;
  __ ld_ptr(resolved_klass_reg, icholder_reg, CompiledICHolder::holder_klass_offset());
  __ ld_ptr(holder_klass_reg,   icholder_reg, CompiledICHolder::holder_metadata_offset());

  // get receiver klass (also an implicit null-check)
  address npe_addr = __ pc();
  __ load_klass(t1, T0);
  {
    // x86 use lookup_interface_method, but lookup_interface_method does not work on LoongArch.
    const int base = InstanceKlass::vtable_start_offset() * wordSize;
    assert(vtableEntry::size() * wordSize == 8, "adjust the scaling in the code below");
    assert(Assembler::is_simm16(base), "change this code");
    __ addi_d(t2, t1, base);
    assert(Assembler::is_simm16(InstanceKlass::vtable_length_offset() * wordSize), "change this code");
    __ ld_w(AT, t1, InstanceKlass::vtable_length_offset() * wordSize);
    __ alsl_d(t2, AT, t2, Address::times_8 - 1);
    if (HeapWordsPerLong > 1) {
      __ round_to(t2, BytesPerLong);
    }

    Label hit, entry;
    assert(Assembler::is_simm16(itableOffsetEntry::size() * wordSize), "change this code");
    __ bind(entry);

#ifdef ASSERT
    // Check that the entry is non-null
    if (DebugVtables) {
      Label L;
      assert(Assembler::is_simm16(itableOffsetEntry::interface_offset_in_bytes()), "change this code");
      __ ld_w(AT, t1, itableOffsetEntry::interface_offset_in_bytes());
      __ bne(AT, R0, L);
      __ stop("null entry point found in itable's offset table");
      __ bind(L);
    }
#endif
    assert(Assembler::is_simm16(itableOffsetEntry::interface_offset_in_bytes()), "change this code");
    __ ld_ptr(AT, t2, itableOffsetEntry::interface_offset_in_bytes());
    __ addi_d(t2, t2, itableOffsetEntry::size() * wordSize);
    __ bne(AT, resolved_klass_reg, entry);

  }

  // add for compressedoops
  __ load_klass(t1, T0);
  // compute itable entry offset (in words)
  const int base = InstanceKlass::vtable_start_offset() * wordSize;
  assert(vtableEntry::size() * wordSize == 8, "adjust the scaling in the code below");
  assert(Assembler::is_simm16(base), "change this code");
  __ addi_d(t2, t1, base);
  assert(Assembler::is_simm16(InstanceKlass::vtable_length_offset() * wordSize), "change this code");
  __ ld_w(AT, t1, InstanceKlass::vtable_length_offset() * wordSize);
  __ alsl_d(t2, AT, t2, Address::times_8 - 1);
  if (HeapWordsPerLong > 1) {
    __ round_to(t2, BytesPerLong);
  }

  Label hit, entry;
  assert(Assembler::is_simm16(itableOffsetEntry::size() * wordSize), "change this code");
  __ bind(entry);

#ifdef ASSERT
  // Check that the entry is non-null
  if (DebugVtables) {
    Label L;
    assert(Assembler::is_simm16(itableOffsetEntry::interface_offset_in_bytes()), "change this code");
    __ ld_w(AT, t1, itableOffsetEntry::interface_offset_in_bytes());
    __ bne(AT, R0, L);
    __ stop("null entry point found in itable's offset table");
    __ bind(L);
  }
#endif
  assert(Assembler::is_simm16(itableOffsetEntry::interface_offset_in_bytes()), "change this code");
  __ ld_ptr(AT, t2, itableOffsetEntry::interface_offset_in_bytes());
  __ addi_d(t2, t2, itableOffsetEntry::size() * wordSize);
  __ bne(AT, holder_klass_reg, entry);

  // We found a hit, move offset into T4
  __ ld_ptr(t2, t2, itableOffsetEntry::offset_offset_in_bytes() - itableOffsetEntry::size() * wordSize);

  // Compute itableMethodEntry.
  const int method_offset = (itableMethodEntry::size() * wordSize * itable_index) +
    itableMethodEntry::method_offset_in_bytes();

  // Get methodOop and entrypoint for compiler
  const Register method = Rmethod;

  __ slli_d(AT, t2, Address::times_1);
  __ add_d(AT, AT, t1 );
  if (Assembler::is_simm(method_offset, 12)) {
    __ ld_ptr(method, AT, method_offset);
  } else {
    __ li(t1, method_offset);
    __ ld_ptr(method, AT, t1);
  }

#ifdef ASSERT
  if (DebugVtables) {
    Label L1;
    __ beq(method, R0, L1);
    __ ld_d(AT, method,in_bytes(Method::from_compiled_offset()));
    __ bne(AT, R0, L1);
    __ stop("methodOop is null");
    __ bind(L1);
  }
#endif // ASSERT

  // Rmethod: methodOop
  // T0: receiver
  // T4: entry point
  address ame_addr = __ pc();
  __ ld_ptr(T4, method,in_bytes(Method::from_compiled_offset()));
  __ jr(T4);
  masm->flush();
  s->set_exception_points(npe_addr, ame_addr);
  return s;
}

// NOTE : whenever you change the code above, dont forget to change the const here
int VtableStub::pd_code_size_limit(bool is_vtable_stub) {
  if (is_vtable_stub) {
    return ( DebugVtables ? 600 : 28) + (CountCompiledCalls ? 24 : 0)+
           (UseCompressedOops ? 16 : 0);
  } else {
    return  ( DebugVtables ? 636 : 152) + (CountCompiledCalls ? 24 : 0)+
            (UseCompressedOops ? 32 : 0);
  }
}

int VtableStub::pd_code_alignment() {
  return wordSize;
}
