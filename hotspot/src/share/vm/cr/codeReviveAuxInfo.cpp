/*
 *
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include "precompiled.hpp"
#include "ci/ciUtilities.hpp"
#include "ci/ciObjArrayKlass.hpp"
#include "cr/codeReviveAuxInfo.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/codeReviveMerge.hpp"
#include "cr/revive.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/align.hpp"
#include "code/relocInfo.hpp"
#include "code/dependencies.hpp"
#include "oops/oop.hpp"
#include "oops/objArrayKlass.hpp"
#include "prims/jvm.h"
#include "classfile/javaClasses.hpp"
#include "runtime/handles.inline.hpp"
#include "compiler/disassembler.hpp"

static void skip_relocs_in_static_stub(RelocIterator& iter) {
  RelocIterator iter_static_stub = iter;
  // Relocations in static_stub
  //   mov %rbx, $0x0; none_type
  //                   static_stub_type
  //                   metadata_type
  //   jmpq xxxxxxxxx; runtime_call_type
  guarantee(iter_static_stub.next(), "should be");
  guarantee(iter_static_stub.type() == relocInfo::metadata_type, "should be");
  guarantee(iter_static_stub.reloc()->addr() == iter.reloc()->addr(), "should be");
  guarantee(iter_static_stub.next(), "should be");
  guarantee(iter_static_stub.type() == relocInfo::runtime_call_type, "should be");
  guarantee(iter_static_stub.reloc()->addr() == iter.reloc()->addr() + 10, "should be");
  iter.next(); // skip metadata reloc
  iter.next(); // skip runtime_call reloc
}

static bool is_non_oop(void* obj) {
  return obj == Universe::non_oop_word() || obj == NULL;
}

static oop get_loader(LoaderType type) {
  if (type == boot_loader) {
    return NULL;
  } else if (type == app_loader) {
    return SystemDictionary::java_system_loader();
  } else if (type == ext_loader) {
    return SystemDictionary::ext_loader();
  }
  guarantee(false, "unexpected");
  return NULL;
}

static void debug_print_nmethod(nmethod* nm) {
  HandleMark hm;
  nm->print_dependencies();
  nm->print_relocations();
  Disassembler::decode(nm);
}

char* ReviveAuxInfoTask::get_retral_address(int len) {
  return _cur - len;
}

void ReviveAuxInfoTask::emit_u2(uint16_t v) {
  assert(((intptr_t)_cur) % sizeof(uint16_t) == 0, "not align");
  *((uint16_t*)_cur) = v;
  _cur += sizeof(uint16_t);
}

void ReviveAuxInfoTask::emit_u4(uint32_t v) {
  assert(((intptr_t)_cur) % sizeof(uint32_t) == 0, "not align");
  *((uint32_t*)_cur) = v;
  _cur += sizeof(uint32_t);
}

uint16_t ReviveAuxInfoTask::read_u2() {
  assert(((intptr_t)_cur) % sizeof(uint16_t) == 0, "not align");
  uint16_t value = *((uint16_t*)_cur);
  _cur += sizeof(uint16_t);
  return value;
}

uint32_t ReviveAuxInfoTask::read_u4() {
  assert(((intptr_t)_cur) % sizeof(uint32_t) == 0, "not align");
  uint32_t value = *((uint32_t*)_cur);
  _cur += sizeof(uint32_t);
  return value;
}

char* ReviveAuxInfoTask::read_c_string() {
  char* str = _cur;
  _cur += (strlen(str) + 1);
  return str;
}

void ReviveAuxInfoTask::set_u4(uint32_t value, char* address) {
  assert(((intptr_t)address) % sizeof(uint32_t) == 0, "not align");
  *((uint32_t*)address) = value;
}

void ReviveAuxInfoTask::emit_c_string(const char* str, bool with_align) {
  size_t len = strlen(str) + 1;
  memcpy(_cur, str, len);
  _cur += len;
  if (with_align) {
    align();
  }
}

LoaderType ReviveAuxInfoTask::get_loader_type(Klass *k) {
  LoaderType type = CodeRevive::klass_loader_type(k);
  if (type == custom_loader) {
    fail();
  }
  return type;
}

void ReviveAuxInfoTask::align() {
  _cur = align_up(_cur, _alignment);
}

int ReviveAuxInfoTask::get_global_meta_index(int old_index) {
  guarantee(_meta_space != NULL, "must be");
  Metadata* metadata = _meta_space->resolved_metadata_or_null(old_index);
  guarantee(metadata != NULL && (metadata->is_method() || metadata->is_klass()),
            "Invalid unresolved metadata during update metadata index");

  int32_t new_index = metadata->csa_meta_index();
  guarantee(new_index >= 0, "should be");
  return new_index;
}

void ReviveAuxInfoTask::emit_global_symbol(CR_GLOBAL_KIND kind, uint32_t offset) {
  emit_u2(tag_vm_global);
  emit_u2((uint16_t)kind);
  emit_u4(offset);
  align();
}

// offset is value from nmethod contents begin to internal word address
void ReviveAuxInfoTask::emit_internal_word(uint32_t offset) {
  emit_u2(tag_internal_word);
  emit_u2(0); // u2 for align padding.
  emit_u4(offset);
  align();
}

void ReviveAuxInfoTask::emit_classloader(LoaderType type) {
  emit_u2((uint16_t)tag_oop_classloader);
  emit_u2((uint16_t)type);
  align();
}

void ReviveAuxInfoTask::emit_tag_only(RelocTypeTag tag) {
  emit_u2((uint16_t)tag);
  align();
}

bool ReviveAuxInfoTask::emit_string(oop o) {
  emit_u2(tag_oop_str);
  emit_u2(0);
  emit_c_string(java_lang_String::as_utf8_string(o), true);
  return true;
}

bool ReviveAuxInfoTask::emit_class(RelocTypeTag tag, Klass* k) {
  LoaderType type = get_loader_type(k);
  if (type == custom_loader) {
    return false;
  }
  emit_u2(tag);
  emit_u2((uint16_t)type);
  int meta_index = _meta_space->record_metadata(k);
  emit_u4(meta_index);
  if (k->oop_is_instance()) {
    emit_u4(((InstanceKlass*)k)->init_state());
  } else {
    emit_u4(0);
  }
  align();
  //emit_c_string(k->name()->as_C_string());
  return true;
}

bool ReviveAuxInfoTask::emit_method(RelocTypeTag tag, Method* m) {
  Klass* klass = m->method_holder();
  LoaderType type = get_loader_type(klass);
  if (type == custom_loader) {
    return false;
  }
  emit_u2((uint16_t)tag);
  emit_u2(type);
  int meta_index = _meta_space->record_metadata(m);
  emit_u4(meta_index);
  align();
  return true;
}

void ReviveAuxInfoTask::emit_int_index(RelocTypeTag tag, int index) {
  emit_u2((uint16_t)tag);
  emit_u2((uint16_t)0);
  emit_u4((uint32_t)index);
  align();
}

bool ReviveAuxInfoTask::iterate_global_oops() {
  CR_KnownOopKind index = get_index_of_global_oop(object());
  if (index != CR_KnownOop_invalid) {
    emit_int_index(tag_global_oop, index);
    return true;
  }
  return false;
}

bool ReviveAuxInfoTask::try_emit_classloader(oop o) {
  if (o == _nm->method()->method_holder()->class_loader()) {
    emit_classloader(method_holder_loader);
    return true;
  }
  return false;
}

bool ReviveAuxInfoTask::emit_oop(oop o, oop_Relocation* r) {
  if ((r != NULL) && (!r->oop_is_immediate())) {
    guarantee(r->value() == (address)_nm->oop_at(r->oop_index()), "should be");
    return true;
  }
  _object = o;
  bool result = iterate_global_oops();
  if (!result) {
    if (java_lang_String::is_instance(o)) {
      result = emit_string(o);
    } else if (java_lang_Class::is_instance(o)) {
      result = emit_class(tag_mirror_by_name_classloader, java_lang_Class::as_Klass(o));
    } else {
      result = try_emit_classloader(o);
    }
  }
  CR_LOG(cr_save, (result ? cr_info : cr_fail), "Emit %s for oop %p, %s\n",
         result ? "success" : "fail",
         _object,
         _object->klass()->name()->as_C_string());
  if (result == false) {
    fail();
  }
  return result;
}

bool ReviveAuxInfoTask::emit_meta(Metadata* m, metadata_Relocation* r) {
  if ((r != NULL) && (!r->metadata_is_immediate())) {
    guarantee(r->value() == (address)_nm->metadata_at(r->metadata_index()), "should be");
    return true;
  }
  _metadata = m;
  bool result = false;
  if (m->is_klass()) {
    Klass* k = (Klass*)m;
    result = emit_class(tag_klass_by_name_classloader, k);
    CR_LOG(cr_save, (result ? cr_info : cr_fail), "Emit %s for klass %p, %s status %d\n",
           result ? "success" : "fail",
           k,
           k->name()->as_C_string(),
           k->oop_is_instance() ? (int)(((InstanceKlass*)k)->init_state()) : 0);
  } else if (m->is_method()) {
    if (_nm->method() == m) {
      emit_tag_only(tag_meta_self_method);
      result = true;
    } else {
      result = emit_method(tag_method_by_name_classloader, (Method*)m);
    }
    CR_LOG(cr_save, (result ? cr_info : cr_fail), "Emit %s for method %p, %s\n",
           result ? "success" : "fail",
           m,
           ((Method*)m)->name_and_sig_as_C_string());
  } else {
    guarantee(false, "unsupported");
  }
  if (result == false) {
    fail();
  }
  return result;
}

void ReviveAuxInfoTask::emit_meta_array() {
  // save metadatas array in data section
  // Processing metadata array
  for (Metadata** meta_ptr = _nm->metadata_begin(); meta_ptr < _nm->metadata_end(); meta_ptr++) {
    Metadata* m = *meta_ptr;
    if (is_non_oop((void*)m)) {
      emit_tag_only(tag_non_oop);
      continue;
    }
    emit_meta(m, NULL);
  }
  emit_tag_only(tag_end);
}

void ReviveAuxInfoTask::emit_oop_array() {
  // save oops & metadatas array in data section
  // Processing oop array
  for (oop* oop_ptr = _nm->oops_begin(); oop_ptr < _nm->oops_end(); oop_ptr++) {
    oopDesc* o = *oop_ptr;
    if (is_non_oop((void*)o)) {
      emit_tag_only(tag_non_oop);
      continue;
    }
    emit_oop(o, NULL);
  }
  emit_tag_only(tag_end);
}

class EmitRelocClosure : public RelocClosure {
private:
  ReviveAuxInfoTask* _task;
public:
  EmitRelocClosure(ReviveAuxInfoTask* task): _task(task) {
  }
  void do_reloc_oop_type(RelocIterator* iter, oop_Relocation* r) {
    oopDesc* o = r->oop_value();
    _task->emit_oop(o, r);
  }
  void do_reloc_metadata_type(RelocIterator* iter, metadata_Relocation* r) {
    Metadata* m = r->metadata_value();
    _task->emit_meta(m, r);
  }
  void do_reloc_data_type(RelocIterator* iter, DataRelocation* r) {
    address destination = r->value();
    bool is_external = r->type() == relocInfo::external_word_type;
    if (destination == NULL) {
      CR_LOG(cr_save, cr_warning, "Emit fail for %s global destination is NULL at %p\n", is_external ? "external" : "runtime", r->addr());
      _task->fail();
      return;
    }
    size_t offset;
    CR_GLOBAL_KIND k = CodeRevive::vm_globals()->find(destination, &offset);
    if (k == CR_GLOBAL_NONE) {
      CR_LOG(cr_save, cr_warning, "Emit fail for %s global destination %p not found at %p\n",
             is_external ? "external" : "runtime",
             destination,
             r->addr());
      _task->fail();
      return;
    }
    CR_LOG(cr_save, cr_info, "Emit success for %s global destination %p %s\n",
           is_external ? "external" : "runtime",
           destination,
           CodeReviveVMGlobals::name(k));
    _task->emit_global_symbol(k,(uint32_t)offset);
  }
  void do_reloc_poll_type(RelocIterator* iter, Relocation* r) {
    if (reloc_should_revive(r)) {
      _task->emit_global_symbol(CR_OS_polling_page);
    } else {
      _task->emit_tag_only(tag_skip);
    }
  }
  void do_reloc_static_stub_type(RelocIterator* iter, static_stub_Relocation* r) {
    skip_relocs_in_static_stub(*iter);
    CR_LOG(cr_save, cr_info, "Skip static stub and external at %p\n", r->addr());
  }
  void do_reloc_internal_word_type(RelocIterator* iter, DataRelocation* r) {
    // internal word usually reocrds swtich table entries in consts section
    // section word usually records switch table header in consts section
    guarantee(r->value() >= _task->_nm->content_begin(), "should be");
    uint32_t offset = r->value() - _task->_nm->content_begin();
    CR_LOG(cr_save, cr_info, "Emit internal/section word %p %p offset from contents %u\n", r->value(), r->addr(), offset);
    _task->emit_internal_word(offset);
  }
private:
  bool reloc_should_revive(Relocation* r) {
    assert(r->type() == relocInfo::poll_type || r->type() == relocInfo::poll_return_type, "only poll");
#if defined(X86) && !defined(ZERO)
    if (!Assembler::is_polling_page_far() || nativeInstruction_at(r->addr())->is_mov_literal64()) {
      return true;
    }
#endif
    return false;
  }
};

void ReviveAuxInfoTask::emit() {
  EmitRelocClosure cl(this);
  relocations_do(&cl);
  emit_tag_only(tag_end);
}

void ReviveAuxInfoTask::fail() {
  _success = false;
  if (CodeRevive::is_fatal_on_fail()) {
    guarantee(_method != NULL, "should be");
    tty->print_cr("Emit/Revive failed on method %s", _method->name_and_sig_as_C_string());
    if (_nm != NULL) {
      debug_print_nmethod(_nm);
    }
    fatal("fatal_on_fail: Emit/Revive");
  }
}

CR_KnownOopKind ReviveAuxInfoTask::get_index_of_global_oop(oop obj) {
  #define CR_KNOWN_OOP_FUNC(name, func) \
    if (obj == func()) { \
      return CR_KnownOop_##name; \
    }
  CR_KNOWN_OOP_DO(CR_KNOWN_OOP_FUNC)
  #undef CR_KNOWN_OOP_FUNC

  #define WK_KLASS_ENUM(name, i1, i2) \
    if (obj == SystemDictionary::name()->java_mirror()) { \
      return CR_KnownOop_##name; \
    }
  WK_KLASSES_DO(WK_KLASS_ENUM)
  #undef WK_KLASS_ENUM

  jobject ci_env_known_obj = ciEnv::_ArrayIndexOutOfBoundsException_handle;
  if (ci_env_known_obj != NULL && obj == JNIHandles::resolve(ci_env_known_obj)) {
    return CR_KnownOop_ArrayIndexOutOfBoundsException;
  }
  ci_env_known_obj = ciEnv::_ArrayStoreException_handle;
  if (ci_env_known_obj != NULL && obj == JNIHandles::resolve(ci_env_known_obj)) {
    return CR_KnownOop_ArrayStoreException;
  }
  ci_env_known_obj = ciEnv::_ClassCastException_handle;
  if (ci_env_known_obj != NULL && obj == JNIHandles::resolve(ci_env_known_obj)) {
    return CR_KnownOop_ClassCastException;
  }
  if (obj == SystemDictionary::java_system_loader()) {
    return CR_KnownOop_AppLoader;
  }
  if (obj == SystemDictionary::ext_loader()) {
    return CR_KnownOop_ExtLoader;
  }

  // zero means not found
  return CR_KnownOop_invalid;
}

oop ReviveAuxInfoTask::get_global_oop(CR_KnownOopKind kind) {
  switch (kind) {
    #define CR_KNOWN_OOP_FUNC(name, func) \
      case CR_KnownOop_##name: return func();
    CR_KNOWN_OOP_DO(CR_KNOWN_OOP_FUNC)
    #undef CR_KNOWN_OOP_FUNC

    #define WK_KLASS_ENUM(name, i1, i2) \
      case CR_KnownOop_##name: return SystemDictionary::name()->java_mirror();
    WK_KLASSES_DO(WK_KLASS_ENUM)
    #undef WK_KLASS_ENUM

    case CR_KnownOop_ArrayIndexOutOfBoundsException:
    case CR_KnownOop_ArrayStoreException:
    case CR_KnownOop_ClassCastException: {
      return get_exception_oop(kind);
    }
    case CR_KnownOop_AppLoader: {
      return SystemDictionary::java_system_loader();
    }
    case CR_KnownOop_ExtLoader: {
      return SystemDictionary::ext_loader();
    }
    default:
      assert(false, "must exists");
  }
  return NULL;
}

oop ReviveAuxInfoTask::get_exception_oop(CR_KnownOopKind kind) {
  ciInstance* ex_obj = NULL;
  switch (kind) {
    case CR_KnownOop_ArrayIndexOutOfBoundsException: {
      ex_obj = CURRENT_ENV->ArrayIndexOutOfBoundsException_instance();
      break;
    }
    case CR_KnownOop_ArrayStoreException: {
      ex_obj = CURRENT_ENV->ArrayStoreException_instance();
      break;
    }
    case CR_KnownOop_ClassCastException: {
      ex_obj = CURRENT_ENV->ClassCastException_instance();
      break;
    }
    default:
      assert(false, "must exists");
  }
  if (CURRENT_ENV->failing()) {
    CR_LOG(cr_restore, cr_fail, "Fail to get global exception oop %d with reason %s\n", kind, CURRENT_ENV->failure_reason());
    return NULL;
  }
  if (ex_obj != NULL) {
    return ex_obj->get_oop();
  }
  CR_LOG(cr_restore, cr_fail, "Fail to get global exception oop %d\n", kind);
  return NULL;
}

class ReviveRelocClosure : public RelocClosure {
private:
  ReviveAuxInfoTask* _task;
  GrowableArray<ciBaseObject*>* _oops_metas;
  int _index;
public:
  ReviveRelocClosure(ReviveAuxInfoTask* task, GrowableArray<ciBaseObject*>* oops_metas):
    _task(task),_oops_metas(oops_metas), _index(0) {
  }
  void do_reloc_oop_type(RelocIterator* iter, oop_Relocation* r) {
    _task->revive_oop(r, _oops_metas, _index);
  }
  void do_reloc_metadata_type(RelocIterator* iter, metadata_Relocation* r) {
    _task->revive_meta(r, _oops_metas, _index);
  }
  void do_reloc_data_type(RelocIterator* iter, DataRelocation* r) {
    _task->revive_global_symbol(r);
  }
  void do_reloc_poll_type(RelocIterator* iter, Relocation* r) {
    _task->revive_polling_symbol(r);
  }
  void do_reloc_call_type(RelocIterator* iter, CallRelocation* r) {
    relocInfo::relocType type = r->type();
    if (type == relocInfo::virtual_call_type) {
      _task->revive_virtual_call(r);
    } else if (type == relocInfo::opt_virtual_call_type) {
      _task->revive_opt_virtual_call(r);
    } else {
      _task->revive_static_call_type(r);
    }
  }
  void do_reloc_static_stub_type(RelocIterator* iter, static_stub_Relocation* r) {
    _task->revive_static_stub_type(*iter);
  }
  void do_reloc_internal_word_type(RelocIterator* iter, DataRelocation* r) {
    _task->revive_internal_word(r);
  }
};

void ReviveAuxInfoTask::revive_data_array(GrowableArray<ciBaseObject*>* meta_array, GrowableArray<ciBaseObject*>* oop_array) {
  oop* oops = _nm->oops_begin();
  Metadata** metas = _nm->metadata_begin();
  int32_t oop_num = _nm->oops_end() - _nm->oops_begin();
  int32_t meta_num = _nm->metadata_end() - _nm->metadata_begin();

  for (int32_t i = 0; i < meta_num; i++) {
    if (is_non_oop(metas[i])) {
      continue;
    }
    metas[i] = meta_array->at(i)->as_metadata()->constant_encoding();
  }

  for (int32_t i = 0; i < oop_num; i++) {
    if (is_non_oop(oops[i])) {
      continue;
    }
    ciBaseObject* base_obj = oop_array->at(i);
    if ((uintptr_t)base_obj & 1) {
      uint32_t global_index = (uint32_t)(uintptr_t)base_obj;
      global_index = global_index >> 1;
      oops[i] = get_global_oop((CR_KnownOopKind)global_index);
    } else {
      oops[i] = JNIHandles::resolve(base_obj->as_object()->constant_encoding());
    }
  }
  guarantee(meta_num == meta_array->length(), "should be");
  guarantee(oop_num == oop_array->length(), "should be");
}

/*
 * Iterate relocations and aux buffer at same time, fix all kinds of relocations
 */
