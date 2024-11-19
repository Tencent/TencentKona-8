/*
 * Copyright (c) 2005, 2021, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, 2024, Loongson Technology. All rights reserved.
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
#include "c1/c1_Compilation.hpp"
#include "c1/c1_FrameMap.hpp"
#include "c1/c1_Instruction.hpp"
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_LIRGenerator.hpp"
#include "c1/c1_Runtime1.hpp"
#include "c1/c1_ValueStack.hpp"
#include "ci/ciArray.hpp"
#include "ci/ciObjArrayKlass.hpp"
#include "ci/ciTypeArrayKlass.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "vmreg_loongarch.inline.hpp"

#ifdef ASSERT
#define __ gen()->lir(__FILE__, __LINE__)->
#else
#define __ gen()->lir()->
#endif

// Item will be loaded into a byte register; Intel only
void LIRItem::load_byte_item() {
  load_item();
}

void LIRItem::load_nonconstant() {
  LIR_Opr r = value()->operand();
  if (r->is_constant()) {
    _result = r;
  } else {
    load_item();
  }
}

//--------------------------------------------------------------
//               LIRGenerator
//--------------------------------------------------------------

LIR_Opr LIRGenerator::exceptionOopOpr() { return FrameMap::a0_oop_opr; }
LIR_Opr LIRGenerator::exceptionPcOpr()  { return FrameMap::a1_opr; }
LIR_Opr LIRGenerator::divInOpr()        { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::divOutOpr()       { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::remOutOpr()       { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::shiftCountOpr()   { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::syncTempOpr()     { return FrameMap::a0_opr; }
LIR_Opr LIRGenerator::getThreadTemp()   { return LIR_OprFact::illegalOpr; }

LIR_Opr LIRGenerator::result_register_for(ValueType* type, bool callee) {
  LIR_Opr opr;
  switch (type->tag()) {
    case intTag:    opr = FrameMap::a0_opr;          break;
    case objectTag: opr = FrameMap::a0_oop_opr;      break;
    case longTag:   opr = FrameMap::long0_opr;       break;
    case floatTag:  opr = FrameMap::fpu0_float_opr;  break;
    case doubleTag: opr = FrameMap::fpu0_double_opr; break;
    case addressTag:
    default: ShouldNotReachHere(); return LIR_OprFact::illegalOpr;
  }

  assert(opr->type_field() == as_OprType(as_BasicType(type)), "type mismatch");
  return opr;
}

LIR_Opr LIRGenerator::rlock_byte(BasicType type) {
  LIR_Opr reg = new_register(T_INT);
  set_vreg_flag(reg, LIRGenerator::byte_reg);
  return reg;
}

//--------- loading items into registers --------------------------------

bool LIRGenerator::can_store_as_constant(Value v, BasicType type) const {
  if (v->type()->as_IntConstant() != NULL) {
    return v->type()->as_IntConstant()->value() == 0L;
  } else if (v->type()->as_LongConstant() != NULL) {
    return v->type()->as_LongConstant()->value() == 0L;
  } else if (v->type()->as_ObjectConstant() != NULL) {
    return v->type()->as_ObjectConstant()->value()->is_null_object();
  } else {
    return false;
  }
}

bool LIRGenerator::can_inline_as_constant(Value v) const {
  // FIXME: Just a guess
  if (v->type()->as_IntConstant() != NULL) {
    return Assembler::is_simm(v->type()->as_IntConstant()->value(), 12);
  } else if (v->type()->as_LongConstant() != NULL) {
    return v->type()->as_LongConstant()->value() == 0L;
  } else if (v->type()->as_ObjectConstant() != NULL) {
    return v->type()->as_ObjectConstant()->value()->is_null_object();
  } else {
    return false;
  }
}

bool LIRGenerator::can_inline_as_constant(LIR_Const* c) const { return false; }

LIR_Opr LIRGenerator::safepoint_poll_register() {
  return LIR_OprFact::illegalOpr;
}

LIR_Address* LIRGenerator::generate_address(LIR_Opr base, LIR_Opr index,
                                            int shift, int disp, BasicType type) {
  assert(base->is_register(), "must be");
  intx large_disp = disp;

  // accumulate fixed displacements
  if (index->is_constant()) {
    LIR_Const *constant = index->as_constant_ptr();
    if (constant->type() == T_INT) {
      large_disp += index->as_jint() << shift;
    } else {
      assert(constant->type() == T_LONG, "should be");
      jlong c = index->as_jlong() << shift;
      if ((jlong)((jint)c) == c) {
        large_disp += c;
        index = LIR_OprFact::illegalOpr;
      } else {
        LIR_Opr tmp = new_register(T_LONG);
        __ move(index, tmp);
        index = tmp;
        // apply shift and displacement below
      }
    }
  }

  if (index->is_register()) {
    // apply the shift and accumulate the displacement
    if (shift > 0) {
      // Use long register to avoid overflow when shifting large index values left.
      LIR_Opr tmp = new_register(T_LONG);
      __ convert(Bytecodes::_i2l, index, tmp);
      __ shift_left(tmp, shift, tmp);
      index = tmp;
    }
    if (large_disp != 0) {
      LIR_Opr tmp = new_pointer_register();
      if (Assembler::is_simm(large_disp, 12)) {
        __ add(index, LIR_OprFact::intptrConst(large_disp), tmp);
        index = tmp;
      } else {
        __ move(LIR_OprFact::intptrConst(large_disp), tmp);
        __ add(tmp, index, tmp);
        index = tmp;
      }
      large_disp = 0;
    }
  } else if (large_disp != 0 && !Assembler::is_simm(large_disp, 12)) {
    // index is illegal so replace it with the displacement loaded into a register
    index = new_pointer_register();
    __ move(LIR_OprFact::intptrConst(large_disp), index);
    large_disp = 0;
  }

  // at this point we either have base + index or base + displacement
  if (large_disp == 0 && index->is_register()) {
    return new LIR_Address(base, index, type);
  } else {
    assert(Assembler::is_simm(large_disp, 12), "must be");
    return new LIR_Address(base, large_disp, type);
  }
}

LIR_Address* LIRGenerator::emit_array_address(LIR_Opr array_opr, LIR_Opr index_opr, BasicType type, bool needs_card_mark) {
  int offset_in_bytes = arrayOopDesc::base_offset_in_bytes(type);
  int elem_size = type2aelembytes(type);
  int shift = exact_log2(elem_size);

  LIR_Address* addr;
  if (index_opr->is_constant()) {
    addr = new LIR_Address(array_opr, offset_in_bytes + (intx)(index_opr->as_jint()) * elem_size, type);
  } else {
    if (offset_in_bytes) {
      LIR_Opr tmp = new_pointer_register();
      __ add(array_opr, LIR_OprFact::intConst(offset_in_bytes), tmp);
      array_opr = tmp;
      offset_in_bytes = 0;
    }
    addr =  new LIR_Address(array_opr, index_opr, LIR_Address::scale(type), offset_in_bytes, type);
  }
  if (needs_card_mark) {
    // This store will need a precise card mark, so go ahead and
    // compute the full adddres instead of computing once for the
    // store and again for the card mark.
    LIR_Opr tmp = new_pointer_register();
    __ leal(LIR_OprFact::address(addr), tmp);
    return new LIR_Address(tmp, type);
  } else {
    return addr;
  }
}

LIR_Opr LIRGenerator::load_immediate(int x, BasicType type) {
  LIR_Opr r;
  if (type == T_LONG) {
    r = LIR_OprFact::longConst(x);
    if (!Assembler::is_simm(x, 12)) {
      LIR_Opr tmp = new_register(type);
      __ move(r, tmp);
      return tmp;
    }
  } else if (type == T_INT) {
    r = LIR_OprFact::intConst(x);
    if (!Assembler::is_simm(x, 12)) {
      // This is all rather nasty.  We don't know whether our constant
      // is required for a logical or an arithmetic operation, wo we
      // don't know what the range of valid values is!!
      LIR_Opr tmp = new_register(type);
      __ move(r, tmp);
      return tmp;
    }
  } else {
    ShouldNotReachHere();
    r = NULL;  // unreachable
  }
  return r;
}

void LIRGenerator::increment_counter(address counter, BasicType type, int step) {
  LIR_Opr pointer = new_pointer_register();
  __ move(LIR_OprFact::intptrConst(counter), pointer);
  LIR_Address* addr = new LIR_Address(pointer, type);
  increment_counter(addr, step);
}

void LIRGenerator::increment_counter(LIR_Address* addr, int step) {
  LIR_Opr imm = NULL;
  switch(addr->type()) {
  case T_INT:
    imm = LIR_OprFact::intConst(step);
    break;
  case T_LONG:
    imm = LIR_OprFact::longConst(step);
    break;
  default:
    ShouldNotReachHere();
  }
  LIR_Opr reg = new_register(addr->type());
  __ load(addr, reg);
  __ add(reg, imm, reg);
  __ store(reg, addr);
}

template<typename T>
void LIRGenerator::cmp_mem_int_branch(LIR_Condition condition, LIR_Opr base,
                                      int disp, int c, T tgt, CodeEmitInfo* info) {
  LIR_Opr reg = new_register(T_INT);
  __ load(generate_address(base, disp, T_INT), reg, info);
  __ cmp_branch(condition, reg, LIR_OprFact::intConst(c), T_INT, tgt);
}

// Explicit instantiation for all supported types.
template void LIRGenerator::cmp_mem_int_branch(LIR_Condition, LIR_Opr, int, int, Label*, CodeEmitInfo*);
template void LIRGenerator::cmp_mem_int_branch(LIR_Condition, LIR_Opr, int, int, BlockBegin*, CodeEmitInfo*);
template void LIRGenerator::cmp_mem_int_branch(LIR_Condition, LIR_Opr, int, int, CodeStub*, CodeEmitInfo*);

template<typename T>
void LIRGenerator::cmp_reg_mem_branch(LIR_Condition condition, LIR_Opr reg, LIR_Opr base,
                                      int disp, BasicType type, T tgt, CodeEmitInfo* info) {
  LIR_Opr reg1 = new_register(T_INT);
  __ load(generate_address(base, disp, type), reg1, info);
  __ cmp_branch(condition, reg, reg1, type, tgt);
}

// Explicit instantiation for all supported types.
template void LIRGenerator::cmp_reg_mem_branch(LIR_Condition, LIR_Opr, LIR_Opr, int, BasicType, Label*, CodeEmitInfo*);
template void LIRGenerator::cmp_reg_mem_branch(LIR_Condition, LIR_Opr, LIR_Opr, int, BasicType, BlockBegin*, CodeEmitInfo*);
template void LIRGenerator::cmp_reg_mem_branch(LIR_Condition, LIR_Opr, LIR_Opr, int, BasicType, CodeStub*, CodeEmitInfo*);

bool LIRGenerator::strength_reduce_multiply(LIR_Opr left, jint c, LIR_Opr result, LIR_Opr tmp) {
  if (is_power_of_2(c - 1)) {
    __ shift_left(left, exact_log2(c - 1), tmp);
    __ add(tmp, left, result);
    return true;
  } else if (is_power_of_2(c + 1)) {
    __ shift_left(left, exact_log2(c + 1), tmp);
    __ sub(tmp, left, result);
    return true;
  } else {
    return false;
  }
}

void LIRGenerator::store_stack_parameter (LIR_Opr item, ByteSize offset_from_sp) {
  BasicType type = item->type();
  __ store(item, new LIR_Address(FrameMap::sp_opr, in_bytes(offset_from_sp), type));
}

//----------------------------------------------------------------------
//             visitor functions
//----------------------------------------------------------------------

void LIRGenerator::do_StoreIndexed(StoreIndexed* x) {
  assert(x->is_pinned(),"");
  bool needs_range_check = x->compute_needs_range_check();
  bool use_length = x->length() != NULL;
  bool obj_store = x->elt_type() == T_ARRAY || x->elt_type() == T_OBJECT;
  bool needs_store_check = obj_store && (x->value()->as_Constant() == NULL ||
                                         !get_jobject_constant(x->value())->is_null_object() ||
                                         x->should_profile());

  LIRItem array(x->array(), this);
  LIRItem index(x->index(), this);
  LIRItem value(x->value(), this);
  LIRItem length(this);

  array.load_item();
  index.load_nonconstant();

  if (use_length && needs_range_check) {
    length.set_instruction(x->length());
    length.load_item();

  }
  if (needs_store_check || x->check_boolean()) {
    value.load_item();
  } else {
    value.load_for_store(x->elt_type());
  }

  set_no_result(x);

  // the CodeEmitInfo must be duplicated for each different
  // LIR-instruction because spilling can occur anywhere between two
  // instructions and so the debug information must be different
  CodeEmitInfo* range_check_info = state_for(x);
  CodeEmitInfo* null_check_info = NULL;
  if (x->needs_null_check()) {
    null_check_info = new CodeEmitInfo(range_check_info);
  }

  // emit array address setup early so it schedules better
  // FIXME?  No harm in this on aarch64, and it might help
  LIR_Address* array_addr = emit_array_address(array.result(), index.result(), x->elt_type(), obj_store);

  if (GenerateRangeChecks && needs_range_check) {
    if (use_length) {
      __ cmp_branch(lir_cond_belowEqual, length.result(), index.result(), x->elt_type(), new RangeCheckStub(range_check_info, index.result()));
    } else {
      array_range_check(array.result(), index.result(), null_check_info, range_check_info);
      // range_check also does the null check
      null_check_info = NULL;
    }
  }

  if (GenerateArrayStoreCheck && needs_store_check) {
    LIR_Opr tmp1 = new_register(objectType);
    LIR_Opr tmp2 = new_register(objectType);
    LIR_Opr tmp3 = new_register(objectType);

    CodeEmitInfo* store_check_info = new CodeEmitInfo(range_check_info);
    __ store_check(value.result(), array.result(), tmp1, tmp2, tmp3, store_check_info, x->profiled_method(), x->profiled_bci());
  }

  if (obj_store) {
    // Needs GC write barriers.
    pre_barrier(LIR_OprFact::address(array_addr), LIR_OprFact::illegalOpr /* pre_val */,
                true /* do_load */, false /* patch */, NULL);
    __ move(value.result(), array_addr, null_check_info);
    // Seems to be a precise
    post_barrier(LIR_OprFact::address(array_addr), value.result());
  } else {
    LIR_Opr result = maybe_mask_boolean(x, array.result(), value.result(), null_check_info);
    __ move(result, array_addr, null_check_info);
  }
}

