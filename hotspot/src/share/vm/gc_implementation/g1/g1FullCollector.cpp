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
#include "code/codeCache.hpp"
#include "gc_implementation/g1/g1CollectedHeap.hpp"
#include "gc_implementation/g1/g1CollectorPolicy.hpp"
#include "gc_implementation/g1/g1FullCollector.hpp"
#include "gc_implementation/g1/g1FullGCAdjustTask.hpp"
#include "gc_implementation/g1/g1FullGCCompactTask.hpp"
#include "gc_implementation/g1/g1FullGCMarker.inline.hpp"
#include "gc_implementation/g1/g1FullGCMarkTask.hpp"
#include "gc_implementation/g1/g1FullGCPrepareTask.hpp"
#include "gc_implementation/g1/g1FullGCReferenceProcessorExecutor.hpp"
#include "gc_implementation/g1/g1FullGCScope.hpp"
#include "gc_implementation/g1/g1OopClosures.hpp"
#include "gc_implementation/g1/g1StringDedup.hpp"
#include "gc_implementation/shared/adaptiveSizePolicy.hpp"
#include "gc_implementation/shared/gcTraceTime.hpp"
#include "gc_implementation/g1/preservedMarks.hpp"
#include "memory/referenceProcessor.hpp"
#include "gc_implementation/shared/weakProcessor.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/handles.inline.hpp"
#include "utilities/debug.hpp"

static void deactivate_derived_pointers() {
  COMPILER2_PRESENT(DerivedPointerTable::set_active(false));
}

CMBitMap* G1FullCollector::mark_bitmap() {
  return _heap->concurrent_mark()->nextMarkBitMap();
}

ReferenceProcessor* G1FullCollector::reference_processor() {
  return _heap->ref_processor_stw();
}

uint G1FullCollector::calc_active_workers() {
  G1CollectedHeap* heap = G1CollectedHeap::heap();
  uint max_worker_count = heap->workers()->total_workers();
  // Only calculate number of workers if UseDynamicNumberOfGCThreads
  // is enabled, otherwise use max.
  if (!UseDynamicNumberOfGCThreads) {
    return max_worker_count;
  }

  // Consider G1HeapWastePercent to decide max number of workers. Each worker
  // will in average cause half a region waste.
  uint max_wasted_regions_allowed = ((heap->num_regions() * G1HeapWastePercent) / 100);
  uint waste_worker_count = MAX2((max_wasted_regions_allowed * 2) , 1u);
  uint heap_waste_worker_limit = MIN2(waste_worker_count, max_worker_count);

  // Also consider HeapSizePerGCThread by calling AdaptiveSizePolicy to calculate
  // the number of workers.
  uint current_active_workers = heap->workers()->active_workers();
  uint adaptive_worker_limit = AdaptiveSizePolicy::calc_active_workers(max_worker_count, current_active_workers, 0);

  // Update active workers to the lower of the limits.
  uint worker_count = MIN2(heap_waste_worker_limit, adaptive_worker_limit);
  worker_count = heap->workers()->set_active_workers(worker_count);

  return worker_count;
}

G1FullCollector::G1FullCollector(G1CollectedHeap* heap, GCMemoryManager* memory_manager, bool explicit_gc, bool clear_soft_refs) :
    _heap(heap),
    _scope(memory_manager, explicit_gc, clear_soft_refs),
    _num_workers(calc_active_workers()),
    _oop_queue_set(_num_workers),
    _array_queue_set(_num_workers),
    _preserved_marks_set(true),
    _serial_compaction_point(),
    _is_alive(heap->concurrent_mark()->nextMarkBitMap()),
    _is_alive_mutator(heap->ref_processor_stw(), &_is_alive),
    _always_subject_to_discovery(),
    _is_subject_mutator(heap->ref_processor_stw(), &_always_subject_to_discovery) {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at a safepoint");

  _preserved_marks_set.init(_num_workers);
  _markers = NEW_C_HEAP_ARRAY(G1FullGCMarker*, _num_workers, mtGC);
  _compaction_points = NEW_C_HEAP_ARRAY(G1FullGCCompactionPoint*, _num_workers, mtGC);
  for (uint i = 0; i < _num_workers; i++) {
    _markers[i] = new G1FullGCMarker(i, _preserved_marks_set.get(i), mark_bitmap());
    _compaction_points[i] = new G1FullGCCompactionPoint();
    _oop_queue_set.register_queue(i, marker(i)->oop_stack());
    _array_queue_set.register_queue(i, marker(i)->objarray_stack());
  }
}

G1FullCollector::~G1FullCollector() {
  for (uint i = 0; i < _num_workers; i++) {
    delete _markers[i];
    delete _compaction_points[i];
  }
  FREE_C_HEAP_ARRAY(G1FullGCMarker*, _markers, mtGC);
  FREE_C_HEAP_ARRAY(G1FullGCCompactionPoint*, _compaction_points, mtGC);
}

void G1FullCollector::prepare_collection() {
  _heap->reset_heap_region_claim_values();

  // When collecting the permanent generation Method*s may be moving,
  // so we either have to flush all bcp data or convert it into bci.
  CodeCache::gc_prologue();

  // We should save the marks of the currently locked biased monitors.
  // The marking doesn't preserve the marks of biased objects.
  BiasedLocking::preserve_marks();
}

