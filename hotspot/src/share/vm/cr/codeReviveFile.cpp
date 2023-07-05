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
#include "classfile/systemDictionary.hpp"
#include "code/codeCache.hpp"
#include "code/nmethod.hpp"
#include "compiler/compilerOracle.hpp"
#include "compiler/disassembler.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/codeReviveCodeBlob.hpp"
#include "cr/codeReviveCodeSpace.hpp"
#include "cr/codeReviveContainer.hpp"
#include "cr/codeReviveLookupTable.hpp"
#include "cr/codeReviveMerge.hpp"
#include "cr/codeReviveMergedMetaInfo.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/revive.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/handles.inline.hpp"
#include "utilities/align.hpp"
#include <sys/stat.h>
#include <errno.h>

#ifndef O_BINARY       // if defined (Win32) use binary files.
#define O_BINARY 0     // otherwise do nothing.
#endif

static int open_for_write(const char* file_path) {
#ifdef _WINDOWS  // On Windows, need WRITE permission to remove the file.
  chmod(file_path, _S_IREAD | _S_IWRITE);
#endif

  // Use remove() to delete the existing file because, on Unix, this will
  // allow processes that have it open continued access to the file.
  remove(file_path);
  int fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0444);
  return fd;
}

static int open_for_read(const char* file_path) {
  int fd = open(file_path, O_RDONLY | O_BINARY, 0);
  return fd;
}

size_t CodeReviveFile::_alignment   = 8;

CodeReviveFile::CodeReviveFile() {
  _cur_pos     = NULL;
  _fd          = -1;
  _header      = NULL;
  _meta_space  = NULL;
  _container   = NULL;
  _ro_addr     = NULL;
  _rw_addr     = NULL;
  _used_size   = 0;
}

CodeReviveFile::~CodeReviveFile() {
  cleanup();
  if (_ro_addr != NULL) {
    os::unmap_memory(_ro_addr, _ro_len);
  }
  if (_rw_addr != NULL) {
    os::unmap_memory(_rw_addr, _rw_len);
  }
  if (_container != NULL) {
    delete _container;
    _container   = NULL;
  }
}

bool CodeReviveFile::cleanup() {
  if (_fd != -1) {
    if (::close(_fd) < 0) {
      CR_LOG(cr_save, cr_fail, "Fail to close file(%d) during saving\n", _fd);
      _fd = -1;
      return false;
    }
    _fd = -1;
  }
  return true;
}

/*
 * Collect candiate C2 compiled methods and save into file by given name.
 * Keep CodeReviveFile content in memory chunk and write into file finally.
 *
 * 1. Reserve memory and initialize, default is 1G,Allocate header, fields filled later
 * 2. Setup header and classpath entry table
 * 3. Create and initialize MetaSpace and Container
 * 4. Save all candidate C2 methods into Container layout and update header
 * 5. Setup Metaspace and write file
 */
bool CodeReviveFile::save(const char* file_path) {
  ResourceMark rm;
  // step1
  ReservedSpace rs(CodeRevive::max_file_size());
  _vs.initialize(rs, CodeRevive::max_file_size());
  _cur_pos = _vs.low();
  _header = (CodeReviveFileHeader*)_cur_pos;
  _cur_pos += align_up(sizeof(CodeReviveFileHeader), _alignment);

  // step2
  setup_header();

  // step3
  _meta_space = new CodeReviveMetaSpace();
  _container = new CodeReviveContainer(_meta_space, _cur_pos, _vs.high());
  _container->init();

  // step4
  // avoid other threads modify code cache when reading code cache
  MutexLockerEx mu(CodeCache_lock, Mutex::_no_safepoint_check_flag);
  if (_container->save() == false) {
    CR_LOG(cr_save, cr_fail, "Fail to save container during saving\n");
    return false;
  } else {
    _header->_code_container_offset = (_cur_pos - _vs.low());
    _header->_code_container_size = _container->size();
    _cur_pos += align_up(_container->size(), _alignment);
  }

  // step5
  // metaspace will have rw access when using in restore scenario.
  // Align metaspace with OS page size, then it will be mapped as rw different with other parts.
  _cur_pos = (char*)align_ptr_up(_cur_pos, os::vm_page_size());
  if (setup_meta_space() == false) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail to setup meta_space during saving\n");
    return false;
  }
  CR_LOG(CodeRevive::log_kind(), cr_trace, "Setup metadata space (%d bytes)\n", _meta_space->size());

  return save_common_steps(file_path);
}