void LIRGenerator::do_MonitorEnter(MonitorEnter* x) {
  assert(x->is_pinned(),"");
  LIRItem obj(x->obj(), this);
  obj.load_item();

  set_no_result(x);

  // "lock" stores the address of the monitor stack slot, so this is not an oop
  LIR_Opr lock = new_register(T_INT);
  // Need a scratch register for biased locking
  LIR_Opr scratch = LIR_OprFact::illegalOpr;
  if (UseBiasedLocking) {
    scratch = new_register(T_INT);
  }

  CodeEmitInfo* info_for_exception = NULL;
  if (x->needs_null_check()) {
    info_for_exception = state_for(x);
  }
  // this CodeEmitInfo must not have the xhandlers because here the
  // object is already locked (xhandlers expect object to be unlocked)
  CodeEmitInfo* info = state_for(x, x->state(), true);
  monitor_enter(obj.result(), lock, syncTempOpr(), scratch,
                x->monitor_no(), info_for_exception, info);
}

void LIRGenerator::do_MonitorExit(MonitorExit* x) {
  assert(x->is_pinned(),"");

  LIRItem obj(x->obj(), this);
  obj.dont_load_item();

  LIR_Opr lock = new_register(T_INT);
  LIR_Opr obj_temp = new_register(T_INT);
  set_no_result(x);
  monitor_exit(obj_temp, lock, syncTempOpr(), LIR_OprFact::illegalOpr, x->monitor_no());
}

