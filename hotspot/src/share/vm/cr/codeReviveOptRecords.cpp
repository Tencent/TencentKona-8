/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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
 */

#include "precompiled.hpp"
#include "ci/ciCallProfile.hpp"
#include "ci/ciEnv.hpp"
#include "ci/ciMethodData.hpp"
#include "ci/ciUtilities.hpp"
#include "cr/codeReviveOptRecords.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/revive.hpp"

static const int cost_virtual_call   = 10;
static const int cost_not_inline     = 10;
static const int cost_receiver_check = 10;
static const int cost_cutted_if      = 10;
static const int cost_test_jmp       = 1;

static void print_indent(outputStream* out, int indent) {
  for (int i = 0; i < indent; i++) {
    out->print("  ");
  }
}

CodeReviveOptRecords::CodeReviveOptRecords(ciEnv* env) {
  _arena = env->arena();
  _oop_recorder = env->oop_recorder();
  _opts = new(_arena) GrowableArray<OptRecord*>(_arena, 10, 0, NULL);
  _content_bytes = NULL;
  _size_in_bytes = 0;
}

/*
 * insert record into _opts, need deduplicate first.
 * duplicate is possible if callee is inlined multiple times and
 * trigger same inline/devirtual in callee.
 */
void CodeReviveOptRecords::insert(OptRecord* r) {
  for (int i = 0; i < _opts->length(); i++) {
    OptRecord* o = _opts->at(i);
    if (r->equal(o)) {
      return;
    }
  }
  _opts->append(r);
  if (CodeRevive::is_log_on(cr_opt, cr_trace)) {
    CodeRevive::out()->print("OptRecord insert: ");
    r->print_on_with_indent(CodeRevive::out(), 0);
  }
}

void CodeReviveOptRecords::add_DeVirtualRecord(ciMethod* caller, ciKlass* klass1, ciKlass* klass2, int bci, int morphism, bool miss_is_trap) {
  insert(new(_arena) OptRecordDeVirtual(
         OptRecord::COMPILE,
         _oop_recorder,
         _oop_recorder->find_index(caller->constant_encoding()),
         _oop_recorder->find_index(klass1->constant_encoding()),
         klass2 == NULL ? -1 : _oop_recorder->find_index(klass2->constant_encoding()),
         bci, morphism, miss_is_trap));
}

void CodeReviveOptRecords::add_ProfiledReceiverRecord(ciMethod* method, int bci, ciKlass* klass) {
  insert(new (_arena) OptProfiledReceiver(
         OptRecord::COMPILE,
         _oop_recorder,
         _oop_recorder->find_index(method->constant_encoding()),
         bci,
         _oop_recorder->find_index(klass->constant_encoding())));
}

void CodeReviveOptRecords::add_ProfiledUnstableIf(ciMethod* method, int bci, bool taken) {
  insert(new (_arena) OptProfiledUnstableIf (
         OptRecord::COMPILE,
         _oop_recorder,
         _oop_recorder->find_index(method->constant_encoding()),
         bci,
         taken));
}

void CodeReviveOptRecords::add_InlineRecord(ciMethod* caller, ciMethod* callee, int bci) {
  insert(new(_arena) OptRecordInline (
         OptRecord::COMPILE,
         _oop_recorder,
         _oop_recorder->find_index(caller->constant_encoding()),
         _oop_recorder->find_index(callee->constant_encoding()),
         bci));
}

void CodeReviveOptRecords::add_ConstantReplaceRecord(ciKlass* klass, int field_offset, jshort field_type, jlong field_val) {
  insert(new (_arena) OptConstantReplace(
         OptRecord::COMPILE,
         _oop_recorder,
         _oop_recorder->find_index(klass->constant_encoding()),
         field_offset,
         field_type,
         field_val));
}

size_t CodeReviveOptRecords::estimate_size_in_bytes() {
  size_t size = 100;
  for (int i = 0; i < _opts->length(); i++) {
    OptRecord* o = _opts->at(i);
    size += o->estimate_size_in_bytes();
  }
  return size;
}
/*
 * iterate _opts and write into buffer
 */
