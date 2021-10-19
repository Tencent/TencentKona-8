/*
 *
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_EXECUTION_UNIT_HPP
#define SHARE_VM_RUNTIME_EXECUTION_UNIT_HPP

#include "runtime/coroutine.hpp"
#if INCLUDE_KONA_FIBER
typedef Coroutine ExecutionType;
#else
#include "runtime/thread.hpp"
typedef JavaThread ExecutionType;
#endif

class ExecutionUnit: AllStatic {
public:
  static ExecutionType* get_execution_unit(oop threadObj);
};

class ExecutionUnitsIterator VALUE_OBJ_CLASS_SPEC {
private:
  ExecutionType* _cur;
#if INCLUDE_KONA_FIBER
  size_t         _cur_bucket_index;
#endif
public:
  ExecutionUnitsIterator();
  ExecutionType* next();
};
#endif // SHARE_VM_RUNTIME_EXECUTION_UNIT_HPP