void ReviveAuxInfoTask::revive(const char* end, GrowableArray<ciBaseObject*>* oops_metas) {
  guarantee(JavaThread::current()->thread_state() == _thread_in_vm, "must be in vm state");
  ReviveRelocClosure cl(this, oops_metas);
  relocations_do(&cl);
  guarantee(read_u2() == tag_end, "");
  align();
}

/*
 * oop format
 * 1. tag_oop_mirror: get from oops_metas_index
 * 2. tag_oop_str: get from oops_metas_index
 * 3. tag_global_oop: resolve global
 */
bool ReviveAuxInfoTask::revive_oop(oop_Relocation* r, GrowableArray<ciBaseObject*>* oops_metas, int& oops_metas_index) {
  if (!r->oop_is_immediate()) {
    r->set_value((address)_nm->oop_at(r->oop_index()));
    return true;
  }
  RelocTypeTag tag = (RelocTypeTag)read_u2();
  LoaderType loader_type = (LoaderType)read_u2();
  oop loader = get_loader(loader_type);
  oop result = NULL;
  if (tag == tag_oop_str) {
    read_c_string();
    guarantee(oops_metas_index < oops_metas->length(), "");
    result = JNIHandles::resolve(oops_metas->at(oops_metas_index)->as_object()->constant_encoding());
    oops_metas_index++;
  } else if (tag == tag_global_oop) {
    CR_KnownOopKind global_index = (CR_KnownOopKind)read_u4();
    result = get_global_oop(global_index);
  } else if (tag == tag_mirror_by_name_classloader) {
    read_u4();
    read_u4();
    guarantee(oops_metas_index < oops_metas->length(), "");
    result = JNIHandles::resolve(oops_metas->at(oops_metas_index)->as_object()->constant_encoding());
    oops_metas_index++; 
  } else {
    guarantee(false, "unexpected type");
  }
  r->set_value((address)result);
  align();
  return true;
}

