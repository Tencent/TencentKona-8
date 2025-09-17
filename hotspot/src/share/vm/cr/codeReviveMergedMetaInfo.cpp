/*
 *
 * Copyright (C) 2022 Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
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
#include "classfile/symbolTable.hpp"
#include "cr/codeReviveMergedMetaInfo.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "cr/revive.hpp"
#include "runtime/handles.inline.hpp"
#include "utilities/align.hpp"

CodeReviveMergedMetaInfo::CodeReviveMergedMetaInfo() :
  _start(NULL),
  _cur_pos(NULL),
  _header_size(0),
  _rs(),
  _vs() {
  _global_meta_pointers = NULL;
  _global_meta_identities = NULL;
  _global_meta_loader_types = NULL;
  _meta_is_method_or_not = NULL;
} 

bool CodeReviveMergedMetaInfo::initialize(Arena* arena) {
  _rs = ReservedSpace(CodeRevive::max_file_size());
  if (!_rs.is_reserved()) {
    return false;
  }
  _vs.initialize(_rs, CodeRevive::max_file_size());
  _cur_pos = _vs.low();
  _start = _cur_pos;
  // create these GrowableArray in Arena
  _global_meta_pointers = new (arena) GrowableArray<char*>(arena, 100, 0, NULL);
  _global_meta_identities = new (arena) GrowableArray<int64_t>(arena, 100, 0, 0);
  _global_meta_loader_types = new (arena) GrowableArray<uint16_t>(arena, 100, 0, 0);
  _meta_is_method_or_not = new (arena) GrowableArray<bool>(arena, 100, 0, 0);
  return true;
}

/*
 * save the meta data into CodeReviveFile
 */
bool CodeReviveMergedMetaInfo::save_metadatas(char* start) {
  char* cur = start;
  int count = _global_meta_pointers->length();

  // write header: count
  *((intptr_t*)cur) = count;
  cur += sizeof(intptr_t);

  // reserve array for metadata + identity + loader type
  CodeReviveMetaSpace::MetaInfo* content = (CodeReviveMetaSpace::MetaInfo*)cur;

  cur = (char*)(content + count);
  cur = align_up(cur, 4);

  _header_size = cur - start;

  // copy the meta name for virtual space
  memcpy(cur, _start, _cur_pos - _start);

   // emit content and identity
  for (int i = 0; i < count; i++) {
    bool is_method = _meta_is_method_or_not->at(i);
    if (is_method) {
      // LSB 1 means unresolved metadata name offset
      // use the second bit to mark method with 1
      content[i].metadata = ((intptr_t)(_header_size + (_global_meta_pointers->at(i) - _start)) << 
                             CodeReviveMetaSpace::meta_unresovled_shift) | 
                             CodeReviveMetaSpace::name_mask | CodeReviveMetaSpace::method_mask;
    } else {
      // LSB 1 means unresolved metadata name offset
      // use the second bit to mark class with 0
      content[i].metadata = ((intptr_t)(_header_size + (_global_meta_pointers->at(i) - _start)) << 
                             CodeReviveMetaSpace::meta_unresovled_shift) | 
                             CodeReviveMetaSpace::name_mask;
    }
    content[i].identity = _global_meta_identities->at(i);
    content[i].loader_type = _global_meta_loader_types->at(i);
  }

  return true;
}

/*
 * record meta information in CodeReviveMergedMetaInfo
 */
void CodeReviveMergedMetaInfo::record_metadata(char* name, int64_t identity, uint16_t loader_type,  bool is_method) {
  Thread* THREAD = Thread::current();
  size_t len = strlen(name);
  Symbol* name_symbol = SymbolTable::lookup(name, (int)len, THREAD);
  // check whether the method has been recorded with name + identity + loader type
  MergePhaseMetaEntry* entry = MergePhaseMetaTable::lookup_only(name_symbol, identity, loader_type);
  if (entry != NULL) {
    return;
  }
  // add the symbol into meta table
  entry = MergePhaseMetaTable::add_meta(name_symbol, identity, loader_type);

  // add meta information into growarray
  // name: use the pointer in virtual space
  _global_meta_pointers->append(_cur_pos);
  // identity
  _global_meta_identities->append(identity);
  // loader type
  _global_meta_loader_types->append(loader_type);
  // method or not
  _meta_is_method_or_not->append(is_method); 
  
  // store the name of meta into virtual space in CodeReviveMergedMetaInfo
  save_metadata(name, is_method, len);

  // set the index in global metas
  entry->set_index(_global_meta_pointers->length() - 1);
}

/*
 * copy the meta name from the origin metaspace
 */
void CodeReviveMergedMetaInfo::save_metadata(char* name, bool is_method, size_t len) {
  // save the name of meta
  save_str(name, len + 1);
  if (is_method) {
    // save the klass, method, signature length
    _cur_pos = (char*)align_up((intptr_t)_cur_pos, 4);
    int32_t* lens = (int32_t*)_cur_pos;
    int32_t* old_lens = (int32_t*)align_up((intptr_t)(name + len + 1), 4);
    lens[0] = old_lens[0];
    lens[1] = old_lens[1];
    lens[2] = old_lens[2];
    _cur_pos += 12;
  }
}

void CodeReviveMergedMetaInfo::save_str(char* str, size_t len) {
  memcpy(_cur_pos, str, len);
  _cur_pos += len;
}

int CodeReviveMergedMetaInfo::get_meta_index(char* name, int64_t identity, uint16_t loader_type) {
  Thread* THREAD = Thread::current();
  size_t len = strlen(name);
  Symbol* name_symbol = SymbolTable::lookup(name, (int)len, THREAD);
  // check whether the method has been recorded
  MergePhaseMetaEntry* entry = MergePhaseMetaTable::lookup_only(name_symbol, identity, loader_type);
  if (entry == NULL) {
    return -1;
  }
  return entry->global_index();
}

char* CodeReviveMergedMetaInfo::metadata_name(int index) {
  return _global_meta_pointers->at(index);
}

int64_t CodeReviveMergedMetaInfo::metadata_identity(int index) {
  return _global_meta_identities->at(index);
}

uint16_t CodeReviveMergedMetaInfo::metadata_loader_type(int index) {
  return _global_meta_loader_types->at(index);
}

size_t CodeReviveMergedMetaInfo::estimate_size() {
  int count = _global_meta_pointers->length();
  size_t size = 0;
  size += sizeof(intptr_t);
  // size of the array of MetaInfo
  size += sizeof(CodeReviveMetaSpace::MetaInfo) * count;
  // the size of meta name
  size += (_cur_pos - _start);
  return size;
}
