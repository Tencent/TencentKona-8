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
#include "memory/resourceArea.hpp"
#include "utilities/align.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/revive.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "classfile/systemDictionary.hpp"

int CodeReviveMetaSpace::_resolve_method_count = 0;
int CodeReviveMetaSpace::_unresolve_method_count = 0;
int CodeReviveMetaSpace::_resolve_klass_count = 0;
int CodeReviveMetaSpace::_unresolve_klass_count = 0;


CodeReviveMetaSpace::CodeReviveMetaSpace(bool local) {
  _start      = NULL;
  _limit      = NULL;
  _cur        = NULL;
  _alignment  = 8;
  _used_metas = new (ResourceObj::C_HEAP, mtInternal) GrowableArray<Metadata*>(1000, true); // heap allocate
  _local      = local;
}

CodeReviveMetaSpace::CodeReviveMetaSpace(char* start, char* limit) {
  _start      = start;
  _limit      = limit;
  _cur        = limit;
  _alignment  = 8;
  _used_metas = NULL;
  _local      = false;
}

static char* cr_metadata_name(Metadata* m) {
  if (m->is_klass()) {
    return ((Klass*)m)->name()->as_C_string();
  } else {
    Method* m1 = (Method*)m;
    return Method::name_and_sig_as_C_string_all(m1->constants()->pool_holder(), m1->name(), m1->signature());
  }
}

void CodeReviveMetaSpace::save_str(char* str) {
  size_t len = strlen(str) + 1;
  guarantee(_cur + len <= _limit, "should be");
  memcpy(_cur, str, len);
  _cur += len;
}

void CodeReviveMetaSpace::save_metadatas(char* start, char* limit) {
  _start = start;
  _cur   = start;
  _limit = limit;
  guarantee(_used_metas != NULL, "should be");

  ResourceMark rm;
  int count = _used_metas->length();

  // write header: count
  *((intptr_t*)_cur) = count;
  _cur += sizeof(intptr_t);

  // reserver array for metadatas
  intptr_t* content = (intptr_t*)_cur;
  _cur = (char*)(content + count);

  // emit content and string
  for (int i = 0; i < count; i++) {
    Metadata* m = _used_metas->at(i);
    if (m->is_method()) {
      // LSB 1 means unresolved metadata name offset
      // use the second bit to mark method with 1
      content[i] = ((intptr_t)(_cur - _start) << meta_unresovled_shift) | name_mask | method_mask;
    } else {
      // LSB 1 means unresolved metadata name offset
      // use the second bit to mark class with 0
      content[i] = ((intptr_t)(_cur - _start) << meta_unresovled_shift) | name_mask;
    }
    char* name = cr_metadata_name(m);
    save_str(name);
    if (m->is_method()) {
      Klass* k = ((Method*)m)->method_holder();
      Symbol* m_name = ((Method*)m)->name();
      Symbol* s_name = ((Method*)m)->signature();

      _cur = (char*)align_up((intptr_t)_cur, 4);
      int32_t* lens = (int32_t*)_cur;
      lens[0] = ((Method*)m)->method_holder()->name()->utf8_length();
      lens[1] = ((Method*)m)->name()->utf8_length();
      lens[2] = ((Method*)m)->signature()->utf8_length();
      _cur += 12;
    }
  }
  _limit = _cur;
}

size_t CodeReviveMetaSpace::estimate_size() {
  ResourceMark rm;
  int count = _used_metas->length();
  size_t size = 0;
  size += sizeof(intptr_t);
  size += sizeof(intptr_t) * count;
  // emit content and string
  for (int i = 0; i < count; i++) {
    Metadata* m = _used_metas->at(i);
    char* name = cr_metadata_name(m);
    size += strlen(name) + 1;

    if (m->is_method()) {
      size += 4;
      size += 12;
    }
  }
  return size;
}