bool ReviveAuxInfoTask::revive_meta(metadata_Relocation* r, GrowableArray<ciBaseObject*>* oops_metas, int& oops_metas_index) {
  if (!r->metadata_is_immediate()) {
    r->set_value((address)_nm->metadata_at(r->metadata_index()));
    return true;
  }

  RelocTypeTag tag = (RelocTypeTag)read_u2();
  read_u2(); // loader type for klass and method, dummy u2 for global_klass
  Metadata* result = NULL;
  if (tag == tag_klass_by_name_classloader || tag == tag_method_by_name_classloader || tag == tag_meta_self_method) {
    read_u4();
    if (tag == tag_klass_by_name_classloader) {
      read_u4();
    }
    result = oops_metas->at(oops_metas_index)->as_metadata()->constant_encoding();
    oops_metas_index++;
  } else {
    guarantee(false, "unexpected type");
  }
  r->set_value((address)result);
  align();
  return true;
}

bool ReviveAuxInfoTask::revive_internal_word(Relocation* r) {
  RelocTypeTag tag = (RelocTypeTag)read_u2();
  read_u2(); // u2 for align padding
  uint32_t offset = read_u4();
  align();
  guarantee(tag == tag_internal_word, "");
  address addr = _nm->consts_begin() + offset;
  r->set_value(addr);
  CR_LOG(cr_restore, cr_info, "Revive internal/section %p at %p\n", addr, r->addr());
  return true;
}

