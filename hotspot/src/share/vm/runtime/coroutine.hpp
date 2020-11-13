/*
 * Copyright 1999-2010 Sun Microsystems, Inc.  All Rights Reserved.
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

#if INCLUDE_KONA_FIBER
#ifndef SHARE_VM_RUNTIME_COROUTINE_HPP
#define SHARE_VM_RUNTIME_COROUTINE_HPP

#include "runtime/jniHandles.hpp"
#include "runtime/handles.hpp"
#include "memory/allocation.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/javaFrameAnchor.hpp"
#include "runtime/monitorChunk.hpp"

// number of heap words that prepareSwitch will add as a safety measure to the CoroutineData size
#define COROUTINE_DATA_OVERSIZE (64)

//#define DEBUG_COROUTINES

#ifdef DEBUG_COROUTINES
#define DEBUG_CORO_ONLY(x) x
#define DEBUG_CORO_PRINT(x) tty->print(x)
#else
#define DEBUG_CORO_ONLY(x) 
#define DEBUG_CORO_PRINT(x)
#endif

class Coroutine;

const size_t CONT_BITMAP_LEN = 10;
const size_t CONT_CONTAINER_SIZE = 1 << CONT_BITMAP_LEN;
const size_t CONT_MASK_SHIFT = 5;
const size_t CONT_MASK = CONT_CONTAINER_SIZE - 1;
const int CONT_PREMAPPED_STACK_NUM = 100;
const int CONT_RESERVED_PHYSICAL_MEM_MAX = 100;
static const int cont_pin_monitor = 3;
static const int cont_pin_jni = 2;

/* Mapping numbers of stacks and set its permission as PROT_READ | PROT_WRITE */
class ContPreMappedStack : public CHeapObj<mtThread> {
private:
  ReservedSpace _reserved_space;
  VirtualSpace _virtual_space;
  ContPreMappedStack* _next;

public:
  int allocated_num;
  ContPreMappedStack(intptr_t size, ContPreMappedStack* next) : _reserved_space(size) { _next = next; allocated_num = 0; };
  bool initialize_virtual_space(intptr_t real_stack_size);
  address get_base_address() { return (address)_virtual_space.high(); };
  ~ContPreMappedStack() { _reserved_space.release(); };
};

class ContReservedStack : AllStatic {
private:
  static Mutex* _lock;
  /*
   * free_array contains stacks which are initialized, we can reuse it directly.
   * free_array is an array, the most recently used stack is at the bottom of
   * this array.
   */
  static GrowableArray<address>* free_array;
  /* 
   * A list of pre mapped stack, each node contains stacks which number is CONT_PREMAPPED_STACK_NUM,
   * current_pre_mapped_stack pointed to the node which is used currently, 
   * we should alloc a new pre mapped node when current_pre_mapped_stack is full.
   */
  static ContPreMappedStack* current_pre_mapped_stack;
  static int free_array_uncommit_index;

  static address get_stack_from_free_array();
  static address get_stack_from_pre_mapped();
  static bool add_pre_mapped_stack();

  static inline address acquire_stack();
  static inline bool pre_mapped_stack_is_full();

public:
  static uintx stack_size;

  static void init();
  /*
   * 1. Try to get stack from free array.
   * 2. If free array is not empty, get a stack from free array and return.
   * 3. If free array is empty, try to get stack from pre-mapped memory.
   * 4. If pre-mapped memory has no space to assign, add a new pre-mapped block.
   * 5. Get pre-mapped memory.
   * 6. Set the permisson of yellow page and red page as PROT_NONE.
   */ 
  static address get_stack();
  /* Release stack and insert the stack into free array */
  static void insert_stack(address node);
};

class ContBucket : public CHeapObj<mtThread> {
private:
  Mutex      _lock;
  Coroutine* _head;
  int        _count;

  // The parity of the last strong_roots iteration in which this ContBucket was
  // claimed as a task.
  jint _oops_do_parity;
  bool claim_oops_do_par_case(int collection_parity);

public:
  Mutex* lock() { return &_lock; }
  Coroutine* head() const { return _head; }
  int count() const { return _count; }
  void insert(Coroutine* cont);
  void remove(Coroutine* cont);
  ContBucket();
  bool claim_oops_do(bool is_par, int collection_parity) {
    if (!is_par) {
      _oops_do_parity = collection_parity;
      return true;
    } else {
      return claim_oops_do_par_case(collection_parity);
    }
  }
  static void create_cont_bucket_roots_tasks(GCTaskQueue* q);
  static void create_cont_bucket_roots_marking_tasks(GCTaskQueue* q);

