/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "compiler/compileBroker.hpp"
#include "gc_implementation/shared/gcTimer.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "gc_implementation/shared/markSweep.inline.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "oops/methodData.hpp"
#include "oops/objArrayKlass.inline.hpp"
#include "oops/oop.inline.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

uint                    MarkSweep::_total_invocations = 0;

Stack<oop, mtGC>              MarkSweep::_marking_stack;
Stack<ObjArrayTask, mtGC>     MarkSweep::_objarray_stack;

Stack<oop, mtGC>              MarkSweep::_preserved_oop_stack;
Stack<markOop, mtGC>          MarkSweep::_preserved_mark_stack;
size_t                  MarkSweep::_preserved_count = 0;
size_t                  MarkSweep::_preserved_count_max = 0;
PreservedMark*          MarkSweep::_preserved_marks = NULL;
ReferenceProcessor*     MarkSweep::_ref_processor   = NULL;
STWGCTimer*             MarkSweep::_gc_timer        = NULL;
SerialOldTracer*        MarkSweep::_gc_tracer       = NULL;

PMSMarkBitMap*          MarkSweep::_pms_mark_bit_map = NULL;
PMSRegionArraySet*      MarkSweep::_pms_region_array_set = NULL;
volatile intptr_t       MarkSweep::_pms_mark_counter = 0;
PMSRegionTaskQueueSet*  MarkSweep::_pms_region_task_queues = NULL;

MarkSweep::FollowRootClosure  MarkSweep::follow_root_closure;

void MarkSweep::FollowRootClosure::do_oop(oop* p)       { follow_root(p); }
void MarkSweep::FollowRootClosure::do_oop(narrowOop* p) { follow_root(p); }

MarkSweep::MarkAndPushClosure MarkSweep::mark_and_push_closure;
CLDToOopClosure               MarkSweep::follow_cld_closure(&mark_and_push_closure);
CLDToOopClosure               MarkSweep::adjust_cld_closure(&adjust_pointer_closure);

void MarkSweep::MarkAndPushClosure::do_oop(oop* p)       { mark_and_push(p); }
void MarkSweep::MarkAndPushClosure::do_oop(narrowOop* p) { mark_and_push(p); }

void MarkSweep::follow_class_loader(ClassLoaderData* cld) {
  MarkSweep::follow_cld_closure.do_cld(cld);
}

void MarkSweep::follow_stack() {
  if (!CMSParallelFullGC) {
    do {
      while (!_marking_stack.is_empty()) {
        oop obj = _marking_stack.pop();
        assert (obj->is_gc_marked(), "p must be marked");
        obj->follow_contents();
      }
      // Process ObjArrays one at a time to avoid marking stack bloat.
      if (!_objarray_stack.is_empty()) {
        ObjArrayTask task = _objarray_stack.pop();
        ObjArrayKlass* k = (ObjArrayKlass*)task.obj()->klass();
        k->oop_follow_contents(task.obj(), task.index());
      }
    } while (!_marking_stack.is_empty() || !_objarray_stack.is_empty());
  } else {
    NamedThread* thr = Thread::current()->as_Named_thread();
    ObjTaskQueue* task_queue = ((ObjTaskQueue*)thr->_pms_task_queue);
    ObjArrayTaskQueue* objarray_task_queue =
        ((ObjArrayTaskQueue*)thr->_pms_objarray_task_queue);
    do {
      // Drain the overflow stack first, to allow stealing from the marking stack.
      oop obj;
      while (task_queue->pop_overflow(obj)) {
        assert(is_object_marked(obj), "p must be marked");
        obj->follow_contents();
      }
      while (task_queue->pop_local(obj)) {
        assert(is_object_marked(obj), "p must be marked");
        obj->follow_contents();
      }
      ObjArrayTask task;
      if (objarray_task_queue->pop_overflow(task)) {
        ObjArrayKlass* k = (ObjArrayKlass*)task.obj()->klass();
        k->oop_follow_contents(task.obj(), task.index());
      } else if (objarray_task_queue->pop_local(task)) {
        ObjArrayKlass* k = (ObjArrayKlass*)task.obj()->klass();
        k->oop_follow_contents(task.obj(), task.index());
      }
    } while (!task_queue->is_empty() || !objarray_task_queue->is_empty());
  }
}

MarkSweep::FollowStackClosure MarkSweep::follow_stack_closure;

void MarkSweep::FollowStackClosure::do_void() { follow_stack(); }

