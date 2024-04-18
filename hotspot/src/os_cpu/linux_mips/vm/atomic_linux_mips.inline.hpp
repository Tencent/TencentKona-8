/*
 * Copyright (c) 1999, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_CPU_LINUX_MIPS_VM_ATOMIC_LINUX_MIPS_INLINE_HPP
#define OS_CPU_LINUX_MIPS_VM_ATOMIC_LINUX_MIPS_INLINE_HPP

#include "orderAccess_linux_mips.inline.hpp"
#include "runtime/atomic.hpp"
#include "runtime/os.hpp"
#include "vm_version_mips.hpp"

// Implementation of class atomic

inline void Atomic::store    (jbyte    store_value, jbyte*    dest) { *dest = store_value; }
inline void Atomic::store    (jshort   store_value, jshort*   dest) { *dest = store_value; }
inline void Atomic::store    (jint     store_value, jint*     dest) { *dest = store_value; }
inline void Atomic::store    (jlong    store_value, jlong*    dest) { *dest = store_value; }
inline void Atomic::store_ptr(intptr_t store_value, intptr_t* dest) { *dest = store_value; }
inline void Atomic::store_ptr(void*    store_value, void*     dest) { *(void**)dest = store_value; }

inline void Atomic::store    (jbyte    store_value, volatile jbyte*    dest) { *dest = store_value; }
inline void Atomic::store    (jshort   store_value, volatile jshort*   dest) { *dest = store_value; }
inline void Atomic::store    (jint     store_value, volatile jint*     dest) { *dest = store_value; }
inline void Atomic::store    (jlong    store_value, volatile jlong*    dest) { *dest = store_value; }
inline void Atomic::store_ptr(intptr_t store_value, volatile intptr_t* dest) { *dest = store_value; }
inline void Atomic::store_ptr(void*    store_value, volatile void*     dest) { *(void**)dest = store_value; }

inline jlong Atomic::load     (volatile jlong* src) { return *src; }

///////////implementation of Atomic::add*/////////////////
inline jint Atomic::add  (jint add_value, volatile jint* dest) {
  jint __ret, __tmp;
  __asm__ __volatile__ (
      " .set push\n\t"
      " .set mips64\n\t"
      " .set noreorder\n\t"

      "1: sync          \n\t"
      "   ll    %[__ret], %[__dest]    \n\t"
      "   addu  %[__tmp], %[__val], %[__ret]  \n\t"
      "   sc    %[__tmp], %[__dest]    \n\t"
      "   beqz  %[__tmp], 1b         \n\t"
      "   nop          \n\t"

      " .set pop\n\t"

      : [__ret] "=&r" (__ret), [__tmp] "=&r" (__tmp)
      : [__dest] "m" (*(volatile jint*)dest), [__val] "r" (add_value)
      : "memory"
      );

  return add_value + __ret;
}

inline intptr_t Atomic::add_ptr (intptr_t add_value, volatile intptr_t* dest) {
  jint __ret, __tmp;
  __asm__ __volatile__ (
      " .set push\n\t"
      " .set mips64\n\t"
      " .set noreorder\n\t"

      "1: sync          \n\t"
      "   lld    %[__ret], %[__dest]    \n\t"
      "   daddu  %[__tmp], %[__val], %[__ret]  \n\t"
      "   scd    %[__tmp], %[__dest]    \n\t"
      "   beqz   %[__tmp], 1b         \n\t"
      "   nop          \n\t"

      " .set pop\n\t"

      : [__ret] "=&r" (__ret), [__tmp] "=&r" (__tmp)
      : [__dest] "m" (*(volatile jint*)dest), [__val] "r" (add_value)
      : "memory"
      );

  return add_value + __ret;
}

inline void* Atomic::add_ptr (intptr_t add_value, volatile void* dest) {
  return (void*)add_ptr((intptr_t)add_value, (volatile intptr_t*)dest);
}

///////////implementation of Atomic::inc*/////////////////
inline void Atomic::inc      (volatile jint*      dest) { (void)add(1, dest); }
inline void Atomic::inc_ptr (volatile intptr_t*  dest) { (void)add_ptr(1, dest); }
inline void Atomic::inc_ptr (volatile void*     dest) { (void)inc_ptr((volatile intptr_t*)dest); }

///////////implementation of Atomic::dec*/////////////////
inline void Atomic::dec      (volatile jint*      dest) { (void)add(-1, dest); }
inline void Atomic::dec_ptr (volatile intptr_t* dest) { (void)add_ptr(-1, dest); }
inline void Atomic::dec_ptr (volatile void*     dest) { (void)dec_ptr((volatile intptr_t*)dest); }


///////////implementation of Atomic::xchg*/////////////////
inline jint     Atomic::xchg    (jint     exchange_value, volatile jint*     dest) {
  jint __ret, __tmp;

  __asm__ __volatile__ (
      " .set push\n\t"
      " .set mips64\n\t"
      " .set noreorder\n\t"

      "1: sync\n\t"
      "   ll    %[__ret], %[__dest]  \n\t"
      "   move  %[__tmp], %[__val]  \n\t"
      "   sc    %[__tmp], %[__dest]  \n\t"
      "   beqz  %[__tmp], 1b    \n\t"
      "   nop        \n\t"

      " .set pop\n\t"

      : [__ret] "=&r" (__ret), [__tmp] "=&r" (__tmp)
      : [__dest] "m" (*(volatile jint*)dest), [__val] "r" (exchange_value)
      : "memory"
      );

  return __ret;
}

