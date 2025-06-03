/*
 * Copyright (c) 1997, 2011, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_LOONGARCH_VM_INTERPRETER_LOONGARCH_HPP
#define CPU_LOONGARCH_VM_INTERPRETER_LOONGARCH_HPP

 public:

  // Sentinel placed in the code for interpreter returns so
  // that i2c adapters and osr code can recognize an interpreter
  // return address and convert the return to a specialized
  // block of code to handle compiedl return values and cleaning
  // the fpu stack.
  static const int return_sentinel;

  static Address::ScaleFactor stackElementScale() {
    return Address::times_8;
  }

  // Offset from sp (which points to the last stack element)
  static int expr_offset_in_bytes(int i) { return stackElementSize * i; }
  // Size of interpreter code.  Increase if too small.  Interpreter will
  // fail with a guarantee ("not enough space for interpreter generation");
  // if too small.
  // Run with +PrintInterpreterSize to get the VM to print out the size.
  // Max size with JVMTI and TaggedStackInterpreter
  const static int InterpreterCodeSize = 168 * 1024;
#endif // CPU_LOONGARCH_VM_INTERPRETER_LOONGARCH_HPP
