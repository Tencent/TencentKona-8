/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_CPU_LINUX_LOONGARCH_VM_PREFETCH_LINUX_LOONGARCH_INLINE_HPP
#define OS_CPU_LINUX_LOONGARCH_VM_PREFETCH_LINUX_LOONGARCH_INLINE_HPP


inline void Prefetch::read (void *loc, intx interval) {
// According to previous and present SPECjbb2015 score,
// comment prefetch is better than if (interval >= 0) prefetch branch.
// So choose comment prefetch as the base line.
#if 0
  __asm__ __volatile__ (
                        "       preld  0, %[__loc] \n"
                        :
                        : [__loc] "m"( *((address)loc + interval) )
                        : "memory"
                        );
#endif
}

inline void Prefetch::write(void *loc, intx interval) {
// Ditto
#if 0
  __asm__ __volatile__ (
                        "       preld  8, %[__loc] \n"
                        :
                        : [__loc] "m"( *((address)loc + interval) )
                        : "memory"
                        );
#endif
}

#endif // OS_CPU_LINUX_LOONGARCH_VM_PREFETCH_LINUX_LOONGARCH_INLINE_HPP