void CodeReviveOptRecords::encode_content_bytes() {
  guarantee(CodeRevive::is_save(), "should be");
  if (_opts->length() == 0) {
    return;
  }
  CompressedWriteStream bytes((int)estimate_size_in_bytes());
  for (int i = 0; i < _opts->length(); i++) {
    OptRecord* o = _opts->at(i);
    o->write_to_stream(&bytes);
  }
  bytes.write_byte(OptRecord::END_OPT);

  // round it out to a word boundary
  while (bytes.position() % sizeof(HeapWord) != 0) {
    bytes.write_byte(OptRecord::END_OPT);
  }
  _content_bytes = bytes.buffer();
  _size_in_bytes = bytes.position();
}

void CodeReviveOptRecords::copy_to(nmethod* nm) {
  address beg = nm->opt_records_begin();
  address end = nm->opt_records_end();
  guarantee(end - beg >= (ptrdiff_t) size_in_bytes(), "bad sizing");
  Copy::disjoint_words((HeapWord*) content_bytes(),
                       (HeapWord*) beg,
                       size_in_bytes() / sizeof(HeapWord));
  assert(size_in_bytes() % sizeof(HeapWord) == 0, "copy by words");
}

OptRecord* OptStream::next() {
  int opt = _bytes.read_byte();
  if (opt == OptRecord::END_OPT) {
    return NULL;
  }
  switch(opt) {
    case OptRecord::DeVirtual: {
      OptRecordDeVirtual* r = new OptRecordDeVirtual(_ctx_type, _ctx);
      r->read_from_stream(&_bytes);
      return r;
    }
    case OptRecord::ProfiledReceiver: {
      OptProfiledReceiver* r = new OptProfiledReceiver(_ctx_type, _ctx);
      r->read_from_stream(&_bytes);
      return r;
    }
    case OptRecord::ProfiledUnstableIf: {
      OptProfiledUnstableIf* r = new OptProfiledUnstableIf(_ctx_type, _ctx);
      r->read_from_stream(&_bytes);
      return r;
    }
    case OptRecord::Inline: {
      OptRecordInline* r = new OptRecordInline(_ctx_type, _ctx);
      r->read_from_stream(&_bytes);
      return r;
    }
    case OptRecord::ConstantReplace: {
      OptConstantReplace* r = new OptConstantReplace(_ctx_type, _ctx);
      r->read_from_stream(&_bytes);
      return r;
    }
    case OptRecord::END_OPT: {
      break;
    }
    default:
      fatal("illegal");
  }
  return NULL;
}

Metadata* OptRecord::get_meta(int idx) {
  guarantee(idx > 0, "should be");
  switch (_ctx_type) {
    case COMPILE:
      return ((OopRecorder*)_ctx)->metadata_at(idx);
    case NM_INSTALL:
      return ((nmethod*)_ctx)->metadata_at(idx);
    case REVIVE: {
      // meta array's slot 0 is not null
      // in nmethod and oop record slot 0 is NULL
      ciBaseObject* o = ((GrowableArray<ciBaseObject*>*)_ctx)->at(idx - 1);
      if (o == NULL) return NULL;
      guarantee(o->is_metadata(), "should be");
      return o->as_metadata()->constant_encoding();
    }
    default:
      guarantee(false, "unexpected");
  }
  return NULL;
}

char* OptRecord::get_meta_name(int idx) {
  // In merge stage the idx is index in the space and can be 0.
  // But in other stage, idx is the index of data array and must be larger than 0.
  guarantee(idx > 0 || (_ctx_type == MERGE && idx >= 0), "should be");
  switch (_ctx_type) {
    case COMPILE:
    case NM_INSTALL:
    case REVIVE: {
      Metadata* m = get_meta(idx);
      if (m->is_klass()) {
        return ((Klass*)m)->name()->as_quoted_ascii();
      } else {
        return ((Method*)m)->name_and_sig_as_C_string();
      }
      break;
    }
    case MERGE: {
      return ((CodeReviveMetaSpace*)_ctx)->metadata_name(idx);
    }
    case DUMP: {
      return ((GrowableArray<char*>*)_ctx)->at(idx - 1);
    }
    default:
      guarantee(false, "unexpected");
  }
  return NULL;
}