// We preserve the mark which should be replaced at the end and the location
// that it will go.  Note that the object that this markOop belongs to isn't
// currently at that address but it will be after phase4
void MarkSweep::preserve_mark(oop obj, markOop mark) {
  if (!CMSParallelFullGC) {
    // We try to store preserved marks in the to space of the new generation since
    // this is storage which should be available.  Most of the time this should be
    // sufficient space for the marks we need to preserve but if it isn't we fall
    // back to using Stacks to keep track of the overflow.
    if (_preserved_count < _preserved_count_max) {
      _preserved_marks[_preserved_count++].init(obj, mark);
    } else {
      _preserved_mark_stack.push(mark);
      _preserved_oop_stack.push(obj);
    }
  } else {
    NamedThread* thr = Thread::current()->as_Named_thread();
    if (thr->_pms_preserved_count < thr->_pms_preserved_count_max) {
      PreservedMark* preserved_marks = (PreservedMark*) thr->_pms_preserved_marks;
      preserved_marks[thr->_pms_preserved_count++].init(obj, mark);
    } else {
      ((Stack<markOop, mtGC>*)thr->_pms_preserved_mark_stack)->push(mark);
      ((Stack<oop, mtGC>*)thr->_pms_preserved_oop_stack)->push(obj);
    }
  }
}

MarkSweep::AdjustPointerClosure MarkSweep::adjust_pointer_closure;

void MarkSweep::AdjustPointerClosure::do_oop(oop* p)       { adjust_pointer(p); }
void MarkSweep::AdjustPointerClosure::do_oop(narrowOop* p) { adjust_pointer(p); }

void MarkSweep::adjust_marks_helper(Stack<markOop, mtGC>* preserved_mark_stack,
                                    Stack<oop, mtGC>*     preserved_oop_stack,
                                    size_t          preserved_count,
                                    PreservedMark*  preserved_marks) {
  assert(preserved_oop_stack->size() == preserved_mark_stack->size(),
         "inconsistent preserved oop stacks");

  // adjust the oops we saved earlier
  for (size_t i = 0; i < preserved_count; i++) {
    preserved_marks[i].adjust_pointer();
  }

  // deal with the overflow stack
  StackIterator<oop, mtGC> iter(*preserved_oop_stack);
  while (!iter.is_empty()) {
    oop* p = iter.next_addr();
    adjust_pointer(p);
  }
}

void MarkSweep::adjust_marks() {
  if (!CMSParallelFullGC) {
    adjust_marks_helper(&_preserved_mark_stack,
                        &_preserved_oop_stack,
                        _preserved_count,
                        _preserved_marks);
  } else {
    NamedThread* vm_thread = Thread::current()->as_Named_thread();
    assert(vm_thread->is_VM_thread(), "Must be run by the VM thread");
    Stack<markOop, mtGC>* vm_thr_preserved_mark_stack =
        (Stack<markOop, mtGC>*) vm_thread->_pms_preserved_mark_stack;
    Stack<oop, mtGC>* vm_thr_preserved_oop_stack =
        (Stack<oop, mtGC>*) vm_thread->_pms_preserved_oop_stack;
    adjust_marks_helper(vm_thr_preserved_mark_stack,
                        vm_thr_preserved_oop_stack,
                        vm_thread->_pms_preserved_count,
                        (PreservedMark*)vm_thread->_pms_preserved_marks);

    GenCollectedHeap* gch = GenCollectedHeap::heap();
    WorkGang* workers = gch->workers();
    int n_workers = workers->total_workers();
    // If n_workers <= 1, then the VM thread must have run the
    // mark phase and there is nothing to do.
    if (n_workers > 1) {
      PMSParAdjustPreservedMarksTask tsk(n_workers, workers);
      gch->set_par_threads(n_workers);
      workers->run_task(&tsk);
      gch->set_par_threads(0);  /* 0 ==> non-parallel.*/
    }
  }
}

void MarkSweep::restore_marks_helper(Stack<markOop, mtGC>* preserved_mark_stack,
                                     Stack<oop, mtGC>*     preserved_oop_stack,
                                     size_t          preserved_count,
                                     PreservedMark*  preserved_marks,
                                     bool            should_release) {
  assert(preserved_oop_stack->size() == preserved_mark_stack->size(),
         "inconsistent preserved oop stacks");
  if (PrintGC && Verbose) {
    gclog_or_tty->print_cr("Restoring %d marks",
                           preserved_count + preserved_oop_stack->size());
  }

  // restore the marks we saved earlier
  for (size_t i = 0; i < preserved_count; i++) {
    preserved_marks[i].restore();
  }

  // deal with the overflow
  while (!preserved_oop_stack->is_empty()) {
    oop obj       = preserved_oop_stack->pop();
    markOop mark  = preserved_mark_stack->pop();
    if (!should_release) {
      obj->set_mark(mark);
    } else {
      obj->release_set_mark(mark);
    }
  }
}

