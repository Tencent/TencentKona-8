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

#include "precompiled.hpp"
#include "gc_implementation/g1/g1CollectedHeap.hpp"
#include "gc_implementation/g1/concurrentMark.inline.hpp"
#include "gc_implementation/g1/g1FullCollector.hpp"
#include "gc_implementation/g1/g1FullGCAdjustTask.hpp"
#include "gc_implementation/g1/g1FullGCCompactionPoint.hpp"
#include "gc_implementation/g1/g1FullGCMarker.hpp"
#include "gc_implementation/g1/g1FullGCOopClosures.inline.hpp"
#include "gc_implementation/g1/heapRegion.inline.hpp"
#include "gc_implementation/shared/gcTraceTime.hpp"
#include "memory/referenceProcessor.hpp"
#include "memory/iterator.inline.hpp"

class G1AdjustLiveClosure : public FullGCPhaseClosure {
  G1AdjustClosure* _adjust_closure;
public:
  G1AdjustLiveClosure(G1AdjustClosure* cl) :
    _adjust_closure(cl) { }

  virtual size_t apply(oop object) {
    size_t s = object->oop_iterate(_adjust_closure);
    return s;
  }
};

class G1AdjustRegionClosure : public HeapRegionClosure {
  CMBitMap* _bitmap;
  uint _worker_id;
 public:
  G1AdjustRegionClosure(CMBitMap* bitmap, uint worker_id) :
    _bitmap(bitmap),
    _worker_id(worker_id) { }

  bool doHeapRegion(HeapRegion* r) {
    G1AdjustClosure cl;
    if (r->isHumongous()) {
      if (r->startsHumongous()) {
        oop obj = oop(r->bottom());
        if (_bitmap->isMarked(obj)) {
          obj->oop_iterate(&cl);
        }
      }
    } else {
      G1AdjustLiveClosure adjust(&cl);
      r->apply_to_marked_objects(_bitmap, &adjust);
    }
    return false;
  }
};

G1FullGCAdjustTask::G1FullGCAdjustTask(G1FullCollector* collector) :
    G1FullGCTask("G1 Adjust", collector),
    _root_processor(G1CollectedHeap::heap()),
    _hrclaimer(collector->workers()),
    _adjust(),
    _adjust_string_dedup(NULL, &_adjust, G1StringDedup::is_enabled()) {
  // Need cleared claim bits for the roots processing
  ClassLoaderDataGraph::clear_claimed_marks();
}

void G1FullGCAdjustTask::work(uint worker_id) {
  Ticks start = Ticks::now();
  ResourceMark rm;

  // Adjust preserved marks first since they are not balanced.
  G1FullGCMarker* marker = collector()->marker(worker_id);
  marker->preserved_stack()->adjust_during_full_gc();

  // Adjust the weak_roots.
  CLDToOopClosure adjust_cld(&_adjust);
  CodeBlobToOopClosure adjust_code(&_adjust, CodeBlobToOopClosure::FixRelocations);
  _root_processor.process_full_gc_weak_roots(&_adjust);

  // Needs to be last, process_all_roots calls all_tasks_completed(...).
  _root_processor.process_all_roots(
      &_adjust,
      &adjust_cld,
      &adjust_code);

  // Adjust string dedup if enabled.
  if (G1StringDedup::is_enabled()) {
    G1StringDedup::parallel_unlink(&_adjust_string_dedup, worker_id);
  }

  // Now adjust pointers region by region
  G1AdjustRegionClosure blk(collector()->mark_bitmap(), worker_id);
  G1CollectedHeap::heap()->heap_region_par_iterate_from_worker_offset(&blk, &_hrclaimer, worker_id);
}