/*
 * Used in merge phase
 * 1. header
 * 2. class path entry table
 * 3. container
 * 4. metaspace from CodeReviveMergedMetaInfo
 */
bool CodeReviveFile::save_merged(const char* file_path, GrowableArray<CodeReviveContainer*>* containers, CodeReviveMergedMetaInfo* meta_info) {
  ResourceMark rm;
  ReservedSpace rs(CodeRevive::max_file_size());
  _vs.initialize(rs, CodeRevive::max_file_size());
  _cur_pos = _vs.low();
  _header = (CodeReviveFileHeader*)_cur_pos;
  _cur_pos += align_up(sizeof(CodeReviveFileHeader), _alignment);

  // Temporary use fingerprint and classpath when merging.
  setup_header();

  _used_size = _cur_pos - _vs.low();

  // compute meta_space size
  _used_size += meta_info->estimate_size();

  // save the container from candidate container array
  if (save_all_container(containers) == false) {
    return false;
  }

  // metaspace will have rw access when using in restore scenario.
  // Align metaspace with OS page size, then it will be mapped as rw different with other parts.
  _cur_pos = (char*)align_ptr_up(_cur_pos, os::vm_page_size());
  // get the metadatas from CodeReviveMergedMetaInfo
  if (meta_info->save_metadatas(_cur_pos) == false) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail to setup meta_space during saving\n");
    return false;
  }
  CR_LOG(CodeRevive::log_kind(), cr_trace, "Setup metadata space (%d bytes)\n", meta_info->size());

  _header->_meta_space_offset = (_cur_pos - _vs.low());
  _header->_meta_space_size   = meta_info->size();
  _cur_pos += align_up(meta_info->size(), _alignment);

  return save_common_steps(file_path);
}

bool CodeReviveFile::save_all_container(GrowableArray<CodeReviveContainer*>* containers) {
  if (containers->length() == 0) {
    return false;
  }
  char* container_start = _cur_pos;
  CodeReviveContainer* container = NULL;
  CodeReviveContainer* prev_container = NULL;
  for (int i = 0; i < containers->length(); i++) {
    CR_LOG(cr_merge, cr_trace, "Save container %d\n", i);
    container = containers->at(i);
    if (_used_size + container->estimated_size() > CodeRevive::max_file_size()) {
      CR_LOG(cr_merge, cr_warning, "Exceed maximum file size in saving containers, abandon subsequent containers\n");
      break;
    }
    // set start and cur_pos
    container->init(_cur_pos, _vs.high());
    if (container->save_merged() == false) {
      CR_LOG(cr_merge, cr_fail, "Fail to save container during merging\n");
      return false;
    } else {
      if (i == 0) {
        _header->_code_container_offset = (_cur_pos - _vs.low());
        _container = container;
      } else {
        prev_container->set_next_offset(_cur_pos - _vs.low());
      }
      prev_container = container;
      _cur_pos += align_up(container->size(), _alignment);
    }
    _used_size += align_up(container->size(), _alignment);
  }
  _header->_code_container_size = _cur_pos - container_start;
  return true;
}

/*
 * Common code when saving JIT code for a single processs or merged files.
 *
 * 1. write content into file_path
 */
bool CodeReviveFile::save_common_steps(const char* file_path) {
  int fd = open_for_write(file_path);
  if (fd < 0) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail to open %s during saving\n", file_path);
    return false;
  }

  // write content into file
  bool write_fail = false;
  int total_bytes = (_cur_pos - _vs.low());
  int written_size = (int)os::write(fd, _vs.low(), total_bytes);
  if (written_size != total_bytes) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Incomplete write %s, expect %d, actual %d\n",
           file_path, total_bytes, written_size);
    write_fail = true;
  }

  if (write_fail == false) {
    if (os::fsync(fd) != 0) {
      CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail to fsync %s\n", file_path);
      write_fail = true;
    }
  }

  // not matter fail or not, try close first
  if (os::close(fd) < 0) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail to close %s", file_path);
    write_fail = true;
  }

  // try remove file when write fails anyway
  if (write_fail) {
    remove(file_path);
    return false;
  }

  // count space usage.
  CodeRevive::add_size_counter(CodeRevive::S_HEADER, sizeof(CodeReviveFileHeader));
  CodeRevive::add_size_counter(CodeRevive::S_LOOKUP_TABLE, (uint)_container->lookup_table_size());
  CodeRevive::total_file_size(_cur_pos - _vs.low());
  return true;
}

