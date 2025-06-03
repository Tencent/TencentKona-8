/*
 * Copyright (c) 1999, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2022, Loongson Technology. All rights reserved.
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

#ifndef OS_CPU_LINUX_LOONGARCH_VM_THREADLS_LINUX_LOONGARCH_HPP
#define OS_CPU_LINUX_LOONGARCH_VM_THREADLS_LINUX_LOONGARCH_HPP

#ifdef MINIMIZE_RAM_USAGE
  // Processor dependent parts of ThreadLocalStorage
  //only the low 2G space for user program in Linux

  #define SP_BITLENGTH  34
  #define PAGE_SHIFT    14
  #define PAGE_SIZE     (1UL << PAGE_SHIFT)

  static Thread* _sp_map[1UL << (SP_BITLENGTH - PAGE_SHIFT)];
  static int _sp_map_low;
  static int _sp_map_high;
#endif // MINIMIZE_RAM_USAGE

public:
#ifdef MINIMIZE_RAM_USAGE
  static Thread** sp_map_addr() { return _sp_map; }
#endif // MINIMIZE_RAM_USAGE

  static Thread* thread() {
#ifdef MINIMIZE_RAM_USAGE
    /* Thread::thread() can also be optimized in the same way as __get_thread() */
    //return (Thread*) os::thread_local_storage_at(thread_index());
    uintptr_t sp;
    uintptr_t mask = (1UL << (SP_BITLENGTH - PAGE_SHIFT)) - 1;

    __asm__ __volatile__ ("addi.d %0, $r29, 0 " : "=r" (sp));

    return _sp_map[(sp >> PAGE_SHIFT) & mask];
#else
    return (Thread*) os::thread_local_storage_at(thread_index());
#endif // MINIMIZE_RAM_USAGE
  }
#endif // OS_CPU_LINUX_LOONGARCH_VM_THREADLS_LINUX_LOONGARCH_HPP
