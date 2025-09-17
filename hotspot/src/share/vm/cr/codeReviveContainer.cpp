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
#include "code/codeCache.hpp"
#include "code/nmethod.hpp"
#include "compiler/disassembler.hpp"
#include "cr/codeReviveLookupTable.hpp"
#include "cr/codeReviveFingerprint.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/codeReviveCodeSpace.hpp"
#include "cr/codeReviveContainer.hpp"
#include "cr/codeReviveCodeBlob.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/codeReviveMerge.hpp"
#include "cr/revive.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/align.hpp"
#include <sys/stat.h>
#include <errno.h>

CodeReviveContainer::CodeReviveContainer(CodeReviveMetaSpace* meta_space, char* start, char* limit) {
  _start               = start;
  _cur_pos             = start;
  _limit               = limit;
  _meta_space          = meta_space;
  _header              = NULL;
  _fingerprint         = NULL;
  _lookup_table        = NULL;
  _code_space          = NULL;
  _save_blobs          = NULL;
  _candidate_codeblobs = NULL;
  _jit_metas           = NULL;
  _file_indexes        = NULL;
}

CodeReviveContainer::~CodeReviveContainer() {
  assert(_save_blobs == NULL && _candidate_codeblobs == NULL && _jit_metas == NULL && _file_indexes == NULL,
         "temporay structure not released");
  if (_lookup_table != NULL) {
    delete _lookup_table;
    _lookup_table = NULL;
  }
  if (_code_space != NULL) {
    delete _code_space;
    _code_space = NULL;
  }
  if (_fingerprint != NULL) {
    delete _fingerprint;
    _fingerprint = NULL;
  }
}

class CollectCandidateClosure: public CodeBlobClosure {
 private:
  GrowableArray<CodeBlob*>* _cbs;
 public:
  CollectCandidateClosure(GrowableArray<CodeBlob*>* cbs) :
    _cbs(cbs) {}

  void do_code_blob(CodeBlob* cb) {
    if (CodeRevive::is_save_candidate(cb)) {
      _cbs->append(cb);
    }
  }
};

bool CodeReviveContainer::collect_save_codes() {
  HandleMark hm(Thread::current());
  _save_blobs = new GrowableArray<CodeBlob*>();
  CollectCandidateClosure cb_cl(_save_blobs);
  CodeCache::blobs_do(&cb_cl);
  return _save_blobs->length() > 0;
}

/*
 * 1. setup lookup table
 * 2. Allocate Entry for method in lookup table
 *    2.1 allocate & store name in table
 *    2.2 allocate entry in table
 *    2.3 setup entry with name stored position, leave code store position empty
 * 3. update header info
 */
bool CodeReviveContainer::setup_lookup_table() {
  ResourceMark rm;
  _lookup_table = new CodeReviveLookupTable(_meta_space, _cur_pos, _limit, true);

  for (int i = 0; i < _save_blobs->length(); i++) {
    CodeBlob* cb = _save_blobs->at(i);
    if (cb->is_nmethod()) {
      nmethod* nm = (nmethod*)cb;
      int meta_index = _meta_space->record_metadata(nm->method());
      _lookup_table->new_entry(nm->method(), meta_index);
    }
  }
  _header->_lookup_table_offset = (_cur_pos - _start);
  _header->_lookup_table_size = _lookup_table->size();
  _cur_pos += align_up(_lookup_table->size(), CodeReviveFile::alignment());
  return true;
}

/*
 * 1. setup code space
 * 2. Allocate space and save code
 * 3. Find lookup Entry and update code information
 * 4. Update header information
 */
bool CodeReviveContainer::setup_code_space() {
  ResourceMark rm;
  HandleMark hm;
  _code_space = new CodeReviveCodeSpace(_cur_pos, _limit, false);
  for (int i = 0; i < _save_blobs->length(); i++) {
    CodeBlob* cb = _save_blobs->at(i);
    if (cb->is_nmethod()) {
      nmethod* nm = (nmethod*)cb;
      Method* m = nm->method();
      CodeReviveLookupTable::Entry* e = _lookup_table->find_entry(m);
      assert(e != NULL, "");
      guarantee(e->_code_offset == -1, "already has code");
      int code_offset = _code_space->saveCode(cb, _meta_space);
      // If the nmethod is make not entrant during memcpy, it's un-usable for later revive
      if (nm->is_in_use()) {
        e->_code_offset = code_offset;
      }
      if (CodeRevive::is_log_on(cr_assembly, cr_trace) ||
          (code_offset == -1 && CodeRevive::is_log_on(cr_assembly, cr_fail))) {
        Disassembler::decode(nm, CodeRevive::out());
      }
      CR_LOG(cr_save, code_offset == -1 ? cr_fail : cr_trace, "Emit method %s %s (Identity: %lx)\n",
             Method::name_and_sig_as_C_string_all(m->constants()->pool_holder(), m->name(), m->signature()),
             code_offset == -1 ? "fail" : "success", m->cr_identity());
    }
  }
  _header->_code_space_offset = (_cur_pos - _start);
  _header->_code_space_size = _code_space->size();
  _cur_pos += align_up(_code_space->size(), CodeReviveFile::alignment());
  return true;
}