void LIRGenerator::do_NegateOp(NegateOp* x) {
  LIRItem from(x->x(), this);
  from.load_item();
  LIR_Opr result = rlock_result(x);
  __ negate (from.result(), result);
}

// for  _fadd, _fmul, _fsub, _fdiv, _frem
//      _dadd, _dmul, _dsub, _ddiv, _drem
void LIRGenerator::do_ArithmeticOp_FPU(ArithmeticOp* x) {
  if (x->op() == Bytecodes::_frem || x->op() == Bytecodes::_drem) {
    // float remainder is implemented as a direct call into the runtime
    LIRItem right(x->x(), this);
    LIRItem left(x->y(), this);

    BasicTypeList signature(2);
    if (x->op() == Bytecodes::_frem) {
      signature.append(T_FLOAT);
      signature.append(T_FLOAT);
    } else {
      signature.append(T_DOUBLE);
      signature.append(T_DOUBLE);
    }
    CallingConvention* cc = frame_map()->c_calling_convention(&signature);

    const LIR_Opr result_reg = result_register_for(x->type());
    left.load_item_force(cc->at(1));
    right.load_item();

    __ move(right.result(), cc->at(0));

    address entry;
    if (x->op() == Bytecodes::_frem) {
      entry = CAST_FROM_FN_PTR(address, SharedRuntime::frem);
    } else {
      entry = CAST_FROM_FN_PTR(address, SharedRuntime::drem);
    }

    LIR_Opr result = rlock_result(x);
    __ call_runtime_leaf(entry, getThreadTemp(), result_reg, cc->args());
    __ move(result_reg, result);
    return;
  }

  LIRItem left(x->x(),  this);
  LIRItem right(x->y(), this);
  LIRItem* left_arg  = &left;
  LIRItem* right_arg = &right;

  // Always load right hand side.
  right.load_item();

  if (!left.is_register())
    left.load_item();

  LIR_Opr reg = rlock(x);

  arithmetic_op_fpu(x->op(), reg, left.result(), right.result(), x->is_strictfp());

  set_result(x, round_item(reg));
}

