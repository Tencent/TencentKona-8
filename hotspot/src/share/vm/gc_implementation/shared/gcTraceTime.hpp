/*
 * Copyright (c) 2012, 2020, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_GCTRACETIME_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_GCTRACETIME_HPP

#include "gc_implementation/shared/gcTrace.hpp"
#include "prims/jni_md.h"
#include "utilities/ticks.hpp"

class GCTimer;

class GCTraceTime {
  const char* _title;
  bool _doit;
  bool _print_cr;
  GCTimer* _timer;
  Ticks _start_counter;

 public:
  GCTraceTime(const char* title, bool doit, bool print_cr, GCTimer* timer, GCId gc_id);
  ~GCTraceTime();
};
class GCTraceCPUTime : public StackObj {
  bool _active;                 // true if times will be measured and printed
  double _starting_user_time;   // user time at start of measurement
  double _starting_system_time; // system time at start of measurement
  double _starting_real_time;   // real time at start of measurement
 public:
  GCTraceCPUTime();
  ~GCTraceCPUTime();
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_GCTRACETIME_HPP
