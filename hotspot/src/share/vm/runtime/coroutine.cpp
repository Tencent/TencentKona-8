/*
 * Copyright 2001-2010 Sun Microsystems, Inc.  All Rights Reserved.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 */

#include "precompiled.hpp"
#if INCLUDE_KONA_FIBER
#include "runtime/coroutine.hpp"
#ifdef TARGET_ARCH_x86
# include "vmreg_x86.inline.hpp"
#endif
#include "services/threadService.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepThread.hpp"
#include "gc_implementation/g1/concurrentMarkThread.inline.hpp"
#include "gc_implementation/parallelScavenge/pcTasks.hpp"
#endif

JavaThread* Coroutine::_main_thread = NULL;
Method* Coroutine::_continuation_start = NULL;
ContBucket* ContContainer::_buckets= NULL;

Mutex* ContReservedStack::_lock = NULL;
GrowableArray<address>* ContReservedStack::free_array = NULL;
ContPreMappedStack* ContReservedStack::current_pre_mapped_stack = NULL;
uintx ContReservedStack::stack_size = 0;
int ContReservedStack::free_array_uncommit_index = 0;

void ContReservedStack::init() {
  _lock = new Mutex(Mutex::leaf, "InitializedStack", false);

  free_array = new (ResourceObj::C_HEAP, mtInternal)GrowableArray<address>(CONT_RESERVED_PHYSICAL_MEM_MAX, true);
  uint reserved_pages = StackShadowPages + StackRedPages + StackYellowPages;
  stack_size = DefaultCoroutineStackSize + (reserved_pages * os::vm_page_size());
}

bool ContReservedStack::add_pre_mapped_stack() {
  uintx alloc_real_stack_size = stack_size * CONT_PREMAPPED_STACK_NUM;
  uintx reserved_size = align_size_up(stack_size * CONT_PREMAPPED_STACK_NUM, os::vm_allocation_granularity());

  ContPreMappedStack* node = new ContPreMappedStack(reserved_size, current_pre_mapped_stack);
  if (node == NULL) {
    return false;
  }

  if (!node->initialize_virtual_space(alloc_real_stack_size)) {
    delete node;
    return false;
  }

  current_pre_mapped_stack = node;
  return true;
}

void ContReservedStack::insert_stack(address node) {
  MutexLockerEx ml(_lock, Mutex::_no_safepoint_check_flag);
  free_array->append(node);

  if (free_array->length() - free_array_uncommit_index > CONT_RESERVED_PHYSICAL_MEM_MAX) {
    address target = free_array->at(free_array_uncommit_index);
    os::free_heap_physical_memory((char *)(target - ContReservedStack::stack_size), ContReservedStack::stack_size);
    free_array_uncommit_index++;
  }
}

address ContReservedStack::get_stack_from_free_array() {
  MutexLockerEx ml(_lock, Mutex::_no_safepoint_check_flag);
  if (free_array->is_empty()) {
    return NULL;
  }

  address stack_base = free_array->pop();
  if (free_array->length() <= free_array_uncommit_index) {
    /* The node which is ahead of uncommit index has no physical memory */
    free_array_uncommit_index = free_array->length();
  }
  return stack_base;
}

bool ContReservedStack::pre_mapped_stack_is_full() {
  if (current_pre_mapped_stack->allocated_num >= CONT_PREMAPPED_STACK_NUM) {
    return true;
  }

  return false;
}

address ContReservedStack::acquire_stack() {
  address result = current_pre_mapped_stack->get_base_address() - current_pre_mapped_stack->allocated_num * stack_size;
  current_pre_mapped_stack->allocated_num++;

  return result;
}

address ContReservedStack::get_stack_from_pre_mapped() {
  address stack_base;
  {
    MutexLockerEx ml(_lock, Mutex::_no_safepoint_check_flag);
    if ((current_pre_mapped_stack == NULL) || pre_mapped_stack_is_full()) {
      if (!add_pre_mapped_stack()) {
        return NULL;
      }
    }

    stack_base = acquire_stack();
  }

  /* guard yellow page and red page of virtual space */
  if (os::uses_stack_guard_pages()) {
    address low_addr = stack_base - ContReservedStack::stack_size;
    size_t len = (StackYellowPages + StackRedPages) * os::vm_page_size();

    bool allocate = os::allocate_stack_guard_pages();

    if (!os::guard_memory((char *) low_addr, len)) {
      warning("Attempt to protect stack guard pages failed.");
      if (!os::uncommit_memory((char *) low_addr, len)) {
        warning("Attempt to deallocate stack guard pages failed.");
      }
    }
  }

  return stack_base;
}

