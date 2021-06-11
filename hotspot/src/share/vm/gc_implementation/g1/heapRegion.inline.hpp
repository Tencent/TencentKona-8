/*
 * Copyright (c) 2001, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_HEAPREGION_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_HEAPREGION_INLINE_HPP

#include "gc_implementation/g1/g1BlockOffsetTable.inline.hpp"
#include "gc_implementation/g1/g1CollectedHeap.hpp"
#include "gc_implementation/g1/heapRegion.hpp"
#include "memory/space.hpp"
#include "runtime/atomic.inline.hpp"

// This version requires locking.
inline HeapWord* G1OffsetTableContigSpace::allocate_impl(size_t size,
                                                HeapWord* const end_value) {
  HeapWord* obj = top();
  if (pointer_delta(end_value, obj) >= size) {
    HeapWord* new_top = obj + size;
    set_top(new_top);
    assert(is_aligned(obj) && is_aligned(new_top), "checking alignment");
    return obj;
  } else {
    return NULL;
  }
}

// This version is lock-free.
inline HeapWord* G1OffsetTableContigSpace::par_allocate_impl(size_t size,
                                                    HeapWord* const end_value) {
  do {
    HeapWord* obj = top();
    if (pointer_delta(end_value, obj) >= size) {
      HeapWord* new_top = obj + size;
      HeapWord* result = (HeapWord*)Atomic::cmpxchg_ptr(new_top, top_addr(), obj);
      // result can be one of two:
      //  the old top value: the exchange succeeded
      //  otherwise: the new value of the top is returned.
      if (result == obj) {
        assert(is_aligned(obj) && is_aligned(new_top), "checking alignment");
        return obj;
      }
    } else {
      return NULL;
    }
  } while (true);
}

inline HeapWord* G1OffsetTableContigSpace::allocate(size_t size) {
  HeapWord* res = allocate_impl(size, end());
  if (res != NULL) {
    _offsets.alloc_block(res, size);
  }
  return res;
}

// Because of the requirement of keeping "_offsets" up to date with the
// allocations, we sequentialize these with a lock.  Therefore, best if
// this is used for larger LAB allocations only.
inline HeapWord* G1OffsetTableContigSpace::par_allocate(size_t size) {
  MutexLocker x(&_par_alloc_lock);
  return allocate(size);
}

inline HeapWord* G1OffsetTableContigSpace::block_start(const void* p) {
  return _offsets.block_start(p);
}

inline HeapWord*
G1OffsetTableContigSpace::block_start_const(const void* p) const {
  return _offsets.block_start_const(p);
}

inline void HeapRegion::complete_compaction() {
  // Reset space and bot after compaction is complete if needed.
  reset_after_compaction();

  /* We do this clear, below, since it has overloaded meanings for some
   space subtypes.  For example, OffsetTableContigSpace's that were
   compacted into will have had their offset table thresholds updated
   continuously, but those that weren't need to have their thresholds
   re-initialized.  Also mangles unused area for debugging.*/
  if (used() == 0) {                                               
    clear(SpaceDecorator::Mangle);
  } else {
    // Clear unused heap memory in debug builds.
    if (ZapUnusedHeapArea) {
      mangle_unused_area();
    }
  }
}

inline bool
HeapRegion::is_obj_dead_with_size(const oop obj, CMBitMapRO* prev_bitmap, size_t* size) const {
  HeapWord* addr = (HeapWord*) obj;

  assert(addr < top(), "must be");
  assert(!isHumongous(), "Humongous objects not handled here");
  bool obj_is_dead = is_obj_dead(obj, prev_bitmap);

  if (ClassUnloadingWithConcurrentMark && obj_is_dead) {
    assert(!block_is_obj(addr), "must be");
    *size = block_size_using_bitmap(addr, prev_bitmap);
  } else {
    assert(block_is_obj(addr), "must be");
    *size = obj->size();
  }
  return obj_is_dead;
}

inline bool
HeapRegion::block_is_obj(const HeapWord* p) const {
  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  if (ClassUnloadingWithConcurrentMark) {
    return !g1h->is_obj_dead(oop(p), this);
  }
  return p < top();
}

