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
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "memory/universe.inline.hpp"
#include "oops/methodData.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"
#include "utilities/macros.hpp"


#ifndef CC_INTERP

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
#define T8 RT8
#define T9 RT9

// Platform-dependent initialization

void TemplateTable::pd_initialize() {
  // No mips specific initialization
}

// Address computation: local variables

static inline Address iaddress(int n) {
  return Address(LVP, Interpreter::local_offset_in_bytes(n));
}

static inline Address laddress(int n) {
  return iaddress(n + 1);
}

static inline Address faddress(int n) {
  return iaddress(n);
}

static inline Address daddress(int n) {
  return laddress(n);
}

static inline Address aaddress(int n) {
  return iaddress(n);
}
static inline Address haddress(int n)            { return iaddress(n + 0); }


static inline Address at_sp()             {  return Address(SP,   0); }
static inline Address at_sp_p1()          { return Address(SP,  1 * wordSize); }
static inline Address at_sp_p2()          { return Address(SP,  2 * wordSize); }

// At top of Java expression stack which may be different than sp().  It
// isn't for category 1 objects.
static inline Address at_tos   () {
  Address tos = Address(SP,  Interpreter::expr_offset_in_bytes(0));
  return tos;
}

static inline Address at_tos_p1() {
  return Address(SP,  Interpreter::expr_offset_in_bytes(1));
}

static inline Address at_tos_p2() {
  return Address(SP,  Interpreter::expr_offset_in_bytes(2));
}

static inline Address at_tos_p3() {
  return Address(SP,  Interpreter::expr_offset_in_bytes(3));
}

// we use S0 as bcp, be sure you have bcp in S0 before you call any of the Template generator
Address TemplateTable::at_bcp(int offset) {
  assert(_desc->uses_bcp(), "inconsistent uses_bcp information");
  return Address(BCP, offset);
}

// Miscelaneous helper routines
// Store an oop (or NULL) at the address described by obj.
// If val == noreg this means store a NULL

static void do_oop_store(InterpreterMacroAssembler* _masm,
                         Address obj,
                         Register val,
                         BarrierSet::Name barrier,
                         bool precise) {
  assert(val == noreg || val == V0, "parameter is just for looks");
  switch (barrier) {
#if INCLUDE_ALL_GCS
    case BarrierSet::G1SATBCT:
    case BarrierSet::G1SATBCTLogging:
      {
        // flatten object address if needed
        if (obj.index() == noreg && obj.disp() == 0) {
          if (obj.base() != T3) {
            __ move(T3, obj.base());
          }
        } else {
          __ lea(T3, obj);
        }
        __ g1_write_barrier_pre(T3 /* obj */,
                                T1 /* pre_val */,
                                TREG /* thread */,
                                T9  /* tmp */,
                                val != noreg /* tosca_live */,
                                false /* expand_call */);
        if (val == noreg) {
          __ store_heap_oop_null(Address(T3, 0));
        } else {
          // G1 barrier needs uncompressed oop for region cross check.
          Register new_val = val;
          if (UseCompressedOops) {
            new_val = T1;
            __ move(new_val, val);
          }
          __ store_heap_oop(Address(T3, 0), val);
          __ g1_write_barrier_post(T3 /* store_adr */,
                                   new_val /* new_val */,
                                   TREG /* thread */,
                                   T9 /* tmp */,
                                   T1 /* tmp2 */);
        }
      }
      break;
#endif // INCLUDE_ALL_GCS
    case BarrierSet::CardTableModRef:
    case BarrierSet::CardTableExtension:
      {
        if (val == noreg) {
          __ store_heap_oop_null(obj);
        } else {
          __ store_heap_oop(obj, val);
          // flatten object address if needed
          if (!precise || (obj.index() == noreg && obj.disp() == 0)) {
            __ store_check(obj.base());
          } else {
            __ lea(T9, obj);
            __ store_check(T9);
          }
        }
      }
      break;
    case BarrierSet::ModRef:
    case BarrierSet::Other:
      if (val == noreg) {
        __ store_heap_oop_null(obj);
      } else {
        __ store_heap_oop(obj, val);
      }
      break;
    default      :
      ShouldNotReachHere();

  }
}

// bytecode folding
void TemplateTable::patch_bytecode(Bytecodes::Code bc, Register bc_reg,
                                   Register tmp_reg, bool load_bc_into_bc_reg/*=true*/,
                                   int byte_no) {
  if (!RewriteBytecodes)  return;
  Label L_patch_done;

  switch (bc) {
  case Bytecodes::_fast_aputfield:
  case Bytecodes::_fast_bputfield:
  case Bytecodes::_fast_zputfield:
  case Bytecodes::_fast_cputfield:
  case Bytecodes::_fast_dputfield:
  case Bytecodes::_fast_fputfield:
  case Bytecodes::_fast_iputfield:
  case Bytecodes::_fast_lputfield:
  case Bytecodes::_fast_sputfield:
    {
      // We skip bytecode quickening for putfield instructions when
      // the put_code written to the constant pool cache is zero.
      // This is required so that every execution of this instruction
      // calls out to InterpreterRuntime::resolve_get_put to do
      // additional, required work.
      assert(byte_no == f1_byte || byte_no == f2_byte, "byte_no out of range");
      assert(load_bc_into_bc_reg, "we use bc_reg as temp");
      __ get_cache_and_index_and_bytecode_at_bcp(tmp_reg, bc_reg, tmp_reg, byte_no, 1);
      __ daddiu(bc_reg, R0, bc);
      __ beq(tmp_reg, R0, L_patch_done);
      __ delayed()->nop();
    }
    break;
  default:
    assert(byte_no == -1, "sanity");
    // the pair bytecodes have already done the load.
    if (load_bc_into_bc_reg) {
      __ move(bc_reg, bc);
    }
  }

  if (JvmtiExport::can_post_breakpoint()) {
    Label L_fast_patch;
    // if a breakpoint is present we can't rewrite the stream directly
    __ lbu(tmp_reg, at_bcp(0));
    __ move(AT, Bytecodes::_breakpoint);
    __ bne(tmp_reg, AT, L_fast_patch);
    __ delayed()->nop();

    __ get_method(tmp_reg);
    // Let breakpoint table handling rewrite to quicker bytecode
    __ call_VM(NOREG, CAST_FROM_FN_PTR(address,
    InterpreterRuntime::set_original_bytecode_at), tmp_reg, BCP, bc_reg);

    __ b(L_patch_done);
    __ delayed()->nop();
    __ bind(L_fast_patch);
  }

#ifdef ASSERT
  Label L_okay;
  __ lbu(tmp_reg, at_bcp(0));
  __ move(AT, (int)Bytecodes::java_code(bc));
  __ beq(tmp_reg, AT, L_okay);
  __ delayed()->nop();
  __ beq(tmp_reg, bc_reg, L_patch_done);
  __ delayed()->nop();
  __ stop("patching the wrong bytecode");
  __ bind(L_okay);
#endif

  // patch bytecode
  __ sb(bc_reg, at_bcp(0));
  __ bind(L_patch_done);
}


// Individual instructions

void TemplateTable::nop() {
  transition(vtos, vtos);
  // nothing to do
}

void TemplateTable::shouldnotreachhere() {
  transition(vtos, vtos);
  __ stop("shouldnotreachhere bytecode");
}

void TemplateTable::aconst_null() {
  transition(vtos, atos);
  __ move(FSR, R0);
}

void TemplateTable::iconst(int value) {
  transition(vtos, itos);
  if (value == 0) {
    __ move(FSR, R0);
  } else {
    __ move(FSR, value);
  }
}

void TemplateTable::lconst(int value) {
  transition(vtos, ltos);
  if (value == 0) {
    __ move(FSR, R0);
  } else {
    __ move(FSR, value);
  }
}

void TemplateTable::fconst(int value) {
  transition(vtos, ftos);
  switch( value ) {
    case 0:  __ mtc1(R0, FSF);    return;
    case 1:  __ addiu(AT, R0, 1); break;
    case 2:  __ addiu(AT, R0, 2); break;
    default: ShouldNotReachHere();
  }
  __ mtc1(AT, FSF);
  __ cvt_s_w(FSF, FSF);
}

void TemplateTable::dconst(int value) {
  transition(vtos, dtos);
  switch( value ) {
    case 0:  __ dmtc1(R0, FSF);
             return;
    case 1:  __ daddiu(AT, R0, 1);
             __ dmtc1(AT, FSF);
             __ cvt_d_w(FSF, FSF);
             break;
    default: ShouldNotReachHere();
  }
}

void TemplateTable::bipush() {
  transition(vtos, itos);
  __ lb(FSR, at_bcp(1));
}

void TemplateTable::sipush() {
  transition(vtos, itos);
  __ lb(FSR, BCP, 1);
  __ lbu(AT, BCP, 2);
  __ dsll(FSR, FSR, 8);
  __ orr(FSR, FSR, AT);
}

// T1 : tags
// T2 : index
// T3 : cpool
// T8 : tag
void TemplateTable::ldc(bool wide) {
  transition(vtos, vtos);
  Label call_ldc, notFloat, notClass, Done;
  // get index in cpool
  if (wide) {
    __ get_unsigned_2_byte_index_at_bcp(T2, 1);
  } else {
    __ lbu(T2, at_bcp(1));
  }

  __ get_cpool_and_tags(T3, T1);

  const int base_offset = ConstantPool::header_size() * wordSize;
  const int tags_offset = Array<u1>::base_offset_in_bytes();

  // get type
  if (UseLEXT1 && Assembler::is_simm(sizeof(tags_offset), 8)) {
    __ gslbx(T1, T1, T2, tags_offset);
  } else {
    __ daddu(AT, T1, T2);
    __ lb(T1, AT, tags_offset);
  }
  if(os::is_MP()) {
    __ sync(); // load acquire
  }
  //now T1 is the tag

  // unresolved class - get the resolved class
  __ daddiu(AT, T1, - JVM_CONSTANT_UnresolvedClass);
  __ beq(AT, R0, call_ldc);
  __ delayed()->nop();

  // unresolved class in error (resolution failed) - call into runtime
  // so that the same error from first resolution attempt is thrown.
  __ daddiu(AT, T1, -JVM_CONSTANT_UnresolvedClassInError);
  __ beq(AT, R0, call_ldc);
  __ delayed()->nop();

  // resolved class - need to call vm to get java mirror of the class
  __ daddiu(AT, T1, - JVM_CONSTANT_Class);
  __ bne(AT, R0, notClass);
  __ delayed()->dsll(T2, T2, Address::times_8);

  __ bind(call_ldc);
  __ move(A1, wide);
  call_VM(FSR, CAST_FROM_FN_PTR(address, InterpreterRuntime::ldc), A1);
  //__ push(atos);
  __ daddiu(SP, SP, - Interpreter::stackElementSize);
  __ b(Done);
  __ delayed()->sd(FSR, SP, 0); // added for performance issue

  __ bind(notClass);
  __ daddiu(AT, T1, -JVM_CONSTANT_Float);
  __ bne(AT, R0, notFloat);
  __ delayed()->nop();
  // ftos
  if (UseLEXT1 && Assembler::is_simm(sizeof(base_offset), 8)) {
    __ gslwxc1(FSF, T3, T2, base_offset);
  } else {
    __ daddu(AT, T3, T2);
    __ lwc1(FSF, AT, base_offset);
  }
  //__ push_f();
  __ daddiu(SP, SP, - Interpreter::stackElementSize);
  __ b(Done);
  __ delayed()->swc1(FSF, SP, 0);

  __ bind(notFloat);
#ifdef ASSERT
  {
    Label L;
    __ daddiu(AT, T1, -JVM_CONSTANT_Integer);
    __ beq(AT, R0, L);
    __ delayed()->nop();
    __ stop("unexpected tag type in ldc");
    __ bind(L);
  }
#endif
  // itos JVM_CONSTANT_Integer only
  if (UseLEXT1 && Assembler::is_simm(sizeof(base_offset), 8)) {
    __ gslwx(FSR, T3, T2, base_offset);
  } else {
    __ daddu(T0, T3, T2);
    __ lw(FSR, T0, base_offset);
  }
  __ push(itos);
  __ bind(Done);
}

// Fast path for caching oop constants.
void TemplateTable::fast_aldc(bool wide) {
  transition(vtos, atos);

  Register result = FSR;
  Register tmp = SSR;
  int index_size = wide ? sizeof(u2) : sizeof(u1);

  Label resolved;

  // We are resolved if the resolved reference cache entry contains a
  // non-null object (String, MethodType, etc.)
  assert_different_registers(result, tmp);
  __ get_cache_index_at_bcp(tmp, 1, index_size);
  __ load_resolved_reference_at_index(result, tmp);
  __ bne(result, R0, resolved);
  __ delayed()->nop();

  address entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_ldc);
  // first time invocation - must resolve first
  int i = (int)bytecode();
  __ move(tmp, i);
  __ call_VM(result, entry, tmp);

  __ bind(resolved);

  if (VerifyOops) {
    __ verify_oop(result);
  }
}


// used register: T2, T3, T1
// T2 : index
// T3 : cpool
// T1 : tag
void TemplateTable::ldc2_w() {
  transition(vtos, vtos);
  Label Long, Done;

  // get index in cpool
  __ get_unsigned_2_byte_index_at_bcp(T2, 1);

  __ get_cpool_and_tags(T3, T1);

  const int base_offset = ConstantPool::header_size() * wordSize;
  const int tags_offset = Array<u1>::base_offset_in_bytes();

  // get type in T1
  if (UseLEXT1 && Assembler::is_simm(tags_offset, 8)) {
    __ gslbx(T1, T1, T2, tags_offset);
  } else {
    __ daddu(AT, T1, T2);
    __ lb(T1, AT, tags_offset);
  }

  __ daddiu(AT, T1, - JVM_CONSTANT_Double);
  __ bne(AT, R0, Long);
  __ delayed()->dsll(T2, T2, Address::times_8);

  // dtos
  if (UseLEXT1 && Assembler::is_simm(base_offset, 8)) {
    __ gsldxc1(FSF, T3, T2, base_offset);
  } else {
    __ daddu(AT, T3, T2);
    __ ldc1(FSF, AT, base_offset);
  }
  __ push(dtos);
  __ b(Done);
  __ delayed()->nop();

  // ltos
  __ bind(Long);
  if (UseLEXT1 && Assembler::is_simm(base_offset, 8)) {
    __ gsldx(FSR, T3, T2, base_offset);
  } else {
    __ daddu(AT, T3, T2);
    __ ld(FSR, AT, base_offset);
  }
  __ push(ltos);

  __ bind(Done);
}

// we compute the actual local variable address here
// the x86 dont do so for it has scaled index memory access model, we dont have, so do here
void TemplateTable::locals_index(Register reg, int offset) {
  __ lbu(reg, at_bcp(offset));
  __ dsll(reg, reg, Address::times_8);
  __ dsubu(reg, LVP, reg);
}

// this method will do bytecode folding of the two form:
// iload iload      iload caload
// used register : T2, T3
// T2 : bytecode
// T3 : folded code
void TemplateTable::iload() {
  transition(vtos, itos);
  if (RewriteFrequentPairs) {
    Label rewrite, done;
    // get the next bytecode in T2
    __ lbu(T2, at_bcp(Bytecodes::length_for(Bytecodes::_iload)));
    // if _iload, wait to rewrite to iload2.  We only want to rewrite the
    // last two iloads in a pair.  Comparing against fast_iload means that
    // the next bytecode is neither an iload or a caload, and therefore
    // an iload pair.
    __ move(AT, Bytecodes::_iload);
    __ beq(AT, T2, done);
    __ delayed()->nop();

    __ move(T3, Bytecodes::_fast_iload2);
    __ move(AT, Bytecodes::_fast_iload);
    __ beq(AT, T2, rewrite);
    __ delayed()->nop();

    // if _caload, rewrite to fast_icaload
    __ move(T3, Bytecodes::_fast_icaload);
    __ move(AT, Bytecodes::_caload);
    __ beq(AT, T2, rewrite);
    __ delayed()->nop();

    // rewrite so iload doesn't check again.
    __ move(T3, Bytecodes::_fast_iload);

    // rewrite
    // T3 : fast bytecode
    __ bind(rewrite);
    patch_bytecode(Bytecodes::_iload, T3, T2, false);
    __ bind(done);
  }

  // Get the local value into tos
  locals_index(T2);
  __ lw(FSR, T2, 0);
}

// used register T2
// T2 : index
void TemplateTable::fast_iload2() {
  transition(vtos, itos);
  locals_index(T2);
  __ lw(FSR, T2, 0);
  __ push(itos);
  locals_index(T2, 3);
  __ lw(FSR, T2, 0);
}

// used register T2
// T2 : index
void TemplateTable::fast_iload() {
  transition(vtos, itos);
  locals_index(T2);
  __ lw(FSR, T2, 0);
}

// used register T2
// T2 : index
void TemplateTable::lload() {
  transition(vtos, ltos);
  locals_index(T2);
  __ ld(FSR, T2, -wordSize);
}

// used register T2
// T2 : index
void TemplateTable::fload() {
  transition(vtos, ftos);
  locals_index(T2);
  __ lwc1(FSF, T2, 0);
}

// used register T2
// T2 : index
void TemplateTable::dload() {
  transition(vtos, dtos);
  locals_index(T2);
  __ ldc1(FSF, T2, -wordSize);
}

// used register T2
// T2 : index
void TemplateTable::aload() {
  transition(vtos, atos);
  locals_index(T2);
  __ ld(FSR, T2, 0);
}

void TemplateTable::locals_index_wide(Register reg) {
  __ get_unsigned_2_byte_index_at_bcp(reg, 2);
  __ dsll(reg, reg, Address::times_8);
  __ dsubu(reg, LVP, reg);
}

// used register T2
// T2 : index
void TemplateTable::wide_iload() {
  transition(vtos, itos);
  locals_index_wide(T2);
  __ ld(FSR, T2, 0);
}

// used register T2
// T2 : index
void TemplateTable::wide_lload() {
  transition(vtos, ltos);
  locals_index_wide(T2);
  __ ld(FSR, T2, -wordSize);
}

