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
#ifndef SHARE_VM_CR_CODE_REVIVE_META_INFO_HPP
#define SHARE_VM_CR_CODE_REVIVE_META_INFO_HPP
#include "memory/allocation.hpp"
#include "runtime/virtualspace.hpp"
#include "utilities/growableArray.hpp"

/*
 * CodeReviveMergedMetaInfo contains:
 * 1. metadata name during merge which is the same as the data in MetaSpace
 * 2. the array of metadatas: offset in virtual space
 * 3. the array of meta identity
 *
 * Format in Virtual Space of CodeReviveMergedMetaInfo
 * -- string for method/klass
 *    -- klass
 *       -- metadata name
 *    -- method
 *       -- metadata name
 *       -- len[3] for the name of method holder/method/signature
 *
 * These CodeRevive runtime data structure points to file content in mapped file.
 *
 * Header to container is readonly when read file.
 * MetaSpace is read and write when read file.
 */
class CodeReviveCodeSpace;
class CodeReviveLookupTable;
class CodeReviveMetaSpace;

class CodeReviveMergedMetaInfo : public CHeapObj<mtInternal> {
 public:
  CodeReviveMergedMetaInfo();
  ~CodeReviveMergedMetaInfo() {}


  // main operations
  bool                   save_metadatas(char* start);
  bool                   initialize(Arena* arena);
  void                   record_metadata(char* name, int64_t identity, uint16_t loader_type, bool is_method);

  // getter
  int                    get_meta_index(char* name, int64_t identity, uint16_t loader_type);
  char*                  metadata_name(int index);
  int64_t                metadata_identity(int index);
  uint16_t               metadata_loader_type(int index);
  size_t                 size() { return _cur_pos - _start + _header_size; }
  size_t                 estimate_size();
 
 private:
  // space information to store the name of meta
  ReservedSpace             _rs;           
  VirtualSpace              _vs;
  char*                     _cur_pos;     // position in virtual space in global meta names
  char*                     _start;
  // the total size of count, meta name pointers, identities
  size_t                    _header_size;

  // meta name pointer in virtual space
  GrowableArray<char*>*     _global_meta_pointers;
  // meta identity
  GrowableArray<int64_t>*   _global_meta_identities;
  // meta loader type
  GrowableArray<uint16_t>*  _global_meta_loader_types;
  // meta type : method or klass
  GrowableArray<bool>*      _meta_is_method_or_not;
  
  void save_metadata(char* name, bool is_method, size_t len);
  void save_str(char* str, size_t len);
};

#endif // SHARE_VM_CR_CODE_REVIVE_META_INFO_HPP
