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

#include "precompiled.hpp"
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "runtime/os.hpp"
#include "runtime/threadLocalStorage.hpp"

#define A0 RA0
#define A1 RA1
#define A2 RA2
#define A3 RA3
#define A4 RA4
#define A5 RA5
#define A6 RA6
#define A7 RA7
#define T0 RT0
#define T1 RT1
#define T2 RT2
#define T3 RT3
#define T4 RT4
#define T5 RT5
#define T6 RT6
#define T7 RT7
#define T8 RT8

void MacroAssembler::get_thread(Register thread) {
#ifdef MINIMIZE_RAM_USAGE
  Register tmp;

  if (thread == AT)
    tmp = T9;
  else
    tmp = AT;

  move(thread, SP);
  shr(thread, PAGE_SHIFT);

  push(tmp);
  li(tmp, ((1UL << (SP_BITLENGTH - PAGE_SHIFT)) - 1));
  andr(thread, thread, tmp);
  shl(thread, Address::times_ptr); // sizeof(Thread *)
  li48(tmp, (long)ThreadLocalStorage::sp_map_addr());
  add_d(tmp, tmp, thread);
  ld_ptr(thread, tmp, 0);
  pop(tmp);
#else
  if (thread != V0) {
    push(V0);
  }
  pushad_except_v0();

  li(A0, ThreadLocalStorage::thread_index());
  push(S5);
  move(S5, SP);
  li(AT, -StackAlignmentInBytes);
  andr(SP, SP, AT);
  // TODO: confirm reloc
  call(CAST_FROM_FN_PTR(address, pthread_getspecific), relocInfo::runtime_call_type);
  move(SP, S5);
  pop(S5);

  popad_except_v0();
  if (thread != V0) {
    move(thread, V0);
    pop(V0);
  }
#endif // MINIMIZE_RAM_USAGE
}
