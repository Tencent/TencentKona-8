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
#ifndef SHARE_VM_CR_CODE_REVIVE_CONTAINER_HPP
#define SHARE_VM_CR_CODE_REVIVE_CONTAINER_HPP
#include "code/codeBlob.hpp"
#include "memory/allocation.hpp"

class CodeReviveMetaSpace;
class CodeReviveFingerprint;
class CodeReviveLookupTable; 
class CodeReviveCodeSpace;
class MergedCodeBlob;
class JitMetaInfo;

/*
 * Multi Container information
 * Container format:
 * -- Header section:
 *    -- method count
 *    -- fingerprint information  offset/size
 *    -- lookup table information offset/size
 *    -- code section information offset/size
 *    -- next container offset
 * 
 * -- FingerPrint
 *
 * -- Lookup table: map from name to code section
 *
 * -- Code Section: save method with same name, different version
 *
 * Scenarios:
 * 1. Application process save, write a single container
 * 2. Application process restore, select and map a container
 * 3. Merge:
 *    - Group CSA file with same fingerprint, create container for each fingerprint
 *    - Read input CSA file container in different stages
 *    - Write all containers into merged file
 */
class CodeReviveContainer : public CHeapObj<mtInternal> {
 public:
  CodeReviveContainer(CodeReviveMetaSpace* meta_space,
                      char* start = NULL,              // write/map start address
                      char* limit = NULL);             // write limit

  ~CodeReviveContainer();

  void   init();
  void   init(char* start, char* limit);
  bool   save();
  bool   save_merged();
  void   map();
  void   map_header();
  void   print();
  size_t size()                         { return _cur_pos - _start; }
  CodeReviveLookupTable* lookup_table() { return _lookup_table; }
  CodeReviveCodeSpace*   code_space()   { return _code_space; }
  CodeReviveFingerprint* fingerprint()  { return _fingerprint; }
  size_t lookup_table_size()            { return _header != NULL ? _header->_lookup_table_size : 0; }
  GrowableArray<MergedCodeBlob*>* candidate_codeblobs() { return _candidate_codeblobs; }
  GrowableArray<JitMetaInfo*>* jit_metas()              { return _jit_metas; }
  GrowableArray<int>*          file_indexes()           { return _file_indexes; }
  void reset_jit_metas()                { _jit_metas = NULL; }
  void reset_file_indexes()             { _file_indexes = NULL; } 
  void add_filename_index(int index);
  void init_jit_metas(Arena* arena);
  void init_for_merge(CodeReviveContainer* container, Arena* arena);
  bool has_same_fingerprint(CodeReviveContainer* container);
  char* get_fingerprint_start();
  size_t get_fingerprint_size();
  void set_next_offset(int32_t offset);
  char* get_next_container(char* file_start);
  void  compute_estimated_size(size_t code_space_size, size_t lookup_table_size);
  size_t estimated_size() { return _estimated_size; }
  static char* find_valid_container(char* file_start, char* container_start);
  static int   compare_by_count(CodeReviveContainer** c1, CodeReviveContainer** c2);
 private:
  struct CodeReviveContainerHeader {
    size_t   _method_count;
    size_t   _fingerprint_offset;
    size_t   _fingerprint_size;
    size_t   _lookup_table_offset;
    size_t   _lookup_table_size;
    size_t   _code_space_offset;
    size_t   _code_space_size;
    int32_t  _next_offset;          // next container's offset from code revive file, 0 for last container
  };

  // container memory space
  char* _cur_pos; // writing position
  char* _start;
  char* _limit;
  size_t _estimated_size;

  // container content, interprter cotnainer memory
  CodeReviveContainerHeader* _header;
  CodeReviveFingerprint*     _fingerprint;
  CodeReviveLookupTable*     _lookup_table;
  CodeReviveCodeSpace*       _code_space;

  // translate meta_index/metadata/name used in container
  CodeReviveMetaSpace*       _meta_space;

  // temporary collections used in dump
  GrowableArray<CodeBlob*>*  _save_blobs;

  // temporary collections used in merge
  GrowableArray<MergedCodeBlob*>* _candidate_codeblobs;
  GrowableArray<JitMetaInfo*>*    _jit_metas;
  GrowableArray<int>*             _file_indexes;

  // single process save utilities
  bool collect_save_codes();
  bool setup_lookup_table();
  bool setup_code_space();
  void setup_header();
  void setup_fingerprint();

  // merge and dump utilities
  void setup_fingerprint_for_merge();
};

#endif // SHARE_VM_CR_CODE_REVIVE_CONTAINER_HPP