// used register T2
// T2 : index
void TemplateTable::wide_fload() {
  transition(vtos, ftos);
  locals_index_wide(T2);
  __ lwc1(FSF, T2, 0);
}

// used register T2
// T2 : index
void TemplateTable::wide_dload() {
  transition(vtos, dtos);
  locals_index_wide(T2);
  __ ldc1(FSF, T2, -wordSize);
}

// used register T2
// T2 : index
void TemplateTable::wide_aload() {
  transition(vtos, atos);
  locals_index_wide(T2);
  __ ld(FSR, T2, 0);
}

// we use A2 as the regiser for index, BE CAREFUL!
// we dont use our tge 29 now, for later optimization
void TemplateTable::index_check(Register array, Register index) {
  // Pop ptr into array
  __ pop_ptr(array);
  index_check_without_pop(array, index);
}

void TemplateTable::index_check_without_pop(Register array, Register index) {
  // destroys A2
  // check array
  __ null_check(array, arrayOopDesc::length_offset_in_bytes());

  // sign extend since tos (index) might contain garbage in upper bits
  __ sll(index, index, 0);

  // check index
  Label ok;
  __ lw(AT, array, arrayOopDesc::length_offset_in_bytes());
#ifndef OPT_RANGECHECK
  __ sltu(AT, index, AT);
  __ bne(AT, R0, ok);
  __ delayed()->nop();

  //throw_ArrayIndexOutOfBoundsException assume abberrant index in A2
  if (A2 != index) __ move(A2, index);
  __ jmp(Interpreter::_throw_ArrayIndexOutOfBoundsException_entry);
  __ delayed()->nop();
  __ bind(ok);
#else
  __ lw(AT, array, arrayOopDesc::length_offset_in_bytes());
  __ move(A2, index);
  __ tgeu(A2, AT, 29);
#endif
}

void TemplateTable::iaload() {
  transition(itos, itos);
  if(UseBoundCheckInstruction) {
    __ pop(SSR); //SSR:array    FSR： index
    __ dsll(FSR, FSR, 2);
    __ daddu(FSR, SSR, FSR);
    __ addiu(FSR, FSR, arrayOopDesc::base_offset_in_bytes(T_INT));

    __ lw(AT, SSR, arrayOopDesc::length_offset_in_bytes());  //bound
    __ dsll(AT, AT, 2);
    __ daddu(AT, SSR, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_INT));

    __ gslwle(FSR, FSR, AT);
  } else {
    index_check(SSR, FSR);
    __ dsll(FSR, FSR, 2);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_INT), 8)) {
      __ gslwx(FSR, FSR, SSR, arrayOopDesc::base_offset_in_bytes(T_INT));
    } else {
      __ daddu(FSR, SSR, FSR);
      __ lw(FSR, FSR, arrayOopDesc::base_offset_in_bytes(T_INT));
    }
  }
}

void TemplateTable::laload() {
  transition(itos, ltos);
  if(UseBoundCheckInstruction) {
    __ pop(SSR); //SSR:array    FSR： index
    __ dsll(FSR, FSR, Address::times_8);
    __ daddu(FSR, SSR, FSR);
    __ addiu(FSR, FSR, arrayOopDesc::base_offset_in_bytes(T_LONG) + 0 * wordSize);

    __ lw(AT, SSR, arrayOopDesc::length_offset_in_bytes());  //bound
    __ dsll(AT, AT, Address::times_8);
    __ daddu(AT, SSR, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_LONG) + 0 * wordSize);

    __ gsldle(FSR, FSR, AT);
  } else {
    index_check(SSR, FSR);
    __ dsll(AT, FSR, Address::times_8);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_LONG), 8)) {
      __ gsldx(FSR, SSR, AT, arrayOopDesc::base_offset_in_bytes(T_LONG));
    } else {
      __ daddu(AT, SSR, AT);
      __ ld(FSR, AT, arrayOopDesc::base_offset_in_bytes(T_LONG));
    }
  }
}

void TemplateTable::faload() {
  transition(itos, ftos);
  if(UseBoundCheckInstruction) {
    __ pop(SSR); //SSR:array    FSR： index
    __ shl(FSR, 2);
    __ daddu(FSR, SSR, FSR);
    __ addiu(FSR, FSR, arrayOopDesc::base_offset_in_bytes(T_FLOAT));

    __ lw(AT, SSR, arrayOopDesc::length_offset_in_bytes());  //bound
    __ shl(AT, 2);
    __ daddu(AT, SSR, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_FLOAT));

    __ gslwlec1(FSF, FSR, AT);
  } else {
    index_check(SSR, FSR);
    __ shl(FSR, 2);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_FLOAT), 8)) {
      __ gslwxc1(FSF, SSR, FSR, arrayOopDesc::base_offset_in_bytes(T_FLOAT));
    } else {
      __ daddu(FSR, SSR, FSR);
      __ lwc1(FSF, FSR, arrayOopDesc::base_offset_in_bytes(T_FLOAT));
    }
  }
}

void TemplateTable::daload() {
  transition(itos, dtos);
  if(UseBoundCheckInstruction) {
    __ pop(SSR); //SSR:array    FSR： index
    __ dsll(FSR, FSR, 3);
    __ daddu(FSR, SSR, FSR);
    __ addiu(FSR, FSR, arrayOopDesc::base_offset_in_bytes(T_DOUBLE) + 0 * wordSize);

    __ lw(AT, SSR, arrayOopDesc::length_offset_in_bytes());  //bound
    __ dsll(AT, AT, 3);
    __ daddu(AT, SSR, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_DOUBLE) + 0 * wordSize);

    __ gsldlec1(FSF, FSR, AT);
  } else {
    index_check(SSR, FSR);
    __ dsll(AT, FSR, 3);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_DOUBLE), 8)) {
      __ gsldxc1(FSF, SSR, AT, arrayOopDesc::base_offset_in_bytes(T_DOUBLE));
    } else {
      __ daddu(AT, SSR, AT);
      __ ldc1(FSF, AT, arrayOopDesc::base_offset_in_bytes(T_DOUBLE));
    }
  }
}

void TemplateTable::aaload() {
  transition(itos, atos);
  index_check(SSR, FSR);
  __ dsll(FSR, FSR, UseCompressedOops ? Address::times_4 : Address::times_8);
  __ daddu(FSR, SSR, FSR);
  //add for compressedoops
  __ load_heap_oop(FSR, Address(FSR, arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}

void TemplateTable::baload() {
  transition(itos, itos);
  if(UseBoundCheckInstruction) {
    __ pop(SSR); //SSR:array   FSR:index
    __ daddu(FSR, SSR, FSR);
    __ addiu(FSR, FSR, arrayOopDesc::base_offset_in_bytes(T_BYTE)); //base

    __ lw(AT, SSR, arrayOopDesc::length_offset_in_bytes());
    __ daddu(AT, SSR, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_BYTE)); //bound

    __ gslble(FSR, FSR, AT);
  } else {
    index_check(SSR, FSR);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_BYTE), 8)) {
      __ gslbx(FSR, SSR, FSR, arrayOopDesc::base_offset_in_bytes(T_BYTE));
    } else {
      __ daddu(FSR, SSR, FSR);
      __ lb(FSR, FSR, arrayOopDesc::base_offset_in_bytes(T_BYTE));
    }
  }
}

void TemplateTable::caload() {
  transition(itos, itos);
  index_check(SSR, FSR);
  __ dsll(FSR, FSR, Address::times_2);
  __ daddu(FSR, SSR, FSR);
  __ lhu(FSR, FSR,  arrayOopDesc::base_offset_in_bytes(T_CHAR));
}

// iload followed by caload frequent pair
// used register : T2
// T2 : index
void TemplateTable::fast_icaload() {
  transition(vtos, itos);
  // load index out of locals
  locals_index(T2);
  __ lw(FSR, T2, 0);
  index_check(SSR, FSR);
  __ dsll(FSR, FSR, 1);
  __ daddu(FSR, SSR, FSR);
  __ lhu(FSR, FSR,  arrayOopDesc::base_offset_in_bytes(T_CHAR));
}

void TemplateTable::saload() {
  transition(itos, itos);
  if(UseBoundCheckInstruction) {
    __ pop(SSR); //SSR:array    FSR： index
    __ dsll(FSR, FSR, Address::times_2);
    __ daddu(FSR, SSR, FSR);
    __ addiu(FSR, FSR, arrayOopDesc::base_offset_in_bytes(T_SHORT));

    __ lw(AT, SSR, arrayOopDesc::length_offset_in_bytes());  //bound
    __ dsll(AT, AT, Address::times_2);
    __ daddu(AT, SSR, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_SHORT));

    __ gslhle(FSR, FSR, AT);
  } else {
    index_check(SSR, FSR);
    __ dsll(FSR, FSR, Address::times_2);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_SHORT), 8)) {
      __ gslhx(FSR, SSR, FSR,  arrayOopDesc::base_offset_in_bytes(T_SHORT));
    } else {
      __ daddu(FSR, SSR, FSR);
      __ lh(FSR, FSR,  arrayOopDesc::base_offset_in_bytes(T_SHORT));
    }
  }
}

void TemplateTable::iload(int n) {
  transition(vtos, itos);
  __ lw(FSR, iaddress(n));
}

void TemplateTable::lload(int n) {
  transition(vtos, ltos);
  __ ld(FSR, laddress(n));
}

void TemplateTable::fload(int n) {
  transition(vtos, ftos);
  __ lwc1(FSF, faddress(n));
}

void TemplateTable::dload(int n) {
  transition(vtos, dtos);
  __ ldc1(FSF, laddress(n));
}

void TemplateTable::aload(int n) {
  transition(vtos, atos);
  __ ld(FSR, aaddress(n));
}

// used register : T2, T3
// T2 : bytecode
// T3 : folded code
void TemplateTable::aload_0() {
  transition(vtos, atos);
  // According to bytecode histograms, the pairs:
  //
  // _aload_0, _fast_igetfield
  // _aload_0, _fast_agetfield
  // _aload_0, _fast_fgetfield
  //
  // occur frequently. If RewriteFrequentPairs is set, the (slow)
  // _aload_0 bytecode checks if the next bytecode is either
  // _fast_igetfield, _fast_agetfield or _fast_fgetfield and then
  // rewrites the current bytecode into a pair bytecode; otherwise it
  // rewrites the current bytecode into _fast_aload_0 that doesn't do
  // the pair check anymore.
  //
  // Note: If the next bytecode is _getfield, the rewrite must be
  //       delayed, otherwise we may miss an opportunity for a pair.
  //
  // Also rewrite frequent pairs
  //   aload_0, aload_1
  //   aload_0, iload_1
  // These bytecodes with a small amount of code are most profitable
  // to rewrite
  if (RewriteFrequentPairs) {
    Label rewrite, done;
    // get the next bytecode in T2
    __ lbu(T2, at_bcp(Bytecodes::length_for(Bytecodes::_aload_0)));

    // do actual aload_0
    aload(0);

    // if _getfield then wait with rewrite
    __ move(AT, Bytecodes::_getfield);
    __ beq(AT, T2, done);
    __ delayed()->nop();

    // if _igetfield then reqrite to _fast_iaccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_iaccess_0) ==
        Bytecodes::_aload_0,
        "fix bytecode definition");
    __ move(T3, Bytecodes::_fast_iaccess_0);
    __ move(AT, Bytecodes::_fast_igetfield);
    __ beq(AT, T2, rewrite);
    __ delayed()->nop();

    // if _agetfield then reqrite to _fast_aaccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_aaccess_0) ==
        Bytecodes::_aload_0,
        "fix bytecode definition");
    __ move(T3, Bytecodes::_fast_aaccess_0);
    __ move(AT, Bytecodes::_fast_agetfield);
    __ beq(AT, T2, rewrite);
    __ delayed()->nop();

    // if _fgetfield then reqrite to _fast_faccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_faccess_0) ==
        Bytecodes::_aload_0,
        "fix bytecode definition");
    __ move(T3, Bytecodes::_fast_faccess_0);
    __ move(AT, Bytecodes::_fast_fgetfield);
    __ beq(AT, T2, rewrite);
    __ delayed()->nop();

    // else rewrite to _fast_aload0
    assert(Bytecodes::java_code(Bytecodes::_fast_aload_0) ==
        Bytecodes::_aload_0,
        "fix bytecode definition");
    __ move(T3, Bytecodes::_fast_aload_0);

    // rewrite
    __ bind(rewrite);
    patch_bytecode(Bytecodes::_aload_0, T3, T2, false);

    __ bind(done);
  } else {
    aload(0);
  }
}

void TemplateTable::istore() {
  transition(itos, vtos);
  locals_index(T2);
  __ sw(FSR, T2, 0);
}

void TemplateTable::lstore() {
  transition(ltos, vtos);
  locals_index(T2);
  __ sd(FSR, T2, -wordSize);
}

void TemplateTable::fstore() {
  transition(ftos, vtos);
  locals_index(T2);
  __ swc1(FSF, T2, 0);
}

void TemplateTable::dstore() {
  transition(dtos, vtos);
  locals_index(T2);
  __ sdc1(FSF, T2, -wordSize);
}

void TemplateTable::astore() {
  transition(vtos, vtos);
  __ pop_ptr(FSR);
  locals_index(T2);
  __ sd(FSR, T2, 0);
}

void TemplateTable::wide_istore() {
  transition(vtos, vtos);
  __ pop_i(FSR);
  locals_index_wide(T2);
  __ sd(FSR, T2, 0);
}

void TemplateTable::wide_lstore() {
  transition(vtos, vtos);
  __ pop_l(FSR);
  locals_index_wide(T2);
  __ sd(FSR, T2, -wordSize);
}

void TemplateTable::wide_fstore() {
  wide_istore();
}

void TemplateTable::wide_dstore() {
  wide_lstore();
}

void TemplateTable::wide_astore() {
  transition(vtos, vtos);
  __ pop_ptr(FSR);
  locals_index_wide(T2);
  __ sd(FSR, T2, 0);
}

// used register : T2
void TemplateTable::iastore() {
  transition(itos, vtos);
  __ pop_i(SSR);   // T2: array  SSR: index
  if(UseBoundCheckInstruction) {
    __ pop_ptr(T2);
    __ dsll(SSR, SSR, Address::times_4);
    __ daddu(SSR, T2, SSR);
    __ addiu(SSR, SSR, arrayOopDesc::base_offset_in_bytes(T_INT));  // base

    __ lw(AT, T2, arrayOopDesc::length_offset_in_bytes());
    __ dsll(AT, AT, Address::times_4);
    __ daddu(AT, T2, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_INT));  //bound

    __ gsswle(FSR, SSR, AT);
  } else {
    index_check(T2, SSR);  // prefer index in SSR
    __ dsll(SSR, SSR, Address::times_4);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_INT), 8)) {
      __ gsswx(FSR, T2, SSR, arrayOopDesc::base_offset_in_bytes(T_INT));
    } else {
      __ daddu(T2, T2, SSR);
      __ sw(FSR, T2, arrayOopDesc::base_offset_in_bytes(T_INT));
    }
  }
}



// used register T2, T3
void TemplateTable::lastore() {
  transition(ltos, vtos);
  __ pop_i (T2);
  if(UseBoundCheckInstruction) {
    __ pop_ptr(T3);
    __ dsll(T2, T2, Address::times_8);
    __ daddu(T2, T3, T2);
    __ addiu(T2, T2, arrayOopDesc::base_offset_in_bytes(T_LONG) + 0 * wordSize);  // base

    __ lw(AT, T3, arrayOopDesc::length_offset_in_bytes());
    __ dsll(AT, AT, Address::times_8);
    __ daddu(AT, T3, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_LONG) + 0 * wordSize);  //bound

    __ gssdle(FSR, T2, AT);
  } else {
    index_check(T3, T2);
    __ dsll(T2, T2, Address::times_8);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_LONG), 8)) {
      __ gssdx(FSR, T3, T2, arrayOopDesc::base_offset_in_bytes(T_LONG));
    } else {
      __ daddu(T3, T3, T2);
      __ sd(FSR, T3, arrayOopDesc::base_offset_in_bytes(T_LONG));
    }
  }
}

// used register T2
void TemplateTable::fastore() {
  transition(ftos, vtos);
  __ pop_i(SSR);
  if(UseBoundCheckInstruction) {
    __ pop_ptr(T2);
    __ dsll(SSR, SSR, Address::times_4);
    __ daddu(SSR, T2, SSR);
    __ addiu(SSR, SSR, arrayOopDesc::base_offset_in_bytes(T_FLOAT));  // base

    __ lw(AT, T2, arrayOopDesc::length_offset_in_bytes());
    __ dsll(AT, AT, Address::times_4);
    __ daddu(AT, T2, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_FLOAT));  //bound

    __ gsswlec1(FSF, SSR, AT);
  } else {
    index_check(T2, SSR);
    __ dsll(SSR, SSR, Address::times_4);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_FLOAT), 8)) {
      __ gsswxc1(FSF, T2, SSR, arrayOopDesc::base_offset_in_bytes(T_FLOAT));
    } else {
      __ daddu(T2, T2, SSR);
      __ swc1(FSF, T2, arrayOopDesc::base_offset_in_bytes(T_FLOAT));
    }
  }
}

// used register T2, T3
void TemplateTable::dastore() {
  transition(dtos, vtos);
  __ pop_i (T2);
  if(UseBoundCheckInstruction) {
    __ pop_ptr(T3);
    __ dsll(T2, T2, Address::times_8);
    __ daddu(T2, T3, T2);
    __ addiu(T2, T2, arrayOopDesc::base_offset_in_bytes(T_DOUBLE) + 0 * wordSize);  // base

    __ lw(AT, T3, arrayOopDesc::length_offset_in_bytes());
    __ dsll(AT, AT, Address::times_8);
    __ daddu(AT, T3, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_DOUBLE) + 0 * wordSize);  //bound

    __ gssdlec1(FSF, T2, AT);
  } else {
    index_check(T3, T2);
    __ dsll(T2, T2, Address::times_8);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_DOUBLE), 8)) {
      __ gssdxc1(FSF, T3, T2, arrayOopDesc::base_offset_in_bytes(T_DOUBLE));
    } else {
      __ daddu(T3, T3, T2);
      __ sdc1(FSF, T3, arrayOopDesc::base_offset_in_bytes(T_DOUBLE));
    }
  }
}

