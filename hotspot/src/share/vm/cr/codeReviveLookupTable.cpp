/*
 * Copyright (C) 2022, 2023, Tencent. All rights reserved.
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
#include "classfile/altHashing.hpp"
#include "cr/codeReviveLookupTable.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/revive.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/align.hpp"

size_t CodeReviveLookupTable::_buckets_num = 10;

CodeReviveLookupTable::CodeReviveLookupTable(CodeReviveMetaSpace* meta_space, char* start, char* limit, bool save, uint64_t seed) {
  _meta_space = meta_space;
  _start = start;
  _limit = limit;
  _bucket = (Entry*)_start;
  _alignment = 8;
  if (save) {
    _seed = 0;
    _cur = _start + sizeof(Entry) * _buckets_num;
    memset(_bucket, 0, sizeof(Entry) * _buckets_num);
    for (size_t i = 0; i < _buckets_num; i++) {
      bucket_entry(i)->_meta_index = -1;
    }
  } else {
    _seed = seed;
    _cur = _limit;
  }
  assert(_cur <= _limit, "out of bound");
}

char* CodeReviveLookupTable::allocate(size_t size) {
  char* result = _cur;
  _cur += size;
  _cur = align_up(_cur, _alignment);
  assert(_cur <= _limit, "out of bound");
  return result;
}

/*
 * only_use_name = true:  search the method using name
 * only_use_name = false: search the method using name + identity + loader type
 */
CodeReviveLookupTable::Entry* CodeReviveLookupTable::find_entry(Method* m, bool only_use_name) {
  char* name = Method::name_and_sig_as_C_string_all(m->constants()->pool_holder(), m->name(), m->signature());
  if (only_use_name) {
    return find_entry(name);
  }
  int64_t identity = m->cr_identity();
  uint16_t loader_type = (uint16_t)CodeRevive::loader_type(m->method_holder()->class_loader());
  return find_entry(name, identity, loader_type);
}

// search the entry with name + identity + loader type
CodeReviveLookupTable::Entry* CodeReviveLookupTable::find_entry(char* name, int64_t identity, uint16_t loader_type) {
  size_t name_len = strlen(name);
  uint32_t hash = AltHashing::halfsiphash_32(_seed, (const uint8_t*)name, (int)name_len);

  // locate Entry
  Entry* e = bucket_entry(hash % _buckets_num);
  if (e->_meta_index == -1) {
    return NULL;
  }
  while (e != NULL) {
    if (e->_hash == hash) {
      char* e_name = _meta_space->metadata_name(e->_meta_index);
      int64_t e_identity = _meta_space->metadata_identity(e->_meta_index);
      uint16_t e_loader_type = _meta_space->metadata_loader_type(e->_meta_index);
      size_t e_name_len = e_name != NULL ? strlen(e_name) : 0;
      if (identity == e_identity && name_len == e_name_len && loader_type == e_loader_type
          &&strncmp(name, e_name, name_len) == 0) {
        return e;
      }
    }
    e = next(e);
  }
  return NULL;
}

// search the entry with name
CodeReviveLookupTable::Entry* CodeReviveLookupTable::find_entry(char* name) {
  size_t name_len = strlen(name);
  uint32_t hash = AltHashing::halfsiphash_32(_seed, (const uint8_t*)name, (int)name_len);

  // locate Entry
  Entry* e = bucket_entry(hash % _buckets_num);
  if (e->_meta_index == -1) {
    return NULL;
  }
  while (e != NULL) {
    if (e->_hash == hash) {
      char* e_name = _meta_space->metadata_name(e->_meta_index);
      size_t e_name_len = e_name != NULL ? strlen(e_name) : 0;
      if (name_len == e_name_len && strncmp(name, e_name, name_len) == 0) {
        return e;
      }
    }
    e = next(e);
  }
  return NULL;
}


void CodeReviveLookupTable::print_entry(Entry* e, const char* prefix) {
  CodeRevive::out()->print_cr("%sEntry: ofst %d, meta index %d, hash %u, code %d, next %d, name %s, identity " UINT64_FORMAT,
    prefix,
    (int)(((char*)e) - _start),
    e->_meta_index,
    e->_hash,
    e->_code_offset,
    e->_next_offset,
    _meta_space->metadata_name(e->_meta_index),
    _meta_space->metadata_identity(e->_meta_index));
}

void CodeReviveLookupTable::print() {
  CodeRevive::out()->print_cr("CodeReviveLookupTable [%p, %p), size %d", _start, _cur, (int)(_cur - _start));
  for (size_t i = 0; i < _buckets_num; i++) {
    Entry* cur = bucket_entry(i);
    if (cur->_meta_index == -1) {
      continue;
    }
    CodeRevive::out()->print_cr("bucket[%d]", (int)i);
    print_entry(cur, "    ");
    cur = next(cur);
    while (cur != NULL) {
      print_entry(cur, "    ");
      cur = next(cur);
    }
  }
}

/*
 * 1. compute hash for name
 * 2. allocate/use space for entry
 * 3. allocate space for name and copy
 * 4. initialize entry
 */
CodeReviveLookupTable::Entry* CodeReviveLookupTable::new_entry(Method* m, int meta_index, bool allow_dup) {
  char* name = Method::name_and_sig_as_C_string_all(m->constants()->pool_holder(), m->name(), m->signature());
  return new_entry(name, meta_index, allow_dup);
}

CodeReviveLookupTable::Entry* CodeReviveLookupTable::new_entry(const char* name, int meta_index, bool allow_dup) {
  Entry* entry = NULL;
  size_t name_len = strlen(name);
  uint32_t hash = AltHashing::halfsiphash_32(_seed, (const uint8_t*)name, (int)name_len);
  // locate Entry
  size_t index = hash % _buckets_num;
  CodeReviveLookupTable::Entry* head = bucket_entry(index);
  if (head->_meta_index == -1) {
    entry = head; // initialized -1
  } else {
    if (allow_dup) {
      // used during merge.
      entry = head;
      while (entry != NULL) {
        if (entry->_meta_index == meta_index) {
           return entry;
        }
        entry = next(entry);
      };
    }

    entry = (Entry*)allocate(sizeof(Entry));
    entry->_next_offset = head->_next_offset;
    head->_next_offset = (int)(((char*)entry) - _start);
  }
  entry->_code_offset = -1;   // align with CodeReviveCodeBlob _next_version_offset, means no code
  entry->_hash = hash;
  entry->_meta_index = meta_index;
  // print_entry(entry);
  return entry;
}

CodeReviveLookupTable::Entry* CodeReviveLookupTableIterator::find_next_bucket_entry(size_t index) {
  for (size_t i = index; i < CodeReviveLookupTable::buckets_num(); i++) {
    _cur = _t->bucket_entry(i);
    if (_cur->_meta_index == -1) {
      continue;
    }
    _bucket_index = i;
    return _cur;
  }
  return NULL;
}

CodeReviveLookupTable::Entry* CodeReviveLookupTableIterator::first() {
  return find_next_bucket_entry(0);
}

CodeReviveLookupTable::Entry* CodeReviveLookupTableIterator::next() {
  CodeReviveLookupTable::Entry* next = _t->next(_cur);
  if (next != NULL) {
    _cur = next;
    return next;
  }
  return find_next_bucket_entry(_bucket_index + 1);
}