bool ReviveAuxInfoTask::revive_global_symbol(Relocation* r) {
  RelocTypeTag tag = (RelocTypeTag)read_u2();
  CR_GLOBAL_KIND kind = (CR_GLOBAL_KIND)read_u2();
  int32_t offset = read_u4();
  align();
  guarantee(tag == tag_vm_global, "");
  guarantee(kind >= CR_GLOBAL_FIRST && kind <= CR_GLOBAL_LAST, "");
  address addr = CodeRevive::vm_globals()->find(kind, offset);
  r->set_value(addr);
  bool is_external = r->type() == relocInfo::external_word_type;
  CR_LOG(cr_restore, cr_info, "Revive %s %s at %p\n",
         is_external? "external" : "runtime",
         CodeRevive::vm_globals()->name(kind),
         r->addr());
  return true;
}

bool ReviveAuxInfoTask::revive_polling_symbol(Relocation* r) {
  // polling page has two type address
  // 1. if it is far (distance exceed 32 bit from code cache low/hight bound)
  //    or with option ForceUnreachable
  //    0x7ffff4ce22b9:      movabs $0x7fffff6b0000,%r10  poll reloc
  //    0x7ffff4ce22c3:      test   %eax,(%r10)
  // 2. otherwise, it encoding pc relative offset
  //    0x7ffff4ce0539:      test   %eax,0xa9cf701(%rip)  poll reloc
  //
  // new polling address might exceed pc relative limit, need record and check in saved file.
  RelocTypeTag tag = (RelocTypeTag)read_u2();
  if (tag == tag_skip) {
    align();
    CR_LOG(cr_restore, cr_info, "Revive polling at %p skipped\n", r->addr());
    return true;
  }
  CR_GLOBAL_KIND kind = (CR_GLOBAL_KIND)read_u2();
  int32_t offset = read_u4();
  align();
  guarantee(tag == tag_vm_global, "");
  guarantee(kind == CR_OS_polling_page, "");
  guarantee(offset == 0, "");
  r->set_value(os::get_polling_page());
  CR_LOG(cr_restore, cr_info, "Revive polling at %p\n", r->addr());
  return true;
}

