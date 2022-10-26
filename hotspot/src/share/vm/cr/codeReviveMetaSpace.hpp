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
#ifndef SHARE_VM_CODE_REVIVE_META_SPACE_HPP
#define SHARE_VM_CODE_REVIVE_META_SPACE_HPP
#include "memory/allocation.hpp"
#include "utilities/growableArray.hpp"

/*
 * MetaSpace stores used meta and symbol.
 * Format in CodeRevive File
 * -- item count
 * -- array for metadatas
 *    -- offset in string part
 * -- string for method/klass
 *    -- klass
 *       -- metadata name
 *    -- method
 *       -- metadata name
 *       -- len[3] for the name of method holder/method/signature
 *
 * In merge/restore, the part of array for metadatas can be modified
 *   after the meta is resolved, it points to resolved metadata
 */
class Metadata;
class CodeReviveMetaSpace : public CHeapObj<mtInternal> {
 public:
  enum { name_bits               = 1,
         meta_unresovled_bits    = 2,
         method_bits             = 1
  };
  enum { name_shift              = 0,
         method_shift            = 1,
         meta_unresovled_shift   = meta_unresovled_bits
  };
  enum { name_mask               = right_n_bits(name_bits),
         meta_unresovled_mask    = right_n_bits(meta_unresovled_bits),
         method_mask             = right_n_bits(method_bits) << method_shift
  };
  char*                     _start;
  char*                     _limit;
  char*                     _cur;
  intptr_t                  _alignment;
  GrowableArray<Metadata*>* _used_metas;
  bool                      _local;              // save do not change meta status

  CodeReviveMetaSpace(bool local);               // create for save
  CodeReviveMetaSpace(char* start, char* limit); // create for restore

  // save related
  int record_metadata(Metadata* m);
  void save_metadatas(char* start, char* limit); // save meta into start position

  // query related
  GrowableArray<Metadata*>* used_metas() { return _used_metas; }

  char* metadata_name(int index);
  Klass*  unresolved_name_or_klass(int32_t index, char** k_name);
  Method* unresolved_name_parts_or_method(int32_t index, char* name_parts[], int32_t** lens);
  Metadata* resolved_metadata_or_null(int32_t index);

  size_t size() {
    return _cur - _start;
  }
  int length();

  size_t estimate_size();

  void set_metadata(int index, Metadata* m) {
    intptr_t* content = ((intptr_t*)_start) + 1;
    if (is_resolved(content[index])) {
      guarantee(m == (Metadata*)content[index], "should be");
      return;
    }
    content[index] = (intptr_t)m;
  }

  void resolve_metadata(CodeReviveMetaSpace* global_metadata);

  bool is_resolved_meta(int index) {
    intptr_t* content = ((intptr_t*)_start) + 1;
    return is_resolved(content[index]);
  }

  // utitlities
  void print();

  static void print_resolve_info();

 private:
  static int _resolve_method_count;
  static int _unresolve_method_count;
  static int _resolve_klass_count;
  static int _unresolve_klass_count;

  void save_str(char* str);
  inline char* unresolved_meta_name(intptr_t v) {
    return _start + (int)(v >> meta_unresovled_bits);
  }
  inline static bool is_resolved(intptr_t v) {
    return (v & name_mask) != name_mask;
  }
  inline static bool is_unresolved_method(intptr_t v) {
    return (v & meta_unresovled_mask) == (name_mask | method_mask);
  }
  inline static bool is_unresolved_class(intptr_t v) {
    return (v & meta_unresovled_mask) == name_mask;
  }
  Klass* resolve_klass(int32_t index, bool add_to_global = false);
  Method* resolve_method(int32_t index, bool add_to_global = false);
};
#endif // SHARE_VM_CODE_REVIVE_META_SPACE_HPP