address ContReservedStack::get_stack() {
  address stack_base = ContReservedStack::get_stack_from_free_array();
  if (stack_base != NULL) {
    return stack_base;
  }

  return ContReservedStack::get_stack_from_pre_mapped();
}

bool ContPreMappedStack::initialize_virtual_space(intptr_t real_stack_size) {
  if (!_virtual_space.initialize(_reserved_space, real_stack_size)) {
    _reserved_space.release();
    return false;
  }

  return true;
}

ContBucket::ContBucket() : _lock(Mutex::leaf, "ContBucket", false) {
  _head = NULL;
  _count = 0;

  // This initial value ==> never claimed.
  _oops_do_parity = 0;
}

void ContBucket::insert(Coroutine* cont) {
  cont->insert_into_list(_head);
  _count++;
}

void ContBucket::remove(Coroutine* cont) {
  cont->remove_from_list(_head);
  _count--;
  assert(_count >= 0, "illegal count");
}

// GC Support
bool ContBucket::claim_oops_do_par_case(int strong_roots_parity) {
  jint cont_bucket_parity = _oops_do_parity;
  if (cont_bucket_parity != strong_roots_parity) {
    jint res = Atomic::cmpxchg(strong_roots_parity, &_oops_do_parity, cont_bucket_parity);
    if (res == cont_bucket_parity) {
      return true;
    } else {
      guarantee(res == strong_roots_parity, "Or else what?");
    }
  }
  assert(SharedHeap::heap()->workers()->active_workers() > 0,
         "Should only fail when parallel.");
  return false;
}

#if INCLUDE_ALL_GCS
// Used by ParallelScavenge
void ContBucket::create_cont_bucket_roots_tasks(GCTaskQueue* q) {
  for (size_t i = 0; i < CONT_CONTAINER_SIZE; i++) {
    q->enqueue(new ContBucketRootsTask((int)i));
  }
}

// Used by Parallel Old
void ContBucket::create_cont_bucket_roots_marking_tasks(GCTaskQueue* q) {
  for (size_t i = 0; i < CONT_CONTAINER_SIZE; i++) {
    q->enqueue(new ContBucketRootsMarkingTask((int)i));
  }
}
#endif // INCLUDE_ALL_GCS

#define ALL_BUCKET_CONTS(OPR)      \
  {                                \
    Coroutine* head = _head;       \
    if (head != NULL) {            \
      Coroutine* current = head;   \
      do {                         \
        current->OPR;              \
        current = current->next(); \
      } while (current != head);   \
    }                              \
  }

void ContBucket::frames_do(void f(frame*, const RegisterMap*)) {
  ALL_BUCKET_CONTS(frames_do(f));
}

void ContBucket::oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf) {
  ALL_BUCKET_CONTS(oops_do(f, cld_f, cf));
}

void ContBucket::nmethods_do(CodeBlobClosure* cf) {
  ALL_BUCKET_CONTS(nmethods_do(cf));
}

void ContBucket::metadata_do(void f(Metadata*)) {
  ALL_BUCKET_CONTS(metadata_do(f));
}

void ContBucket::print_stack_on(outputStream* st) {
  ALL_BUCKET_CONTS(print_stack_on(st));
}

void ContContainer::init() {
  assert(is_power_of_2(CONT_CONTAINER_SIZE), "Must be a power of two");
  _buckets = new ContBucket[CONT_CONTAINER_SIZE];
}

ContBucket* ContContainer::bucket(size_t i) {
  return &(_buckets[i]);
}

size_t ContContainer::hash_code(Coroutine* cont) {
  return ((uintptr_t)cont >> CONT_MASK_SHIFT) & CONT_MASK;
}

