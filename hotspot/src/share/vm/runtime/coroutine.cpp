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
#include "runtime/coroutine.hpp"
#ifdef TARGET_ARCH_x86
# include "vmreg_x86.inline.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "vmreg_sparc.inline.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "vmreg_zero.inline.hpp"
#endif

#include "services/threadService.hpp"

JavaThread* Coroutine::_main_thread = NULL;

void CoroutineStack::add_stack_frame(void* frames, int* depth, javaVFrame* jvf) {
  StackFrameInfo* frame = new StackFrameInfo(jvf, false);
  ((GrowableArray<StackFrameInfo*>*)frames)->append(frame);
  (*depth)++;
}

#ifdef _WINDOWS

LONG WINAPI topLevelExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo);


void coroutine_start(Coroutine* coroutine, jobject contObj) {
  coroutine->thread()->set_thread_state(_thread_in_vm);
  coroutine->run(contObj);
  ShouldNotReachHere();
}
#endif

#if defined(LINUX) || defined(_ALLBSD_SOURCE)

void coroutine_start(Coroutine* coroutine, jobject coroutineObj) {
  coroutine->thread()->set_thread_state(_thread_in_vm);

  coroutine->run(coroutineObj);
  ShouldNotReachHere();
}
#endif
void Coroutine::TerminateCoroutine(Coroutine* coro) {
	//cannot reclaim here,must recaim before swithto
  //Coroutine::ReclaimJavaCallStack(coro);
  JavaThread* thread = coro->thread();
  if (TraceCoroutine) {
    ResourceMark rm;
    tty->print_cr("[Co]: TerminateCoroutine %s(%p) in thread %s(%p)", coro->name(), coro, coro->thread()->name(), coro->thread());
  }
  guarantee(thread == JavaThread::current(), "thread not match");

  {
    MutexLockerEx ml(thread->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
    CoroutineStack* stack = coro->stack();
    stack->remove_from_list(thread->coroutine_stack_list());
    if (thread->coroutine_stack_cache_size() < MaxFreeCoroutinesCacheSize) {
      stack->insert_into_list(thread->coroutine_stack_cache());
      thread->coroutine_stack_cache_size() ++;
    } else {
      CoroutineStack::free_stack(stack, thread);
    }
    coro->remove_from_list(thread->coroutine_list());
  }
  delete coro;
}

void Coroutine::TerminateCoroutineObj(jobject coroutine)
{
  
  oop old_oop = JNIHandles::resolve(coroutine);
  Coroutine* coro = (Coroutine*)java_lang_Continuation::data(old_oop);
  assert(coro != NULL, "NULL old coroutine in switchToAndTerminate");
  java_lang_Continuation::set_data(old_oop, 0);
  TerminateCoroutine(coro);
}

void Coroutine::run(jobject coroutine) {

  // do not call JavaThread::current() here!

  //_thread->set_resource_area(new (mtThread) ResourceArea(32));
  //_thread->set_handle_area(new (mtThread) HandleArea(NULL, 32));
  //_thread->set_metadata_handles(new (ResourceObj::C_HEAP, mtClass) GrowableArray<Metadata*>(30, true));
  // used to test validitity of stack trace backs
//  this->record_base_of_stack_pointer();

  // Record real stack base and size.
//  this->record_stack_base_and_size();

  // Initialize thread local storage; set before calling MutexLocker
//  this->initialize_thread_local_storage();

//  this->create_stack_guard_pages();

  // Thread is now sufficient initialized to be handled by the safepoint code as being
  // in the VM. Change thread state from _thread_new to _thread_in_vm
//  ThreadStateTransition::transition_and_fence(this, _thread_new, _thread_in_vm);

  // This operation might block. We call that after all safepoint checks for a new thread has
  // been completed.
//  this->set_active_handles(JNIHandleBlock::allocate_block());

  // We call another function to do the rest so we are sure that the stack addresses used
  // from there will be lower than the stack base just computed
  {
    HandleMark hm(_thread);
    HandleMark hm2(_thread);
	_hm = &hm;
	_hm2 = &hm2;
    Handle obj(_thread, JNIHandles::resolve(coroutine));
    JNIHandles::destroy_global(coroutine);
    JavaValue result(T_VOID);
	set_has_javacall(true);
    JavaCalls::call_virtual(&result,
                            obj,
                            KlassHandle(_thread, SystemDictionary::continuation_klass()),
                            vmSymbols::cont_start_method_name(),
                            vmSymbols::void_method_signature(),
                            _thread);
  }
}

Coroutine::Coroutine() 
{
	_resource_area = NULL;
	_handle_area = NULL;
	_metadata_handles = NULL;
	_JavaCallWrapper = NULL;
	_CallInfo = NULL;
	_last_handle_mark = NULL;
	_has_javacall = false;
	_active_handles = NULL;
  _monitor_chunks = NULL;
  _jni_frames = 0;
}

Coroutine::~Coroutine() {
  if(!is_thread_coroutine()) {
    delete resource_area();
    delete handle_area();
    delete metadata_handles();
    JNIHandleBlock* JNIHandles = active_handles();
    guarantee(JNIHandles != NULL, "stop with null active handles");
    set_active_handles(NULL);
    JNIHandleBlock::release_block(JNIHandles);
    assert(_monitor_chunks == NULL, "not empty _monitor_chunks");
  }
}

/*
 * Norma exitflow is in startInternal, it invokes switchToAndTerminate(current, target)
 * This method will switch from current to target and release all resources in current coroutine
 * 1. In switchToAndTerminate, before switch context invoke Coroutine::ReclaimJavaCallStack
 * 2. In switchToAndTerminate, after switch context, invoke CoroutineSupport_switchToAndTerminate
 *    2.1 release stack
 *    2.2 release coroutine
 *
 * This method is to release stack object allcoated in first Java call to CoroutineBase.startInternal
 * Because JavaCallWrapper\CallInfo\HandleMark is allocated on coroutine's own stack, they will be
 * released automatically in 2.1.
 *
 * HandleMark release is to maintain Coroutine::_handle_area, actually no need release.
 */
void Coroutine::ReclaimJavaCallStack(Coroutine* coro) {
	if(!coro->_is_thread_coroutine && coro->_has_javacall) {
    guarantee(coro->_JavaCallWrapper != NULL, "Recaim live stack without call wrapper");
    guarantee(coro == JavaThread::current()->current_coroutine(), "Not expect coroutine");
    coro->_JavaCallWrapper->ClearForCoro();//reclaim jnihandle

    // remove method handle from _thread->metadata_handles()
    // _thread->metadata_handles() will be released in Coroutine's destructor
    guarantee(coro->_CallInfo != NULL, "Recaim live stack without call info");
    coro->_CallInfo->~CallInfo(); //reclaim metahandle

    // handle mark can be discarded without destructor
    guarantee(coro->_hm2 != NULL, "Recaim live stack without hm2");
    coro->_hm2->~HandleMark();
    guarantee(coro->_hm != NULL, "Recaim live stack without hm");
    coro->_hm->~HandleMark();
  }
}

void Coroutine::SetJavaCallWrapper(JavaThread* thread, JavaCallWrapper* jcw) {
  Coroutine* co = thread->current_coroutine();
  if(co && co->_JavaCallWrapper == NULL)	{
    guarantee(jcw != NULL, "NULL JavaCallWrapper");
    co->_JavaCallWrapper = jcw;
  }
}

static inline CoroutineStack* extract_from(Coroutine* coro, JavaThread* from) {
  CoroutineStack* stack_ptr = coro->stack();
  guarantee(stack_ptr != NULL, "stack is NULL");
  MutexLockerEx ml(from->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
  stack_ptr->remove_from_list(from->coroutine_stack_list());
  coro->remove_from_list(from->coroutine_list());
  return stack_ptr;
}

static inline void insert_into(Coroutine* coro, JavaThread* to, CoroutineStack* stack) {
  MutexLockerEx ml(to->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
  stack->insert_into_list(to->coroutine_stack_list());
  coro->insert_into_list(to->coroutine_list());
}

/*
 * switch from coroutine running _thread to target_thread
 * extract coroutine/stack from _thread, insert into target thread
 */
void Coroutine::switchTo_current_thread(Coroutine* coro) {
  guarantee(coro->is_continuation() == true, "Only Continuation allowed");
  JavaThread* target_thread = JavaThread::current();
  JavaThread* old_thread = coro->thread();
  if (old_thread == target_thread) {
    return;
  }
  CoroutineStack* stack_ptr = extract_from(coro, old_thread);
  insert_into(coro, target_thread, stack_ptr);
  coro->set_thread(target_thread);
  if (TraceCoroutine) {
    ResourceMark rm;
    // can not get old thread name
    tty->print_cr("[Co]: SwitchToCurrent Coroutine %s(%p) from (%p) to %s(%p)",
      coro->name(), coro, old_thread, target_thread->name(), target_thread);
  }
}

void Coroutine::switchFrom_current_thread(Coroutine* coro, JavaThread* to) {
  JavaThread* from = JavaThread::current();
  guarantee(from == coro->thread(), "not from current thread");
  if (from == to) {
    return;
  }
  CoroutineStack* stack_ptr = extract_from(coro, from);
  insert_into(coro, to, stack_ptr);
  coro->set_thread(to);
  if (TraceCoroutine) {
    ResourceMark rm;
    // can not get to thread name
    tty->print_cr("[Co]: SwitchFromCurrent Coroutine %s(%p) from %s(%p) to (%p)",
      coro->name(), coro, from->name(), from, to);
  }
}

const char* Coroutine::get_coroutine_name() const
{
	return name();
}
const char* Coroutine::get_coroutine_state_name(CoroutineState state)
{
	switch (state)
	{
	case Coroutine::_onstack:
		return "onstack";
		break;
	case Coroutine::_current:
		return "current";
		break;
	case Coroutine::_dead:
		return "dead";
		break;
	case Coroutine::_dummy:
		return "dummy";
		break;
	default:
		return "default";
		break;
	}
}
void Coroutine::print_on(outputStream* st) const
{
	st->print("\"%s\" ", get_coroutine_name());
	st->print("#" INT64_FORMAT " ", (long int)(this));
	if (!has_javacall())
	{
		st->print(" no_java_call");
	}
	// print guess for valid stack memory region (assume 4K pages); helps lock debugging
	st->print_cr("[" INTPTR_FORMAT "]", (intptr_t)stack()->last_sp() & ~right_n_bits(12));
	st->print_cr("   java.lang.Thread.State: %s", get_coroutine_state_name(this->state()));
}

void Coroutine::print_stack_on(void* frames, int* depth)
{
  if (!has_javacall() || state() != Coroutine::_onstack) return;
  stack()->print_stack_on(frames, depth);
}

void Coroutine::print_stack_on(outputStream* st)
{
	if (!has_javacall()) return;
	if( state() == Coroutine::_onstack)
		stack()->print_stack_on(st);
}

bool Coroutine::is_lock_owned(address adr) const {
  if (stack()->on_local_stack(adr)) {
    return true;
  }
  for (MonitorChunk* chunk = monitor_chunks(); chunk != NULL; chunk = chunk->next()) {
    if (chunk->contains(adr)) {
      return true;
    }
  }
  return false;
}

void Coroutine::SetCallInfo(Thread* thread,CallInfo* ci)
{
	if(!thread->is_Java_thread()) return;
	JavaThread* jt = (JavaThread*)thread;
  Coroutine* cur = jt->current_coroutine();
	if(cur != NULL && cur->_CallInfo == NULL)	{
		cur->_CallInfo = ci;
	}
}

void Coroutine::add_monitor_chunk(MonitorChunk* chunk) {
  chunk->set_next(monitor_chunks());
  set_monitor_chunks(chunk);
}

void Coroutine::remove_monitor_chunk(MonitorChunk* chunk) {
  guarantee(monitor_chunks() != NULL, "must be non empty");
  if (monitor_chunks() == chunk) {
    set_monitor_chunks(chunk->next());
  } else {
    MonitorChunk* prev = monitor_chunks();
    while (prev->next() != chunk) prev = prev->next();
    prev->set_next(chunk->next()); // will crash if not found
  }
}

Coroutine* Coroutine::create_thread_coroutine(const char* name,JavaThread* thread, CoroutineStack* stack) {
  Coroutine* coro = new Coroutine();
  if (coro == NULL)
    return NULL;
  if (_main_thread == NULL) {
    _main_thread = thread;
  }
  coro->set_name(name);
  coro->_state = _current;
  coro->_is_thread_coroutine = true;
  coro->_is_continuation = false;
  coro->_thread = thread;
  coro->_stack = stack;
  coro->set_resource_area(thread->resource_area());
  coro->set_handle_area(thread->handle_area());
  coro->set_metadata_handles(thread->metadata_handles());
  coro->set_last_handle_mark(thread->last_handle_mark());
  coro->_has_javacall = true;
#ifdef ASSERT
  coro->_java_call_counter = 0;
#endif
#if defined(_WINDOWS)
  coro->_last_SEH = NULL;
#endif
  return coro;
}

Coroutine* Coroutine::create_coroutine(const char* name,JavaThread* thread, CoroutineStack* stack, oop coroutineObj) {
  Coroutine* coro = new Coroutine();
  if (coro == NULL) {
    return NULL;
  }
  Klass* klass = coroutineObj->klass();
  Klass* continuation_klass = SystemDictionary::continuation_klass();
  coro->_is_continuation = false;
  while (klass != NULL) {
    if (klass == continuation_klass) {
      coro->_is_continuation = true;
      break;
    }
    klass = klass->superklass();
  }
  // coro->_is_continuation = coroutineObj->klass() == SystemDictionary::continuation_klass();
  coro->set_name(name);
  intptr_t** d = (intptr_t**)stack->stack_base();
  //*(--d) = NULL;
  *(--d) = NULL;
  jobject obj = JNIHandles::make_global(coroutineObj);
  *(--d) = (intptr_t*)obj;
  *(--d) = (intptr_t*)coro;
  *(--d) = NULL;
  *(--d) = (intptr_t*)coroutine_start;
  *(--d) = NULL;

  stack->set_last_sp((address) d);

  coro->_state = _onstack;
  coro->_is_thread_coroutine = false;
  coro->_thread = thread;
  coro->_stack = stack;
  coro->set_resource_area(new (mtThread) ResourceArea(32));
  coro->set_handle_area(new (mtThread) HandleArea(NULL, 32));
  coro->set_metadata_handles(new (ResourceObj::C_HEAP, mtClass) GrowableArray<Metadata*>(30, true));
  coro->set_active_handles(JNIHandleBlock::allocate_block());

#ifdef ASSERT
  coro->_java_call_counter = 0;
#endif
#if defined(_WINDOWS)
  coro->_last_SEH = NULL;
#endif
  if (TraceCoroutine) {
    ResourceMark rm;
    tty->print_cr("[Co]: CreateCoroutine %s(%p) in thread %s(%p)", coro->name(), coro, coro->thread()->name(), coro->thread());
  }
  return coro;
}

void Coroutine::free_coroutine(Coroutine* coroutine, JavaThread* thread) {
  coroutine->remove_from_list(thread->coroutine_list());
  delete coroutine;
}

void Coroutine::frames_do(FrameClosure* fc) {
  switch (_state) {
    case Coroutine::_current:
      // the contents of this coroutine have already been visited
      break;
    case Coroutine::_onstack:
      _stack->frames_do(fc, _is_thread_coroutine);
      break;
    case Coroutine::_dead:
      // coroutine is dead, ignore
      break;
  }
}

class oops_do_Closure: public FrameClosure {
private:
  OopClosure* _f;
 // CLDClosure* _cld_f;
  CodeBlobClosure* _cf;
  CLDClosure* _cld_f;

public:
  oops_do_Closure(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf): _f(f), _cld_f(cld_f), _cf(cf) { }
  void frames_do(frame* fr, RegisterMap* map) { fr->oops_do(_f, _cld_f, _cf, map); }
};

void Coroutine::oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf) {
	if(state() != Coroutine::_onstack)
	{
		return;
	}
  oops_do_Closure fc(f, cld_f, cf);
  frames_do(&fc);
  if (_state == _onstack ) {
    if (_handle_area != NULL)
    {
        DEBUG_CORO_ONLY(tty->print_cr("collecting handle area %08x", _handle_area));
        _handle_area->oops_do(f);
    }
    if (_active_handles != NULL)
    {
        DEBUG_CORO_ONLY(tty->print_cr("collecting _active_handles %08x", _active_handles));
        _active_handles->oops_do(f);
    }
  }
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
	if(state() != Coroutine::_onstack)
	{
		return;
	}
  if (metadata_handles() != NULL) {
    for (int i = 0; i< metadata_handles()->length(); i++) {
      f(metadata_handles()->at(i));
    }
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


CoroutineStack* CoroutineStack::create_thread_stack(JavaThread* thread) {
  CoroutineStack* stack = new CoroutineStack(0);
  if (stack == NULL)
    return NULL;

  stack->_thread = thread;
  stack->_is_thread_stack = true;
//  stack->_reserved_space;
//  stack->_virtual_space;
  stack->_stack_base = thread->stack_base();
  stack->_stack_size = thread->stack_size();
  stack->_last_sp = NULL;
  stack->_default_size = false;
  return stack;
}

CoroutineStack* CoroutineStack::create_stack(JavaThread* thread, intptr_t size/* = -1*/) {
  bool default_size = false;
  if (size <= 0) {
    size = DefaultCoroutineStackSize;
    default_size = true;
  }

  uint reserved_pages = StackShadowPages + StackRedPages + StackYellowPages;
  uintx real_stack_size = size + (reserved_pages * os::vm_page_size());
  uintx reserved_size = align_size_up(real_stack_size, os::vm_allocation_granularity());

  CoroutineStack* stack = new CoroutineStack(reserved_size);
  if (stack == NULL)
    return NULL;
  if (!stack->_virtual_space.initialize(stack->_reserved_space, real_stack_size)) {
    stack->_reserved_space.release();
    delete stack;
    return NULL;
  }

  stack->_thread = thread;
  stack->_is_thread_stack = false;
  stack->_stack_base = (address)stack->_virtual_space.high();
  stack->_stack_size = stack->_virtual_space.committed_size();
  stack->_last_sp = NULL;
  stack->_default_size = default_size;

  if (os::uses_stack_guard_pages()) {
    address low_addr = stack->stack_base() - stack->stack_size();
    size_t len = (StackYellowPages + StackRedPages) * os::vm_page_size();

    bool allocate = os::allocate_stack_guard_pages();

    if (!os::guard_memory((char *) low_addr, len)) {
      warning("Attempt to protect stack guard pages failed.");
      if (os::uncommit_memory((char *) low_addr, len)) {
        warning("Attempt to deallocate stack guard pages failed.");
      }
    }
  }

  ThreadLocalStorage::add_coroutine_stack(thread, stack->stack_base(), stack->stack_size());
  DEBUG_CORO_ONLY(tty->print("created coroutine stack at %08x with stack size %i (real size: %i)\n", stack->_stack_base, size, stack->_stack_size));
  return stack;
}

void CoroutineStack::free_stack(CoroutineStack* stack, JavaThread* thread) {
  //guarantee(!stack->is_thread_stack(), "cannot free thread stack");
  if(!stack->is_thread_stack())
  { 
      ThreadLocalStorage::remove_coroutine_stack(thread, stack->stack_base(), stack->stack_size());

      if (stack->_reserved_space.size() > 0) {
        stack->_virtual_space.release();
        stack->_reserved_space.release();
      }
  }
  delete stack;
}

void CoroutineStack::print_stack_on(outputStream* st)
{
  return print_stack_on(st, NULL, NULL);
}

void CoroutineStack::print_stack_on(void* frames, int* depth)
{
  return print_stack_on(NULL, frames, depth);
}

void CoroutineStack::print_stack_on(outputStream* st, void* frames, int* depth)
{
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
			if (vf->is_java_frame())
			{
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
					if (JavaMonitorsInStackTrace) {
						jvf->print_lock_info_on(st, count);
					}
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
void CoroutineStack::frames_do(FrameClosure* fc, bool isThreadCoroutine) {
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

frame CoroutineStack::last_frame(Coroutine* coro, RegisterMap& map) const {
  DEBUG_CORO_ONLY(tty->print_cr("last_frame CoroutineStack"));

  intptr_t* fp = ((intptr_t**)_last_sp)[0];
  assert(fp != NULL, "coroutine with NULL fp");

  address pc = ((address*)_last_sp)[1];
  intptr_t* sp = ((intptr_t*)_last_sp) + 2;

  return frame(sp, fp, pc);
}

JVM_ENTRY(jint, CONT_isPinned0(JNIEnv* env, jclass klass, long data)) {
  JavaThread* thread = JavaThread::thread_from_jni_environment(env);
  if (thread->locksAcquired != 0) {
    return 3;
  }
  Coroutine* coro = (Coroutine*)data;
  if (coro->jni_frames() != 0) {
    return 2; // JNI
  }
  return 0;
}
JVM_END

JVM_ENTRY(jlong, CONT_createContinuation(JNIEnv* env, jclass klass, jstring name, jobject cont, long stackSize)) {
  DEBUG_CORO_PRINT("CONT_createContinuation\n");
  assert(cont != NULL, "cannot create coroutine with NULL Coroutine object");

  if (stackSize < 0) {
    guarantee(thread->current_coroutine() == NULL, "current thread already has default continuation");
    thread->initialize_coroutine_support();
    if (TraceCoroutine) {
      tty->print_cr("CONT_createContinuation: create thread continuation %p", thread->current_coroutine());
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
  CoroutineStack* stack = NULL;
  if (stackSize == 0) {
    {
      MutexLockerEx ml(thread->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
      if (thread->coroutine_stack_cache_size() > 0) {
        stack = thread->coroutine_stack_cache();
        stack->remove_from_list(thread->coroutine_stack_cache());
        thread->coroutine_stack_cache_size()--;
        DEBUG_CORO_ONLY(tty->print("reused coroutine stack at %08x\n", stack->stack_base()));
        guarantee(stack != NULL, "get NULL from cache");
        stack->insert_into_list(thread->coroutine_stack_list());
      }
    }
  }
  if (stack == NULL) {
    stack = CoroutineStack::create_stack(thread, stackSize);
    if (stack == NULL) {
      THROW_0(vmSymbols::java_lang_OutOfMemoryError());
    }
    MutexLockerEx ml(thread->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
    stack->insert_into_list(thread->coroutine_stack_list());
  }

  Coroutine* coro = Coroutine::create_coroutine(NULL, thread, stack, JNIHandles::resolve(cont));
  if (coro == NULL) {
    ThreadInVMfromNative tivm(thread);
    HandleMark mark(thread);
    THROW_0(vmSymbols::java_lang_OutOfMemoryError());
  }
  {
    MutexLockerEx ml(thread->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
    coro->insert_into_list(thread->coroutine_list());
  }
  if (TraceCoroutine) {
    tty->print_cr("CONT_createContinuation: create continuation %p", coro);
  }
  return (jlong)coro;
}
JVM_END

JVM_ENTRY(void, CONT_switchTo(JNIEnv* env, jclass klass, jobject from, jobject to)) {
  ShouldNotReachHere();
}
JVM_END

JVM_ENTRY(void, CONT_switchToAndTerminate(JNIEnv* env, jclass klass, jobject from, jobject to)) {
  Coroutine::TerminateCoroutineObj(from);
}
JVM_END

JVM_ENTRY(jobjectArray, CONT_dumpStackTrace(JNIEnv *env, jclass klass, jobject cont))
  oop contOop = JNIHandles::resolve(cont);
  Coroutine* coro = (Coroutine*)java_lang_Continuation::data(contOop);
  assert(coro != NULL, "target coroutine is NULL in CoroutineSupport_dumpVirtualThreads");

  VirtualThreadStackTrace* res = new VirtualThreadStackTrace(coro);
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
  {CC"createContinuation",        CC"("JLSTR JLCONT "J)J", FN_PTR(CONT_createContinuation)},
  {CC"switchTo",                  CC"("JLCONT JLCONT")V", FN_PTR(CONT_switchTo)},
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

JVM_ENTRY(void, JVM_RegisterContinuationNativeMethods(JNIEnv *env, jclass cls)) {
    assert(thread->is_Java_thread(), "");
    ThreadToNativeFromVM trans((JavaThread*)thread);
    int status = env->RegisterNatives(cls, CONT_methods, sizeof(CONT_methods)/sizeof(JNINativeMethod));
    guarantee(status == JNI_OK && !env->ExceptionOccurred(), "register java.lang.Continuation natives");
    initializeForceWrapper(env, cls, thread, switchToIndex);
    initializeForceWrapper(env, cls, thread, switchToAndTerminateIndex);
}
JVM_END
