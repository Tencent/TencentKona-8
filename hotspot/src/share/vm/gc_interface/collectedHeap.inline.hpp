/*
 * Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_INLINE_HPP
#define SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_INLINE_HPP

#include "gc_interface/allocTracer.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "memory/threadLocalAllocBuffer.inline.hpp"
#include "memory/universe.hpp"
#include "oops/arrayOop.hpp"
#include "prims/jvmtiExport.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/thread.inline.hpp"
#include "services/lowMemoryDetector.hpp"
#include "utilities/copy.hpp"

// Inline allocation implementations.

void CollectedHeap::post_allocation_setup_common(KlassHandle klass,
                                                 HeapWord* obj) {
  post_allocation_setup_no_klass_install(klass, obj);
  post_allocation_install_obj_klass(klass, oop(obj));
}

void CollectedHeap::post_allocation_setup_no_klass_install(KlassHandle klass,
                                                           HeapWord* objPtr) {
  oop obj = (oop)objPtr;

  assert(obj != NULL, "NULL object pointer");
  if (UseBiasedLocking && (klass() != NULL)) {
    obj->set_mark(klass->prototype_header());
  } else {
    // May be bootstrapping
    obj->set_mark(markOopDesc::prototype());
  }
}

void CollectedHeap::post_allocation_install_obj_klass(KlassHandle klass,
                                                   oop obj) {
  // These asserts are kind of complicated because of klassKlass
  // and the beginning of the world.
  assert(klass() != NULL || !Universe::is_fully_initialized(), "NULL klass");
  assert(klass() == NULL || klass()->is_klass(), "not a klass");
  assert(obj != NULL, "NULL object pointer");
  obj->set_klass(klass());
  assert(!Universe::is_fully_initialized() || obj->klass() != NULL,
         "missing klass");
}

// Support for jvmti and dtrace
inline void post_allocation_notify(KlassHandle klass, oop obj, int size) {
  // support low memory notifications (no-op if not enabled)
  LowMemoryDetector::detect_low_memory_for_collected_pools();

  // support for JVMTI VMObjectAlloc event (no-op if not enabled)
  JvmtiExport::vm_object_alloc_event_collector(obj);

  if (DTraceAllocProbes) {
    // support for Dtrace object alloc event (no-op most of the time)
    if (klass() != NULL && klass()->name() != NULL) {
      SharedRuntime::dtrace_object_alloc(obj, size);
    }
  }
}

void CollectedHeap::post_allocation_setup_obj(KlassHandle klass,
                                              HeapWord* obj,
                                              int size) {
  post_allocation_setup_common(klass, obj);
  assert(Universe::is_bootstrapping() ||
         !((oop)obj)->is_array(), "must not be an array");
  // notify jvmti and dtrace
  post_allocation_notify(klass, (oop)obj, size);
}

void CollectedHeap::post_allocation_setup_array(KlassHandle klass,
                                                HeapWord* obj,
                                                int length) {
  // Set array length before setting the _klass field
  // in post_allocation_setup_common() because the klass field
  // indicates that the object is parsable by concurrent GC.
  assert(length >= 0, "length should be non-negative");
  ((arrayOop)obj)->set_length(length);
  post_allocation_setup_common(klass, obj);
  oop new_obj = (oop)obj;
  assert(new_obj->is_array(), "must be an array");
  // notify jvmti and dtrace (must be after length is set for dtrace)
  post_allocation_notify(klass, new_obj, new_obj->size());
}

HeapWord* CollectedHeap::common_mem_allocate_noinit(KlassHandle klass, size_t size, TRAPS) {

  // Clear unhandled oops for memory allocation.  Memory allocation might
  // not take out a lock if from tlab, so clear here.
  CHECK_UNHANDLED_OOPS_ONLY(THREAD->clear_unhandled_oops();)

  if (HAS_PENDING_EXCEPTION) {
    NOT_PRODUCT(guarantee(false, "Should not allocate with exception pending"));
    return NULL;  // caller does a CHECK_0 too
  }

  HeapWord* result = NULL;
  if (UseTLAB) {
    result = allocate_from_tlab(klass, THREAD, size);
    if (result != NULL) {
      assert(!HAS_PENDING_EXCEPTION,
             "Unexpected exception, will result in uninitialized storage");
      return result;
    }
  }
  bool gc_overhead_limit_was_exceeded = false;
  result = Universe::heap()->mem_allocate(size,
                                          &gc_overhead_limit_was_exceeded);
  if (result != NULL) {
    NOT_PRODUCT(Universe::heap()->
      check_for_non_bad_heap_word_value(result, size));
    assert(!HAS_PENDING_EXCEPTION,
           "Unexpected exception, will result in uninitialized storage");
    THREAD->incr_allocated_bytes(size * HeapWordSize);

    AllocTracer::send_allocation_outside_tlab_event(klass, size * HeapWordSize);

    return result;
  }


  if (!gc_overhead_limit_was_exceeded) {
    // -XX:+HeapDumpOnOutOfMemoryError and -XX:OnOutOfMemoryError support
    report_java_out_of_memory("Java heap space");

    if (JvmtiExport::should_post_resource_exhausted()) {
      JvmtiExport::post_resource_exhausted(
        JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR | JVMTI_RESOURCE_EXHAUSTED_JAVA_HEAP,
        "Java heap space");
    }

    THROW_OOP_0(Universe::out_of_memory_error_java_heap());
  } else {
    // -XX:+HeapDumpOnOutOfMemoryError and -XX:OnOutOfMemoryError support
    report_java_out_of_memory("GC overhead limit exceeded");

    if (JvmtiExport::should_post_resource_exhausted()) {
      JvmtiExport::post_resource_exhausted(
        JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR | JVMTI_RESOURCE_EXHAUSTED_JAVA_HEAP,
        "GC overhead limit exceeded");
    }

    THROW_OOP_0(Universe::out_of_memory_error_gc_overhead_limit());
  }
}

HeapWord* CollectedHeap::common_mem_allocate_init(KlassHandle klass, size_t size, TRAPS) {
  HeapWord* obj = common_mem_allocate_noinit(klass, size, CHECK_NULL);
  init_obj(obj, size);
  return obj;
}

HeapWord* CollectedHeap::allocate_from_tlab(KlassHandle klass, Thread* thread, size_t size) {
  assert(UseTLAB, "should use UseTLAB");

  HeapWord* obj = thread->tlab().allocate(size);
  if (obj != NULL) {
    return obj;
  }
  // Otherwise...
  return allocate_from_tlab_slow(klass, thread, size);
}

void CollectedHeap::init_obj(HeapWord* obj, size_t size) {
  assert(obj != NULL, "cannot initialize NULL object");
  const size_t hs = oopDesc::header_size();
  assert(size >= hs, "unexpected object size");
  ((oop)obj)->set_klass_gap(0);
  Copy::fill_to_aligned_words(obj + hs, size - hs);
}

oop CollectedHeap::obj_allocate(KlassHandle klass, int size, TRAPS) {
  debug_only(check_for_valid_allocation_state());
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
  HeapWord* obj = common_mem_allocate_init(klass, size, CHECK_NULL);
  post_allocation_setup_obj(klass, obj, size);
  NOT_PRODUCT(Universe::heap()->check_for_bad_heap_word_value(obj, size));
  return (oop)obj;
}

oop CollectedHeap::array_allocate(KlassHandle klass,
                                  int size,
                                  int length,
                                  TRAPS) {
  debug_only(check_for_valid_allocation_state());
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
  HeapWord* obj = common_mem_allocate_init(klass, size, CHECK_NULL);
  post_allocation_setup_array(klass, obj, length);
  NOT_PRODUCT(Universe::heap()->check_for_bad_heap_word_value(obj, size));
  return (oop)obj;
}

oop CollectedHeap::array_allocate_nozero(KlassHandle klass,
                                         int size,
                                         int length,
                                         TRAPS) {
  debug_only(check_for_valid_allocation_state());
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
  HeapWord* obj = common_mem_allocate_noinit(klass, size, CHECK_NULL);
  ((oop)obj)->set_klass_gap(0);
  post_allocation_setup_array(klass, obj, length);
#ifndef PRODUCT
  const size_t hs = oopDesc::header_size()+1;
  Universe::heap()->check_for_non_bad_heap_word_value(obj+hs, size-hs);
#endif
  return (oop)obj;
}

inline void CollectedHeap::oop_iterate_no_header(OopClosure* cl) {
  NoHeaderExtendedOopClosure no_header_cl(cl);
  oop_iterate(&no_header_cl);
}


inline HeapWord* CollectedHeap::align_allocation_or_fail(HeapWord* addr,
                                                         HeapWord* end,
                                                         unsigned short alignment_in_bytes) {
  if (alignment_in_bytes <= ObjectAlignmentInBytes) {
    return addr;
  }

  assert(is_ptr_aligned(addr, HeapWordSize),
    err_msg("Address " PTR_FORMAT " is not properly aligned.", p2i(addr)));
  assert(is_size_aligned(alignment_in_bytes, HeapWordSize),
    err_msg("Alignment size %u is incorrect.", alignment_in_bytes));

  HeapWord* new_addr = (HeapWord*) align_pointer_up(addr, alignment_in_bytes);
  size_t padding = pointer_delta(new_addr, addr);

  if (padding == 0) {
    return addr;
  }

  if (padding < CollectedHeap::min_fill_size()) {
    padding += alignment_in_bytes / HeapWordSize;
    assert(padding >= CollectedHeap::min_fill_size(),
      err_msg("alignment_in_bytes %u is expect to be larger "
      "than the minimum object size", alignment_in_bytes));
    new_addr = addr + padding;
  }

  assert(new_addr > addr, err_msg("Unexpected arithmetic overflow "
    PTR_FORMAT " not greater than " PTR_FORMAT, p2i(new_addr), p2i(addr)));
  if(new_addr < end) {
    CollectedHeap::fill_with_object(addr, padding);
    return new_addr;
  } else {
    return NULL;
  }
}

#ifndef PRODUCT

inline bool
CollectedHeap::promotion_should_fail(volatile size_t* count) {
  // Access to count is not atomic; the value does not have to be exact.
  if (PromotionFailureALot) {
    const size_t gc_num = total_collections();
    const size_t elapsed_gcs = gc_num - _promotion_failure_alot_gc_number;
    if (elapsed_gcs >= PromotionFailureALotInterval) {
      // Test for unsigned arithmetic wrap-around.
      if (++*count >= PromotionFailureALotCount) {
        *count = 0;
        return true;
      }
    }
  }
  return false;
}

inline bool CollectedHeap::promotion_should_fail() {
  return promotion_should_fail(&_promotion_failure_alot_count);
}

inline void CollectedHeap::reset_promotion_should_fail(volatile size_t* count) {
  if (PromotionFailureALot) {
    _promotion_failure_alot_gc_number = total_collections();
    *count = 0;
  }
}

inline void CollectedHeap::reset_promotion_should_fail() {
  reset_promotion_should_fail(&_promotion_failure_alot_count);
}
#endif  // #ifndef PRODUCT

#endif // SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_INLINE_HPP