inline intptr_t Atomic::xchg_ptr(intptr_t exchange_value, volatile intptr_t* dest) {
  intptr_t __ret, __tmp;
  __asm__ __volatile__ (
      " .set push\n\t"
      " .set mips64\n\t"
      " .set noreorder\n\t"

      "1: sync\n\t"
      "   lld   %[__ret], %[__dest]  \n\t"
      "   move  %[__tmp], %[__val]  \n\t"
      "   scd   %[__tmp], %[__dest]  \n\t"
      "   beqz  %[__tmp], 1b    \n\t"
      "   nop        \n\t"

      " .set pop\n\t"

      : [__ret] "=&r" (__ret), [__tmp] "=&r" (__tmp)
      : [__dest] "m" (*(volatile intptr_t*)dest), [__val] "r" (exchange_value)
      : "memory"
      );
  return __ret;
}

inline void*    Atomic::xchg_ptr(void*    exchange_value, volatile void*     dest) {
  return (void*)xchg_ptr((intptr_t)exchange_value, (volatile intptr_t*)dest);
}

///////////implementation of Atomic::cmpxchg*/////////////////
inline jint     Atomic::cmpxchg    (jint     exchange_value, volatile jint*     dest, jint     compare_value) {
  jint __prev, __cmp;

  __asm__ __volatile__ (
      "  .set push\n\t"
      "  .set mips64\n\t"
      "  .set noreorder\n\t"

      "1:sync \n\t"
      "  ll     %[__prev], %[__dest]    \n\t"
      "  bne    %[__prev], %[__old], 2f  \n\t"
      "  move  %[__cmp],  $0          \n\t"
      "  move  %[__cmp],  %[__new]  \n\t"
      "  sc  %[__cmp],  %[__dest]  \n\t"
      "  beqz  %[__cmp],  1b    \n\t"
      "  nop        \n\t"
      "2:        \n\t"
      "  sync        \n\t"

      "  .set pop\n\t"

      : [__prev] "=&r" (__prev), [__cmp] "=&r" (__cmp)
      : [__dest] "m" (*(volatile jint*)dest), [__old] "r" (compare_value),  [__new] "r" (exchange_value)
      : "memory"
      );

  return __prev;
}

inline jlong    Atomic::cmpxchg    (jlong    exchange_value, volatile jlong*    dest, jlong    compare_value) {
  jlong __prev, __cmp;

  __asm__ __volatile__ (
      "  .set push\n\t"
      "  .set mips64\n\t"
      "  .set noreorder\n\t"

      "1:sync \n\t"
      "  lld   %[__prev], %[__dest]    \n\t"
      "  bne   %[__prev], %[__old], 2f  \n\t"
      "  move  %[__cmp],  $0          \n\t"
      "  move  %[__cmp],  %[__new]  \n\t"
      "  scd   %[__cmp],  %[__dest]  \n\t"
      "  beqz  %[__cmp],  1b    \n\t"
      "  nop        \n\t"
      "2:        \n\t"
      "  sync \n\t"

      "  .set pop\n\t"

      : [__prev] "=&r" (__prev), [__cmp] "=&r" (__cmp)
      : [__dest] "m" (*(volatile jlong*)dest), [__old] "r" (compare_value),  [__new] "r" (exchange_value)
      : "memory"
      );
  return __prev;
}

inline intptr_t Atomic::cmpxchg_ptr(intptr_t exchange_value, volatile intptr_t* dest, intptr_t compare_value) {
  intptr_t __prev, __cmp;
  __asm__ __volatile__ (
      " .set push \n\t"
      " .set mips64\n\t\t"
      " .set noreorder\n\t"

      "1:sync \n\t"
      "  lld    %[__prev], %[__dest]    \n\t"
      "  bne    %[__prev], %[__old], 2f  \n\t"
      "  move  %[__cmp],  $0          \n\t"
      "  move  %[__cmp],  %[__new]  \n\t"
      "  scd  %[__cmp],  %[__dest]  \n\t"
      "  beqz  %[__cmp],  1b    \n\t"
      "  nop        \n\t"
      "2:        \n\t"
      "  sync \n\t"
      "  .set pop  \n\t"

      : [__prev] "=&r" (__prev), [__cmp] "=&r" (__cmp)
      : [__dest] "m" (*(volatile intptr_t*)dest), [__old] "r" (compare_value),  [__new] "r" (exchange_value)
      : "memory"
      );

      return __prev;
}

inline void* Atomic::cmpxchg_ptr(void* exchange_value, volatile void* dest, void* compare_value) {
  return (void*)cmpxchg_ptr((intptr_t)exchange_value, (volatile intptr_t*)dest, (intptr_t)compare_value);
}

#endif // OS_CPU_LINUX_MIPS_VM_ATOMIC_LINUX_MIPS_INLINE_HPP
