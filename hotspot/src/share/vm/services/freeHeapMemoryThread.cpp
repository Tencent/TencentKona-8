/*
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates 
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "precompiled.hpp"
#include "freeHeapMemoryThread.hpp"
#include "runtime/mutexLocker.hpp"
FreeHeapPhysicalMemoryThread* FreeHeapPhysicalMemoryThread::_thread = 0;
FreeHeapPhysicalMemoryThread::FreeHeapPhysicalMemoryThread() :
    ConcurrentGCThread() {
  _monitor = FreeHeapMemory_lock;
  _sh = Universe::heap();
  set_name("Free Heap Physical Memory Thread");
  create_and_start();
}

void FreeHeapPhysicalMemoryThread::start() {
  _thread = new FreeHeapPhysicalMemoryThread();
  if (NULL == _thread) {
    if (UseG1GC) {
      //disable free heap physical memory feature
      FreeHeapPhysicalMemory = false;
    }
  }
}

void FreeHeapPhysicalMemoryThread::sleep_before_next_cycle(uintx waitms) {
  MutexLockerEx x(_monitor, Mutex::_no_safepoint_check_flag);
  if (!_should_terminate) {
    _monitor->wait(Mutex::_no_safepoint_check_flag, waitms);
  }
}

void FreeHeapPhysicalMemoryThread::run() {
  Task* task = 0;
  const uintx waitms = 300000; // 5min
  while (!_should_terminate) {
    sleep_before_next_cycle(waitms);

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

void FreeHeapPhysicalMemoryThread::stop_service() {
  MutexLockerEx x(_monitor, Mutex::_no_safepoint_check_flag);
  _monitor->notify();
}