bool ReviveAuxInfoTask::revive_virtual_call(Relocation* r) {
  address cached_value = ((virtual_call_Relocation*)r)->cached_value();
  NativeMovConstReg* mov = nativeMovConstReg_at(cached_value);
  mov->set_data((intptr_t)Universe::non_oop_word());
  CR_LOG(cr_restore, cr_info, "Revive virtual clear IC cache at %p\n", cached_value);
  r->set_value(SharedRuntime::get_resolve_virtual_call_stub());
  CR_LOG(cr_restore, cr_info, "Revive virtual call to SharedRuntime::get_resolve_virtual_call_stub at %p\n", r->addr());
  return true;
}

bool ReviveAuxInfoTask::revive_opt_virtual_call(Relocation* r) {
  r->set_value(SharedRuntime::get_resolve_opt_virtual_call_stub());
  CR_LOG(cr_restore, cr_info, "Revive opt virtual call to SharedRuntime::get_resolve_opt_virtual_call_stub at %p\n", r->addr());
  return true;
}

bool ReviveAuxInfoTask::revive_static_call_type(Relocation* r) {
  r->set_value(SharedRuntime::get_resolve_static_call_stub());
  CR_LOG(cr_restore, cr_info, "Revive static call to SharedRuntime::get_resolve_static_call_stub at %p\n", r->addr());
  return true;
}

bool ReviveAuxInfoTask::revive_static_stub_type(RelocIterator& iter) {
  // Relocations in static_stub
  //   mov %rbx, $0x0; none_type
  //                   static_stub_type
  //                   metadata_type
  //   jmpq xxxxxxxxx; runtime_call_type
  Relocation* static_stub = iter.reloc();
  // metadata: clear
  iter.next();
  Relocation* meta_stub = iter.reloc();
  guarantee(meta_stub->type() == relocInfo::metadata_type, "");
  meta_stub->set_value(0);
  CR_LOG(cr_restore, cr_info, "Revive static stub clear meta at %p\n", meta_stub->addr());
  // runtime_call_type: call to self
  iter.next();
  Relocation* runtime_call = iter.reloc();
  guarantee(runtime_call->type() == relocInfo::runtime_call_type, "");
  runtime_call->set_value(runtime_call->addr());
  CR_LOG(cr_restore, cr_info, "Revive static stub clear jump target at %p\n", runtime_call->addr());
  return true;
}

void ReviveAuxInfoTask::relocations_do(RelocClosure* cl) {
  RelocIterator iter(_nm);
  while (iter.next()) {
    relocInfo::relocType type = iter.type();
    Relocation* r = iter.reloc();
    switch(type) {
      case relocInfo::oop_type: {
        cl->do_reloc_oop_type(&iter, (oop_Relocation*)r);
        break;
      }
      case relocInfo::metadata_type: {
        cl->do_reloc_metadata_type(&iter, (metadata_Relocation*)r);
        break;
      }
      case relocInfo::external_word_type:
      case relocInfo::runtime_call_type: {
        cl->do_reloc_data_type(&iter, (DataRelocation*)r);
        break;
      }
      case relocInfo::poll_type:
      case relocInfo::poll_return_type: {
        cl->do_reloc_poll_type(&iter, r);
        break;
      }
      case relocInfo::virtual_call_type:
      case relocInfo::opt_virtual_call_type:
      case relocInfo::static_call_type: {
        cl->do_reloc_call_type(&iter, (CallRelocation*)r);
        break;
      }
      case relocInfo::static_stub_type: {
        cl->do_reloc_static_stub_type(&iter, (static_stub_Relocation*)r);
        break;
      }
      case relocInfo::internal_word_type:
      case relocInfo::section_word_type: {
        cl->do_reloc_internal_word_type(&iter, (DataRelocation*)r);
        break;
      }
      case relocInfo::none:
        break;
      default: {
        guarantee(false, "unexpected type");
      }
    }
  }
}