inline size_t HeapRegion::block_size_using_bitmap(const HeapWord* addr, const CMBitMapRO* prev_bitmap) const {
  assert(ClassUnloadingWithConcurrentMark,
         err_msg("All blocks should be objects if class unloading isn't used, so this method should not be called. "
                 "HR: [" PTR_FORMAT ", " PTR_FORMAT ", " PTR_FORMAT ") "
                 "addr: " PTR_FORMAT,
                 p2i(bottom()), p2i(top()), p2i(end()), p2i(addr)));

  // Old regions' dead objects may have dead classes
  // We need to find the next live object using the bitmap
  HeapWord* next = prev_bitmap->getNextMarkedWordAddress(addr, prev_top_at_mark_start());

  assert(next > addr, "must get the next live object");
  return pointer_delta(next, addr);
}

inline bool HeapRegion::is_obj_dead(const oop obj, const CMBitMapRO* prev_bitmap) const {
  assert(is_in_reserved(obj), err_msg("Object " PTR_FORMAT " must be in region", p2i(obj)));
  return !obj_allocated_since_prev_marking(obj) && !prev_bitmap->isMarked((HeapWord*)obj);
}

inline size_t
HeapRegion::block_size(const HeapWord *addr) const {
  if (addr == top()) {
    return pointer_delta(end(), addr);
  }

  if (block_is_obj(addr)) {
    return oop(addr)->size();
  }

  return block_size_using_bitmap(addr, G1CollectedHeap::heap()->concurrent_mark()->prevMarkBitMap());
}

inline HeapWord* HeapRegion::par_allocate_no_bot_updates(size_t word_size) {
  assert(is_young(), "we can only skip BOT updates on young regions");
  return par_allocate_impl(word_size, end());
}

inline HeapWord* HeapRegion::allocate_no_bot_updates(size_t word_size) {
  assert(is_young(), "we can only skip BOT updates on young regions");
  return allocate_impl(word_size, end());
}

inline void HeapRegion::note_start_of_marking() {
  _next_marked_bytes = 0;
  _next_top_at_mark_start = top();
}

inline void HeapRegion::note_end_of_marking() {
  _prev_top_at_mark_start = _next_top_at_mark_start;
  _prev_marked_bytes = _next_marked_bytes;
  _next_marked_bytes = 0;

  assert(_prev_marked_bytes <=
         (size_t) pointer_delta(prev_top_at_mark_start(), bottom()) *
         HeapWordSize, "invariant");
}

inline void HeapRegion::note_start_of_copying(bool during_initial_mark) {
  if (is_survivor()) {
    // This is how we always allocate survivors.
    assert(_next_top_at_mark_start == bottom(), "invariant");
  } else {
    if (during_initial_mark) {
      // During initial-mark we'll explicitly mark any objects on old
      // regions that are pointed to by roots. Given that explicit
      // marks only make sense under NTAMS it'd be nice if we could
      // check that condition if we wanted to. Given that we don't
      // know where the top of this region will end up, we simply set
      // NTAMS to the end of the region so all marks will be below
      // NTAMS. We'll set it to the actual top when we retire this region.
      _next_top_at_mark_start = end();
    } else {
      // We could have re-used this old region as to-space over a
      // couple of GCs since the start of the concurrent marking
      // cycle. This means that [bottom,NTAMS) will contain objects
      // copied up to and including initial-mark and [NTAMS, top)
      // will contain objects copied during the concurrent marking cycle.
      assert(top() >= _next_top_at_mark_start, "invariant");
    }
  }
}

template<typename ApplyToMarkedClosure>
inline void HeapRegion::apply_to_marked_objects(CMBitMap* bitmap, ApplyToMarkedClosure* closure) {
  HeapWord* limit = scan_limit();
  HeapWord* next_addr = bottom();

  while (next_addr < limit) {
    Prefetch::write(next_addr, PrefetchScanIntervalInBytes);
    // This explicit is_marked check is a way to avoid
    // some extra work done by get_next_marked_addr for
    // the case where next_addr is marked.
    if (bitmap->isMarked(next_addr)) {
      oop current = oop(next_addr);
      next_addr += closure->apply(current);
    } else {
      next_addr = bitmap->getNextMarkedWordAddress(next_addr, limit);
    }
  }

  assert(next_addr == limit, "Should stop the scan at the limit.");
}

inline void HeapRegion::note_end_of_copying(bool during_initial_mark) {
  if (is_survivor()) {
    // This is how we always allocate survivors.
    assert(_next_top_at_mark_start == bottom(), "invariant");
  } else {
    if (during_initial_mark) {
      // See the comment for note_start_of_copying() for the details
      // on this.
      assert(_next_top_at_mark_start == end(), "pre-condition");
      _next_top_at_mark_start = top();
    } else {
      // See the comment for note_start_of_copying() for the details
      // on this.
      assert(top() >= _next_top_at_mark_start, "invariant");
    }
  }
}

