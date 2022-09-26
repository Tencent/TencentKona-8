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
#ifndef SHARE_VM_CR_CODE_REVIVE_CODE_BLOB_HPP
#define SHARE_VM_CR_CODE_REVIVE_CODE_BLOB_HPP
#include "code/codeBlob.hpp"
#include "code/dependencies.hpp"
#include "cr/codeReviveVMGlobals.hpp"
#include "cr/codeReviveOptRecords.hpp"

class AbstractCompiler;
class ciBaseObject;
class CodeReviveMetaSpace;
class ReviveDependencies;
class ReviveDepRecord;
/*
 * CodeReviveCodeBlob represent a binary code blob in revive file.
 *
 * Relocation and generate information for a code blob in same orders
 * 1. iterate relocs
 * 2. iterate blob specific contents
 *    2.1 nmethod: oops & metadatas
 *
 * Layout is in following format
 * 1. Header: 
 *    -- CodeReviveCodeBlob size
 *    -- CodeBlob size
 *    -- offset for entry/verified_entry
 *    -- size/offset for meta_array/oop_array/reloc_data
 *    -- oop map size
 *    -- offset for next version
 * 2. CodeBlob body: begin/end
 * 3. Reslove information align with relocation order, item includes:
 *    3.1 external world/runtime call
 *        external_kind, offset
 *    3.2 oop:
 *        string: class_name, cp_index
 *        mirror: class_name
 *    3.3 metadata:
 *        klass: index in metaspace
 *        method: class index, method index
 *    3.4 poll_type, poll_return_type
 *        external kind -- polling page address
 *    3.5 what does virtual/static call means
 * 4. Oop Maps
 *    -- count/heap size
 *    -- Oop Map
 *       -- pc offset
 *       -- omv count
 *       -- omv data size
 *       -- omv data
 *    -- Oop Map
 *    -- ...
 *    -- Oop Map
 */
// optimize class name, method signature later, might have a big global symbol table
class CodeReviveCodeBlob : public StackObj {
 private:
  struct Header {
    int                    _size;
    int                    _cb_size;
    int                    _entry_offset;
    int                    _verified_entry_offset;
    int                    _aux_meta_array_begin;
    int                    _aux_meta_array_size;
    int                    _aux_oop_array_begin;
    int                    _aux_oop_array_size;
    int                    _aux_reloc_data_begin; // offset from header
    int                    _aux_reloc_data_size;  // in bytes
    int                    _oop_map_size;
    int                    _next_version_offset;  // next version offset, -1 means no next version
  };
  Header*                  _header;
  CodeBlob*                _cb;
  char*                    _start;
  char*                    _cur;
  char*                    _limit;
  intptr_t                 _alignment;
  CodeReviveMetaSpace*     _meta_space;

  static const char* _revive_result_name[5];
  static OopMapSet*  _dummy_oopmapset;
  static OopMap*     _dummy_oopmap;

  void init(char* start, CodeReviveMetaSpace* meta_space);
  bool save_revive_aux_info();
  void save_oop_map_set();
  // redundant methods, need have a common base class for continuous space later
  void emit_u4(uint32_t v) {
    assert(((intptr_t)_cur) % sizeof(uint32_t) == 0, "not align");
    *((uint32_t*)_cur) = v;
    _cur += sizeof(uint32_t);
  }
  uint32_t read_u4() {
    assert(((intptr_t)_cur) % sizeof(uint32_t) == 0, "not align");
    uint32_t value = *((uint32_t*)_cur);
    _cur += sizeof(uint32_t);
    return value;
  }

  bool validate_aot_method_dependencies(ReviveDependencies* revive_deps);
  bool check_meta_resolve(char* aux_reloc_begin, bool data_array = false);
 public:
  class JitVersionReviveState : public ResourceObj {
   public:
    char* _start;
    Method* _method;
    GrowableArray<ciBaseObject*>* _oops_metas;
    GrowableArray<ciBaseObject*>* _meta_array;
    GrowableArray<ciBaseObject*>* _oop_array;
    GrowableArray<uint32_t>* _global_oop_array;
    int32_t _version;
    uint32_t _check_results;

    enum VersionState {
      check_passed       = 1,
      preproccess_passed = 2,
      state_mask         = 3
    };