// used register : T2, T3, T8
// T2 : array
// T3 : subklass
// T8 : supklass
void TemplateTable::aastore() {
  Label is_null, ok_is_subtype, done;
  transition(vtos, vtos);
  // stack: ..., array, index, value
  __ ld(FSR, at_tos());     // Value
  __ lw(SSR, at_tos_p1());  // Index
  __ ld(T2, at_tos_p2());  // Array

  // index_check(T2, SSR);
  index_check_without_pop(T2, SSR);
  // do array store check - check for NULL value first
  __ beq(FSR, R0, is_null);
  __ delayed()->nop();

  // Move subklass into T3
  //add for compressedoops
  __ load_klass(T3, FSR);
  // Move superklass into T8
  //add for compressedoops
  __ load_klass(T8, T2);
  __ ld(T8, Address(T8,  ObjArrayKlass::element_klass_offset()));
  // Compress array+index*4+12 into a single register. T2
  __ dsll(AT, SSR, UseCompressedOops? Address::times_4 : Address::times_8);
  __ daddu(T2, T2, AT);
  __ daddiu(T2, T2, arrayOopDesc::base_offset_in_bytes(T_OBJECT));

  // Generate subtype check.
  // Superklass in T8.  Subklass in T3.
  __ gen_subtype_check(T8, T3, ok_is_subtype);
  // Come here on failure
  // object is at FSR
  __ jmp(Interpreter::_throw_ArrayStoreException_entry);
  __ delayed()->nop();
  // Come here on success
  __ bind(ok_is_subtype);
  do_oop_store(_masm, Address(T2, 0), FSR, _bs->kind(), true);
  __ b(done);
  __ delayed()->nop();

  // Have a NULL in FSR, T2=array, SSR=index.  Store NULL at ary[idx]
  __ bind(is_null);
  __ profile_null_seen(T9);
  __ dsll(AT, SSR, UseCompressedOops? Address::times_4 : Address::times_8);
  __ daddu(T2, T2, AT);
  do_oop_store(_masm, Address(T2, arrayOopDesc::base_offset_in_bytes(T_OBJECT)), noreg, _bs->kind(), true);

  __ bind(done);
  __ daddiu(SP, SP, 3 * Interpreter::stackElementSize);
}

void TemplateTable::bastore() {
  transition(itos, vtos);
  __ pop_i(SSR);
  if(UseBoundCheckInstruction) {
    guarantee(false, "unimplemented yet!");
    __ pop_ptr(T2);
    __ daddu(SSR, T2, SSR);
    __ addiu(SSR, SSR, arrayOopDesc::base_offset_in_bytes(T_BYTE));  // base

    __ lw(AT, T2, arrayOopDesc::length_offset_in_bytes());
    __ daddu(AT, T2, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_BYTE));  //bound

    __ gssble(FSR, SSR, AT);
  } else {
    index_check(T2, SSR);

    // Need to check whether array is boolean or byte
    // since both types share the bastore bytecode.
    __ load_klass(T9, T2);
    __ lw(T9, T9, in_bytes(Klass::layout_helper_offset()));

    int diffbit = Klass::layout_helper_boolean_diffbit();
    __ move(AT, diffbit);

    Label L_skip;
    __ andr(AT, T9, AT);
    __ beq(AT, R0, L_skip);
    __ delayed()->nop();
    __ andi(FSR, FSR, 0x1);
    __ bind(L_skip);

    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_BYTE), 8)) {
      __ gssbx(FSR, T2, SSR, arrayOopDesc::base_offset_in_bytes(T_BYTE));
    } else {
      __ daddu(SSR, T2, SSR);
      __ sb(FSR, SSR, arrayOopDesc::base_offset_in_bytes(T_BYTE));
    }
  }
}

void TemplateTable::castore() {
  transition(itos, vtos);
  __ pop_i(SSR);
  if(UseBoundCheckInstruction) {
    __ pop_ptr(T2);
    __ dsll(SSR, SSR, Address::times_2);
    __ daddu(SSR, T2, SSR);
    __ addiu(SSR, SSR, arrayOopDesc::base_offset_in_bytes(T_CHAR));  // base

    __ lw(AT, T2, arrayOopDesc::length_offset_in_bytes());
    __ dsll(AT, AT, Address::times_2);
    __ daddu(AT, T2, AT);
    __ addiu(AT, AT, arrayOopDesc::base_offset_in_bytes(T_CHAR));  //bound

    __ gsshle(FSR, SSR, AT);
  } else {
    index_check(T2, SSR);
    __ dsll(SSR, SSR, Address::times_2);
    if (UseLEXT1 && Assembler::is_simm(arrayOopDesc::base_offset_in_bytes(T_CHAR), 8)) {
      __ gsshx(FSR, T2, SSR, arrayOopDesc::base_offset_in_bytes(T_CHAR));
    } else {
      __ daddu(SSR, T2, SSR);
      __ sh(FSR, SSR, arrayOopDesc::base_offset_in_bytes(T_CHAR));
    }
  }
}

void TemplateTable::sastore() {
  castore();
}

void TemplateTable::istore(int n) {
  transition(itos, vtos);
  __ sw(FSR, iaddress(n));
}

void TemplateTable::lstore(int n) {
  transition(ltos, vtos);
  __ sd(FSR, laddress(n));
}

void TemplateTable::fstore(int n) {
  transition(ftos, vtos);
  __ swc1(FSF, faddress(n));
}

void TemplateTable::dstore(int n) {
  transition(dtos, vtos);
  __ sdc1(FSF, laddress(n));
}

void TemplateTable::astore(int n) {
  transition(vtos, vtos);
  __ pop_ptr(FSR);
  __ sd(FSR, aaddress(n));
}

void TemplateTable::pop() {
  transition(vtos, vtos);
  __ daddiu(SP, SP, Interpreter::stackElementSize);
}

void TemplateTable::pop2() {
  transition(vtos, vtos);
  __ daddiu(SP, SP, 2 * Interpreter::stackElementSize);
}

void TemplateTable::dup() {
  transition(vtos, vtos);
  // stack: ..., a
  __ load_ptr(0, FSR);
  __ push_ptr(FSR);
  // stack: ..., a, a
}

// blows FSR
void TemplateTable::dup_x1() {
  transition(vtos, vtos);
  // stack: ..., a, b
  __ load_ptr(0, FSR);  // load b
  __ load_ptr(1, A5);  // load a
  __ store_ptr(1, FSR); // store b
  __ store_ptr(0, A5); // store a
  __ push_ptr(FSR);             // push b
  // stack: ..., b, a, b
}

// blows FSR
void TemplateTable::dup_x2() {
  transition(vtos, vtos);
  // stack: ..., a, b, c
  __ load_ptr(0, FSR);  // load c
  __ load_ptr(2, A5);  // load a
  __ store_ptr(2, FSR); // store c in a
  __ push_ptr(FSR);             // push c
  // stack: ..., c, b, c, c
  __ load_ptr(2, FSR);  // load b
  __ store_ptr(2, A5); // store a in b
  // stack: ..., c, a, c, c
  __ store_ptr(1, FSR); // store b in c
  // stack: ..., c, a, b, c
}

// blows FSR
void TemplateTable::dup2() {
  transition(vtos, vtos);
  // stack: ..., a, b
  __ load_ptr(1, FSR);  // load a
  __ push_ptr(FSR);             // push a
  __ load_ptr(1, FSR);  // load b
  __ push_ptr(FSR);             // push b
  // stack: ..., a, b, a, b
}

// blows FSR
void TemplateTable::dup2_x1() {
  transition(vtos, vtos);
  // stack: ..., a, b, c
  __ load_ptr(0, T2);  // load c
  __ load_ptr(1, FSR);  // load b
  __ push_ptr(FSR);             // push b
  __ push_ptr(T2);             // push c
  // stack: ..., a, b, c, b, c
  __ store_ptr(3, T2); // store c in b
  // stack: ..., a, c, c, b, c
  __ load_ptr(4, T2);  // load a
  __ store_ptr(2, T2); // store a in 2nd c
  // stack: ..., a, c, a, b, c
  __ store_ptr(4, FSR); // store b in a
  // stack: ..., b, c, a, b, c

  // stack: ..., b, c, a, b, c
}

// blows FSR, SSR
void TemplateTable::dup2_x2() {
  transition(vtos, vtos);
  // stack: ..., a, b, c, d
  // stack: ..., a, b, c, d
  __ load_ptr(0, T2);  // load d
  __ load_ptr(1, FSR);  // load c
  __ push_ptr(FSR);             // push c
  __ push_ptr(T2);             // push d
  // stack: ..., a, b, c, d, c, d
  __ load_ptr(4, FSR);  // load b
  __ store_ptr(2, FSR); // store b in d
  __ store_ptr(4, T2); // store d in b
  // stack: ..., a, d, c, b, c, d
  __ load_ptr(5, T2);  // load a
  __ load_ptr(3, FSR);  // load c
  __ store_ptr(3, T2); // store a in c
  __ store_ptr(5, FSR); // store c in a
  // stack: ..., c, d, a, b, c, d

  // stack: ..., c, d, a, b, c, d
}

// blows FSR
void TemplateTable::swap() {
  transition(vtos, vtos);
  // stack: ..., a, b

  __ load_ptr(1, A5);  // load a
  __ load_ptr(0, FSR);  // load b
  __ store_ptr(0, A5); // store a in b
  __ store_ptr(1, FSR); // store b in a

  // stack: ..., b, a
}

void TemplateTable::iop2(Operation op) {
  transition(itos, itos);

  __ pop_i(SSR);
  switch (op) {
    case add  : __ addu32(FSR, SSR, FSR); break;
    case sub  : __ subu32(FSR, SSR, FSR); break;
    case mul  : __ mul(FSR, SSR, FSR);    break;
    case _and : __ andr(FSR, SSR, FSR);   break;
    case _or  : __ orr(FSR, SSR, FSR);    break;
    case _xor : __ xorr(FSR, SSR, FSR);   break;
    case shl  : __ sllv(FSR, SSR, FSR);   break;
    case shr  : __ srav(FSR, SSR, FSR);   break;
    case ushr : __ srlv(FSR, SSR, FSR);   break;
    default   : ShouldNotReachHere();
  }
}

// the result stored in FSR, SSR,
// used registers : T2, T3
void TemplateTable::lop2(Operation op) {
  transition(ltos, ltos);
  __ pop_l(T2);

  switch (op) {
    case add : __ daddu(FSR, T2, FSR); break;
    case sub : __ dsubu(FSR, T2, FSR); break;
    case _and: __ andr(FSR, T2, FSR);  break;
    case _or : __ orr(FSR, T2, FSR);   break;
    case _xor: __ xorr(FSR, T2, FSR);  break;
    default : ShouldNotReachHere();
  }
}

// java require this bytecode could handle 0x80000000/-1, dont cause a overflow exception,
// the result is 0x80000000
// the godson2 cpu do the same, so we need not handle this specially like x86
void TemplateTable::idiv() {
  transition(itos, itos);
  Label not_zero;

  __ bne(FSR, R0, not_zero);
  __ delayed()->nop();
  __ jmp(Interpreter::_throw_ArithmeticException_entry);
  __ delayed()->nop();
  __ bind(not_zero);

  __ pop_i(SSR);
  if (UseLEXT1) {
    __ gsdiv(FSR, SSR, FSR);
  } else {
    __ div(SSR, FSR);
    __ mflo(FSR);
  }
}

void TemplateTable::irem() {
  transition(itos, itos);
  Label not_zero;
  __ pop_i(SSR);
  __ div(SSR, FSR);

  __ bne(FSR, R0, not_zero);
  __ delayed()->nop();
  //__ brk(7);
  __ jmp(Interpreter::_throw_ArithmeticException_entry);
  __ delayed()->nop();

  __ bind(not_zero);
  __ mfhi(FSR);
}

void TemplateTable::lmul() {
  transition(ltos, ltos);
  __ pop_l(T2);
  if (UseLEXT1) {
    __ gsdmult(FSR, T2, FSR);
  } else {
    __ dmult(T2, FSR);
    __ mflo(FSR);
  }
}

// NOTE: i DONT use the Interpreter::_throw_ArithmeticException_entry
void TemplateTable::ldiv() {
  transition(ltos, ltos);
  Label normal;

  __ bne(FSR, R0, normal);
  __ delayed()->nop();

  //__ brk(7);    //generate FPE
  __ jmp(Interpreter::_throw_ArithmeticException_entry);
  __ delayed()->nop();

  __ bind(normal);
  __ pop_l(A2);
  if (UseLEXT1) {
    __ gsddiv(FSR, A2, FSR);
  } else {
    __ ddiv(A2, FSR);
    __ mflo(FSR);
  }
}

// NOTE: i DONT use the Interpreter::_throw_ArithmeticException_entry
void TemplateTable::lrem() {
  transition(ltos, ltos);
  Label normal;

  __ bne(FSR, R0, normal);
  __ delayed()->nop();

  __ jmp(Interpreter::_throw_ArithmeticException_entry);
  __ delayed()->nop();

  __ bind(normal);
  __ pop_l (A2);

  if (UseLEXT1) {
    __ gsdmod(FSR, A2, FSR);
  } else {
    __ ddiv(A2, FSR);
    __ mfhi(FSR);
  }
}

// result in FSR
// used registers : T0
void TemplateTable::lshl() {
  transition(itos, ltos);
  __ pop_l(T0);
  __ dsllv(FSR, T0, FSR);
}

// used registers : T0
void TemplateTable::lshr() {
  transition(itos, ltos);
  __ pop_l(T0);
  __ dsrav(FSR, T0, FSR);
}

// used registers : T0
void TemplateTable::lushr() {
  transition(itos, ltos);
  __ pop_l(T0);
  __ dsrlv(FSR, T0, FSR);
}

// result in FSF
void TemplateTable::fop2(Operation op) {
  transition(ftos, ftos);
  switch (op) {
    case add:
      __ lwc1(FTF, at_sp());
      __ add_s(FSF, FTF, FSF);
      break;
    case sub:
      __ lwc1(FTF, at_sp());
      __ sub_s(FSF, FTF, FSF);
      break;
    case mul:
      __ lwc1(FTF, at_sp());
      __ mul_s(FSF, FTF, FSF);
      break;
    case div:
      __ lwc1(FTF, at_sp());
      __ div_s(FSF, FTF, FSF);
      break;
    case rem:
      __ mov_s(F13, FSF);
      __ lwc1(F12, at_sp());
       __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::frem), 2);
      break;
    default : ShouldNotReachHere();
  }

  __ daddiu(SP, SP, 1 * wordSize);
}

// result in SSF||FSF
// i dont handle the strict flags
void TemplateTable::dop2(Operation op) {
  transition(dtos, dtos);
  switch (op) {
    case add:
      __ ldc1(FTF, at_sp());
      __ add_d(FSF, FTF, FSF);
      break;
    case sub:
      __ ldc1(FTF, at_sp());
      __ sub_d(FSF, FTF, FSF);
      break;
    case mul:
      __ ldc1(FTF, at_sp());
      __ mul_d(FSF, FTF, FSF);
      break;
    case div:
      __ ldc1(FTF, at_sp());
      __ div_d(FSF, FTF, FSF);
      break;
    case rem:
      __ mov_d(F13, FSF);
      __ ldc1(F12, at_sp());
      __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::drem), 2);
      break;
    default : ShouldNotReachHere();
  }

  __ daddiu(SP, SP, 2 * wordSize);
}

void TemplateTable::ineg() {
  transition(itos, itos);
  __ subu32(FSR, R0, FSR);
}

void TemplateTable::lneg() {
  transition(ltos, ltos);
  __ dsubu(FSR, R0, FSR);
}

void TemplateTable::fneg() {
  transition(ftos, ftos);
  __ neg_s(FSF, FSF);
}

void TemplateTable::dneg() {
  transition(dtos, dtos);
  __ neg_d(FSF, FSF);
}

// used registers : T2
void TemplateTable::iinc() {
  transition(vtos, vtos);
  locals_index(T2);
  __ lw(FSR, T2, 0);
  __ lb(AT, at_bcp(2));           // get constant
  __ daddu(FSR, FSR, AT);
  __ sw(FSR, T2, 0);
}

// used register : T2
void TemplateTable::wide_iinc() {
  transition(vtos, vtos);
  locals_index_wide(T2);
  __ get_2_byte_integer_at_bcp(FSR, AT, 4);
  __ hswap(FSR);
  __ lw(AT, T2, 0);
  __ daddu(FSR, AT, FSR);
  __ sw(FSR, T2, 0);
}

