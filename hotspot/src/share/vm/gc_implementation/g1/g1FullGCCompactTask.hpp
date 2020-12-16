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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCCOMPACTTASK_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCCOMPACTTASK_HPP

#include "gc_implementation/g1/g1FullGCCompactionPoint.hpp"
#include "gc_implementation/g1/g1FullGCScope.hpp"
#include "gc_implementation/g1/g1FullGCTask.hpp"
#include "gc_implementation/g1/g1StringDedup.hpp"
#include "gc_implementation/g1/heapRegionManager.hpp"
#include "memory/referenceProcessor.hpp"

class G1CollectedHeap;
class CMBitMap;
class HeapRegionClaimer;

class G1FullGCCompactTask : public G1FullGCTask {
protected:
  HeapRegionClaimer _claimer;

private:
  void compact_region(HeapRegion* hr);

public:
  G1FullGCCompactTask(G1FullCollector* collector) :
    G1FullGCTask("G1 Compact Task", collector),
    _claimer(collector->workers()) { }
  void work(uint worker_id);
  void serial_compaction();

  class G1CompactRegionClosure : public FullGCPhaseClosure {
    CMBitMap* _bitmap;

  public:
    G1CompactRegionClosure(CMBitMap* bitmap) : _bitmap(bitmap) { }
    virtual size_t apply(oop object);
  };
};

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCCOMPACTTASK_HPP