void G1FullCollector::collect() {
  phase1_mark_live_objects();
  verify_after_marking();

  // Don't add any more derived pointers during later phases
  deactivate_derived_pointers();

  phase2_prepare_compaction();

  phase3_adjust_pointers();

  phase4_do_compaction();
}

void G1FullCollector::complete_collection() {
  // Restore all marks.
  restore_marks();

  Threads::gc_epilogue();
  BiasedLocking::restore_marks();
  CodeCache::gc_epilogue();
  JvmtiExport::gc_epilogue();
}

void G1FullCollector::phase1_mark_live_objects() {
  // Recursively traverse all live objects and mark them.
  GCTraceTime t("Phase 1: Mark live objects", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());

  {
    // Do the actual marking.
    GCTraceTime x("Phase 1: Mark strong roots", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());
    G1FullGCMarkTask marking_task(this);
    _heap->set_par_threads();
    run_task(&marking_task);
    _heap->set_par_threads(0);
  }

  {
    // Process references discovered during marking.
    GCTraceTime x("Phase 1: Reference Processing", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());
    G1FullGCReferenceProcessingExecutor reference_processing(this);
    reference_processing.execute(scope()->timer(), scope()->tracer());
  }

  // Weak oops cleanup.
  {
    GCTraceTime x("Phase 1: Weak Processing", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());
    WeakProcessor::weak_oops_do(&_is_alive, &do_null_cl);
  }

  // Class unloading and cleanup.
  if (ClassUnloading) {
    GCTraceTime x("Phase 1: Class Unloading and Cleanup", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());
    // Unload classes and purge the SystemDictionary.
    bool purged_class = SystemDictionary::do_unloading(&_is_alive, true);
     // Unload nmethods.
     CodeCache::do_unloading(&_is_alive, purged_class);

     // Prune dead klasses from subklass/sibling/implementor lists.
     Klass::clean_weak_klass_links(&_is_alive);
  }

  {
    // If no class unloading just clean out strings and symbols.
    GCTraceTime time("Phase 1: String and Symbol Tables Cleanup", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());

    // Delete entries for dead interned string and clean up unreferenced symbols in symbol table.
    G1CollectedHeap::heap()->unlink_string_and_symbol_table(&_is_alive);
  }

  scope()->tracer()->report_object_count_after_gc(&_is_alive);
}

void G1FullCollector::phase2_prepare_compaction() {
  GCTraceTime t("Phase 2: Prepare for compaction", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());
  G1FullGCPrepareTask task(this);
  _heap->set_par_threads();
  run_task(&task);
  _heap->set_par_threads(0);
  _heap->reset_heap_region_claim_values();

  // To avoid OOM when there is memory left.
  if (!task.has_freed_regions()) {
    task.prepare_serial_compaction();
  }
}

void G1FullCollector::phase3_adjust_pointers() {
  // Adjust the pointers to reflect the new locations
  GCTraceTime t("Phase 3: Adjust pointers", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());

  G1FullGCAdjustTask task(this);
  _heap->set_par_threads();
  run_task(&task);
  _heap->set_par_threads(0);
  _heap->reset_heap_region_claim_values();
}

void G1FullCollector::phase4_do_compaction() {
  // Compact the heap using the compaction queues created in phase 2.
  GCTraceTime t("Phase 4: Compact heap", G1Log::finest(), false, NULL, scope()->tracer()->gc_id());
  G1FullGCCompactTask task(this);
  _heap->set_par_threads();
  run_task(&task);
  _heap->set_par_threads(0);
  _heap->reset_heap_region_claim_values();

  // Serial compact to avoid OOM when very few free regions.
  if (serial_compaction_point()->has_regions()) {
    task.serial_compaction();
  }
}

void G1FullCollector::restore_marks() {
  SharedRestorePreservedMarksTaskExecutor task_executor(_heap->workers());
  _preserved_marks_set.restore(&task_executor);
  _preserved_marks_set.reclaim();
}

void G1FullCollector::run_task(AbstractGangTask* task) {
  _heap->workers()->run_task(task);
}

void G1FullCollector::verify_after_marking() {
  
  if (!VerifyDuringGC) {
    // Only do verification if VerifyDuringGC and G1VerifyFull is set.
    return;
  }

  HandleMark hm;  // handle scope

  COMPILER2_PRESENT(DerivedPointerTableDeactivate dpt_deact);

  Universe::heap()->prepare_for_verify();
  // Note: we can verify only the heap here. When an object is
  // marked, the previous value of the mark word (including
  // identity hash values, ages, etc) is preserved, and the mark
  // word is set to markOop::marked_value - effectively removing
  // any hash values from the mark word. These hash values are
  // used when verifying the dictionaries and so removing them
  // from the mark word can make verification of the dictionaries
  // fail. At the end of the GC, the original mark word values
  // (including hash values) are restored to the appropriate
  // objects.
  if (!VerifySilently) {
    gclog_or_tty->print(" VerifyDuringGC:(full)[Verifying ");
  }
  Universe::heap()->verify(VerifySilently, VerifyOption_G1UseFullMarking);
  if (!VerifySilently) {
    gclog_or_tty->print_cr("]");
  }
}