void ContContainer::insert(Coroutine* cont) {
  size_t index = hash_code(cont);
  guarantee(index < CONT_CONTAINER_SIZE, "Must in the range from 0 to CONT_CONTAINER_SIZE - 1");
  {
    ContBucket* bucket = ContContainer::bucket(index);
    MutexLockerEx ml(bucket->lock(), Mutex::_no_safepoint_check_flag);
    bucket->insert(cont);
    if (TraceCoroutine) {
      ResourceMark rm;
      tty->print_cr("[insert] cont: %p, index: %d, count : %d", cont, (int)index, bucket->count());
    }
  }
}

void ContContainer::remove(Coroutine* cont) {
  size_t index = hash_code(cont);
  guarantee(index < CONT_CONTAINER_SIZE, "Must in the range from 0 to CONT_CONTAINER_SIZE - 1");
  {
    ContBucket* bucket = ContContainer::bucket(index);
    MutexLockerEx ml(bucket->lock(), Mutex::_no_safepoint_check_flag);
    bucket->remove(cont);
    if (TraceCoroutine) {
      ResourceMark rm;
      tty->print_cr("[remove] cont: %p, index: %d, count : %d", cont, (int)index, bucket->count());
    }
  }
}

#define ALL_BUCKETS_DO(OPR)                                     \
  {                                                             \
    for (size_t i = 0; i < CONT_CONTAINER_SIZE; i++) {          \
      ContBucket* bucket = ContContainer::bucket(i);            \
      MutexLockerEx ml(bucket->lock(), Mutex::_no_safepoint_check_flag); \
      bucket->OPR;                                              \
    }                                                           \
  }

void ContContainer::frames_do(void f(frame*, const RegisterMap*)) {
  ALL_BUCKETS_DO(frames_do(f));
}

void ContContainer::oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf) {
  ALL_BUCKETS_DO(oops_do(f, cld_f, cf));
}

void ContContainer::nmethods_do(CodeBlobClosure* cf) {
  ALL_BUCKETS_DO(nmethods_do(cf));
}

void ContContainer::metadata_do(void f(Metadata*)) {
  ALL_BUCKETS_DO(metadata_do(f));
}

void ContContainer::print_stack_on(outputStream* st) {
  ALL_BUCKETS_DO(print_stack_on(st));
}

// count is same with real count in list
// all elements in a bucket has correct hash code
void ContContainer::verify() {
  for (size_t i = 0; i < CONT_CONTAINER_SIZE; i++) {
    ContBucket* bucket = ContContainer::bucket(i);
    MutexLockerEx ml(bucket->lock(), Mutex::_no_safepoint_check_flag);
    Coroutine* head = bucket->head();
    if (head == NULL) {
      guarantee(bucket->count() == 0, "mismatch count");
      continue;
    }

    Coroutine* current = head;
    int num = 0;
    do {
      guarantee(hash_code(current) == i, "invalid hash location");
      num++;
      current = current->next();
    } while (current != head);
    guarantee(num == bucket->count(), "mismatch count");
  }
}

void Coroutine::add_stack_frame(void* frames, int* depth, javaVFrame* jvf) {
  StackFrameInfo* frame = new StackFrameInfo(jvf, false);
  ((GrowableArray<StackFrameInfo*>*)frames)->append(frame);
  (*depth)++;
}

#if defined(LINUX) || defined(_ALLBSD_SOURCE) || defined(_WINDOWS)
void coroutine_start(void* dummy, const void* coroutineObjAddr) {
#ifndef AMD64
  fatal("Corotuine not supported on current platform");
#endif
  JavaThread* thread = JavaThread::current();
  thread->set_thread_state(_thread_in_vm);
  // passing raw object address form stub to C method
  // normally oop is OopDesc*, can use raw object directly
  // in fastdebug mode, oop is "class oop", raw object addrss is stored in class oop structure
#ifdef CHECK_UNHANDLED_OOPS
  oop coroutineObj = oop(coroutineObjAddr);
#else
  oop coroutineObj = (oop)coroutineObjAddr;
#endif
  JavaCalls::call_continuation_start(coroutineObj, thread);
  ShouldNotReachHere();
}
#endif

