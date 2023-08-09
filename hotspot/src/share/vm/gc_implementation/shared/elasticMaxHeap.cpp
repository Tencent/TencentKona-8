/*
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
 */

#include "precompiled.hpp"
#include "gc_implementation/shared/elasticMaxHeap.hpp"
#include "utilities/defaultStream.hpp"

size_t ElasticMaxHeapConfig::_initial_max_heap_size = 0;

VM_ElasticMaxHeapOp::VM_ElasticMaxHeapOp(size_t new_max_heap) :
  VM_GC_Operation(0, GCCause::_elastic_max_heap, 0, true) {
  _new_max_heap = new_max_heap;
  _resize_success = false;
}

PS_ElasticMaxHeapOp::PS_ElasticMaxHeapOp(size_t new_max_heap) :
  VM_ElasticMaxHeapOp(new_max_heap) {
}

Gen_ElasticMaxHeapOp::Gen_ElasticMaxHeapOp(size_t new_max_heap) :
  VM_ElasticMaxHeapOp(new_max_heap) {
}

G1_ElasticMaxHeapOp::G1_ElasticMaxHeapOp(size_t new_max_heap) :
  VM_ElasticMaxHeapOp(new_max_heap) {
}

bool VM_ElasticMaxHeapOp::skip_operation() const {
  return false;
}
/*
 * 1. calculate new young/old gen limit size.
 * 2. trigger Full GC if necessary
 * 3. check and reset new limitation
 */