void TemplateTable::convert() {
  // Checking
#ifdef ASSERT
  {
    TosState tos_in  = ilgl;
    TosState tos_out = ilgl;
    switch (bytecode()) {
      case Bytecodes::_i2l: // fall through
      case Bytecodes::_i2f: // fall through
      case Bytecodes::_i2d: // fall through
      case Bytecodes::_i2b: // fall through
      case Bytecodes::_i2c: // fall through
      case Bytecodes::_i2s: tos_in = itos; break;
      case Bytecodes::_l2i: // fall through
      case Bytecodes::_l2f: // fall through
      case Bytecodes::_l2d: tos_in = ltos; break;
      case Bytecodes::_f2i: // fall through
      case Bytecodes::_f2l: // fall through
      case Bytecodes::_f2d: tos_in = ftos; break;
      case Bytecodes::_d2i: // fall through
      case Bytecodes::_d2l: // fall through
      case Bytecodes::_d2f: tos_in = dtos; break;
      default             : ShouldNotReachHere();
    }
    switch (bytecode()) {
      case Bytecodes::_l2i: // fall through
      case Bytecodes::_f2i: // fall through
      case Bytecodes::_d2i: // fall through
      case Bytecodes::_i2b: // fall through
      case Bytecodes::_i2c: // fall through
      case Bytecodes::_i2s: tos_out = itos; break;
      case Bytecodes::_i2l: // fall through
      case Bytecodes::_f2l: // fall through
      case Bytecodes::_d2l: tos_out = ltos; break;
      case Bytecodes::_i2f: // fall through
      case Bytecodes::_l2f: // fall through
      case Bytecodes::_d2f: tos_out = ftos; break;
      case Bytecodes::_i2d: // fall through
      case Bytecodes::_l2d: // fall through
      case Bytecodes::_f2d: tos_out = dtos; break;
      default             : ShouldNotReachHere();
    }
    transition(tos_in, tos_out);
  }
#endif // ASSERT

  // Conversion
  switch (bytecode()) {
    case Bytecodes::_i2l:
      __ sll(FSR, FSR, 0);
      break;
    case Bytecodes::_i2f:
      __ mtc1(FSR, FSF);
      __ cvt_s_w(FSF, FSF);
      break;
    case Bytecodes::_i2d:
      __ mtc1(FSR, FSF);
      __ cvt_d_w(FSF, FSF);
      break;
    case Bytecodes::_i2b:
      __ seb(FSR, FSR);
      break;
    case Bytecodes::_i2c:
      __ andi(FSR, FSR, 0xFFFF);  // truncate upper 56 bits
      break;
    case Bytecodes::_i2s:
      __ seh(FSR, FSR);
      break;
    case Bytecodes::_l2i:
      __ sll(FSR, FSR, 0);
      break;
    case Bytecodes::_l2f:
      __ dmtc1(FSR, FSF);
      __ cvt_s_l(FSF, FSF);
      break;
    case Bytecodes::_l2d:
      __ dmtc1(FSR, FSF);
      __ cvt_d_l(FSF, FSF);
      break;
    case Bytecodes::_f2i:
    {
      Label L;

      __ trunc_w_s(F12, FSF);
      __ move(AT, 0x7fffffff);
      __ mfc1(FSR, F12);
      __ c_un_s(FSF, FSF);    //NaN?
      __ movt(FSR, R0);

      __ bne(AT, FSR, L);
      __ delayed()->lui(T9, 0x8000);

      __ mfc1(AT, FSF);
      __ andr(AT, AT, T9);

      __ movn(FSR, T9, AT);

      __ bind(L);
    }
      break;
    case Bytecodes::_f2l:
    {
      Label L;

      __ trunc_l_s(F12, FSF);
      __ daddiu(AT, R0, -1);
      __ dsrl(AT, AT, 1);
      __ dmfc1(FSR, F12);
      __ c_un_s(FSF, FSF);    //NaN?
      __ movt(FSR, R0);

      __ bne(AT, FSR, L);
      __ delayed()->lui(T9, 0x8000);

      __ mfc1(AT, FSF);
      __ andr(AT, AT, T9);

      __ dsll32(T9, T9, 0);
      __ movn(FSR, T9, AT);

      __ bind(L);
    }
      break;
    case Bytecodes::_f2d:
      __ cvt_d_s(FSF, FSF);
      break;
    case Bytecodes::_d2i:
    {
      Label L;

      __ trunc_w_d(F12, FSF);
      __ move(AT, 0x7fffffff);
      __ mfc1(FSR, F12);

      __ bne(FSR, AT, L);
      __ delayed()->mtc1(R0, F12);

      __ cvt_d_w(F12, F12);
      __ c_ult_d(FSF, F12);
      __ bc1f(L);
      __ delayed()->addiu(T9, R0, -1);

      __ c_un_d(FSF, FSF);    //NaN?
      __ subu32(FSR, T9, AT);
      __ movt(FSR, R0);

      __ bind(L);
    }
      break;
    case Bytecodes::_d2l:
    {
      Label L;

      __ trunc_l_d(F12, FSF);
      __ daddiu(AT, R0, -1);
      __ dsrl(AT, AT, 1);
      __ dmfc1(FSR, F12);

      __ bne(FSR, AT, L);
      __ delayed()->mtc1(R0, F12);

      __ cvt_d_w(F12, F12);
      __ c_ult_d(FSF, F12);
      __ bc1f(L);
      __ delayed()->daddiu(T9, R0, -1);

      __ c_un_d(FSF, FSF);    //NaN?
      __ subu(FSR, T9, AT);
      __ movt(FSR, R0);

    __ bind(L);
    }
      break;
    case Bytecodes::_d2f:
      __ cvt_s_d(FSF, FSF);
      break;
    default             :
      ShouldNotReachHere();
  }
}

void TemplateTable::lcmp() {
  transition(ltos, itos);

  Label low, high, done;
  __ pop(T0);
  __ pop(R0);
  __ slt(AT, T0, FSR);
  __ bne(AT, R0, low);
  __ delayed()->nop();

  __ bne(T0, FSR, high);
  __ delayed()->nop();

  __ li(FSR, (long)0);
  __ b(done);
  __ delayed()->nop();

  __ bind(low);
  __ li(FSR, (long)-1);
  __ b(done);
  __ delayed()->nop();

  __ bind(high);
  __ li(FSR, (long)1);
  __ b(done);
  __ delayed()->nop();

  __ bind(done);
}

void TemplateTable::float_cmp(bool is_float, int unordered_result) {
  Label less, done;

  __ move(FSR, R0);

  if (is_float) {
    __ lwc1(FTF, at_sp());
    __ c_eq_s(FTF, FSF);
    __ bc1t(done);
    __ delayed()->daddiu(SP, SP, 1 * wordSize);

    if (unordered_result<0)
      __ c_ult_s(FTF, FSF);
    else
      __ c_olt_s(FTF, FSF);
  } else {
    __ ldc1(FTF, at_sp());
    __ c_eq_d(FTF, FSF);
    __ bc1t(done);
    __ delayed()->daddiu(SP, SP, 2 * wordSize);

    if (unordered_result<0)
      __ c_ult_d(FTF, FSF);
    else
      __ c_olt_d(FTF, FSF);
  }
  __ bc1t(less);
  __ delayed()->nop();
  __ move(FSR, 1);
  __ b(done);
  __ delayed()->nop();
  __ bind(less);
  __ move(FSR, -1);
  __ bind(done);
}


// used registers : T3, A7, Rnext
// FSR : return bci, this is defined by the vm specification
// T2 : MDO taken count
// T3 : method
// A7 : offset
// Rnext : next bytecode, this is required by dispatch_base
void TemplateTable::branch(bool is_jsr, bool is_wide) {
  __ get_method(T3);
  __ profile_taken_branch(A7, T2);    // only C2 meaningful

  const ByteSize be_offset = MethodCounters::backedge_counter_offset() +
                             InvocationCounter::counter_offset();
  const ByteSize inv_offset = MethodCounters::invocation_counter_offset() +
                              InvocationCounter::counter_offset();

  // Load up T4 with the branch displacement
  if (!is_wide) {
    __ lb(A7, BCP, 1);
    __ lbu(AT, BCP, 2);
    __ dsll(A7, A7, 8);
    __ orr(A7, A7, AT);
  } else {
    __ get_4_byte_integer_at_bcp(A7, AT, 1);
    __ swap(A7);
  }

  // Handle all the JSR stuff here, then exit.
  // It's much shorter and cleaner than intermingling with the non-JSR
  // normal-branch stuff occuring below.
  if (is_jsr) {
    // Pre-load the next target bytecode into Rnext
    __ daddu(AT, BCP, A7);
    __ lbu(Rnext, AT, 0);

    // compute return address as bci in FSR
    __ daddiu(FSR, BCP, (is_wide?5:3) - in_bytes(ConstMethod::codes_offset()));
    __ ld(AT, T3, in_bytes(Method::const_offset()));
    __ dsubu(FSR, FSR, AT);
    // Adjust the bcp in BCP by the displacement in A7
    __ daddu(BCP, BCP, A7);
    // jsr returns atos that is not an oop
    // Push return address
    __ push_i(FSR);
    // jsr returns vtos
    __ dispatch_only_noverify(vtos);

    return;
  }

  // Normal (non-jsr) branch handling

  // Adjust the bcp in S0 by the displacement in T4
  __ daddu(BCP, BCP, A7);

  assert(UseLoopCounter || !UseOnStackReplacement, "on-stack-replacement requires loop counters");
  Label backedge_counter_overflow;
  Label profile_method;
  Label dispatch;
  if (UseLoopCounter) {
    // increment backedge counter for backward branches
    // T3: method
    // T4: target offset
    // BCP: target bcp
    // LVP: locals pointer
    __ bgtz(A7, dispatch);  // check if forward or backward branch
    __ delayed()->nop();

    // check if MethodCounters exists
    Label has_counters;
    __ ld(AT, T3, in_bytes(Method::method_counters_offset()));  // use AT as MDO, TEMP
    __ bne(AT, R0, has_counters);
    __ delayed()->nop();
    __ push(T3);
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::build_method_counters),
               T3);
    __ pop(T3);
    __ ld(AT, T3, in_bytes(Method::method_counters_offset()));  // use AT as MDO, TEMP
    __ beq(AT, R0, dispatch);
    __ delayed()->nop();
    __ bind(has_counters);

    if (TieredCompilation) {
      Label no_mdo;
      int increment = InvocationCounter::count_increment;
      int mask = ((1 << Tier0BackedgeNotifyFreqLog) - 1) << InvocationCounter::count_shift;
      if (ProfileInterpreter) {
        // Are we profiling?
        __ ld(T0, Address(T3, in_bytes(Method::method_data_offset())));
        __ beq(T0, R0, no_mdo);
        __ delayed()->nop();
        // Increment the MDO backedge counter
        const Address mdo_backedge_counter(T0, in_bytes(MethodData::backedge_counter_offset()) +
                                           in_bytes(InvocationCounter::counter_offset()));
        __ increment_mask_and_jump(mdo_backedge_counter, increment, mask,
                                   T1, false, Assembler::zero, &backedge_counter_overflow);
        __ beq(R0, R0, dispatch);
        __ delayed()->nop();
      }
      __ bind(no_mdo);
      // Increment backedge counter in MethodCounters*
      __ ld(T0, Address(T3, Method::method_counters_offset()));
      __ increment_mask_and_jump(Address(T0, be_offset), increment, mask,
                                 T1, false, Assembler::zero, &backedge_counter_overflow);
      if (!UseOnStackReplacement) {
        __ bind(backedge_counter_overflow);
      }
    } else {
      // increment back edge counter
      __ ld(T1, T3, in_bytes(Method::method_counters_offset()));
      __ lw(T0, T1, in_bytes(be_offset));
      __ increment(T0, InvocationCounter::count_increment);
      __ sw(T0, T1, in_bytes(be_offset));

      // load invocation counter
      __ lw(T1, T1, in_bytes(inv_offset));
      // buffer bit added, mask no needed

      // daddu backedge counter & invocation counter
      __ daddu(T1, T1, T0);

      if (ProfileInterpreter) {
        // Test to see if we should create a method data oop
        // T1 : backedge counter & invocation counter
        if (Assembler::is_simm16(InvocationCounter::InterpreterProfileLimit)) {
          __ slti(AT, T1, InvocationCounter::InterpreterProfileLimit);
        } else {
          __ li(AT, (long)&InvocationCounter::InterpreterProfileLimit);
          __ lw(AT, AT, 0);
          __ slt(AT, T1, AT);
        }

        __ bne(AT, R0, dispatch);
        __ delayed()->nop();

        // if no method data exists, go to profile method
        __ test_method_data_pointer(T1, profile_method);

        if (UseOnStackReplacement) {
          if (Assembler::is_simm16(InvocationCounter::InterpreterBackwardBranchLimit)) {
            __ slti(AT, T2, InvocationCounter::InterpreterBackwardBranchLimit);
          } else {
            __ li(AT, (long)&InvocationCounter::InterpreterBackwardBranchLimit);
            __ lw(AT, AT, 0);
            __ slt(AT, T2, AT);
          }

          __ bne(AT, R0, dispatch);
          __ delayed()->nop();

          // When ProfileInterpreter is on, the backedge_count comes
          // from the methodDataOop, which value does not get reset on
          // the call to  frequency_counter_overflow().
          // To avoid excessive calls to the overflow routine while
          // the method is being compiled, daddu a second test to make
          // sure the overflow function is called only once every
          // overflow_frequency.
          const int overflow_frequency = 1024;
          __ andi(AT, T2, overflow_frequency-1);
          __ beq(AT, R0, backedge_counter_overflow);
          __ delayed()->nop();
        }
      } else {
        if (UseOnStackReplacement) {
          // check for overflow against AT, which is the sum of the counters
          __ li(AT, (long)&InvocationCounter::InterpreterBackwardBranchLimit);
          __ lw(AT, AT, 0);
          __ slt(AT, T1, AT);
          __ beq(AT, R0, backedge_counter_overflow);
          __ delayed()->nop();
        }
      }
    }
    __ bind(dispatch);
  }

  // Pre-load the next target bytecode into Rnext
  __ lbu(Rnext, BCP, 0);

  // continue with the bytecode @ target
  // FSR: return bci for jsr's, unused otherwise
  // Rnext: target bytecode
  // BCP: target bcp
  __ dispatch_only(vtos);

  if (UseLoopCounter) {
    if (ProfileInterpreter) {
      // Out-of-line code to allocate method data oop.
      __ bind(profile_method);
      __ call_VM(NOREG, CAST_FROM_FN_PTR(address, InterpreterRuntime::profile_method));
      __ lbu(Rnext, BCP, 0);
      __ set_method_data_pointer_for_bcp();
      __ b(dispatch);
      __ delayed()->nop();
    }

    if (UseOnStackReplacement) {
      // invocation counter overflow
      __ bind(backedge_counter_overflow);
      __ subu(A7, BCP, A7);  // branch bcp
      call_VM(NOREG, CAST_FROM_FN_PTR(address,
      InterpreterRuntime::frequency_counter_overflow), A7);
      __ lbu(Rnext, BCP, 0);

      // V0: osr nmethod (osr ok) or NULL (osr not possible)
      // V1: osr adapter frame return address
      // Rnext: target bytecode
      // LVP: locals pointer
      // BCP: bcp
      __ beq(V0, R0, dispatch);
      __ delayed()->nop();
      // nmethod may have been invalidated (VM may block upon call_VM return)
      __ lw(T3, V0, nmethod::entry_bci_offset());
      __ move(AT, InvalidOSREntryBci);
      __ beq(AT, T3, dispatch);
      __ delayed()->nop();
      // We need to prepare to execute the OSR method. First we must
      // migrate the locals and monitors off of the stack.
      //V0: osr nmethod (osr ok) or NULL (osr not possible)
      //V1: osr adapter frame return address
      //Rnext: target bytecode
      //LVP: locals pointer
      //BCP: bcp
      __ move(BCP, V0);
      const Register thread = TREG;
#ifndef OPT_THREAD
      __ get_thread(thread);
#endif
      call_VM(noreg, CAST_FROM_FN_PTR(address, SharedRuntime::OSR_migration_begin));

      // V0 is OSR buffer, move it to expected parameter location
      // refer to osrBufferPointer in c1_LIRAssembler_mips.cpp
      __ move(T0, V0);

      // pop the interpreter frame
      __ ld(A7, Address(FP, frame::interpreter_frame_sender_sp_offset * wordSize));
      //FIXME, shall we keep the return address on the stack?
      __ leave();                                // remove frame anchor
      __ move(LVP, RA);
      __ move(SP, A7);

      __ move(AT, -(StackAlignmentInBytes));
      __ andr(SP , SP , AT);

      // push the (possibly adjusted) return address
      //refer to osr_entry in c1_LIRAssembler_mips.cpp
      __ ld(AT, BCP, nmethod::osr_entry_point_offset());
      __ jr(AT);
      __ delayed()->nop();
    }
  }
}