void Coroutine::TerminateCoroutine(Coroutine* coro) {
  JavaThread* thread = coro->thread();
  if (TraceCoroutine) {
    ResourceMark rm;
    tty->print_cr("[Co]: TerminateCoroutine %p in thread %s(%p)", coro, coro->thread()->name(), coro->thread());
  }
  guarantee(thread == JavaThread::current(), "thread not match");

  {
    ContContainer::remove(coro);
    if (thread->coroutine_cache_size() < MaxFreeCoroutinesCacheSize) {
      coro->insert_into_list(thread->coroutine_cache());
      thread->coroutine_cache_size() ++;
    } else {
      delete coro;
    }
  }
}

void Coroutine::TerminateCoroutineObj(jobject coroutine) {
  oop old_oop = JNIHandles::resolve(coroutine);
  Coroutine* coro = (Coroutine*)java_lang_Continuation::data(old_oop);
  assert(coro != NULL, "NULL old coroutine in switchToAndTerminate");
  java_lang_Continuation::set_data(old_oop, 0);
  coro->_continuation = NULL;
  TerminateCoroutine(coro);
}

void Coroutine::Initialize() {
  guarantee(_continuation_start == NULL, "continuation start already initialized");
  KlassHandle klass = KlassHandle(SystemDictionary::continuation_klass());
  Symbol* method_name = vmSymbols::cont_start_method_name();
  Symbol* signature = vmSymbols::void_method_signature();
  methodHandle method = LinkResolver::linktime_resolve_virtual_method_or_null(
    klass, method_name, signature, klass, true);
  _continuation_start = method();
  guarantee(_continuation_start != NULL, "continuation start not resolveds");
}

void Coroutine::cont_metadata_do(void f(Metadata*)) {
  if (_continuation_start != NULL) {
    f(_continuation_start);
  }
}

Coroutine::Coroutine() {
  _has_javacall = false;
  _continuation = NULL;
}

Coroutine::~Coroutine() {
  if (_verify_state != NULL) {
    delete _verify_state; 
  } else {
    assert(VerifyCoroutineStateOnYield == false || _is_thread_coroutine,
      "VerifyCoroutineStateOnYield is on and _verify_state is NULL");
  }
  free_stack();
}

// check yield is from thread contianuation or to thread contianuation
// check resource is not still hold by contianuation when yield back to thread contianuation
void Coroutine::yield_verify(Coroutine* from, Coroutine* to, bool terminate) {
  if (TraceCoroutine) {
    tty->print_cr("yield_verify from %p to %p", from, to);
  }
  // check before change, check is need when yield from none thread to others
  if (!from->is_thread_coroutine()) {
    // switch to other corotuine, compare status
    JavaThread* thread = from->_thread;
    JNIHandleBlock* jni_handle_block = thread->active_handles();
    guarantee(from->_verify_state->saved_active_handles == jni_handle_block, "must same handle");
    guarantee(from->_verify_state->saved_active_handle_count == jni_handle_block->get_number_of_live_handles(), "must same count");
    guarantee(thread->monitor_chunks() == NULL, "not empty _monitor_chunks");
    if (terminate) {
      assert(thread->java_call_counter() == 1, "must be 1 when terminate");
    }
    if (from->_verify_state->saved_handle_area_hwm != thread->handle_area()->hwm()) {
      tty->print_cr("%p failed %p, %p", from, from->_verify_state->saved_handle_area_hwm, thread->handle_area()->hwm());
      guarantee(false, "handle area leak");
    }
    if (from->_verify_state->saved_resource_area_hwm != thread->resource_area()->hwm()) {
      tty->print_cr("%p failed %p, %p", from, from->_verify_state->saved_resource_area_hwm, thread->resource_area()->hwm());
      guarantee(false, "resource area leak");
    }
  }
  // save context if yield to none thread coroutine
  if (!to->is_thread_coroutine()) {
    // save snapshot
    guarantee(terminate == false, "switch from kernel to continnuation");
    JavaThread* thread = from->_thread;
    JNIHandleBlock* jni_handle_block = thread->active_handles();
    to->_verify_state->saved_active_handles = jni_handle_block;
    to->_verify_state->saved_active_handle_count = jni_handle_block->get_number_of_live_handles();
    to->_verify_state->saved_resource_area_hwm = thread->resource_area()->hwm();
    to->_verify_state->saved_handle_area_hwm = thread->handle_area()->hwm();
  }
}

