/*
 * Copyright (c) 2001, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/javaClasses.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "code/codeCache.hpp"
#include "code/icBuffer.hpp"
#include "gc_implementation/shared/gcHeapSummary.hpp"
#include "gc_implementation/shared/gcTimer.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "gc_implementation/shared/gcTraceTime.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "memory/genCollectedHeap.hpp"
#include "memory/genMarkSweep.hpp"
#include "memory/genOopClosures.inline.hpp"
#include "memory/generation.inline.hpp"
#include "memory/modRefBarrierSet.hpp"
#include "memory/referencePolicy.hpp"
#include "memory/space.hpp"
#include "oops/instanceRefKlass.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "runtime/fprofiler.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/vmThread.hpp"
#include "utilities/copy.hpp"
#include "utilities/events.hpp"

void GenMarkSweep::invoke_at_safepoint(int level, ReferenceProcessor* rp, bool clear_all_softrefs) {
  guarantee(level == 1, "We always collect both old and young.");
  assert(SafepointSynchronize::is_at_safepoint(), "must be at a safepoint");

  GenCollectedHeap* gch = GenCollectedHeap::heap();
#ifdef ASSERT
  if (gch->collector_policy()->should_clear_all_soft_refs()) {
    assert(clear_all_softrefs, "Policy should have been checked earlier");
  }
#endif

  // hook up weak ref data so it can be used during Mark-Sweep
  assert(ref_processor() == NULL, "no stomping");
  assert(rp != NULL, "should be non-NULL");
  _ref_processor = rp;
  rp->setup_policy(clear_all_softrefs);

  GCTraceTime t1(GCCauseString("Full GC", gch->gc_cause()), PrintGC && !PrintGCDetails, true, NULL, _gc_tracer->gc_id());

  gch->trace_heap_before_gc(_gc_tracer);

  // When collecting the permanent generation Method*s may be moving,
  // so we either have to flush all bcp data or convert it into bci.
  CodeCache::gc_prologue();
  Threads::gc_prologue();

  // Increment the invocation count
  _total_invocations++;

  // Capture heap size before collection for printing.
  size_t gch_prev_used = gch->used();

  // Capture used regions for each generation that will be
  // subject to collection, so that card table adjustments can
  // be made intelligently (see clear / invalidate further below).
  gch->save_used_regions(level);

  allocate_stacks();

  mark_sweep_phase1(level, clear_all_softrefs);

  mark_sweep_phase2();

  // Don't add any more derived pointers during phase3
  COMPILER2_PRESENT(assert(DerivedPointerTable::is_active(), "Sanity"));
  COMPILER2_PRESENT(DerivedPointerTable::set_active(false));

  mark_sweep_phase3(level);

  mark_sweep_phase4();

  restore_marks();

  // Set saved marks for allocation profiler (and other things? -- dld)
  // (Should this be in general part?)
  gch->save_marks();

  deallocate_stacks();

  // If compaction completely evacuated all generations younger than this
  // one, then we can clear the card table.  Otherwise, we must invalidate
  // it (consider all cards dirty).  In the future, we might consider doing
  // compaction within generations only, and doing card-table sliding.
  bool all_empty = true;
  for (int i = 0; all_empty && i < level; i++) {
    Generation* g = gch->get_gen(i);
    all_empty = all_empty && gch->get_gen(i)->used() == 0;
  }
  GenRemSet* rs = gch->rem_set();
  Generation* old_gen = gch->get_gen(level);
  // Clear/invalidate below make use of the "prev_used_regions" saved earlier.
  if (all_empty) {
    // We've evacuated all generations below us.
    rs->clear_into_younger(old_gen);
  } else {
    // Invalidate the cards corresponding to the currently used
    // region and clear those corresponding to the evacuated region.
    rs->invalidate_or_clear(old_gen);
  }

  Threads::gc_epilogue();
  CodeCache::gc_epilogue();
  JvmtiExport::gc_epilogue();

  if (PrintGC && !PrintGCDetails) {
    gch->print_heap_change(gch_prev_used);
  }

  // refs processing: clean slate
  _ref_processor = NULL;

  // Update heap occupancy information which is used as
  // input to soft ref clearing policy at the next gc.
  Universe::update_heap_info_at_gc();

  // Update time of last gc for all generations we collected
  // (which curently is all the generations in the heap).
  // We need to use a monotonically non-deccreasing time in ms
  // or we will see time-warp warnings and os::javaTimeMillis()
  // does not guarantee monotonicity.
  jlong now = os::javaTimeNanos() / NANOSECS_PER_MILLISEC;
  gch->update_time_of_last_gc(now);

  gch->trace_heap_after_gc(_gc_tracer);
}

typedef OverflowTaskQueue<oop, mtGC>                      ObjTaskQueue;
typedef GenericTaskQueueSet<ObjTaskQueue, mtGC>           ObjTaskQueueSet;
typedef OverflowTaskQueue<ObjArrayTask, mtGC>             ObjArrayTaskQueue;
typedef GenericTaskQueueSet<ObjArrayTaskQueue, mtGC>      ObjArrayTaskQueueSet;

ObjTaskQueueSet*      GenMarkSweep::_pms_task_queues = NULL;
ObjArrayTaskQueueSet* GenMarkSweep::_pms_objarray_task_queues = NULL;
ObjTaskQueue*         GenMarkSweep::_pms_vm_thread_task_queue = NULL;
ObjArrayTaskQueue*    GenMarkSweep::_pms_vm_thread_objarray_task_queue = NULL;
bool                  GenMarkSweep::_pms_task_queues_initialized = false;

// Initialize data structures for PMS.
void GenMarkSweep::initialize_pms_task_queues() {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  WorkGang* work_gang = gch->workers();
  int n_workers = work_gang->total_workers();
  _pms_task_queues = new ObjTaskQueueSet(n_workers);
  _pms_objarray_task_queues = new ObjArrayTaskQueueSet(n_workers);
  for (int i = 0; i < n_workers; i++) {
    ObjTaskQueue* q = new ObjTaskQueue();
    _pms_task_queues->register_queue(i, q);
    _pms_task_queues->queue(i)->initialize();
    ObjArrayTaskQueue* oaq = new ObjArrayTaskQueue();
    _pms_objarray_task_queues->register_queue(i, oaq);
    _pms_objarray_task_queues->queue(i)->initialize();
  }
  _pms_vm_thread_task_queue = new ObjTaskQueue();
  _pms_vm_thread_task_queue->initialize();
  _pms_vm_thread_objarray_task_queue = new ObjArrayTaskQueue();
  _pms_vm_thread_objarray_task_queue->initialize();
}

void GenMarkSweep::allocate_stacks() {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  // Scratch request on behalf of oldest generation; will do no
  // allocation.
  ScratchBlock* scratch = gch->gather_scratch(gch->_gens[gch->_n_gens-1], 0);

  // $$$ To cut a corner, we'll only use the first scratch block, and then
  // revert to malloc.
  if (scratch != NULL) {
    _preserved_count_max =
      scratch->num_words * HeapWordSize / sizeof(PreservedMark);
  } else {
    _preserved_count_max = 0;
  }

  _preserved_marks = (PreservedMark*)scratch;
  _preserved_count = 0;

  if (CMSParallelFullGC) {
    if (!_pms_task_queues_initialized) {
      _pms_task_queues_initialized = true;
      initialize_pms_task_queues();
    }

    // Split evenly the scratch memory among the vm thread and the
    // worker threads.
    WorkGang* work_gang = gch->workers();
    int n_workers = work_gang->total_workers();
    PreservedMark* preserved_marks_top = _preserved_marks;
    size_t preserved_count_max_per_thread = _preserved_count_max / (1 + n_workers);

    NamedThread* vm_thread = Thread::current()->as_Named_thread();
    assert(vm_thread->is_VM_thread(), "Must be run by the VM thread");
    vm_thread->_pms_task_queue = _pms_vm_thread_task_queue;
    vm_thread->_pms_objarray_task_queue = _pms_vm_thread_objarray_task_queue;
    // Assign the statically allocated data structures to the VM
    // thread and avoid allocating a new set for the VM thread.
    vm_thread->_pms_preserved_mark_stack = &_preserved_mark_stack;
    vm_thread->_pms_preserved_oop_stack = &_preserved_oop_stack;
    vm_thread->_pms_preserved_count = _preserved_count;
    vm_thread->_pms_preserved_count_max = preserved_count_max_per_thread;
    vm_thread->_pms_preserved_marks = preserved_marks_top;
    preserved_marks_top += preserved_count_max_per_thread;

    // allocate per-thread marking_stack and objarray_stack here.
    for (int i = 0; i < n_workers; i++) {
      GangWorker* worker = work_gang->gang_worker(i);
      // typedef to workaround NEW_C_HEAP_OBJ macro, which can not deal with ','
      typedef Stack<markOop, mtGC> GCMarkOopStack;
      typedef Stack<oop, mtGC> GCOopStack;
      // A ResourceStack might be a good choice here, but since there's no precedent of its
      // use anywhere else in HotSpot, it may not be reliable. Instead, allocate a Stack
      // with NEW_C_HEAP_OBJ, and call the constructor explicitly.
      worker->_pms_preserved_mark_stack = NEW_C_HEAP_OBJ(GCMarkOopStack, mtGC);
      new (worker->_pms_preserved_mark_stack) Stack<markOop, mtGC>();
      worker->_pms_preserved_oop_stack = NEW_C_HEAP_OBJ(GCOopStack, mtGC);
      new (worker->_pms_preserved_oop_stack) Stack<oop, mtGC>();
      worker->_pms_preserved_count = 0;
      worker->_pms_preserved_count_max = preserved_count_max_per_thread;
      worker->_pms_preserved_marks = preserved_marks_top;
      preserved_marks_top += preserved_count_max_per_thread;
    }
    // Note _preserved_marks and _preserved_count_max aren't directly used
    // by the marking code if CMSParallelFullGC.
    assert(preserved_marks_top <= _preserved_marks + _preserved_count_max,
           "buffer overrun");

    assert(_pms_mark_bit_map != NULL, "the mark bit map must be initialized at this point.");
    if (ShareCMSMarkBitMapWithParallelFullGC) {
      // Clear it before the GC because it's shared and can be dirty
      // here.
      _pms_mark_bit_map->clear();
    } else {
      // If the mark bit map isn't shared, clear it at the end of GC.
      assert(_pms_mark_bit_map->isAllClear(),
             "Must have been cleared at the last invocation or at initialization.");
    }
    _pms_mark_counter = 0;

    assert(_pms_region_array_set == NULL, "Must be NULL");
    // Create region arrays before marking.
    // We are in a ResourceMark in CMSCollector::do_collection().
    _pms_region_array_set = new PMSRegionArraySet();
  }
}


void GenMarkSweep::deallocate_stacks() {
  if (!UseG1GC) {
    GenCollectedHeap* gch = GenCollectedHeap::heap();
    gch->release_scratch();
  }

  _preserved_mark_stack.clear(true);
  _preserved_oop_stack.clear(true);
  _marking_stack.clear();
  _objarray_stack.clear(true);

  if (CMSParallelFullGC) {
    assert_marking_stack_empty();

    NamedThread* vm_thread = Thread::current()->as_Named_thread();
    assert(vm_thread->is_VM_thread(), "Must be run by the main CMS thread");
    vm_thread->reset_pms_data();

    // clear per-thread marking_stack and objarray_stack here.
    GenCollectedHeap* gch = GenCollectedHeap::heap();
    WorkGang* work_gang = gch->workers();
    int n_workers = work_gang->total_workers();
    for (int i = 0; i < n_workers; i++) {
      GangWorker* worker = work_gang->gang_worker(i);
      // typedef to workaround FREE_C_HEAP_ARRAY macro, which can not deal
      // with ','
      typedef Stack<markOop, mtGC> GCMarkOopStack;
      typedef Stack<oop, mtGC> GCOopStack;
      // Call the Stack destructor which is the clear function
      // since FREE_C_HEAP_ARRAY doesn't.
      ((Stack<markOop, mtGC>*)worker->_pms_preserved_mark_stack)->clear(true);
      ((Stack<oop, mtGC>*)worker->_pms_preserved_oop_stack)->clear(true);
      // Free the allocated memory
      FREE_C_HEAP_ARRAY(GCMarkOopStack, worker->_pms_preserved_mark_stack, mtGC);
      FREE_C_HEAP_ARRAY(GCOopStack, worker->_pms_preserved_oop_stack, mtGC);
      worker->_pms_preserved_mark_stack = NULL;
      worker->_pms_preserved_oop_stack = NULL;

      worker->reset_pms_data();
    }

    if (!ShareCMSMarkBitMapWithParallelFullGC) {
      _pms_mark_bit_map->clear();
    }
    _pms_region_array_set->cleanup();
    _pms_region_array_set = NULL;
    _pms_mark_counter = 0;
  }
}

void GenMarkSweep::assert_marking_stack_empty() {
#ifdef ASSERT
  if (!CMSParallelFullGC) {
    assert(_marking_stack.is_empty(), "just drained");
    assert(_objarray_stack.is_empty(), "just drained");
  } else {
    NamedThread* thr = Thread::current()->as_Named_thread();
    assert(thr->is_VM_thread(), "Must be run by the main CMS thread");
    assert(((ObjTaskQueue*)thr->_pms_task_queue)->is_empty(), "just drained");
    assert(((ObjArrayTaskQueue*)thr->_pms_objarray_task_queue)->is_empty(), "just drained");
    // Check that all the per-thread marking stacks are empty here.
    GenCollectedHeap* gch = GenCollectedHeap::heap();
    WorkGang* work_gang = gch->workers();
    int n_workers = work_gang->total_workers();
    for (int i = 0; i < n_workers; i++) {
      GangWorker* worker = work_gang->gang_worker(i);
      assert(((ObjTaskQueue*)worker->_pms_task_queue)->is_empty(), "just drained");
      assert(((ObjArrayTaskQueue*)worker->_pms_objarray_task_queue)->is_empty(), "just drained");
    }
  }
#endif // ASSERT
}

void GenMarkSweep::mark_sweep_phase1(int level,
                                  bool clear_all_softrefs) {
  // Recursively traverse all live objects and mark them
  GCTraceTime tm("phase 1",
                 PrintGC && (Verbose || LogCMSParallelFullGC),
                 true, _gc_timer, _gc_tracer->gc_id());
  trace(" 1");

  GenCollectedHeap* gch = GenCollectedHeap::heap();

  // Because follow_root_closure is created statically, cannot
  // use OopsInGenClosure constructor which takes a generation,
  // as the Universe has not been created when the static constructors
  // are run.
  follow_root_closure.set_orig_generation(gch->get_gen(level));

  // Need new claim bits before marking starts.
  ClassLoaderDataGraph::clear_claimed_marks();

  {
    GCTraceTime tm1("marking", PrintGC && (Verbose || LogCMSParallelFullGC),
                    true, NULL, _gc_tracer->gc_id());
    if (!CMSParallelFullGC) {
      gch->gen_process_roots(level,
                         false, // Younger gens are not roots.
                         true,  // activate StrongRootsScope
                         GenCollectedHeap::SO_None,
                         ClassUnloading,
                         &follow_root_closure,
                         &follow_root_closure,
                         &follow_cld_closure);
    } else {
      GenCollectedHeap* gch = GenCollectedHeap::heap();
      WorkGang* workers = gch->workers();
      assert(workers != NULL, "Need parallel worker threads.");
      int n_workers = workers->total_workers();



      PMSMarkTask tsk(level, n_workers, workers, _pms_task_queues,
                      _pms_objarray_task_queues);

      // Set up for parallel process_strong_roots work.
      gch->set_par_threads(n_workers);

      if (n_workers > 1) {
        // Make sure refs discovery MT-safe
        assert(ref_processor()->discovery_is_mt(),
               "Ref discovery must already be set to MT-safe");
        GenCollectedHeap::StrongRootsScope srs(gch);
        workers->run_task(&tsk);
      } else {
        GenCollectedHeap::StrongRootsScope srs(gch);
        tsk.work(0);
      }
      gch->set_par_threads(0);  // 0 ==> non-parallel.
    }
  }

  assert_marking_stack_empty();

  // Process reference objects found during marking
  {
    GCTraceTime tm2("ref processing", PrintGC && (Verbose || LogCMSParallelFullGC),
                    true, NULL, _gc_tracer->gc_id());
    ref_processor()->setup_policy(clear_all_softrefs);

    if (ref_processor()->processing_is_mt()) {
      assert(CMSParallelFullGC, "CMSParallelFullGC must be true");
      PMSRefProcTaskExecutor task_executor(_pms_task_queues, _pms_objarray_task_queues);
      const ReferenceProcessorStats& stats =
        ref_processor()->process_discovered_references(
          &is_alive, &keep_alive, &follow_stack_closure, &task_executor, _gc_timer, _gc_tracer->gc_id());
      gc_tracer()->report_gc_reference_stats(stats);

    } else {
      assert(!CMSParallelFullGC, "CMSParallelFullGC must be false");
      const ReferenceProcessorStats& stats =
        ref_processor()->process_discovered_references(
          &is_alive, &keep_alive, &follow_stack_closure, NULL, _gc_timer, _gc_tracer->gc_id());
      gc_tracer()->report_gc_reference_stats(stats);

    }
    assert_marking_stack_empty();
  }

  // This is the point where the entire marking should have completed.
  assert_marking_stack_empty();

  GCTraceTime tm3("class unloading", PrintGC && (Verbose || LogCMSParallelFullGC),
                  true, NULL, _gc_tracer->gc_id());

  // Unload classes and purge the SystemDictionary.
  bool purged_class = SystemDictionary::do_unloading(&is_alive);

  // Unload nmethods.
  CodeCache::do_unloading(&is_alive, purged_class);
  assert_marking_stack_empty();

  // Prune dead klasses from subklass/sibling/implementor lists.
  Klass::clean_weak_klass_links(&is_alive);

  // Delete entries for dead interned strings.
  StringTable::unlink(&is_alive);

  // Clean up unreferenced symbols in symbol table.
  SymbolTable::unlink();

#ifdef ASSERT
  if (CMSParallelFullGC) {
    // This is expensive!  Verify that the region live sizes computed
    // during marking match what the mark bit map says.
    MarkSweep::pms_region_array_set()->verify_live_size();
  }
#endif

  gc_tracer()->report_object_count_after_gc(&is_alive);
}


void GenMarkSweep::mark_sweep_phase2() {
  // Now all live objects are marked, compute the new object addresses.

  // It is imperative that we traverse perm_gen LAST. If dead space is
  // allowed a range of dead object may get overwritten by a dead int
  // array. If perm_gen is not traversed last a Klass* may get
  // overwritten. This is fine since it is dead, but if the class has dead
  // instances we have to skip them, and in order to find their size we
  // need the Klass*!
  //
  // It is not required that we traverse spaces in the same order in
  // phase2, phase3 and phase4, but the ValidateMarkSweep live oops
  // tracking expects us to do so. See comment under phase4.

  GenCollectedHeap* gch = GenCollectedHeap::heap();

  GCTraceTime tm("phase 2", PrintGC && (Verbose || LogCMSParallelFullGC),
                 true, _gc_timer, _gc_tracer->gc_id());
  trace("2");

  gch->prepare_for_compaction();
}

class GenAdjustPointersClosure: public GenCollectedHeap::GenClosure {
public:
  void do_generation(Generation* gen) {
    GCTraceTime tm("per-gen-adjust", PrintGC && (Verbose || LogCMSParallelFullGC),
                   true, NULL, GCId::undefined());
    if (LogCMSParallelFullGC) {
      gclog_or_tty->print_cr("%s", gen->name());
    }
    gen->adjust_pointers();
  }
};

void GenMarkSweep::mark_sweep_phase3(int level) {
  GenCollectedHeap* gch = GenCollectedHeap::heap();

  // Adjust the pointers to reflect the new locations
  GCTraceTime tm("phase 3", PrintGC && (Verbose || LogCMSParallelFullGC),
                 true, _gc_timer, _gc_tracer->gc_id());
  trace("3");

  // Need new claim bits for the pointer adjustment tracing.
  ClassLoaderDataGraph::clear_claimed_marks();

  // Because the closure below is created statically, we cannot
  // use OopsInGenClosure constructor which takes a generation,
  // as the Universe has not been created when the static constructors
  // are run.
  adjust_pointer_closure.set_orig_generation(gch->get_gen(level));

  {
    GCTraceTime tm("adjust-strong-roots",
                   (PrintGC && Verbose) || LogCMSParallelFullGC,
                   true, NULL, _gc_tracer->gc_id());
    if (!CMSParallelFullGC) {
      gch->gen_process_roots(level,
                             false, // Younger gens are not roots.
                             true,  // activate StrongRootsScope
                             GenCollectedHeap::SO_AllCodeCache,
                             GenCollectedHeap::StrongAndWeakRoots,
                             &adjust_pointer_closure,
                             &adjust_pointer_closure,
                             &adjust_cld_closure);
    } else {
      WorkGang* workers = gch->workers();
      assert(workers != NULL, "Need parallel worker threads.");
      int n_workers = workers->total_workers();
      PMSAdjustRootsTask tsk(level, n_workers, workers);
      // Set up for parallel process_strong_roots work.
      gch->set_par_threads(n_workers);
      if (n_workers > 1) {
        GenCollectedHeap::StrongRootsScope srs(gch);
        workers->run_task(&tsk);
      } else {
        GenCollectedHeap::StrongRootsScope srs(gch);
        tsk.work(0);
      }
      gch->set_par_threads(0);  // 0 ==> non-parallel.
    }
  }


  {
    GCTraceTime tm("adjust-weak-roots",
                   PrintGC && (Verbose || LogCMSParallelFullGC),
                   true, NULL, _gc_tracer->gc_id());
    // Now adjust pointers in remaining weak roots.  (All of which should
    // have been cleared if they pointed to non-surviving objects.)
    gch->gen_process_weak_roots(&adjust_pointer_closure);
  }

  {
    GCTraceTime tm("adjust-preserved-marks",
                   PrintGC && (Verbose || LogCMSParallelFullGC),
                   true, NULL, _gc_tracer->gc_id());
    adjust_marks();
  }

  {
    GCTraceTime tm("adjust-heap",
                   PrintGC && (Verbose || LogCMSParallelFullGC),
                   true, NULL, _gc_tracer->gc_id());
    GenAdjustPointersClosure blk;
    gch->generation_iterate(&blk, true);
  }
}

class GenCompactClosure: public GenCollectedHeap::GenClosure {
public:
  void do_generation(Generation* gen) {
    gen->compact();
  }
};

void GenMarkSweep::mark_sweep_phase4() {
  // All pointers are now adjusted, move objects accordingly

  // It is imperative that we traverse perm_gen first in phase4. All
  // classes must be allocated earlier than their instances, and traversing
  // perm_gen first makes sure that all Klass*s have moved to their new
  // location before any instance does a dispatch through it's klass!

  // The ValidateMarkSweep live oops tracking expects us to traverse spaces
  // in the same order in phase2, phase3 and phase4. We don't quite do that
  // here (perm_gen first rather than last), so we tell the validate code
  // to use a higher index (saved from phase2) when verifying perm_gen.
  GenCollectedHeap* gch = GenCollectedHeap::heap();

  GCTraceTime tm("phase 4", PrintGC && (Verbose || LogCMSParallelFullGC),
                 true, _gc_timer, _gc_tracer->gc_id());
  trace("4");

  GenCompactClosure blk;
  gch->generation_iterate(&blk, true);
}