ciMetadata* OptRecord::get_ci_meta(int idx) {
  guarantee(idx > 0, "should be");
  guarantee(_ctx_type == REVIVE, "should be");
  ciBaseObject* o = ((GrowableArray<ciBaseObject*>*)_ctx)->at(idx - 1);
  guarantee(o != NULL && o->is_metadata(), "should be");
  return o->as_metadata();
}

void OptRecord::nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest) {
  _method_idx = dest->at(_method_idx - 1);
}

void OptRecordDeVirtual::nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest) {
  OptRecord::nm_meta_index_to_meta_space_index(dest);
  _klass1_idx = dest->at(_klass1_idx - 1);
  assert(_klass2_idx != 0, "must be");
  if (_klass2_idx > 0) {
    _klass2_idx = dest->at(_klass2_idx - 1);
  }
}

bool OptRecordDeVirtual::equal(OptRecord* other) {
  if (other->opt_type() != OptRecord::DeVirtual) {
    return false;
  }
  OptRecordDeVirtual* o = (OptRecordDeVirtual*)other;
  // trap/morphism could be different when trap count/profile changes
  if (o->_method_idx == _method_idx && o->_bci == _bci && o->_morphism == _morphism && o->_miss_is_trap == _miss_is_trap) {
    if ((o->_klass1_idx == _klass1_idx && o->_klass2_idx == _klass2_idx) ||
        (o->_klass2_idx == _klass1_idx && o->_klass1_idx == _klass2_idx)) {
      return true;
    }
  }
  return false;
}

/*
 * emulate exeution on optimized code with current profiling data
 * VirtualCall cost = 10
 * Basic test       = 1
 * Trap             = max_jint
 *
 * return saved/more cost comapred with direct virtual call
 * count() is total;
 */
int OptRecordDeVirtual::calc_opt_score() {
  assert(_method_idx >= 1, "must be");
  assert(_klass1_idx >= 1, "must be");
  ciMethod* caller = (ciMethod*)get_ci_meta(_method_idx);
  ciCallProfile profile = caller->call_profile_at_bci(_bci);

  // no profile, optimized code maybe not executed or loaded too early
  // no side effect if not executed
  int total_count = profile.count();
  if (total_count <= 0) {
    return 0;
  }

  // emulate execution
  // tty->print_cr("startming matching");
  int total_save = 0;
  int other_count = total_count;
  Klass* klass1 = (Klass*)get_meta(_klass1_idx);
  Klass* klass2 = _klass2_idx == -1 ? NULL : (Klass*)get_meta(_klass2_idx);
  int test_count = klass2 == NULL ? 1 : 2;
  for (int i = 0; profile.has_receiver(i); i++) {
    Klass* receiver = (Klass*)profile.receiver(i)->constant_encoding();
    int receiver_count = profile.receiver_count(i);
    int result = 0;
    if (receiver == klass1) {
      result = (cost_test_jmp - cost_virtual_call) * receiver_count;
    } else if (receiver == klass2) {
      result = (cost_test_jmp * 2 - cost_virtual_call) * receiver_count;
    } else if (_miss_is_trap) {
      if (CodeRevive::is_log_on(cr_restore, cr_warning)) {
        CodeRevive::out()->print_cr("OptRecordDeVirtual: fail_with_trap klass not hit");
        print_on_with_indent(CodeRevive::out(), 0);
        profile.print_on(CodeRevive::out());
      }
      return max_jint;
    } else {
      result = test_count * receiver_count * cost_test_jmp;
    }
    other_count -= receiver_count;
    total_save += result;
  }
  // other count
  if (other_count != 0) {
    guarantee(other_count > 0, "should be");
    if (_miss_is_trap) {
      if (CodeRevive::is_log_on(cr_restore, cr_warning)) {
        CodeRevive::out()->print_cr("OptRecordDeVirtual: fail_with_trap other not hit");
        print_on_with_indent(CodeRevive::out());
        profile.print_on(CodeRevive::out());
      }
      return max_jint;
    }
    total_save += test_count * other_count * cost_test_jmp;
  }
  return total_save;
}