void Coroutine::print_stack_on(void* frames, int* depth) {
  if (!has_javacall() || state() != Coroutine::_onstack) {
    return;
  }
  print_stack_on(NULL, frames, depth);
}

void Coroutine::print_stack_on(outputStream* st) {
  if (!has_javacall()) {
    return;
  }
  if (state() == Coroutine::_onstack) {
    st->cr();
    st->print("   Coroutine: %p", this);
    if (is_thread_coroutine()) {
      st->print_cr("  [thread coroutine]");
    } else {
      print_VT_info(st);
      st->cr();
    }
    print_stack_on(st, NULL, NULL);
  }
}

Coroutine* Coroutine::create_thread_coroutine(JavaThread* thread) {
  Coroutine* coro = new Coroutine();
  coro->_state = _current;
  coro->_verify_state = NULL;
  coro->_is_thread_coroutine = true;
  coro->_thread = thread;
  coro->init_thread_stack(thread);
  coro->_has_javacall = true;
#ifdef ASSERT
  coro->_java_call_counter = 0;
#endif
#if defined(_WINDOWS)
  coro->_last_SEH = NULL;
#endif
  ContContainer::insert(coro);
  return coro;
}

void Coroutine::reset_coroutine(Coroutine* coro) {
  coro->_has_javacall = false;
}

void Coroutine::init_coroutine(Coroutine* coro, JavaThread* thread) {
  intptr_t** d = (intptr_t**)coro->_stack_base;
  *(--d) = NULL;
  *(--d) = NULL;
  *(--d) = NULL;
  *(--d) = (intptr_t*)coroutine_start;
  *(--d) = NULL;

  coro->set_last_sp((address) d);

  coro->_state = _onstack;
  coro->_is_thread_coroutine = false;
  coro->_thread = thread;

#ifdef ASSERT
  coro->_java_call_counter = 0;
#endif
#if defined(_WINDOWS)
  coro->_last_SEH = NULL;
#endif
  if (TraceCoroutine) {
    ResourceMark rm;
    tty->print_cr("[Co]: CreateCoroutine %p in thread %s(%p)", coro, coro->thread()->name(), coro->thread());
  }
}

Coroutine* Coroutine::create_coroutine(JavaThread* thread, long stack_size, oop coroutineObj) {
  assert(stack_size <= 0, "Can not specify stack size by users");

  Coroutine* coro = new Coroutine();
  if (VerifyCoroutineStateOnYield) {
    coro->_verify_state = new CoroutineVerify();
  } else {
    coro->_verify_state = NULL;
  }
  if (!coro->init_stack(thread)) {
    return NULL;
  }

  Coroutine::init_coroutine(coro, thread);
  return coro;
}

void Coroutine::frames_do(FrameClosure* fc) {
  if (_state == Coroutine::_onstack) {
    on_stack_frames_do(fc, _is_thread_coroutine);
  }
}

class oops_do_Closure: public FrameClosure {
private:
  OopClosure* _f;
  CodeBlobClosure* _cf;
  CLDClosure* _cld_f;

public:
  oops_do_Closure(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf): _f(f), _cld_f(cld_f), _cf(cf) { }
  void frames_do(frame* fr, RegisterMap* map) { fr->oops_do(_f, _cld_f, _cf, map); }
};

void Coroutine::oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf) {
  f->do_oop(&_continuation);
  if(state() != Coroutine::_onstack) {
    return;
  }
  oops_do_Closure fc(f, cld_f, cf);
  frames_do(&fc);
}

class nmethods_do_Closure: public FrameClosure {
private:
  CodeBlobClosure* _cf;
public:
  nmethods_do_Closure(CodeBlobClosure* cf): _cf(cf) { }
  void frames_do(frame* fr, RegisterMap* map) { fr->nmethods_do(_cf); }
};

void Coroutine::nmethods_do(CodeBlobClosure* cf) {
  nmethods_do_Closure fc(cf);
  frames_do(&fc);
}

