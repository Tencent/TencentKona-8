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
#ifndef SHARE_VM_CR_CODE_REVIVE_LOOKUP_TABLE_HPP
#define SHARE_VM_CR_CODE_REVIVE_LOOKUP_TABLE_HPP
#include "code/codeBlob.hpp"
#include "memory/allocation.hpp"

/*
 * Lookup table is mapping name to saved code information
 *
 * -----------
 * _bucket[_buckets_num]
 * // misc name and entries
 * method_name
 * method_name
 * Entry
 * method_name
 * Entry
 * method_name
 * method_name
 * method_name
 * ...
 * Entry
 * method_name
 * -----------
 *
 * Lookup table Format in CodeRevive File
 * -- _bucket[_buckets_num]
 *    -- Entry
 *       -- index in CodeReviveMetaspace 
 *       -- hash value for the bucket
 *       -- next entry offset with the same hash
 *       -- code offset in CodeReviveCodeSpace
 *    -- ...
 * -- Entry
 * -- Entry
 *    ...
 * -- Entry 
 */
class CodeReviveMetaSpace;
class CodeReviveLookupTable : public CHeapObj<mtInternal> {
 public:
  struct Entry {
    int32_t  _meta_index;  // index into CodeReviveMetaSpace::used_metas()
    uint32_t _hash;        // saved hash for method name
    int32_t  _next_offset; // next entry's offset in table space, 0 means no next Entry
    int32_t  _code_offset; // code offset in code section
  };

 private:
  CodeReviveMetaSpace*     _meta_space;
  char*                    _start;
  char*                    _limit;
  char*                    _cur;
  Entry*                   _bucket;
  uint64_t                 _seed;   // used for table mapping hash
  intptr_t                 _alignment;
  // static fields
  static size_t            _buckets_num;
 public:
  CodeReviveLookupTable(CodeReviveMetaSpace* meta_space, char* start, char* limit, bool save, uint64_t seed=0);
  Entry* bucket_entry(size_t index) {
    return _bucket + index;
  }
  Entry* new_entry(Method* m, int meta_index, bool allow_dup = false);
  Entry* new_entry(const char* name, int meta_index, bool allow_dup = false);
  Entry* find_entry(Method* m);
  Entry* find_entry(char* name);
  Entry* next(Entry* e) {
    if (e->_meta_index == -1 || e->_next_offset == 0) {
      return NULL;
    }
    return (Entry*)(_start + e->_next_offset);
  }
  void print_entry(Entry* e, const char* prefix);
  void print();
  size_t size() const { return _cur - _start; }
  static size_t buckets_num() { return _buckets_num; }
 private:
  char*  allocate(size_t size);
};


class CodeReviveLookupTableIterator VALUE_OBJ_CLASS_SPEC {
private:
  CodeReviveLookupTable*        _t;
  size_t                        _bucket_index;
  CodeReviveLookupTable::Entry* _cur;
  CodeReviveLookupTable::Entry* find_next_bucket_entry(size_t index);
public:
  CodeReviveLookupTableIterator(CodeReviveLookupTable* t) {
    _t = t;
    _bucket_index = 0;
    _cur = NULL;
  }

  CodeReviveLookupTable::Entry* first();
  CodeReviveLookupTable::Entry* next();
}; 
#endif // SHARE_VM_CR_CODE_REVIVE_LOOKUP_TABLE_HPP
