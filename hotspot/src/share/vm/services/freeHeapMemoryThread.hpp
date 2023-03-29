/*
 * Copyright (C) 2020, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef FREE_HEAP_PHYSICAL_MEMORY_HPP
#define FREE_HEAP_PHYSICAL_MEMORY_HPP

#include "runtime/thread.hpp"

// A JavaThread for free heap physical memory.
class FreeHeapPhysicalMemoryThread : public JavaThread {
  friend class VMStructs;
 private:
  static CollectedHeap* _sh;
  static FreeHeapPhysicalMemoryThread* _thread;

  static void free_heap_memory_thread_entry(JavaThread* thread, TRAPS);
  FreeHeapPhysicalMemoryThread(ThreadFunction entry_point) : JavaThread(entry_point) { _sh = Universe::heap(); };

 public:
  static void initialize();

  // Hide this thread from external view.
  bool is_hidden_from_external_view() const      { return true; }

  static FreeHeapPhysicalMemoryThread* thread()  { return _thread;}
};
#endif // FREE_HEAP_PHYSICAL_MEMORY_HPP
