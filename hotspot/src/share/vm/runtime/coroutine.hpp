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
class CoroutineStack;


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

  CoroutineStack* _stack;
  // for verify check
  JNIHandleBlock* saved_active_handles;
  size_t saved_active_handle_count;
  char* saved_handle_area_hwm;
  char* saved_resource_area_hwm;
  int saved_methodhandles_len;

  // mutable
  MonitorChunk*   _monitor_chunks;  // if deoptimizing happens in corutine it should record own monitor chunks
  int             _jni_frames;      // jni frame count in current coroutine stack, pinned
  JavaThread*     _thread;

#ifdef ASSERT
  int             _java_call_counter;
#endif

#ifdef _LP64
  intptr_t        _storage[2];
#endif

  // objects of this type can only be created via static functions
   Coroutine();

  virtual ~Coroutine();

  void frames_do(FrameClosure* fc);

  //for javacall stack reclaim
  bool             _has_javacall;

  char _name[64];

  static JavaThread* _main_thread;
  static Method* _continuation_start;

public:
  static void UpdateJniFrame(Thread* t, bool enter);
  static void Initialize();

  void set_name(const char* inname) 
  {
#ifdef TARGET_OS_FAMILY_windows
	  _snprintf(_name, sizeof(_name), "%s",inname);
#else
	  snprintf(_name, sizeof(_name), "%s", inname);
#endif
  }

  const char* name() const
  {
	  return _name;
  }
  static void switchTo_current_thread(Coroutine* coro);
  static void switchFrom_current_thread(Coroutine* coro, JavaThread* to);
  static void yield_verify(Coroutine* from, Coroutine* to, bool terminate);
  static JavaThread* main_thread() { return _main_thread; }
  static void set_main_thread(JavaThread* t) { _main_thread = t; }
  static Method* cont_start_method() { return _continuation_start; }

  void print_on(outputStream* st) const;
  void print_stack_on(outputStream* st);
  void print_stack_on(void* frames, int* depth);
  const char* get_coroutine_name() const;
  static const char* get_coroutine_state_name(CoroutineState state);
  bool is_lock_owned(address adr) const;

  bool has_javacall() const { return _has_javacall; }
  void set_has_javacall(bool hjc) { _has_javacall = hjc; }

  static Coroutine* create_thread_coroutine(const char* name,JavaThread* thread, CoroutineStack* stack);
  static Coroutine* create_coroutine(const char* name,JavaThread* thread, CoroutineStack* stack, oop coroutineObj);
  static void free_coroutine(Coroutine* coroutine, JavaThread* thread);

  int jni_frames() const { return _jni_frames; }
  void inc_jni_frames()  { _jni_frames++; }
  void dec_jni_frames()  { _jni_frames--; }

  CoroutineState state() const      { return _state; }
  void set_state(CoroutineState x)  { _state = x; }

  bool is_thread_coroutine() const  { return _is_thread_coroutine; }

  JavaThread* thread() const        { return _thread; }
  void set_thread(JavaThread* x)    { _thread = x; }

  CoroutineStack* stack() const     { return _stack; }

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
  static ByteSize stack_offset()              { return byte_offset_of(Coroutine, _stack); }

  static ByteSize thread_offset()             { return byte_offset_of(Coroutine, _thread); }
  static ByteSize jni_frame_offset()          { return byte_offset_of(Coroutine, _jni_frames); }
  static ByteSize has_javacall_offset()   { return byte_offset_of(Coroutine, _has_javacall); }

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

class CoroutineStack: public CHeapObj<mtThread>, public DoublyLinkedList<CoroutineStack> {
private:
  JavaThread*     _thread;

  bool            _is_thread_stack;
  ReservedSpace   _reserved_space;
  VirtualSpace    _virtual_space;

  address         _stack_base;
  intptr_t        _stack_size;
  bool            _default_size;

  address         _last_sp;

  // objects of this type can only be created via static functions
  CoroutineStack(intptr_t size) : _reserved_space(size) { }
  virtual ~CoroutineStack() { }

  void add_stack_frame(void* frames, int* depth, javaVFrame* jvf);
  void print_stack_on(outputStream* st, void* frames, int* depth);

public:
  static CoroutineStack* create_thread_stack(JavaThread* thread);
  static CoroutineStack* create_stack(JavaThread* thread, intptr_t size = -1);
  static void free_stack(CoroutineStack* stack, JavaThread* THREAD);

  static intptr_t get_start_method();

    bool    on_local_stack(address adr) const {
    /* QQQ this has knowledge of direction, ought to be a stack method */
    return (_stack_base >= adr && adr >= (_stack_base - _stack_size));
  }


  JavaThread* thread() const                { return _thread; }
  void set_thread(JavaThread* t)            { _thread = t; }
  bool is_thread_stack() const              { return _is_thread_stack; }

  address last_sp() const                   { return _last_sp; }
  void set_last_sp(address x)               { _last_sp = x; }

  address stack_base() const                { return _stack_base; }
  intptr_t stack_size() const               { return _stack_size; }
  bool is_default_size() const              { return _default_size; }

  frame last_frame(Coroutine* coro, RegisterMap& map) const;
  void print_stack_on(outputStream* st);
  void print_stack_on(void* frames, int* depth);
  // GC support
  void frames_do(FrameClosure* fc, bool isThreadCoroutine);

  static ByteSize stack_base_offset()         { return byte_offset_of(CoroutineStack, _stack_base); }
  static ByteSize stack_size_offset()         { return byte_offset_of(CoroutineStack, _stack_size); }
  static ByteSize last_sp_offset()            { return byte_offset_of(CoroutineStack, _last_sp); }
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
