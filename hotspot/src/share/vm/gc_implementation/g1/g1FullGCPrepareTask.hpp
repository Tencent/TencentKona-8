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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCPREPARETASK_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCPREPARETASK_HPP

#include "gc_implementation/g1/g1FullGCCompactionPoint.hpp"
#include "gc_implementation/g1/g1FullGCScope.hpp"
#include "gc_implementation/g1/g1FullGCTask.hpp"
#include "gc_implementation/g1/g1RootProcessor.hpp"
#include "gc_implementation/g1/g1StringDedup.hpp"
#include "gc_implementation/g1/heapRegionManager.hpp"
#include "gc_implementation/g1/heapRegionSet.hpp"
#include "memory/referenceProcessor.hpp"

class CMBitMap;

class G1FullGCPrepareTask : public G1FullGCTask {
protected:
  volatile bool     _freed_regions;
  HeapRegionClaimer _hrclaimer;

  void set_freed_regions();

public:
  G1FullGCPrepareTask(G1FullCollector* collector);
  void work(uint worker_id);
  void prepare_serial_compaction();
  bool has_freed_regions();

protected:
  class G1CalculatePointersClosure : public HeapRegionClosure {
  protected:
    G1CollectedHeap* _g1h;
    CMBitMap* _bitmap;
    G1FullGCCompactionPoint* _cp;
    HeapRegionSetCount _humongous_regions_removed;

    virtual void prepare_for_compaction(HeapRegion* hr);
    void prepare_for_compaction_work(G1FullGCCompactionPoint* cp, HeapRegion* hr);
    void free_humongous_region(HeapRegion* hr);
    void reset_region_metadata(HeapRegion* hr);

  public:
    G1CalculatePointersClosure(CMBitMap* bitmap,
                               G1FullGCCompactionPoint* cp);

    void update_sets();
    bool doHeapRegion(HeapRegion* hr);
    bool freed_regions();
  };

  class G1PrepareCompactLiveClosure : public StackObj {
    G1FullGCCompactionPoint* _cp;

  public:
    G1PrepareCompactLiveClosure(G1FullGCCompactionPoint* cp);
    size_t apply(oop object);
  };

  class G1RePrepareClosure : public StackObj {
    G1FullGCCompactionPoint* _cp;
    HeapRegion* _current;

  public:
    G1RePrepareClosure(G1FullGCCompactionPoint* hrcp,
                       HeapRegion* hr) :
        _cp(hrcp),
        _current(hr) { }

    size_t apply(oop object);
  };
};

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCPREPARETASK_HPP