void PS_ElasticMaxHeapOp::doit() {
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  GenCollectorPolicy* policy = (GenCollectorPolicy*)(heap->collector_policy());
  assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "must be a ParallelScavengeHeap");

  // step1
  PSOldGen* old_gen      = heap->old_gen();
  PSYoungGen* young_gen  = heap->young_gen();
  size_t cur_heap_limit  = heap->current_max_heap_size();
  size_t cur_old_limit   = old_gen->gen_size_limit();
  size_t cur_young_limit = young_gen->gen_size_limit();
  size_t gen_alignment   = policy->gen_alignment();
  bool is_shrink         = _new_max_heap < cur_heap_limit;

  const size_t young_reserved_size = young_gen->reserved().byte_size();
  const size_t young_min_size =  young_gen->min_gen_size();
  const size_t old_reserved_size = old_gen->reserved().byte_size();
  const size_t old_min_size = old_gen->min_gen_size();

  guarantee(cur_old_limit + cur_young_limit == cur_heap_limit, "must be");
  if (_new_max_heap == cur_heap_limit) {
    EMH_LOG("PS_ElasticMaxHeapOp abort: no size change");
    return;
  }

  // fix with young gen size limitation
  size_t new_young_limit = policy->scale_by_NewRatio_aligned(_new_max_heap);
  new_young_limit = MIN2(new_young_limit, young_reserved_size);
  new_young_limit = MAX2(new_young_limit, young_min_size);
  // align shrink/expand direction
  if ((is_shrink && (new_young_limit > cur_young_limit)) ||
      (!is_shrink && (new_young_limit < cur_young_limit))) {
    new_young_limit = cur_young_limit;
  }
  size_t new_old_limit = _new_max_heap - new_young_limit;

  if (new_old_limit > old_reserved_size) {
    new_old_limit = old_reserved_size;
    new_young_limit = _new_max_heap - new_old_limit;
  }

  // keep the new_old_limit aligned with shrink/expand direction
  if ((is_shrink && (new_old_limit > cur_old_limit)) ||
      (!is_shrink && (new_old_limit < cur_old_limit))) {
    new_old_limit = cur_old_limit;
    new_young_limit = _new_max_heap - new_old_limit;
  }

  // After the final calcuation, check the leagle limit
  if ((new_old_limit < old_min_size) ||
      (new_old_limit > old_reserved_size) ||
      (new_young_limit < young_min_size) ||
      (new_young_limit > young_reserved_size)) {
    EMH_LOG("PS_ElasticMaxHeapOp abort: can not calculate new legal limit:"
             " new_old_limit: "SIZE_FORMAT"K, " "old gen min size: "SIZE_FORMAT"K, old gen reserved size: "SIZE_FORMAT"K"
             " new_young_limit: "SIZE_FORMAT"K, ""young gen min size: "SIZE_FORMAT"K, young gen reserved size: "SIZE_FORMAT"K",
             (new_old_limit / K), (old_min_size / K), (old_reserved_size / K),
             (new_young_limit / K), (young_min_size / K), (young_reserved_size / K));
    return;
  }

  EMH_LOG("PS_ElasticMaxHeapOp plan: "
          "desired young gen size (" SIZE_FORMAT "K" "->" SIZE_FORMAT "K), "
          "desired old gen size(" SIZE_FORMAT "K" "->" SIZE_FORMAT "K), ",
          (cur_young_limit / K),
          (new_young_limit / K),
          (cur_old_limit / K),
          (new_old_limit / K));
  if (is_shrink) {
    guarantee(new_old_limit <= cur_old_limit && new_young_limit <= cur_young_limit, "must be");
  } else {
    guarantee(new_old_limit >= cur_old_limit && new_young_limit >= cur_young_limit, "must be");
  }

  // step2
  // Check resize legality
  // 1. expand: always legal
  // 2. shrink:
  //    1 young: must be empty after full gc
  //    2 old: accordign to PSAdaptiveSizePolicy::calculated_old_free_size_in_bytes
  //      (new_old_limit - old_used) >= min_free
  if (is_shrink) {
    // check whether old/young can be resized, trigger full gc as needed
    if (!ps_old_gen_can_shrink(new_old_limit, false) || !ps_young_gen_can_shrink(new_young_limit)) {
      GCCauseSetter gccs(heap, _gc_cause);
      heap->do_full_collection(true);
      EMH_LOG("PS_ElasticMaxHeapOp heap after Full GC");
      if (TraceElasticMaxHeap) {
        heap->print_on(gclog_or_tty);
      }
      if (young_gen->used_in_bytes() != 0) {
        EMH_LOG("PS_ElasticMaxHeapOp abort: young is not empty after full gc");
        return;
      }
    }

    if (!ps_old_gen_can_shrink(new_old_limit, true)) {
      // still not enough old free after full gc, cannot shrink and print log
      return;
    }

    // step3
    // shrink generation committed size if needed
    // 1. old gen
    //    1 old gen can shrink capacity without full gc
    //    2 old gen have passed shrink valid check since the code is executed here
    //    3 old gen can shrink capacity if needed
    // 2. young gen
    //    1 young gen must shrink capacity after full gc
    //    2 there may be three situations after shrink valid check in step2:
    //        1) both old gen and young gen have passed the check,
    //           indicating new_young_limit is big enough,
    //           there's no need to shrink capacity
    //        2) old gen failed the check and triggered full gc
    //        3) young gen failed the check and triggered full gc
    if (old_gen->capacity_in_bytes() > new_old_limit) {
      size_t desried_free = new_old_limit - old_gen->used_in_bytes();
      char* old_high = old_gen->virtual_space()->committed_high_addr();
      old_gen->resize(desried_free);
      char* new_old_high = old_gen->virtual_space()->committed_high_addr();
      if (old_gen->capacity_in_bytes() > new_old_limit) {
        EMH_LOG("PS_ElasticMaxHeapOp abort: resize old fail " SIZE_FORMAT "K",
                old_gen->capacity_in_bytes() / K);
        return;
      }
      EMH_LOG("PS_ElasticMaxHeapOp continue: shrink old success " SIZE_FORMAT "K",
              old_gen->capacity_in_bytes() / K);
      if (old_high > new_old_high) {
        // shrink is caused by elastic max heap, free physical memory
        size_t shrink_bytes = old_high - new_old_high;
        guarantee((shrink_bytes > 0) && (shrink_bytes % os::vm_page_size() == 0), "should be");
        bool result = os::free_heap_physical_memory(new_old_high, shrink_bytes);
        guarantee(result, "free heap physical memory should be successful");
      }
    }

    if (young_gen->virtual_space()->committed_size() > new_young_limit) {
      // entering this branch means full gc must have been triggered
      guarantee(young_gen->eden_space()->is_empty() &&
                young_gen->to_space()->is_empty() &&
                young_gen->from_space()->is_empty(),
                "must be empty");

      char* young_high = young_gen->virtual_space()->committed_high_addr();
      if (young_gen->shrink_after_full_gc(new_young_limit) == false) {
        EMH_LOG("PS_ElasticMaxHeapOp abort: shrink young fail");
        return;
      }
      char* new_young_high = young_gen->virtual_space()->committed_high_addr();
      EMH_LOG("PS_ElasticMaxHeapOp continue: shrink young success " SIZE_FORMAT "K",
               young_gen->virtual_space()->committed_size() / K);
      if (young_high > new_young_high) {
        // shrink is caused by elastic max heap, free physical memory
        size_t shrink_bytes = young_high - new_young_high;
        guarantee((shrink_bytes > 0) && (shrink_bytes % os::vm_page_size() == 0), "should be");
        bool result = os::free_heap_physical_memory(new_young_high, shrink_bytes);
        guarantee(result, "free heap physical memory should be successful");
      }
    }
  }
  // update young/old gen limit, avoid further expand
  old_gen->set_cur_max_gen_size(new_old_limit);
  young_gen->set_cur_max_gen_size(new_young_limit);
  heap->set_current_max_heap_size(_new_max_heap);
  _resize_success = true;
  EMH_LOG("PS_ElasticMaxHeapOp success")
}