void MarkSweep::restore_marks() {
  if (!CMSParallelFullGC) {
    restore_marks_helper(&_preserved_mark_stack,
                         &_preserved_oop_stack,
                         _preserved_count,
                         _preserved_marks,
                         false);
  } else {
    NamedThread* vm_thread = Thread::current()->as_Named_thread();
    assert(vm_thread->is_VM_thread(), "Must be run by the VM thread");
    Stack<markOop, mtGC>* vm_thr_preserved_mark_stack =
        (Stack<markOop, mtGC>*) vm_thread->_pms_preserved_mark_stack;
    Stack<oop, mtGC>* vm_thr_preserved_oop_stack =
        (Stack<oop, mtGC>*) vm_thread->_pms_preserved_oop_stack;
    restore_marks_helper(vm_thr_preserved_mark_stack,
                         vm_thr_preserved_oop_stack,
                         vm_thread->_pms_preserved_count,
                         (PreservedMark*)vm_thread->_pms_preserved_marks,
                         true);

    // Handle the per-thread data here.
    GenCollectedHeap* gch = GenCollectedHeap::heap();
    WorkGang* work_gang = gch->workers();
    int n_workers = work_gang->total_workers();
    for (int i = 0; i < n_workers; i++) {
      GangWorker* worker = work_gang->gang_worker(i);
      Stack<markOop, mtGC>* preserved_mark_stack =
          (Stack<markOop, mtGC>*) worker->_pms_preserved_mark_stack;
      Stack<oop, mtGC>* preserved_oop_stack =
          (Stack<oop, mtGC>*) worker->_pms_preserved_oop_stack;
      restore_marks_helper(preserved_mark_stack,
                           preserved_oop_stack,
                           worker->_pms_preserved_count,
                           (PreservedMark*)worker->_pms_preserved_marks,
                           true);
    }
  }
}

MarkSweep::IsAliveClosure   MarkSweep::is_alive;

bool MarkSweep::IsAliveClosure::do_object_b(oop p) { return is_object_marked(p); }

MarkSweep::KeepAliveClosure MarkSweep::keep_alive;

void MarkSweep::KeepAliveClosure::do_oop(oop* p)       { MarkSweep::KeepAliveClosure::do_oop_work(p); }
void MarkSweep::KeepAliveClosure::do_oop(narrowOop* p) { MarkSweep::KeepAliveClosure::do_oop_work(p); }

void marksweep_init() {

  if (CMSParallelFullGC) {
    MarkSweep::initialize_parallel_mark_sweep();
  }
  MarkSweep::_gc_timer = new (ResourceObj::C_HEAP, mtGC) STWGCTimer();
  MarkSweep::_gc_tracer = new (ResourceObj::C_HEAP, mtGC) SerialOldTracer();
}

#ifndef PRODUCT

void MarkSweep::trace(const char* msg) {
  if (TraceMarkSweep)
    gclog_or_tty->print("%s", msg);
}

#endif

void MarkSweep::initialize_parallel_mark_sweep() {
  assert(CMSParallelFullGC, "Must be CMSParallelFullGC == true");
  if (ShareCMSMarkBitMapWithParallelFullGC) {
    // Share the underlying memory for the bit map with the CMS bit map
    CMSCollector* cms_collector = ConcurrentMarkSweepGeneration::the_cms_collector();
    assert(cms_collector != NULL, "Must be already initialized");
    MemRegion cms_mbm_mr = cms_collector->markBitMap()->vspace_mr();
    _pms_mark_bit_map = new PMSMarkBitMap(cms_mbm_mr);
  } else {
    // Allocate a new bit map for parallel mark-sweep
    _pms_mark_bit_map = new PMSMarkBitMap();
  }
  _pms_region_array_set = NULL;
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  WorkGang* work_gang = gch->workers();
  int n_workers = work_gang->total_workers();
  _pms_region_task_queues = new PMSRegionTaskQueueSet(n_workers);
  for (int i = 0; i < n_workers; i++) {
    PMSRegionTaskQueue* q = new PMSRegionTaskQueue();
    _pms_region_task_queues->register_queue(i, q);
    q->initialize();
  }
}