class EstimateRelocEmitSizeClosure : public RelocClosure {
private:
  size_t _size;
  size_t _alignment;
public:
  EstimateRelocEmitSizeClosure(size_t align): _alignment(align), _size(0) {
  }
  size_t size() const { return _size; }
  void do_reloc_oop_type(RelocIterator* iter, oop_Relocation* r) {
    // tag_oop_str: 2+2+4+4
    // tag_oop_mirror: 2+2+4+4+4
    // tag_global_oop: 2+2+4
    _size += 24;
  }
  void do_reloc_metadata_type(RelocIterator* iter, metadata_Relocation* r) {
    // tag_meta_class: 2+2+4+4
    // tag_meta_array_kls: 2+2+4+4
    // tag_klass_by_name_classloader: 2+2+4
    // tag_meta_method: 2+2+4+2+2+4
    // tag_global_klass: 2+2
    _size += 24;
  }
  void do_reloc_data_type(RelocIterator* iter, DataRelocation* r) {
    // tag_vm_global: 2 + 2 + align
    _size += align_up((size_t)4, _alignment);
  }
  void do_reloc_poll_type(RelocIterator* iter, Relocation* r) {
    // tag_vm_global: 2 + 2 + align
    _size += align_up((size_t)4, _alignment);;
  }
  void do_reloc_call_type(RelocIterator* iter, CallRelocation* r) {
    // no emit
  }
  void do_reloc_static_stub_type(RelocIterator* iter, static_stub_Relocation* r) {
    // no emit, need skip
    skip_relocs_in_static_stub(*iter);
  }
  void do_reloc_internal_word_type(RelocIterator* iter, DataRelocation* r) {
    // tag_internal_word: 2+2+4
    _size += align_up((size_t)8, _alignment);;
  }
};

/*
 * Iterate relocs and data array and estimate emit size
 */
size_t ReviveAuxInfoTask::estimate_emit_size() {
  guarantee(_nm != NULL && _cur == NULL, "should be");
  EstimateRelocEmitSizeClosure cl(_alignment);
  relocations_do(&cl);
  return cl.size();
}

void ReviveAuxInfoTask::iterate_reloc_aux_info() {
  while(iterate_condition()) {
    RelocTypeTag tag = (RelocTypeTag)read_u2();
    switch (tag) {
      case tag_vm_global: {
        uint16_t kind = read_u2();
        uint32_t offset = read_u4();
        process_vm_global(kind, offset);
        break;
      }
      case tag_oop_str: {
        uint16_t u2 = read_u2();
        char* str   = read_c_string();
        process_oop_str(u2, str);
        break;
      }
      case tag_meta_self_method: {
        process_meta_self_method();
        break;
      }
      case tag_klass_by_name_classloader: {
        LoaderType loader_type = (LoaderType)read_u2();
        int32_t meta_index = (int32_t)read_u4();
        int32_t min_state = (int32_t)read_u4();
        process_klass_by_name_classloader(loader_type, meta_index, min_state);
        break;
      }
      case tag_mirror_by_name_classloader: {
        LoaderType loader_type = (LoaderType)read_u2();
        int32_t meta_index = (int32_t)read_u4();
        int32_t min_state = (int32_t)read_u4();
        process_mirror_by_name_classloader(loader_type, meta_index, min_state);
        break;
      }
      case tag_method_by_name_classloader: {
        LoaderType loader_type = (LoaderType)read_u2();
        int32_t meta_index = (int32_t)read_u4();
        process_method_by_name_classloader(loader_type, meta_index);
        break;
      }
      case tag_meta_method_data: {
        guarantee(false, "FYI tag_meta_method_data");
        break;
      }
      case tag_internal_word: {
        read_u2(); // padding
        uint32_t u4 = read_u4();
        process_internal_word(u4);
        break;
      }
      case tag_oop_classloader: {
        LoaderType type = (LoaderType)read_u2();
        process_oop_classloader(type);
        break;
      }
      case tag_global_oop: {
        uint16_t u2 = read_u2();
        uint32_t global_index = read_u4();
        process_global_oop(u2, global_index);
        break;
      }
      case tag_skip: {
        process_skip();
        break;
      } 
      case tag_non_oop: {
        process_non_oop();
        break;
      }
      case tag_end: {
        return;
      }
      default: {
        guarantee(false, "unknown RelocTypeTag in iterate_reloc_aux_info");
      }
    }
    align();
  }
}

const char* class_state_name[] = {
  "allocated", "loaded", "linked", "being_initialized", "fully_initialized", "initialization_error"
};

bool PreReviveTask::check_instance_klass_state(InstanceKlass* k, int min_state) {
  int state = (int)k->init_state();
  if (state < min_state) {
    CR_LOG(cr_restore, cr_warning, "Unexpected state %s(%d) for klass %s, which should be at least %s(%d)\n",
           class_state_name[state],
           state,
           k->external_name(),
           class_state_name[min_state],
           min_state);
    fail();
    return false;
  }
  return true;
}

Klass* PreReviveTask::revive_get_klass(Symbol* name, oop loader, int min_state) {
  Klass* k = SystemDictionary::find_instance_or_array_klass(name, Handle(loader), Handle(), Thread::current());
  if (k == NULL) {
    CR_LOG(cr_restore, cr_warning, "Fail to find klass with name %s\n", name->as_C_string());
    fail();
  } else if (k->oop_is_instance()) {
    InstanceKlass* instance = (InstanceKlass*)k;

    // If the klass is pure interface klass, which means no java field and default methods,
    // 'linked' state is enough to ensure the safety.
    if (instance->is_interface() && instance->java_fields_count() == 0 && !instance->has_default_methods()) {
      if (instance->is_linked()) {
        CR_LOG(cr_restore, cr_info, "Revive interface klass %s at state %s(%d)\n",
               name->as_C_string(),
               class_state_name[((InstanceKlass*)k)->init_state()],
               ((InstanceKlass*)k)->init_state());
        return k;
      }
    }

    if (!check_instance_klass_state(instance, min_state)) {
      return NULL;
    }
  }
  return k;
}

