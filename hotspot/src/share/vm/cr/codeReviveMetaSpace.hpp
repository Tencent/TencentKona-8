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
 * -- array for identity of metadatas
 *    -- identity for each meta
 * -- array for loader type of metadatas
 *    -- loader type for each meta
 * -- string for method/klass
 *    -- klass
 *       -- metadata name
 *    -- method
 *       -- metadata name
 *       -- len[3] for the name of method holder/method/signature
 *
 * In restore, the part of array for metadatas can be modified
 *   after the meta is resolved, check whether the identity in metaspace
 *   is the same as the value in instanceKlass
 *   1) if same, it points to resolved metadata
 *   2) if different, it points to NULL
 */
class Metadata;
class CodeReviveMergedMetaInfo;
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
  // used in merge stage to store the meta information
  CodeReviveMergedMetaInfo*       _meta_info;
  // used in save stage to store the Metadata and identity
  GrowableArray<Metadata*>* _used_metas;
  GrowableArray<int64_t>*   _used_meta_identities;
  GrowableArray<uint16_t>*  _used_meta_loader_types;

  CodeReviveMetaSpace();                         // create for save
  CodeReviveMetaSpace(char* start, char* limit); // create for restore
  CodeReviveMetaSpace(CodeReviveMergedMetaInfo* meta_info); // create for merge

  // save related
  void append_used_metadata(Metadata* m, bool with_identity, int64_t identity);
  int record_metadata(Metadata* m, bool with_identity = false, int64_t identity = 0);
  void save_metadatas(char* start, char* limit); // save meta into start position

  // query related
  GrowableArray<Metadata*>* used_metas() { return _used_metas; }
  GrowableArray<int64_t>*   used_meta_identities() { return _used_meta_identities; }

  char*    metadata_name(int index);
  int64_t  metadata_identity(int index);
  uint16_t metadata_loader_type(int index);

  // Set new metadata (m) to metaspace at certain index.
  // Return true if succeed.
  //        false if identity is different with that recorded at saving.
  // If parameter m is NULL, will return true always.
  bool set_metadata(int index, Metadata* m);
  // Get class name or resolved Klass* from metaspace at certain index.
  // Return true if
  //     (1) Class at index is unresolved, and name is set to k_name
  //     (2) Class is resolved, and Klass* is set to klass
  // Return false if class is resolved, but identity is different with that saved in metaspace.
  bool unresolved_name_or_klass(int32_t index, Klass** klass, char** k_name);
  // Get method name or resolved Method* from metaspace at certain index.
  // Return true if
  //     (1) Method at index is unresolved, name and length is set to name_parts and lens.
  //     (2) Method is resolved, and Method* is set to method
  // Return false if method is resolved, but identity is different with that saved in metaspace.
  bool unresolved_name_parts_or_method(int32_t index, Method** method, char* name_parts[], int32_t** lens);

  size_t size() {
    return _cur - _start;
  }
  int length();

  size_t estimate_size();

  // record all the meta name into global Meta info
  void record_metadata(CodeReviveMergedMetaInfo* global_metadata);

  bool is_resolved_meta(int index) {
    MetaInfo* content = (MetaInfo*)(((intptr_t*)_start) + 1);
    return is_resolved(content[index].metadata);
  }

  // utitlities
  void print();

 private:
  // metadata stored in MetaInfo has 3 status.
  // (1) unresolved.
  // (2) resolved and valid.
  // (3) resolved and invalid.
  // At begining, all metadatas are in unresolved status.
  // When a metadata is revived, if the identity matches that saved in metaspace, it turns
  //   to resolved and valid status, otherwise it turns to resolved and invalid status.
  // For a resolved and valid class/method, if it/its holder is redefined, it will turn
  //   to resolved and invalid status.
  struct MetaInfo {
    intptr_t metadata;
    int64_t  identity;
    uint16_t loader_type;
  };
  void save_str(char* str);
  inline char* unresolved_meta_name(intptr_t v) {
    return _start + (int)(v >> meta_unresovled_bits);
  }
  inline static bool is_valid(intptr_t v) {
    return v != 0;
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

  friend class CodeReviveMergedMetaInfo;
};
#endif // SHARE_VM_CODE_REVIVE_META_SPACE_HPP
