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
#include "classfile/classLoaderExt.hpp"
#include "cr/codeReviveFingerprint.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/revive.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "runtime/arguments.hpp"
#include "utilities/align.hpp"


#define WHITE_LIST_DO(do_option)                     \
  do_option("Print")                                 \
  do_option("Trace")                                 \
  do_option("Profile")                               \
  do_option("Perf")                                  \
  do_option("CI")                                    \
  do_option("HeapDump")                              \
  do_option("Unlock")                                \
  do_option("Log")                                   \
  do_option("BackgroundCompilation")                 \
  do_option("UseCompressedOops")                     \
  do_option("UseCompressedClassPointers")            \
  do_option("UseUTF8UTF16Intrinsics")                \
  do_option("IgnoreUnrecognizedVMOptions")           \
  do_option("ClassUnloading")                        \
  do_option("CMSClassUnloadingEnabled")              \
  do_option("DisableExplicitGC")                     \
  do_option("UseSharedSpaces")                       \
  do_option("CMSIgnoreYoungGenPerWorker")            \
  do_option("CMSScavengeBeforeRemark")               \
  do_option("FastTLABRefill")                        \
  do_option("UseAdaptiveSizePolicy")                 \
  do_option("UseSHM")                                \
  do_option("CMSCleanOnEnter")                       \
  do_option("ParGCUseLocalOverflow")                 \
  do_option("CMSScavengeBeforeRemark")               \
  do_option("IgnoreNoShareValue")                    \
  do_option("UseGCOverheadLimit")                    \
  do_option("UseContainerSupport")                   \
  do_option("CMSConcurrentMTEnabled")                \


bool CodeReviveFingerprint::in_white_list(const char* name) {
  #define WHITE_LIST_CHECK(option_name)                         \
    if (strncmp(name, option_name, strlen(option_name)) == 0) { \
      return true;                                              \
    }
  WHITE_LIST_DO(WHITE_LIST_CHECK)
  #undef WHITE_LIST_CHECK
  return false;
}

bool CodeReviveFingerprint::invalid_gc_option(const char* name) {
  if (!UseG1GC) {
    // Filter the options starting with G1
    if (strncmp(name, "G1", 2) == 0) {
      return true;
    }
  } else if (!UseConcMarkSweepGC) {
    // Filter the options starting with CMS
    if (strncmp(name, "CMS", 3) == 0) {
      return true;
    }
    // Filter the options starting with UseCMS
    if (strncmp(name, "UseCMS", 6) == 0) {
      return true;
    }
  }
  return false;
}

void CodeReviveFingerprint::setup() {
  _header = (FingerPrintHeader*)_start;

  setup_options();
  setup_narrow_ptr();
}

void CodeReviveFingerprint::setup_options() {
  _header->_obj_alignment = ObjectAlignmentInBytes;
  _header->_contendedPaddingWidth = ContendedPaddingWidth;
  _header->_fieldsAllocationStyle = FieldsAllocationStyle;

  // get the validated flags
  const int length = (int) Flag::numFlags - 1;
  Flag* flag = NULL;
  const char* name = NULL;
  GrowableArray<int>* validate_bool_flag_array = new GrowableArray<int>();
  for (int i = 0; i < length; i++) {
    flag = &Flag::flags[i];
    if (flag->is_diagnostic() || flag->is_experimental() || flag->is_develop()) {
      continue;
    }
    if (flag->is_bool() && flag->is_unlocked()) {
      name = flag->_name;
      if (invalid_gc_option(name)) {
        continue;
      }
      if (in_white_list(name)) {
        continue;
      }
      validate_bool_flag_array->append(i);
    }
  }
  _header->_bool_flag_count = validate_bool_flag_array->length();
  _size = align_up(sizeof(FingerPrintHeader), CodeReviveFile::alignment());

  char* cur_pos = _start + _size;
  size_t bool_flag_size = sizeof(BoolFlag);
  int index = 0;
  BoolFlag* bflag = NULL;
  for (int i = 0; i < _header->_bool_flag_count; i++) {
    index = validate_bool_flag_array->at(i);
    bflag = (BoolFlag*) (cur_pos + bool_flag_size * i);
    bflag->_index = index;
    flag = &Flag::flags[index];
    bflag->_value = flag->get_bool();
  }

  _size += bool_flag_size * _header->_bool_flag_count;
}

void CodeReviveFingerprint::setup_narrow_ptr() {
  _header->_narrow_oop._base = Universe::narrow_oop_base();
  _header->_narrow_oop._shift = Universe::narrow_oop_shift();
  _header->_narrow_oop._use_implicit_null_checks = Universe::narrow_oop_use_implicit_null_checks();
  _header->_narrow_klass._base = Universe::narrow_klass_base();
  _header->_narrow_klass._shift = Universe::narrow_klass_shift();
  _header->_narrow_klass._use_implicit_null_checks = Universe::narrow_klass_use_implicit_null_checks();
}

bool CodeReviveFingerprint::validate() {
  _header = (FingerPrintHeader*)_start;
  if (!validate_options()) {
    return false;
  }
  if (!validate_narrow_ptr()) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Different setting of CompressedOops.\n");
    return false;
  }
  return true;
}