void OptRecordDeVirtual::read_from_stream(CompressedReadStream* in) {
  _morphism     = in->read_byte();
  _miss_is_trap = in->read_bool() != 0;
  _bci          = in->read_short();
  _method_idx   = in->read_short();
  _klass1_idx   = in->read_short();
  _klass2_idx   = in->read_short();
}

void OptRecordDeVirtual::write_to_stream(CompressedWriteStream* out) {
  out->write_byte(DeVirtual);
  out->write_byte(_morphism);
  out->write_bool(_miss_is_trap);
  out->write_short(_bci);
  out->write_short(_method_idx);
  out->write_short(_klass1_idx);
  out->write_short(_klass2_idx);
}

size_t OptRecordDeVirtual::estimate_size_in_bytes() {
  return 1 + 1 + 1 + 2 * 4;
}

void OptRecordDeVirtual::print_on_with_indent(outputStream* out, int indent) {
  assert(_method_idx >= 0, "must be");
  assert(_klass1_idx >= 0, "must be");
  print_indent(out, indent);
  out->print("DeVirtual at method=%s bci=%d morphism=%d", get_meta_name(_method_idx), _bci, _morphism);
  out->print("  klass1: %s", get_meta_name(_klass1_idx));
  if (_klass2_idx != -1) {
    out->print("  klass2: %s", get_meta_name(_klass2_idx));
  }
  out->print_cr("  default: %s", _miss_is_trap ? "trap" : "virtual call");
}

void OptProfiledReceiver::nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest) {
  OptRecord::nm_meta_index_to_meta_space_index(dest);
  _klass_idx = dest->at(_klass_idx - 1);
}

void OptProfiledReceiver::read_from_stream(CompressedReadStream* in) {
  _bci        = in->read_short();
  _method_idx = in->read_short();
  _klass_idx  = in->read_short();
}

void OptProfiledReceiver::write_to_stream(CompressedWriteStream* out) {
  out->write_byte(ProfiledReceiver);
  out->write_short(_bci);
  out->write_short(_method_idx);
  out->write_short(_klass_idx);
}

void OptProfiledReceiver::print_on_with_indent(outputStream* out, int indent) {
  assert(_method_idx >= 0, "must be");
  assert(_klass_idx >= 0, "must be");
  print_indent(out, indent);
  out->print_cr("ProfiledReceiver at method=%s bci=%d klass: %s",
                get_meta_name(_method_idx), _bci, get_meta_name(_klass_idx));
}

size_t OptProfiledReceiver::estimate_size_in_bytes() {
  return 1 + 2 * 3;
}

bool OptProfiledReceiver::equal(OptRecord* other) {
  if (other->opt_type() != OptRecord::ProfiledReceiver) {
    return false;
  }
  OptProfiledReceiver* o = (OptProfiledReceiver*)other;
  if (o->_method_idx == _method_idx && o->_bci == _bci && o->_klass_idx == _klass_idx) {
    return true;
  }
  return false;
}

int OptProfiledReceiver::calc_opt_score() {
  assert(_method_idx >= 1, "must be");
  ciMethod* method = (ciMethod*)get_ci_meta(_method_idx);
  assert(_klass_idx >= 1, "must be");
  ciKlass* expected_kls = (ciKlass*)get_ci_meta(_klass_idx);
  assert(method->is_method(), "must be");
  assert(expected_kls->is_klass(), "must be");

  ciCallProfile profile = method->call_profile_at_bci(_bci);

  int result = 0; // default: unjudgeable

  if (profile.count() < 0) {
    // negative counter means:
    //   1, the method data is not mature, it's safe to exclude the aot code.
    //   2, the type check has failed. The counter will decrement if subtype check is failed. And the trap
    //      is only generated when there is no subtype check failure. If we see subtype check failure, the
    //      generated trap in aot jit code will always be triggered, so we should not install the aot code.
    if (CodeRevive::is_log_on(cr_restore, cr_warning)) {
      ResourceMark rm;
      CodeRevive::out()->print_cr("Invalid neg count in opt profiled receiver");
      print_on_with_indent(CodeRevive::out(), 0);
    }
    return max_jint;
  } else if (profile.count() >= 0 && profile.has_receiver(0) && profile.morphism() == 1) {
    ciKlass* receiver = profile.receiver(0);
    if (receiver == expected_kls) {
      int count = profile.receiver_count(0);
      result += (cost_test_jmp - cost_receiver_check) * count;
    } else {
      if (CodeRevive::is_log_on(cr_restore, cr_warning)) {
        ResourceMark rm;
        CodeRevive::out()->print_cr("Different klass in opt profiled receiver");
        print_on_with_indent(CodeRevive::out(), 0);
        CodeRevive::out()->print("receiver: ");
        receiver->print_name_on(CodeRevive::out());
      }
      return max_jint;
    }
  }

  return result;
}