void CodeReviveContainer::setup_fingerprint() {
  _fingerprint = new CodeReviveFingerprint(_cur_pos);
  _header->_fingerprint_offset = (_cur_pos - _start);
  _fingerprint->setup();
  _header->_fingerprint_size = _fingerprint->size();
  _cur_pos += align_up(_header->_fingerprint_size, CodeReviveFile::alignment());
}

void CodeReviveContainer::init() {
  _header = (CodeReviveContainerHeader*)_cur_pos;
  _header->_next_offset = 0;
  _cur_pos += align_up(sizeof(CodeReviveContainerHeader), CodeReviveFile::alignment());

  setup_fingerprint();
}

// initialize container during merge
void CodeReviveContainer::init(char* start, char* limit) {
  _start = start;
  _cur_pos = start;
  _limit = limit;
  _header = (CodeReviveContainerHeader*)_cur_pos;
  _header->_next_offset = 0;
  _cur_pos += align_up(sizeof(CodeReviveContainerHeader), CodeReviveFile::alignment());

  setup_fingerprint_for_merge();
}

// In merge stage, the fingerprint isn't from the option array of the current process,
// and is got from the merged csa file
void CodeReviveContainer::setup_fingerprint_for_merge() {
  _header->_fingerprint_offset = (_cur_pos - _start);
  _header->_fingerprint_size = _fingerprint->size();
  memcpy(_cur_pos, _fingerprint->start(), _fingerprint->size());
  _cur_pos += align_up(_header->_fingerprint_size, CodeReviveFile::alignment());
}

bool CodeReviveContainer::save() {
  if (collect_save_codes() == false) {
    CR_LOG(cr_save, cr_fail, "No candidate found during saving\n");
    return false;
  }
  CR_LOG(cr_save, cr_trace, "Collect %d candidate code blobs\n", _save_blobs->length());

  if (setup_lookup_table() == false) {
    CR_LOG(cr_save, cr_fail, "Fail to setup lookup table during saving\n");
    return false;
  }
  CR_LOG(cr_save, cr_trace, "Setup lookup table (%d bytes)\n", _lookup_table->size());

  if (setup_code_space() == false) {
    CR_LOG(cr_save, cr_fail, "Fail to setup code space during saving\n");
    return false;
  }
  return true;
}

bool CodeReviveContainer::save_merged() {
  GrowableArray<MergedCodeBlob*>* candidate_codeblobs = _candidate_codeblobs;
  _lookup_table = new CodeReviveLookupTable(_meta_space, _cur_pos, _limit, true);

  for (int i = 0; i < candidate_codeblobs->length(); i++) {
     MergedCodeBlob* mcb = candidate_codeblobs->at(i);
     // get the MergedCodeBlob with _next
     while (mcb != NULL) {
       _lookup_table->new_entry(_meta_space->metadata_name(mcb->meta_index()), mcb->meta_index(), true);
       mcb = mcb->next();
     }
  }
  _header->_lookup_table_offset = (_cur_pos - _start);
  _header->_lookup_table_size = _lookup_table->size();
  _cur_pos += align_up(_lookup_table->size(), CodeReviveFile::alignment());


  _header->_code_space_offset = (_cur_pos - _start);
  char* code_space_start = _cur_pos;

  for (int i = 0; i < candidate_codeblobs->length(); i++) {
    HandleMark   hm;
    ResourceMark rm;
    MergedCodeBlob* mcb = candidate_codeblobs->at(i);
    // get csa file name
    char* file_name = CodeReviveMerge::csa_file_name(mcb->file_index());

    CodeReviveFile* csa_file = new CodeReviveFile();
    // map the read only part and read write part
    // construct CodeReviveMetaSpace
    if (!csa_file->map(file_name, false, true, false, cr_merge)) {
      CR_LOG(cr_merge, cr_fail, "Fail to merge the candidate csa file: %s", file_name);
      delete csa_file;
      continue;
    }

    CodeReviveMetaSpace* meta_space = csa_file->meta_space();

    char* code_space_start = _start + _header->_code_space_offset;

    // dump all the codeblob in the same csa file
    while(mcb != NULL) {
      char* codeblob = csa_file->offset_to_addr(mcb->file_offset());
      // the offset is computed in candidate_selection
      char* code_ptr = code_space_start + mcb->code_offset();
      guarantee(codeblob != NULL, "should be");

      CodeReviveCodeBlob* cb = new CodeReviveCodeBlob(codeblob, NULL);

      char* method_name = _meta_space->metadata_name(mcb->meta_index());
      int64_t identity = _meta_space->metadata_identity(mcb->meta_index());
      uint32_t loader_type = _meta_space->metadata_loader_type(mcb->meta_index());

      // find the entry with method name + identity + loader type
      CodeReviveLookupTable::Entry* entry = _lookup_table->find_entry(method_name, identity, loader_type);
      guarantee(entry != NULL, "should be");
      CR_LOG(cr_merge, cr_trace, "Merge saving %s\n", method_name);

      memcpy(code_ptr, codeblob, mcb->code_size());

      CodeReviveMerge::update_code_blob_info(meta_space, code_ptr, mcb->next_offset());
      if (mcb->first_offset() != -1) {
        entry->_code_offset = mcb->first_offset();
      }

      _cur_pos += align_up((size_t)mcb->code_size(), CodeReviveFile::alignment());
      mcb = mcb->next();
    }
    // delete csa file, release the resource in CodeReviveFile
    delete csa_file;
  }
  _header->_code_space_size = (_cur_pos - _start) - _header->_code_space_offset;
  _candidate_codeblobs = NULL; // clear none used temp structures
  return true;
}