/*
 * Map CSA file into memory and prepare for revive/merge operations
 *
 * Actions includes:
 * - openfile
 * - map readonly part
 * - map readwrite part (they are continous in file but might not continous in memory)
 * - initialize and validate data structure
 *   - _header
 *   - _container
 *   - _meta_space
 *
 * Using scenarios
 * - revive: load csa file and get container has compatible fingerprint
 * - merge:
 *   - get uniq container types in all csa file: map readonly part, initilaze container
 *   - get candidate nmethods in csa files: map all, initilaze container, metaspace
 *   - get selected nemethods and copy into merege result: map all, initilaze container, metaspace
 *
 *
 * Steps:
 * 1. openfile, read&validate header, check file integrity
 * 2. map readonly part
 * 3. fingerprint check
 * 4. [optional] map readwrite part and setup metaspace
 * 5. [optional] setup container
 */
bool CodeReviveFile::map(const char* file_path,
                         bool need_container,
                         bool need_meta_sapce,
                         bool select_container, // should select from multiple containers
                         uint32_t cr_kind) {
  // Step1 open file and basic checks
  guarantee(_fd == -1, "can only map once");
  _fd = open_for_read(file_path);
  if (_fd < 0) {
    CR_LOG(cr_kind, cr_fail, "Unable to open archive file %s\n", file_path);
    return false;
  }

  CodeReviveFileHeader header;
  size_t sz = sizeof(CodeReviveFileHeader);
  _header = &header;

  // read header onto stack
  size_t n = os::read(_fd, _header, (unsigned int)sz);
  if (n != sz) {
    CR_LOG(cr_kind, cr_fail, "Unable to read the header from archive file %s\n", file_path);
    return false;
  }

  // validate header for csa file
  if (!validate_header(cr_kind)) {
    return false;
  }

  // check whether the file size is equal to the expected size.
  size_t file_size = os::lseek(_fd, 0, SEEK_END);
  if (!file_integrity_check(file_size)) {
    CR_LOG(cr_kind, cr_fail, "Incomplete archive file %s\n", file_path);
    return false;
  }

  // Step2 map read only part
  _ro_len = _header->_meta_space_offset;
  _ro_addr = os::map_memory(_fd, file_path, 0, NULL, _ro_len, true);
  if (_ro_addr != NULL) {
    _header = (CodeReviveFileHeader*)_ro_addr;
  } else {
    _ro_len = 0;
    CR_LOG(cr_kind, cr_fail, "Fail to map readonly part of %s\n", file_path);
    return false;
  }

  // Step3 map readwrite part and setup metas
  if (need_meta_sapce) {
    _rw_len = _header->_meta_space_size;
    _rw_addr = os::map_memory(_fd, file_path, _ro_len, NULL, _rw_len, false);
    if (_rw_addr == NULL) {
      CR_LOG(cr_kind, cr_fail, "Fail to map readwrite part %s\n", file_path);
      _rw_len = 0;
      return false;
    }
    // could not use file_start + _header->_meta_space_offset, map into not continous address
    char* meta_sapce_start = _rw_addr;
    _meta_space = new CodeReviveMetaSpace(meta_sapce_start, meta_sapce_start + _header->_meta_space_size);
  }

  // Step4 setup container
  // when revive select container with same fingerprint
  // In other cases, expect only one container in each file
  if (need_container) {
    char* container_start = start() + _header->_code_container_offset;
    if (select_container) {
      container_start = CodeReviveContainer::find_valid_container(start(), container_start);
      if (container_start == NULL) {
        CR_LOG(cr_kind, cr_fail, "Fail to find valid container %s\n", file_path);
        return false;
      }
    }
    // _meta_space could be NULL when need_meta_sapce is false
    _container = new CodeReviveContainer(_meta_space, container_start);
    _container->map();
  }
  CR_LOG(cr_kind, CodeRevive::is_merge() ? cr_info : cr_trace, "Succeed to load CSA file %s\n", file_path);
  return true;
}

