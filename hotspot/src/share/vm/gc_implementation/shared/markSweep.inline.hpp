/*
 * Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_MARKSWEEP_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_MARKSWEEP_INLINE_HPP

#include "gc_implementation/shared/markSweep.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "utilities/stack.inline.hpp"
#include "utilities/macros.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1StringDedup.hpp"
#include "gc_implementation/parallelScavenge/psParallelCompact.hpp"
#endif // INCLUDE_ALL_GCS

inline void MarkSweep::mark_object(oop obj) {
  assert(!CMSParallelFullGC, "sanity check");
#if INCLUDE_ALL_GCS
  if (G1StringDedup::is_enabled()) {
    // We must enqueue the object before it is marked
    // as we otherwise can't read the object's age.
    G1StringDedup::enqueue_from_mark(obj);
  }
#endif
  // some marks may contain information we need to preserve so we store them away
  // and overwrite the mark.  We'll restore it at the end of markSweep.
  markOop mark = obj->mark();
  obj->set_mark(markOopDesc::prototype()->set_marked());

  if (mark->must_be_preserved(obj)) {
    preserve_mark(obj, mark);
  }
}

inline void MarkSweep::follow_klass(Klass* klass) {
  oop op = klass->klass_holder();
  MarkSweep::mark_and_push(&op);
}

inline bool MarkSweep::par_mark_object(oop obj) {
  assert(CMSParallelFullGC, "sanity check");
  // some marks may contain information we need to preserve so we store them away
  // and overwrite the mark.  We'll restore it at the end of markSweep.
  markOop mark = obj->mark();

  if (_pms_mark_bit_map->is_marked(obj)) {
    // Already marked. Just return.
    return false;
  }

  if (_pms_mark_bit_map->mark(obj)) {
    if (mark->must_be_preserved(obj)) {
      preserve_mark(obj, mark);
    }
    // Record the live size per region during marking so that Phase 2
    // (forwarding) doesn't need to. This code is performance
    // critical for marking.
    PMSRegionArraySet* region_array_set = MarkSweep::pms_region_array_set();
    PMSRegionArray* regions = region_array_set->fast_region_array_for((HeapWord*)obj);
    assert(regions != NULL, "Must not be null");
    PMSRegion* region = regions->region_for_addr((HeapWord*)obj);
    size_t live_size = obj->size();
    region->add_to_live_size(live_size);
    region->add_to_cfls_live_size(CompactibleFreeListSpace::adjustObjectSize(live_size));
    return true;
  }

  return false;
}

inline bool MarkSweep::is_object_marked(oop obj) {
  if (!CMSParallelFullGC) {
    return obj->mark()->is_marked();
  } else {
    return _pms_mark_bit_map->is_marked(obj);
  }

}

template <class T> inline void MarkSweep::follow_root(T* p) {
  assert(!Universe::heap()->is_in_reserved(p),
         "roots shouldn't be things within the heap");
  T heap_oop = oopDesc::load_heap_oop(p);
  if (!oopDesc::is_null(heap_oop)) {
    oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
    if (!is_object_marked(obj)) {
      if (!CMSParallelFullGC) {
        mark_object(obj);
        obj->follow_contents();
      } else {
        if (par_mark_object(obj)) {
          obj->follow_contents();
        }
      }
    }
  }
  follow_stack();
}

typedef OverflowTaskQueue<oop, mtGC>          ObjTaskQueue;
typedef OverflowTaskQueue<ObjArrayTask, mtGC> ObjArrayTaskQueue;

template <class T> inline void MarkSweep::mark_and_push(T* p) {
//  assert(Universe::heap()->is_in_reserved(p), "should be in object space");
  T heap_oop = oopDesc::load_heap_oop(p);
  if (!oopDesc::is_null(heap_oop)) {
    oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
    if (!is_object_marked(obj)) {
      if (!CMSParallelFullGC) {
        mark_object(obj);
        _marking_stack.push(obj);
      } else {
        if (par_mark_object(obj)) {
          NamedThread* thr = Thread::current()->as_Named_thread();
          bool res = ((ObjTaskQueue*)thr->_pms_task_queue)->push(obj);
          assert(res, "Low water mark should be less than capacity?");
        }
      }
    }
  }
}

void MarkSweep::push_objarray(oop obj, size_t index) {
  ObjArrayTask task(obj, index);
  assert(task.is_valid(), "bad ObjArrayTask");
  if (!CMSParallelFullGC) {
    _objarray_stack.push(task);
  } else {
    NamedThread* thr = Thread::current()->as_Named_thread();
    bool res = ((ObjArrayTaskQueue*)thr->_pms_objarray_task_queue)->push(task);
    assert(res, "Low water mark should be less than capacity?");
  }
}

template <class T> inline void MarkSweep::adjust_pointer(T* p) {
  T heap_oop = oopDesc::load_heap_oop(p);
  if (!oopDesc::is_null(heap_oop)) {
    oop obj     = oopDesc::decode_heap_oop_not_null(heap_oop);
    oop new_obj = oop(obj->mark()->decode_pointer());
    assert(new_obj != NULL ||                         // is forwarding ptr?
           obj->mark() == markOopDesc::prototype() || // not gc marked?
           (UseBiasedLocking && obj->mark()->has_bias_pattern()),
                                                      // not gc marked?
           "should be forwarded");
    if (new_obj != NULL) {
      assert(Universe::heap()->is_in_reserved(new_obj),
             "should be in object space");
      oopDesc::encode_store_heap_oop_not_null(p, new_obj);
    }
  }
}

template <class T> inline void MarkSweep::KeepAliveClosure::do_oop_work(T* p) {
  mark_and_push(p);
}

inline size_t PMSMarkBitMap::addr_to_bit(HeapWord* addr) const {
  assert(addr >= _start, "addr too small");
  // Note: the addr == _start + _size case is valid for the
  // (exclusive) right index for the iterate() call.
  assert(addr <= _start + _size, "addr too large");
  assert((intptr_t)addr % MinObjAlignmentInBytes == 0,
         "Non-object-aligned address is suspicious");
  return pointer_delta(addr, _start) >> _shifter;
}

inline HeapWord* PMSMarkBitMap::bit_to_addr(size_t bit) const {
  assert(bit < _bits.size(), "bit out of range");
  return _start + (bit << _shifter);
}

inline bool PMSMarkBitMap::mark(oop obj) {
  size_t bit = addr_to_bit((HeapWord*)obj);
  return _bits.par_set_bit(bit);
}

inline bool PMSMarkBitMap::is_marked(oop obj) const {
  size_t bit = addr_to_bit((HeapWord*)obj);
  return _bits.at(bit);
}

inline bool PMSMarkBitMap::iterate(BitMapClosure* cl) {
  return _bits.iterate(cl);
}

inline bool PMSMarkBitMap::iterate(BitMapClosure* cl, size_t left, size_t right) {
  return _bits.iterate(cl, left, right);
}

inline size_t PMSMarkBitMap::count_one_bits() const {
  return _bits.count_one_bits();
}

inline void PMSMarkBitMap::clear() {
  return _bits.clear();
}

inline bool PMSMarkBitMap::isAllClear() const {
  return _bits.is_empty();
}

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_MARKSWEEP_INLINE_HPP