PMSMarkBitMap::PMSMarkBitMap() {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  MemRegion reserved = gch->reserved_region();
  _start = reserved.start();
  _size = reserved.word_size();

  // Internally allocates memory for the BitMap in the heap. By
  // default, both in 32-bit and 64-bit, the size of the bit map is
  // 1/64 of the heap size (as tight as possible based on how object
  // alignment works).
  //
  // In 64-bit with the non-default ObjectAlignmentInBytes (which is 8
  // by default), the bit map footprint is smaller than 1/64 of the
  // heap size. For example, when ObjectAlignmentInBytes = 16, the
  // footprint is 1/128 of the heap size, and so on.
  //
  // In 32-bit, ObjectAlignmentInBytes is fixed at 8.
  //
  // The following asserts are mostly development notes and for
  // documentation purpose only.
  //
  // cf. set_object_alignment() in arguments.cpp.
#ifdef ASSERT
#ifdef _LP64
  // 64-bit.
  if (ObjectAlignmentInBytes == 8) {
    assert(MinObjAlignment == 1, "MinObjAlignment = 1 in 64-bit by default");
    assert(LogMinObjAlignment == 0, "LogMinObjAlignment = 0 in 64-bit by default");
  } else {
    assert(MinObjAlignment == ObjectAlignmentInBytes / HeapWordSize,
           "MinObjAlignment = ObjectAlignmentInBytes / HeapWordSize in 64-bit");
    assert(LogMinObjAlignment == exact_log2(ObjectAlignmentInBytes / HeapWordSize),
           "LogMinObjAlignment = exact_log2(ObjectAlignmentInBytes / HeapWordSize) in 64-bit");
  }
#else
  // 32-bit. ObjectAlignmentInBytes is fixed at 8.
  assert(MinObjAlignment == 2, "MinObjAlignment = 2 in 32-bit");
  assert(LogMinObjAlignment == 1, "LogMinObjAlignment = 1 in 32-bit");
#endif // _LP64
#endif // ASSERT

  _shifter = LogMinObjAlignment;
  _bits = BitMap((BitMap::idx_t)(_size / MinObjAlignment), false);
}

PMSMarkBitMap::PMSMarkBitMap(MemRegion underlying_memory) {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  MemRegion reserved = gch->reserved_region();
  _start = reserved.start();
  _size = reserved.word_size();

  // Accepts already-allocated memory for the BitMap. The CMS bit
  // map uses one bit per word regardless of whether 32-bit or
  // 64-bit. So, follow that.
  // Check the consistency between the heap size and the size of
  // the underlying memory for the bit map. (cf. CMSBitMap::allocate()).
  size_t nbits_by_heap_size = ReservedSpace::allocation_align_size_up(
      (_size >> LogBitsPerByte) + 1) * BitsPerByte;
  size_t nbits_by_bm_size   = underlying_memory.byte_size() * BitsPerByte;
  assert(nbits_by_heap_size == nbits_by_bm_size,
         err_msg("Inconsisntecy between the heap sizes and the bit map, "
                 SIZE_FORMAT " vs " SIZE_FORMAT,
                 nbits_by_heap_size, nbits_by_bm_size));
  _shifter = 0;
  _bits = BitMap((BitMap::bm_word_t*) underlying_memory.start(),
                 (BitMap::idx_t) nbits_by_bm_size);
}

void PMSMarkTask::work(uint worker_id) {
  elapsedTimer timer;
  ResourceMark rm;
  HandleMark   hm;
  timer.start();

  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel mark worker %d started...", worker_id);
  }

  NamedThread* thr = Thread::current()->as_Named_thread();
  thr->_pms_task_queue = _task_queues->queue(worker_id);
  thr->_pms_objarray_task_queue = _objarray_task_queues->queue(worker_id);

  GenCollectedHeap* gch = GenCollectedHeap::heap();
  elapsedTimer timer1;
  timer1.start();
  gch->gen_process_roots(_level,
                         false, // Younger gens are not roots.
                         false, // this is parallel code
                         GenCollectedHeap::SO_None,
                         GenCollectedHeap::StrongRootsOnly,
                         &MarkSweep::follow_root_closure,
                         &MarkSweep::follow_root_closure,
                         &MarkSweep::follow_cld_closure);

  timer1.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel mark worker %d finished scanning (%3.5f sec)",
                           worker_id, timer1.seconds());
  }
  do_steal_work(worker_id);
  timer.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel mark worker %d finished (%3.5f sec)",
                           worker_id, timer.seconds());
  }
}