// for  _ladd, _lmul, _lsub, _ldiv, _lrem
void LIRGenerator::do_ArithmeticOp_Long(ArithmeticOp* x) {
  // missing test if instr is commutative and if we should swap
  LIRItem left(x->x(), this);
  LIRItem right(x->y(), this);

  if (x->op() == Bytecodes::_ldiv || x->op() == Bytecodes::_lrem) {
    left.load_item();
    bool need_zero_check = true;
    if (right.is_constant()) {
      jlong c = right.get_jlong_constant();
      // no need to do div-by-zero check if the divisor is a non-zero constant
      if (c != 0) need_zero_check = false;
      // do not load right if the divisor is a power-of-2 constant
      if (c > 0 && is_power_of_2(c) && Assembler::is_uimm(c - 1, 12)) {
        right.dont_load_item();
      } else {
        right.load_item();
      }
    } else {
      right.load_item();
    }
    if (need_zero_check) {
      CodeEmitInfo* info = state_for(x);
      CodeStub* stub = new DivByZeroStub(info);
      __ cmp_branch(lir_cond_equal, right.result(), LIR_OprFact::longConst(0), T_LONG, stub);
    }

    rlock_result(x);
    switch (x->op()) {
    case Bytecodes::_lrem:
      __ rem (left.result(), right.result(), x->operand());
      break;
    case Bytecodes::_ldiv:
      __ div (left.result(), right.result(), x->operand());
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  } else {
    assert(x->op() == Bytecodes::_lmul || x->op() == Bytecodes::_ladd || x->op() == Bytecodes::_lsub,
           "expect lmul, ladd or lsub");
    // add, sub, mul
    left.load_item();
    if (!right.is_register()) {
      if (x->op() == Bytecodes::_lmul || !right.is_constant() ||
          (x->op() == Bytecodes::_ladd && !Assembler::is_simm(right.get_jlong_constant(), 12)) ||
          (x->op() == Bytecodes::_lsub && !Assembler::is_simm(-right.get_jlong_constant(), 12))) {
        right.load_item();
      } else { // add, sub
        assert(x->op() == Bytecodes::_ladd || x->op() == Bytecodes::_lsub, "expect ladd or lsub");
        // don't load constants to save register
        right.load_nonconstant();
      }
    }
    rlock_result(x);
    arithmetic_op_long(x->op(), x->operand(), left.result(), right.result(), NULL);
  }
}

// for: _iadd, _imul, _isub, _idiv, _irem
void LIRGenerator::do_ArithmeticOp_Int(ArithmeticOp* x) {
  // Test if instr is commutative and if we should swap
  LIRItem left(x->x(),  this);
  LIRItem right(x->y(), this);
  LIRItem* left_arg = &left;
  LIRItem* right_arg = &right;
  if (x->is_commutative() && left.is_stack() && right.is_register()) {
    // swap them if left is real stack (or cached) and right is real register(not cached)
    left_arg = &right;
    right_arg = &left;
  }

  left_arg->load_item();

  // do not need to load right, as we can handle stack and constants
  if (x->op() == Bytecodes::_idiv || x->op() == Bytecodes::_irem) {
    rlock_result(x);
    bool need_zero_check = true;
    if (right.is_constant()) {
      jint c = right.get_jint_constant();
      // no need to do div-by-zero check if the divisor is a non-zero constant
      if (c != 0) need_zero_check = false;
      // do not load right if the divisor is a power-of-2 constant
      if (c > 0 && is_power_of_2(c) && Assembler::is_uimm(c - 1, 12)) {
        right_arg->dont_load_item();
      } else {
        right_arg->load_item();
      }
    } else {
      right_arg->load_item();
    }
    if (need_zero_check) {
      CodeEmitInfo* info = state_for(x);
      CodeStub* stub = new DivByZeroStub(info);
      __ cmp_branch(lir_cond_equal, right_arg->result(), LIR_OprFact::longConst(0), T_INT, stub);
    }

    LIR_Opr ill = LIR_OprFact::illegalOpr;
    if (x->op() == Bytecodes::_irem) {
      __ irem(left_arg->result(), right_arg->result(), x->operand(), ill, NULL);
    } else if (x->op() == Bytecodes::_idiv) {
      __ idiv(left_arg->result(), right_arg->result(), x->operand(), ill, NULL);
    }
  } else if (x->op() == Bytecodes::_iadd || x->op() == Bytecodes::_isub) {
    if (right.is_constant() &&
        ((x->op() == Bytecodes::_iadd && Assembler::is_simm(right.get_jint_constant(), 12)) ||
         (x->op() == Bytecodes::_isub && Assembler::is_simm(-right.get_jint_constant(), 12)))) {
      right.load_nonconstant();
    } else {
      right.load_item();
    }
    rlock_result(x);
    arithmetic_op_int(x->op(), x->operand(), left_arg->result(), right_arg->result(), LIR_OprFact::illegalOpr);
  } else {
    assert (x->op() == Bytecodes::_imul, "expect imul");
    if (right.is_constant()) {
      jint c = right.get_jint_constant();
      if (c > 0 && c < max_jint && (is_power_of_2(c) || is_power_of_2(c - 1) || is_power_of_2(c + 1))) {
        right_arg->dont_load_item();
      } else {
        // Cannot use constant op.
        right_arg->load_item();
      }
    } else {
      right.load_item();
    }
    rlock_result(x);
    arithmetic_op_int(x->op(), x->operand(), left_arg->result(), right_arg->result(), new_register(T_INT));
  }
}

void LIRGenerator::do_ArithmeticOp(ArithmeticOp* x) {
  // when an operand with use count 1 is the left operand, then it is
  // likely that no move for 2-operand-LIR-form is necessary
  if (x->is_commutative() && x->y()->as_Constant() == NULL && x->x()->use_count() > x->y()->use_count()) {
    x->swap_operands();
  }

  ValueTag tag = x->type()->tag();
  assert(x->x()->type()->tag() == tag && x->y()->type()->tag() == tag, "wrong parameters");
  switch (tag) {
    case floatTag:
    case doubleTag: do_ArithmeticOp_FPU(x);  return;
    case longTag:   do_ArithmeticOp_Long(x); return;
    case intTag:    do_ArithmeticOp_Int(x);  return;
    default:        ShouldNotReachHere();    return;
  }
}

// _ishl, _lshl, _ishr, _lshr, _iushr, _lushr
void LIRGenerator::do_ShiftOp(ShiftOp* x) {
  LIRItem left(x->x(),  this);
  LIRItem right(x->y(), this);

  left.load_item();

  rlock_result(x);
  if (right.is_constant()) {
    right.dont_load_item();
    int c;
    switch (x->op()) {
      case Bytecodes::_ishl:
        c = right.get_jint_constant() & 0x1f;
        __ shift_left(left.result(), c, x->operand());
        break;
      case Bytecodes::_ishr:
        c = right.get_jint_constant() & 0x1f;
        __ shift_right(left.result(), c, x->operand());
        break;
      case Bytecodes::_iushr:
        c = right.get_jint_constant() & 0x1f;
        __ unsigned_shift_right(left.result(), c, x->operand());
        break;
      case Bytecodes::_lshl:
        c = right.get_jint_constant() & 0x3f;
        __ shift_left(left.result(), c, x->operand());
        break;
      case Bytecodes::_lshr:
        c = right.get_jint_constant() & 0x3f;
        __ shift_right(left.result(), c, x->operand());
        break;
      case Bytecodes::_lushr:
        c = right.get_jint_constant() & 0x3f;
        __ unsigned_shift_right(left.result(), c, x->operand());
        break;
      default:
        ShouldNotReachHere();
    }
  } else {
    right.load_item();
    LIR_Opr tmp = new_register(T_INT);
    switch (x->op()) {
    case Bytecodes::_ishl:
      __ logical_and(right.result(), LIR_OprFact::intConst(0x1f), tmp);
      __ shift_left(left.result(), tmp, x->operand(), tmp);
      break;
    case Bytecodes::_ishr:
      __ logical_and(right.result(), LIR_OprFact::intConst(0x1f), tmp);
      __ shift_right(left.result(), tmp, x->operand(), tmp);
      break;
    case Bytecodes::_iushr:
      __ logical_and(right.result(), LIR_OprFact::intConst(0x1f), tmp);
      __ unsigned_shift_right(left.result(), tmp, x->operand(), tmp);
      break;
    case Bytecodes::_lshl:
      __ logical_and(right.result(), LIR_OprFact::intConst(0x3f), tmp);
      __ shift_left(left.result(), tmp, x->operand(), tmp);
      break;
    case Bytecodes::_lshr:
      __ logical_and(right.result(), LIR_OprFact::intConst(0x3f), tmp);
      __ shift_right(left.result(), tmp, x->operand(), tmp);
      break;
    case Bytecodes::_lushr:
      __ logical_and(right.result(), LIR_OprFact::intConst(0x3f), tmp);
      __ unsigned_shift_right(left.result(), tmp, x->operand(), tmp);
      break;
    default:
      ShouldNotReachHere();
    }
  }
}

// _iand, _land, _ior, _lor, _ixor, _lxor
void LIRGenerator::do_LogicOp(LogicOp* x) {
  LIRItem left(x->x(),  this);
  LIRItem right(x->y(), this);

  left.load_item();

  rlock_result(x);
  if (right.is_constant()
      && ((right.type()->tag() == intTag
           && Assembler::is_uimm(right.get_jint_constant(), 12))
          || (right.type()->tag() == longTag
              && Assembler::is_uimm(right.get_jlong_constant(), 12)))) {
    right.dont_load_item();
  } else {
    right.load_item();
  }
  switch (x->op()) {
    case Bytecodes::_iand:
    case Bytecodes::_land:
      __ logical_and(left.result(), right.result(), x->operand()); break;
    case Bytecodes::_ior:
    case Bytecodes::_lor:
      __ logical_or (left.result(), right.result(), x->operand()); break;
    case Bytecodes::_ixor:
    case Bytecodes::_lxor:
      __ logical_xor(left.result(), right.result(), x->operand()); break;
    default: Unimplemented();
  }
}

// _lcmp, _fcmpl, _fcmpg, _dcmpl, _dcmpg
void LIRGenerator::do_CompareOp(CompareOp* x) {
  LIRItem left(x->x(), this);
  LIRItem right(x->y(), this);
  ValueTag tag = x->x()->type()->tag();
  if (tag == longTag) {
    left.set_destroys_register();
  }
  left.load_item();
  right.load_item();
  LIR_Opr reg = rlock_result(x);

  if (x->x()->type()->is_float_kind()) {
    Bytecodes::Code code = x->op();
    __ fcmp2int(left.result(), right.result(), reg,
                (code == Bytecodes::_fcmpl || code == Bytecodes::_dcmpl));
  } else if (x->x()->type()->tag() == longTag) {
    __ lcmp2int(left.result(), right.result(), reg);
  } else {
    Unimplemented();
  }
}

void LIRGenerator::do_LibmIntrinsic(Intrinsic* x) {
  LIRItem value(x->argument_at(0), this);
  value.set_destroys_register();

  LIR_Opr calc_result = rlock_result(x);
  LIR_Opr result_reg = result_register_for(x->type());

  CallingConvention* cc = NULL;

  if (x->id() == vmIntrinsics::_dpow) {
    LIRItem value1(x->argument_at(1), this);

    value1.set_destroys_register();

    BasicTypeList signature(2);
    signature.append(T_DOUBLE);
    signature.append(T_DOUBLE);
    cc = frame_map()->c_calling_convention(&signature);
    value.load_item_force(cc->at(0));
    value1.load_item_force(cc->at(1));
  } else {
    BasicTypeList signature(1);
    signature.append(T_DOUBLE);
    cc = frame_map()->c_calling_convention(&signature);
    value.load_item_force(cc->at(0));
  }

  switch (x->id()) {
    case vmIntrinsics::_dexp:
      __ call_runtime_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dexp), getThreadTemp(), result_reg, cc->args());
      break;
    case vmIntrinsics::_dlog:
      __ call_runtime_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dlog), getThreadTemp(), result_reg, cc->args());
      break;
    case vmIntrinsics::_dlog10:
      __ call_runtime_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dlog10), getThreadTemp(), result_reg, cc->args());
      break;
    case vmIntrinsics::_dpow:
      __ call_runtime_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dpow), getThreadTemp(), result_reg, cc->args());
      break;
    case vmIntrinsics::_dsin:
      __ call_runtime_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dsin), getThreadTemp(), result_reg, cc->args());
      break;
    case vmIntrinsics::_dcos:
      __ call_runtime_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dcos), getThreadTemp(), result_reg, cc->args());
      break;
    case vmIntrinsics::_dtan:
      __ call_runtime_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dtan), getThreadTemp(), result_reg, cc->args());
      break;
    default:  ShouldNotReachHere();
  }
  __ move(result_reg, calc_result);
}

