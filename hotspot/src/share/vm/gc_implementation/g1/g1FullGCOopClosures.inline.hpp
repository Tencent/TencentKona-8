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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCOOPCLOSURES_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCOOPCLOSURES_INLINE_HPP

#include "gc_implementation/g1/g1Allocator.hpp"
#include "gc_implementation/g1/g1FullGCMarker.inline.hpp"
#include "gc_implementation/g1/g1FullGCOopClosures.hpp"
#include "gc_implementation/g1/heapRegionRemSet.hpp"
#include "memory/iterator.inline.hpp"
#include "oops/oop.inline.hpp"

template <typename T>
inline void G1MarkAndPushClosure::do_oop_nv(T* p) {
   _marker->mark_and_push(p);
}

template <class T> 
inline void G1AdjustClosure::do_oop_nv(T* p) {
  T heap_oop = oopDesc::load_heap_oop(p);

  if (oopDesc::is_null(heap_oop)) {
    return;
  }
  oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);

  assert(Universe::heap()->is_in(obj), "should be in heap");

  oop forwardee = obj->forwardee();
  if (forwardee == NULL) {
    // Not forwarded, return current reference.
    assert(obj->mark() == markOopDesc::prototype_for_object(obj) || // Correct mark
           obj->mark()->must_be_preserved(obj) || // Will be restored by PreservedMarksSet
           (UseBiasedLocking && obj->has_bias_pattern()), // Will be restored by BiasedLocking
           "Must have correct prototype or be preserved");
    return;
  }

  // Forwarded, just update.
  assert(Universe::heap()->is_in_reserved(forwardee), "should be in object space");
  oopDesc::encode_store_heap_oop(p, forwardee);
}

template<typename T>
inline void G1FullKeepAliveClosure::do_oop_nv(T* p) {
  _marker->mark_and_push(p);
}
#endif //SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCOOPCLOSURES_INLINE_HPP
