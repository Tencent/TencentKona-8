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
#include "ci/ciBaseObject.hpp"
#include "ci/ciMethod.hpp"
#include "cr/codeReviveDependencies.hpp"
#include "cr/codeReviveMerge.hpp"

Metadata* ReviveDependencies::metadata_at(int index) {
  if (index == 0) {
    return NULL;
  }
  guarantee(index <= _meta_array->length(), "must be");
  ciBaseObject* obj = _meta_array->at(index - 1);
  if (obj == NULL) {
    return NULL;
  }
  return obj->as_metadata()->constant_encoding();
}

oop ReviveDependencies::oop_at(int index) {
  guarantee(0, "ReviveDependencies::oop_at shouldn't be called.");
  return (oop) NULL;
}

/*
 * Sort by 1. type 2. args length 3. meta name or index
 */
int ReviveDepRecord::compare_by_type_name_or_index(ReviveDepRecord* other) {
  if (_type != other->_type) {
    return (int)_type - (int)other->_type;
  }
  // could be not same, some dependency use index 0 as implict, some use explict
  if (_args_meta_idxs->length() != other->_args_meta_idxs->length()) {
    return _args_meta_idxs->length() - other->_args_meta_idxs->length();
  }
  for (int i = 0; i < _args_meta_idxs->length(); i++) {
    int cur_index = _args_meta_idxs->at(i);
    int other_index = other->_args_meta_idxs->at(i);
    // index=0 means default ctx type, see Dependencies::encode_content_bytes.
    // Same null indexed depenedency record will always have the same null-indexed context klass.
    if ((cur_index != 0 && other_index == 0) || (cur_index == 0 && other_index != 0)) {
      guarantee(i < _args_meta_idxs->length() - 1, "must be");
      return cur_index - other_index;
    }
    if (cur_index == 0 && other_index == 0) {
      guarantee(i < _args_meta_idxs->length() - 1, "must be");
      continue;
    }
    assert(cur_index != 0 && other_index != 0, "must be");
    int res = 0;
    if (_meta_array_names != NULL) {
      res = strcmp(_meta_array_names->at(cur_index - 1), other->_meta_array_names->at(other_index - 1));
    } else {
      res = _meta_array_index->at(cur_index - 1) - other->_meta_array_index->at(other_index - 1);
    }
    if (res != 0) {
      return res;
    }
  }
  return 0;
}

ReviveDepRecord* ReviveDepRecord::duplicate_in_arena(Arena* arena, GrowableArray<int32_t>* meta_array_index) {
  ReviveDepRecord* dep = new (arena) ReviveDepRecord(_type, meta_array_index, arena);
  for (int i = 0; i < _args_meta_idxs->length(); i++) {
    dep->add_argument_idx(_args_meta_idxs->at(i));
  }
  return dep;
}

void ReviveDepRecord::print_on_with_indent(outputStream* out, int indent) {
  for (int i = 0; i < indent; i++) {
    out->print("  ");
  }
  out->print("%s", Dependencies::dep_name(_type));
  for (int i = 0; i < _args_meta_idxs->length(); i++) {
    int index = _args_meta_idxs->at(i);
    if (index == 0) {
      out->print(" %s", "null");
    } else {
      if (_meta_array_names != NULL) {
        out->print(" %s", _meta_array_names->at(_args_meta_idxs->at(i) - 1));
      } else {
        int32_t global_index = _meta_array_index->at(_args_meta_idxs->at(i) - 1);
        out->print(" %s", CodeReviveMerge::metadata_name(global_index));
      }
    }
  }
  out->cr();
}