  void frames_do(void f(frame*, const RegisterMap*));
  void oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf);
  void nmethods_do(CodeBlobClosure* cf);
  void metadata_do(void f(Metadata*));
  void print_stack_on(outputStream* st);
};

class ContContainer {
private:
  static ContBucket* _buckets;
  static size_t hash_code(Coroutine* cont);
public:
  static ContBucket* bucket(size_t index);
  static ContBucket* buckets() { return _buckets; };
  static void insert(Coroutine* cont);
  static void remove(Coroutine* cont);
  static void init();
  static void verify();

  static void frames_do(void f(frame*, const RegisterMap*));
  static void oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf);
  static void nmethods_do(CodeBlobClosure* cf);
  static void metadata_do(void f(Metadata*));
  static void print_stack_on(outputStream* st);
};

template<class T>
class DoublyLinkedList {
private:
  T*  _last;
  T*  _next;

public:
  DoublyLinkedList() {
    _last = NULL;
    _next = NULL;
  }

  typedef T* pointer;

  void remove_from_list(pointer& list);
  void insert_into_list(pointer& list);

  T* last() const   { return _last; }
  T* next() const   { return _next; }
};

class FrameClosure: public StackObj {
public:
  virtual void frames_do(frame* fr, RegisterMap* map) = 0;
};

class Coroutine: public CHeapObj<mtThread>, public DoublyLinkedList<Coroutine> {
public:
  enum CoroutineState {
    _onstack    = 0x00000001,
    _current    = 0x00000002,
    _dead       = 0x00000003,      // TODO is this really needed?
    _dummy      = 0xffffffff
  };

private:
  CoroutineState  _state;
  bool            _is_thread_coroutine;

  address         _stack_base;
  intptr_t        _stack_size;
  address         _last_sp;

  // for verify check
  JNIHandleBlock* saved_active_handles;
  size_t saved_active_handle_count;
  char* saved_handle_area_hwm;
  char* saved_resource_area_hwm;
  int saved_methodhandles_len;

  // mutable
  MonitorChunk*   _monitor_chunks;  // if deoptimizing happens in corutine it should record own monitor chunks
  JavaThread*     _thread;

#ifdef ASSERT
  int             _java_call_counter;
#endif

#ifdef _LP64
  intptr_t        _storage[2];
#endif

  // objects of this type can only be created via static functions
  Coroutine();

  void frames_do(FrameClosure* fc);

  //for javacall stack reclaim
  bool             _has_javacall;

  static JavaThread* _main_thread;
  static Method* _continuation_start;

  bool init_stack(JavaThread* thread);

  void add_stack_frame(void* frames, int* depth, javaVFrame* jvf);
  void print_stack_on(outputStream* st, void* frames, int* depth);

public:
  virtual ~Coroutine();
  static void Initialize();

  static void yield_verify(Coroutine* from, Coroutine* to, bool terminate);
  static JavaThread* main_thread() { return _main_thread; }
  static void set_main_thread(JavaThread* t) { _main_thread = t; }
  static Method* cont_start_method() { return _continuation_start; }

  void print_on(outputStream* st) const;
  void print_stack_on(outputStream* st);
  void print_stack_on(void* frames, int* depth);
  static const char* get_coroutine_state_name(CoroutineState state);
  bool is_lock_owned(address adr) const;

  bool has_javacall() const { return _has_javacall; }
  void set_has_javacall(bool hjc) { _has_javacall = hjc; }

  static Coroutine* create_thread_coroutine(JavaThread* thread);
  static Coroutine* create_coroutine(JavaThread* thread, long stack_size, oop coroutineObj);
  static void reset_coroutine(Coroutine* coro);
  static void init_coroutine(Coroutine* coro, JavaThread* thread);

  CoroutineState state() const      { return _state; }
  void set_state(CoroutineState x)  { _state = x; }

  bool is_thread_coroutine() const  { return _is_thread_coroutine; }

  JavaThread* thread() const        { return _thread; }
  void set_thread(JavaThread* x)    { _thread = x; }

  void set_saved_handle_area_hwm(char* wm) { saved_handle_area_hwm = wm; }
  char* get_saved_handle_area_hwm()        { return saved_handle_area_hwm; }

