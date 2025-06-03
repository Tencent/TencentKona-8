/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2020, Loongson Technology. All rights reserved.
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

#ifndef OS_CPU_LINUX_LOONGARCH_VM_COPY_LINUX_LOONGARCH_INLINE_HPP
#define OS_CPU_LINUX_LOONGARCH_VM_COPY_LINUX_LOONGARCH_INLINE_HPP

static void pd_conjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  (void)memmove(to, from, count * HeapWordSize);
}

static void pd_disjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  switch (count) {
  case 8:  to[7] = from[7];
  case 7:  to[6] = from[6];
  case 6:  to[5] = from[5];
  case 5:  to[4] = from[4];
  case 4:  to[3] = from[3];
  case 3:  to[2] = from[2];
  case 2:  to[1] = from[1];
  case 1:  to[0] = from[0];
  case 0:  break;
  default:
    (void)memcpy(to, from, count * HeapWordSize);
    break;
  }
}

static void pd_disjoint_words_atomic(HeapWord* from, HeapWord* to, size_t count) {
  switch (count) {
  case 8:  to[7] = from[7];
  case 7:  to[6] = from[6];
  case 6:  to[5] = from[5];
  case 5:  to[4] = from[4];
  case 4:  to[3] = from[3];
  case 3:  to[2] = from[2];
  case 2:  to[1] = from[1];
  case 1:  to[0] = from[0];
  case 0:  break;
  default:
    while (count-- > 0) {
      *to++ = *from++;
    }
    break;
  }
}

static void pd_aligned_conjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_words(from, to, count);
}

static void pd_aligned_disjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  pd_disjoint_words(from, to, count);
}

static void pd_conjoint_bytes(void* from, void* to, size_t count) {
  (void)memmove(to, from, count);
}

static void pd_conjoint_bytes_atomic(void* from, void* to, size_t count) {
  pd_conjoint_bytes(from, to, count);
}

static void pd_conjoint_jshorts_atomic(jshort* from, jshort* to, size_t count) {
  copy_conjoint_atomic<jshort>(from, to, count);
}

static void pd_conjoint_jints_atomic(jint* from, jint* to, size_t count) {
  copy_conjoint_atomic<jint>(from, to, count);
}

static void pd_conjoint_jlongs_atomic(jlong* from, jlong* to, size_t count) {
  copy_conjoint_atomic<jlong>(from, to, count);
}

static void pd_conjoint_oops_atomic(oop* from, oop* to, size_t count) {
  //assert(!UseCompressedOops, "foo!");
  assert(HeapWordSize == BytesPerOop, "heapwords and oops must be the same size");
  copy_conjoint_atomic<oop>(from, to, count);
}

static void pd_arrayof_conjoint_bytes(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_bytes_atomic(from, to, count);
}

static void pd_arrayof_conjoint_jshorts(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_jshorts_atomic((jshort*)from, (jshort*)to, count);
}

static void pd_arrayof_conjoint_jints(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_jints_atomic((jint*)from, (jint*)to, count);
}

static void pd_arrayof_conjoint_jlongs(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_jlongs_atomic((jlong*)from, (jlong*)to, count);
}

static void pd_arrayof_conjoint_oops(HeapWord* from, HeapWord* to, size_t count) {
  //assert(!UseCompressedOops, "foo!");
  assert(BytesPerLong == BytesPerOop, "jlongs and oops must be the same size");
  pd_conjoint_oops_atomic((oop*)from, (oop*)to, count);
}

#endif // OS_CPU_LINUX_LOONGARCH_VM_COPY_LINUX_LOONGARCH_INLINE_HPP