void PMSMarkTask::do_steal_work(uint worker_id) {
  elapsedTimer timer;
  timer.start();
  NamedThread* thr = Thread::current()->as_Named_thread();
  int seed = 17;
  int objarray_seed = 17;
  int num_steals = 0;
  ObjTaskQueue* task_queue = (ObjTaskQueue*) thr->_pms_task_queue;
  ObjArrayTaskQueue* objarray_task_queue = (ObjArrayTaskQueue*) thr->_pms_objarray_task_queue;
  assert(task_queue == _task_queues->queue(worker_id), "Sanity check");
  assert(objarray_task_queue == _objarray_task_queues->queue(worker_id), "Sanity check");
  oop obj;
  ObjArrayTask task;
  do {
    assert(task_queue->is_empty(), "Task queue should be empty before work stealing");
    assert(objarray_task_queue->is_empty(), "Task queue should be empty before work stealing");
    while (_task_queues->steal(worker_id, &seed, obj)) {
      num_steals++;
      assert(obj->is_oop(), "Santiy check");
      assert(MarkSweep::is_object_marked(obj), "Must be already marked");
      obj->follow_contents();
      MarkSweep::follow_stack();
    }
    while (_objarray_task_queues->steal(worker_id, &objarray_seed, task)) {
      num_steals++;
      ObjArrayKlass* k = (ObjArrayKlass*)task.obj()->klass();
      k->oop_follow_contents(task.obj(), task.index());
      MarkSweep::follow_stack();
    }
  } while (!_terminator.offer_termination() || !_objarray_terminator.offer_termination());
  assert(task_queue->is_empty(), "Check my task queue is empty again");
  assert(objarray_task_queue->is_empty(), "Check my task queue is empty again");
  timer.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel mark worker %d finished work-stealing "
                           "(%d steals, %3.5f sec)", worker_id, num_steals, timer.seconds());
  }
}

void PMSAdjustRootsTask::work(uint worker_id) {
  elapsedTimer timer;
  ResourceMark rm;
  HandleMark   hm;
  timer.start();

  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel adjust-roots worker %d started...", worker_id);
  }
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  gch->gen_process_roots(_level,
                         false, // Younger gens are not roots.
                         false, // this is parallel code
                         GenCollectedHeap::SO_AllCodeCache,
                         GenCollectedHeap::StrongAndWeakRoots,
                         &MarkSweep::adjust_pointer_closure,
                         &MarkSweep::adjust_pointer_closure,
                         &MarkSweep::adjust_cld_closure);
  timer.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel adjust-roots worker %d finished (%3.5f sec)",
                           worker_id, timer.seconds());
  }
}

void PMSParAdjustTask::work(uint worker_id) {
  elapsedTimer timer;
  ResourceMark rm;
  HandleMark   hm;
  timer.start();

  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel adjust-pointers worker %d started...", worker_id);
  }

  PMSRegionTaskQueue* task_queue = _task_queues->queue(worker_id);
  PMSMarkBitMap* mark_bit_map = MarkSweep::pms_mark_bit_map();
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  elapsedTimer timer1;
  timer1.start();
  PMSRegion* r;
  do {
    while (task_queue->pop_overflow(r)) {
      mark_bit_map->iterate(_cl,
                            mark_bit_map->addr_to_bit(r->start()),
                            mark_bit_map->addr_to_bit(r->end()));
    }
    while (task_queue->pop_local(r)) {
      mark_bit_map->iterate(_cl,
                            mark_bit_map->addr_to_bit(r->start()),
                            mark_bit_map->addr_to_bit(r->end()));
    }
  } while (!task_queue->is_empty());

  timer1.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel adjust-pointers worker %d finished scanning (%3.5f sec)",
                           worker_id, timer1.seconds());
  }
  do_steal_work( worker_id);
  timer.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel adjust-pointers worker %d finished (%3.5f sec)",
                           worker_id, timer.seconds());
  }
}

void PMSParAdjustTask::do_steal_work(uint worker_id) {
  elapsedTimer timer;
  timer.start();
  int seed = 17;
  int num_steals = 0;
  PMSRegionTaskQueue* task_queue = _task_queues->queue(worker_id);
  assert(task_queue == _task_queues->queue(worker_id), "Sanity check");
  PMSMarkBitMap* mark_bit_map = MarkSweep::pms_mark_bit_map();
  PMSRegion* r;
  do {
    assert(task_queue->is_empty(), "Task queue should be empty before work stealing");
    while (_task_queues->steal(worker_id, &seed, r)) {
      num_steals++;
      mark_bit_map->iterate(_cl,
                            mark_bit_map->addr_to_bit(r->start()),
                            mark_bit_map->addr_to_bit(r->end()));
    }
  } while (!_terminator.offer_termination());
  assert(task_queue->is_empty(), "Check my task queue is empty again");
  timer.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel adjust-pointers worker %d finished work-stealing "
                           "(%d steals, %3.5f sec)", worker_id, num_steals, timer.seconds());
  }
}

void PMSParAdjustPreservedMarksTask::work(uint worker_id) {
  elapsedTimer timer;
  ResourceMark rm;
  HandleMark   hm;
  timer.start();

  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel adjust-preserved-marks worker %d started...", worker_id);
  }

  GangWorker* worker = (GangWorker*) Thread::current();
  Stack<markOop, mtGC>* preserved_mark_stack =
      (Stack<markOop, mtGC>*) worker->_pms_preserved_mark_stack;
  Stack<oop, mtGC>* preserved_oop_stack =
      (Stack<oop, mtGC>*) worker->_pms_preserved_oop_stack;
  MarkSweep::adjust_marks_helper(preserved_mark_stack,
                                 preserved_oop_stack,
                                 worker->_pms_preserved_count,
                                 (PreservedMark*)worker->_pms_preserved_marks);
  timer.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel adjust-preserved-marks worker %d finished (%3.5f sec)",
                           worker_id, timer.seconds());
  }
}