void TemplateTable::if_0cmp(Condition cc) {
  transition(itos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  switch(cc) {
    case not_equal:
      __ beq(FSR, R0, not_taken);
      break;
    case equal:
      __ bne(FSR, R0, not_taken);
      break;
    case less:
      __ bgez(FSR, not_taken);
      break;
    case less_equal:
      __ bgtz(FSR, not_taken);
      break;
    case greater:
      __ blez(FSR, not_taken);
      break;
    case greater_equal:
      __ bltz(FSR, not_taken);
      break;
  }
  __ delayed()->nop();

  branch(false, false);

  __ bind(not_taken);
  __ profile_not_taken_branch(FSR);
}

void TemplateTable::if_icmp(Condition cc) {
  transition(itos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;

  __ pop_i(SSR);
  switch(cc) {
    case not_equal:
      __ beq(SSR, FSR, not_taken);
      break;
    case equal:
      __ bne(SSR, FSR, not_taken);
      break;
    case less:
      __ slt(AT, SSR, FSR);
      __ beq(AT, R0, not_taken);
      break;
    case less_equal:
      __ slt(AT, FSR, SSR);
      __ bne(AT, R0, not_taken);
      break;
    case greater:
      __ slt(AT, FSR, SSR);
      __ beq(AT, R0, not_taken);
      break;
    case greater_equal:
      __ slt(AT, SSR, FSR);
      __ bne(AT, R0, not_taken);
      break;
  }
  __ delayed()->nop();

  branch(false, false);
  __ bind(not_taken);
  __ profile_not_taken_branch(FSR);
}

void TemplateTable::if_nullcmp(Condition cc) {
  transition(atos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  switch(cc) {
    case not_equal:
      __ beq(FSR, R0, not_taken);
      break;
    case equal:
      __ bne(FSR, R0, not_taken);
      break;
    default:
      ShouldNotReachHere();
  }
  __ delayed()->nop();

  branch(false, false);
  __ bind(not_taken);
  __ profile_not_taken_branch(FSR);
}


void TemplateTable::if_acmp(Condition cc) {
  transition(atos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  //  __ lw(SSR, SP, 0);
  __ pop_ptr(SSR);
  switch(cc) {
    case not_equal:
      __ beq(SSR, FSR, not_taken);
      break;
    case equal:
      __ bne(SSR, FSR, not_taken);
      break;
    default:
      ShouldNotReachHere();
  }
  __ delayed()->nop();

  branch(false, false);

  __ bind(not_taken);
  __ profile_not_taken_branch(FSR);
}

// used registers : T1, T2, T3
// T1 : method
// T2 : returb bci
void TemplateTable::ret() {
  transition(vtos, vtos);

  locals_index(T2);
  __ ld(T2, T2, 0);
  __ profile_ret(T2, T3);

  __ get_method(T1);
  __ ld(BCP, T1, in_bytes(Method::const_offset()));
  __ daddu(BCP, BCP, T2);
  __ daddiu(BCP, BCP, in_bytes(ConstMethod::codes_offset()));

  __ dispatch_next(vtos);
}

// used registers : T1, T2, T3
// T1 : method
// T2 : returb bci
void TemplateTable::wide_ret() {
  transition(vtos, vtos);

  locals_index_wide(T2);
  __ ld(T2, T2, 0);                   // get return bci, compute return bcp
  __ profile_ret(T2, T3);

  __ get_method(T1);
  __ ld(BCP, T1, in_bytes(Method::const_offset()));
  __ daddu(BCP, BCP, T2);
  __ daddiu(BCP, BCP, in_bytes(ConstMethod::codes_offset()));

  __ dispatch_next(vtos);
}

// used register T2, T3, A7, Rnext
// T2 : bytecode pointer
// T3 : low
// A7 : high
// Rnext : dest bytecode, required by dispatch_base
void TemplateTable::tableswitch() {
  Label default_case, continue_execution;
  transition(itos, vtos);

  // align BCP
  __ daddiu(T2, BCP, BytesPerInt);
  __ li(AT, -BytesPerInt);
  __ andr(T2, T2, AT);

  // load lo & hi
  __ lw(T3, T2, 1 * BytesPerInt);
  __ swap(T3);
  __ lw(A7, T2, 2 * BytesPerInt);
  __ swap(A7);

  // check against lo & hi
  __ slt(AT, FSR, T3);
  __ bne(AT, R0, default_case);
  __ delayed()->nop();

  __ slt(AT, A7, FSR);
  __ bne(AT, R0, default_case);
  __ delayed()->nop();

  // lookup dispatch offset, in A7 big endian
  __ dsubu(FSR, FSR, T3);
  __ dsll(AT, FSR, Address::times_4);
  __ daddu(AT, T2, AT);
  __ lw(A7, AT, 3 * BytesPerInt);
  __ profile_switch_case(FSR, T9, T3);

  __ bind(continue_execution);
  __ swap(A7);
  __ daddu(BCP, BCP, A7);
  __ lbu(Rnext, BCP, 0);
  __ dispatch_only(vtos);

  // handle default
  __ bind(default_case);
  __ profile_switch_default(FSR);
  __ lw(A7, T2, 0);
  __ b(continue_execution);
  __ delayed()->nop();
}

void TemplateTable::lookupswitch() {
  transition(itos, itos);
  __ stop("lookupswitch bytecode should have been rewritten");
}

// used registers : T2, T3, A7, Rnext
// T2 : bytecode pointer
// T3 : pair index
// A7 : offset
// Rnext : dest bytecode
// the data after the opcode is the same as lookupswitch
// see Rewriter::rewrite_method for more information
void TemplateTable::fast_linearswitch() {
  transition(itos, vtos);
  Label loop_entry, loop, found, continue_execution;

  // swap FSR so we can avoid swapping the table entries
  __ swap(FSR);

  // align BCP
  __ daddiu(T2, BCP, BytesPerInt);
  __ li(AT, -BytesPerInt);
  __ andr(T2, T2, AT);

  // set counter
  __ lw(T3, T2, BytesPerInt);
  __ swap(T3);
  __ b(loop_entry);
  __ delayed()->nop();

  // table search
  __ bind(loop);
  // get the entry value
  __ dsll(AT, T3, Address::times_8);
  __ daddu(AT, T2, AT);
  __ lw(AT, AT, 2 * BytesPerInt);

  // found?
  __ beq(FSR, AT, found);
  __ delayed()->nop();

  __ bind(loop_entry);
  __ bgtz(T3, loop);
  __ delayed()->daddiu(T3, T3, -1);

  // default case
  __ profile_switch_default(FSR);
  __ lw(A7, T2, 0);
  __ b(continue_execution);
  __ delayed()->nop();

  // entry found -> get offset
  __ bind(found);
  __ dsll(AT, T3, Address::times_8);
  __ daddu(AT, T2, AT);
  __ lw(A7, AT, 3 * BytesPerInt);
  __ profile_switch_case(T3, FSR, T2);

  // continue execution
  __ bind(continue_execution);
  __ swap(A7);
  __ daddu(BCP, BCP, A7);
  __ lbu(Rnext, BCP, 0);
  __ dispatch_only(vtos);
}

// used registers : T0, T1, T2, T3, A7, Rnext
// T2 : pairs address(array)
// Rnext : dest bytecode
// the data after the opcode is the same as lookupswitch
// see Rewriter::rewrite_method for more information
void TemplateTable::fast_binaryswitch() {
  transition(itos, vtos);
  // Implementation using the following core algorithm:
  //
  // int binary_search(int key, LookupswitchPair* array, int n) {
  //   // Binary search according to "Methodik des Programmierens" by
  //   // Edsger W. Dijkstra and W.H.J. Feijen, Addison Wesley Germany 1985.
  //   int i = 0;
  //   int j = n;
  //   while (i+1 < j) {
  //     // invariant P: 0 <= i < j <= n and (a[i] <= key < a[j] or Q)
  //     // with      Q: for all i: 0 <= i < n: key < a[i]
  //     // where a stands for the array and assuming that the (inexisting)
  //     // element a[n] is infinitely big.
  //     int h = (i + j) >> 1;
  //     // i < h < j
  //     if (key < array[h].fast_match()) {
  //       j = h;
  //     } else {
  //       i = h;
  //     }
  //   }
  //   // R: a[i] <= key < a[i+1] or Q
  //   // (i.e., if key is within array, i is the correct index)
  //   return i;
  // }

  // register allocation
  const Register array = T2;
  const Register i = T3, j = A7;
  const Register h = T1;
  const Register temp = T0;
  const Register key = FSR;

  // setup array
  __ daddiu(array, BCP, 3*BytesPerInt);
  __ li(AT, -BytesPerInt);
  __ andr(array, array, AT);

  // initialize i & j
  __ move(i, R0);
  __ lw(j, array, - 1 * BytesPerInt);
  // Convert j into native byteordering
  __ swap(j);

  // and start
  Label entry;
  __ b(entry);
  __ delayed()->nop();

  // binary search loop
  {
    Label loop;
    __ bind(loop);
    // int h = (i + j) >> 1;
    __ daddu(h, i, j);
    __ dsrl(h, h, 1);
    // if (key < array[h].fast_match()) {
    //   j = h;
    // } else {
    //   i = h;
    // }
    // Convert array[h].match to native byte-ordering before compare
    __ dsll(AT, h, Address::times_8);
    __ daddu(AT, array, AT);
    __ lw(temp, AT, 0 * BytesPerInt);
    __ swap(temp);

    {
      Label set_i, end_of_if;
      __ slt(AT, key, temp);
      __ beq(AT, R0, set_i);
      __ delayed()->nop();

      __ b(end_of_if);
      __ delayed(); __ move(j, h);

      __ bind(set_i);
      __ move(i, h);

      __ bind(end_of_if);
    }
    // while (i+1 < j)
    __ bind(entry);
    __ daddiu(h, i, 1);
    __ slt(AT, h, j);
    __ bne(AT, R0, loop);
    __ delayed()->nop();
  }

  // end of binary search, result index is i (must check again!)
  Label default_case;
  // Convert array[i].match to native byte-ordering before compare
  __ dsll(AT, i, Address::times_8);
  __ daddu(AT, array, AT);
  __ lw(temp, AT, 0 * BytesPerInt);
  __ swap(temp);
  __ bne(key, temp, default_case);
  __ delayed()->nop();

  // entry found -> j = offset
  __ dsll(AT, i, Address::times_8);
  __ daddu(AT, array, AT);
  __ lw(j, AT, 1 * BytesPerInt);
  __ profile_switch_case(i, key, array);
  __ swap(j);

  __ daddu(BCP, BCP, j);
  __ lbu(Rnext, BCP, 0);
  __ dispatch_only(vtos);

  // default case -> j = default offset
  __ bind(default_case);
  __ profile_switch_default(i);
  __ lw(j, array, - 2 * BytesPerInt);
  __ swap(j);
  __ daddu(BCP, BCP, j);
  __ lbu(Rnext, BCP, 0);
  __ dispatch_only(vtos);
}

void TemplateTable::_return(TosState state) {
  transition(state, state);
  assert(_desc->calls_vm(),
      "inconsistent calls_vm information"); // call in remove_activation

  if (_desc->bytecode() == Bytecodes::_return_register_finalizer) {
    assert(state == vtos, "only valid state");
    __ ld(T1, aaddress(0));
    __ load_klass(LVP, T1);
    __ lw(LVP, LVP, in_bytes(Klass::access_flags_offset()));
    __ move(AT, JVM_ACC_HAS_FINALIZER);
    __ andr(AT, AT, LVP);
    Label skip_register_finalizer;
    __ beq(AT, R0, skip_register_finalizer);
    __ delayed()->nop();
    __ call_VM(noreg, CAST_FROM_FN_PTR(address,
    InterpreterRuntime::register_finalizer), T1);
    __ bind(skip_register_finalizer);
  }

  // Narrow result if state is itos but result type is smaller.
  // Need to narrow in the return bytecode rather than in generate_return_entry
  // since compiled code callers expect the result to already be narrowed.
  if (state == itos) {
    __ narrow(FSR);
  }

  __ remove_activation(state, T9);
  __ sync();

  __ jr(T9);
  __ delayed()->nop();
}

// ----------------------------------------------------------------------------
// Volatile variables demand their effects be made known to all CPU's
// in order.  Store buffers on most chips allow reads & writes to
// reorder; the JMM's ReadAfterWrite.java test fails in -Xint mode
// without some kind of memory barrier (i.e., it's not sufficient that
// the interpreter does not reorder volatile references, the hardware
// also must not reorder them).
//
// According to the new Java Memory Model (JMM):
// (1) All volatiles are serialized wrt to each other.  ALSO reads &
//     writes act as aquire & release, so:
// (2) A read cannot let unrelated NON-volatile memory refs that
//     happen after the read float up to before the read.  It's OK for
//     non-volatile memory refs that happen before the volatile read to
//     float down below it.
// (3) Similar a volatile write cannot let unrelated NON-volatile
//     memory refs that happen BEFORE the write float down to after the
//     write.  It's OK for non-volatile memory refs that happen after the
//     volatile write to float up before it.
//
// We only put in barriers around volatile refs (they are expensive),
// not _between_ memory refs (that would require us to track the
// flavor of the previous memory refs).  Requirements (2) and (3)
// require some barriers before volatile stores and after volatile
// loads.  These nearly cover requirement (1) but miss the
// volatile-store-volatile-load case.  This final case is placed after
// volatile-stores although it could just as well go before
// volatile-loads.
void TemplateTable::volatile_barrier() {
  if(os::is_MP()) __ sync();
}

// we dont shift left 2 bits in get_cache_and_index_at_bcp
// for we always need shift the index we use it. the ConstantPoolCacheEntry
// is 16-byte long, index is the index in
// ConstantPoolCache, so cache + base_offset() + index * 16 is
// the corresponding ConstantPoolCacheEntry
// used registers : T2
// NOTE : the returned index need also shift left 4 to get the address!
void TemplateTable::resolve_cache_and_index(int byte_no,
                                            Register Rcache,
                                            Register index,
                                            size_t index_size) {
  assert(byte_no == f1_byte || byte_no == f2_byte, "byte_no out of range");
  const Register temp = A1;
  assert_different_registers(Rcache, index);

  Label resolved;
  __ get_cache_and_index_and_bytecode_at_bcp(Rcache, index, temp, byte_no, 1, index_size);
  // is resolved?
  int i = (int)bytecode();
  __ addiu(temp, temp, -i);
  __ beq(temp, R0, resolved);
  __ delayed()->nop();
  // resolve first time through
  address entry;
  switch (bytecode()) {
    case Bytecodes::_getstatic      : // fall through
    case Bytecodes::_putstatic      : // fall through
    case Bytecodes::_getfield       : // fall through
    case Bytecodes::_putfield       :
      entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_get_put);
      break;
    case Bytecodes::_invokevirtual  : // fall through
    case Bytecodes::_invokespecial  : // fall through
    case Bytecodes::_invokestatic   : // fall through
    case Bytecodes::_invokeinterface:
      entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invoke);
      break;
    case Bytecodes::_invokehandle:
      entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invokehandle);
      break;
    case Bytecodes::_invokedynamic:
      entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invokedynamic);
      break;
    default                          :
      fatal(err_msg("unexpected bytecode: %s", Bytecodes::name(bytecode())));
      break;
  }

  __ move(temp, i);
  __ call_VM(NOREG, entry, temp);

  // Update registers with resolved info
  __ get_cache_and_index_at_bcp(Rcache, index, 1, index_size);
  __ bind(resolved);
}

// The Rcache and index registers must be set before call
void TemplateTable::load_field_cp_cache_entry(Register obj,
                                              Register cache,
                                              Register index,
                                              Register off,
                                              Register flags,
                                              bool is_static = false) {
  assert_different_registers(cache, index, flags, off);

  ByteSize cp_base_offset = ConstantPoolCache::base_offset();
  // Field offset
  __ dsll(AT, index, Address::times_ptr);
  __ daddu(AT, cache, AT);
  __ ld(off, AT, in_bytes(cp_base_offset + ConstantPoolCacheEntry::f2_offset()));
  // Flags
  __ ld(flags, AT, in_bytes(cp_base_offset + ConstantPoolCacheEntry::flags_offset()));

  // klass overwrite register
  if (is_static) {
    __ ld(obj, AT, in_bytes(cp_base_offset + ConstantPoolCacheEntry::f1_offset()));
    const int mirror_offset = in_bytes(Klass::java_mirror_offset());
    __ ld(obj, Address(obj, mirror_offset));

    __ verify_oop(obj);
  }
}

// get the method, itable_index and flags of the current invoke
void TemplateTable::load_invoke_cp_cache_entry(int byte_no,
                                               Register method,
                                               Register itable_index,
                                               Register flags,
                                               bool is_invokevirtual,
                                               bool is_invokevfinal, /*unused*/
                                               bool is_invokedynamic) {
  // setup registers
  const Register cache = T3;
  const Register index = T1;
  assert_different_registers(method, flags);
  assert_different_registers(method, cache, index);
  assert_different_registers(itable_index, flags);
  assert_different_registers(itable_index, cache, index);
  assert(is_invokevirtual == (byte_no == f2_byte), "is invokevirtual flag redundant");
  // determine constant pool cache field offsets
  const int method_offset = in_bytes(
    ConstantPoolCache::base_offset() +
      ((byte_no == f2_byte)
       ? ConstantPoolCacheEntry::f2_offset()
       : ConstantPoolCacheEntry::f1_offset()));
  const int flags_offset = in_bytes(ConstantPoolCache::base_offset() +
                                    ConstantPoolCacheEntry::flags_offset());
  // access constant pool cache fields
  const int index_offset = in_bytes(ConstantPoolCache::base_offset() +
                                    ConstantPoolCacheEntry::f2_offset());

  size_t index_size = (is_invokedynamic ? sizeof(u4): sizeof(u2));
  resolve_cache_and_index(byte_no, cache, index, index_size);

  //assert(wordSize == 8, "adjust code below");
  // note we shift 4 not 2, for we get is the true inde
  // of ConstantPoolCacheEntry, not the shifted 2-bit index as x86 version
  __ dsll(AT, index, Address::times_ptr);
  __ daddu(AT, cache, AT);
  __ ld(method, AT, method_offset);

  if (itable_index != NOREG) {
    __ ld(itable_index, AT, index_offset);
  }
  __ ld(flags, AT, flags_offset);
}

// The registers cache and index expected to be set before call.
// Correct values of the cache and index registers are preserved.
void TemplateTable::jvmti_post_field_access(Register cache, Register index,
                                            bool is_static, bool has_tos) {
  // do the JVMTI work here to avoid disturbing the register state below
  // We use c_rarg registers here because we want to use the register used in
  // the call to the VM
  if (JvmtiExport::can_post_field_access()) {
    // Check to see if a field access watch has been set before we
    // take the time to call into the VM.
    Label L1;
    // kill FSR
    Register tmp1 = T2;
    Register tmp2 = T1;
    Register tmp3 = T3;
    assert_different_registers(cache, index, AT);
    __ li(AT, (intptr_t)JvmtiExport::get_field_access_count_addr());
    __ lw(AT, AT, 0);
    __ beq(AT, R0, L1);
    __ delayed()->nop();

    __ get_cache_and_index_at_bcp(tmp2, tmp3, 1);

    // cache entry pointer
    __ daddiu(tmp2, tmp2, in_bytes(ConstantPoolCache::base_offset()));
    __ shl(tmp3, LogBytesPerWord);
    __ daddu(tmp2, tmp2, tmp3);
    if (is_static) {
      __ move(tmp1, R0);
    } else {
      __ ld(tmp1, SP, 0);
      __ verify_oop(tmp1);
    }
    // tmp1: object pointer or NULL
    // tmp2: cache entry pointer
    // tmp3: jvalue object on the stack
    __ call_VM(NOREG, CAST_FROM_FN_PTR(address,
                                       InterpreterRuntime::post_field_access),
               tmp1, tmp2, tmp3);
    __ get_cache_and_index_at_bcp(cache, index, 1);
    __ bind(L1);
  }
}

void TemplateTable::pop_and_check_object(Register r) {
  __ pop_ptr(r);
  __ null_check(r);  // for field access must check obj.
  __ verify_oop(r);
}