inline bool HeapRegion::in_collection_set() const {
  return G1CollectedHeap::heap()->is_in_cset(this);
}

template <class Closure, bool is_gc_active>
bool HeapRegion::do_oops_on_card_in_humongous(MemRegion mr,
                                              Closure* cl,
                                              G1CollectedHeap* g1h) {
  assert(isHumongous(), "precondition");
  HeapRegion* sr = humongous_start_region();
  oop obj = oop(sr->bottom());

  // If concurrent and klass_or_null is NULL, then space has been
  // allocated but the object has not yet been published by setting
  // the klass.  That can only happen if the card is stale.  However,
  // we've already set the card clean, so we must return failure,
  // since the allocating thread could have performed a write to the
  // card that might be missed otherwise.
  if (!is_gc_active && (obj->klass_or_null_acquire() == NULL)) {
    return false;
  }

  // We have a well-formed humongous object at the start of sr.
  // Only filler objects follow a humongous object in the containing
  // regions, and we can ignore those.  So only process the one
  // humongous object.
  if (!g1h->is_obj_dead(obj, sr)) {
    if (obj->is_objArray() || (sr->bottom() < mr.start())) {
      // objArrays are always marked precisely, so limit processing
      // with mr.  Non-objArrays might be precisely marked, and since
      // it's humongous it's worthwhile avoiding full processing.
      // However, the card could be stale and only cover filler
      // objects.  That should be rare, so not worth checking for;
      // instead let it fall out from the bounded iteration.
      obj->oop_iterate(cl, mr);
    } else {
      // If obj is not an objArray and mr contains the start of the
      // obj, then this could be an imprecise mark, and we need to
      // process the entire object.
      obj->oop_iterate(cl);
    }
  }
  return true;
}

template <bool is_gc_active, class Closure>
bool HeapRegion::oops_on_card_seq_iterate_careful(MemRegion mr,
                                                  Closure* cl) {
  assert(MemRegion(bottom(), end()).contains(mr), "Card region not in heap region");
  G1CollectedHeap* g1h = G1CollectedHeap::heap();

  // Special handling for humongous regions.
  if (isHumongous()) {
    return do_oops_on_card_in_humongous<Closure, is_gc_active>(mr, cl, g1h);
  }
  assert(is_old(), "precondition");

  // Because mr has been trimmed to what's been allocated in this
  // region, the parts of the heap that are examined here are always
  // parsable; there's no need to use klass_or_null to detect
  // in-progress allocation.

  // Cache the boundaries of the memory region in some const locals
  HeapWord* const start = mr.start();
  HeapWord* const end = mr.end();

  // Find the obj that extends onto mr.start().
  // Update BOT as needed while finding start of (possibly dead)
  // object containing the start of the region.
  HeapWord* cur = block_start(start);

#ifdef ASSERT
  {
    assert(cur <= start,
           err_msg("cur: " PTR_FORMAT ", start: " PTR_FORMAT, p2i(cur), p2i(start)));
    HeapWord* next = cur + block_size(cur);
    assert(start < next,
           err_msg("start: " PTR_FORMAT ", next: " PTR_FORMAT, p2i(start), p2i(next)));
  }
#endif

  CMBitMapRO* bitmap = g1h->concurrent_mark()->prevMarkBitMap();
  do {
    oop obj = oop(cur);
    assert(obj->is_oop(true), err_msg("Not an oop at " PTR_FORMAT, p2i(cur)));
    assert(obj->klass_or_null() != NULL,
           err_msg("Unparsable heap at " PTR_FORMAT, p2i(cur)));

    size_t size;
    bool is_dead = is_obj_dead_with_size(obj, bitmap, &size);

    cur += size;
    if (!is_dead) {
      // Process live object's references.

      // Non-objArrays are usually marked imprecise at the object
      // start, in which case we need to iterate over them in full.
      // objArrays are precisely marked, but can still be iterated
      // over in full if completely covered.
      if (!obj->is_objArray() || (((HeapWord*)obj) >= start && cur <= end)) {
        obj->oop_iterate(cl);
      } else {
        obj->oop_iterate(cl, mr);
      }
    }
  } while (cur < end);

  return true;
}

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_HEAPREGION_INLINE_HPP