void LIRGenerator::do_CompareAndSwap(Intrinsic* x, ValueType* type) {
  assert(x->number_of_arguments() == 4, "wrong type");
  LIRItem obj   (x->argument_at(0), this);  // object
  LIRItem offset(x->argument_at(1), this);  // offset of field
  LIRItem cmp   (x->argument_at(2), this);  // value to compare with field
  LIRItem val   (x->argument_at(3), this);  // replace field with val if matches cmp

  assert(obj.type()->tag() == objectTag, "invalid type");

  // In 64bit the type can be long, sparc doesn't have this assert
  // assert(offset.type()->tag() == intTag, "invalid type");

  assert(cmp.type()->tag() == type->tag(), "invalid type");
  assert(val.type()->tag() == type->tag(), "invalid type");

  // get address of field
  obj.load_item();
  offset.load_nonconstant();
  val.load_item();
  cmp.load_item();

  LIR_Address* a;
  if(offset.result()->is_constant()) {
    jlong c = offset.result()->as_jlong();
    if ((jlong)((jint)c) == c) {
      a = new LIR_Address(obj.result(),
                          (jint)c,
                          as_BasicType(type));
    } else {
      LIR_Opr tmp = new_register(T_LONG);
      __ move(offset.result(), tmp);
      a = new LIR_Address(obj.result(),
                          tmp,
                          as_BasicType(type));
    }
  } else {
    a = new LIR_Address(obj.result(),
                        offset.result(),
                        LIR_Address::times_1,
                        0,
                        as_BasicType(type));
  }
  LIR_Opr addr = new_pointer_register();
  __ leal(LIR_OprFact::address(a), addr);

  if (type == objectType) {  // Write-barrier needed for Object fields.
    // Do the pre-write barrier, if any.
    pre_barrier(addr, LIR_OprFact::illegalOpr /* pre_val */,
                true /* do_load */, false /* patch */, NULL);
  }

  LIR_Opr result = rlock_result(x);

  LIR_Opr ill = LIR_OprFact::illegalOpr;  // for convenience
  if (type == objectType)
    __ cas_obj(addr, cmp.result(), val.result(), new_register(T_INT), new_register(T_INT),
               result);
  else if (type == intType)
    __ cas_int(addr, cmp.result(), val.result(), ill, ill);
  else if (type == longType)
    __ cas_long(addr, cmp.result(), val.result(), ill, ill);
  else {
    ShouldNotReachHere();
  }

  __ move(FrameMap::scr1_opr, result);

  if (type == objectType) {   // Write-barrier needed for Object fields.
    // Seems to be precise
    post_barrier(addr, val.result());
  }
}

