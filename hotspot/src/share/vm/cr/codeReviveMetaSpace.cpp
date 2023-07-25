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
#include "memory/resourceArea.hpp"
#include "utilities/align.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/codeReviveMergedMetaInfo.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/revive.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "classfile/systemDictionary.hpp"

CodeReviveMetaSpace::CodeReviveMetaSpace() {
  _start      = NULL;
  _limit      = NULL;
  _cur        = NULL;
  _alignment  = 8;
  _used_metas = new (ResourceObj::C_HEAP, mtInternal) GrowableArray<Metadata*>(1000, true); // heap allocate
  _used_meta_identities = new (ResourceObj::C_HEAP, mtInternal) GrowableArray<int64_t>(1000, true); // heap allocate
  _used_meta_loader_types = new (ResourceObj::C_HEAP, mtInternal) GrowableArray<uint16_t>(1000, true); // heap allocate
  _meta_info = NULL;
}

CodeReviveMetaSpace::CodeReviveMetaSpace(char* start, char* limit) {
  _start      = start;
  _limit      = limit;
  _cur        = limit;
  _alignment  = 8;
  _used_metas = NULL;
  _used_meta_identities = NULL;
  _used_meta_loader_types = NULL;
  _meta_info = NULL;
}

// used in merge stage, and use CodeReviveMergedMetaInfo to store the meta information
CodeReviveMetaSpace::CodeReviveMetaSpace(CodeReviveMergedMetaInfo* meta_info) {
  _start      = NULL;
  _limit      = NULL;
  _cur        = NULL;
  _alignment  = 8;
  _used_metas = NULL;
  _used_meta_identities = NULL;
  _used_meta_loader_types = NULL;
  _meta_info = meta_info;
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

  // reserver array for metadata + identity + loader type
  MetaInfo* content = (MetaInfo*)_cur;

  _cur = (char*)(content + count);
  _cur = align_up(_cur, 4);

  // emit content and string
  for (int i = 0; i < count; i++) {
    Metadata* m = _used_metas->at(i);
    if (m->is_method()) {
      // LSB 1 means unresolved metadata name offset
      // use the second bit to mark method with 1
      content[i].metadata = ((intptr_t)(_cur - _start) << meta_unresovled_shift) | name_mask | method_mask;
    } else {
      // LSB 1 means unresolved metadata name offset
      // use the second bit to mark class with 0
      content[i].metadata = ((intptr_t)(_cur - _start) << meta_unresovled_shift) | name_mask;
    }
    content[i].identity = _used_meta_identities->at(i);
    content[i].loader_type = _used_meta_loader_types->at(i);
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
  // size of the array of MetaInfo
  size += sizeof(MetaInfo) * count;
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

void CodeReviveMetaSpace::append_used_metadata(Metadata* m, bool with_identity, int64_t identity) {
  _used_metas->append(m);
  if (with_identity) {
    _used_meta_identities->append(identity);
  } else {
    _used_meta_identities->append(m->cr_identity());
  }
  // add loader type
  if (m->is_method()) {
    // for method, use the loader type of method holder
    Method* method = (Method*)m;
    _used_meta_loader_types->append(CodeRevive::loader_type(method->method_holder()->class_loader()));
  } else {
    // for class, use the loader type
    _used_meta_loader_types->append(CodeRevive::klass_loader_type((Klass*)m));
  }
}

int CodeReviveMetaSpace::record_metadata(Metadata* m, bool with_identity, int64_t identity) {
  guarantee(m->is_method() || m->is_klass(), "should be");
  int result = m->csa_meta_index();
  if (result == -1) {
    append_used_metadata(m, with_identity, identity);
    result = _used_metas->length() - 1;
    m->set_csa_meta_index(result);
  }
  return result;
}

char* CodeReviveMetaSpace::metadata_name(int index) {
  if (_start == NULL) {
    if (_meta_info != NULL) {
      // use in merge
      return _meta_info->metadata_name(index);
    } else {
      // use in save
      return cr_metadata_name(_used_metas->at(index));
    }
  }
  guarantee(_cur != NULL && _cur == _limit, "should be"); // must call after emit/map finish

  MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);
  intptr_t v = content[index].metadata;
  if (is_resolved(v)) { // resovled
    return cr_metadata_name((Method*)v);
  }
  return unresolved_meta_name(v);
}

int64_t CodeReviveMetaSpace::metadata_identity(int index) {
  if (_start == NULL) {
    if (_meta_info != NULL) {
      // use in merge
      return _meta_info->metadata_identity(index);
    } else {
      // use in save
      return _used_meta_identities->at(index);
    }
  }

  MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);
  return content[index].identity;
}

