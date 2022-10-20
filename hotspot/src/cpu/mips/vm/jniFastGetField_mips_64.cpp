/*
 * Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "asm/macroAssembler.hpp"
#include "memory/resourceArea.hpp"
#include "prims/jniFastGetField.hpp"
#include "prims/jvm_misc.hpp"
#include "runtime/safepoint.hpp"

#define __ masm->

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
#define T8 RT8
#define T9 RT9

#define BUFFER_SIZE 30*wordSize

// Instead of issuing lfence for LoadLoad barrier, we create data dependency
// between loads, which is more efficient than lfence.

address JNI_FastGetField::generate_fast_get_int_field0(BasicType type) {
  const char *name = NULL;
  switch (type) {
    case T_BOOLEAN: name = "jni_fast_GetBooleanField"; break;
    case T_BYTE:    name = "jni_fast_GetByteField";    break;
    case T_CHAR:    name = "jni_fast_GetCharField";    break;
    case T_SHORT:   name = "jni_fast_GetShortField";   break;
    case T_INT:     name = "jni_fast_GetIntField";     break;
    case T_LONG:    name = "jni_fast_GetLongField";    break;
    case T_FLOAT:   name = "jni_fast_GetFloatField";   break;
    case T_DOUBLE:  name = "jni_fast_GetDoubleField";  break;
    default:        ShouldNotReachHere();
  }
  ResourceMark rm;
  BufferBlob* blob = BufferBlob::create(name, BUFFER_SIZE);
  CodeBuffer cbuf(blob);
  MacroAssembler* masm = new MacroAssembler(&cbuf);
  address fast_entry = __ pc();

  Label slow;

  //  return pc        RA
  //  jni env          A0
  //  obj              A1
  //  jfieldID         A2

  address counter_addr = SafepointSynchronize::safepoint_counter_addr();
  __ set64(AT, (long)counter_addr);
  __ lw(T1, AT, 0);

  // Parameters(A0~A3) should not be modified, since they will be used in slow path
  __ andi(AT, T1, 1);
  __ bne(AT, R0, slow);
  __ delayed()->nop();

  __ move(T0, A1);
  __ clear_jweak_tag(T0);

  __ ld(T0, T0, 0);              // unbox, *obj
  __ dsrl(T2, A2, 2);                 // offset
  __ daddu(T0, T0, T2);

  assert(count < LIST_CAPACITY, "LIST_CAPACITY too small");
  speculative_load_pclist[count] = __ pc();
  switch (type) {
    case T_BOOLEAN: __ lbu (V0, T0, 0); break;
    case T_BYTE:    __ lb  (V0, T0, 0); break;
    case T_CHAR:    __ lhu (V0, T0, 0); break;
    case T_SHORT:   __ lh  (V0, T0, 0); break;
    case T_INT:     __ lw  (V0, T0, 0); break;
    case T_LONG:    __ ld  (V0, T0, 0); break;
    case T_FLOAT:   __ lwc1(F0, T0, 0); break;
    case T_DOUBLE:  __ ldc1(F0, T0, 0); break;
    default:        ShouldNotReachHere();
  }

  __ set64(AT, (long)counter_addr);
  __ lw(AT, AT, 0);
  __ bne(T1, AT, slow);
  __ delayed()->nop();

  __ jr(RA);
  __ delayed()->nop();

  slowcase_entry_pclist[count++] = __ pc();
  __ bind (slow);
  address slow_case_addr = NULL;
  switch (type) {
    case T_BOOLEAN: slow_case_addr = jni_GetBooleanField_addr(); break;
    case T_BYTE:    slow_case_addr = jni_GetByteField_addr();    break;
    case T_CHAR:    slow_case_addr = jni_GetCharField_addr();    break;
    case T_SHORT:   slow_case_addr = jni_GetShortField_addr();   break;
    case T_INT:     slow_case_addr = jni_GetIntField_addr();     break;
    case T_LONG:    slow_case_addr = jni_GetLongField_addr();    break;
    case T_FLOAT:   slow_case_addr = jni_GetFloatField_addr();   break;
    case T_DOUBLE:  slow_case_addr = jni_GetDoubleField_addr();  break;
    default:        ShouldNotReachHere();
  }
  __ jmp(slow_case_addr);
  __ delayed()->nop();

  __ flush ();

  return fast_entry;
}

address JNI_FastGetField::generate_fast_get_boolean_field() {
  return generate_fast_get_int_field0(T_BOOLEAN);
}

address JNI_FastGetField::generate_fast_get_byte_field() {
  return generate_fast_get_int_field0(T_BYTE);
}

address JNI_FastGetField::generate_fast_get_char_field() {
  return generate_fast_get_int_field0(T_CHAR);
}

address JNI_FastGetField::generate_fast_get_short_field() {
  return generate_fast_get_int_field0(T_SHORT);
}

address JNI_FastGetField::generate_fast_get_int_field() {
  return generate_fast_get_int_field0(T_INT);
}

address JNI_FastGetField::generate_fast_get_long_field() {
  return generate_fast_get_int_field0(T_LONG);
}

address JNI_FastGetField::generate_fast_get_float_field() {
  return generate_fast_get_int_field0(T_FLOAT);
}

address JNI_FastGetField::generate_fast_get_double_field() {
  return generate_fast_get_int_field0(T_DOUBLE);
}
