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

#ifndef SHARE_VM_CR_CODE_REVIVE_MERGE_HPP
#define SHARE_VM_CR_CODE_REVIVE_MERGE_HPP
#include "cr/codeReviveCodeBlob.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "memory/allocation.hpp"

class CodeReviveFile;
class CodeReviveContainer;
class JitMetaInfo;
class WildcardEntryInfo;

struct CandidateCodeBlob {
  int32_t _file_index;
  int32_t _meta_index;
  int32_t _file_offset;
  int32_t _code_size;

  CandidateCodeBlob() : _file_index(0), _meta_index(0), _file_offset(0), _code_size(0) { }
  CandidateCodeBlob(int32_t file_index, int32_t meta_index) :
    _file_index(file_index), _meta_index(meta_index), _file_offset(0), _code_size(0) { }
  CandidateCodeBlob(int32_t file_index, int32_t meta_index, int32_t file_offset, int32_t code_size) :
    _file_index(file_index), _meta_index(meta_index), _file_offset(file_offset), _code_size(code_size) { }

  void set_offset_and_size(int file_offset, int32_t code_size);
};

class MergedCodeBlob : public ResourceObj {
 friend class CodeReviveMerge;
 private:
  int32_t _file_index;
  int32_t _meta_index;
  int32_t _file_offset;
  int32_t _code_size;
  int32_t _code_offset;
  int32_t _next_offset;  // the offset of next version
  int32_t _first_offset; // the offset of the first version
  MergedCodeBlob* _next;

 public:
  MergedCodeBlob(int32_t file_index, int32_t meta_index, int32_t file_offset, int32_t code_size, int32_t code_offset) :
    _file_index(file_index), _meta_index(meta_index), _file_offset(file_offset), _code_size(code_size),
    _code_offset(code_offset), _next_offset(-1), _first_offset(-1), _next(NULL) { }
  void set_next(MergedCodeBlob* codeblob) { _next = codeblob; }
  MergedCodeBlob* next() { return _next; }
  int32_t file_index()   { return _file_index; }
  int32_t meta_index()   { return _meta_index; }
  int32_t file_offset()  { return _file_offset; }
  int32_t code_size()    { return _code_size; }
  int32_t code_offset()  { return _code_offset; }
  int32_t first_offset() { return _first_offset; }
  int32_t next_offset()  { return _next_offset; }
  void set_next_offset(int32 next_offset) { _next_offset = next_offset; }
  void set_first_offset(int32 first_offset) { _first_offset = first_offset; }
};

/*
 * Global data structrue for CodeReviveFile merge.
 *
 * Inputs:
 *   1. all input CSA file paths
 *   2. merge class path: CSA file's classpath should start with given merge class path
 *   3. limitations: merged file size/container method count limitation/hot method threshold etc
 *
 * Process:
 *   1. Group CSA file by fingerprint (configurations\JVM options etc);
 *   2. Iterate CSA files in each group and build containers
 *      - select/remove/combine different JIT compiled versions from CSA multiple files
 *   3. Write containers into merged file
 *
 * Output:
 *   Merged file on success, or fail message
 */
class CodeReviveMerge : AllStatic {
 private:
  static CodeReviveMetaSpace*            _global_meta_space;
  static CodeReviveMergedMetaInfo*       _global_meta_info;
  static GrowableArray<char*>*           _csa_filenames;
  static GrowableArray<const char*>*     _cp_array;
  static Arena*                          _arena;
  static GrowableArray<WildcardEntryInfo*>*     _merged_cp_array;
  static GrowableArray<const char*>*     _sorted_jars_in_wildcards;
  static int _cp_array_size;
  static int _valid_csa_count;

  static bool init(Arena* arena);
  static bool acquire_input_files();
  static GrowableArray<CodeReviveContainer*>* check_and_group_files_by_fingerprint();
  static void preprocess_candidate_nmethods(CodeReviveContainer* container, Arena* arena);
  static void add_candidate_code_into_jit_metas(CodeReviveFile* file, CodeReviveCodeBlob* cb, int32_t meta_index,
                                                CodeReviveContainer* container, int32_t file_index, Arena* arena);
  static void determine_merged_code_for_container(CodeReviveContainer* container, Arena* arena);
  static void candidate_selection(CodeReviveContainer* container, Arena* arena);
  static void add_merged_codeblob(MergedCodeBlob* mcb, GrowableArray<MergedCodeBlob*>* candidate_codeblobs);
  static void remove_containers(GrowableArray<CodeReviveContainer*>* containers);
  static void print_jit_versions(GrowableArray<JitMetaInfo*>* jit_metas);
  static void print_candidate_info_in_container(CodeReviveContainer* container, int index);
  static void print_merged_code_info_in_container(CodeReviveContainer* container, int index);
public:
  static void merge_and_dump(TRAPS);
  static GrowableArray<const char*>* cp_array() { return _cp_array; }
  static GrowableArray<WildcardEntryInfo*>* merged_cp_array() { return _merged_cp_array; }
  static GrowableArray<const char*>* sorted_jars_in_wildcards()          { return _sorted_jars_in_wildcards; }
  static int cp_array_size() { return _cp_array_size; }
  static char* metadata_name(int index) { return _global_meta_space->metadata_name(index); }
  static char* csa_file_name(int index) { return _csa_filenames->at(index); }
  static void dump_merged_file(GrowableArray<CodeReviveContainer*>* containers);
  static void update_code_blob_info(CodeReviveMetaSpace* meta_space, char* start, int32_t next_offset);
  static CodeReviveMetaSpace* global_meta_space() { return _global_meta_space; }
  static GrowableArray<OptRecord*>* duplicate_opt_record_array_in_arena(GrowableArray<OptRecord*>* opts, Arena* arena);
  static GrowableArray<ReviveDepRecord*>* duplicate_dep_record_array_in_arena(GrowableArray<ReviveDepRecord*>* deps, Arena* arena);
};

#endif // SHARE_VM_CR_CODE_REVIVE_MERGE_HPP
