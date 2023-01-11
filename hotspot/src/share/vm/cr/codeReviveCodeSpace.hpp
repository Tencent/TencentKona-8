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

#ifndef SHARE_VM_CR_CODE_REVIVE_CODE_SPACE_HPP
#define SHARE_VM_CR_CODE_REVIVE_CODE_SPACE_HPP
#include "code/codeBlob.hpp"

/*
 * CodeSpace stores saved code.
 * Code is stored in serial order
 *
 * Format in CodeReviveFile
 * -- CodeReviveCodeBlob 1
 * -- CodeReviveCodeBlob 2
 * -- ...
 * -- CodeReviveCodeBlob n
 */
class CodeReviveMetaSpace;
class CodeReviveCodeSpace : public CHeapObj<mtInternal> {
 public:
  char*                    _start;
  char*                    _limit;
  char*                    _cur;
  intptr_t                 _alignment;

  CodeReviveCodeSpace(char* start, char* limit, bool load);
  int saveCode(CodeBlob* cb, CodeReviveMetaSpace* meta_space);
  void print();
  size_t size() {
    return _cur - _start;
  }
  char* get_code_address(int code_offset) {
    return _start + code_offset;
  }
};
#endif // SHARE_VM_CR_CODE_REVIVE_CODE_SPACE_HPP