int CodeReviveMetaSpace::record_metadata(Metadata* m) {
  guarantee(m->is_method() || m->is_klass(), "should be");
  if (_local) {
    for (int i = 0; i < _used_metas->length(); i++) {
      if (m == _used_metas->at(i)) {
        return i;
      }
    }
    _used_metas->append(m);
    return _used_metas->length() - 1;
  }
  int result = m->csa_meta_index();
  if (result == -1) {
    _used_metas->append(m);
    result = _used_metas->length() - 1;
    m->set_csa_meta_index(result);
  }
  return result;
}

char* CodeReviveMetaSpace::metadata_name(int index) {
  if (_start == NULL) {
    return cr_metadata_name(_used_metas->at(index));
  }
  guarantee(_cur != NULL && _cur == _limit, "should be"); // must call after emit/map finish

  intptr_t* content = ((intptr_t*)_start) + 1;
  intptr_t v = content[index];
  if (is_resolved(v)) { // resovled
    return cr_metadata_name((Method*)v);
  }
  return unresolved_meta_name(v);
}

Klass* CodeReviveMetaSpace::unresolved_name_or_klass(int32_t index, char** k_name) {
  intptr_t* content = ((intptr_t*)_start) + 1;
  intptr_t v = content[index];
  if (is_resolved(v)) { // resovled
    return (Klass*)v;
  }
  // unresolved
  *k_name = unresolved_meta_name(v);
  return NULL;
}

Method* CodeReviveMetaSpace::unresolved_name_parts_or_method(int32_t index, char* name_parts[], int32_t** lens) {
  intptr_t* content = ((intptr_t*)_start) + 1;
  intptr_t v = content[index];
  if (is_resolved(v)) { // resovled
    return (Method*)v;
  }
  // unresolved
  char* name = unresolved_meta_name(v);
  *lens = (int32_t*)align_up((intptr_t)(name + strlen(name) + 1), 4);

  name_parts[0] = name;
  name_parts[1] = name_parts[0] + (*lens)[0] + 1;
  name_parts[2] = name_parts[1] + (*lens)[1];
  return NULL;
}

Metadata* CodeReviveMetaSpace::resolved_metadata_or_null(int32_t index) {
  intptr_t* content = ((intptr_t*)_start) + 1;
  intptr_t v = content[index];
  if (is_resolved(v)) {
    return (Metadata*)v;
  }
  return NULL;
}

int CodeReviveMetaSpace::length() {
  int count = 0;
  if (_start == NULL) {
    count = _used_metas->length();
  } else {
    guarantee(_cur != NULL && _cur == _limit, "should be"); // must call after emit/map finish
    count = (int)(*((intptr_t*)_start));
  }
  return count;
}

void CodeReviveMetaSpace::print() {
  ResourceMark rm;
  int count = length();
  CodeRevive::out()->print_cr("MetaSpace [%p, %p), size %d, count %d", _start, _cur, (int)(_cur - _start), count);
  if (_start == NULL) {
    for (int i = 0; i < count; i++) {
      CodeRevive::out()->print_cr("\t%10d   resovled %s", i, cr_metadata_name(_used_metas->at(i)));
    }
    return;
  }
  intptr_t* content = ((intptr_t*)_start) + 1;
  for (int i = 0; i < count; i++) {
    intptr_t v = content[i];
    if (!is_resolved(v)) {
      char* str = unresolved_meta_name(v);
      CodeRevive::out()->print_cr("\t%10d unresolved %s %s", i, str, is_unresolved_class(v) ? "class" : "method");
    } else {
      Metadata* m = ((Metadata*)v);
      CodeRevive::out()->print_cr("\t%10d   resovled %s %s", i, cr_metadata_name(m), m->is_klass() ? "class" : "method");
    }
  }
}