// used registers : T1, T2, T3, T1
// T1 : flags
// T2 : off
// T3 : obj
// T1 : field address
// The flags 31, 30, 29, 28 together build a 4 bit number 0 to 8 with the
// following mapping to the TosState states:
// btos: 0
// ctos: 1
// stos: 2
// itos: 3
// ltos: 4
// ftos: 5
// dtos: 6
// atos: 7
// vtos: 8
// see ConstantPoolCacheEntry::set_field for more info
void TemplateTable::getfield_or_static(int byte_no, bool is_static) {
  transition(vtos, vtos);

  const Register cache = T3;
  const Register index = T0;

  const Register obj   = T3;
  const Register off   = T2;
  const Register flags = T1;

  const Register scratch = T8;

  resolve_cache_and_index(byte_no, cache, index, sizeof(u2));
  jvmti_post_field_access(cache, index, is_static, false);
  load_field_cp_cache_entry(obj, cache, index, off, flags, is_static);

  {
    __ move(scratch, 1 << ConstantPoolCacheEntry::is_volatile_shift);
    __ andr(scratch, scratch, flags);

    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }

  if (!is_static) pop_and_check_object(obj);
  __ daddu(index, obj, off);


  Label Done, notByte, notBool, notInt, notShort, notChar,
              notLong, notFloat, notObj, notDouble;

  assert(btos == 0, "change code, btos != 0");
  __ dsrl(flags, flags, ConstantPoolCacheEntry::tos_state_shift);
  __ andi(flags, flags, ConstantPoolCacheEntry::tos_state_mask);
  __ bne(flags, R0, notByte);
  __ delayed()->nop();

  // btos
  __ lb(FSR, index, 0);
  __ push(btos);

  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_bgetfield, T3, T2);
  }
  __ b(Done);
  __ delayed()->nop();


  __ bind(notByte);
  __ move(AT, ztos);
  __ bne(flags, AT, notBool);
  __ delayed()->nop();

  // ztos
  __ lb(FSR, index, 0);
  __ push(ztos);

  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_bgetfield, T3, T2);
  }
  __ b(Done);
  __ delayed()->nop();


  __ bind(notBool);
  __ move(AT, itos);
  __ bne(flags, AT, notInt);
  __ delayed()->nop();

  // itos
  __ lw(FSR, index, 0);
  __ push(itos);

  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_igetfield, T3, T2);
  }
  __ b(Done);
  __ delayed()->nop();

  __ bind(notInt);
  __ move(AT, atos);
  __ bne(flags, AT, notObj);
  __ delayed()->nop();

  // atos
  //add for compressedoops
  __ load_heap_oop(FSR, Address(index, 0));
  __ push(atos);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_agetfield, T3, T2);
  }
  __ b(Done);
  __ delayed()->nop();

  __ bind(notObj);
  __ move(AT, ctos);
  __ bne(flags, AT, notChar);
  __ delayed()->nop();

  // ctos
  __ lhu(FSR, index, 0);
  __ push(ctos);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_cgetfield, T3, T2);
  }
  __ b(Done);
  __ delayed()->nop();

  __ bind(notChar);
  __ move(AT, stos);
  __ bne(flags, AT, notShort);
  __ delayed()->nop();

  // stos
  __ lh(FSR, index, 0);
  __ push(stos);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_sgetfield, T3, T2);
  }
  __ b(Done);
  __ delayed()->nop();

  __ bind(notShort);
  __ move(AT, ltos);
  __ bne(flags, AT, notLong);
  __ delayed()->nop();

  // FIXME : the load/store should be atomic, we have no simple method to do this in mips32
  // ltos
  __ ld(FSR, index, 0 * wordSize);
  __ push(ltos);

  // Don't rewrite to _fast_lgetfield for potential volatile case.
  __ b(Done);
  __ delayed()->nop();

  __ bind(notLong);
  __ move(AT, ftos);
  __ bne(flags, AT, notFloat);
  __ delayed()->nop();

  // ftos
  __ lwc1(FSF, index, 0);
  __ push(ftos);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_fgetfield, T3, T2);
  }
  __ b(Done);
  __ delayed()->nop();

  __ bind(notFloat);
  __ move(AT, dtos);
#ifdef ASSERT
  __ bne(flags, AT, notDouble);
  __ delayed()->nop();
#endif

  // dtos
  __ ldc1(FSF, index, 0 * wordSize);
  __ push(dtos);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_dgetfield, T3, T2);
  }


#ifdef ASSERT
  __ b(Done);
  __ delayed()->nop();
  __ bind(notDouble);
  __ stop("Bad state");
#endif

  __ bind(Done);

  {
    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }
}


void TemplateTable::getfield(int byte_no) {
  getfield_or_static(byte_no, false);
}

void TemplateTable::getstatic(int byte_no) {
  getfield_or_static(byte_no, true);
}

// The registers cache and index expected to be set before call.
// The function may destroy various registers, just not the cache and index registers.
void TemplateTable::jvmti_post_field_mod(Register cache, Register index, bool is_static) {
  transition(vtos, vtos);

  ByteSize cp_base_offset = ConstantPoolCache::base_offset();

  if (JvmtiExport::can_post_field_modification()) {
    // Check to see if a field modification watch has been set before
    // we take the time to call into the VM.
    Label L1;
    //kill AT, T1, T2, T3, T9
    Register tmp1 = T2;
    Register tmp2 = T1;
    Register tmp3 = T3;
    Register tmp4 = T9;
    assert_different_registers(cache, index, tmp4);

    __ li(AT, JvmtiExport::get_field_modification_count_addr());
    __ lw(AT, AT, 0);
    __ beq(AT, R0, L1);
    __ delayed()->nop();

    __ get_cache_and_index_at_bcp(tmp2, tmp4, 1);

    if (is_static) {
      __ move(tmp1, R0);
    } else {
      // Life is harder. The stack holds the value on top, followed by
      // the object.  We don't know the size of the value, though; it
      // could be one or two words depending on its type. As a result,
      // we must find the type to determine where the object is.
      Label two_word, valsize_known;
      __ dsll(AT, tmp4, Address::times_8);
      __ daddu(AT, tmp2, AT);
      __ ld(tmp3, AT, in_bytes(cp_base_offset +
                               ConstantPoolCacheEntry::flags_offset()));
      __ shr(tmp3, ConstantPoolCacheEntry::tos_state_shift);

      ConstantPoolCacheEntry::verify_tos_state_shift();
      __ move(tmp1, SP);
      __ move(AT, ltos);
      __ beq(tmp3, AT, two_word);
      __ delayed()->nop();
      __ move(AT, dtos);
      __ beq(tmp3, AT, two_word);
      __ delayed()->nop();
      __ b(valsize_known);
      __ delayed()->daddiu(tmp1, tmp1, Interpreter::expr_offset_in_bytes(1) );

      __ bind(two_word);
      __ daddiu(tmp1, tmp1, Interpreter::expr_offset_in_bytes(2));

      __ bind(valsize_known);
      // setup object pointer
      __ ld(tmp1, tmp1, 0*wordSize);
    }
    // cache entry pointer
    __ daddiu(tmp2, tmp2, in_bytes(cp_base_offset));
    __ shl(tmp4, LogBytesPerWord);
    __ daddu(tmp2, tmp2, tmp4);
    // object (tos)
    __ move(tmp3, SP);
    // tmp1: object pointer set up above (NULL if static)
    // tmp2: cache entry pointer
    // tmp3: jvalue object on the stack
    __ call_VM(NOREG,
               CAST_FROM_FN_PTR(address,
                                InterpreterRuntime::post_field_modification),
               tmp1, tmp2, tmp3);
    __ get_cache_and_index_at_bcp(cache, index, 1);
    __ bind(L1);
  }
}

// used registers : T0, T1, T2, T3, T8
// T1 : flags
// T2 : off
// T3 : obj
// T8 : volatile bit
// see ConstantPoolCacheEntry::set_field for more info
void TemplateTable::putfield_or_static(int byte_no, bool is_static) {
  transition(vtos, vtos);

  const Register cache = T3;
  const Register index = T0;
  const Register obj   = T3;
  const Register off   = T2;
  const Register flags = T1;
  const Register bc    = T3;

  const Register scratch = T8;

  resolve_cache_and_index(byte_no, cache, index, sizeof(u2));
  jvmti_post_field_mod(cache, index, is_static);
  load_field_cp_cache_entry(obj, cache, index, off, flags, is_static);

  Label Done;
  {
    __ move(scratch, 1 << ConstantPoolCacheEntry::is_volatile_shift);
    __ andr(scratch, scratch, flags);

    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }


  Label notByte, notBool, notInt, notShort, notChar, notLong, notFloat, notObj, notDouble;

  assert(btos == 0, "change code, btos != 0");

  // btos
  __ dsrl(flags, flags, ConstantPoolCacheEntry::tos_state_shift);
  __ andi(flags, flags, ConstantPoolCacheEntry::tos_state_mask);
  __ bne(flags, R0, notByte);
  __ delayed()->nop();

  __ pop(btos);
  if (!is_static) {
    pop_and_check_object(obj);
  }
  __ daddu(AT, obj, off);
  __ sb(FSR, AT, 0);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_bputfield, bc, off, true, byte_no);
  }
  __ b(Done);
  __ delayed()->nop();

  // ztos
  __ bind(notByte);
  __ move(AT, ztos);
  __ bne(flags, AT, notBool);
  __ delayed()->nop();

  __ pop(ztos);
  if (!is_static) {
    pop_and_check_object(obj);
  }
  __ daddu(AT, obj, off);
  __ andi(FSR, FSR, 0x1);
  __ sb(FSR, AT, 0);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_zputfield, bc, off, true, byte_no);
  }
  __ b(Done);
  __ delayed()->nop();

  // itos
  __ bind(notBool);
  __ move(AT, itos);
  __ bne(flags, AT, notInt);
  __ delayed()->nop();

  __ pop(itos);
  if (!is_static) {
    pop_and_check_object(obj);
  }
  __ daddu(AT, obj, off);
  __ sw(FSR, AT, 0);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_iputfield, bc, off, true, byte_no);
  }
  __ b(Done);
  __ delayed()->nop();

  // atos
  __ bind(notInt);
  __ move(AT, atos);
  __ bne(flags, AT, notObj);
  __ delayed()->nop();

  __ pop(atos);
  if (!is_static) {
    pop_and_check_object(obj);
  }

  do_oop_store(_masm, Address(obj, off, Address::times_1, 0), FSR, _bs->kind(), false);

  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_aputfield, bc, off, true, byte_no);
  }
  __ b(Done);
  __ delayed()->nop();

  // ctos
  __ bind(notObj);
  __ move(AT, ctos);
  __ bne(flags, AT, notChar);
  __ delayed()->nop();

  __ pop(ctos);
  if (!is_static) {
    pop_and_check_object(obj);
  }
  __ daddu(AT, obj, off);
  __ sh(FSR, AT, 0);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_cputfield, bc, off, true, byte_no);
  }
  __ b(Done);
  __ delayed()->nop();

  // stos
  __ bind(notChar);
  __ move(AT, stos);
  __ bne(flags, AT, notShort);
  __ delayed()->nop();

  __ pop(stos);
  if (!is_static) {
    pop_and_check_object(obj);
  }
  __ daddu(AT, obj, off);
  __ sh(FSR, AT, 0);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_sputfield, bc, off, true, byte_no);
  }
  __ b(Done);
  __ delayed()->nop();

  // ltos
  __ bind(notShort);
  __ move(AT, ltos);
  __ bne(flags, AT, notLong);
  __ delayed()->nop();

  __ pop(ltos);
  if (!is_static) {
    pop_and_check_object(obj);
  }
  __ daddu(AT, obj, off);
  __ sd(FSR, AT, 0);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_lputfield, bc, off, true, byte_no);
  }
  __ b(Done);
  __ delayed()->nop();

  // ftos
  __ bind(notLong);
  __ move(AT, ftos);
  __ bne(flags, AT, notFloat);
  __ delayed()->nop();

  __ pop(ftos);
  if (!is_static) {
    pop_and_check_object(obj);
  }
  __ daddu(AT, obj, off);
  __ swc1(FSF, AT, 0);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_fputfield, bc, off, true, byte_no);
  }
  __ b(Done);
  __ delayed()->nop();


  // dtos
  __ bind(notFloat);
  __ move(AT, dtos);
#ifdef ASSERT
  __ bne(flags, AT, notDouble);
  __ delayed()->nop();
#endif

  __ pop(dtos);
  if (!is_static) {
    pop_and_check_object(obj);
  }
  __ daddu(AT, obj, off);
  __ sdc1(FSF, AT, 0);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_dputfield, bc, off, true, byte_no);
  }

#ifdef ASSERT
  __ b(Done);
  __ delayed()->nop();

  __ bind(notDouble);
  __ stop("Bad state");
#endif

  __ bind(Done);

  {
    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }
}

void TemplateTable::putfield(int byte_no) {
  putfield_or_static(byte_no, false);
}

void TemplateTable::putstatic(int byte_no) {
  putfield_or_static(byte_no, true);
}

// used registers : T1, T2, T3
// T1 : cp_entry
// T2 : obj
// T3 : value pointer
void TemplateTable::jvmti_post_fast_field_mod() {
  if (JvmtiExport::can_post_field_modification()) {
    // Check to see if a field modification watch has been set before
    // we take the time to call into the VM.
    Label L2;
    //kill AT, T1, T2, T3, T9
    Register tmp1 = T2;
    Register tmp2 = T1;
    Register tmp3 = T3;
    Register tmp4 = T9;
    __ li(AT, JvmtiExport::get_field_modification_count_addr());
    __ lw(tmp3, AT, 0);
    __ beq(tmp3, R0, L2);
    __ delayed()->nop();
    __ pop_ptr(tmp1);
    __ verify_oop(tmp1);
    __ push_ptr(tmp1);
    switch (bytecode()) {          // load values into the jvalue object
    case Bytecodes::_fast_aputfield: __ push_ptr(FSR); break;
    case Bytecodes::_fast_bputfield: // fall through
    case Bytecodes::_fast_zputfield: // fall through
    case Bytecodes::_fast_sputfield: // fall through
    case Bytecodes::_fast_cputfield: // fall through
    case Bytecodes::_fast_iputfield: __ push_i(FSR); break;
    case Bytecodes::_fast_dputfield: __ push_d(FSF); break;
    case Bytecodes::_fast_fputfield: __ push_f(); break;
    case Bytecodes::_fast_lputfield: __ push_l(FSR); break;
      default:  ShouldNotReachHere();
    }
    __ move(tmp3, SP);
    // access constant pool cache entry
    __ get_cache_entry_pointer_at_bcp(tmp2, FSR, 1);
    __ verify_oop(tmp1);
    // tmp1: object pointer copied above
    // tmp2: cache entry pointer
    // tmp3: jvalue object on the stack
    __ call_VM(NOREG,
               CAST_FROM_FN_PTR(address,
                                InterpreterRuntime::post_field_modification),
               tmp1, tmp2, tmp3);

    switch (bytecode()) {             // restore tos values
    case Bytecodes::_fast_aputfield: __ pop_ptr(FSR); break;
    case Bytecodes::_fast_bputfield: // fall through
    case Bytecodes::_fast_zputfield: // fall through
    case Bytecodes::_fast_sputfield: // fall through
    case Bytecodes::_fast_cputfield: // fall through
    case Bytecodes::_fast_iputfield: __ pop_i(FSR); break;
    case Bytecodes::_fast_dputfield: __ pop_d(); break;
    case Bytecodes::_fast_fputfield: __ pop_f(); break;
    case Bytecodes::_fast_lputfield: __ pop_l(FSR); break;
    }
    __ bind(L2);
  }
}

// used registers : T2, T3, T1
// T2 : index & off & field address
// T3 : cache & obj
// T1 : flags
void TemplateTable::fast_storefield(TosState state) {
  transition(state, vtos);

  const Register scratch = T8;

  ByteSize base = ConstantPoolCache::base_offset();

  jvmti_post_fast_field_mod();

  // access constant pool cache
  __ get_cache_and_index_at_bcp(T3, T2, 1);

  // Must prevent reordering of the following cp cache loads with bytecode load
  __ sync();

  // test for volatile with T1
  __ dsll(AT, T2, Address::times_8);
  __ daddu(AT, T3, AT);
  __ ld(T1, AT, in_bytes(base + ConstantPoolCacheEntry::flags_offset()));

  // replace index with field offset from cache entry
  __ ld(T2, AT, in_bytes(base + ConstantPoolCacheEntry::f2_offset()));

  Label Done;
  {
    __ move(scratch, 1 << ConstantPoolCacheEntry::is_volatile_shift);
    __ andr(scratch, scratch, T1);

    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }

  // Get object from stack
  pop_and_check_object(T3);

  if (bytecode() != Bytecodes::_fast_aputfield) {
    // field address
    __ daddu(T2, T3, T2);
  }

  // access field
  switch (bytecode()) {
    case Bytecodes::_fast_zputfield:
      __ andi(FSR, FSR, 0x1);  // boolean is true if LSB is 1
      // fall through to bputfield
    case Bytecodes::_fast_bputfield:
      __ sb(FSR, T2, 0);
      break;
    case Bytecodes::_fast_sputfield: // fall through
    case Bytecodes::_fast_cputfield:
      __ sh(FSR, T2, 0);
      break;
    case Bytecodes::_fast_iputfield:
      __ sw(FSR, T2, 0);
      break;
    case Bytecodes::_fast_lputfield:
      __ sd(FSR, T2, 0 * wordSize);
      break;
    case Bytecodes::_fast_fputfield:
      __ swc1(FSF, T2, 0);
      break;
    case Bytecodes::_fast_dputfield:
      __ sdc1(FSF, T2, 0 * wordSize);
      break;
    case Bytecodes::_fast_aputfield:
      do_oop_store(_masm, Address(T3, T2, Address::times_1, 0), FSR, _bs->kind(), false);
      break;
    default:
      ShouldNotReachHere();
  }

  {
    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }
}