/*
 * 1. calculate desired new old and young gen limit with new size
 * 2. trigger full gc and perform adjustion in Generation.compute_new_size
 * 3. if step2 success, set new heap/old/young limit
 */
void Gen_ElasticMaxHeapOp::doit() {
  GenCollectedHeap* heap = (GenCollectedHeap*)Universe::heap();
  assert(heap->kind() == CollectedHeap::GenCollectedHeap, "must be a GenCollectedHeap");
  assert(heap->n_gens() == 2, "must be");

  Generation* young       = heap->get_gen(0);
  Generation* old         = heap->get_gen(1);
  CollectorPolicy* policy = heap->collector_policy();
  size_t gen_alignment    = Generation::GenGrain;

  // step1
  // both current size is recorded elastic max heap size, not actual reserved size
  size_t cur_max_heap    = heap->current_max_heap_size();
  size_t cur_old_limit   = old->EMH_size();
  size_t cur_young_limit = young->EMH_size();

  const size_t young_reserved_size = young->reserved().byte_size();
  const size_t old_reserved_size = old->reserved().byte_size();

  guarantee(cur_old_limit + cur_young_limit == cur_max_heap, "must be");
  if (_new_max_heap == cur_max_heap) {
    EMH_LOG("Gen_ElasticMaxHeapOp abort: no size change");
    return;
  }
  bool is_shrink = _new_max_heap < cur_max_heap;
  size_t new_young_limit = 0;
  size_t new_old_limit   = 0;
  if ((old->kind() != Generation::ConcurrentMarkSweep) || CMSIgnoreYoungGenPerWorker) {
    new_young_limit = align_size_down_bounded(_new_max_heap / (NewRatio + 1), gen_alignment);
    EMH_LOG("Gen_ElasticMaxHeapOp cacluate new young limit with NewRatio");
  } else {
    const uintx parallel_gc_threads = (ParallelGCThreads == 0 ? 1 : ParallelGCThreads);
    size_t young_gen_per_worker = CMSYoungGenPerWorker;
    size_t preferred_max_new_size_unaligned =
      MIN2((size_t)(_new_max_heap / (NewRatio + 1)), ScaleForWordSize(young_gen_per_worker * parallel_gc_threads));
    new_young_limit = align_size_up(preferred_max_new_size_unaligned, gen_alignment);
    EMH_LOG("Gen_ElasticMaxHeapOp cacluate new young limit with fixed CMSYoungGenPerWorker and NewRatio");
  }
  // keep new limit aligned with shrink/expand direction
  if ((is_shrink && (new_young_limit > cur_young_limit)) ||
      (!is_shrink && (new_young_limit < cur_young_limit))) {
    new_young_limit = cur_young_limit;
  }
  if (new_young_limit > young_reserved_size) {
    new_young_limit = young_reserved_size;
  }

  new_old_limit = _new_max_heap - new_young_limit;
  assert((new_old_limit % gen_alignment) == 0, "must be");

  if (new_old_limit > old_reserved_size) {
    new_old_limit = old_reserved_size;
    new_young_limit = _new_max_heap - new_old_limit;
  }

  // keep the new_old_limit aligned with shrink/expand direction
  if ((is_shrink && (new_old_limit > cur_old_limit)) ||
      (!is_shrink && (new_old_limit < cur_old_limit))) {
    new_old_limit = cur_old_limit;
    new_young_limit = _new_max_heap - new_old_limit;
  }

  // After the final calcuation, check the leagle limit
  if ((new_old_limit > old_reserved_size) ||
      (new_young_limit > young_reserved_size)) {
    EMH_LOG("Gen_ElasticMaxHeapOp abort: can not calculate new leagle limit: "
            " new_old_limit: "SIZE_FORMAT"K, old gen reserved size: "SIZE_FORMAT"K"
            " new_young_limit: "SIZE_FORMAT"K, young gen reserved size: "SIZE_FORMAT"K",
            (new_old_limit / K), (old_reserved_size / K),
            (new_young_limit / K), (young_reserved_size / K));
    return;
  }

  if (is_shrink) {
    guarantee(new_old_limit <= cur_old_limit && new_young_limit <= cur_young_limit, "must be");
  } else {
    guarantee(new_old_limit >= cur_old_limit && new_young_limit >= cur_young_limit, "must be");
  }

  EMH_LOG("Gen_ElasticMaxHeapOp plan: "
          "desired young gen size (" SIZE_FORMAT "K" "->" SIZE_FORMAT "K), "
          "desired old gen size(" SIZE_FORMAT "K" "->" SIZE_FORMAT "K)",
          (cur_young_limit / K),
          (new_young_limit / K),
          (cur_old_limit / K),
          (new_old_limit / K));

  // step2
  if (is_shrink) {
    if (new_young_limit < young->capacity() || new_old_limit < old->capacity()) {
      // cannot shrink directly, trigger full gc
      old->set_exp_EMH_size(new_old_limit);
      young->set_exp_EMH_size(new_young_limit);
      GCCauseSetter gccs(heap, _gc_cause);
      heap->do_full_collection(true);
      EMH_LOG("Gen_ElasticMaxHeapOp heap after Full GC");
      if (TraceElasticMaxHeap) {
        heap->print_on(gclog_or_tty);
      }
      old->set_exp_EMH_size(0);
      young->set_exp_EMH_size(0);
    }
  }

  // step3 check and adjust heap/generation max capacity
  size_t old_committed   = old->committed_size();
  size_t young_committed = young->committed_size();
  EMH_LOG("Gen_ElasticMaxHeapOp: [current limit, committed, desired limit]: "
          "heap [" SIZE_FORMAT "K, " SIZE_FORMAT "K, " SIZE_FORMAT "], "
          "old [" SIZE_FORMAT "K, " SIZE_FORMAT "K, " SIZE_FORMAT "], "
          "young [" SIZE_FORMAT "K, " SIZE_FORMAT "K, " SIZE_FORMAT "]",
          cur_max_heap / K, (old_committed + young_committed) / K, _new_max_heap / K,
          cur_old_limit / K, old_committed / K, new_old_limit / K,
          cur_young_limit / K, young_committed / K, new_young_limit / K);

  if (old_committed <= new_old_limit && young_committed <= new_young_limit) {
    heap->set_current_max_heap_size(_new_max_heap);
    old->set_EMH_size(new_old_limit);
    old->update_gen_max_counter(new_old_limit);
    young->set_EMH_size(new_young_limit);
    young->update_gen_max_counter(new_young_limit);
    if (is_shrink) {
      // shrink is caused by elastic max heap, free physical memory
      char* base = (char*)young->reserved().start() + young_committed;
      size_t shrink_bytes = (char*)young->reserved().end() - base;
      if (shrink_bytes > 0) {
        bool result = os::free_heap_physical_memory(base, shrink_bytes);
        guarantee(result, "free heap physical memory should be successful");
      }
      base = (char*)old->reserved().start() + old_committed;
      shrink_bytes = (char*)old->reserved().end() - base;
      if (shrink_bytes > 0) {
        bool result = os::free_heap_physical_memory(base, shrink_bytes);
        guarantee(result, "free heap physical memory should be successful");
      }
    }
    _resize_success = true;
    EMH_LOG("Gen_ElasticMaxHeapOp success");
  }
}