void PMSParEvacTask::work(uint worker_id) {
  elapsedTimer timer;
  ResourceMark rm;
  HandleMark   hm;
  timer.start();

  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel evac worker %d started...", worker_id);
  }

  PMSMarkBitMap* mark_bit_map = MarkSweep::pms_mark_bit_map();
  PMSRegionArray* regions = MarkSweep::pms_region_array_set()->region_array_for(_space);
  assert(regions != NULL && regions->space() == _space, "Must be this space");
  HeapWord* first_moved = _space->first_moved();
  HeapWord* end_of_live = _space->end_of_live();
  PMSRegion* start_r = regions->region_for_addr(first_moved);
  PMSRegion* end_r = regions->region_for_addr(end_of_live - 1); // _end_of_live is exclusive
  // All the worker threads simultaneously scan the region array to
  // find a yet-to-be-evacuated region to find a region to work on (in
  // a first-come first-served basis).
  for (PMSRegion* r = start_r; r <= end_r; r++) {
    if (r->_evac_state == PMSRegion::NOT_EVAC) {
      jbyte res = Atomic::cmpxchg(PMSRegion::BEING_EVAC,
                                  &r->_evac_state,
                                  PMSRegion::NOT_EVAC);
      if (res == PMSRegion::NOT_EVAC) {
        // Found one to work on.
        if (LogCMSParallelFullGC) {
          gclog_or_tty->print_cr("Parallel evac worker %d: region %d", worker_id, r->index());
        }
        // Lock the region's so that other threads working on regions
        // that 'depends on' the region will block.
        MutexLockerEx ml(r->_monitor, Mutex::_no_safepoint_check_flag);

        // Wait until all the dependency regions have been evacuated.
        GrowableArray<PMSRegion*>* deps = r->dependencies();
        size_t n_deps = deps->length();
        for (size_t i = 0; i < n_deps; i++) {
          PMSRegion* depr = deps->at(i);
          MutexLockerEx ml(depr->_monitor, Mutex::_no_safepoint_check_flag);
          // Block until the dependency region is evacuated
          bool has_waited = false;
          while (depr->_evac_state != PMSRegion::HAS_BEEN_EVAC) {
            has_waited = true;
            if (LogCMSParallelFullGC) {
              gclog_or_tty->print_cr("Parallel evac worker %d: waiting for region "
                                     INTX_FORMAT,
                                     i, depr->index());
            }
            depr->_monitor->wait(Mutex::_no_safepoint_check_flag);
          }
          assert(depr->_evac_state == PMSRegion::HAS_BEEN_EVAC,
                 "Must have been evacuated");
          if (has_waited && LogCMSParallelFullGC) {
            gclog_or_tty->print_cr("Parallel evac worker %d: done waiting for region "
                                   INTX_FORMAT,
                                   i, depr->index());
          }
        }

        // Here, all the dependency regions have been evacuated, go
        // ahead and start copying
        GrowableArray<PMSRegion::RegionDest*>* dests = r->destinations();
        int n_dests = dests->length();
        for (int i = 0; i < n_dests; i++) {
          PMSRegion::RegionDest* d = dests->at(i);
          if (LogCMSParallelFullGC) {
            d->log(gclog_or_tty);
          }
          HeapWord* iterate_start = MAX2(d->_from_mr.start(), first_moved);
          HeapWord* iterate_end   = MIN2(d->_from_mr.end(), end_of_live);
          mark_bit_map->iterate(_cl,
                                mark_bit_map->addr_to_bit(iterate_start),
                                mark_bit_map->addr_to_bit(iterate_end)); // Exclusive
        }
        r->_evac_state = PMSRegion::HAS_BEEN_EVAC;

        // Notify any blockers on this region
        r->_monitor->notify_all();
      }
    }
  }
  timer.stop();
  if (LogCMSParallelFullGC) {
    gclog_or_tty->print_cr("Parallel evac worker %d finished (%3.5f sec)",
                           worker_id, timer.seconds());
  }
}

