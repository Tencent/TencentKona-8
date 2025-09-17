/*
 * Copyright (C) 2020, 2023, Tencent. All rights reserved.
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
#include "freeHeapMemoryThread.hpp"
#include "runtime/mutexLocker.hpp"

FreeHeapPhysicalMemoryThread* FreeHeapPhysicalMemoryThread::_thread = NULL;
CollectedHeap* FreeHeapPhysicalMemoryThread::_sh = NULL;

void FreeHeapPhysicalMemoryThread::initialize() {
  EXCEPTION_MARK;

  instanceKlassHandle klass (THREAD,  SystemDictionary::Thread_klass());
  instanceHandle thread_oop = klass->allocate_instance_handle(CHECK);

  const char* name = "Free Heap Physical Memory Thread";

  Handle string = java_lang_String::create_from_str(name, CHECK);

  // Initialize thread_oop to put it into the system threadGroup
  Handle thread_group (THREAD, Universe::system_thread_group());
  JavaValue result(T_VOID);
  JavaCalls::call_special(&result, thread_oop,
                          klass,
                          vmSymbols::object_initializer_name(),
                          vmSymbols::threadgroup_string_void_signature(),
                          thread_group,
                          string,
                          CHECK);

  {
    MutexLocker mu(Threads_lock);
    FreeHeapPhysicalMemoryThread* thread =  new FreeHeapPhysicalMemoryThread(&free_heap_memory_thread_entry);

    // At this point it may be possible that no osthread was created for the
    // JavaThread due to lack of memory. We would have to throw an exception
    // in that case. However, since this must work and we do not allow
    // exceptions anyway, check and abort if this fails.
    if (thread == NULL || thread->osthread() == NULL) {
      vm_exit_during_initialization("java.lang.OutOfMemoryError",
                                    "unable to create new native thread");
      if (UseG1GC) {
        //disable free heap physical memory feature
        FreeHeapPhysicalMemory = false;
      }
    }

    java_lang_Thread::set_thread(thread_oop(), thread);
    java_lang_Thread::set_priority(thread_oop(), NearMaxPriority);
    java_lang_Thread::set_daemon(thread_oop());
    thread->set_threadObj(thread_oop());
    _thread = thread;

    Threads::add(thread);
    Thread::start(thread);
  }
}


void FreeHeapPhysicalMemoryThread::free_heap_memory_thread_entry(JavaThread* jt, TRAPS) {
  Task* task = 0;
  uintx waitms = FreeHeapPhysicalMemoryCheckInterval * 1000;
  if (waitms < 1000) {
    waitms = 300000; // default 5min
    if (PrintGCDetails) {
      gclog_or_tty->print_cr("illegal FreeHeapPhysicalMemoryCheckInterval " UINTX_FORMAT ", update interval to 5min",
                              FreeHeapPhysicalMemoryCheckInterval);
    }
  }
  while (true) {
    {
      ThreadBlockInVM tbivm(jt);

      MutexLockerEx ml(FreeHeapMemory_lock, Mutex::_no_safepoint_check_flag);
      FreeHeapMemory_lock->wait(Mutex::_no_safepoint_check_flag, waitms);
    }
    while (!_sh->free_heap_memory_task_queue()->is_empty()) {
      if (_sh->free_heap_memory_task_queue()->pop_local(task)) {
        task->doit();
        //dont forget to delete task to avoid memory leak
        delete task;
        task = 0;
      }
    }

    //check if should do periodic gc
    _sh->check_for_periodic_gc(waitms);
  }
}