// used registers : T2, T3, T1
// T3 : cp_entry & cache
// T2 : index & offset
void TemplateTable::fast_accessfield(TosState state) {
  transition(atos, state);

  const Register scratch = T8;

  // do the JVMTI work here to avoid disturbing the register state below
  if (JvmtiExport::can_post_field_access()) {
    // Check to see if a field access watch has been set before we take
    // the time to call into the VM.
    Label L1;
    __ li(AT, (intptr_t)JvmtiExport::get_field_access_count_addr());
    __ lw(T3, AT, 0);
    __ beq(T3, R0, L1);
    __ delayed()->nop();
    // access constant pool cache entry
    __ get_cache_entry_pointer_at_bcp(T3, T1, 1);
    __ move(TSR, FSR);
    __ verify_oop(FSR);
    // FSR: object pointer copied above
    // T3: cache entry pointer
    __ call_VM(NOREG,
               CAST_FROM_FN_PTR(address, InterpreterRuntime::post_field_access),
               FSR, T3);
    __ move(FSR, TSR);
    __ bind(L1);
  }

  // access constant pool cache
  __ get_cache_and_index_at_bcp(T3, T2, 1);

  // Must prevent reordering of the following cp cache loads with bytecode load
  __ sync();

  // replace index with field offset from cache entry
  __ dsll(AT, T2, Address::times_8);
  __ daddu(AT, T3, AT);
  __ ld(T2, AT, in_bytes(ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::f2_offset()));

  {
    __ ld(AT, AT, in_bytes(ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::flags_offset()));
    __ move(scratch, 1 << ConstantPoolCacheEntry::is_volatile_shift);
    __ andr(scratch, scratch, AT);

    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }

  // FSR: object
  __ verify_oop(FSR);
  __ null_check(FSR);
  // field addresses
  __ daddu(FSR, FSR, T2);

  // access field
  switch (bytecode()) {
    case Bytecodes::_fast_bgetfield:
      __ lb(FSR, FSR, 0);
      break;
    case Bytecodes::_fast_sgetfield:
      __ lh(FSR, FSR, 0);
      break;
    case Bytecodes::_fast_cgetfield:
      __ lhu(FSR, FSR, 0);
      break;
    case Bytecodes::_fast_igetfield:
      __ lw(FSR, FSR, 0);
      break;
    case Bytecodes::_fast_lgetfield:
      __ stop("should not be rewritten");
      break;
    case Bytecodes::_fast_fgetfield:
      __ lwc1(FSF, FSR, 0);
      break;
    case Bytecodes::_fast_dgetfield:
      __ ldc1(FSF, FSR, 0);
      break;
    case Bytecodes::_fast_agetfield:
      //add for compressedoops
      __ load_heap_oop(FSR, Address(FSR, 0));
      __ verify_oop(FSR);
      break;
    default:
      ShouldNotReachHere();
  }

  {
    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }
}

// generator for _fast_iaccess_0, _fast_aaccess_0, _fast_faccess_0
// used registers : T1, T2, T3, T1
// T1 : obj & field address
// T2 : off
// T3 : cache
// T1 : index
void TemplateTable::fast_xaccess(TosState state) {
  transition(vtos, state);

  const Register scratch = T8;

  // get receiver
  __ ld(T1, aaddress(0));
  // access constant pool cache
  __ get_cache_and_index_at_bcp(T3, T2, 2);
  __ dsll(AT, T2, Address::times_8);
  __ daddu(AT, T3, AT);
  __ ld(T2, AT, in_bytes(ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::f2_offset()));

  {
    __ ld(AT, AT, in_bytes(ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::flags_offset()));
    __ move(scratch, 1 << ConstantPoolCacheEntry::is_volatile_shift);
    __ andr(scratch, scratch, AT);

    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }

  // make sure exception is reported in correct bcp range (getfield is
  // next instruction)
  __ daddiu(BCP, BCP, 1);
  __ null_check(T1);
  __ daddu(T1, T1, T2);

  if (state == itos) {
    __ lw(FSR, T1, 0);
  } else if (state == atos) {
    __ load_heap_oop(FSR, Address(T1, 0));
    __ verify_oop(FSR);
  } else if (state == ftos) {
    __ lwc1(FSF, T1, 0);
  } else {
    ShouldNotReachHere();
  }
  __ daddiu(BCP, BCP, -1);

  {
    Label notVolatile;
    __ beq(scratch, R0, notVolatile);
    __ delayed()->nop();
    volatile_barrier();
    __ bind(notVolatile);
  }
}



//-----------------------------------------------------------------------------
// Calls

void TemplateTable::count_calls(Register method, Register temp) {
  // implemented elsewhere
  ShouldNotReachHere();
}

// method, index, recv, flags: T1, T2, T3, T1
// byte_no = 2 for _invokevirtual, 1 else
// T0 : return address
// get the method & index of the invoke, and push the return address of
// the invoke(first word in the frame)
// this address is where the return code jmp to.
// NOTE : this method will set T3&T1 as recv&flags
void TemplateTable::prepare_invoke(int byte_no,
                                   Register method,  // linked method (or i-klass)
                                   Register index,   // itable index, MethodType, etc.
                                   Register recv,    // if caller wants to see it
                                   Register flags    // if caller wants to test it
                                   ) {
  // determine flags
  const Bytecodes::Code code = bytecode();
  const bool is_invokeinterface  = code == Bytecodes::_invokeinterface;
  const bool is_invokedynamic    = code == Bytecodes::_invokedynamic;
  const bool is_invokehandle     = code == Bytecodes::_invokehandle;
  const bool is_invokevirtual    = code == Bytecodes::_invokevirtual;
  const bool is_invokespecial    = code == Bytecodes::_invokespecial;
  const bool load_receiver       = (recv  != noreg);
  const bool save_flags          = (flags != noreg);
  assert(load_receiver == (code != Bytecodes::_invokestatic && code != Bytecodes::_invokedynamic),"");
  assert(save_flags    == (is_invokeinterface || is_invokevirtual), "need flags for vfinal");
  assert(flags == noreg || flags == T1, "error flags reg.");
  assert(recv  == noreg || recv  == T3, "error recv reg.");

  // setup registers & access constant pool cache
  if(recv == noreg) recv  = T3;
  if(flags == noreg) flags  = T1;
  assert_different_registers(method, index, recv, flags);

  // save 'interpreter return address'
  __ save_bcp();

  load_invoke_cp_cache_entry(byte_no, method, index, flags, is_invokevirtual, false, is_invokedynamic);

  if (is_invokedynamic || is_invokehandle) {
   Label L_no_push;
     __ move(AT, (1 << ConstantPoolCacheEntry::has_appendix_shift));
     __ andr(AT, AT, flags);
     __ beq(AT, R0, L_no_push);
     __ delayed()->nop();
     // Push the appendix as a trailing parameter.
     // This must be done before we get the receiver,
     // since the parameter_size includes it.
     Register tmp = SSR;
     __ push(tmp);
     __ move(tmp, index);
     assert(ConstantPoolCacheEntry::_indy_resolved_references_appendix_offset == 0, "appendix expected at index+0");
     __ load_resolved_reference_at_index(index, tmp);
     __ pop(tmp);
     __ push(index);  // push appendix (MethodType, CallSite, etc.)
     __ bind(L_no_push);
  }

  // load receiver if needed (after appendix is pushed so parameter size is correct)
  // Note: no return address pushed yet
  if (load_receiver) {
    __ move(AT, ConstantPoolCacheEntry::parameter_size_mask);
    __ andr(recv, flags, AT);
    // Since we won't push RA on stack, no_return_pc_pushed_yet should be 0.
    const int no_return_pc_pushed_yet = 0;  // argument slot correction before we push return address
    const int receiver_is_at_end      = -1;  // back off one slot to get receiver
    Address recv_addr = __ argument_address(recv, no_return_pc_pushed_yet + receiver_is_at_end);
    __ ld(recv, recv_addr);
    __ verify_oop(recv);
  }
  if(save_flags) {
    __ move(BCP, flags);
  }

  // compute return type
  __ dsrl(flags, flags, ConstantPoolCacheEntry::tos_state_shift);
  __ andi(flags, flags, 0xf);

  // Make sure we don't need to mask flags for tos_state_shift after the above shift
  ConstantPoolCacheEntry::verify_tos_state_shift();
  // load return address
  {
    const address table = (address) Interpreter::invoke_return_entry_table_for(code);
    __ li(AT, (long)table);
    __ dsll(flags, flags, LogBytesPerWord);
    __ daddu(AT, AT, flags);
    __ ld(RA, AT, 0);
  }

  if (save_flags) {
    __ move(flags, BCP);
    __ restore_bcp();
  }
}

// used registers : T0, T3, T1, T2
// T3 : recv, this two register using convention is by prepare_invoke
// T1 : flags, klass
// Rmethod : method, index must be Rmethod
void TemplateTable::invokevirtual_helper(Register index,
                                         Register recv,
                                         Register flags) {

  assert_different_registers(index, recv, flags, T2);

  // Test for an invoke of a final method
  Label notFinal;
  __ move(AT, (1 << ConstantPoolCacheEntry::is_vfinal_shift));
  __ andr(AT, flags, AT);
  __ beq(AT, R0, notFinal);
  __ delayed()->nop();

  Register method = index;  // method must be Rmethod
  assert(method == Rmethod, "methodOop must be Rmethod for interpreter calling convention");

  // do the call - the index is actually the method to call
  // the index is indeed methodOop, for this is vfinal,
  // see ConstantPoolCacheEntry::set_method for more info

  __ verify_oop(method);

  // It's final, need a null check here!
  __ null_check(recv);

  // profile this call
  __ profile_final_call(T2);

  // T2: tmp, used for mdp
  // method: callee
  // T9: tmp
  // is_virtual: true
  __ profile_arguments_type(T2, method, T9, true);

  __ jump_from_interpreted(method, T2);

  __ bind(notFinal);

  // get receiver klass
  __ null_check(recv, oopDesc::klass_offset_in_bytes());
  __ load_klass(T2, recv);
  __ verify_oop(T2);

  // profile this call
  __ profile_virtual_call(T2, T0, T1);

  // get target methodOop & entry point
  const int base = InstanceKlass::vtable_start_offset() * wordSize;
  assert(vtableEntry::size() * wordSize == wordSize, "adjust the scaling in the code below");
  __ dsll(AT, index, Address::times_ptr);
  // T2: receiver
  __ daddu(AT, T2, AT);
  //this is a ualign read
  __ ld(method, AT, base + vtableEntry::method_offset_in_bytes());
  __ profile_arguments_type(T2, method, T9, true);
  __ jump_from_interpreted(method, T2);

}

void TemplateTable::invokevirtual(int byte_no) {
  transition(vtos, vtos);
  assert(byte_no == f2_byte, "use this argument");
  prepare_invoke(byte_no, Rmethod, NOREG, T3, T1);
  // now recv & flags in T3, T1
  invokevirtual_helper(Rmethod, T3, T1);
}

// T9 : entry
// Rmethod : method
void TemplateTable::invokespecial(int byte_no) {
  transition(vtos, vtos);
  assert(byte_no == f1_byte, "use this argument");
  prepare_invoke(byte_no, Rmethod, NOREG, T3);
  // now recv & flags in T3, T1
  __ verify_oop(T3);
  __ null_check(T3);
  __ profile_call(T9);

  // T8: tmp, used for mdp
  // Rmethod: callee
  // T9: tmp
  // is_virtual: false
  __ profile_arguments_type(T8, Rmethod, T9, false);

  __ jump_from_interpreted(Rmethod, T9);
  __ move(T0, T3);
}

void TemplateTable::invokestatic(int byte_no) {
  transition(vtos, vtos);
  assert(byte_no == f1_byte, "use this argument");
  prepare_invoke(byte_no, Rmethod, NOREG);
  __ verify_oop(Rmethod);

  __ profile_call(T9);

  // T8: tmp, used for mdp
  // Rmethod: callee
  // T9: tmp
  // is_virtual: false
  __ profile_arguments_type(T8, Rmethod, T9, false);

  __ jump_from_interpreted(Rmethod, T9);
}

// i have no idea what to do here, now. for future change. FIXME.
void TemplateTable::fast_invokevfinal(int byte_no) {
  transition(vtos, vtos);
  assert(byte_no == f2_byte, "use this argument");
  __ stop("fast_invokevfinal not used on mips64");
}

// used registers : T0, T1, T2, T3, T1, A7
// T0 : itable, vtable, entry
// T1 : interface
// T3 : receiver
// T1 : flags, klass
// Rmethod : index, method, this is required by interpreter_entry
void TemplateTable::invokeinterface(int byte_no) {
  transition(vtos, vtos);
  //this method will use T1-T4 and T0
  assert(byte_no == f1_byte, "use this argument");
  prepare_invoke(byte_no, T2, Rmethod, T3, T1);
  // T2: reference klass
  // Rmethod: method
  // T3: receiver
  // T1: flags

  // Special case of invokeinterface called for virtual method of
  // java.lang.Object.  See cpCacheOop.cpp for details.
  // This code isn't produced by javac, but could be produced by
  // another compliant java compiler.
  Label notMethod;
  __ move(AT, (1 << ConstantPoolCacheEntry::is_forced_virtual_shift));
  __ andr(AT, T1, AT);
  __ beq(AT, R0, notMethod);
  __ delayed()->nop();

  invokevirtual_helper(Rmethod, T3, T1);
  __ bind(notMethod);
  // Get receiver klass into T1 - also a null check
  //add for compressedoops
  __ load_klass(T1, T3);
  __ verify_oop(T1);

  Label no_such_interface, no_such_method;

  // Receiver subtype check against REFC.
  // Superklass in T2. Subklass in T1.
  __ lookup_interface_method(// inputs: rec. class, interface, itable index
                             T1, T2, noreg,
                             // outputs: scan temp. reg, scan temp. reg
                             T0, FSR,
                             no_such_interface,
                             /*return_method=*/false);


  // profile this call
  __ profile_virtual_call(T1, T0, FSR);

  // Get declaring interface class from method, and itable index
  __ ld_ptr(T2, Rmethod, in_bytes(Method::const_offset()));
  __ ld_ptr(T2, T2, in_bytes(ConstMethod::constants_offset()));
  __ ld_ptr(T2, T2, ConstantPool::pool_holder_offset_in_bytes());
  __ lw(Rmethod, Rmethod, in_bytes(Method::itable_index_offset()));
  __ addiu(Rmethod, Rmethod, (-1) * Method::itable_index_max);
  __ subu32(Rmethod, R0, Rmethod);

  __ lookup_interface_method(// inputs: rec. class, interface, itable index
                             T1, T2, Rmethod,
                             // outputs: method, scan temp. reg
                             Rmethod, T0,
                             no_such_interface);

  // Rmethod: Method* to call
  // T3: receiver
  // Check for abstract method error
  // Note: This should be done more efficiently via a throw_abstract_method_error
  //       interpreter entry point and a conditional jump to it in case of a null
  //       method.
  __ beq(Rmethod, R0, no_such_method);
  __ delayed()->nop();

  __ profile_arguments_type(T1, Rmethod, T0, true);

  // do the call
  // T3: receiver
  // Rmethod: Method*
  __ jump_from_interpreted(Rmethod, T1);
  __ should_not_reach_here();

  // exception handling code follows...
  // note: must restore interpreter registers to canonical
  //       state for exception handling to work correctly!

  __ bind(no_such_method);
  // throw exception
  __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::throw_AbstractMethodError));
  // the call_VM checks for exception, so we should never return here.
  __ should_not_reach_here();

  __ bind(no_such_interface);
  // throw exception
  __ call_VM(noreg, CAST_FROM_FN_PTR(address,
                   InterpreterRuntime::throw_IncompatibleClassChangeError));
  // the call_VM checks for exception, so we should never return here.
  __ should_not_reach_here();

}


void TemplateTable::invokehandle(int byte_no) {
  transition(vtos, vtos);
  assert(byte_no == f1_byte, "use this argument");
  const Register T2_method  = Rmethod;
  const Register FSR_mtype  = FSR;
  const Register T3_recv    = T3;

  if (!EnableInvokeDynamic) {
     // rewriter does not generate this bytecode
     __ should_not_reach_here();
     return;
   }

   prepare_invoke(byte_no, T2_method, FSR_mtype, T3_recv);
   //??__ verify_method_ptr(T2_method);
   __ verify_oop(T3_recv);
   __ null_check(T3_recv);

   // T9: MethodType object (from cpool->resolved_references[f1], if necessary)
   // T2_method: MH.invokeExact_MT method (from f2)

   // Note:  T9 is already pushed (if necessary) by prepare_invoke

   // FIXME: profile the LambdaForm also
   __ profile_final_call(T9);

   // T8: tmp, used for mdp
   // T2_method: callee
   // T9: tmp
   // is_virtual: true
   __ profile_arguments_type(T8, T2_method, T9, true);

  __ jump_from_interpreted(T2_method, T9);
}

 void TemplateTable::invokedynamic(int byte_no) {
   transition(vtos, vtos);
   assert(byte_no == f1_byte, "use this argument");

   if (!EnableInvokeDynamic) {
     // We should not encounter this bytecode if !EnableInvokeDynamic.
     // The verifier will stop it.  However, if we get past the verifier,
     // this will stop the thread in a reasonable way, without crashing the JVM.
     __ call_VM(noreg, CAST_FROM_FN_PTR(address,
                      InterpreterRuntime::throw_IncompatibleClassChangeError));
     // the call_VM checks for exception, so we should never return here.
     __ should_not_reach_here();
     return;
   }

   //const Register Rmethod   = T2;
   const Register T2_callsite = T2;

   prepare_invoke(byte_no, Rmethod, T2_callsite);

   // T2: CallSite object (from cpool->resolved_references[f1])
   // Rmethod: MH.linkToCallSite method (from f2)

   // Note:  T2_callsite is already pushed by prepare_invoke
   // %%% should make a type profile for any invokedynamic that takes a ref argument
   // profile this call
   __ profile_call(T9);

   // T8: tmp, used for mdp
   // Rmethod: callee
   // T9: tmp
   // is_virtual: false
   __ profile_arguments_type(T8, Rmethod, T9, false);

   __ verify_oop(T2_callsite);

   __ jump_from_interpreted(Rmethod, T9);
 }