/*
 * No need calculate young/old size, shrink will adjust young automatically.
 * ensure young_list_length, _young_list_max_length, _young_list_target_length align.
 *
 * 1. check if need perform gc: new_heap_max >= minimum_desired_capacity
 * 2. perform full GC if necessary
 * 3. update new limit
 * 4. validation
 */
void G1_ElasticMaxHeapOp::doit() {
  G1CollectedHeap* heap      = (G1CollectedHeap*)Universe::heap();
  G1CollectorPolicy* policy  = heap->g1_policy();
  const size_t min_heap_size = policy->min_heap_byte_size();
  const size_t max_heap_size = heap->current_max_heap_size();
  bool is_shrink             = _new_max_heap < max_heap_size;

  // step1. calculate maximum_used_percentage for shrink validity check
  const double minimum_free_percentage = (double) MinHeapFreeRatio / 100.0;
  const double maximum_used_percentage = 1.0 - minimum_free_percentage;

  // step2 trigger GC as needed and resize
  if (is_shrink) {
    bool triggered_full_gc = false;
    if (!g1_can_shrink(_new_max_heap, maximum_used_percentage, max_heap_size)) {
      // trigger Young GC
      policy->set_gcs_are_young(true);
      GCCauseSetter gccs(heap, _gc_cause);
      bool minor_gc_succeeded = heap->do_collection_pause_at_safepoint(policy->max_pause_time_ms());
      if (minor_gc_succeeded) {
        EMH_LOG("G1_ElasticMaxHeapOp heap after Young GC");
        if (TraceElasticMaxHeap) {
          heap->print_on(gclog_or_tty);
        }
      }
      if (!g1_can_shrink(_new_max_heap, maximum_used_percentage, max_heap_size)) {
        // trigger Full GC and adjust everything in resize_if_necessary_after_full_collection
        heap->set_exp_EMH_size(_new_max_heap);
        heap->do_full_collection(true);
        EMH_LOG("G1_ElasticMaxHeapOp heap after Full GC");
        if (TraceElasticMaxHeap) {
          heap->print_on(gclog_or_tty);
        }
        heap->set_exp_EMH_size(0);
        triggered_full_gc = true;
      }
    }

    if (!triggered_full_gc) {
      // there may be two situations when entering this branch:
      //     1. first check passed, no GC triggered
      //     2. first check failed, triggered Young GC,
      //        second check passed
      // so the shrink has not been completed and it must be valid to shrink
      g1_shrink_without_full_gc(_new_max_heap);
    }
  }

  EMH_LOG("G1_ElasticMaxHeapOp: current capacity " SIZE_FORMAT "K, new max heap " SIZE_FORMAT "K",
          heap->capacity() / K, _new_max_heap / K);

  // step3 check if can update new limit
  if (heap->capacity() <= _new_max_heap) {
    heap->set_current_max_heap_size(_new_max_heap);
    uint EMH_len = (uint)(_new_max_heap / HeapRegion::GrainBytes);
    heap->hrm()->set_EMH_length(EMH_len);
    // G1 young/old share same max size
    heap->update_gen_max_counter(_new_max_heap);
    _resize_success = true;
    EMH_LOG("G1_ElasticMaxHeapOp success");
  }
}