// For each container:
// _fingerprint_offset is the offset from the Container start
// _next_offset is the offset from the csa file start
char* CodeReviveContainer::find_valid_container(char* file_start, char* container_start) {
  CodeReviveContainerHeader* header = (CodeReviveContainerHeader*)container_start;
  CodeReviveFingerprint fingerprint(container_start + header->_fingerprint_offset);
  bool result = fingerprint.validate();

  if (result) {
    return container_start;
  } else if (header->_next_offset != 0) {
    return find_valid_container(file_start, file_start + header->_next_offset);
  }
  return NULL;
}

/*
 * get the next container start ptr in the same csa file
 */
char* CodeReviveContainer::get_next_container(char* file_start) {
  if (_header->_next_offset != 0) {
    return file_start + _header->_next_offset;
  }
  return NULL;
}

void CodeReviveContainer::map() {
  _header = (CodeReviveContainerHeader*)_start;
  _lookup_table = new CodeReviveLookupTable(_meta_space,
                                            _start + _header->_lookup_table_offset,
                                            _start + _header->_lookup_table_offset + _header->_lookup_table_size,
                                            false, 0);
  _code_space = new CodeReviveCodeSpace(_start + _header->_code_space_offset,
                                        _start + _header->_code_space_offset + _header->_code_space_size,
                                        true);
}

void CodeReviveContainer::map_header() {
  _header = (CodeReviveContainerHeader*)_start;
}

void CodeReviveContainer::add_filename_index(int index) {
  _file_indexes->append(index);
}

void CodeReviveContainer::print() {
  ResourceMark rm;
  // iterate lookup table and print entry and code
  CodeReviveLookupTableIterator iter(_lookup_table);
  for (CodeReviveLookupTable::Entry* e = iter.first(); e != NULL; e = iter.next()) {
    _lookup_table->print_entry(e, "");
    if (e->_code_offset == -1) {
      continue;
    }
    CodeReviveCodeBlob* cr_cb = new CodeReviveCodeBlob(_code_space->get_code_address(e->_code_offset), _meta_space);
    cr_cb->print();
  }
}

char* CodeReviveContainer::get_fingerprint_start() {
  assert(_start != NULL && _header != NULL, "");
  return _start + _header->_fingerprint_offset;
}

size_t CodeReviveContainer::get_fingerprint_size() {
  return _header->_fingerprint_size;
}

void CodeReviveContainer::init_for_merge(CodeReviveContainer* container, Arena* arena) {
  _candidate_codeblobs = new (arena) GrowableArray<MergedCodeBlob*>(arena, 500, 0, NULL);
  _file_indexes = new (arena) GrowableArray<int>(arena, 1000, 0, 0);
  // the fingperprint of the candidate container is got from the merged container
  char* start = (char*)NEW_C_HEAP_ARRAY(char, container->get_fingerprint_size(), mtInternal);
  memcpy(start, container->get_fingerprint_start(), container->get_fingerprint_size());
  _fingerprint = new CodeReviveFingerprint(start, container->get_fingerprint_size());
}

void CodeReviveContainer::init_jit_metas(Arena* arena) {
  _jit_metas = new (arena) GrowableArray<JitMetaInfo*>(arena, 1000, 0, NULL);
}

bool CodeReviveContainer::has_same_fingerprint(CodeReviveContainer* container) {
  CodeReviveFingerprint fingerprint(container->get_fingerprint_start(), container->get_fingerprint_size());
  return _fingerprint->is_same(&fingerprint);
}

void CodeReviveContainer::set_next_offset(int32_t offset) {
  _header->_next_offset = offset;
}

int CodeReviveContainer::compare_by_count(CodeReviveContainer** c1, CodeReviveContainer** c2) {
  return (*c2)->_file_indexes->length() - (*c1)->_file_indexes->length();
}

  // estimate the size of container:
  // 1. container header size, compute in set_estimated_size
  // 2. fingerprint size
  // 3. max size of lookup table = entry_size * nmethod_number
  // 4. codeblob size = offset_in_codespace
void CodeReviveContainer::compute_estimated_size(size_t code_space_size, size_t lookup_table_size) {
  _estimated_size = align_up(sizeof(CodeReviveContainerHeader), CodeReviveFile::alignment());
  _estimated_size += align_up(_fingerprint->size(), CodeReviveFile::alignment());
  _estimated_size += lookup_table_size;
  _estimated_size += code_space_size;
}
