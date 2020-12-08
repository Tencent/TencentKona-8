/*
 * Copyright (c) 2016, 2020, Oracle and/or its affiliates. All rights reserved.
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
#include "gc_implementation/g1/g1HeapTransition.hpp"
#include "gc_implementation/g1/g1CollectorPolicy.hpp"
#include "memory/metaspace.hpp"

G1HeapTransition::Data::Data(G1CollectedHeap* g1_heap) {
  _eden_length = g1_heap->eden_regions_count();
  _survivor_length = g1_heap->survivor_regions_count();
  _old_length = g1_heap->old_regions_count();
  _humongous_length = g1_heap->humongous_regions_count();
  _metaspace_used_bytes = MetaspaceAux::used_bytes();
}

G1HeapTransition::G1HeapTransition(G1CollectedHeap* g1_heap) : _g1_heap(g1_heap), _before(g1_heap) { }

struct DetailedUsage : public StackObj {
  size_t _eden_used;
  size_t _survivor_used;
  size_t _old_used;
  size_t _humongous_used;

  size_t _eden_region_count;
  size_t _survivor_region_count;
  size_t _old_region_count;
  size_t _humongous_region_count;

  DetailedUsage() :
    _eden_used(0), _survivor_used(0), _old_used(0), _humongous_used(0),
    _eden_region_count(0), _survivor_region_count(0), _old_region_count(0), _humongous_region_count(0) {}
};

class DetailedUsageClosure: public HeapRegionClosure {
public:
  DetailedUsage _usage;
  bool doHeapRegion(HeapRegion* r) {
    if (r->is_old()) {
      _usage._old_used += r->used();
      _usage._old_region_count++;
    } else if (r->is_survivor()) {
      _usage._survivor_used += r->used();
      _usage._survivor_region_count++;
    } else if (r->is_eden()) {
      _usage._eden_used += r->used();
      _usage._eden_region_count++;
    } else if (r->isHumongous()) {
      _usage._humongous_used += r->used();
      _usage._humongous_region_count++;
    } else {
      assert(r->used() == 0, "Expected used to be 0");
    }
    return false;
  }
};

void G1HeapTransition::print() {
  Data after(_g1_heap);

  size_t eden_capacity_length_after_gc = _g1_heap->g1_policy()->young_list_target_length() - after._survivor_length;
  size_t survivor_capacity_length_after_gc = _g1_heap->g1_policy()->max_survivor_regions();

  DetailedUsage usage;
  MetaspaceAux::print_metaspace_change(_before._metaspace_used_bytes);
}