void CodeReviveMetaSpace::resolve_metadata(CodeReviveMetaSpace* global_metadata) {
  ResourceMark rm;
  HandleMark   hm;
  int count = 0;
  count = (int)(*((intptr_t*)_start));
  intptr_t* content = ((intptr_t*)_start) + 1;
  for (int i = 0; i < count; i++) {
    intptr_t v = content[i];
    guarantee(!is_resolved(v), "must be not resolved"); 
    if (is_unresolved_class(v)) {
      Klass* k = resolve_klass(i, global_metadata != NULL);
      if (k != NULL) {
        set_metadata(i, k);
        if (global_metadata != NULL) {
          global_metadata->record_metadata(k);
          _resolve_klass_count++;
        }       
      } else if (global_metadata != NULL) {
        _unresolve_klass_count++;
      }
    } else {
      Method* m = resolve_method(i, global_metadata != NULL);
      if (m != NULL) {
        set_metadata(i, m);
        if (global_metadata != NULL) {
          _resolve_method_count++;
          global_metadata->record_metadata(m);
        }
      } else if (global_metadata != NULL) {
        _unresolve_method_count++;
      }
    }
  }
  if (global_metadata != NULL && CodeRevive::is_log_on(cr_merge, cr_info)) {
    print();
  }
}

Klass* CodeReviveMetaSpace::resolve_klass(int32_t index, bool add_to_global) {
  Thread* THREAD = Thread::current();
  char* name = NULL;
  Klass* k = unresolved_name_or_klass(index, &name);
  guarantee(k == NULL, "must be not resolved");
  Symbol* class_name_symbol = SymbolTable::lookup(name, (int)strlen(name), THREAD);
  // check whether the klass has been resolved
  MergePhaseKlassResovleCacheEntry* entry = MergePhaseKlassResovleCacheTable::lookup_only(class_name_symbol);
  if (entry != NULL) {
    // klass can be NULL
    return entry->klass();
  }
  k = SystemDictionary::resolve_or_null(class_name_symbol, SystemDictionary::java_system_loader(), Handle(), THREAD);
  CLEAR_PENDING_EXCEPTION;
  if (add_to_global) {
    // Regardless of whether it is successful or not, 
    // add the result into klass resolve cache table
    MergePhaseKlassResovleCacheTable::add_klass(class_name_symbol, k);
  }
  if (k == NULL) {
    CR_LOG(cr_merge, cr_warning, "Fail to find klass with name %s\n", name);
    return NULL;
  }
  return k;
}

Method* CodeReviveMetaSpace::resolve_method(int32_t index, bool add_to_global) {
  Thread* THREAD = Thread::current();
  char* names[3];
  int32_t* lens;
  Method* m = unresolved_name_parts_or_method(index, names, &lens);
  guarantee(m == NULL, "must be not resolved");
  Symbol* class_name_symbol = SymbolTable::lookup(names[0], lens[0], THREAD);
  // check whether the klass has been resolved
  MergePhaseKlassResovleCacheEntry* entry = MergePhaseKlassResovleCacheTable::lookup_only(class_name_symbol);
  Klass* k = NULL;
  if (entry != NULL) {
    k = entry->klass();
    // if the klass is NULL, directly return NULL to avoid printing duplicate error messages
    if (k == NULL) {
      return NULL;
    }
  } else {
    k = SystemDictionary::resolve_or_null(class_name_symbol, SystemDictionary::java_system_loader(), Handle(), THREAD);
    if (add_to_global) {
      // add the result into global Klass table
      MergePhaseKlassResovleCacheTable::add_klass(class_name_symbol, k);
    }
  }
  CLEAR_PENDING_EXCEPTION;
  if (k == NULL) {
    CR_LOG(cr_merge, cr_warning, "Fail to find klass with name %s\n", names[0]);
    return NULL;
  }
  Symbol* method_name_symbol = SymbolTable::lookup(names[1], lens[1], THREAD);
  Symbol* sig_name_symbol = SymbolTable::lookup(names[2], lens[2], THREAD);
  m = k->lookup_method(method_name_symbol, sig_name_symbol);
  if (m == NULL) {
    CR_LOG(cr_merge, cr_warning, "Fail to find method with name %s\n", names[0]);
    return NULL;
  }
  return m;
}

void CodeReviveMetaSpace::print_resolve_info() {
  outputStream* output = CodeRevive::out();
  output->print_cr("Klass/Method resolve information: ");
  output->print_cr("  Resolved klass  = %d      \tunresolved klass  = %d", _resolve_klass_count, _unresolve_klass_count);
  output->print_cr("  Resolved method = %d      \tunresolved method = %d", _resolve_method_count, _unresolve_method_count);
}