/*
 * step1: validity check
 * new current max heap must be:
 * 1. >= min_heap_byte_size
 * 2. <= max_heap_byte_size
 * 3. not equal with current_max_heap_size
 *
 * step2: build vm operation op and execute
 */
bool CollectedHeap::update_elastic_max_heap(size_t new_size, outputStream* st, bool init_shrink){
  new_size = align_size_up(new_size, collector_policy()->heap_alignment());
  if (new_size > collector_policy()->max_heap_byte_size()) {
    st->print_cr("GC.elastic_max_heap " SIZE_FORMAT "K exceeds maximum limit " SIZE_FORMAT "K",
                 (new_size / K),
                 (collector_policy()->max_heap_byte_size() / K));
    return false;
  }
  if (new_size < collector_policy()->min_heap_byte_size()) {
    st->print_cr("GC.elastic_max_heap " SIZE_FORMAT "K below minimum limit " SIZE_FORMAT "K",
                 (new_size / K),
                 (collector_policy()->min_heap_byte_size() / K));
    return false;
  }
  // don't print log if it is init shrink triggered by ElasticMaxHeapSize
  if (new_size == current_max_heap_size()) {
    if (!init_shrink) {
      st->print_cr("GC.elastic_max_heap " SIZE_FORMAT "K same with current max heap size " SIZE_FORMAT "K",
                   (new_size / K),
                   (current_max_heap_size() / K));
    }
    return true;
  }
  if (!init_shrink) {
    st->print_cr("GC.elastic_max_heap (" SIZE_FORMAT "K" "->" SIZE_FORMAT "K)(" SIZE_FORMAT "K)",
                 (current_max_heap_size() / K),
                 (new_size / K),
                 (collector_policy()->max_heap_byte_size() / K));
  }
  assert(!Heap_lock->owned_by_self(), "this thread should not own the Heap_lock");
  // trigger real operations
  bool result = false;
  switch (Universe::heap()->kind()) {
    case CollectedHeap::ParallelScavengeHeap: {
      PS_ElasticMaxHeapOp op(new_size);
      VMThread::execute(&op);
      result = op.resize_success();
      break;
    }
    case CollectedHeap::GenCollectedHeap: {
      Gen_ElasticMaxHeapOp op(new_size);
      VMThread::execute(&op);
      result = op.resize_success();
      break;
    }
    case CollectedHeap::G1CollectedHeap: {
      G1_ElasticMaxHeapOp op(new_size);
      VMThread::execute(&op);
      result = op.resize_success();
      break;
    }
    default:
      ShouldNotReachHere();
  }
  return result;
}