//-----------------------------------------------------------------------------
// Allocation
// T1 : tags & buffer end & thread
// T2 : object end
// T3 : klass
// T1 : object size
// A1 : cpool
// A2 : cp index
// return object in FSR
void TemplateTable::_new() {
  transition(vtos, atos);
  __ get_unsigned_2_byte_index_at_bcp(A2, 1);

  Label slow_case;
  Label done;
  Label initialize_header;
  Label initialize_object; // including clearing the fields
  Label allocate_shared;

  // get InstanceKlass in T3
  __ get_cpool_and_tags(A1, T1);

  __ dsll(AT, A2, Address::times_8);
  if (UseLEXT1 && Assembler::is_simm(sizeof(ConstantPool), 8)) {
    __ gsldx(T3, A1, AT, sizeof(ConstantPool));
  } else {
    __ daddu(AT, A1, AT);
    __ ld(T3, AT, sizeof(ConstantPool));
  }

  // make sure the class we're about to instantiate has been resolved.
  // Note: slow_case does a pop of stack, which is why we loaded class/pushed above
  const int tags_offset = Array<u1>::base_offset_in_bytes();
  if (UseLEXT1 && Assembler::is_simm(tags_offset, 8)) {
    __ gslbx(AT, T1, A2, tags_offset);
  } else {
    __ daddu(T1, T1, A2);
    __ lb(AT, T1, tags_offset);
  }
  if(os::is_MP()) {
    __ sync(); // load acquire
  }
  __ daddiu(AT, AT, - (int)JVM_CONSTANT_Class);
  __ bne(AT, R0, slow_case);
  __ delayed()->nop();


  // make sure klass is initialized & doesn't have finalizer
  // make sure klass is fully initialized
  __ lhu(T1, T3, in_bytes(InstanceKlass::init_state_offset()));
  __ daddiu(AT, T1, - (int)InstanceKlass::fully_initialized);
  __ bne(AT, R0, slow_case);
  __ delayed()->nop();

  // has_finalizer
  __ lw(T0, T3, in_bytes(Klass::layout_helper_offset()) );
  __ andi(AT, T0, Klass::_lh_instance_slow_path_bit);
  __ bne(AT, R0, slow_case);
  __ delayed()->nop();

  // Allocate the instance
  // 1) Try to allocate in the TLAB
  // 2) if fail and the object is large allocate in the shared Eden
  // 3) if the above fails (or is not applicable), go to a slow case
  // (creates a new TLAB, etc.)

  const bool allow_shared_alloc =
    Universe::heap()->supports_inline_contig_alloc() && !CMSIncrementalMode;

#ifndef OPT_THREAD
    const Register thread = T8;
    if (UseTLAB || allow_shared_alloc) {
      __ get_thread(thread);
    }
#else
    const Register thread = TREG;
#endif

  if (UseTLAB) {
    // get tlab_top
    __ ld(FSR, thread, in_bytes(JavaThread::tlab_top_offset()));
    // get tlab_end
    __ ld(AT, thread, in_bytes(JavaThread::tlab_end_offset()));
    __ daddu(T2, FSR, T0);
    __ slt(AT, AT, T2);
    __ bne(AT, R0, allow_shared_alloc ? allocate_shared : slow_case);
    __ delayed()->nop();
    __ sd(T2, thread, in_bytes(JavaThread::tlab_top_offset()));

    if (ZeroTLAB) {
      // the fields have been already cleared
      __ beq(R0, R0, initialize_header);
    } else {
      // initialize both the header and fields
      __ beq(R0, R0, initialize_object);
    }
    __ delayed()->nop();
  }

  // Allocation in the shared Eden , if allowed
  // T0 : instance size in words
  if(allow_shared_alloc){
    __ bind(allocate_shared);

    Label retry;
    Address heap_top(T1);
    __ set64(T1, (long)Universe::heap()->top_addr());
    __ ld(FSR, heap_top);

    __ bind(retry);
    __ set64(AT, (long)Universe::heap()->end_addr());
    __ ld(AT, AT, 0);
    __ daddu(T2, FSR, T0);
    __ slt(AT, AT, T2);
    __ bne(AT, R0, slow_case);
    __ delayed()->nop();

    // Compare FSR with the top addr, and if still equal, store the new
    // top addr in T2 at the address of the top addr pointer. Sets AT if was
    // equal, and clears it otherwise. Use lock prefix for atomicity on MPs.
    //
    // FSR: object begin
    // T2: object end
    // T0: instance size in words

    // if someone beat us on the allocation, try again, otherwise continue
    __ cmpxchg(T2, heap_top, FSR);
    __ beq(AT, R0, retry);
    __ delayed()->nop();

    __ incr_allocated_bytes(thread, T0, 0);
  }

  if (UseTLAB || Universe::heap()->supports_inline_contig_alloc()) {
    // The object is initialized before the header.  If the object size is
    // zero, go directly to the header initialization.
    __ bind(initialize_object);
    __ set64(AT, - sizeof(oopDesc));
    __ daddu(T0, T0, AT);
    __ beq(T0, R0, initialize_header);
    __ delayed()->nop();

    // initialize remaining object fields: T0 is a multiple of 2
    {
      Label loop;
      __ daddu(T1, FSR, T0);
      __ daddiu(T1, T1, -oopSize);

      __ bind(loop);
      __ sd(R0, T1, sizeof(oopDesc) + 0 * oopSize);
      __ bne(T1, FSR, loop); //dont clear header
      __ delayed()->daddiu(T1, T1, -oopSize);
    }

    //klass in T3,
    // initialize object header only.
    __ bind(initialize_header);
    if (UseBiasedLocking) {
      __ ld(AT, T3, in_bytes(Klass::prototype_header_offset()));
      __ sd(AT, FSR, oopDesc::mark_offset_in_bytes ());
    } else {
      __ set64(AT, (long)markOopDesc::prototype());
      __ sd(AT, FSR, oopDesc::mark_offset_in_bytes());
    }

    __ store_klass_gap(FSR, R0);
    __ store_klass(FSR, T3);

    {
      SkipIfEqual skip_if(_masm, &DTraceAllocProbes, 0);
      // Trigger dtrace event for fastpath
      __ push(atos);
      __ call_VM_leaf(
           CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_object_alloc), FSR);
      __ pop(atos);

    }
    __ b(done);
    __ delayed()->nop();
  }

  // slow case
  __ bind(slow_case);
  call_VM(FSR, CAST_FROM_FN_PTR(address, InterpreterRuntime::_new), A1, A2);

  // continue
  __ bind(done);
  __ sync();
}

void TemplateTable::newarray() {
  transition(itos, atos);
  __ lbu(A1, at_bcp(1));
  //type, count
  call_VM(FSR, CAST_FROM_FN_PTR(address, InterpreterRuntime::newarray), A1, FSR);
  __ sync();
}

void TemplateTable::anewarray() {
  transition(itos, atos);
  __ get_2_byte_integer_at_bcp(A2, AT, 1);
  __ huswap(A2);
  __ get_constant_pool(A1);
  // cp, index, count
  call_VM(FSR, CAST_FROM_FN_PTR(address, InterpreterRuntime::anewarray), A1, A2, FSR);
  __ sync();
}

void TemplateTable::arraylength() {
  transition(atos, itos);
  __ null_check(FSR, arrayOopDesc::length_offset_in_bytes());
  __ lw(FSR, FSR, arrayOopDesc::length_offset_in_bytes());
}

// when invoke gen_subtype_check, super in T3, sub in T2, object in FSR(it's always)
// T2 : sub klass
// T3 : cpool
// T3 : super klass
void TemplateTable::checkcast() {
  transition(atos, atos);
  Label done, is_null, ok_is_subtype, quicked, resolved;
  __ beq(FSR, R0, is_null);
  __ delayed()->nop();

  // Get cpool & tags index
  __ get_cpool_and_tags(T3, T1);
  __ get_2_byte_integer_at_bcp(T2, AT, 1);
  __ huswap(T2);

  // See if bytecode has already been quicked
  __ daddu(AT, T1, T2);
  __ lb(AT, AT, Array<u1>::base_offset_in_bytes());
  if(os::is_MP()) {
    __ sync(); // load acquire
  }
  __ daddiu(AT, AT, - (int)JVM_CONSTANT_Class);
  __ beq(AT, R0, quicked);
  __ delayed()->nop();

  // In InterpreterRuntime::quicken_io_cc, lots of new classes may be loaded.
  // Then, GC will move the object in V0 to another places in heap.
  // Therefore, We should never save such an object in register.
  // Instead, we should save it in the stack. It can be modified automatically by the GC thread.
  // After GC, the object address in FSR is changed to a new place.
  //
  __ push(atos);
  const Register thread = TREG;
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  call_VM(NOREG, CAST_FROM_FN_PTR(address, InterpreterRuntime::quicken_io_cc));
  __ get_vm_result_2(T3, thread);
  __ pop_ptr(FSR);
  __ b(resolved);
  __ delayed()->nop();

  // klass already in cp, get superklass in T3
  __ bind(quicked);
  __ dsll(AT, T2, Address::times_8);
  __ daddu(AT, T3, AT);
  __ ld(T3, AT, sizeof(ConstantPool));

  __ bind(resolved);

  // get subklass in T2
  //add for compressedoops
  __ load_klass(T2, FSR);
  // Superklass in T3.  Subklass in T2.
  __ gen_subtype_check(T3, T2, ok_is_subtype);

  // Come here on failure
  // object is at FSR
  __ jmp(Interpreter::_throw_ClassCastException_entry);
  __ delayed()->nop();

  // Come here on success
  __ bind(ok_is_subtype);

  // Collect counts on whether this check-cast sees NULLs a lot or not.
  if (ProfileInterpreter) {
    __ b(done);
    __ delayed()->nop();
    __ bind(is_null);
    __ profile_null_seen(T3);
  } else {
    __ bind(is_null);
  }
  __ bind(done);
}

// i use T3 as cpool, T1 as tags, T2 as index
// object always in FSR, superklass in T3, subklass in T2
void TemplateTable::instanceof() {
  transition(atos, itos);
  Label done, is_null, ok_is_subtype, quicked, resolved;

  __ beq(FSR, R0, is_null);
  __ delayed()->nop();

  // Get cpool & tags index
  __ get_cpool_and_tags(T3, T1);
  // get index
  __ get_2_byte_integer_at_bcp(T2, AT, 1);
  __ huswap(T2);

  // See if bytecode has already been quicked
  // quicked
  __ daddu(AT, T1, T2);
  __ lb(AT, AT, Array<u1>::base_offset_in_bytes());
  if(os::is_MP()) {
    __ sync(); // load acquire
  }
  __ daddiu(AT, AT, - (int)JVM_CONSTANT_Class);
  __ beq(AT, R0, quicked);
  __ delayed()->nop();

  __ push(atos);
  const Register thread = TREG;
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  call_VM(NOREG, CAST_FROM_FN_PTR(address, InterpreterRuntime::quicken_io_cc));
  __ get_vm_result_2(T3, thread);
  __ pop_ptr(FSR);
  __ b(resolved);
  __ delayed()->nop();

  // get superklass in T3, subklass in T2
  __ bind(quicked);
  __ dsll(AT, T2, Address::times_8);
  __ daddu(AT, T3, AT);
  __ ld(T3, AT, sizeof(ConstantPool));

  __ bind(resolved);
  // get subklass in T2
  //add for compressedoops
  __ load_klass(T2, FSR);

  // Superklass in T3.  Subklass in T2.
  __ gen_subtype_check(T3, T2, ok_is_subtype);
  // Come here on failure
  __ b(done);
  __ delayed(); __ move(FSR, R0);

  // Come here on success
  __ bind(ok_is_subtype);
  __ move(FSR, 1);

  // Collect counts on whether this test sees NULLs a lot or not.
  if (ProfileInterpreter) {
    __ beq(R0, R0, done);
    __ delayed()->nop();
    __ bind(is_null);
    __ profile_null_seen(T3);
  } else {
    __ bind(is_null);   // same as 'done'
  }
  __ bind(done);
  // FSR = 0: obj == NULL or  obj is not an instanceof the specified klass
  // FSR = 1: obj != NULL and obj is     an instanceof the specified klass
}

//--------------------------------------------------------
//--------------------------------------------
// Breakpoints
void TemplateTable::_breakpoint() {
  // Note: We get here even if we are single stepping..
  // jbug inists on setting breakpoints at every bytecode
  // even if we are in single step mode.

  transition(vtos, vtos);

  // get the unpatched byte code
  __ get_method(A1);
  __ call_VM(NOREG,
             CAST_FROM_FN_PTR(address,
                              InterpreterRuntime::get_original_bytecode_at),
             A1, BCP);
  __ move(Rnext, V0); // Rnext will be used in dispatch_only_normal

  // post the breakpoint event
  __ get_method(A1);
  __ call_VM(NOREG, CAST_FROM_FN_PTR(address, InterpreterRuntime::_breakpoint), A1, BCP);

  // complete the execution of original bytecode
  __ dispatch_only_normal(vtos);
}

//-----------------------------------------------------------------------------
// Exceptions

void TemplateTable::athrow() {
  transition(atos, vtos);
  __ null_check(FSR);
  __ jmp(Interpreter::throw_exception_entry());
  __ delayed()->nop();
}

//-----------------------------------------------------------------------------
// Synchronization
//
// Note: monitorenter & exit are symmetric routines; which is reflected
//       in the assembly code structure as well
//
// Stack layout:
//
// [expressions  ] <--- SP               = expression stack top
// ..
// [expressions  ]
// [monitor entry] <--- monitor block top = expression stack bot
// ..
// [monitor entry]
// [frame data   ] <--- monitor block bot
// ...
// [return addr  ] <--- FP

// we use T2 as monitor entry pointer, T3 as monitor top pointer, c_rarg0 as free slot pointer
// object always in FSR
void TemplateTable::monitorenter() {
  transition(atos, vtos);

  // check for NULL object
  __ null_check(FSR);

  const Address monitor_block_top(FP, frame::interpreter_frame_monitor_block_top_offset
      * wordSize);
  const int entry_size = (frame::interpreter_frame_monitor_size()* wordSize);
  Label allocated;

  // initialize entry pointer
  __ move(c_rarg0, R0);

  // find a free slot in the monitor block (result in c_rarg0)
  {
    Label entry, loop, exit, next;
    __ ld(T2, monitor_block_top);
    __ b(entry);
    __ delayed()->daddiu(T3, FP, frame::interpreter_frame_initial_sp_offset * wordSize);

    // free slot?
    __ bind(loop);
    __ ld(AT, T2, BasicObjectLock::obj_offset_in_bytes());
    __ bne(AT, R0, next);
    __ delayed()->nop();
    __ move(c_rarg0, T2);

    __ bind(next);
    __ beq(FSR, AT, exit);
    __ delayed()->nop();
    __ daddiu(T2, T2, entry_size);

    __ bind(entry);
    __ bne(T3, T2, loop);
    __ delayed()->nop();
    __ bind(exit);
  }

  __ bne(c_rarg0, R0, allocated);
  __ delayed()->nop();

  // allocate one if there's no free slot
  {
    Label entry, loop;
    // 1. compute new pointers                   // SP: old expression stack top
    __ ld(c_rarg0, monitor_block_top);
    __ daddiu(SP, SP, - entry_size);
    __ daddiu(c_rarg0, c_rarg0, - entry_size);
    __ sd(c_rarg0, monitor_block_top);
    __ b(entry);
    __ delayed(); __ move(T3, SP);

    // 2. move expression stack contents
    __ bind(loop);
    __ ld(AT, T3, entry_size);
    __ sd(AT, T3, 0);
    __ daddiu(T3, T3, wordSize);
    __ bind(entry);
    __ bne(T3, c_rarg0, loop);
    __ delayed()->nop();
  }

  __ bind(allocated);
  // Increment bcp to point to the next bytecode,
  // so exception handling for async. exceptions work correctly.
  // The object has already been poped from the stack, so the
  // expression stack looks correct.
  __ daddiu(BCP, BCP, 1);
  __ sd(FSR, c_rarg0, BasicObjectLock::obj_offset_in_bytes());
  __ lock_object(c_rarg0);
  // check to make sure this monitor doesn't cause stack overflow after locking
  __ save_bcp();  // in case of exception
  __ generate_stack_overflow_check(0);
  // The bcp has already been incremented. Just need to dispatch to next instruction.

  __ dispatch_next(vtos);
}

// T2 : top
// c_rarg0 : entry
void TemplateTable::monitorexit() {
  transition(atos, vtos);

  __ null_check(FSR);

  const int entry_size =(frame::interpreter_frame_monitor_size()* wordSize);
  Label found;

  // find matching slot
  {
    Label entry, loop;
    __ ld(c_rarg0, FP, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    __ b(entry);
    __ delayed()->daddiu(T2, FP, frame::interpreter_frame_initial_sp_offset * wordSize);

    __ bind(loop);
    __ ld(AT, c_rarg0, BasicObjectLock::obj_offset_in_bytes());
    __ beq(FSR, AT, found);
    __ delayed()->nop();
    __ daddiu(c_rarg0, c_rarg0, entry_size);
    __ bind(entry);
    __ bne(T2, c_rarg0, loop);
    __ delayed()->nop();
  }

  // error handling. Unlocking was not block-structured
  Label end;
  __ call_VM(NOREG, CAST_FROM_FN_PTR(address,
  InterpreterRuntime::throw_illegal_monitor_state_exception));
  __ should_not_reach_here();

  // call run-time routine
  // c_rarg0: points to monitor entry
  __ bind(found);
  __ move(TSR, FSR);
  __ unlock_object(c_rarg0);
  __ move(FSR, TSR);
  __ bind(end);
}


// Wide instructions
void TemplateTable::wide() {
  transition(vtos, vtos);
  __ lbu(Rnext, at_bcp(1));
  __ dsll(T9, Rnext, Address::times_8);
  __ li(AT, (long)Interpreter::_wentry_point);
  __ daddu(AT, T9, AT);
  __ ld(T9, AT, 0);
  __ jr(T9);
  __ delayed()->nop();
}


void TemplateTable::multianewarray() {
  transition(vtos, atos);
  // last dim is on top of stack; we want address of first one:
  // first_addr = last_addr + (ndims - 1) * wordSize
  __ lbu(A1, at_bcp(3));  // dimension
  __ daddiu(A1, A1, -1);
  __ dsll(A1, A1, Address::times_8);
  __ daddu(A1, SP, A1);    // now A1 pointer to the count array on the stack
  call_VM(FSR, CAST_FROM_FN_PTR(address, InterpreterRuntime::multianewarray), A1);
  __ lbu(AT, at_bcp(3));
  __ dsll(AT, AT, Address::times_8);
  __ daddu(SP, SP, AT);
  __ sync();
}
#endif // !CC_INTERP