class metadata_do_Closure: public FrameClosure {
private:
  void (*_f)(Metadata*);
public:
  metadata_do_Closure(void f(Metadata*)): _f(f) { }
  void frames_do(frame* fr, RegisterMap* map) { fr->metadata_do(_f); }
};

void Coroutine::metadata_do(void f(Metadata*)) {
	if(state() != Coroutine::_onstack) {
		return;
	}
  metadata_do_Closure fc(f);
  frames_do(&fc);
}

class frames_do_Closure: public FrameClosure {
private:
  void (*_f)(frame*, const RegisterMap*);
public:
  frames_do_Closure(void f(frame*, const RegisterMap*)): _f(f) { }
  void frames_do(frame* fr, RegisterMap* map) { _f(fr, map); }
};

void Coroutine::frames_do(void f(frame*, const RegisterMap* map)) {
  frames_do_Closure fc(f);
  frames_do(&fc);
}

bool Coroutine::is_disposable() {
  return false;
}


void Coroutine::init_thread_stack(JavaThread* thread) {
  _stack_base = thread->stack_base();
  _stack_size = thread->stack_size();
  _last_sp = NULL;
}

bool Coroutine::init_stack(JavaThread* thread) {
  _stack_base = ContReservedStack::get_stack();
  if (_stack_base == NULL) {
    return false;
  }
  _stack_size = ContReservedStack::stack_size;
  _last_sp = NULL;

  DEBUG_CORO_ONLY(tty->print("created coroutine stack at %08x with real size: %i\n", _stack_base, _stack_size));
  return true;
}

void Coroutine::free_stack() {
  if(!is_thread_coroutine()) {
    ContReservedStack::insert_stack(_stack_base);
  }
}

static const char* VirtualThreadStateNames[] = {
  "NEW",
  "STARTED",
  "RUNNABLE",
  "RUNNING",
  "PARKING",
  "PARKED",
  "PINNED"
};

static const char* virtual_thread_get_state_name(int state) {
  if (state >= 0 && state < (int)(sizeof(VirtualThreadStateNames) / sizeof(const char*))) {
    return VirtualThreadStateNames[state];
  } else if (state == 99) {
    return "TERMINATED";
  } else {
    return "ERROR STATE";
  }
}
// dump VirtualThread info if possible
// 1. check if continuation is java/lang/VirtualThread$VTContinuation
// 2. Get VT from continuation
// 3. Print VT name and state
void Coroutine::print_VT_info(outputStream* st) {
  if (_continuation == NULL) {
    guarantee(is_thread_coroutine(), "null continuation oop when print");
    return;
  }
  Klass* k = _continuation->klass();
  if (k != SystemDictionary::VTcontinuation_klass()) {
    return;
  }
  oop vt = java_lang_VTContinuation::VT(_continuation);
  guarantee(vt != NULL, "on stack VT is null");
  oop vt_name = java_lang_Thread::name(vt);
  int state = java_lang_VT::state(vt);
  if (vt_name == NULL) {
    st->print("\tVirtualThread => name: null, state %s",
      virtual_thread_get_state_name(state));
  } else {
    ResourceMark rm;
    st->print("\tVirtualThread => name: %s, state %s",
      java_lang_String::as_utf8_string(vt_name),
      virtual_thread_get_state_name(state));
  }
}

void Coroutine::print_stack_on(outputStream* st, void* frames, int* depth) {
  if (_last_sp == NULL) return;
  address pc = ((address*)_last_sp)[1];
  if (pc != (address)coroutine_start) {
    intptr_t * fp = ((intptr_t**)_last_sp)[0];
    intptr_t* sp = ((intptr_t*)_last_sp) + 2;

    RegisterMap _reg_map(_thread, true);
    _reg_map.set_location(rbp->as_VMReg(), (address)_last_sp);
    _reg_map.set_include_argument_oops(false);
    frame f(sp, fp, pc);
    vframe* start_vf = NULL;
    for (vframe* vf = vframe::new_vframe(&f, &_reg_map, _thread); vf; vf = vf->sender()) {
      if (vf->is_java_frame()) {
        start_vf = javaVFrame::cast(vf);
        break;
      }
    }
    int count = 0;
    for (vframe* f = start_vf; f; f = f->sender()) {
      if (f->is_java_frame()) {
        javaVFrame* jvf = javaVFrame::cast(f);
        if (st != NULL) {
          java_lang_Throwable::print_stack_element(st, jvf->method(), jvf->bci());

          // Print out lock information
          // We only print stack info of coroutines which status is _on_stack.
          // If coroutine has lock, it can't yield from current thread,
          // its status must be _current.
          // So there is no lock info to print.
        } else {
          add_stack_frame(frames, depth, jvf);
        }
      }
      else {
        // Ignore non-Java frames
      }
      // Bail-out case for too deep stacks
      count++;
      if (MaxJavaStackTraceDepth == count) return;
    }
  }
}