bool PS_ElasticMaxHeapOp::ps_old_gen_can_shrink(size_t new_limit, bool print_logs) {
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  size_t old_used_bytes = heap->old_gen()->used_in_bytes();
  PSAdaptiveSizePolicy* size_policy = heap->size_policy();
  uintx min_heap_free_ration = MinHeapFreeRatio != 0 ? MinHeapFreeRatio : ElasticMaxHeapShrinkMinFreeRatio;
  size_t min_free = size_policy->calculate_free_based_on_live(old_used_bytes, min_heap_free_ration);
  min_free = align_size_up(min_free, heap->old_gen()->virtual_space()->alignment());
  bool can_shrink = (new_limit >= (old_used_bytes + min_free));
  if (!can_shrink && print_logs) {
    EMH_LOG("PS_ElasticMaxHeapOp abort: not enough old free for shrink"
            " expect_free " SIZE_FORMAT "K"
            " old_used_bytes " SIZE_FORMAT "K"
            " new_old_limit " SIZE_FORMAT "K",
            min_free / K, old_used_bytes / K, new_limit / K);
  }
  return can_shrink;
}

bool PS_ElasticMaxHeapOp::ps_young_gen_can_shrink(size_t new_limit) {
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  PSYoungGen* young_gen  = heap->young_gen();
  size_t committed_size = young_gen->virtual_space()->committed_size();
  bool can_shrink = (new_limit >= committed_size);
  return can_shrink;
}