void OptProfiledUnstableIf::read_from_stream(CompressedReadStream* in) {
  _bci               = in->read_short();
  _method_idx        = in->read_short();
  _taken_branch_trap = in->read_bool() != 0;
}

void OptProfiledUnstableIf::write_to_stream(CompressedWriteStream* out) {
  out->write_byte(ProfiledUnstableIf);
  out->write_short(_bci);
  out->write_short(_method_idx);
  out->write_bool(_taken_branch_trap);
}

void OptProfiledUnstableIf::print_on_with_indent(outputStream* out, int indent) {
  print_indent(out, indent);
  out->print_cr("ProfiledUnstableIf at method=%s bci=%d uncommon trap: %s edge",
                get_meta_name(_method_idx), _bci, _taken_branch_trap ? "taken" : "untaken");
}

size_t OptProfiledUnstableIf::estimate_size_in_bytes() {
  return 1 + 2 * 2 + 1;
}

bool OptProfiledUnstableIf::equal(OptRecord* other) {
  if (other->opt_type() != OptRecord::ProfiledUnstableIf) {
    return false;
  }
  OptProfiledUnstableIf* o = (OptProfiledUnstableIf*)other;
  if (o->_method_idx == _method_idx && o->_bci == _bci && o->_taken_branch_trap == _taken_branch_trap) {
    return true;
  }
  return false;
}

int OptProfiledUnstableIf::calc_opt_score() {
  assert(_method_idx >= 1, "must be");
  ciMethod* method = (ciMethod*)get_ci_meta(_method_idx);
  assert(method->is_method(), "must be");
  if (!(method->method_data() != NULL && method->method_data()->is_mature())) {
    // no enough information to get a conclusion
    return 0;
  }
  ciProfileData* data = method->method_data()->bci_to_data(_bci);

  int result = 0; // default: unjudgeable

  if (data != NULL && data->is_BranchData()) {
    BranchData* br = data->as_BranchData();
    // copied from 'Parse::dynamic_branch_prediction'
    int taken = method->scale_count(br->taken());
    int not_taken = method->scale_count(br->not_taken());
    if (taken >= 0 && not_taken >= 0 && taken + not_taken > 40) {
      // interpreter start recording profile from the 3300-th invocation of the method.
      // if both branch has been taken, there will be no trap.
      if ((_taken_branch_trap && taken == 0) || (!_taken_branch_trap && not_taken == 0)) {
        result += (0 - cost_cutted_if) * (taken + not_taken);
      } else {
        if (CodeRevive::is_log_on(cr_restore, cr_warning)) {
          ResourceMark rm;
          CodeRevive::out()->print_cr("Fail with trap in opt profiled unstable if");
          print_on_with_indent(CodeRevive::out(), 0);
          CodeRevive::out()->print_cr("Actual taken %d not taken %d", taken, not_taken);
        }
        return max_jint;
      }
    }
  }
  return result;
}

void OptRecordInline::nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest) {
  OptRecord::nm_meta_index_to_meta_space_index(dest);
  _callee_idx = dest->at(_callee_idx - 1);
}

void OptRecordInline::write_to_stream(CompressedWriteStream* out) {
  out->write_byte(Inline);
  out->write_short(_bci);
  out->write_short(_method_idx);
  out->write_short(_callee_idx);
}

