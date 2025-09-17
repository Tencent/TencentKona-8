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

#ifndef SHARE_VM_CR_CODE_REVIVE_FILE_HPP
#define SHARE_VM_CR_CODE_REVIVE_FILE_HPP
#include "memory/allocation.hpp"
#include "memory/filemap.hpp"

/*
 * CodeReviveFile represents a CSA (Code Shared Archive) file in VM. Scenarios include
 * 1. Save JIT code: write JIT result into CodeReviveFile.
 * 2. Restore JIT code: map CodeReviveFile and read.
 * 3. Merge CSA file:
 *    3.1 Input CSA: map CodeReviveFile and read.
 *    3.2 Output CSA: write merged result.
 *
 * CodeReviveFile Layout:
 * -- Header
 *    -- magic
 *    -- offset & size to 1. first container 2. global metaspace
 *    -- jvm idenitity information string
 *
 * -- CodeReviveContainer 1   // container is linked with next offset
 * -- CodeReviveContainer 2
 * -- ...
 * -- CodeReviveContainer n
 *
 * -- CodeReviveMetaSpace
 *
 * These CodeRevive runtime data structure points to file content in mapped file.
 *
 * Header to container is readonly when read file.
 * MetaSpace is read and write when read file.
 */
class CodeReviveCodeSpace;
class CodeReviveContainer;
class CodeReviveLookupTable;
class CodeReviveMetaSpace;
class CodeReviveMergedMetaInfo;

class CodeReviveFile : public CHeapObj<mtInternal> {
 public:
  CodeReviveFile();
  ~CodeReviveFile();

  // getter
  CodeReviveMetaSpace*   meta_space()   const { return _meta_space; }
  CodeReviveContainer*   container()    const { return _container; }
  CodeReviveLookupTable* lookup_table() const; // get from container
  CodeReviveCodeSpace*   code_space()   const; // get from container
  char*                  start()        const { return _ro_addr; } // _ro_addr is mapped file start

  // main operations
  bool                   save(const char* file_path);
  bool                   save_merged(const char* file_path, GrowableArray<CodeReviveContainer*>* containers, CodeReviveMergedMetaInfo* meta_info);
  bool                   map(const char* file_path, bool need_container, bool need_meta_sapce,
                             bool select_container, uint32_t cr_kind);
  // utilities
  void                   print();
  void                   print_opt();
  void                   print_file_info();
  char*                  find_revive_code(Method* m, bool only_use_name = false); // get m's saved code address in file, return NULL if not found
  char*                  offset_to_addr(int32_t offset) const { return _ro_addr + offset; } // map from file offset to mapped address
  static size_t          alignment() { return _alignment; }  // file write alignment between elemenets

 private:
  struct CodeReviveFileHeader {
    int    _magic;
    size_t _code_container_offset;
    size_t _code_container_size;
    size_t _meta_space_offset;
    size_t _meta_space_size;
    char   _jvm_ident[JVM_IDENT_MAX];
  };
  // space and file information
  VirtualSpace              _vs;          // use in save/merge, invalid when read
  int32_t                   _fd;          // use in only map read (merge/revive), -1 means not opened
  char*                     _cur_pos;     // use in save/merge for contents write
  char*                     _ro_addr;     // used in read, mapped file readonly part address
  char*                     _rw_addr;     // used in read, mapped file writable part address
  size_t                    _ro_len;      // used in read, mapped file readonly part length in bytes
  size_t                    _rw_len;      // used in read, mapped file writable part length in bytes

  // file content
  CodeReviveFileHeader*     _header;
  CodeReviveContainer*      _container;   // first container for write, matched container for restore
  CodeReviveMetaSpace*      _meta_space;

  static size_t             _alignment;
  size_t                    _used_size;

  // save steps
  void setup_header();
  bool save_common_steps(const char* file_path);
  bool setup_meta_space();
  bool cleanup();

  // internal methods for restore/merge
  bool validate_header(uint32_t cr_kind);
  bool file_integrity_check(size_t file_size);
  bool save_all_container(GrowableArray<CodeReviveContainer*>* containers);

  // utilities
  void print_opt_with_container(CodeReviveContainer* container);
};

#endif // SHARE_VM_CR_CODE_REVIVE_FILE_HPP
