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
#include "precompiled.hpp"
#include "cr/codeReviveCodeSpace.hpp"
#include "cr/codeReviveCodeBlob.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/align.hpp"
#include "cr/revive.hpp"

CodeReviveCodeSpace::CodeReviveCodeSpace(char* start, char* limit, bool load) {
  _start = start;
  _limit = limit;
  _cur = load ? _limit : _start;
  _alignment = 8;
}

int CodeReviveCodeSpace::saveCode(CodeBlob* cb, CodeReviveMetaSpace* meta_space) {
  char* save_pos = _cur;

  CodeReviveCodeBlob rcb(save_pos, _limit, cb, meta_space);
  if (rcb.save()) {
    _cur += align_up(rcb.size(), _alignment);
    return (int)(save_pos - _start);
  } else {
    return -1;
  }
}

void CodeReviveCodeSpace::print() {
  CodeRevive::out()->print_cr("CodeSpace [%p, %p), size %d", _start, _cur, (int)(_cur - _start));
}