bool CodeReviveFingerprint::validate_options() {
  if (_header->_obj_alignment != ObjectAlignmentInBytes) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Different setting of object alignment.\n")
    return false;
  }

  if (_header->_contendedPaddingWidth != ContendedPaddingWidth) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Different setting of ContendedPaddingWidth.\n")
    return false;
  }

  if (_header->_fieldsAllocationStyle != FieldsAllocationStyle) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Different setting of FieldsAllocationStyle.\n")
    return false;
  }

  char* cur_pos = _start + align_up(sizeof(FingerPrintHeader), CodeReviveFile::alignment());
  size_t bool_flag_size = sizeof(BoolFlag);
  int index = 0;
  BoolFlag* bflag = NULL;
  Flag* flag = NULL;
  for (int i = 0; i < _header->_bool_flag_count; i++) {
    bflag = (BoolFlag*) (cur_pos + bool_flag_size * i);
    index = bflag->_index;
    flag = &Flag::flags[index];
    if (bflag->_value != flag->get_bool()) {
      CR_LOG(CodeRevive::log_kind(), cr_fail, "Different value of %s.\n", flag->_name);
      return false;
    }
  }
  return true;
}

static bool is_same_oop_nps(NarrowPtrStruct a, NarrowPtrStruct b) {
  if ((a._base == 0) ^ (b._base == 0)) {
    return false;
  }
  if (a._shift != b._shift) {
    return false;
  }
  if (a._use_implicit_null_checks != b._use_implicit_null_checks) {
    return false;
  }
  return true;
}

bool CodeReviveFingerprint::validate_narrow_ptr() {
  NarrowPtrStruct oop_nps = { Universe::narrow_oop_base(),
                              Universe::narrow_oop_shift(),
                              Universe::narrow_oop_use_implicit_null_checks() };
  NarrowPtrStruct kls_nps = { Universe::narrow_klass_base(),
                              Universe::narrow_klass_shift(),
                              Universe::narrow_klass_use_implicit_null_checks() };

  if (!is_same_oop_nps(_header->_narrow_oop, oop_nps)) {
    return false;
  }
  if (!is_same_oop_nps(_header->_narrow_klass, kls_nps)) {
    return false;
  }

  return true;
}

// In merge, directly compare the memory for the fingerprint of each container
bool CodeReviveFingerprint::is_same(CodeReviveFingerprint* fingerprint) {
  FingerPrintHeader* cur = (FingerPrintHeader*)_start;
  FingerPrintHeader* other = (FingerPrintHeader*)fingerprint->_start;
  if (!is_same_oop_nps(cur->_narrow_oop, other->_narrow_oop)) {
    return false;
  }
  if (!is_same_oop_nps(cur->_narrow_klass, other->_narrow_klass)) {
    return false;
  }
  if (cur->_obj_alignment != other->_obj_alignment) {
    return false;
  }
  if (cur->_contendedPaddingWidth != other->_contendedPaddingWidth) {
    return false;
  }
  if (cur->_fieldsAllocationStyle != other->_fieldsAllocationStyle) {
    return false;
  }
  if (cur->_bool_flag_count != other->_bool_flag_count) {
    return false;
  }
  if (_size != fingerprint->_size) {
    return false;
  }
  size_t header_size = align_up(sizeof(FingerPrintHeader), CodeReviveFile::alignment());
  guarantee(_size > header_size, "must be");
  return memcmp(_start + header_size, fingerprint->_start + header_size, _size - header_size) == 0;
}

void CodeReviveFingerprint::print(outputStream* output) {
  _header = (FingerPrintHeader*)_start;
  NarrowPtrStruct* oop_nps = &_header->_narrow_oop;
  NarrowPtrStruct* klass_nps = &_header->_narrow_klass;
  output->print_cr("    ObjectAlignmentInBytes = %d", _header->_obj_alignment);
  output->print_cr("    ContendedPaddingWidth  = %d", _header->_contendedPaddingWidth);
  output->print_cr("    FieldsAllocationStyle  = %d", _header->_fieldsAllocationStyle);
  output->print_cr("    NarrowPtrStruct for UseCompressedOops");
  output->print_cr("      base  = %p", oop_nps->_base);
  output->print_cr("      shift = %d", oop_nps->_shift);
  output->print_cr("    NarrowPtrStruct for UseCompressedClassPointers");
  output->print_cr("      base  = %p", klass_nps->_base);
  output->print_cr("      shift = %d", klass_nps->_shift);

  // print options with non default value
  output->print("    Option with non default value: ");
  char* cur_pos = _start + align_up(sizeof(FingerPrintHeader), CodeReviveFile::alignment());
  size_t bool_flag_size = sizeof(BoolFlag);
  int index = 0;
  BoolFlag* bflag = NULL;
  Flag* flag = NULL;
  int print_index = 0;
  for (int i = 0; i < _header->_bool_flag_count; i++) {
    bflag = (BoolFlag*) (cur_pos + bool_flag_size * i);
    index = bflag->_index;
    flag = &Flag::flags[index];
    if ((bflag->_value != flag->get_bool() && flag->is_default()) ||
        (bflag->_value == flag->get_bool() && !flag->is_default())) {
      if (print_index++ % 5 == 0) {
        output->cr();
        output->print("      ");
      }
      if (bflag->_value == true) {
        output->print(" -XX:+%s", flag->_name);
      } else {
        output->print(" -XX:-%s", flag->_name);
      }
    }
  }
  output->cr();
}