// Compute dependence regions based on the destinations (RegionDest).
void PMSRegionArray::compute_compact_dependencies() {
  if (_space->end_of_live() <= _space->first_moved()) {
    // No objects moved. Nothing to compact.
    return;
  }
  PMSRegion* first_r = region_for_addr(_space->first_moved());
  PMSRegion* last_r = region_for_addr(_space->end_of_live() - 1);
  for (PMSRegion* r = first_r; r <= last_r; r++) {
    GrowableArray<PMSRegion::RegionDest*>* dests = r->destinations();
    size_t n_dests = dests->length();
    for (size_t i = 0; i < n_dests; i++) {
      PMSRegion::RegionDest* d = dests->at(i);
      CompactibleSpace* to_space = d->_to_space;
      PMSRegionArray* to_space_regions = MarkSweep::pms_region_array_set()->region_array_for(to_space);
      assert(to_space_regions != NULL && to_space_regions->space() == to_space, "Must be the to space");
      if (_space != to_space) {
        // we don't need to worry about inter-space copying
        // because we can assume there are no live objects at the
        // destinations in a different space.
        continue;
      }
      assert(_space == to_space, "Obvious");
      MemRegion d_mr(to_space->bottom() + d->_to_space_off,
                     to_space->bottom() + d->_to_space_off + d->_to_space_size);
      PMSRegion* first_dreg = to_space_regions->region_for_addr(d_mr.start());
      PMSRegion* last_dreg = to_space_regions->region_for_addr(d_mr.end() - 1);
      GrowableArray<PMSRegion*>* deps = r->dependencies();
      // Since a large object can stick out of the region, search
      // backwards for the first region with a non-empty live range
      // and check if the non-empt live region overlaps with the
      // current destination. If so, add a dependency.
      for (PMSRegion* dr = first_dreg - 1; first_r <= dr; dr--) {
        MemRegion lr = dr->live_range();
        if (!lr.is_empty()) {                      // non-empty live range
          if (!lr.intersection(d_mr).is_empty()) { // the live range overlaps
            deps->append(dr);
          }
          // search is over at the first region with a non-empty live
          // range any way
          break;
        }
      }
      // Check if the live ranges of the regions that correspond to
      // the destination overlap with the current destination.  If so,
      // add a dependency.
      for (PMSRegion* dr = first_dreg; dr <= last_dreg; dr++) {
        MemRegion lr = dr->live_range();
        if (dr < r &&                            // on the left of r
            !lr.is_empty() &&                    // non-empty live range
            !lr.intersection(d_mr).is_empty()) { // the live range overlaps
          deps->append(dr);
        }
      }
    }
  }
}

bool PMSRegionArraySet::SizeLiveObjectClosure::do_bit(size_t offset) {
  HeapWord* addr = _mark_bit_map->bit_to_addr(offset);
  oop obj = oop(addr);
  size_t live_size = obj->size();
  _live_size += live_size;
  _cfls_live_size += CompactibleFreeListSpace::adjustObjectSize(live_size);
  return true;
}

// Verify the live size accounting during Phase 1 (marking) against
// the mark bit map. This code is only used by an assert and is very
// expensive.
void PMSRegionArraySet::verify_live_size() {
#ifdef ASSERT
  PMSMarkBitMap* mark_bit_map = MarkSweep::pms_mark_bit_map();
  for (size_t i = 0; i < N_SPACES; i++) {
    PMSRegionArray* ra = &_arrays[i];
    Space* space = ra->space();
    for (PMSRegion* r = ra->begin(); r <= ra->end(); r++) {
      SizeLiveObjectClosure cl(mark_bit_map, space);
      mark_bit_map->iterate(&cl,
                            mark_bit_map->addr_to_bit(r->start()),
                            mark_bit_map->addr_to_bit(r->end()));
      size_t live_size = cl.live_size();
      size_t cfls_live_size = cl.cfls_live_size();
      assert(r->live_size() == live_size, err_msg("Wrong live size: region live size : " SIZE_FORMAT
                                                  " != " SIZE_FORMAT, r->live_size(), live_size));
      assert(r->cfls_live_size() == cfls_live_size,
             err_msg("Wrong cms live size: region live size : " SIZE_FORMAT
                     " != " SIZE_FORMAT, r->cfls_live_size(), cfls_live_size));
    }
  }
#endif // ASSERT
}

class PMSRefProcTask : public AbstractGangTask {
  typedef AbstractRefProcTaskExecutor::ProcessTask ProcessTask;
  ProcessTask&           _task;
  WorkGang*              _workers;
  int                    _n_workers;
  ObjTaskQueueSet*       _task_queues;
  ObjArrayTaskQueueSet*  _objarray_task_queues;
  ParallelTaskTerminator _terminator;
  ParallelTaskTerminator _objarray_terminator;
 public:
  PMSRefProcTask(ProcessTask& task, int n_workers, WorkGang* workers,
                 ObjTaskQueueSet* task_queues,
                 ObjArrayTaskQueueSet*  objarray_task_queues) :
      AbstractGangTask("genMarkSweep parallel ref proccessing"),
      _task(task), _n_workers(n_workers), _workers(workers),
      _task_queues(task_queues),
      _objarray_task_queues(objarray_task_queues),
      _terminator(ParallelTaskTerminator(n_workers, task_queues)),
      _objarray_terminator(ParallelTaskTerminator(n_workers, objarray_task_queues)) {}