Klass* PreReviveTask::revive_get_klass(const char* name, oop loader, int min_state) {
  uint32_t hash;
  Symbol* holder_symbol = SymbolTable::lookup_only(name, (int)strlen(name), hash);
  if (holder_symbol == NULL) {
    CR_LOG(cr_restore, cr_warning, "Miss klass symbol %s\n", name);
    fail();
    return NULL;
  }
  return revive_get_klass(holder_symbol, loader, min_state);
}

Klass* PreReviveTask::revive_get_klass(int32_t meta_index, oop loader, int min_state) {
  char* name = NULL;
  Klass* k = _meta_space->unresolved_name_or_klass(meta_index, &name);
  if (k != NULL) {
    guarantee(k->is_klass(), "should be");
    if (k->oop_is_instance() && !check_instance_klass_state((InstanceKlass*)k, min_state)) {
      return NULL;
    }
    return k;
  }
  // change to unresovle_name_or_method
  guarantee(name != NULL, "should be");
  k = revive_get_klass(name, loader, min_state);
  if (k != NULL) {
    _meta_space->set_metadata(meta_index, k);
  }
  return k;
}

Method* PreReviveTask::revive_get_method(int32_t meta_index, oop loader) {
  char* names[3];
  int32_t* lens;
  Method* m = _meta_space->unresolved_name_parts_or_method(meta_index, names, &lens);
  if (m != NULL) {
    return m;
  }
  uint32_t hash;
  Symbol* k_s = SymbolTable::lookup_only(names[0], lens[0], hash);
  if (k_s == NULL) {
    CR_LOG(cr_restore, cr_warning, "Miss klass symbol %s\n", names[0]);
    fail();
    return NULL;
  }
  Symbol* m_s = SymbolTable::lookup_only(names[1], lens[1], hash);
  if (m_s == NULL) {
    CR_LOG(cr_restore, cr_warning, "Miss method symbol %s\n", names[1]);
    fail();
    return NULL;
  }
  Symbol* s_s = SymbolTable::lookup_only(names[2], lens[2], hash);
  if (s_s == NULL) {
    CR_LOG(cr_restore, cr_warning, "Miss signature symbol %s\n", names[2]);
    fail();
    return NULL;
  }
  Klass* k = revive_get_klass(k_s, loader, InstanceKlass::loaded);
  if (k == NULL) {
    CR_LOG(cr_restore, cr_warning, "Miss klass %s\n", names[0]);
    fail();
    return NULL;
  }
  m = k->lookup_method(m_s, s_s);
  if (m == NULL) {
    CR_LOG(cr_restore, cr_warning, "Fail to find method for %s\n", names[0]);
    fail();
  } else {
    _meta_space->set_metadata(meta_index, m);
  }
  return m;
}

ciObject* PreReviveTask::prepare_oop_str(char* str) {
  guarantee(JavaThread::current()->thread_state() == _thread_in_vm, "must be in vm state");
  oop result = StringTable::intern(str, Thread::current());
  ciObject* ci_obj = CURRENT_ENV->get_object(result);
  return ci_obj;
}

ciMetadata* PreReviveTask::prepare_klass_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  guarantee(JavaThread::current()->thread_state() == _thread_in_vm, "must be in vm state");
  oop loader = get_loader(loader_type);
  Klass* k = revive_get_klass(meta_index, loader, min_state);
  if (k == NULL) {
    return NULL;    
  }

  ciKlass* result = CURRENT_ENV->get_klass(k);
  CR_LOG(cr_restore, cr_info, "Prepare found klass metadata %p %s\n",
         result->constant_encoding(),
         result->name()->as_quoted_ascii());
  return result;
}

ciObject* PreReviveTask::prepare_mirror_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  guarantee(JavaThread::current()->thread_state() == _thread_in_vm, "must be in vm state");
  ciKlass* k = (ciKlass*)prepare_klass_by_name_classloader(loader_type, meta_index, min_state);
  if (k == NULL) {
    fail();
    return NULL;    
  }

  ciInstance* result = k->java_mirror();
  CR_LOG(cr_restore, cr_info, "Prepare found mirror %p %s\n",
         result->constant_encoding(),
         k->name()->as_quoted_ascii());
  return result;
}

ciMetadata* PreReviveTask::prepare_method_by_name_classloader(LoaderType loader_type, int32_t meta_index) {
  guarantee(JavaThread::current()->thread_state() == _thread_in_vm, "must be in vm state");
  oop loader = get_loader(loader_type);
  Method* m = revive_get_method(meta_index, loader);
  if (m == NULL) {
    fail();
    return NULL;
  }

  ciMetadata* result = CURRENT_ENV->get_method(m);
  CR_LOG(cr_restore, cr_info, "Prepare found method by name classloader %p %s\n",
         result->constant_encoding(),
         m->name()->as_quoted_ascii());
  return result;
}

/*
 * Tterate reloc aux info and find oop & meta need resolutions. This must performed
 * out of create_nmethod because:
 * 1. Using CiEnv interface to query class same with C1/C2 compilation time.
 * 2. CiEnv interface can not work when holding codecache lock.
 *
 * Iterate oop & meta tags and find oop & meta and keep in ciObject and ciMetadata.
 * Because compile and load usually happens in native mode, need switch to vm state
 * in this method and keep result in CiObject to avoid GC and safepoint.
 *
 * For other tags type, content is skipped.
 */
GrowableArray<ciBaseObject*>* PreReviveTask::pre_revive_oop_and_meta(bool prepare_data_array, GrowableArray<uint32_t>* global_oop_array) {
  _oops_and_metas = new GrowableArray<ciBaseObject*>();
  _prepare_data_array = prepare_data_array;
  _global_oop_array = global_oop_array;
  GUARDED_VM_ENTRY(
    iterate_reloc_aux_info();
  );
  return _oops_and_metas;
}

void PreReviveTask::process_non_oop() {
  _oops_and_metas->append(NULL);
}

void PreReviveTask::process_oop_str(uint16_t u2, char* str) {
  _oops_and_metas->append(prepare_oop_str(str));
}

void PreReviveTask::process_meta_self_method() {
  _oops_and_metas->append(CURRENT_ENV->get_metadata(_method));
}

void PreReviveTask::process_klass_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  _oops_and_metas->append(prepare_klass_by_name_classloader(loader_type, meta_index, min_state));
}

