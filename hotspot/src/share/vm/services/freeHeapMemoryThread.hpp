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

#include "gc_implementation/shared/concurrentGCThread.hpp"

class FreeHeapPhysicalMemoryThread: public ConcurrentGCThread {
private:
  Monitor* _monitor;
  CollectedHeap* _sh;
  static FreeHeapPhysicalMemoryThread* _thread;

  void stop_service();

  void sleep_before_next_cycle(uintx waitms);
  FreeHeapPhysicalMemoryThread();

public:
  virtual void run();
  static void start();
  static FreeHeapPhysicalMemoryThread* thread() { return _thread;}
};
#endif // FREE_HEAP_PHYSICAL_MEMORY_HPP