// Resize for DynamicHeapSize, shrink to new_size
bool PSYoungGen::shrink_after_full_gc(size_t new_size) {
  const size_t alignment = virtual_space()->alignment();
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  size_t space_alignment = heap->space_alignment();
  size_t orig_size = virtual_space()->committed_size();
  guarantee(eden_space()->is_empty() && to_space()->is_empty() && from_space()->is_empty(), "must be empty");
  guarantee(new_size % alignment == 0, "must be");
  guarantee(new_size < orig_size, "must be");

  // shrink virtual space
  size_t shrink_bytes = virtual_space()->committed_size() - new_size;
  bool success = virtual_space()->shrink_by(shrink_bytes);
  EMH_LOG("PSYoungGen::shrink_after_full_gc: shrink virutal space %s "
          "orig committed " SIZE_FORMAT "K "
          "current committed " SIZE_FORMAT "K "
          "shrink by " SIZE_FORMAT "K",
          success ? "success" : "fail",
          orig_size / K,
          virtual_space()->committed_size() / K,
          shrink_bytes / K);

  if (!success) {
    return false;
  }

  // caculate new eden/survivor size
  // shrink with same ratio, let size policy adjust later
  size_t current_survivor_ratio = eden_space()->capacity_in_bytes() / from_space()->capacity_in_bytes();
  current_survivor_ratio = MAX2(current_survivor_ratio, (size_t)1);
  size_t new_survivor_size = new_size / (current_survivor_ratio + 2);
  new_survivor_size = align_size_down(new_survivor_size, space_alignment);
  new_survivor_size = MAX2(new_survivor_size, space_alignment);
  size_t new_eden_size = new_size - 2 * new_survivor_size;

  guarantee(new_eden_size % space_alignment == 0, "must be");
  EMH_LOG("PSYoungGen::shrink_after_full_gc: "
          "new eden size " SIZE_FORMAT "K "
          "new survivor size " SIZE_FORMAT "K "
          "new young gen size " SIZE_FORMAT "K",
          new_eden_size / K,
          new_survivor_size / K,
          new_size / K);

  // setup new eden/suvirvor space
  set_space_boundaries(new_eden_size, new_survivor_size);
  post_resize();
  if (TraceElasticMaxHeap) {
    print_on(gclog_or_tty);
  }
  return true;
}

/*
 * check if heap can shrink
 * 1. calculate minimun heap size by used size
 * 2. skip GC if new heap size bigger than minimun heap size
 */
bool G1_ElasticMaxHeapOp::g1_can_shrink(size_t _new_max_heap,
                                      double maximum_used_percentage,
                                      size_t max_heap_size) {
  G1CollectedHeap* heap = (G1CollectedHeap*)Universe::heap();
  // We have to be careful here as these two calculations can overflow
  // 32-bit size_t's.
  double used_after_gc_d = (double) heap->used();
  double minimum_desired_capacity_d = used_after_gc_d / maximum_used_percentage;
  double desired_capacity_upper_bound = (double) max_heap_size;
  minimum_desired_capacity_d = MIN2(minimum_desired_capacity_d, desired_capacity_upper_bound);
  size_t minimum_desired_capacity = (size_t) minimum_desired_capacity_d;
  minimum_desired_capacity = MIN2(minimum_desired_capacity, max_heap_size);
  bool can_shrink = (_new_max_heap >= minimum_desired_capacity);
  return can_shrink;
}

