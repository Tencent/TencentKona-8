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
Method* Coroutine::_continuation_start = NULL;

void Coroutine::add_stack_frame(void* frames, int* depth, javaVFrame* jvf) {
  StackFrameInfo* frame = new StackFrameInfo(jvf, false);
  ((GrowableArray<StackFrameInfo*>*)frames)->append(frame);
  (*depth)++;
}

#if defined(LINUX) || defined(_ALLBSD_SOURCE)
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
    tty->print_cr("[Co]: TerminateCoroutine %s(%p) in thread %s(%p)", coro->name(), coro, coro->thread()->name(), coro->thread());
  }
  guarantee(thread == JavaThread::current(), "thread not match");

  {
    MutexLockerEx ml(thread->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
    coro->remove_from_list(thread->coroutine_list());

    if (thread->coroutine_cache_size() < MaxFreeCoroutinesCacheSize) {
      coro->insert_into_list(thread->coroutine_cache());
      thread->coroutine_cache_size() ++;
    } else {
      delete coro;
    }
  }
}

void Coroutine::TerminateCoroutineObj(jobject coroutine)
{
  
  oop old_oop = JNIHandles::resolve(coroutine);
  Coroutine* coro = (Coroutine*)java_lang_Continuation::data(old_oop);
  assert(coro != NULL, "NULL old coroutine in switchToAndTerminate");
  java_lang_Continuation::set_data(old_oop, 0);
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

Coroutine::Coroutine(intptr_t size) : _reserved_space(size) {
	_has_javacall = false;
  _monitor_chunks = NULL;
  _jni_frames = 0;
}

Coroutine::~Coroutine() {
  if(!is_thread_coroutine()) {
    assert(_monitor_chunks == NULL, "not empty _monitor_chunks");
  }

  free_stack();
}

static inline void extract_from(Coroutine* coro, JavaThread* from) {
  MutexLockerEx ml(from->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
  coro->remove_from_list(from->coroutine_list());
}

static inline void insert_into(Coroutine* coro, JavaThread* to) {
  MutexLockerEx ml(to->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
  coro->insert_into_list(to->coroutine_list());
}

/*
 * switch from coroutine running _thread to target_thread
 * extract coroutine/stack from _thread, insert into target thread
 */
void Coroutine::switchTo_current_thread(Coroutine* coro) {
  JavaThread* target_thread = JavaThread::current();
  JavaThread* old_thread = coro->thread();
  if (old_thread == target_thread) {
    return;
  }

  extract_from(coro, old_thread);
  insert_into(coro, target_thread);
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

  extract_from(coro, from);
  insert_into(coro, to);
  coro->set_thread(to);
  if (TraceCoroutine) {
    ResourceMark rm;
    // can not get to thread name
    tty->print_cr("[Co]: SwitchFromCurrent Coroutine %s(%p) from %s(%p) to (%p)",
      coro->name(), coro, from->name(), from, to);
  }
}

/*
 * 1. check yield is from thread coroutine or to thread coroutine
 * 2. check java_call_counter is 1 when terminate coroutine
 */
void Coroutine::yield_verify(Coroutine* from, Coroutine* to, bool terminate) {
  if (TraceCoroutine) {
    tty->print_cr("yield_verify from %p to %p", from, to);
  }
  if (from->is_thread_coroutine()) {
    // save snapshot
    guarantee(terminate == false, "switch from kernel to continnuation");
    JavaThread* thread = from->_thread;
    JNIHandleBlock* jni_handle_block = thread->active_handles();
    to->saved_active_handles = jni_handle_block;
    to->saved_active_handle_count = jni_handle_block->get_number_of_live_handles();
    to->saved_resource_area_hwm = thread->resource_area()->hwm();
    to->saved_handle_area_hwm = thread->handle_area()->hwm();
    to->saved_methodhandles_len = thread->metadata_handles()->length();
  } else {
    // switch to thread corotuine, compare status
    JavaThread* thread = from->_thread;
    JNIHandleBlock* jni_handle_block = thread->active_handles();
    guarantee(from->saved_active_handles == jni_handle_block, "must same handle");
    guarantee(from->saved_active_handle_count == jni_handle_block->get_number_of_live_handles(), "must same count");
    from->saved_active_handles = NULL;
    from->saved_active_handle_count = 0;
    if (terminate) {
      assert(thread->java_call_counter() == 1, "must be 1 when terminate");
    }
    if (from->saved_handle_area_hwm != thread->handle_area()->hwm()) {
      tty->print_cr("%p failed %p, %p", from, from->saved_handle_area_hwm, thread->handle_area()->hwm());
      guarantee(false, "handle area leak");
    }
    if (from->saved_resource_area_hwm != thread->resource_area()->hwm()) {
      tty->print_cr("%p failed %p, %p", from, from->saved_resource_area_hwm, thread->resource_area()->hwm());
      guarantee(false, "resource area leak");
    }
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
	st->print_cr("[" INTPTR_FORMAT "]", (intptr_t)_last_sp & ~right_n_bits(12));
	st->print_cr("   java.lang.Thread.State: %s", get_coroutine_state_name(this->state()));
}

void Coroutine::print_stack_on(void* frames, int* depth)
{
  if (!has_javacall() || state() != Coroutine::_onstack) return;
  print_stack_on(NULL, frames, depth);
}

void Coroutine::print_stack_on(outputStream* st)
{
	if (!has_javacall()) return;
	if( state() == Coroutine::_onstack)
		print_stack_on(st, NULL, NULL);
}

bool Coroutine::is_lock_owned(address adr) const {
  if (on_local_stack(adr)) {
    return true;
  }
  for (MonitorChunk* chunk = monitor_chunks(); chunk != NULL; chunk = chunk->next()) {
    if (chunk->contains(adr)) {
      return true;
    }
  }
  return false;
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

Coroutine* Coroutine::create_thread_coroutine(const char* name,JavaThread* thread) {
  Coroutine* coro = new Coroutine(0);
  if (coro == NULL)
    return NULL;
  coro->set_name(name);
  coro->_state = _current;
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
  return coro;
}

void Coroutine::reset_coroutine(Coroutine* coro) {
  coro->_has_javacall = false;
  assert(coro->_monitor_chunks == NULL, "monitor_chunks must be NULL");
  assert(coro->_jni_frames == 0, "jni_frames must be 0");
}

void Coroutine::init_coroutine(Coroutine* coro, const char* name, JavaThread* thread) {
  coro->set_name(name);
  intptr_t** d = (intptr_t**)coro->_stack_base;
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
    tty->print_cr("[Co]: CreateCoroutine %s(%p) in thread %s(%p)", coro->name(), coro, coro->thread()->name(), coro->thread());
  }
}

Coroutine* Coroutine::create_coroutine(const char* name,JavaThread* thread, long stack_size, oop coroutineObj) {
  if (stack_size <= 0) {
    stack_size = DefaultCoroutineStackSize;
  }

  uint reserved_pages = StackShadowPages + StackRedPages + StackYellowPages;
  uintx real_stack_size = stack_size + (reserved_pages * os::vm_page_size());
  uintx reserved_size = align_size_up(real_stack_size, os::vm_allocation_granularity());

  Coroutine* coro = new Coroutine(reserved_size);
  if (coro == NULL) {
    return NULL;
  }

  if (!coro->init_stack(thread, real_stack_size)) {
    return NULL;
  }

  Coroutine::init_coroutine(coro, name, thread);
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
      on_stack_frames_do(fc, _is_thread_coroutine);
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

bool Coroutine::init_stack(JavaThread* thread, intptr_t real_stack_size) {
  if (!_virtual_space.initialize(_reserved_space, real_stack_size)) {
    _reserved_space.release();
    return false;
  }

  _stack_base = (address)_virtual_space.high();
  _stack_size = _virtual_space.committed_size();
  _last_sp = NULL;

  if (os::uses_stack_guard_pages()) {
    address low_addr = _stack_base - _stack_size;
    size_t len = (StackYellowPages + StackRedPages) * os::vm_page_size();

    bool allocate = os::allocate_stack_guard_pages();

    if (!os::guard_memory((char *) low_addr, len)) {
      warning("Attempt to protect stack guard pages failed.");
      if (os::uncommit_memory((char *) low_addr, len)) {
        warning("Attempt to deallocate stack guard pages failed.");
      }
    }
  }

  ThreadLocalStorage::add_coroutine_stack(thread, _stack_base, _stack_size);
  DEBUG_CORO_ONLY(tty->print("created coroutine stack at %08x with real size: %i\n", _stack_base, _stack_size));

  return true;
}

void Coroutine::free_stack() {
  //guarantee(!stack->is_thread_stack(), "cannot free thread stack");
  if(!is_thread_coroutine())
  { 
      ThreadLocalStorage::remove_coroutine_stack(_thread, _stack_base, _stack_size);

      if (_reserved_space.size() > 0) {
        _virtual_space.release();
        _reserved_space.release();
      }
  }
}

void Coroutine::print_stack_on(outputStream* st, void* frames, int* depth)
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
    {
      MutexLockerEx ml(thread->coroutine_list_lock(), Mutex::_no_safepoint_check_flag);
      if (thread->coroutine_cache_size() > 0) {
        coro = thread->coroutine_cache();
        coro->remove_from_list(thread->coroutine_cache());
        thread->coroutine_cache_size()--;
        Coroutine::reset_coroutine(coro);
        Coroutine::init_coroutine(coro, NULL, thread);
        DEBUG_CORO_ONLY(tty->print("reused coroutine stack at %08x\n", _stack_base));
        guarantee(coro != NULL, "get NULL from cache");
      }
    }
  }
  if (coro == NULL) {
    coro = Coroutine::create_coroutine(NULL, thread, stackSize, JNIHandles::resolve(cont));
    if (coro == NULL) {
      ThreadInVMfromNative tivm(thread);
      HandleMark mark(thread);
      THROW_0(vmSymbols::java_lang_OutOfMemoryError());
    }
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

JVM_ENTRY(void, CONT_switchTo(JNIEnv* env, jclass klass, jobject target, jobject current)) {
  ShouldNotReachHere();
}
JVM_END

JVM_ENTRY(void, CONT_switchToAndTerminate(JNIEnv* env, jclass klass, jobject target, jobject current)) {
  Coroutine::TerminateCoroutineObj(current);
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

void CONT_RegisterNativeMethods(JNIEnv *env, jclass cls, JavaThread* thread) {
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