  void work(uint worker_id) {
    elapsedTimer timer;
    ResourceMark rm;
    HandleMark   hm;
    timer.start();

    if (LogCMSParallelFullGC) {
      gclog_or_tty->print_cr("Parallel ref-proc worker %d started...", worker_id);
    }

    NamedThread* thr = Thread::current()->as_Named_thread();
    thr->_pms_task_queue = _task_queues->queue(worker_id);
    thr->_pms_objarray_task_queue = _objarray_task_queues->queue(worker_id);

    elapsedTimer timer1;
    timer1.start();
    _task.work(worker_id, MarkSweep::is_alive, MarkSweep::keep_alive, MarkSweep::follow_stack_closure);
    timer1.stop();
    if (LogCMSParallelFullGC) {
      gclog_or_tty->print_cr("Parallel ref-proc worker %d finished scanning (%3.5f sec)",
                             worker_id, timer1.seconds());
    }
    if (_task.marks_oops_alive()) {
      do_steal_work(worker_id);
    }
    timer.stop();
    if (LogCMSParallelFullGC) {
      gclog_or_tty->print_cr("Parallel ref-proc worker %d finished (%3.5f sec)",
                             worker_id, timer.seconds());
    }
    assert(_task_queues->queue(worker_id)->size() == 0, "work_queue should be empty");
    assert(_objarray_task_queues->queue(worker_id)->size() == 0, "work_queue should be empty");
  }
  void do_steal_work(uint worker_id) {
    elapsedTimer timer;
    timer.start();
    NamedThread* thr = Thread::current()->as_Named_thread();
    int seed = 17;
    int objarray_seed = 17;
    int num_steals = 0;
    ObjTaskQueue* task_queue = (ObjTaskQueue*) thr->_pms_task_queue;
    ObjArrayTaskQueue* objarray_task_queue = (ObjArrayTaskQueue*) thr->_pms_objarray_task_queue;
    assert(task_queue == _task_queues->queue(worker_id), "Sanity check");
    assert(objarray_task_queue == _objarray_task_queues->queue(worker_id), "Sanity check");
    oop obj;
    ObjArrayTask task;
    do {
      assert(task_queue->is_empty(), "Task queue should be empty before work stealing");
      assert(objarray_task_queue->is_empty(), "Task queue should be empty before work stealing");
      while (_task_queues->steal(worker_id, &seed, obj)) {
        num_steals++;
        assert(obj->is_oop(), "Santiy check");
        assert(MarkSweep::is_object_marked(obj), "Must be already marked");
        obj->follow_contents();
        MarkSweep::follow_stack();
      }
      while (_objarray_task_queues->steal(worker_id, &objarray_seed, task)) {
        num_steals++;
        ObjArrayKlass* k = (ObjArrayKlass*)task.obj()->klass();
        k->oop_follow_contents(task.obj(), task.index());
        MarkSweep::follow_stack();
      }
    } while (!_terminator.offer_termination() || !_objarray_terminator.offer_termination());
    assert(task_queue->is_empty(), "Check my task queue is empty again");
    assert(objarray_task_queue->is_empty(), "Check my task queue is empty again");
    timer.stop();
    if (LogCMSParallelFullGC) {
      gclog_or_tty->print_cr("Parallel ref-proc worker %d finished work-stealing "
                             "(%d steals, %3.5f sec)", worker_id, num_steals, timer.seconds());
    }
  }
};

class PMSRefEnqueueTask: public AbstractGangTask {
  typedef AbstractRefProcTaskExecutor::EnqueueTask EnqueueTask;
  EnqueueTask& _task;

public:
  PMSRefEnqueueTask(EnqueueTask& task)
    : AbstractGangTask("Enqueue reference objects in parallel"),
      _task(task)
  { }

  virtual void work(uint worker_id)
  {
    _task.work(worker_id);
  }
};

void PMSRefProcTaskExecutor::execute(ProcessTask& task) {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  FlexibleWorkGang* workers = gch->workers();
  assert(workers != NULL, "Need parallel worker threads.");
  PMSRefProcTask rp_task(task, workers->total_workers(),
                         workers, _task_queues, _objarray_task_queues);
  workers->run_task(&rp_task);
}

void PMSRefProcTaskExecutor::execute(EnqueueTask& task) {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  FlexibleWorkGang* workers = gch->workers();
  assert(workers != NULL, "Need parallel worker threads.");
  PMSRefEnqueueTask enq_task(task);
  workers->run_task(&enq_task);
}