void LIRGenerator::do_MathIntrinsic(Intrinsic* x) {
  assert(x->number_of_arguments() == 1 || (x->number_of_arguments() == 2 && x->id() == vmIntrinsics::_dpow),
         "wrong type");
  if (x->id() == vmIntrinsics::_dexp || x->id() == vmIntrinsics::_dlog ||
      x->id() == vmIntrinsics::_dpow || x->id() == vmIntrinsics::_dcos ||
      x->id() == vmIntrinsics::_dsin || x->id() == vmIntrinsics::_dtan ||
      x->id() == vmIntrinsics::_dlog10) {
    do_LibmIntrinsic(x);
    return;
  }
  switch (x->id()) {
    case vmIntrinsics::_dabs:
    case vmIntrinsics::_dsqrt: {
      assert(x->number_of_arguments() == 1, "wrong type");
      LIRItem value(x->argument_at(0), this);
      value.load_item();
      LIR_Opr dst = rlock_result(x);

      switch (x->id()) {
        case vmIntrinsics::_dsqrt:
          __ sqrt(value.result(), dst, LIR_OprFact::illegalOpr);
          break;
        case vmIntrinsics::_dabs:
          __ abs(value.result(), dst, LIR_OprFact::illegalOpr);
          break;
        default:
          ShouldNotReachHere();
      }
      break;
    }
    default:
      ShouldNotReachHere();
  }
}

void LIRGenerator::do_ArrayCopy(Intrinsic* x) {
  Register j_rarg0 = RT0;
  Register j_rarg1 = RA0;
  Register j_rarg2 = RA1;
  Register j_rarg3 = RA2;
  Register j_rarg4 = RA3;
  Register j_rarg5 = RA4;

  assert(x->number_of_arguments() == 5, "wrong type");

  // Make all state_for calls early since they can emit code
  CodeEmitInfo* info = state_for(x, x->state());

  LIRItem src(x->argument_at(0), this);
  LIRItem src_pos(x->argument_at(1), this);
  LIRItem dst(x->argument_at(2), this);
  LIRItem dst_pos(x->argument_at(3), this);
  LIRItem length(x->argument_at(4), this);

  // operands for arraycopy must use fixed registers, otherwise
  // LinearScan will fail allocation (because arraycopy always needs a
  // call)

  // The java calling convention will give us enough registers
  // so that on the stub side the args will be perfect already.
  // On the other slow/special case side we call C and the arg
  // positions are not similar enough to pick one as the best.
  // Also because the java calling convention is a "shifted" version
  // of the C convention we can process the java args trivially into C
  // args without worry of overwriting during the xfer

  src.load_item_force     (FrameMap::as_oop_opr(j_rarg0));
  src_pos.load_item_force (FrameMap::as_opr(j_rarg1));
  dst.load_item_force     (FrameMap::as_oop_opr(j_rarg2));
  dst_pos.load_item_force (FrameMap::as_opr(j_rarg3));
  length.load_item_force  (FrameMap::as_opr(j_rarg4));

  LIR_Opr tmp =           FrameMap::as_opr(j_rarg5);

  set_no_result(x);

  int flags;
  ciArrayKlass* expected_type;
  arraycopy_helper(x, &flags, &expected_type);

  __ arraycopy(src.result(), src_pos.result(), dst.result(), dst_pos.result(),
               length.result(), tmp, expected_type, flags, info); // does add_safepoint
}

void LIRGenerator::do_update_CRC32(Intrinsic* x) {
  assert(UseCRC32Intrinsics, "why are we here?");
  // Make all state_for calls early since they can emit code
  LIR_Opr result = rlock_result(x);
  int flags = 0;
  switch (x->id()) {
    case vmIntrinsics::_updateCRC32: {
      LIRItem crc(x->argument_at(0), this);
      LIRItem val(x->argument_at(1), this);
      // val is destroyed by update_crc32
      val.set_destroys_register();
      crc.load_item();
      val.load_item();
      __ update_crc32(crc.result(), val.result(), result);
      break;
    }
    case vmIntrinsics::_updateBytesCRC32:
    case vmIntrinsics::_updateByteBufferCRC32: {
      bool is_updateBytes = (x->id() == vmIntrinsics::_updateBytesCRC32);

      LIRItem crc(x->argument_at(0), this);
      LIRItem buf(x->argument_at(1), this);
      LIRItem off(x->argument_at(2), this);
      LIRItem len(x->argument_at(3), this);
      buf.load_item();
      off.load_nonconstant();

      LIR_Opr index = off.result();
      int offset = is_updateBytes ? arrayOopDesc::base_offset_in_bytes(T_BYTE) : 0;
      if(off.result()->is_constant()) {
        index = LIR_OprFact::illegalOpr;
       offset += off.result()->as_jint();
      }
      LIR_Opr base_op = buf.result();

      if (index->is_valid()) {
        LIR_Opr tmp = new_register(T_LONG);
        __ convert(Bytecodes::_i2l, index, tmp);
        index = tmp;
      }

      if (offset) {
        LIR_Opr tmp = new_pointer_register();
        __ add(base_op, LIR_OprFact::intConst(offset), tmp);
        base_op = tmp;
        offset = 0;
      }

      LIR_Address* a = new LIR_Address(base_op, index, LIR_Address::times_1, offset, T_BYTE);
      BasicTypeList signature(3);
      signature.append(T_INT);
      signature.append(T_ADDRESS);
      signature.append(T_INT);
      CallingConvention* cc = frame_map()->c_calling_convention(&signature);
      const LIR_Opr result_reg = result_register_for(x->type());

      LIR_Opr addr = new_pointer_register();
      __ leal(LIR_OprFact::address(a), addr);

      crc.load_item_force(cc->at(0));
      __ move(addr, cc->at(1));
      len.load_item_force(cc->at(2));

      __ call_runtime_leaf(StubRoutines::updateBytesCRC32(), getThreadTemp(), result_reg, cc->args());
      __ move(result_reg, result);

      break;
    }
    default: {
      ShouldNotReachHere();
    }
  }
}

// _i2l, _i2f, _i2d, _l2i, _l2f, _l2d, _f2i, _f2l, _f2d, _d2i, _d2l, _d2f
// _i2b, _i2c, _i2s
void LIRGenerator::do_Convert(Convert* x) {
  LIRItem value(x->value(), this);
  value.load_item();
  LIR_Opr input = value.result();
  LIR_Opr result = rlock(x);

  // arguments of lir_convert
  LIR_Opr conv_input = input;
  LIR_Opr conv_result = result;

  switch (x->op()) {
    case Bytecodes::_f2i:
    case Bytecodes::_f2l:
      __ convert(x->op(), conv_input, conv_result, NULL, new_register(T_FLOAT));
      break;
    case Bytecodes::_d2i:
    case Bytecodes::_d2l:
      __ convert(x->op(), conv_input, conv_result, NULL, new_register(T_DOUBLE));
      break;
    default:
      __ convert(x->op(), conv_input, conv_result);
      break;
  }

  assert(result->is_virtual(), "result must be virtual register");
  set_result(x, result);
}

void LIRGenerator::do_NewInstance(NewInstance* x) {
#ifndef PRODUCT
  if (PrintNotLoaded && !x->klass()->is_loaded()) {
    tty->print_cr("   ###class not loaded at new bci %d", x->printable_bci());
  }
#endif
  CodeEmitInfo* info = state_for(x, x->state());
  LIR_Opr reg = result_register_for(x->type());
  new_instance(reg, x->klass(), x->is_unresolved(),
                       FrameMap::t0_oop_opr,
                       FrameMap::t1_oop_opr,
                       FrameMap::a4_oop_opr,
                       LIR_OprFact::illegalOpr,
                       FrameMap::a3_metadata_opr, info);
  LIR_Opr result = rlock_result(x);
  __ move(reg, result);
}

