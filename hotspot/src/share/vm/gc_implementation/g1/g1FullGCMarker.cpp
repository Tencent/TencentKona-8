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
#include "gc_implementation/g1/g1FullGCMarker.inline.hpp"
#include "memory/referenceProcessor.hpp"
#include "memory/iterator.inline.hpp"

G1FullGCMarker::G1FullGCMarker(uint worker_id, PreservedMarks* preserved_stack, CMBitMap* bitmap) :
    _worker_id(worker_id),
    _mark_closure(worker_id, this, G1CollectedHeap::heap()->ref_processor_stw()),
    _verify_closure(VerifyOption_G1UseFullMarking),
    _cld_closure(mark_closure()),
    _stack_closure(this),
    _preserved_stack(preserved_stack),
    _bitmap(bitmap) {
  _oop_stack.initialize();
  _objarray_stack.initialize();
}

G1FullGCMarker::~G1FullGCMarker() {
  assert(is_empty(), "Must be empty at this point");
}

void G1FullGCMarker::complete_marking(OopQueueSet* oop_stacks,
                                      ObjArrayTaskQueueSet* array_stacks,
                                      ParallelTaskTerminator* terminator) {
  //int hash_seed = 17;
  do {
    drain_stack();
    ObjArrayTask steal_array;
    if (array_stacks->steal(_worker_id, /*&hash_seed,*/ steal_array)) {
      follow_array_chunk(objArrayOop(steal_array.obj()), steal_array.index());
    } else {
      oop steal_oop;
      if (oop_stacks->steal(_worker_id, /*&hash_seed,*/ steal_oop)) {
        follow_object(steal_oop);
      }
    }
  } while (!is_empty() || !terminator->offer_termination());
}

G1VerifyOopClosure::G1VerifyOopClosure(VerifyOption option) :
   _g1h(G1CollectedHeap::heap()),
   _containing_obj(NULL),
   _verify_option(option),
   _cc(0),
   _failures(false) {
}

void G1VerifyOopClosure::print_object(outputStream* out, oop obj) {
#ifdef PRODUCT
  Klass* k = obj->klass();
  const char* class_name = InstanceKlass::cast(k)->external_name();
  out->print_cr("class name %s", class_name);
#else // PRODUCT
  obj->print_on(out);
#endif // PRODUCT
}

template <class T>
void G1VerifyOopClosure::do_oop_work(T* p) {
  T heap_oop = oopDesc::load_heap_oop(p);
  
  if (!oopDesc::is_null(heap_oop)) {
    _cc++;
    oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
    bool failed = false;
    if (!_g1h->is_in_closed_subset(obj) || _g1h->is_obj_dead_cond(obj, _verify_option)) {
      MutexLockerEx x(ParGCRareEvent_lock,
          Mutex::_no_safepoint_check_flag);
      outputStream* yy = gclog_or_tty;
      if (!_failures) {
        yy->cr();
        yy->print_cr("----------");
      }
      if (!_g1h->is_in_closed_subset(obj)) {
        HeapRegion* from = _g1h->heap_region_containing((HeapWord*)p);
        yy->print_cr("Field " PTR_FORMAT
            " of live obj " PTR_FORMAT " in region "
            "[" PTR_FORMAT ", " PTR_FORMAT ")",
            p2i(p), p2i(_containing_obj),
            p2i(from->bottom()), p2i(from->end()));
        print_object(yy, _containing_obj);
        yy->print_cr("points to obj " PTR_FORMAT " not in the heap",
            p2i(obj));
      } else {
        HeapRegion* from = _g1h->heap_region_containing((HeapWord*)p);
        HeapRegion* to   = _g1h->heap_region_containing((HeapWord*)obj);
        yy->print_cr("Field " PTR_FORMAT
            " of live obj " PTR_FORMAT " in region "
            "[" PTR_FORMAT ", " PTR_FORMAT ")",
            p2i(p), p2i(_containing_obj),
            p2i(from->bottom()), p2i(from->end()));
        print_object(yy, _containing_obj);
        yy->print_cr("points to dead obj " PTR_FORMAT " in region "
            "[" PTR_FORMAT ", " PTR_FORMAT ")",
            p2i(obj), p2i(to->bottom()), p2i(to->end()));
        print_object(yy, obj);
      }
      yy->print_cr("----------");
      yy->flush();
      _failures = true;
      failed = true;
    }
  }
}