void PreReviveTask::process_mirror_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  _oops_and_metas->append(prepare_mirror_by_name_classloader(loader_type, meta_index, min_state));
}

void PreReviveTask::process_method_by_name_classloader(LoaderType loader_type, int32_t meta_index) {
  _oops_and_metas->append(prepare_method_by_name_classloader(loader_type, meta_index));
}

void PreReviveTask::process_oop_classloader(LoaderType type) {
  if (_prepare_data_array) {
    guarantee(type == method_holder_loader, "should be");
    _oops_and_metas->append(CURRENT_ENV->get_object(_method->method_holder()->class_loader()));
  }
}

void PreReviveTask::process_global_oop(uint16_t u2, uint32_t global_index) {
  if (_prepare_data_array) {
    _oops_and_metas->append((ciBaseObject*)((uintptr_t)global_index << 1 | 1));
  }
  _global_oop_array->append(global_index);
}

void PrintAuxInfoTask::process_vm_global(uint16_t kind, uint32_t offset) {
  const char* symbol_name = CodeReviveVMGlobals::name((CR_GLOBAL_KIND)kind);
  _out->print_cr("global %s %u", symbol_name, offset);
}

void PrintAuxInfoTask::process_oop_str(uint16_t u2, char* str) {
  _out->print_cr("const string : %s", str);
}

void PrintAuxInfoTask::process_meta_self_method() {
  _out->print_cr("method self");
}

void PrintAuxInfoTask::process_klass_by_name_classloader(LoaderType type, int32_t meta_index, int32_t init_status) {
  _out->print_cr("klass by name and classloader : tag =%d, kls = %s, loader = %d, meta_index = %d, init_status %d",
                  tag_klass_by_name_classloader, _meta_space->metadata_name(meta_index), type, meta_index, init_status);
}

void PrintAuxInfoTask::process_mirror_by_name_classloader(LoaderType type, int32_t meta_index, int32_t init_status) {
  _out->print_cr("mirror by name and classloader : tag =%d, kls = %s, loader = %d, meta_index = %d, init_status %d",
                  tag_mirror_by_name_classloader, _meta_space->metadata_name(meta_index), type, meta_index, init_status);
}

void PrintAuxInfoTask::process_method_by_name_classloader(LoaderType type, int32_t meta_index) {
  _out->print_cr("method by name and classloader : tag =%d, method = %s, loader = %d, meta_index = %d",
                  tag_method_by_name_classloader, _meta_space->metadata_name(meta_index), type, meta_index);
}

void PrintAuxInfoTask::process_internal_word(uint32_t offset) {
  _out->print_cr("internal word offset %u ", offset);
}

void PrintAuxInfoTask::process_oop_classloader(LoaderType loader_type) {
  _out->print_cr("classloader : %d", loader_type);
}

void PrintAuxInfoTask::process_global_oop(uint16_t u2, uint32_t index) {
  _out->print_cr("global oop index %d", index);
}

void PrintAuxInfoTask::process_skip() {
  _out->print_cr("reloc skipped");
}

void PrintAuxInfoTask::process_non_oop() {
  _out->print_cr("non oop");
}

void CheckMetaResolveTask::process_klass_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  if (!_meta_space->is_resolved(meta_index)) {
    fail();
  }
}

void CheckMetaResolveTask::process_mirror_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  if (!_meta_space->is_resolved(meta_index)) {
    fail();
  }
}

void CheckMetaResolveTask::process_method_by_name_classloader(LoaderType loader_type, int32_t meta_index) {
  if (!_meta_space->is_resolved(meta_index)) {
    fail();
  }
}

int32_t UpdateMetaIndexTask::update_one_meta_index(int32_t old_index, char* index_address) {
  int32_t new_index = get_global_meta_index(old_index);
  set_u4(new_index, index_address);

  if (CodeRevive::is_log_on(cr_merge, cr_info)) {
    CR_LOG(cr_merge, cr_info, "Reset index from %d to %d in %s\n", old_index, new_index, CodeReviveMerge::metadata_name(new_index));
    guarantee(!strcmp(_meta_space->metadata_name(old_index), CodeReviveMerge::metadata_name(new_index)), "should be");
  }
  return old_index;
}

void UpdateMetaIndexTask::process_klass_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  char* index_address = get_retral_address(2 * sizeof(uint32_t));
  update_one_meta_index(meta_index, index_address); 
}

void UpdateMetaIndexTask::process_mirror_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  char* index_address = get_retral_address(2 * sizeof(uint32_t));
  update_one_meta_index(meta_index, index_address);
}

void UpdateMetaIndexTask::process_method_by_name_classloader(LoaderType loader_type, int32_t meta_index) {
  char* index_address = get_retral_address(sizeof(uint32_t)); 
  update_one_meta_index(meta_index, index_address);
}

GrowableArray<char*>* CollectMetadataArrayNameTask::collect_names() {
  _names = new GrowableArray<char*>();
  GUARDED_VM_ENTRY(
    iterate_reloc_aux_info();
  );
  return _names;
}

void CollectMetadataArrayNameTask::process_meta_self_method() {
  _names->append(_method_name);
}

void CollectMetadataArrayNameTask::process_klass_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  _names->append(_meta_space->metadata_name(meta_index));
}

void CollectMetadataArrayNameTask::process_method_by_name_classloader(LoaderType loader_type, int32_t meta_index) {
  _names->append(_meta_space->metadata_name(meta_index));
}

void CollectMetadataArrayNameTask::process_non_oop() {
  _names->append(NULL);
}

void CollectKlassAndMethodIndexTask::process_meta_self_method() {
  _indexes->append(_self_method);
}

void CollectKlassAndMethodIndexTask::process_klass_by_name_classloader(LoaderType loader_type, int32_t meta_index, int32_t min_state) {
  _indexes->append(get_global_meta_index(meta_index));
}

void CollectKlassAndMethodIndexTask::process_method_by_name_classloader(LoaderType loader_type, int32_t meta_index) {
  _indexes->append(get_global_meta_index(meta_index));
}

void CollectKlassAndMethodIndexTask::process_non_oop() {
  _indexes->append(-1);
}