void OptRecordInline::read_from_stream(CompressedReadStream* in) {
  _bci          = in->read_short();
  _method_idx   = in->read_short();
  _callee_idx   = in->read_short();
}

size_t OptRecordInline::estimate_size_in_bytes() {
  return 1 + 2 * 3;
}

int OptRecordInline::calc_opt_score() {
  assert(_method_idx >= 1, "must be");
  assert(_callee_idx >= 1, "must be");
  ciMethod* caller = (ciMethod*)get_ci_meta(_method_idx);
  ciMethodData* method_data = caller->method_data_or_null();
  if (method_data == NULL) {
    return 0;
  }
  ciProfileData* profile = method_data->bci_to_data(_bci);
  if (profile == NULL) {
    return 0;
  }
  guarantee(profile->is_CounterData(), "should be");
  CounterData* counted_data = profile->as_CounterData();

  // print_on(tty);
  // profile->print_data_on(tty);

  if (!counted_data->is_ReceiverTypeData()) {
    // direct call
    return -((int)counted_data->count()) * cost_not_inline;
  } else {
    // not direct call, get profiling count for this method
    // 1. speculative/optimized to direct + inline:
    //    passing depency, receiver must be same with callee receiver
    // 2. devirtual to direct + inline: check devirtual record and get count
    ciKlass* callee = ((ciMethod*)get_ci_meta(_callee_idx))->holder();
    guarantee(profile->is_VirtualCallData(), "should be");
    ciVirtualCallData* virutal_data = (ciVirtualCallData*)profile;
    for (uint i = 0; i < virutal_data->row_limit(); i++) {
      if (virutal_data->receiver(i) == callee) {
        return -((int)virutal_data->receiver_count(i)) * cost_not_inline;
      }
    }
  }
  // no matching
  return 0;
}
bool OptRecordInline::equal(OptRecord* other) {
  if (other->opt_type() != OptRecord::Inline) {
    return false;
  }
  OptRecordInline* o = (OptRecordInline*)other;
  return o->_bci == _bci && o->_method_idx == _method_idx && o->_callee_idx == _callee_idx;
}

void OptRecordInline::print_on_with_indent(outputStream* out, int indent) {
  assert(_method_idx >= 0, "must be");
  assert(_callee_idx >= 0, "must be");
  print_indent(out, indent);
  out->print_cr("Inline at method=%s bci=%d callee: %s", get_meta_name(_method_idx), _bci, get_meta_name(_callee_idx));
}

void OptConstantReplace::read_from_stream(CompressedReadStream* in) {
  _klass_idx    = in->read_short();
  _field_offset = in->read_int();
  _field_type   = in->read_short();
  _field_val    = in->read_long();
}

void OptConstantReplace::write_to_stream(CompressedWriteStream* out) {
  out->write_byte(ConstantReplace);
  out->write_short(_klass_idx);
  out->write_int(_field_offset);
  out->write_short(_field_type);
  out->write_long(_field_val);
}

void OptConstantReplace::print_on_with_indent(outputStream* out, int indent) {
  assert(_klass_idx >= 0, "must be");
  print_indent(out, indent);
  out->print_cr("ConstantReplace at klass: %s field offset: %d",
                get_meta_name(_klass_idx), _field_offset);
}

void OptConstantReplace::nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest) {
  _klass_idx = dest->at(_klass_idx - 1);
}

bool OptConstantReplace::equal(OptRecord* other) {
  if (other->opt_type() != OptRecord::ConstantReplace) {
    return false;
  }
  OptConstantReplace* o = (OptConstantReplace*)other;
  return o->_klass_idx == _klass_idx && o->_field_offset == _field_offset && o->_field_val == _field_val;
}

size_t OptConstantReplace::estimate_size_in_bytes() {
  return 1 + 2 + 4 + 2 + 8;
}