void LIRGenerator::do_NewTypeArray(NewTypeArray* x) {
  CodeEmitInfo* info = state_for(x, x->state());

  LIRItem length(x->length(), this);
  length.load_item_force(FrameMap::s0_opr);

  LIR_Opr reg = result_register_for(x->type());
  LIR_Opr tmp1 = FrameMap::t0_oop_opr;
  LIR_Opr tmp2 = FrameMap::t1_oop_opr;
  LIR_Opr tmp3 = FrameMap::a5_oop_opr;
  LIR_Opr tmp4 = reg;
  LIR_Opr klass_reg = FrameMap::a3_metadata_opr;
  LIR_Opr len = length.result();
  BasicType elem_type = x->elt_type();

  __ metadata2reg(ciTypeArrayKlass::make(elem_type)->constant_encoding(), klass_reg);

  CodeStub* slow_path = new NewTypeArrayStub(klass_reg, len, reg, info);
  __ allocate_array(reg, len, tmp1, tmp2, tmp3, tmp4, elem_type, klass_reg, slow_path);

  LIR_Opr result = rlock_result(x);
  __ move(reg, result);
}

void LIRGenerator::do_NewObjectArray(NewObjectArray* x) {
  LIRItem length(x->length(), this);
  // in case of patching (i.e., object class is not yet loaded), we need to reexecute the instruction
  // and therefore provide the state before the parameters have been consumed
  CodeEmitInfo* patching_info = NULL;
  if (!x->klass()->is_loaded() || PatchALot) {
    patching_info =  state_for(x, x->state_before());
  }

  CodeEmitInfo* info = state_for(x, x->state());

  LIR_Opr reg = result_register_for(x->type());
  LIR_Opr tmp1 = FrameMap::t0_oop_opr;
  LIR_Opr tmp2 = FrameMap::t1_oop_opr;
  LIR_Opr tmp3 = FrameMap::a5_oop_opr;
  LIR_Opr tmp4 = reg;
  LIR_Opr klass_reg = FrameMap::a3_metadata_opr;

  length.load_item_force(FrameMap::s0_opr);
  LIR_Opr len = length.result();

  CodeStub* slow_path = new NewObjectArrayStub(klass_reg, len, reg, info);
  ciKlass* obj = (ciKlass*) ciObjArrayKlass::make(x->klass());
  if (obj == ciEnv::unloaded_ciobjarrayklass()) {
    BAILOUT("encountered unloaded_ciobjarrayklass due to out of memory error");
  }
  klass2reg_with_patching(klass_reg, obj, patching_info);
  __ allocate_array(reg, len, tmp1, tmp2, tmp3, tmp4, T_OBJECT, klass_reg, slow_path);

  LIR_Opr result = rlock_result(x);
  __ move(reg, result);
}

void LIRGenerator::do_NewMultiArray(NewMultiArray* x) {
  Values* dims = x->dims();
  int i = dims->length();
  LIRItemList* items = new LIRItemList(i, NULL);
  while (i-- > 0) {
    LIRItem* size = new LIRItem(dims->at(i), this);
    items->at_put(i, size);
  }

  // Evaluate state_for early since it may emit code.
  CodeEmitInfo* patching_info = NULL;
  if (!x->klass()->is_loaded() || PatchALot) {
    patching_info = state_for(x, x->state_before());

    // Cannot re-use same xhandlers for multiple CodeEmitInfos, so
    // clone all handlers (NOTE: Usually this is handled transparently
    // by the CodeEmitInfo cloning logic in CodeStub constructors but
    // is done explicitly here because a stub isn't being used).
    x->set_exception_handlers(new XHandlers(x->exception_handlers()));
  }
  CodeEmitInfo* info = state_for(x, x->state());

  i = dims->length();
  while (i-- > 0) {
    LIRItem* size = items->at(i);
    size->load_item();

    store_stack_parameter(size->result(), in_ByteSize(i*4));
  }

  LIR_Opr klass_reg = FrameMap::a0_metadata_opr;
  klass2reg_with_patching(klass_reg, x->klass(), patching_info);

  LIR_Opr rank = FrameMap::s0_opr;
  __ move(LIR_OprFact::intConst(x->rank()), rank);
  LIR_Opr varargs = FrameMap::a2_opr;
  __ move(FrameMap::sp_opr, varargs);
  LIR_OprList* args = new LIR_OprList(3);
  args->append(klass_reg);
  args->append(rank);
  args->append(varargs);
  LIR_Opr reg = result_register_for(x->type());
  __ call_runtime(Runtime1::entry_for(Runtime1::new_multi_array_id),
                  LIR_OprFact::illegalOpr,
                  reg, args, info);

  LIR_Opr result = rlock_result(x);
  __ move(reg, result);
}

void LIRGenerator::do_BlockBegin(BlockBegin* x) {
  // nothing to do for now
}

void LIRGenerator::do_CheckCast(CheckCast* x) {
  LIRItem obj(x->obj(), this);

  CodeEmitInfo* patching_info = NULL;
  if (!x->klass()->is_loaded() ||
      (PatchALot && !x->is_incompatible_class_change_check() &&
       !x->is_invokespecial_receiver_check())) {
    // must do this before locking the destination register as an oop register,
    // and before the obj is loaded (the latter is for deoptimization)
    patching_info = state_for(x, x->state_before());
  }
  obj.load_item();

  // info for exceptions
  CodeEmitInfo* info_for_exception =
      (x->needs_exception_state() ? state_for(x) :
                                    state_for(x, x->state_before(), true /*ignore_xhandler*/));

  CodeStub* stub;
  if (x->is_incompatible_class_change_check()) {
    assert(patching_info == NULL, "can't patch this");
    stub = new SimpleExceptionStub(Runtime1::throw_incompatible_class_change_error_id,
                                   LIR_OprFact::illegalOpr, info_for_exception);
  } else if (x->is_invokespecial_receiver_check()) {
    assert(patching_info == NULL, "can't patch this");
    stub = new DeoptimizeStub(info_for_exception);
  } else {
    stub = new SimpleExceptionStub(Runtime1::throw_class_cast_exception_id,
                                   obj.result(), info_for_exception);
  }
  LIR_Opr reg = rlock_result(x);
  LIR_Opr tmp3 = LIR_OprFact::illegalOpr;
  if (!x->klass()->is_loaded() || UseCompressedClassPointers) {
    tmp3 = new_register(objectType);
  }
  __ checkcast(reg, obj.result(), x->klass(),
               new_register(objectType), new_register(objectType), tmp3,
               x->direct_compare(), info_for_exception, patching_info, stub,
               x->profiled_method(), x->profiled_bci());
}

