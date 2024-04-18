/*
 * Copyright (c) 1997, 2018, Oracle and/or its affiliates. All rights reserved.
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

/*
 * This file has been modified by Loongson Technology in 2020. These
 * modifications are Copyright (c) 2015, 2020, Loongson Technology, and are made
 * available on the same license terms set forth above.
 */

#ifndef SHARE_VM_INTERPRETER_CPPINTERPRETERGENERATOR_HPP
#define SHARE_VM_INTERPRETER_CPPINTERPRETERGENERATOR_HPP

// This file contains the platform-independent parts
// of the template interpreter generator.

#ifdef CC_INTERP
#ifdef TARGET_ARCH_zero
# include "entry_zero.hpp"
#endif

class CppInterpreterGenerator: public AbstractInterpreterGenerator {
  protected:
  // shared code sequences
  // Converter for native abi result to tosca result
  address generate_result_handler_for(BasicType type);
  address generate_tosca_to_stack_converter(BasicType type);
  address generate_stack_to_stack_converter(BasicType type);
  address generate_stack_to_native_abi_converter(BasicType type);

  void generate_all();

 public:
  CppInterpreterGenerator(StubQueue* _code);

#ifdef TARGET_ARCH_x86
# include "cppInterpreterGenerator_x86.hpp"
#endif
#ifdef TARGET_ARCH_mips
# include "cppInterpreterGenerator_mips.hpp"
#endif
#ifdef TARGET_ARCH_loongarch
# include "cppInterpreterGenerator_loongarch.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "cppInterpreterGenerator_aarch64.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "cppInterpreterGenerator_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "cppInterpreterGenerator_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "cppInterpreterGenerator_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "cppInterpreterGenerator_ppc.hpp"
#endif

};

#endif // CC_INTERP

#endif // SHARE_VM_INTERPRETER_CPPINTERPRETERGENERATOR_HPP