uint16_t CodeReviveMetaSpace::metadata_loader_type(int index) {
  if (_start == NULL) {
    if (_meta_info != NULL) {
      // use in merge
      return _meta_info->metadata_loader_type(index);
    } else {
      // use in save
      return _used_meta_loader_types->at(index);
    }
  }

  MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);
  return content[index].loader_type;
}

bool CodeReviveMetaSpace::set_metadata(int index, Metadata* m) {
  MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);
  if (m == NULL || m->cr_identity() != metadata_identity(index)) {
    content[index].metadata = 0;
    return m == NULL;
  }

  intptr_t v = content[index].metadata;
  if (is_resolved(v)) {
    guarantee(m == (Metadata*)v, "should be");
    return true;
  }
  content[index].metadata = (intptr_t)m;
  m->set_csa_meta_index(index);
  return true;
}

bool CodeReviveMetaSpace::unresolved_name_or_klass(int32_t index, Klass** klass, char** k_name) {
  MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);
  intptr_t v = content[index].metadata;
  if (!is_valid(v)) {
    return false;
  }

  if (is_resolved(v)) { // resovled
    Klass* k = (Klass*)v;
    if (k->cr_identity() == metadata_identity(index)) {
      *klass = k;
      return true;
    }
    CR_LOG(cr_restore, cr_fail, "Identity changed, set NULL to klass at index %d.\n", index);
    set_metadata(index, NULL);
    return false;
  }
  // unresolved
  *klass = NULL;
  *k_name = unresolved_meta_name(v);
  return true;
}

bool CodeReviveMetaSpace::unresolved_name_parts_or_method(int32_t index, Method** method,
                                                          char* name_parts[], int32_t** lens) {
  MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);
  intptr_t v = content[index].metadata;
  if (!is_valid(v)) {
    return false;
  }
  if (is_resolved(v)) { // resovled
    Method* m = (Method*)v;
    if (m->cr_identity() == metadata_identity(index)) {
      *method = m;
      return true;
    }
    CR_LOG(cr_restore, cr_fail, "Identity changed, set NULL to method at index %d.\n", index);
    set_metadata(index, NULL);
    return false;
  }
  // unresolved
  *method = NULL;
  char* name = unresolved_meta_name(v);
  *lens = (int32_t*)align_up((intptr_t)(name + strlen(name) + 1), 4);

  name_parts[0] = name;
  name_parts[1] = name_parts[0] + (*lens)[0] + 1;
  name_parts[2] = name_parts[1] + (*lens)[1];
  return true;
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
  MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);

  for (int i = 0; i < count; i++) {
    intptr_t v = content[i].metadata;
    if (!is_resolved(v)) {
      char* str = unresolved_meta_name(v);
      CodeRevive::out()->print_cr("\t%10d unresolved %s %s (" INT64_FORMAT ")", i, str, is_unresolved_class(v) ? "class" : "method", content[i].identity);
    } else {
      Metadata* m = ((Metadata*)v);
      CodeRevive::out()->print_cr("\t%10d   resovled %s %s (" INT64_FORMAT ")", i, cr_metadata_name(m), m->is_klass() ? "class" : "method", content[i].identity);
    }
  }
}

// record the meta information into merged meta info from the metaspace
void CodeReviveMetaSpace::record_metadata(CodeReviveMergedMetaInfo* global_metadata) {
  int count = 0;
  count = (int)(*((intptr_t*)_start));
  MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);
  // traverse all the meta in the metaspace
  for (int i = 0; i < count; i++) {
    intptr_t v = content[i].metadata;
    // the meta don't be resolved in merge
    guarantee(!is_resolved(v), "must be not resolved");
    char* name = unresolved_meta_name(v);
    // record the name, identity, loader type, method_or_not into global meta info
    global_metadata->record_metadata(name, content[i].identity, content[i].loader_type, is_unresolved_method(v));
  }
}
