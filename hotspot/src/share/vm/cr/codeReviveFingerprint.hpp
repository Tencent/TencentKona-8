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

#ifndef SHARE_VM_CR_CODEREVIVEFINGERPRINT_HPP
#define SHARE_VM_CR_CODEREVIVEFINGERPRINT_HPP

#include "memory/allocation.hpp"
#include "memory/universe.hpp"

/*
 * CodeReviveFingerprint stores jvm options which may affect the behavior of code generation.
 * Currently, use white list to filter the hurtless options
 *
 * CodeReviveFingerprint Format
 * -- Header
 *    -- value for the below options
 *       -- ObjectAlignmentInBytes
 *       -- ContendedPaddingWidth
 *       -- FieldsAllocationStyle
 *    -- NarrowPtrStruct for UseCompressedOops and UseCompressedClassPointers
 *    -- count for boolean option
 * -- boolean option array
 *    -- index in Flag::flags
 *    -- value of the option
 */
class CodeReviveFingerprint: public CHeapObj<mtInternal> {
 public:
  CodeReviveFingerprint(char* start) : _start(start), _size(0) {}
  CodeReviveFingerprint(char* start, size_t size) : _start(start), _size(size) {};
  void   setup();
  bool   validate();
  size_t size()  { return _size; }
  char*  start() { return _start; }
  bool   is_same(CodeReviveFingerprint* finger);
  void   print(outputStream* output);

  struct BoolFlag {
    int  _index;
    bool _value;
  };

  struct FingerPrintHeader {
    int                _obj_alignment;            // value of ObjectAlignmentInBytes
    int                _contendedPaddingWidth;    // value of ContendedPaddingWidth
    int                _fieldsAllocationStyle;    // value of FieldsAllocationStyle
    int                _bool_flag_count;          // count of the validated flags whose value type is bool
    NarrowPtrStruct    _narrow_oop;               // For UseCompressedOops.
    NarrowPtrStruct    _narrow_klass;             // For UseCompressedClassPointers.
  };

 private:
  char*               _start;
  size_t              _size;
  FingerPrintHeader*  _header;
  BoolFlag*           _flag_array;
  void setup_options();
  void setup_narrow_ptr();
  bool validate_options();
  bool validate_narrow_ptr();
  bool in_white_list(const char* name);
  bool invalid_gc_option(const char* name);
};

#endif // SHARE_VM_CR_CODEREVIVEFINGERPRINT_HPP