void Coroutine::on_stack_frames_do(FrameClosure* fc, bool isThreadCoroutine) {
  assert(_last_sp != NULL, "CoroutineStack with NULL last_sp");

  DEBUG_CORO_ONLY(tty->print_cr("frames_do stack "INTPTR_FORMAT, _stack_base));
  // optimization to skip coroutine not started yet, check if return address is coroutine_start
  // fp is only valid for call from interperter, from compiled code fp register is not gurantee valid
  // JIT method utilize sp and oop map for oops iteration.
  address pc = ((address*)_last_sp)[1];
  intptr_t* fp = ((intptr_t**)_last_sp)[0];
  if (pc != (address)coroutine_start) {
    intptr_t* sp = ((intptr_t*)_last_sp) + 2;
    frame fr(sp, fp, pc);
    StackFrameStream fst(_thread, fr);
    fst.register_map()->set_location(rbp->as_VMReg(), (address)_last_sp);
    fst.register_map()->set_include_argument_oops(false);
    for(; !fst.is_done(); fst.next()) {
      fc->frames_do(fst.current(), fst.register_map());
    }
  } else {
    DEBUG_CORO_ONLY(tty->print_cr("corountine not started "INTPTR_FORMAT, _stack_base));
    guarantee(!isThreadCoroutine, "thread conrotuine with coroutine_start as return address");
    guarantee(fp == NULL, "conrotuine fp not in init status"); 
  }
}

JVM_ENTRY(jint, CONT_isPinned0(JNIEnv* env, jclass klass, long data)) {
  JavaThread* thread = JavaThread::thread_from_jni_environment(env);
  if (thread->locksAcquired != 0) {
    return CONT_PIN_MONITOR;
  }
  if (thread->contJniFrames != 0) {
    return CONT_PIN_JNI;
  }
  return 0;
}
JVM_END

JVM_ENTRY(jlong, CONT_createContinuation(JNIEnv* env, jclass klass, jobject cont, long stackSize)) {
  DEBUG_CORO_PRINT("CONT_createContinuation\n");
  assert(cont != NULL, "cannot create coroutine with NULL Coroutine object");

  if (stackSize < 0) {
    guarantee(thread->current_coroutine()->is_thread_coroutine(), "current coroutine is not thread coroutine");
    if (TraceCoroutine) {
      tty->print_cr("CONT_createContinuation: reuse main thread continuation %p", thread->current_coroutine());
    }
    return (jlong)thread->current_coroutine();
  }

  // illegal arguments is checked in library side
  // 0 means default stack size
  // -1 means no stack, this is continuation for kernel thread
  // Stack is cached in thread local now, later will be cached in bucket sized list
  // now cache is not cosiderering different size, for example:
  // 1. DefaultCoroutineStackSize is 256K
  // 2. if user allocate stack as 8k and freed, 8K stack will be in cache too and reused as default size
  // This will be solved later with global coroutine cache
  Coroutine* coro = NULL;
  if (stackSize == 0) {
    if (thread->coroutine_cache_size() > 0) {
      coro = thread->coroutine_cache();
      coro->remove_from_list(thread->coroutine_cache());
      thread->coroutine_cache_size()--;
      Coroutine::reset_coroutine(coro);
      Coroutine::init_coroutine(coro, thread);
      DEBUG_CORO_ONLY(tty->print("reused coroutine stack at %08x\n", _stack_base));
    }
  }
  if (coro == NULL) {
    coro = Coroutine::create_coroutine(thread, stackSize, JNIHandles::resolve(cont));
    if (coro == NULL) {
      ThreadInVMfromNative tivm(thread);
      HandleMark mark(thread);
      THROW_0(vmSymbols::java_lang_OutOfMemoryError());
    }
  }
  ContContainer::insert(coro);
  if (TraceCoroutine) {
    tty->print_cr("CONT_createContinuation: create continuation %p", coro);
  }
  return (jlong)coro;
}
JVM_END

