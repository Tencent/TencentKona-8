/*
 * Copyright (c) 1997, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2021, Loongson Technology. All rights reserved.
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

#ifndef CPU_MIPS_VM_BYTES_MIPS_HPP
#define CPU_MIPS_VM_BYTES_MIPS_HPP

#include "memory/allocation.hpp"

class Bytes: AllStatic {
 public:
  // Returns true if the byte ordering used by Java is different from the native byte ordering
  // of the underlying machine. For example, this is true for Intel x86, but false for Solaris
  // on Sparc.
  // we use mipsel, so return true
  static inline bool is_Java_byte_ordering_different(){ return true; }


  // Efficient reading and writing of unaligned unsigned data in platform-specific byte ordering
  // (no special code is needed since x86 CPUs can access unaligned data)
  static inline u2   get_native_u2(address p)         {
    if ((intptr_t)p & 0x1) {
      return ((u2)p[1] << 8) | (u2)p[0];
    } else {
      return *(u2*)p;
    }
  }

  static inline u4   get_native_u4(address p)         {
    if ((intptr_t)p & 3) {
      u4 res;
      __asm__ __volatile__ (
          " .set push\n"
          " .set mips64\n"
          " .set noreorder\n"

          "    lwr %[res], 0(%[addr])    \n"
          "    lwl  %[res], 3(%[addr])    \n"

          " .set pop"
          :  [res] "=&r" (res)
          : [addr] "r" (p)
          : "memory"
          );
      return res;
    } else {
      return *(u4*)p;
    }
  }

  static inline u8   get_native_u8(address p)         {
    u8 res;
    u8 temp;
    //  u4 tp;//tmp register
    __asm__ __volatile__ (
        " .set push\n"
        " .set mips64\n"
        " .set noreorder\n"
        " .set noat\n"
        "    andi $1,%[addr],0x7    \n"
        "    beqz $1,1f        \n"
        "    nop        \n"
        "    ldr %[temp], 0(%[addr])    \n"
        "    ldl  %[temp], 7(%[addr])  \n"
        "               b 2f        \n"
        "    nop        \n"
        "  1:\t  ld  %[temp],0(%[addr])  \n"
        "  2:\t   sd  %[temp], %[res]    \n"

        " .set at\n"
        " .set pop\n"
        :  [addr]"=r"(p), [temp]"=r" (temp)
        :  "[addr]"(p), "[temp]" (temp), [res]"m" (*(volatile jint*)&res)
        : "memory"
        );

    return res;
  }

  //use mips unaligned load instructions
  static inline void put_native_u2(address p, u2 x)   {
    if((intptr_t)p & 0x1) {
      p[0] = (u_char)(x);
      p[1] = (u_char)(x>>8);
    } else {
      *(u2*)p  = x;
    }
  }

  static inline void put_native_u4(address p, u4 x)   {
    // refer to sparc implementation.
    // Note that sparc is big-endian, while mips is little-endian
    switch ( intptr_t(p) & 3 ) {
    case 0:  *(u4*)p = x;
        break;

    case 2:  ((u2*)p)[1] = x >> 16;
       ((u2*)p)[0] = x;
       break;

    default: ((u1*)p)[3] = x >> 24;
       ((u1*)p)[2] = x >> 16;
       ((u1*)p)[1] = x >>  8;
       ((u1*)p)[0] = x;
       break;
    }
  }

  static inline void put_native_u8(address p, u8 x)   {
    // refer to sparc implementation.
    // Note that sparc is big-endian, while mips is little-endian
    switch ( intptr_t(p) & 7 ) {
    case 0:  *(u8*)p = x;
      break;

    case 4:  ((u4*)p)[1] = x >> 32;
      ((u4*)p)[0] = x;
      break;

    case 2:  ((u2*)p)[3] = x >> 48;
      ((u2*)p)[2] = x >> 32;
      ((u2*)p)[1] = x >> 16;
      ((u2*)p)[0] = x;
      break;

    default: ((u1*)p)[7] = x >> 56;
      ((u1*)p)[6] = x >> 48;
      ((u1*)p)[5] = x >> 40;
      ((u1*)p)[4] = x >> 32;
      ((u1*)p)[3] = x >> 24;
      ((u1*)p)[2] = x >> 16;
      ((u1*)p)[1] = x >>  8;
      ((u1*)p)[0] = x;
    }
  }


  // Efficient reading and writing of unaligned unsigned data in Java
  // byte ordering (i.e. big-endian ordering). Byte-order reversal is
  // needed since MIPS64EL CPUs use little-endian format.
  static inline u2   get_Java_u2(address p)           { return swap_u2(get_native_u2(p)); }
  static inline u4   get_Java_u4(address p)           { return swap_u4(get_native_u4(p)); }
  static inline u8   get_Java_u8(address p)           { return swap_u8(get_native_u8(p)); }

  static inline void put_Java_u2(address p, u2 x)     { put_native_u2(p, swap_u2(x)); }
  static inline void put_Java_u4(address p, u4 x)     { put_native_u4(p, swap_u4(x)); }
  static inline void put_Java_u8(address p, u8 x)     { put_native_u8(p, swap_u8(x)); }


  // Efficient swapping of byte ordering
  static inline u2   swap_u2(u2 x);                   // compiler-dependent implementation
  static inline u4   swap_u4(u4 x);                   // compiler-dependent implementation
  static inline u8   swap_u8(u8 x);
};


// The following header contains the implementations of swap_u2, swap_u4, and swap_u8[_base]
#ifdef TARGET_OS_ARCH_linux_mips
# include "bytes_linux_mips.inline.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_mips
# include "bytes_solaris_mips.inline.hpp"
#endif
#ifdef TARGET_OS_ARCH_windows_mips
# include "bytes_windows_mips.inline.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_mips
# include "bytes_bsd_mips.inline.hpp"
#endif


#endif // CPU_MIPS_VM_BYTES_MIPS_HPP
