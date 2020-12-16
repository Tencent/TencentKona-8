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
#include "gc_implementation/g1/g1FullGCCompactionPoint.hpp"
#include "gc_implementation/g1/g1FullGCCompactTask.hpp"
#include "gc_implementation/g1/heapRegion.inline.hpp"
#include "gc_implementation/shared/gcTraceTime.hpp"
#include "oops/oop.inline.hpp"
#include "utilities/ticks.hpp"

class G1ResetHumongousClosure : public HeapRegionClosure {
  CMBitMap* _bitmap;

public:
  G1ResetHumongousClosure(CMBitMap* bitmap) :
      _bitmap(bitmap) { }

  bool doHeapRegion(HeapRegion* current) {
    if (current->isHumongous()) {
      if (current->startsHumongous()) {
        oop obj = oop(current->bottom());
        if (_bitmap->isMarked(obj)) {
          // Clear bitmap and fix mark word.
          _bitmap->clear(obj);
          obj->init_mark();
        } else {
          assert(current->is_empty(), "Should have been cleared in phase 2.");
        }
      }
      current->reset_during_compaction();
    }
    return false;
  }
};

size_t G1FullGCCompactTask::G1CompactRegionClosure::apply(oop obj) {
  size_t size = obj->size();
  HeapWord* destination = (HeapWord*)obj->forwardee();
  if (destination == NULL) {
    // Object not moving
    return size;
  }

  // copy object and reinit its mark
  HeapWord* obj_addr = (HeapWord*) obj;
  assert(obj_addr != destination, "everything in this pass should be moving");
  Copy::aligned_conjoint_words(obj_addr, destination, size);
  oop(destination)->init_mark();
  assert(oop(destination)->klass() != NULL, "should have a class");

  return size;
}

void G1FullGCCompactTask::compact_region(HeapRegion* hr) {
  assert(!hr->isHumongous(), "Should be no humongous regions in compaction queue");
  G1CompactRegionClosure compact(collector()->mark_bitmap());
  hr->apply_to_marked_objects(collector()->mark_bitmap(), &compact);
  // Once all objects have been moved the liveness information
  // needs be cleared.
  collector()->mark_bitmap()->clearRange(hr);
  hr->complete_compaction();
}

void G1FullGCCompactTask::work(uint worker_id) {
  Ticks start = Ticks::now();
  GrowableArray<HeapRegion*>* compaction_queue = collector()->compaction_point(worker_id)->regions();
  for (GrowableArrayIterator<HeapRegion*> it = compaction_queue->begin();
       it != compaction_queue->end();
       ++it) {
    compact_region(*it);
  }

  G1ResetHumongousClosure hc(collector()->mark_bitmap());
  G1CollectedHeap::heap()->heap_region_par_iterate_from_worker_offset(&hc, &_claimer, worker_id);
}

void G1FullGCCompactTask::serial_compaction() {
  GrowableArray<HeapRegion*>* compaction_queue = collector()->serial_compaction_point()->regions();
  for (GrowableArrayIterator<HeapRegion*> it = compaction_queue->begin();
       it != compaction_queue->end();
       ++it) {
    compact_region(*it);
  }
}