    JitVersionReviveState(Method* method, char* start, int32_t version = -1) : _start(start), _method(method), _oops_metas(NULL),
                          _meta_array(NULL), _oop_array(NULL), _global_oop_array(NULL), _version(version), _check_results(0) {}

    bool no_available_version() { return _version == -1; }
    void add_result(VersionState state) { _check_results |= state; }
    bool is_valid() { return (_check_results & state_mask) == (check_passed | preproccess_passed); }
  };
  enum ReviveResult {
    REVIVE_OK, REVIVE_FAILED_ON_META_ARRAY, REVIVE_FAILED_ON_OOP_ARRAY, REVIVE_FAILED_ON_RELOCINFO, REVIVE_FAILED_ON_OPT_RECORD
  };

  CodeReviveCodeBlob(char* start, CodeReviveMetaSpace* meta_space);
  CodeReviveCodeBlob(char* start, char* limit, CodeBlob* cb, CodeReviveMetaSpace* meta_space);
  CodeBlob* codeblob() const               { return _cb; }
  char* start() const                      { return _start; }
  char* cb_begin() const                   { return _start + sizeof(Header); }
  int   cb_size() const                    { return _header->_cb_size; }
  char* aux_reloc_begin() const            { return _start + _header->_aux_reloc_data_begin; }
  int   aux_reloc_size() const             { return _header->_aux_reloc_data_size; }
  char* aux_meta_array_begin() const       { return _start + _header->_aux_meta_array_begin; }
  int   aux_meta_array_size() const        { return _header->_aux_meta_array_size; }
  char* aux_oop_array_begin() const       { return _start + _header->_aux_oop_array_begin; }
  int   aux_oop_array_size() const         { return _header->_aux_oop_array_size; }
  char* oop_maps_begin() const             { return aux_reloc_begin() + aux_reloc_size(); }
  int   oop_maps_size() const              { return _header->_oop_map_size; }
  char* dependencies_begin();
  int   dependencies_size();
  address opt_records_begin();
  int   opt_records_size();
  bool  can_be_merged();

  // save
  bool save();

  // revive
  nmethod* create_nmethod(methodHandle method,
                          uint compile_id,
                          AbstractCompiler* compiler,
                          JitVersionReviveState* revive_state);
  GrowableArray<ciBaseObject*>* preprocess_oop_and_meta(char* aux_reloc_begin, Method* method, bool data_array,
                                                        GrowableArray<uint32_t>* global_oop_array);
  OopMapSet* restore_oop_map_set();
  bool create_global_oops(GrowableArray<uint32_t>* global_oop_array);
  int check_aot_method_opt_records(GrowableArray<ciBaseObject*>* meta_array);
  GrowableArray<OptRecord*>* get_opt_records(OptRecord::CtxType ctx_type, void* ctx, GrowableArray<int32_t>* data_array_meta_index);
  GrowableArray<OptRecord*>* get_opt_records_for_merge(CodeReviveMetaSpace* space, GrowableArray<int32_t>* data_array_meta_index) {
    return get_opt_records(OptRecord::MERGE, space, data_array_meta_index);
  }
  GrowableArray<OptRecord*>* get_opt_records_for_dump(GrowableArray<char*>* meta_names) {
    return get_opt_records(OptRecord::DUMP, meta_names, NULL);
  }
  GrowableArray<ReviveDepRecord*>* get_revive_deps(GrowableArray<char*>* meta_array_names);
  GrowableArray<ReviveDepRecord*>* get_revive_deps(GrowableArray<int32_t>* meta_array_index);

  ReviveResult revive_and_opt_record_check(JitVersionReviveState* revive_state);
  ReviveResult revive_preprocess(JitVersionReviveState* revive_state);

  // utils
  int size() const                       { return _header->_size; }
  int next_version_offset() const        { return _header->_next_version_offset; }
  void set_next_version_offset(int next) { _header->_next_version_offset = next; }
  CodeBlob* extract_cb();
  void print();
  void print_aux_info(char* begin, char* end);
  void print_opt(char* method_name);

  static const char* revive_result_name(int idx) { return _revive_result_name[idx]; }
  static void init_dummy_oopmap();
};
#endif // SHARE_VM_CR_CODE_REVIVE_CODE_BLOB_HPP