bool OptConstantReplace::get_field_val(ciConstant field_const, jlong &field_val) {
  jfloat f_val = 0;
  jint i_val = 0;
  jdouble d_val = 0;
  switch (field_const.basic_type()) {
    case T_BOOLEAN:
    case T_CHAR:
    case T_BYTE:
    case T_SHORT:
    case T_INT:
        field_val = field_const.as_int();
        break;
    case T_LONG:
        field_val = field_const.as_long();
        break;
    case T_FLOAT:
        f_val = field_const.as_float();
        i_val = *(jint*)&f_val;
        field_val = i_val;
        break;
    case T_DOUBLE:
        d_val = field_const.as_double();
        field_val = *(jlong*)&d_val;
        break;
    default:
        return false;
  }
  return true;
}

int OptConstantReplace::calc_opt_score() {
  assert(_klass_idx >= 1 && _field_offset >= 1, "must be");
  ciInstanceKlass* expected_kls = (ciInstanceKlass*)get_ci_meta(_klass_idx);
  guarantee(expected_kls != NULL, "should be");
  ciField* expected_fld = expected_kls->get_field_by_offset(_field_offset, true);
  guarantee(expected_fld != NULL, "should be");
  ciConstant expected_const = expected_fld->constant_value();
  jshort field_type = (jshort)expected_const.basic_type();

  jlong field_val = 0;
  bool field_status = get_field_val(expected_const, field_val);
  if (field_status == false) {
    CR_LOG(cr_restore, cr_fail, "Fail for get_field_val, illegal field_type: %d\n", field_type);
    return max_jint;
  }

  int result = 0;
  if (field_type == _field_type && field_val == _field_val) {
    result = -1;
  } else {
    CR_LOG(cr_restore, cr_trace, "Fail for field consistency check, csa _field_type: %d, current field_type: %d\n", _field_type, field_type);
    CR_LOG(cr_restore, cr_trace, "csa _field_val: " JLONG_FORMAT ", current field_val: " JLONG_FORMAT "\n", _field_val, field_val);
    return max_jint;
  }
  return result;
}

int OptRecord::compare_meta_name(int idx1, int idx2) {
  char* name1 = get_meta_name(idx1);
  char* name2 = get_meta_name(idx2);
  return strcmp(name1, name2);
}
/*
 * Sort OptRecords for quick finding same/diff opts records
 * 1. by caller method
 * 2. by bci
 * profilereciver and if should always same in same caller and bci
 */
int OptRecord::compare_by_type_name(OptRecord* other) {
  if (_opt_type != other->_opt_type) {
    return (int)_opt_type - (int)other->_opt_type;
  }
  int res = compare_meta_name(_method_idx, other->_method_idx);
  return res != 0 ? res : (_bci - other->_bci);
}

int OptRecordDeVirtual::compare_by_type_name(OptRecord* other) {
  int res = OptRecord::compare_by_type_name(other);
  if (res != 0) {
    return res;
  }
  OptRecordDeVirtual* other_devritual = (OptRecordDeVirtual*)other;
  if (_morphism != other_devritual->_morphism) {
    return _morphism - other_devritual->_morphism;
  }
  if (_miss_is_trap != other_devritual->_miss_is_trap) {
    return _miss_is_trap ? - 1 : 1;
  }
  return 0;
}

int OptRecordInline::compare_by_type_name(OptRecord* other) {
  int res = OptRecord::compare_by_type_name(other);
  OptRecordInline* other_inline = (OptRecordInline*)other;
  return res != 0 ? res : compare_meta_name(_callee_idx, other_inline->_callee_idx);
}

int OptConstantReplace::compare_by_type_name(OptRecord* other) {
  // check if OptRecord is of type ConstantReplace
  if (_opt_type != other->opt_type()) {
    return (int)_opt_type - (int)other->opt_type();
  }

  OptConstantReplace* other_constant = (OptConstantReplace*)other;
  if (_field_offset != other_constant->_field_offset) {
    return _field_offset - other_constant->_field_offset;
  }
  if (_field_type != other_constant->_field_type) {
    return _field_type - other_constant->_field_type;
  }
  if (_field_val != other_constant->_field_val) {
    return _field_val - other_constant->_field_val;
  }
  int res = compare_meta_name(_klass_idx, other_constant->_klass_idx);
  return res != 0 ? res : 0;
}