void LIRGenerator::do_InstanceOf(InstanceOf* x) {
  LIRItem obj(x->obj(), this);

  // result and test object may not be in same register
  LIR_Opr reg = rlock_result(x);
  CodeEmitInfo* patching_info = NULL;
  if ((!x->klass()->is_loaded() || PatchALot)) {
    // must do this before locking the destination register as an oop register
    patching_info = state_for(x, x->state_before());
  }
  obj.load_item();
  LIR_Opr tmp3 = LIR_OprFact::illegalOpr;
  if (!x->klass()->is_loaded() || UseCompressedClassPointers) {
    tmp3 = new_register(objectType);
  }
  __ instanceof(reg, obj.result(), x->klass(),
                new_register(objectType), new_register(objectType), tmp3,
                x->direct_compare(), patching_info, x->profiled_method(), x->profiled_bci());
}

void LIRGenerator::do_If(If* x) {
  assert(x->number_of_sux() == 2, "inconsistency");
  ValueTag tag = x->x()->type()->tag();
  bool is_safepoint = x->is_safepoint();

  If::Condition cond = x->cond();

  LIRItem xitem(x->x(), this);
  LIRItem yitem(x->y(), this);
  LIRItem* xin = &xitem;
  LIRItem* yin = &yitem;

  if (tag == longTag) {
    // for longs, only conditions "eql", "neq", "lss", "geq" are valid;
    // mirror for other conditions
    if (cond == If::gtr || cond == If::leq) {
      cond = Instruction::mirror(cond);
      xin = &yitem;
      yin = &xitem;
    }
    xin->set_destroys_register();
  }
  xin->load_item();

  if (tag == longTag) {
    if (yin->is_constant() && yin->get_jlong_constant() == 0) {
      yin->dont_load_item();
    } else {
      yin->load_item();
    }
  } else if (tag == intTag) {
    if (yin->is_constant() && yin->get_jint_constant() == 0)  {
      yin->dont_load_item();
    } else {
      yin->load_item();
    }
  } else {
    yin->load_item();
  }

  set_no_result(x);

  LIR_Opr left = xin->result();
  LIR_Opr right = yin->result();

  // add safepoint before generating condition code so it can be recomputed
  if (x->is_safepoint()) {
    // increment backedge counter if needed
    increment_backedge_counter(state_for(x, x->state_before()), x->profiled_bci());
    __ safepoint(LIR_OprFact::illegalOpr, state_for(x, x->state_before()));
  }

  // Generate branch profiling. Profiling code doesn't kill flags.
  profile_branch(x, cond, left, right);
  move_to_phi(x->state());
  if (x->x()->type()->is_float_kind()) {
    __ cmp_branch(lir_cond(cond), left, right, right->type(), x->tsux(), x->usux());
  } else {
    __ cmp_branch(lir_cond(cond), left, right, right->type(), x->tsux());
  }
  assert(x->default_sux() == x->fsux(), "wrong destination above");
  __ jump(x->default_sux());
}

LIR_Opr LIRGenerator::getThreadPointer() {
   return FrameMap::as_pointer_opr(TREG);
}

void LIRGenerator::trace_block_entry(BlockBegin* block) { Unimplemented(); }

void LIRGenerator::volatile_field_store(LIR_Opr value, LIR_Address* address,
                                        CodeEmitInfo* info) {
  __ volatile_store_mem_reg(value, address, info);
}

void LIRGenerator::volatile_field_load(LIR_Address* address, LIR_Opr result,
                                       CodeEmitInfo* info) {
  // 8179954: We need to make sure that the code generated for
  // volatile accesses forms a sequentially-consistent set of
  // operations when combined with STLR and LDAR.  Without a leading
  // membar it's possible for a simple Dekker test to fail if loads
  // use LD;DMB but stores use STLR.  This can happen if C2 compiles
  // the stores in one method and C1 compiles the loads in another.
  __ membar();
  __ volatile_load_mem_reg(address, result, info);
}

void LIRGenerator::get_Object_unsafe(LIR_Opr dst, LIR_Opr src, LIR_Opr offset,
                                     BasicType type, bool is_volatile) {
  LIR_Address* addr = new LIR_Address(src, offset, type);
  __ load(addr, dst);
}

void LIRGenerator::put_Object_unsafe(LIR_Opr src, LIR_Opr offset, LIR_Opr data,
                                     BasicType type, bool is_volatile) {
  LIR_Address* addr = new LIR_Address(src, offset, type);
  bool is_obj = (type == T_ARRAY || type == T_OBJECT);
  if (is_obj) {
    // Do the pre-write barrier, if any.
    pre_barrier(LIR_OprFact::address(addr), LIR_OprFact::illegalOpr /* pre_val */,
                true /* do_load */, false /* patch */, NULL);
    __ move(data, addr);
    assert(src->is_register(), "must be register");
    // Seems to be a precise address
    post_barrier(LIR_OprFact::address(addr), data);
  } else {
    __ move(data, addr);
  }
}

void LIRGenerator::do_UnsafeGetAndSetObject(UnsafeGetAndSetObject* x) {
  BasicType type = x->basic_type();
  LIRItem src(x->object(), this);
  LIRItem off(x->offset(), this);
  LIRItem value(x->value(), this);

  src.load_item();
  off.load_nonconstant();

  // We can cope with a constant increment in an xadd
  if (! (x->is_add()
         && value.is_constant()
         && can_inline_as_constant(x->value()))) {
    value.load_item();
  }

  LIR_Opr dst = rlock_result(x, type);
  LIR_Opr data = value.result();
  bool is_obj = (type == T_ARRAY || type == T_OBJECT);
  LIR_Opr offset = off.result();

  if (data == dst) {
    LIR_Opr tmp = new_register(data->type());
    __ move(data, tmp);
    data = tmp;
  }

  LIR_Address* addr;
  if (offset->is_constant()) {
    jlong l = offset->as_jlong();
    assert((jlong)((jint)l) == l, "offset too large for constant");
    jint c = (jint)l;
    addr = new LIR_Address(src.result(), c, type);
  } else {
    addr = new LIR_Address(src.result(), offset, type);
  }

  LIR_Opr tmp = new_register(T_INT);
  LIR_Opr ptr = LIR_OprFact::illegalOpr;

  if (x->is_add()) {
    __ xadd(LIR_OprFact::address(addr), data, dst, tmp);
  } else {
    if (is_obj) {
      // Do the pre-write barrier, if any.
      ptr = new_pointer_register();
      __ add(src.result(), off.result(), ptr);
      pre_barrier(ptr, LIR_OprFact::illegalOpr /* pre_val */,
                  true /* do_load */, false /* patch */, NULL);
    }
    __ xchg(LIR_OprFact::address(addr), data, dst, tmp);
    if (is_obj) {
      post_barrier(ptr, data);
    }
  }
}
