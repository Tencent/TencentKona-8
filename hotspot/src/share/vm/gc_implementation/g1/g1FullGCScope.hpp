/*
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCSCOPE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCSCOPE_HPP

#include "gc_implementation/g1/g1CollectedHeap.hpp"
#include "gc_implementation/g1/g1HeapTransition.hpp"
#include "gc_implementation/shared/collectorCounters.hpp"
#include "gc_implementation/shared/gcId.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "gc_implementation/shared/gcTraceTime.hpp"
#include "gc_implementation/shared/gcTimer.hpp"
#include "gc_implementation/shared/isGCActiveMark.hpp"
#include "gc_implementation/shared/vmGCOperations.hpp"
#include "memory/allocation.hpp"
#include "services/memoryService.hpp"

class GCMemoryManager;

// Class used to group scoped objects used in the Full GC together.
class G1FullGCScope : public StackObj {
  ResourceMark            _rm;
  bool                    _explicit_gc;
  G1CollectedHeap*        _g1h;
  GCIdMark                _gc_id;
  SvcGCMarker             _svc_marker;
  STWGCTimer              _timer;
  G1FullGCTracer          _tracer;
  GCTraceCPUTime          _cpu_time;
  ClearedAllSoftRefs      _soft_refs;
  TraceCollectorStats     _collector_stats;
  TraceMemoryManagerStats _memory_stats;
  G1HeapTransition        _heap_transition;

public:
  G1FullGCScope(GCMemoryManager* memory_manager, bool explicit_gc, bool clear_soft);
  ~G1FullGCScope();

  bool is_explicit_gc();
  bool should_clear_soft_refs();

  STWGCTimer* timer();
  G1FullGCTracer* tracer();
  G1HeapTransition* heap_transition();
};

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCSCOPE_HPP