JVM_ENTRY(jint, CONT_switchTo(JNIEnv* env, jclass klass, jobject target, jobject current)) {
  ShouldNotReachHere();
  return 0;
}
JVM_END

JVM_ENTRY(void, CONT_switchToAndTerminate(JNIEnv* env, jclass klass, jobject target, jobject current)) {
  Coroutine::TerminateCoroutineObj(current);
}
JVM_END

JVM_ENTRY(jobjectArray, CONT_dumpStackTrace(JNIEnv *env, jclass klass, jobject cont))
  oop contOop = JNIHandles::resolve(cont);
  Coroutine* coro = (Coroutine*)java_lang_Continuation::data(contOop);
  VirtualThreadStackTrace* res = new VirtualThreadStackTrace(coro);
  // if coro is NULL, array is empty
  res->dump_stack();

  Handle stacktraces = res->allocate_fill_stack_trace_element_array(thread);
  return (jobjectArray)JNIHandles::make_local(env, stacktraces());
JVM_END

#define CC (char*)  /*cast a literal from (const char*)*/
#define FN_PTR(f) CAST_FROM_FN_PTR(void*, &f)
#define JLSTR  "Ljava/lang/String;"
#define JLSTE  "Ljava/lang/StackTraceElement;"
#define JLCONT "Ljava/lang/Continuation;"

static JNINativeMethod CONT_methods[] = {
  {CC"isPinned0",                 CC"(J)I", FN_PTR(CONT_isPinned0)},
  {CC"createContinuation",        CC"("JLCONT "J)J", FN_PTR(CONT_createContinuation)},
  {CC"switchTo",                  CC"("JLCONT JLCONT")I", FN_PTR(CONT_switchTo)},
  {CC"switchToAndTerminate",      CC"("JLCONT JLCONT")V", FN_PTR(CONT_switchToAndTerminate)},
  {CC"dumpStackTrace",            CC"("JLCONT ")[" JLSTE, FN_PTR(CONT_dumpStackTrace)},
};

static const int switchToIndex = 2;
static const int switchToAndTerminateIndex = 3;

static void initializeForceWrapper(JNIEnv *env, jclass cls, JavaThread* thread, int index) {
  jmethodID id = env->GetStaticMethodID(cls, CONT_methods[index].name, CONT_methods[index].signature);
  {
    ThreadInVMfromNative tivfn(thread);
    methodHandle method(Method::resolve_jmethod_id(id));
    AdapterHandlerLibrary::create_native_wrapper(method);
    // switch method doesn't have real implemenation, when JVMTI is on and JavaThread::interp_only_mode is true
    // it crashes when execute registered native method, as empty or incorrect.
    // set i2i as point to i2c entry, force it execute native wrapper
    method->set_interpreter_entry(method->from_interpreted_entry());
  }
}

void CONT_RegisterNativeMethods(JNIEnv *env, jclass cls, JavaThread* thread) {
    if (UseKonaFiber == false) {
      fatal("UseKonaFiber is off");
    }
    assert(thread->is_Java_thread(), "");
    ThreadToNativeFromVM trans((JavaThread*)thread);
    int status = env->RegisterNatives(cls, CONT_methods, sizeof(CONT_methods)/sizeof(JNINativeMethod));
    guarantee(status == JNI_OK && !env->ExceptionOccurred(), "register java.lang.Continuation natives");
#ifdef ASSERT
    if (FLAG_IS_DEFAULT(VerifyCoroutineStateOnYield)) {
      FLAG_SET_DEFAULT(VerifyCoroutineStateOnYield, true);
    }
#endif
    initializeForceWrapper(env, cls, thread, switchToIndex);
    initializeForceWrapper(env, cls, thread, switchToAndTerminateIndex);
    Coroutine::Initialize();
}
#endif// INCLUDE_KONA_FIBER