  void set_monitor_chunks(MonitorChunk* monitor_chunks) { _monitor_chunks = monitor_chunks; }
  MonitorChunk* monitor_chunks() const           { return _monitor_chunks; }
  void add_monitor_chunk(MonitorChunk* chunk);
  void remove_monitor_chunk(MonitorChunk* chunk);

#ifdef ASSERT
  int java_call_counter() const           { return _java_call_counter; }
  void set_java_call_counter(int x)       { _java_call_counter = x; }
#endif

  bool is_disposable();

  // GC support
  void oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf);
  void nmethods_do(CodeBlobClosure* cf);
  void metadata_do(void f(Metadata*));
  void frames_do(void f(frame*, const RegisterMap* map));
  static void cont_metadata_do(void f(Metadata*));
  static void TerminateCoroutineObj(jobject coro);
  static void TerminateCoroutine(Coroutine* coro);
  static ByteSize state_offset()              { return byte_offset_of(Coroutine, _state); }

  static ByteSize thread_offset()             { return byte_offset_of(Coroutine, _thread); }
  static ByteSize has_javacall_offset()   { return byte_offset_of(Coroutine, _has_javacall); }

  void init_thread_stack(JavaThread* thread);
  void free_stack();
  void on_stack_frames_do(FrameClosure* fc, bool isThreadCoroutine);
  bool on_local_stack(address adr) const {
    /* QQQ this has knowledge of direction, ought to be a stack method */
    return (_stack_base >= adr && adr >= (_stack_base - _stack_size));
  }
  void set_last_sp(address x)               { _last_sp = x; }

  static ByteSize stack_base_offset()         { return byte_offset_of(Coroutine, _stack_base); }
  static ByteSize stack_size_offset()         { return byte_offset_of(Coroutine, _stack_size); }
  static ByteSize last_sp_offset()            { return byte_offset_of(Coroutine, _last_sp); }

#ifdef ASSERT
  static ByteSize java_call_counter_offset()  { return byte_offset_of(Coroutine, _java_call_counter); }
#endif

#ifdef _LP64
  static ByteSize storage_offset()            { return byte_offset_of(Coroutine, _storage); }
#endif

#if defined(_WINDOWS)
private:
  address _last_SEH;
public:
  static ByteSize last_SEH_offset()           { return byte_offset_of(Coroutine, _last_SEH); }
#endif
};

/*
 * Continuation can only yield from Java frame, if there is some call from
 * runtime/native to Java code, these frame might use native monitor/thread/pointer
 * address and resources(handle/metadata handle/JNI...)
 *
 * This mark is used when call java from native frame, when yield happen and there
 * exists ContNativeFrameMark on stack, yield will fail with native pin.
 *
 * TBD: kernel thread might also has JNI frame and native lock used
 */
class ContNativeFrameMark: public StackObj {
 private:
  Coroutine* _cont;
  bool _update;
 public:
  ContNativeFrameMark(JavaThread* thread, bool first_frame) {
    assert(thread == JavaThread::current(), "not invoke in current thread");
    _cont = thread->current_coroutine();
    _update = (_cont != NULL && first_frame == false && _cont->is_thread_coroutine() == false);
    if (_update) {
      thread->inc_cont_jni_frames();
    }
  }

  ~ContNativeFrameMark() {
    assert(_cont == JavaThread::current()->current_coroutine(), "not same coroutine");
    if (_update) {
      JavaThread::current()->dec_cont_jni_frames();
    }
  }
};


template<class T> void DoublyLinkedList<T>::remove_from_list(pointer& list) {
  if (list == this) {
    if (list->_next == list)
      list = NULL;
    else
      list = list->_next;
  }
  _last->_next = _next;
  _next->_last = _last;
  _last = NULL;
  _next = NULL;
}

template<class T> void DoublyLinkedList<T>::insert_into_list(pointer& list) {
  if (list == NULL) {
    _next = (T*)this;
    _last = (T*)this;
    list = (T*)this;
  } else {
    _next = list->_next;
    list->_next = (T*)this;
    _last = list;
    _next->_last = (T*)this;
  }
}

void CONT_RegisterNativeMethods(JNIEnv *env, jclass cls, JavaThread* thread);
#endif // SHARE_VM_RUNTIME_COROUTINE_HPP
#endif // INCLUDE_KONA_FIBER
