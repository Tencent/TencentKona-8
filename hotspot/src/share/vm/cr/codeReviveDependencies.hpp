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

#ifndef SHARE_VM_CR_CODE_REVIVE_DEPENDENCIES_HPP
#define SHARE_VM_CR_CODE_REVIVE_DEPENDENCIES_HPP

#include "code/dependencies.hpp"
#include "memory/allocation.hpp"
#include "utilities/growableArray.hpp"

/*
 * Avoid massive changes in Dependencies.hpp/cpp
 */
class ReviveDependencies : public StackObj {
 private:
  char* _dependencies_begin;
  int   _dependencies_size;
  GrowableArray<ciBaseObject*>* _meta_array;
 public:
  ReviveDependencies(char* begin, int size, GrowableArray<ciBaseObject*>* meta_array) :
    _dependencies_begin(begin), _dependencies_size(size),  _meta_array(meta_array) { }
  char*     dependencies_begin() { return _dependencies_begin; }
  int       dependencies_size()  { return _dependencies_size; }
  Metadata* metadata_at(int index);
  oop       oop_at(int index);
};

/*
 * Avoid massive changes in Dependencies.hpp/cpp.
 *
 * Decompressed dependency from stream and record in ReviveDepRecord.
 * Dependency print/check based on Metadata, ReviveDepRecord compare with name (or meta index later).
 * No check function here.
 */
class ReviveDepRecord : public ResourceObj {
 protected:
  Dependencies::DepType _type;
  GrowableArray<char*>* _meta_array_names;
  GrowableArray<int32_t>* _meta_array_index;
  GrowableArray<int>*   _args_meta_idxs; // maybe meta_space index later
 public:
  ReviveDepRecord(Dependencies::DepType type, GrowableArray<char*>* meta_array_names) :
    _type(type), _meta_array_names(meta_array_names), _meta_array_index(NULL) {
    _args_meta_idxs = new GrowableArray<int>();
  }
  ReviveDepRecord(Dependencies::DepType type, GrowableArray<int32_t>* meta_array_index) :
    _type(type), _meta_array_names(NULL), _meta_array_index(meta_array_index) {
    _args_meta_idxs = new GrowableArray<int>();
  }
  ReviveDepRecord(Dependencies::DepType type, GrowableArray<int32_t>* meta_array_index, Arena* arena) :
    _type(type), _meta_array_names(NULL), _meta_array_index(meta_array_index) {
    _args_meta_idxs = new (arena) GrowableArray<int>(arena, 5, 0, 0);
  }
  Dependencies::DepType type() const { return _type; }
  void add_argument_idx(int idx) {
    _args_meta_idxs->append(idx);
  }
  int  compare_by_type_name_or_index(ReviveDepRecord* other);
  void print_on(outputStream* out, int indent=0);
  GrowableArray<int32_t>* get_meta_array_index() { return _meta_array_index; }
  ReviveDepRecord* duplicate_in_arena(Arena* arena, GrowableArray<int32_t>* meta_array_index);
};

#endif // SHARE_VM_CR_CODE_REVIVE_DEPENDENCIES_HPP