/*
 * print code records file content
 * 1. header infomration
 * 2. iterate all lookup entry and methods
 *    2.1 try to print method information
 */
void CodeReviveFile::print() {
  ResourceMark rm;
  _container->print();
  _meta_space->print();
}

/*
 * used in save phase
 */
bool CodeReviveFile::setup_meta_space() {
  ResourceMark rm;
  _meta_space->save_metadatas(_cur_pos, _vs.high());

  _header->_meta_space_offset = (_cur_pos - _vs.low());
  _header->_meta_space_size   = _meta_space->size();
  _cur_pos += align_up(_meta_space->size(), _alignment);
  return true;
}

void CodeReviveFile::setup_header() {
  _header->_magic = 0xf00baba1;
  const char *vm_version = VM_Version::internal_vm_info_string();
  memset(_header->_jvm_ident, 0, JVM_IDENT_MAX);
  strncpy(_header->_jvm_ident, vm_version, JVM_IDENT_MAX - 1);
}

bool CodeReviveFile::file_integrity_check(size_t file_size) {
  size_t expected_size = align_up(_header->_meta_space_offset + _header->_meta_space_size, _alignment);
  return file_size == expected_size;
}

bool CodeReviveFile::validate_header(uint32_t cr_kind) {
  if (_header->_magic != (int)0xf00baba1) {
    CR_LOG(cr_kind, cr_fail, "Bad magic number of archive file\n");
    return false;
  }
  if (!CodeRevive::validate_check()) {
    return true;
  }
  const char *vm_version = VM_Version::internal_vm_info_string();
  if (strncmp(_header->_jvm_ident, vm_version, JVM_IDENT_MAX-1) != 0) {
    CR_LOG(cr_kind, cr_fail, "Different version or build of Hotspot built this archive file\n");
    return false;
  }
  return true;
}

char* CodeReviveFile::find_revive_code(Method* m, bool only_use_name) {
  CodeReviveLookupTable::Entry* e = lookup_table()->find_entry(m, only_use_name);
  if (e != NULL && e->_code_offset != -1) {
    return (char*)(_container->code_space()->_start + e->_code_offset);
  }
  return NULL;
}

CodeReviveCodeSpace* CodeReviveFile::code_space() const {
  return _container->code_space();
}

CodeReviveLookupTable* CodeReviveFile::lookup_table() const {
  return _container->lookup_table();
}

/*
 * dump opt info for all saved method
 */
void CodeReviveFile::print_opt() {
  // print containers
  char* container_start = start() + _header->_code_container_offset;
  while(container_start != NULL) {
    CodeReviveContainer* container = new CodeReviveContainer(_meta_space, container_start);
    container->map();
    print_opt_with_container(container);
    container_start = container->get_next_container(start());
  }
}

void CodeReviveFile::print_opt_with_container(CodeReviveContainer* container) {
  CodeReviveLookupTable* lookup_table = container->lookup_table();
  CodeReviveCodeSpace* code_space = container->code_space();
  ResourceMark rm;
  // collect methods
  CodeReviveLookupTableIterator iter(lookup_table);
  for (CodeReviveLookupTable::Entry* e = iter.first(); e != NULL; e = iter.next()) {
    int32_t code_offset = e->_code_offset;
    if (code_offset == -1) {
      continue;
    }
    int32_t version_idx = 0;
    char* method_name = _meta_space->metadata_name(e->_meta_index);
    while (true) {
      CodeRevive::out()->print_cr("%s version %d", method_name, version_idx);
      CodeReviveCodeBlob* cr_cb = new CodeReviveCodeBlob(code_space->get_code_address(code_offset), _meta_space);
      cr_cb->print_opt(method_name);
      if (cr_cb->next_version_offset() != -1) {
        code_offset = cr_cb->next_version_offset();
        version_idx++;
      } else {
        break;
      }
    }
  }
}

void CodeReviveFile::print_file_info() {
  outputStream* output = CodeRevive::out();
  output->print_cr("CSA file size                 : %d", CodeRevive::get_file_size());
  output->print_cr("  the size of read only part  : " SIZE_FORMAT, _header->_meta_space_offset);
  output->print_cr("  the size of read write part : " SIZE_FORMAT, _header->_meta_space_size);
}