void G1_ElasticMaxHeapOp::g1_shrink_without_full_gc(size_t _new_max_heap) {
  G1CollectedHeap* heap = (G1CollectedHeap*)Universe::heap();
  size_t capacity_before_shrink = heap->capacity();
  // _new_max_heap is large enough, do nothing
  if (_new_max_heap >= capacity_before_shrink) {
    return;
  }
  // Capacity too large, compute shrinking size and shrink
  size_t shrink_bytes = capacity_before_shrink - _new_max_heap;
  heap->verify_region_sets_optional();
  heap->tear_down_region_sets(true /* free_list_only */);
  heap->shrink_helper(shrink_bytes);
  heap->rebuild_region_sets(true /* free_list_only */, true /* is_elastic_max_heap_shrink */);
  heap->_hrm.verify_optional();
  heap->verify_region_sets_optional();
  heap->verify_after_gc();

  EMH_LOG("G1_ElasticMaxHeapOp: attempt heap shrinking for elastic max heap %s "
          "origin capacity " SIZE_FORMAT "K "
          "new capacity " SIZE_FORMAT "K "
          "shrink by " SIZE_FORMAT "K",
          heap->capacity() <= _new_max_heap ? "success" : "fail",
          capacity_before_shrink / K,
          heap->capacity() / K,
          shrink_bytes / K);
}

#define CAN_NOT_CMDLINE_CHECK(OPTION)                                         \
  if (FLAG_IS_CMDLINE(OPTION)) {                                              \
    jio_fprintf(defaultStream::error_stream(),                                \
        "-XX:+ElasticMaxHeap can not be combined with -XX:%s=..\n", #OPTION); \
    vm_exit(1);                                                               \
  }                                                                           \

#define MUST_BE_FALSE_CHECK(OPTION)                                         \
  if (OPTION) {                                                             \
    jio_fprintf(defaultStream::error_stream(),                              \
        "-XX:+ElasticMaxHeap can not be combined with -XX:+%s\n", #OPTION); \
    vm_exit(1);                                                             \
  }                                                                         \

#define MUST_BE_TRUE_CHECK(OPTION)                                          \
  if (!OPTION) {                                                            \
    jio_fprintf(defaultStream::error_stream(),                              \
        "-XX:+ElasticMaxHeap can not be combined with -XX:-%s\n", #OPTION); \
    vm_exit(1);                                                             \
  }

/*
 * conflict options check for Elastic Max Heap(EMH), some might not support and
 * other might support later. Major confilict options reasons:
 * 1. options specify new/old gen size that might break with EMH.
 * 2. GC only sweep cannot compact
 * 3. not implemented/tested yet, like serial/gen collected and not CMS
 * 4. must be true, UseAdaptiveSizePolicy (otherwise all size fixed)
 *
 * some check has been done before heap initialize in cased of FLAG is made ergonomic.
 */
void ElasticMaxHeapChecker::check_common_options() {
  if (!ElasticMaxHeap) {
    return;
  }
#if !defined(LINUX)
  jio_fprintf(defaultStream::error_stream(), "-XX:+ElasticMaxHeap can only used on Linux now");
  vm_exit(1);
#endif

  CAN_NOT_CMDLINE_CHECK(OldSize);
  CAN_NOT_CMDLINE_CHECK(NewSize);
  CAN_NOT_CMDLINE_CHECK(MaxNewSize);

  MUST_BE_FALSE_CHECK(UseAdaptiveGCBoundary);

  MUST_BE_TRUE_CHECK(UseAdaptiveSizePolicy);
}

void ElasticMaxHeapChecker::check_PS_options() {
}

void ElasticMaxHeapChecker::check_G1_options() {
}

void ElasticMaxHeapChecker::check_GenCollected_options() {
  if (!ElasticMaxHeap) {
    return;
  }
  MUST_BE_TRUE_CHECK(UseConcMarkSweepGC);
}
