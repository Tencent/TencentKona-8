/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interpreter.hpp"
#include "nativeInst_loongarch.hpp"
#include "oops/instanceOop.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/top.hpp"
#ifdef COMPILER2
#include "opto/runtime.hpp"
#endif

// Declaration and definition of StubGenerator (no .hpp file).
// For a more detailed description of the stub routine structure
// see the comment in stubRoutines.hpp

#define __ _masm->

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

#define TIMES_OOP (UseCompressedOops ? Address::times_4 : Address::times_8)

//#ifdef PRODUCT
//#define BLOCK_COMMENT(str) /* nothing */
//#else
//#define BLOCK_COMMENT(str) __ block_comment(str)
//#endif

//#define BIND(label) bind(label); BLOCK_COMMENT(#label ":")
const int MXCSR_MASK = 0xFFC0;  // Mask out any pending exceptions

// Stub Code definitions

static address handle_unsafe_access() {
  JavaThread* thread = JavaThread::current();
  address pc = thread->saved_exception_pc();
  // pc is the instruction which we must emulate
  // doing a no-op is fine:  return garbage from the load
  // therefore, compute npc
  address npc = (address)((unsigned long)pc + sizeof(unsigned int));

  // request an async exception
  thread->set_pending_unsafe_access_error();

  // return address of next instruction to execute
  return npc;
}

class StubGenerator: public StubCodeGenerator {
 private:

  // This fig is not LA ABI. It is call Java from C ABI.
  // Call stubs are used to call Java from C
  //
  //    [ return_from_Java     ]
  //    [ argument word n-1    ] <--- sp
  //      ...
  //    [ argument word 0      ]
  //      ...
  // -8 [ S6                   ]
  // -7 [ S5                   ]
  // -6 [ S4                   ]
  // -5 [ S3                   ]
  // -4 [ S1                   ]
  // -3 [ TSR(S2)              ]
  // -2 [ LVP(S7)              ]
  // -1 [ BCP(S1)              ]
  //  0 [ saved fp             ] <--- fp_after_call
  //  1 [ return address       ]
  //  2 [ ptr. to call wrapper ] <--- a0 (old sp -->)fp
  //  3 [ result               ] <--- a1
  //  4 [ result_type          ] <--- a2
  //  5 [ method               ] <--- a3
  //  6 [ entry_point          ] <--- a4
  //  7 [ parameters           ] <--- a5
  //  8 [ parameter_size       ] <--- a6
  //  9 [ thread               ] <--- a7

  //
  // LA ABI does not save paras in sp.
  //
  //    [ return_from_Java     ]
  //    [ argument word n-1    ] <--- sp
  //      ...
  //    [ argument word 0      ]
  //      ...
  //-13 [ thread               ]
  //-12 [ result_type          ] <--- a2
  //-11 [ result               ] <--- a1
  //-10 [                      ]
  // -9 [ ptr. to call wrapper ] <--- a0
  // -8 [ S6                   ]
  // -7 [ S5                   ]
  // -6 [ S4                   ]
  // -5 [ S3                   ]
  // -4 [ S1                   ]
  // -3 [ TSR(S2)              ]
  // -2 [ LVP(S7)              ]
  // -1 [ BCP(S1)              ]
  //  0 [ saved fp             ] <--- fp_after_call
  //  1 [ return address       ]
  //  2 [                      ] <--- old sp
  //
  // Find a right place in the call_stub for S8.
  // S8 will point to the starting point of Interpreter::dispatch_table(itos).
  // It should be saved/restored before/after Java calls.
  //
  enum call_stub_layout {
    RA_off             =  1,
    FP_off             =  0,
    BCP_off            = -1,
    LVP_off            = -2,
    TSR_off            = -3,
    S1_off             = -4,
    S3_off             = -5,
    S4_off             = -6,
    S5_off             = -7,
    S6_off             = -8,
    call_wrapper_off   = -9,
    result_off         = -11,
    result_type_off    = -12,
    thread_off         = -13,
    total_off          = thread_off - 1,
    S8_off             = -14,
  };

  address generate_call_stub(address& return_address) {
    assert((int)frame::entry_frame_call_wrapper_offset == (int)call_wrapper_off, "adjust this code");
    StubCodeMark mark(this, "StubRoutines", "call_stub");
    address start = __ pc();

    // same as in generate_catch_exception()!

    // stub code
    // save ra and fp
    __ enter();
    // I think 14 is the max gap between argument and callee saved register
    __ addi_d(SP, SP, total_off * wordSize);
    __ st_d(BCP, FP, BCP_off * wordSize);
    __ st_d(LVP, FP, LVP_off * wordSize);
    __ st_d(TSR, FP, TSR_off * wordSize);
    __ st_d(S1, FP, S1_off * wordSize);
    __ st_d(S3, FP, S3_off * wordSize);
    __ st_d(S4, FP, S4_off * wordSize);
    __ st_d(S5, FP, S5_off * wordSize);
    __ st_d(S6, FP, S6_off * wordSize);
    __ st_d(A0, FP, call_wrapper_off * wordSize);
    __ st_d(A1, FP, result_off * wordSize);
    __ st_d(A2, FP, result_type_off * wordSize);
    __ st_d(A7, FP, thread_off * wordSize);
    __ st_d(S8, FP, S8_off * wordSize);

    __ li(S8, (long)Interpreter::dispatch_table(itos));

#ifdef OPT_THREAD
    __ move(TREG, A7);
#endif
    //add for compressedoops
    __ reinit_heapbase();

#ifdef ASSERT
    // make sure we have no pending exceptions
    {
      Label L;
      __ ld_d(AT, A7, in_bytes(Thread::pending_exception_offset()));
      __ beq(AT, R0, L);
      /* FIXME: I do not know how to realize stop in LA, do it in the future */
      __ stop("StubRoutines::call_stub: entered with pending exception");
      __ bind(L);
    }
#endif

    // pass parameters if any
    // A5: parameter
    // A6: parameter_size
    // T0: parameter_size_tmp(--)
    // T2: offset(++)
    // T3: tmp
    Label parameters_done;
    // judge if the parameter_size equals 0
    __ beq(A6, R0, parameters_done);
    __ slli_d(AT, A6, Interpreter::logStackElementSize);
    __ sub_d(SP, SP, AT);
    __ li(AT, -StackAlignmentInBytes);
    __ andr(SP, SP, AT);
    // Copy Java parameters in reverse order (receiver last)
    // Note that the argument order is inverted in the process
    Label loop;
    __ move(T0, A6);
    __ move(T2, R0);
    __ bind(loop);

    // get parameter
    __ alsl_d(T3, T0, A5, LogBytesPerWord - 1);
    __ ld_d(AT, T3,  -wordSize);
    __ alsl_d(T3, T2, SP, LogBytesPerWord - 1);
    __ st_d(AT, T3, Interpreter::expr_offset_in_bytes(0));
    __ addi_d(T2, T2, 1);
    __ addi_d(T0, T0, -1);
    __ bne(T0, R0, loop);
    // advance to next parameter

    // call Java function
    __ bind(parameters_done);

    // receiver in V0, methodOop in Rmethod

    __ move(Rmethod, A3);
    __ move(Rsender, SP);             //set sender sp
    __ jalr(A4);
    return_address = __ pc();

    Label common_return;
    __ bind(common_return);

    // store result depending on type
    // (everything that is not T_LONG, T_FLOAT or T_DOUBLE is treated as T_INT)
    __ ld_d(T0, FP, result_off * wordSize);   // result --> T0
    Label is_long, is_float, is_double, exit;
    __ ld_d(T2, FP, result_type_off * wordSize);  // result_type --> T2
    __ addi_d(T3, T2, (-1) * T_LONG);
    __ beq(T3, R0, is_long);
    __ addi_d(T3, T2, (-1) * T_FLOAT);
    __ beq(T3, R0, is_float);
    __ addi_d(T3, T2, (-1) * T_DOUBLE);
    __ beq(T3, R0, is_double);

    // handle T_INT case
    __ st_d(V0, T0, 0 * wordSize);
    __ bind(exit);

    // restore
    __ ld_d(BCP, FP, BCP_off * wordSize);
    __ ld_d(LVP, FP, LVP_off * wordSize);
    __ ld_d(S8, FP, S8_off * wordSize);
    __ ld_d(TSR, FP, TSR_off * wordSize);

    __ ld_d(S1, FP, S1_off * wordSize);
    __ ld_d(S3, FP, S3_off * wordSize);
    __ ld_d(S4, FP, S4_off * wordSize);
    __ ld_d(S5, FP, S5_off * wordSize);
    __ ld_d(S6, FP, S6_off * wordSize);

    __ leave();

    // return
    __ jr(RA);

    // handle return types different from T_INT
    __ bind(is_long);
    __ st_d(V0, T0, 0 * wordSize);
    __ b(exit);

    __ bind(is_float);
    __ fst_s(FV0, T0, 0 * wordSize);
    __ b(exit);

    __ bind(is_double);
    __ fst_d(FV0, T0, 0 * wordSize);
    __ b(exit);
    StubRoutines::la::set_call_stub_compiled_return(__ pc());
    __ b(common_return);
    return start;
  }

  // Return point for a Java call if there's an exception thrown in
  // Java code.  The exception is caught and transformed into a
  // pending exception stored in JavaThread that can be tested from
  // within the VM.
  //
  // Note: Usually the parameters are removed by the callee. In case
  // of an exception crossing an activation frame boundary, that is
  // not the case if the callee is compiled code => need to setup the
  // sp.
  //
  // V0: exception oop

  address generate_catch_exception() {
    StubCodeMark mark(this, "StubRoutines", "catch_exception");
    address start = __ pc();

    Register thread = TREG;

    // get thread directly
#ifndef OPT_THREAD
    __ ld_d(thread, FP, thread_off * wordSize);
#endif

#ifdef ASSERT
    // verify that threads correspond
    { Label L;
      __ get_thread(T8);
      __ beq(T8, thread, L);
      __ stop("StubRoutines::catch_exception: threads must correspond");
      __ bind(L);
    }
#endif
    // set pending exception
    __ verify_oop(V0);
    __ st_d(V0, thread, in_bytes(Thread::pending_exception_offset()));
    __ li(AT, (long)__FILE__);
    __ st_d(AT, thread, in_bytes(Thread::exception_file_offset   ()));
    __ li(AT, (long)__LINE__);
    __ st_d(AT, thread, in_bytes(Thread::exception_line_offset   ()));

    // complete return to VM
    assert(StubRoutines::_call_stub_return_address != NULL, "_call_stub_return_address must have been generated before");
    __ jmp(StubRoutines::_call_stub_return_address, relocInfo::none);
    return start;
  }

  // Continuation point for runtime calls returning with a pending
  // exception.  The pending exception check happened in the runtime
  // or native call stub.  The pending exception in Thread is
  // converted into a Java-level exception.
  //
  // Contract with Java-level exception handlers:
  // V0: exception
  // V1: throwing pc
  //
  // NOTE: At entry of this stub, exception-pc must be on stack !!

  address generate_forward_exception() {
    StubCodeMark mark(this, "StubRoutines", "forward exception");
    //Register thread = TREG;
    Register thread = TREG;
    address start = __ pc();

    // Upon entry, the sp points to the return address returning into
    // Java (interpreted or compiled) code; i.e., the return address
    // throwing pc.
    //
    // Arguments pushed before the runtime call are still on the stack
    // but the exception handler will reset the stack pointer ->
    // ignore them.  A potential result in registers can be ignored as
    // well.

#ifndef OPT_THREAD
    __ get_thread(thread);
#endif
#ifdef ASSERT
    // make sure this code is only executed if there is a pending exception
    {
      Label L;
      __ ld_d(AT, thread, in_bytes(Thread::pending_exception_offset()));
      __ bne(AT, R0, L);
      __ stop("StubRoutines::forward exception: no pending exception (1)");
      __ bind(L);
    }
#endif

    // compute exception handler into T4
    __ ld_d(A1, SP, 0);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::exception_handler_for_return_address), thread, A1);
    __ move(T4, V0);
    __ pop(V1);

#ifndef OPT_THREAD
    __ get_thread(thread);
#endif
    __ ld_d(V0, thread, in_bytes(Thread::pending_exception_offset()));
    __ st_d(R0, thread, in_bytes(Thread::pending_exception_offset()));

#ifdef ASSERT
    // make sure exception is set
    {
      Label L;
      __ bne(V0, R0, L);
      __ stop("StubRoutines::forward exception: no pending exception (2)");
      __ bind(L);
    }
#endif

    // continue at exception handler (return address removed)
    // V0: exception
    // T4: exception handler
    // V1: throwing pc
    __ verify_oop(V0);
    __ jr(T4);
    return start;
  }

  // The following routine generates a subroutine to throw an
  // asynchronous UnknownError when an unsafe access gets a fault that
  // could not be reasonably prevented by the programmer.  (Example:
  // SIGBUS/OBJERR.)
  address generate_handler_for_unsafe_access() {
    StubCodeMark mark(this, "StubRoutines", "handler_for_unsafe_access");
    address start = __ pc();
    __ push(V0);
    __ pushad_except_v0();                      // push registers
    __ call(CAST_FROM_FN_PTR(address, handle_unsafe_access), relocInfo::runtime_call_type);
    __ popad_except_v0();
    __ move(RA, V0);
    __ pop(V0);
    __ jr(RA);
    return start;
  }

  // Non-destructive plausibility checks for oops
  //
  address generate_verify_oop() {
    StubCodeMark mark(this, "StubRoutines", "verify_oop");
    address start = __ pc();
    __ reinit_heapbase();
    __ verify_oop_subroutine();
    address end = __ pc();
    return start;
  }

  //
  // Generate stub for array fill. If "aligned" is true, the
  // "to" address is assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //   to:    A0
  //   value: A1
  //   count: A2 treated as signed
  //
  address generate_fill(BasicType t, bool aligned, const char *name) {
    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ pc();

    const Register to        = A0;  // source array address
    const Register value     = A1;  // value
    const Register count     = A2;  // elements count

    const Register end       = T5;  // source array address end
    const Register tmp       = T8;  // temp register

    Label L_fill_elements;

    int shift = -1;
    switch (t) {
      case T_BYTE:
        shift = 0;
        __ slti(AT, count, 9);              // Short arrays (<= 8 bytes) fill by element
        __ bstrins_d(value, value, 15, 8);  //  8 bit -> 16 bit
        __ bstrins_d(value, value, 31, 16); // 16 bit -> 32 bit
        __ bstrins_d(value, value, 63, 32); // 32 bit -> 64 bit
        __ bnez(AT, L_fill_elements);
        break;
      case T_SHORT:
        shift = 1;
        __ slti(AT, count, 5);              // Short arrays (<= 8 bytes) fill by element
        __ bstrins_d(value, value, 31, 16); // 16 bit -> 32 bit
        __ bstrins_d(value, value, 63, 32); // 32 bit -> 64 bit
        __ bnez(AT, L_fill_elements);
        break;
      case T_INT:
        shift = 2;
        __ slti(AT, count, 3);              // Short arrays (<= 8 bytes) fill by element
        __ bstrins_d(value, value, 63, 32); // 32 bit -> 64 bit
        __ bnez(AT, L_fill_elements);
        break;
      default: ShouldNotReachHere();
    }

    switch (t) {
      case T_BYTE:
        __ add_d(end, to, count);
        break;
      case T_SHORT:
      case T_INT:
        __ alsl_d(end, count, to, shift-1);
        break;
      default: ShouldNotReachHere();
    }
    if (!aligned) {
      __ st_d(value, to,  0);
      __ bstrins_d(to, R0, 2, 0);
      __ addi_d(to, to, 8);
    }
    __ st_d(value, end, -8);
    __ bstrins_d(end, R0, 2, 0);

    //
    //  Fill large chunks
    //
    Label L_loop_begin, L_not_64bytes_fill, L_loop_end;
    __ addi_d(AT, to, 64);
    __ blt(end, AT, L_not_64bytes_fill);
    __ addi_d(to, to, 64);
    __ bind(L_loop_begin);
    __ st_d(value, to,  -8);
    __ st_d(value, to, -16);
    __ st_d(value, to, -24);
    __ st_d(value, to, -32);
    __ st_d(value, to, -40);
    __ st_d(value, to, -48);
    __ st_d(value, to, -56);
    __ st_d(value, to, -64);
    __ addi_d(to, to, 64);
    __ bge(end, to, L_loop_begin);
    __ addi_d(to, to, -64);
    __ beq(to, end, L_loop_end);

    __ bind(L_not_64bytes_fill);
    // There are 0 - 7 words
    __ pcaddi(AT, 4);
    __ sub_d(tmp, end, to);
    __ alsl_d(AT, tmp, AT, 1);
    __ jr(AT);

    // 0:
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 1:
    __ st_d(value, to, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 2:
    __ st_d(value, to, 0);
    __ st_d(value, to, 8);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 3:
    __ st_d(value, to,  0);
    __ st_d(value, to,  8);
    __ st_d(value, to, 16);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 4:
    __ st_d(value, to,  0);
    __ st_d(value, to,  8);
    __ st_d(value, to, 16);
    __ st_d(value, to, 24);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 5:
    __ st_d(value, to,  0);
    __ st_d(value, to,  8);
    __ st_d(value, to, 16);
    __ st_d(value, to, 24);
    __ st_d(value, to, 32);
    __ jr(RA);
    __ nop();
    __ nop();

    // 6:
    __ st_d(value, to,  0);
    __ st_d(value, to,  8);
    __ st_d(value, to, 16);
    __ st_d(value, to, 24);
    __ st_d(value, to, 32);
    __ st_d(value, to, 40);
    __ jr(RA);
    __ nop();

    // 7:
    __ st_d(value, to,  0);
    __ st_d(value, to,  8);
    __ st_d(value, to, 16);
    __ st_d(value, to, 24);
    __ st_d(value, to, 32);
    __ st_d(value, to, 40);
    __ st_d(value, to, 48);

    __ bind(L_loop_end);
    __ jr(RA);

    // Short arrays (<= 8 bytes)
    __ bind(L_fill_elements);
    __ pcaddi(AT, 4);
    __ slli_d(tmp, count, 4 + shift);
    __ add_d(AT, AT, tmp);
    __ jr(AT);

    // 0:
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 1:
    __ st_b(value, to, 0);
    __ jr(RA);
    __ nop();
    __ nop();

    // 2:
    __ st_h(value, to, 0);
    __ jr(RA);
    __ nop();
    __ nop();

    // 3:
    __ st_h(value, to, 0);
    __ st_b(value, to, 2);
    __ jr(RA);
    __ nop();

    // 4:
    __ st_w(value, to, 0);
    __ jr(RA);
    __ nop();
    __ nop();

    // 5:
    __ st_w(value, to, 0);
    __ st_b(value, to, 4);
    __ jr(RA);
    __ nop();

    // 6:
    __ st_w(value, to, 0);
    __ st_h(value, to, 4);
    __ jr(RA);
    __ nop();

    // 7:
    __ st_w(value, to, 0);
    __ st_w(value, to, 3);
    __ jr(RA);
    __ nop();

    // 8:
    __ st_d(value, to, 0);
    __ jr(RA);
    return start;
  }

  //
  //  Generate overlap test for array copy stubs
  //
  //  Input:
  //    A0   - source array address
  //    A1   - destination array address
  //    A2   - element count
  //
  //  Temp:
  //    AT   - destination array address - source array address
  //    T4   - element count * element size
  //
  void array_overlap_test(address no_overlap_target, int log2_elem_size) {
    __ slli_d(T4, A2, log2_elem_size);
    __ sub_d(AT, A1, A0);
    __ bgeu(AT, T4, no_overlap_target);
  }

  // Generate code for an array write pre barrier
  //
  //   Input:
  //     addr    -  starting address
  //     count   -  element count
  //
  //  Temp:
  //     AT      -  used to swap addr and count
  //
  void gen_write_ref_array_pre_barrier(Register addr, Register count, bool dest_uninitialized) {
    BarrierSet* bs = Universe::heap()->barrier_set();
    switch (bs->kind()) {
      case BarrierSet::G1SATBCT:
      case BarrierSet::G1SATBCTLogging:
        // With G1, don't generate the call if we statically know that the target in uninitialized
        if (!dest_uninitialized) {
           if (count == A0) {
             if (addr == A1) {
               // exactly backwards!!
               __ move(AT, A0);
               __ move(A0, A1);
               __ move(A1, AT);
             } else {
               __ move(A1, count);
               __ move(A0, addr);
             }
           } else {
             __ move(A0, addr);
             __ move(A1, count);
           }
           __ call_VM_leaf(CAST_FROM_FN_PTR(address, BarrierSet::static_write_ref_array_pre), 2);
        }
        break;
      case BarrierSet::CardTableModRef:
      case BarrierSet::CardTableExtension:
      case BarrierSet::ModRef:
        break;
      default:
        ShouldNotReachHere();
    }
  }

  //
  // Generate code for an array write post barrier
  //
  //  Input:
  //     start    - register containing starting address of destination array
  //     count    - elements count
  //     scratch  - scratch register
  //
  //  Temp:
  //     AT       - used to swap addr and count
  //
  //  The input registers are overwritten.
  //
  void gen_write_ref_array_post_barrier(Register start, Register count, Register scratch) {
    assert_different_registers(start, count, scratch, AT);
    BarrierSet* bs = Universe::heap()->barrier_set();
    switch (bs->kind()) {
      case BarrierSet::G1SATBCT:
      case BarrierSet::G1SATBCTLogging:
        {
          if (count == A0) {
            if (start == A1) {
              // exactly backwards!!
              __ move(AT, A0);
              __ move(A0, A1);
              __ move(A1, AT);
            } else {
              __ move(A1, count);
              __ move(A0, start);
            }
          } else {
            __ move(A0, start);
            __ move(A1, count);
          }
          __ call_VM_leaf(CAST_FROM_FN_PTR(address, BarrierSet::static_write_ref_array_post), 2);
        }
        break;
      case BarrierSet::CardTableModRef:
      case BarrierSet::CardTableExtension:
        {
          CardTableModRefBS* ct = (CardTableModRefBS*)bs;
          assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");

          Label L_loop;
          const Register end = count;

          if (UseConcMarkSweepGC) {
            __ membar(__ StoreLoad);
          }

          int64_t disp = (int64_t) ct->byte_map_base;
          __ li(scratch, disp);

          __ lea(end, Address(start, count, TIMES_OOP, 0));  // end == start + count * oop_size
          __ addi_d(end, end, -BytesPerHeapOop); // end - 1 to make inclusive
          __ shr(start, CardTableModRefBS::card_shift);
          __ shr(end,   CardTableModRefBS::card_shift);
          __ sub_d(end, end, start); // end --> cards count

          __ add_d(start, start, scratch);

          __ bind(L_loop);
          __ stx_b(R0, start, count);
          __ addi_d(count, count, -1);
          __ bge(count, R0, L_loop);
        }
        break;
      default:
        ShouldNotReachHere();
    }
  }

  // disjoint large copy
  void generate_disjoint_large_copy(Label &entry, const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);

    Label loop, le32, le16, le8, lt8;

    __ bind(entry);
    __ add_d(A3, A1, A2);
    __ add_d(A2, A0, A2);
    __ ld_d(A6, A0, 0);
    __ ld_d(A7, A2, -8);

    __ andi(T1, A0, 7);
    __ sub_d(T0, R0, T1);
    __ addi_d(T0, T0, 8);

    __ add_d(A0, A0, T0);
    __ add_d(A5, A1, T0);

    __ addi_d(A4, A2, -64);
    __ bgeu(A0, A4, le32);

    __ bind(loop);
    __ ld_d(T0, A0, 0);
    __ ld_d(T1, A0, 8);
    __ ld_d(T2, A0, 16);
    __ ld_d(T3, A0, 24);
    __ ld_d(T4, A0, 32);
    __ ld_d(T5, A0, 40);
    __ ld_d(T6, A0, 48);
    __ ld_d(T7, A0, 56);
    __ addi_d(A0, A0, 64);
    __ st_d(T0, A5, 0);
    __ st_d(T1, A5, 8);
    __ st_d(T2, A5, 16);
    __ st_d(T3, A5, 24);
    __ st_d(T4, A5, 32);
    __ st_d(T5, A5, 40);
    __ st_d(T6, A5, 48);
    __ st_d(T7, A5, 56);
    __ addi_d(A5, A5, 64);
    __ bltu(A0, A4, loop);

    __ bind(le32);
    __ addi_d(A4, A2, -32);
    __ bgeu(A0, A4, le16);
    __ ld_d(T0, A0, 0);
    __ ld_d(T1, A0, 8);
    __ ld_d(T2, A0, 16);
    __ ld_d(T3, A0, 24);
    __ addi_d(A0, A0, 32);
    __ st_d(T0, A5, 0);
    __ st_d(T1, A5, 8);
    __ st_d(T2, A5, 16);
    __ st_d(T3, A5, 24);
    __ addi_d(A5, A5, 32);

    __ bind(le16);
    __ addi_d(A4, A2, -16);
    __ bgeu(A0, A4, le8);
    __ ld_d(T0, A0, 0);
    __ ld_d(T1, A0, 8);
    __ addi_d(A0, A0, 16);
    __ st_d(T0, A5, 0);
    __ st_d(T1, A5, 8);
    __ addi_d(A5, A5, 16);

    __ bind(le8);
    __ addi_d(A4, A2, -8);
    __ bgeu(A0, A4, lt8);
    __ ld_d(T0, A0, 0);
    __ st_d(T0, A5, 0);

    __ bind(lt8);
    __ st_d(A6, A1, 0);
    __ st_d(A7, A3, -8);
    __ jr(RA);
  }

  // conjoint large copy
  void generate_conjoint_large_copy(Label &entry, const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);

    Label loop, le32, le16, le8, lt8;

    __ bind(entry);
    __ add_d(A3, A1, A2);
    __ add_d(A2, A0, A2);
    __ ld_d(A6, A0, 0);
    __ ld_d(A7, A2, -8);

    __ andi(T1, A0, 7);
    __ sub_d(A2, A2, T1);
    __ sub_d(A5, A3, T1);

    __ addi_d(A4, A0, 64);
    __ bgeu(A4, A2, le32);

    __ bind(loop);
    __ ld_d(T0, A2, -8);
    __ ld_d(T1, A2, -16);
    __ ld_d(T2, A2, -24);
    __ ld_d(T3, A2, -32);
    __ ld_d(T4, A2, -40);
    __ ld_d(T5, A2, -48);
    __ ld_d(T6, A2, -56);
    __ ld_d(T7, A2, -64);
    __ addi_d(A2, A2, -64);
    __ st_d(T0, A5, -8);
    __ st_d(T1, A5, -16);
    __ st_d(T2, A5, -24);
    __ st_d(T3, A5, -32);
    __ st_d(T4, A5, -40);
    __ st_d(T5, A5, -48);
    __ st_d(T6, A5, -56);
    __ st_d(T7, A5, -64);
    __ addi_d(A5, A5, -64);
    __ bltu(A4, A2, loop);

    __ bind(le32);
    __ addi_d(A4, A0, 32);
    __ bgeu(A4, A2, le16);
    __ ld_d(T0, A2, -8);
    __ ld_d(T1, A2, -16);
    __ ld_d(T2, A2, -24);
    __ ld_d(T3, A2, -32);
    __ addi_d(A2, A2, -32);
    __ st_d(T0, A5, -8);
    __ st_d(T1, A5, -16);
    __ st_d(T2, A5, -24);
    __ st_d(T3, A5, -32);
    __ addi_d(A5, A5, -32);

    __ bind(le16);
    __ addi_d(A4, A0, 16);
    __ bgeu(A4, A2, le8);
    __ ld_d(T0, A2, -8);
    __ ld_d(T1, A2, -16);
    __ addi_d(A2, A2, -16);
    __ st_d(T0, A5, -8);
    __ st_d(T1, A5, -16);
    __ addi_d(A5, A5, -16);

    __ bind(le8);
    __ addi_d(A4, A0, 8);
    __ bgeu(A4, A2, lt8);
    __ ld_d(T0, A2, -8);
    __ st_d(T0, A5, -8);

    __ bind(lt8);
    __ st_d(A6, A1, 0);
    __ st_d(A7, A3, -8);
    __ jr(RA);
  }

  // Byte small copy: less than 9 elements.
  void generate_byte_small_copy(Label &entry, const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);

    Label L;
    __ bind(entry);
    __ lipc(AT, L);
    __ slli_d(A2, A2, 5);
    __ add_d(AT, AT, A2);
    __ jr(AT);

    __ bind(L);
    // 0:
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 1:
    __ ld_b(AT, A0, 0);
    __ st_b(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 2:
    __ ld_h(AT, A0, 0);
    __ st_h(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 3:
    __ ld_h(AT, A0, 0);
    __ ld_b(A2, A0, 2);
    __ st_h(AT, A1, 0);
    __ st_b(A2, A1, 2);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 4:
    __ ld_w(AT, A0, 0);
    __ st_w(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 5:
    __ ld_w(AT, A0, 0);
    __ ld_b(A2, A0, 4);
    __ st_w(AT, A1, 0);
    __ st_b(A2, A1, 4);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 6:
    __ ld_w(AT, A0, 0);
    __ ld_h(A2, A0, 4);
    __ st_w(AT, A1, 0);
    __ st_h(A2, A1, 4);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 7:
    __ ld_w(AT, A0, 0);
    __ ld_w(A2, A0, 3);
    __ st_w(AT, A1, 0);
    __ st_w(A2, A1, 3);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 8:
    __ ld_d(AT, A0, 0);
    __ st_d(AT, A1, 0);
    __ jr(RA);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   A0      - source array address
  //   A1      - destination array address
  //   A2      - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  // Side Effects:
  //   disjoint_byte_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_byte_copy().
  //
  address generate_disjoint_byte_copy(bool aligned, Label &small, Label &large,
                                      const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    __ sltui(T0, A2, 9);
    __ bnez(T0, small);

    __ b(large);

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   A0      - source array address
  //   A1      - destination array address
  //   A2      - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  address generate_conjoint_byte_copy(bool aligned, Label &small, Label &large,
                                      const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    array_overlap_test(StubRoutines::jbyte_disjoint_arraycopy(), 0);

    __ sltui(T0, A2, 9);
    __ bnez(T0, small);

    __ b(large);

    return start;
  }

  // Short small copy: less than 9 elements.
  void generate_short_small_copy(Label &entry, const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);

    Label L;
    __ bind(entry);
    __ lipc(AT, L);
    __ slli_d(A2, A2, 5);
    __ add_d(AT, AT, A2);
    __ jr(AT);

    __ bind(L);
    // 0:
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 1:
    __ ld_h(AT, A0, 0);
    __ st_h(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 2:
    __ ld_w(AT, A0, 0);
    __ st_w(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 3:
    __ ld_w(AT, A0, 0);
    __ ld_h(A2, A0, 4);
    __ st_w(AT, A1, 0);
    __ st_h(A2, A1, 4);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 4:
    __ ld_d(AT, A0, 0);
    __ st_d(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 5:
    __ ld_d(AT, A0, 0);
    __ ld_h(A2, A0, 8);
    __ st_d(AT, A1, 0);
    __ st_h(A2, A1, 8);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 6:
    __ ld_d(AT, A0, 0);
    __ ld_w(A2, A0, 8);
    __ st_d(AT, A1, 0);
    __ st_w(A2, A1, 8);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 7:
    __ ld_d(AT, A0, 0);
    __ ld_d(A2, A0, 6);
    __ st_d(AT, A1, 0);
    __ st_d(A2, A1, 6);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 8:
    __ ld_d(AT, A0, 0);
    __ ld_d(A2, A0, 8);
    __ st_d(AT, A1, 0);
    __ st_d(A2, A1, 8);
    __ jr(RA);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   A0      - source array address
  //   A1      - destination array address
  //   A2      - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  // Side Effects:
  //   disjoint_short_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_short_copy().
  //
  address generate_disjoint_short_copy(bool aligned, Label &small, Label &large,
                                       const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    __ sltui(T0, A2, 9);
    __ bnez(T0, small);

    __ slli_d(A2, A2, 1);
    __ b(large);

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   A0      - source array address
  //   A1      - destination array address
  //   A2      - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4- or 2-byte boundaries, we
  // let the hardware handle it.  The two or four words within dwords
  // or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  address generate_conjoint_short_copy(bool aligned, Label &small, Label &large,
                                       const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    array_overlap_test(StubRoutines::jshort_disjoint_arraycopy(), 1);

    __ sltui(T0, A2, 9);
    __ bnez(T0, small);

    __ slli_d(A2, A2, 1);
    __ b(large);

    return start;
  }

  // Short small copy: less than 7 elements.
  void generate_int_small_copy(Label &entry, const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);

    Label L;
    __ bind(entry);
    __ lipc(AT, L);
    __ slli_d(A2, A2, 5);
    __ add_d(AT, AT, A2);
    __ jr(AT);

    __ bind(L);
    // 0:
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 1:
    __ ld_w(AT, A0, 0);
    __ st_w(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 2:
    __ ld_d(AT, A0, 0);
    __ st_d(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 3:
    __ ld_d(AT, A0, 0);
    __ ld_w(A2, A0, 8);
    __ st_d(AT, A1, 0);
    __ st_w(A2, A1, 8);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 4:
    __ ld_d(AT, A0, 0);
    __ ld_d(A2, A0, 8);
    __ st_d(AT, A1, 0);
    __ st_d(A2, A1, 8);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 5:
    __ ld_d(AT, A0, 0);
    __ ld_d(A2, A0, 8);
    __ ld_w(A3, A0, 16);
    __ st_d(AT, A1, 0);
    __ st_d(A2, A1, 8);
    __ st_w(A3, A1, 16);
    __ jr(RA);
    __ nop();

    // 6:
    __ ld_d(AT, A0, 0);
    __ ld_d(A2, A0, 8);
    __ ld_d(A3, A0, 16);
    __ st_d(AT, A1, 0);
    __ st_d(A2, A1, 8);
    __ st_d(A3, A1, 16);
    __ jr(RA);
  }

  // Generate maybe oop copy
  void gen_maybe_oop_copy(bool is_oop, Label &small, Label &large,
                          const char *name, int small_limit, int log2_elem_size,
                          bool dest_uninitialized = false) {
    Label post, _large;

    if (is_oop) {
      __ addi_d(SP, SP, -4 * wordSize);
      __ st_d(A2, SP, 3 * wordSize);
      __ st_d(A1, SP, 2 * wordSize);
      __ st_d(A0, SP, 1 * wordSize);
      __ st_d(RA, SP, 0 * wordSize);

      gen_write_ref_array_pre_barrier(A1, A2, dest_uninitialized);

      __ ld_d(A2, SP, 3 * wordSize);
      __ ld_d(A1, SP, 2 * wordSize);
      __ ld_d(A0, SP, 1 * wordSize);
    }

    __ sltui(T0, A2, small_limit);
    if (is_oop) {
      __ beqz(T0, _large);
      __ bl(small);
      __ b(post);
    } else {
      __ bnez(T0, small);
    }

    __ bind(_large);
    __ slli_d(A2, A2, log2_elem_size);

    if (is_oop) {
      __ bl(large);
    } else {
      __ b(large);
    }

    if (is_oop) {
      __ bind(post);
      __ ld_d(A2, SP, 3 * wordSize);
      __ ld_d(A1, SP, 2 * wordSize);

      gen_write_ref_array_post_barrier(A1, A2, T1);

      __ ld_d(RA, SP, 0 * wordSize);
      __ addi_d(SP, SP, 4 * wordSize);
      __ jr(RA);
    }
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   A0      - source array address
  //   A1      - destination array address
  //   A2      - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  // Side Effects:
  //   disjoint_int_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_int_oop_copy().
  //
  address generate_disjoint_int_oop_copy(bool aligned, bool is_oop, Label &small,
                                         Label &large, const char *name,
                                         bool dest_uninitialized = false) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    gen_maybe_oop_copy(is_oop, small, large, name, 7, 2, dest_uninitialized);

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   A0      - source array address
  //   A1      - destination array address
  //   A2      - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  address generate_conjoint_int_oop_copy(bool aligned, bool is_oop,
                                         Label &small, Label &large, const char *name,
                                         bool dest_uninitialized = false) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    if (is_oop) {
      array_overlap_test(StubRoutines::oop_disjoint_arraycopy(), 2);
    } else {
      array_overlap_test(StubRoutines::jint_disjoint_arraycopy(), 2);
    }

    gen_maybe_oop_copy(is_oop, small, large, name, 7, 2, dest_uninitialized);

    return start;
  }

  // Long small copy: less than 4 elements.
  void generate_long_small_copy(Label &entry, const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);

    Label L;
    __ bind(entry);
    __ lipc(AT, L);
    __ slli_d(A2, A2, 5);
    __ add_d(AT, AT, A2);
    __ jr(AT);

    __ bind(L);
    // 0:
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 1:
    __ ld_d(AT, A0, 0);
    __ st_d(AT, A1, 0);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    // 2:
    __ ld_d(AT, A0, 0);
    __ ld_d(A2, A0, 8);
    __ st_d(AT, A1, 0);
    __ st_d(A2, A1, 8);
    __ jr(RA);
    __ nop();
    __ nop();
    __ nop();

    // 3:
    __ ld_d(AT, A0, 0);
    __ ld_d(A2, A0, 8);
    __ ld_d(A3, A0, 16);
    __ st_d(AT, A1, 0);
    __ st_d(A2, A1, 8);
    __ st_d(A3, A1, 16);
    __ jr(RA);
    __ nop();
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   A0      - source array address
  //   A1      - destination array address
  //   A2      - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  // Side Effects:
  //   disjoint_int_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_int_oop_copy().
  //
  address generate_disjoint_long_oop_copy(bool aligned, bool is_oop, Label &small,
                                          Label &large, const char *name,
                                          bool dest_uninitialized = false) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    gen_maybe_oop_copy(is_oop, small, large, name, 4, 3, dest_uninitialized);

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   A0      - source array address
  //   A1      - destination array address
  //   A2      - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  address generate_conjoint_long_oop_copy(bool aligned, bool is_oop, Label &small,
                                          Label &large, const char *name,
                                          bool dest_uninitialized = false) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    if (is_oop) {
      array_overlap_test(StubRoutines::oop_disjoint_arraycopy(), 3);
    } else {
      array_overlap_test(StubRoutines::jlong_disjoint_arraycopy(), 3);
    }

    gen_maybe_oop_copy(is_oop, small, large, name, 4, 3, dest_uninitialized);

    return start;
  }

  void generate_arraycopy_stubs() {
    Label disjoint_large_copy, conjoint_large_copy;
    Label byte_small_copy, short_small_copy, int_small_copy, long_small_copy;

    generate_disjoint_large_copy(disjoint_large_copy, "disjoint_large_copy");
    generate_conjoint_large_copy(conjoint_large_copy, "conjoint_large_copy");
    generate_byte_small_copy(byte_small_copy, "jbyte_small_copy");
    generate_short_small_copy(short_small_copy, "jshort_small_copy");
    generate_int_small_copy(int_small_copy, "jint_small_copy");
    generate_long_small_copy(long_small_copy, "jlong_small_copy");

    if (UseCompressedOops) {
      StubRoutines::_oop_disjoint_arraycopy          = generate_disjoint_int_oop_copy(false, true, int_small_copy, disjoint_large_copy, "oop_disjoint_arraycopy");
      StubRoutines::_oop_arraycopy                   = generate_conjoint_int_oop_copy(false, true, int_small_copy, conjoint_large_copy, "oop_arraycopy");
      StubRoutines::_oop_disjoint_arraycopy_uninit   = generate_disjoint_int_oop_copy(false, true, int_small_copy, disjoint_large_copy, "oop_disjoint_arraycopy_uninit", true);
      StubRoutines::_oop_arraycopy_uninit            = generate_conjoint_int_oop_copy(false, true, int_small_copy, conjoint_large_copy, "oop_arraycopy_uninit", true);
    } else {
      StubRoutines::_oop_disjoint_arraycopy          = generate_disjoint_long_oop_copy(false, true, long_small_copy, disjoint_large_copy, "oop_disjoint_arraycopy");
      StubRoutines::_oop_arraycopy                   = generate_conjoint_long_oop_copy(false, true, long_small_copy, conjoint_large_copy, "oop_arraycopy");
      StubRoutines::_oop_disjoint_arraycopy_uninit   = generate_disjoint_long_oop_copy(false, true, long_small_copy, disjoint_large_copy, "oop_disjoint_arraycopy_uninit", true);
      StubRoutines::_oop_arraycopy_uninit            = generate_conjoint_long_oop_copy(false, true, long_small_copy, conjoint_large_copy, "oop_arraycopy_uninit", true);
    }

    StubRoutines::_jbyte_disjoint_arraycopy          = generate_disjoint_byte_copy(false, byte_small_copy, disjoint_large_copy, "jbyte_disjoint_arraycopy");
    StubRoutines::_jshort_disjoint_arraycopy         = generate_disjoint_short_copy(false, short_small_copy, disjoint_large_copy, "jshort_disjoint_arraycopy");
    StubRoutines::_jint_disjoint_arraycopy           = generate_disjoint_int_oop_copy(false, false, int_small_copy, disjoint_large_copy, "jint_disjoint_arraycopy");
    StubRoutines::_jlong_disjoint_arraycopy          = generate_disjoint_long_oop_copy(false, false, long_small_copy, disjoint_large_copy, "jlong_disjoint_arraycopy", false);

    StubRoutines::_jbyte_arraycopy  = generate_conjoint_byte_copy(false, byte_small_copy, conjoint_large_copy, "jbyte_arraycopy");
    StubRoutines::_jshort_arraycopy = generate_conjoint_short_copy(false, short_small_copy, conjoint_large_copy, "jshort_arraycopy");
    StubRoutines::_jint_arraycopy   = generate_conjoint_int_oop_copy(false, false, int_small_copy, conjoint_large_copy, "jint_arraycopy");
    StubRoutines::_jlong_arraycopy  = generate_conjoint_long_oop_copy(false, false, long_small_copy, conjoint_large_copy, "jlong_arraycopy", false);

    // We don't generate specialized code for HeapWord-aligned source
    // arrays, so just use the code we've already generated
    StubRoutines::_arrayof_jbyte_disjoint_arraycopy  = StubRoutines::_jbyte_disjoint_arraycopy;
    StubRoutines::_arrayof_jbyte_arraycopy           = StubRoutines::_jbyte_arraycopy;

    StubRoutines::_arrayof_jshort_disjoint_arraycopy = StubRoutines::_jshort_disjoint_arraycopy;
    StubRoutines::_arrayof_jshort_arraycopy          = StubRoutines::_jshort_arraycopy;

    StubRoutines::_arrayof_jint_disjoint_arraycopy   = StubRoutines::_jint_disjoint_arraycopy;
    StubRoutines::_arrayof_jint_arraycopy            = StubRoutines::_jint_arraycopy;

    StubRoutines::_arrayof_jlong_disjoint_arraycopy  = StubRoutines::_jlong_disjoint_arraycopy;
    StubRoutines::_arrayof_jlong_arraycopy           = StubRoutines::_jlong_arraycopy;

    StubRoutines::_arrayof_oop_disjoint_arraycopy    = StubRoutines::_oop_disjoint_arraycopy;
    StubRoutines::_arrayof_oop_arraycopy             = StubRoutines::_oop_arraycopy;

    StubRoutines::_arrayof_oop_disjoint_arraycopy_uninit    = StubRoutines::_oop_disjoint_arraycopy_uninit;
    StubRoutines::_arrayof_oop_arraycopy_uninit             = StubRoutines::_oop_arraycopy_uninit;

    StubRoutines::_jbyte_fill = generate_fill(T_BYTE, false, "jbyte_fill");
    StubRoutines::_jshort_fill = generate_fill(T_SHORT, false, "jshort_fill");
    StubRoutines::_jint_fill = generate_fill(T_INT, false, "jint_fill");
    StubRoutines::_arrayof_jbyte_fill = generate_fill(T_BYTE, true, "arrayof_jbyte_fill");
    StubRoutines::_arrayof_jshort_fill = generate_fill(T_SHORT, true, "arrayof_jshort_fill");
    StubRoutines::_arrayof_jint_fill = generate_fill(T_INT, true, "arrayof_jint_fill");
  }

  // Arguments:
  //
  // Inputs:
  //   A0        - source byte array address
  //   A1        - destination byte array address
  //   A2        - K (key) in little endian int array
  //   A3        - r vector byte array address
  //   A4        - input length
  //
  // Output:
  //   A0        - input length
  //
  address generate_aescrypt_encryptBlock(bool cbc) {
    static const uint32_t ft_consts[256] = {
      0xc66363a5, 0xf87c7c84, 0xee777799, 0xf67b7b8d,
      0xfff2f20d, 0xd66b6bbd, 0xde6f6fb1, 0x91c5c554,
      0x60303050, 0x02010103, 0xce6767a9, 0x562b2b7d,
      0xe7fefe19, 0xb5d7d762, 0x4dababe6, 0xec76769a,
      0x8fcaca45, 0x1f82829d, 0x89c9c940, 0xfa7d7d87,
      0xeffafa15, 0xb25959eb, 0x8e4747c9, 0xfbf0f00b,
      0x41adadec, 0xb3d4d467, 0x5fa2a2fd, 0x45afafea,
      0x239c9cbf, 0x53a4a4f7, 0xe4727296, 0x9bc0c05b,
      0x75b7b7c2, 0xe1fdfd1c, 0x3d9393ae, 0x4c26266a,
      0x6c36365a, 0x7e3f3f41, 0xf5f7f702, 0x83cccc4f,
      0x6834345c, 0x51a5a5f4, 0xd1e5e534, 0xf9f1f108,
      0xe2717193, 0xabd8d873, 0x62313153, 0x2a15153f,
      0x0804040c, 0x95c7c752, 0x46232365, 0x9dc3c35e,
      0x30181828, 0x379696a1, 0x0a05050f, 0x2f9a9ab5,
      0x0e070709, 0x24121236, 0x1b80809b, 0xdfe2e23d,
      0xcdebeb26, 0x4e272769, 0x7fb2b2cd, 0xea75759f,
      0x1209091b, 0x1d83839e, 0x582c2c74, 0x341a1a2e,
      0x361b1b2d, 0xdc6e6eb2, 0xb45a5aee, 0x5ba0a0fb,
      0xa45252f6, 0x763b3b4d, 0xb7d6d661, 0x7db3b3ce,
      0x5229297b, 0xdde3e33e, 0x5e2f2f71, 0x13848497,
      0xa65353f5, 0xb9d1d168, 0x00000000, 0xc1eded2c,
      0x40202060, 0xe3fcfc1f, 0x79b1b1c8, 0xb65b5bed,
      0xd46a6abe, 0x8dcbcb46, 0x67bebed9, 0x7239394b,
      0x944a4ade, 0x984c4cd4, 0xb05858e8, 0x85cfcf4a,
      0xbbd0d06b, 0xc5efef2a, 0x4faaaae5, 0xedfbfb16,
      0x864343c5, 0x9a4d4dd7, 0x66333355, 0x11858594,
      0x8a4545cf, 0xe9f9f910, 0x04020206, 0xfe7f7f81,
      0xa05050f0, 0x783c3c44, 0x259f9fba, 0x4ba8a8e3,
      0xa25151f3, 0x5da3a3fe, 0x804040c0, 0x058f8f8a,
      0x3f9292ad, 0x219d9dbc, 0x70383848, 0xf1f5f504,
      0x63bcbcdf, 0x77b6b6c1, 0xafdada75, 0x42212163,
      0x20101030, 0xe5ffff1a, 0xfdf3f30e, 0xbfd2d26d,
      0x81cdcd4c, 0x180c0c14, 0x26131335, 0xc3ecec2f,
      0xbe5f5fe1, 0x359797a2, 0x884444cc, 0x2e171739,
      0x93c4c457, 0x55a7a7f2, 0xfc7e7e82, 0x7a3d3d47,
      0xc86464ac, 0xba5d5de7, 0x3219192b, 0xe6737395,
      0xc06060a0, 0x19818198, 0x9e4f4fd1, 0xa3dcdc7f,
      0x44222266, 0x542a2a7e, 0x3b9090ab, 0x0b888883,
      0x8c4646ca, 0xc7eeee29, 0x6bb8b8d3, 0x2814143c,
      0xa7dede79, 0xbc5e5ee2, 0x160b0b1d, 0xaddbdb76,
      0xdbe0e03b, 0x64323256, 0x743a3a4e, 0x140a0a1e,
      0x924949db, 0x0c06060a, 0x4824246c, 0xb85c5ce4,
      0x9fc2c25d, 0xbdd3d36e, 0x43acacef, 0xc46262a6,
      0x399191a8, 0x319595a4, 0xd3e4e437, 0xf279798b,
      0xd5e7e732, 0x8bc8c843, 0x6e373759, 0xda6d6db7,
      0x018d8d8c, 0xb1d5d564, 0x9c4e4ed2, 0x49a9a9e0,
      0xd86c6cb4, 0xac5656fa, 0xf3f4f407, 0xcfeaea25,
      0xca6565af, 0xf47a7a8e, 0x47aeaee9, 0x10080818,
      0x6fbabad5, 0xf0787888, 0x4a25256f, 0x5c2e2e72,
      0x381c1c24, 0x57a6a6f1, 0x73b4b4c7, 0x97c6c651,
      0xcbe8e823, 0xa1dddd7c, 0xe874749c, 0x3e1f1f21,
      0x964b4bdd, 0x61bdbddc, 0x0d8b8b86, 0x0f8a8a85,
      0xe0707090, 0x7c3e3e42, 0x71b5b5c4, 0xcc6666aa,
      0x904848d8, 0x06030305, 0xf7f6f601, 0x1c0e0e12,
      0xc26161a3, 0x6a35355f, 0xae5757f9, 0x69b9b9d0,
      0x17868691, 0x99c1c158, 0x3a1d1d27, 0x279e9eb9,
      0xd9e1e138, 0xebf8f813, 0x2b9898b3, 0x22111133,
      0xd26969bb, 0xa9d9d970, 0x078e8e89, 0x339494a7,
      0x2d9b9bb6, 0x3c1e1e22, 0x15878792, 0xc9e9e920,
      0x87cece49, 0xaa5555ff, 0x50282878, 0xa5dfdf7a,
      0x038c8c8f, 0x59a1a1f8, 0x09898980, 0x1a0d0d17,
      0x65bfbfda, 0xd7e6e631, 0x844242c6, 0xd06868b8,
      0x824141c3, 0x299999b0, 0x5a2d2d77, 0x1e0f0f11,
      0x7bb0b0cb, 0xa85454fc, 0x6dbbbbd6, 0x2c16163a
    };
    static const uint8_t fsb_consts[256] = {
      0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
      0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
      0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
      0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
      0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
      0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
      0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
      0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
      0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
      0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
      0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
      0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
      0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
      0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
      0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
      0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
      0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
      0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
      0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
      0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
      0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
      0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
      0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
      0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
      0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
      0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
      0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
      0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
      0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
      0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
      0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
      0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
    };

    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", "aescrypt_encryptBlock");

    // Allocate registers
    Register src = A0;
    Register dst = A1;
    Register key = A2;
    Register rve = A3;
    Register srclen = A4;
    Register keylen = T8;
    Register srcend = A5;
    Register keyold = A6;
    Register t0 = A7;
    Register t1, t2, t3, ftp;
    Register xa[4] = { T0, T1, T2, T3 };
    Register ya[4] = { T4, T5, T6, T7 };

    Label loop, tail, done;
    address start = __ pc();

    if (cbc) {
      t1 = S0;
      t2 = S1;
      t3 = S2;
      ftp = S3;

      __ beqz(srclen, done);

      __ addi_d(SP, SP, -4 * wordSize);
      __ st_d(S3, SP, 3 * wordSize);
      __ st_d(S2, SP, 2 * wordSize);
      __ st_d(S1, SP, 1 * wordSize);
      __ st_d(S0, SP, 0 * wordSize);

      __ add_d(srcend, src, srclen);
      __ move(keyold, key);
    } else {
      t1 = A3;
      t2 = A4;
      t3 = A5;
      ftp = A6;
    }

    __ ld_w(keylen, key, arrayOopDesc::length_offset_in_bytes() - arrayOopDesc::base_offset_in_bytes(T_INT));

    // Round 1
    if (cbc) {
      for (int i = 0; i < 4; i++) {
        __ ld_w(xa[i], rve, 4 * i);
      }

      __ bind(loop);

      for (int i = 0; i < 4; i++) {
        __ ld_w(ya[i], src, 4 * i);
      }
      for (int i = 0; i < 4; i++) {
        __ XOR(xa[i], xa[i], ya[i]);
      }
    } else {
      for (int i = 0; i < 4; i++) {
        __ ld_w(xa[i], src, 4 * i);
      }
    }
    for (int i = 0; i < 4; i++) {
      __ ld_w(ya[i], key, 4 * i);
    }
    for (int i = 0; i < 4; i++) {
      __ revb_2h(xa[i], xa[i]);
    }
    for (int i = 0; i < 4; i++) {
      __ rotri_w(xa[i], xa[i], 16);
    }
    for (int i = 0; i < 4; i++) {
      __ XOR(xa[i], xa[i], ya[i]);
    }

    __ li(ftp, (intptr_t)ft_consts);

    // Round 2 - (N-1)
    for (int r = 0; r < 14; r++) {
      Register *xp;
      Register *yp;

      if (r & 1) {
        xp = xa;
        yp = ya;
      } else {
        xp = ya;
        yp = xa;
      }

      for (int i = 0; i < 4; i++) {
        __ ld_w(xp[i], key, 4 * (4 * (r + 1) + i));
      }

      for (int i = 0; i < 4; i++) {
        __ bstrpick_d(t0, yp[(i + 3) & 3], 7, 0);
        __ bstrpick_d(t1, yp[(i + 2) & 3], 15, 8);
        __ bstrpick_d(t2, yp[(i + 1) & 3], 23, 16);
        __ bstrpick_d(t3, yp[(i + 0) & 3], 31, 24);
        __ slli_w(t0, t0, 2);
        __ slli_w(t1, t1, 2);
        __ slli_w(t2, t2, 2);
        __ slli_w(t3, t3, 2);
        __ ldx_w(t0, ftp, t0);
        __ ldx_w(t1, ftp, t1);
        __ ldx_w(t2, ftp, t2);
        __ ldx_w(t3, ftp, t3);
        __ rotri_w(t0, t0, 24);
        __ rotri_w(t1, t1, 16);
        __ rotri_w(t2, t2, 8);
        __ XOR(xp[i], xp[i], t0);
        __ XOR(t0, t1, t2);
        __ XOR(xp[i], xp[i], t3);
        __ XOR(xp[i], xp[i], t0);
      }

      if (r == 8) {
        // AES 128
        __ li(t0, 44);
        __ beq(t0, keylen, tail);
      } else if (r == 10) {
        // AES 192
        __ li(t0, 52);
        __ beq(t0, keylen, tail);
      }
    }

    __ bind(tail);
    __ li(ftp, (intptr_t)fsb_consts);
    __ alsl_d(key, keylen, key, 2 - 1);

    // Round N
    for (int i = 0; i < 4; i++) {
      __ bstrpick_d(t0, ya[(i + 3) & 3], 7, 0);
      __ bstrpick_d(t1, ya[(i + 2) & 3], 15, 8);
      __ bstrpick_d(t2, ya[(i + 1) & 3], 23, 16);
      __ bstrpick_d(t3, ya[(i + 0) & 3], 31, 24);
      __ ldx_bu(t0, ftp, t0);
      __ ldx_bu(t1, ftp, t1);
      __ ldx_bu(t2, ftp, t2);
      __ ldx_bu(t3, ftp, t3);
      __ ld_w(xa[i], key, 4 * i - 16);
      __ slli_w(t1, t1, 8);
      __ slli_w(t2, t2, 16);
      __ slli_w(t3, t3, 24);
      __ XOR(xa[i], xa[i], t0);
      __ XOR(t0, t1, t2);
      __ XOR(xa[i], xa[i], t3);
      __ XOR(xa[i], xa[i], t0);
    }

    for (int i = 0; i < 4; i++) {
      __ revb_2h(xa[i], xa[i]);
    }
    for (int i = 0; i < 4; i++) {
      __ rotri_w(xa[i], xa[i], 16);
    }
    for (int i = 0; i < 4; i++) {
      __ st_w(xa[i], dst, 4 * i);
    }

    if (cbc) {
      __ move(key, keyold);
      __ addi_d(src, src, 16);
      __ addi_d(dst, dst, 16);
      __ blt(src, srcend, loop);

      for (int i = 0; i < 4; i++) {
        __ st_w(xa[i], rve, 4 * i);
      }

      __ ld_d(S3, SP, 3 * wordSize);
      __ ld_d(S2, SP, 2 * wordSize);
      __ ld_d(S1, SP, 1 * wordSize);
      __ ld_d(S0, SP, 0 * wordSize);
      __ addi_d(SP, SP, 4 * wordSize);

      __ bind(done);
      __ move(A0, srclen);
    }

    __ jr(RA);

    return start;
  }

  // Arguments:
  //
  // Inputs:
  //   A0        - source byte array address
  //   A1        - destination byte array address
  //   A2        - K (key) in little endian int array
  //   A3        - r vector byte array address
  //   A4        - input length
  //
  // Output:
  //   A0        - input length
  //
  address generate_aescrypt_decryptBlock(bool cbc) {
    static const uint32_t rt_consts[256] = {
      0x51f4a750, 0x7e416553, 0x1a17a4c3, 0x3a275e96,
      0x3bab6bcb, 0x1f9d45f1, 0xacfa58ab, 0x4be30393,
      0x2030fa55, 0xad766df6, 0x88cc7691, 0xf5024c25,
      0x4fe5d7fc, 0xc52acbd7, 0x26354480, 0xb562a38f,
      0xdeb15a49, 0x25ba1b67, 0x45ea0e98, 0x5dfec0e1,
      0xc32f7502, 0x814cf012, 0x8d4697a3, 0x6bd3f9c6,
      0x038f5fe7, 0x15929c95, 0xbf6d7aeb, 0x955259da,
      0xd4be832d, 0x587421d3, 0x49e06929, 0x8ec9c844,
      0x75c2896a, 0xf48e7978, 0x99583e6b, 0x27b971dd,
      0xbee14fb6, 0xf088ad17, 0xc920ac66, 0x7dce3ab4,
      0x63df4a18, 0xe51a3182, 0x97513360, 0x62537f45,
      0xb16477e0, 0xbb6bae84, 0xfe81a01c, 0xf9082b94,
      0x70486858, 0x8f45fd19, 0x94de6c87, 0x527bf8b7,
      0xab73d323, 0x724b02e2, 0xe31f8f57, 0x6655ab2a,
      0xb2eb2807, 0x2fb5c203, 0x86c57b9a, 0xd33708a5,
      0x302887f2, 0x23bfa5b2, 0x02036aba, 0xed16825c,
      0x8acf1c2b, 0xa779b492, 0xf307f2f0, 0x4e69e2a1,
      0x65daf4cd, 0x0605bed5, 0xd134621f, 0xc4a6fe8a,
      0x342e539d, 0xa2f355a0, 0x058ae132, 0xa4f6eb75,
      0x0b83ec39, 0x4060efaa, 0x5e719f06, 0xbd6e1051,
      0x3e218af9, 0x96dd063d, 0xdd3e05ae, 0x4de6bd46,
      0x91548db5, 0x71c45d05, 0x0406d46f, 0x605015ff,
      0x1998fb24, 0xd6bde997, 0x894043cc, 0x67d99e77,
      0xb0e842bd, 0x07898b88, 0xe7195b38, 0x79c8eedb,
      0xa17c0a47, 0x7c420fe9, 0xf8841ec9, 0x00000000,
      0x09808683, 0x322bed48, 0x1e1170ac, 0x6c5a724e,
      0xfd0efffb, 0x0f853856, 0x3daed51e, 0x362d3927,
      0x0a0fd964, 0x685ca621, 0x9b5b54d1, 0x24362e3a,
      0x0c0a67b1, 0x9357e70f, 0xb4ee96d2, 0x1b9b919e,
      0x80c0c54f, 0x61dc20a2, 0x5a774b69, 0x1c121a16,
      0xe293ba0a, 0xc0a02ae5, 0x3c22e043, 0x121b171d,
      0x0e090d0b, 0xf28bc7ad, 0x2db6a8b9, 0x141ea9c8,
      0x57f11985, 0xaf75074c, 0xee99ddbb, 0xa37f60fd,
      0xf701269f, 0x5c72f5bc, 0x44663bc5, 0x5bfb7e34,
      0x8b432976, 0xcb23c6dc, 0xb6edfc68, 0xb8e4f163,
      0xd731dcca, 0x42638510, 0x13972240, 0x84c61120,
      0x854a247d, 0xd2bb3df8, 0xaef93211, 0xc729a16d,
      0x1d9e2f4b, 0xdcb230f3, 0x0d8652ec, 0x77c1e3d0,
      0x2bb3166c, 0xa970b999, 0x119448fa, 0x47e96422,
      0xa8fc8cc4, 0xa0f03f1a, 0x567d2cd8, 0x223390ef,
      0x87494ec7, 0xd938d1c1, 0x8ccaa2fe, 0x98d40b36,
      0xa6f581cf, 0xa57ade28, 0xdab78e26, 0x3fadbfa4,
      0x2c3a9de4, 0x5078920d, 0x6a5fcc9b, 0x547e4662,
      0xf68d13c2, 0x90d8b8e8, 0x2e39f75e, 0x82c3aff5,
      0x9f5d80be, 0x69d0937c, 0x6fd52da9, 0xcf2512b3,
      0xc8ac993b, 0x10187da7, 0xe89c636e, 0xdb3bbb7b,
      0xcd267809, 0x6e5918f4, 0xec9ab701, 0x834f9aa8,
      0xe6956e65, 0xaaffe67e, 0x21bccf08, 0xef15e8e6,
      0xbae79bd9, 0x4a6f36ce, 0xea9f09d4, 0x29b07cd6,
      0x31a4b2af, 0x2a3f2331, 0xc6a59430, 0x35a266c0,
      0x744ebc37, 0xfc82caa6, 0xe090d0b0, 0x33a7d815,
      0xf104984a, 0x41ecdaf7, 0x7fcd500e, 0x1791f62f,
      0x764dd68d, 0x43efb04d, 0xccaa4d54, 0xe49604df,
      0x9ed1b5e3, 0x4c6a881b, 0xc12c1fb8, 0x4665517f,
      0x9d5eea04, 0x018c355d, 0xfa877473, 0xfb0b412e,
      0xb3671d5a, 0x92dbd252, 0xe9105633, 0x6dd64713,
      0x9ad7618c, 0x37a10c7a, 0x59f8148e, 0xeb133c89,
      0xcea927ee, 0xb761c935, 0xe11ce5ed, 0x7a47b13c,
      0x9cd2df59, 0x55f2733f, 0x1814ce79, 0x73c737bf,
      0x53f7cdea, 0x5ffdaa5b, 0xdf3d6f14, 0x7844db86,
      0xcaaff381, 0xb968c43e, 0x3824342c, 0xc2a3405f,
      0x161dc372, 0xbce2250c, 0x283c498b, 0xff0d9541,
      0x39a80171, 0x080cb3de, 0xd8b4e49c, 0x6456c190,
      0x7bcb8461, 0xd532b670, 0x486c5c74, 0xd0b85742
    };
    static const uint8_t rsb_consts[256] = {
      0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38,
      0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
      0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
      0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
      0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d,
      0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
      0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2,
      0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
      0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
      0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
      0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda,
      0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
      0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a,
      0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
      0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
      0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
      0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea,
      0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
      0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85,
      0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
      0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
      0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
      0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20,
      0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
      0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31,
      0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
      0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
      0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
      0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0,
      0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
      0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26,
      0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
    };

    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", "aescrypt_decryptBlock");

    // Allocate registers
    Register src = A0;
    Register dst = A1;
    Register key = A2;
    Register rve = A3;
    Register srclen = A4;
    Register keylen = T8;
    Register srcend = A5;
    Register t0 = A6;
    Register t1 = A7;
    Register t2, t3, rtp, rvp;
    Register xa[4] = { T0, T1, T2, T3 };
    Register ya[4] = { T4, T5, T6, T7 };

    Label loop, tail, done;
    address start = __ pc();

    if (cbc) {
      t2 = S0;
      t3 = S1;
      rtp = S2;
      rvp = S3;

      __ beqz(srclen, done);

      __ addi_d(SP, SP, -4 * wordSize);
      __ st_d(S3, SP, 3 * wordSize);
      __ st_d(S2, SP, 2 * wordSize);
      __ st_d(S1, SP, 1 * wordSize);
      __ st_d(S0, SP, 0 * wordSize);

      __ add_d(srcend, src, srclen);
      __ move(rvp, rve);
    } else {
      t2 = A3;
      t3 = A4;
      rtp = A5;
    }

    __ ld_w(keylen, key, arrayOopDesc::length_offset_in_bytes() - arrayOopDesc::base_offset_in_bytes(T_INT));

    __ bind(loop);

    // Round 1
    for (int i = 0; i < 4; i++) {
      __ ld_w(xa[i], src, 4 * i);
    }
    for (int i = 0; i < 4; i++) {
      __ ld_w(ya[i], key, 4 * (4 + i));
    }
    for (int i = 0; i < 4; i++) {
      __ revb_2h(xa[i], xa[i]);
    }
    for (int i = 0; i < 4; i++) {
      __ rotri_w(xa[i], xa[i], 16);
    }
    for (int i = 0; i < 4; i++) {
      __ XOR(xa[i], xa[i], ya[i]);
    }

    __ li(rtp, (intptr_t)rt_consts);

    // Round 2 - (N-1)
    for (int r = 0; r < 14; r++) {
      Register *xp;
      Register *yp;

      if (r & 1) {
        xp = xa;
        yp = ya;
      } else {
        xp = ya;
        yp = xa;
      }

      for (int i = 0; i < 4; i++) {
        __ ld_w(xp[i], key, 4 * (4 * (r + 1) + 4 + i));
      }

      for (int i = 0; i < 4; i++) {
        __ bstrpick_d(t0, yp[(i + 1) & 3], 7, 0);
        __ bstrpick_d(t1, yp[(i + 2) & 3], 15, 8);
        __ bstrpick_d(t2, yp[(i + 3) & 3], 23, 16);
        __ bstrpick_d(t3, yp[(i + 0) & 3], 31, 24);
        __ slli_w(t0, t0, 2);
        __ slli_w(t1, t1, 2);
        __ slli_w(t2, t2, 2);
        __ slli_w(t3, t3, 2);
        __ ldx_w(t0, rtp, t0);
        __ ldx_w(t1, rtp, t1);
        __ ldx_w(t2, rtp, t2);
        __ ldx_w(t3, rtp, t3);
        __ rotri_w(t0, t0, 24);
        __ rotri_w(t1, t1, 16);
        __ rotri_w(t2, t2, 8);
        __ XOR(xp[i], xp[i], t0);
        __ XOR(t0, t1, t2);
        __ XOR(xp[i], xp[i], t3);
        __ XOR(xp[i], xp[i], t0);
      }

      if (r == 8) {
        // AES 128
        __ li(t0, 44);
        __ beq(t0, keylen, tail);
      } else if (r == 10) {
        // AES 192
        __ li(t0, 52);
        __ beq(t0, keylen, tail);
      }
    }

    __ bind(tail);
    __ li(rtp, (intptr_t)rsb_consts);

    // Round N
    for (int i = 0; i < 4; i++) {
      __ bstrpick_d(t0, ya[(i + 1) & 3], 7, 0);
      __ bstrpick_d(t1, ya[(i + 2) & 3], 15, 8);
      __ bstrpick_d(t2, ya[(i + 3) & 3], 23, 16);
      __ bstrpick_d(t3, ya[(i + 0) & 3], 31, 24);
      __ ldx_bu(t0, rtp, t0);
      __ ldx_bu(t1, rtp, t1);
      __ ldx_bu(t2, rtp, t2);
      __ ldx_bu(t3, rtp, t3);
      __ ld_w(xa[i], key, 4 * i);
      __ slli_w(t1, t1, 8);
      __ slli_w(t2, t2, 16);
      __ slli_w(t3, t3, 24);
      __ XOR(xa[i], xa[i], t0);
      __ XOR(t0, t1, t2);
      __ XOR(xa[i], xa[i], t3);
      __ XOR(xa[i], xa[i], t0);
    }

    if (cbc) {
      for (int i = 0; i < 4; i++) {
        __ ld_w(ya[i], rvp, 4 * i);
      }
    }
    for (int i = 0; i < 4; i++) {
      __ revb_2h(xa[i], xa[i]);
    }
    for (int i = 0; i < 4; i++) {
      __ rotri_w(xa[i], xa[i], 16);
    }
    if (cbc) {
      for (int i = 0; i < 4; i++) {
        __ XOR(xa[i], xa[i], ya[i]);
      }
    }
    for (int i = 0; i < 4; i++) {
      __ st_w(xa[i], dst, 4 * i);
    }

    if (cbc) {
      __ move(rvp, src);
      __ addi_d(src, src, 16);
      __ addi_d(dst, dst, 16);
      __ blt(src, srcend, loop);

      __ ld_d(t0, src, -16);
      __ ld_d(t1, src, -8);
      __ st_d(t0, rve, 0);
      __ st_d(t1, rve, 8);

      __ ld_d(S3, SP, 3 * wordSize);
      __ ld_d(S2, SP, 2 * wordSize);
      __ ld_d(S1, SP, 1 * wordSize);
      __ ld_d(S0, SP, 0 * wordSize);
      __ addi_d(SP, SP, 4 * wordSize);

      __ bind(done);
      __ move(A0, srclen);
    }

    __ jr(RA);

    return start;
  }

  // Arguments:
  //
  // Inputs:
  //   A0        - byte[]  source+offset
  //   A1        - int[]   SHA.state
  //   A2        - int     offset
  //   A3        - int     limit
  //
  void generate_sha1_implCompress(const char *name, address &entry, address &entry_mb) {
    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    Label keys, loop;

    // Keys
    __ bind(keys);
    __ emit_int32(0x5a827999);
    __ emit_int32(0x6ed9eba1);
    __ emit_int32(0x8f1bbcdc);
    __ emit_int32(0xca62c1d6);

    // Allocate registers
    Register t0 = T5;
    Register t1 = T6;
    Register t2 = T7;
    Register t3 = T8;
    Register buf = A0;
    Register state = A1;
    Register ofs = A2;
    Register limit = A3;
    Register ka[4] = { A4, A5, A6, A7 };
    Register sa[5] = { T0, T1, T2, T3, T4 };

    // Entry
    entry = __ pc();
    __ move(ofs, R0);
    __ move(limit, R0);

    // Entry MB
    entry_mb = __ pc();

    // Allocate scratch space
    __ addi_d(SP, SP, -64);

    // Load keys
    __ lipc(t0, keys);
    __ ld_w(ka[0], t0, 0);
    __ ld_w(ka[1], t0, 4);
    __ ld_w(ka[2], t0, 8);
    __ ld_w(ka[3], t0, 12);

    __ bind(loop);
    // Load arguments
    __ ld_w(sa[0], state, 0);
    __ ld_w(sa[1], state, 4);
    __ ld_w(sa[2], state, 8);
    __ ld_w(sa[3], state, 12);
    __ ld_w(sa[4], state, 16);

    // 80 rounds of hashing
    for (int i = 0; i < 80; i++) {
      Register a = sa[(5 - (i % 5)) % 5];
      Register b = sa[(6 - (i % 5)) % 5];
      Register c = sa[(7 - (i % 5)) % 5];
      Register d = sa[(8 - (i % 5)) % 5];
      Register e = sa[(9 - (i % 5)) % 5];

      if (i < 16) {
        __ ld_w(t0, buf, i * 4);
        __ revb_2h(t0, t0);
        __ rotri_w(t0, t0, 16);
        __ add_w(e, e, t0);
        __ st_w(t0, SP, i * 4);
        __ XOR(t0, c, d);
        __ AND(t0, t0, b);
        __ XOR(t0, t0, d);
      } else {
        __ ld_w(t0, SP, ((i - 3) & 0xF) * 4);
        __ ld_w(t1, SP, ((i - 8) & 0xF) * 4);
        __ ld_w(t2, SP, ((i - 14) & 0xF) * 4);
        __ ld_w(t3, SP, ((i - 16) & 0xF) * 4);
        __ XOR(t0, t0, t1);
        __ XOR(t0, t0, t2);
        __ XOR(t0, t0, t3);
        __ rotri_w(t0, t0, 31);
        __ add_w(e, e, t0);
        __ st_w(t0, SP, (i & 0xF) * 4);

        if (i < 20) {
          __ XOR(t0, c, d);
          __ AND(t0, t0, b);
          __ XOR(t0, t0, d);
        } else if (i < 40 || i >= 60) {
          __ XOR(t0, b, c);
          __ XOR(t0, t0, d);
        } else if (i < 60) {
          __ OR(t0, c, d);
          __ AND(t0, t0, b);
          __ AND(t2, c, d);
          __ OR(t0, t0, t2);
        }
      }

      __ rotri_w(b, b, 2);
      __ add_w(e, e, t0);
      __ add_w(e, e, ka[i / 20]);
      __ rotri_w(t0, a, 27);
      __ add_w(e, e, t0);
    }

    // Save updated state
    __ ld_w(t0, state, 0);
    __ ld_w(t1, state, 4);
    __ ld_w(t2, state, 8);
    __ ld_w(t3, state, 12);
    __ add_w(sa[0], sa[0], t0);
    __ ld_w(t0, state, 16);
    __ add_w(sa[1], sa[1], t1);
    __ add_w(sa[2], sa[2], t2);
    __ add_w(sa[3], sa[3], t3);
    __ add_w(sa[4], sa[4], t0);
    __ st_w(sa[0], state, 0);
    __ st_w(sa[1], state, 4);
    __ st_w(sa[2], state, 8);
    __ st_w(sa[3], state, 12);
    __ st_w(sa[4], state, 16);

    __ addi_w(ofs, ofs, 64);
    __ addi_d(buf, buf, 64);
    __ bge(limit, ofs, loop);
    __ move(V0, ofs); // return ofs

    __ addi_d(SP, SP, 64);
    __ jr(RA);
  }

  // Arguments:
  //
  // Inputs:
  //   A0        - byte[]  source+offset
  //   A1        - int[]   SHA.state
  //   A2        - int     offset
  //   A3        - int     limit
  //
  void generate_sha256_implCompress(const char *name, address &entry, address &entry_mb) {
    static const uint32_t round_consts[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
      0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
      0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
      0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
      0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
      0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };
    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    Label loop;

    // Allocate registers
    Register t0 = A4;
    Register t1 = A5;
    Register t2 = A6;
    Register t3 = A7;
    Register buf = A0;
    Register state = A1;
    Register ofs = A2;
    Register limit = A3;
    Register kptr = T8;
    Register sa[8] = { T0, T1, T2, T3, T4, T5, T6, T7 };

    // Entry
    entry = __ pc();
    __ move(ofs, R0);
    __ move(limit, R0);

    // Entry MB
    entry_mb = __ pc();

    // Allocate scratch space
    __ addi_d(SP, SP, -64);

    // Load keys base address
    __ li(kptr, (intptr_t)round_consts);

    __ bind(loop);
    // Load state
    __ ld_w(sa[0], state, 0);
    __ ld_w(sa[1], state, 4);
    __ ld_w(sa[2], state, 8);
    __ ld_w(sa[3], state, 12);
    __ ld_w(sa[4], state, 16);
    __ ld_w(sa[5], state, 20);
    __ ld_w(sa[6], state, 24);
    __ ld_w(sa[7], state, 28);

    // Do 64 rounds of hashing
    for (int i = 0; i < 64; i++) {
      Register a = sa[(0 - i) & 7];
      Register b = sa[(1 - i) & 7];
      Register c = sa[(2 - i) & 7];
      Register d = sa[(3 - i) & 7];
      Register e = sa[(4 - i) & 7];
      Register f = sa[(5 - i) & 7];
      Register g = sa[(6 - i) & 7];
      Register h = sa[(7 - i) & 7];

      if (i < 16) {
        __ ld_w(t1, buf, i * 4);
        __ revb_2h(t1, t1);
        __ rotri_w(t1, t1, 16);
      } else {
        __ ld_w(t0, SP, ((i - 15) & 0xF) * 4);
        __ ld_w(t1, SP, ((i - 16) & 0xF) * 4);
        __ ld_w(t2, SP, ((i - 7) & 0xF) * 4);
        __ add_w(t1, t1, t2);
        __ rotri_w(t2, t0, 18);
        __ srli_w(t3, t0, 3);
        __ rotri_w(t0, t0, 7);
        __ XOR(t2, t2, t3);
        __ XOR(t0, t0, t2);
        __ add_w(t1, t1, t0);
        __ ld_w(t0, SP, ((i - 2) & 0xF) * 4);
        __ rotri_w(t2, t0, 19);
        __ srli_w(t3, t0, 10);
        __ rotri_w(t0, t0, 17);
        __ XOR(t2, t2, t3);
        __ XOR(t0, t0, t2);
        __ add_w(t1, t1, t0);
      }

      __ rotri_w(t2, e, 11);
      __ rotri_w(t3, e, 25);
      __ rotri_w(t0, e, 6);
      __ XOR(t2, t2, t3);
      __ XOR(t0, t0, t2);
      __ XOR(t2, g, f);
      __ ld_w(t3, kptr, i * 4);
      __ AND(t2, t2, e);
      __ XOR(t2, t2, g);
      __ add_w(t0, t0, t2);
      __ add_w(t0, t0, t3);
      __ add_w(h, h, t1);
      __ add_w(h, h, t0);
      __ add_w(d, d, h);
      __ rotri_w(t2, a, 13);
      __ rotri_w(t3, a, 22);
      __ rotri_w(t0, a, 2);
      __ XOR(t2, t2, t3);
      __ XOR(t0, t0, t2);
      __ add_w(h, h, t0);
      __ OR(t0, c, b);
      __ AND(t2, c, b);
      __ AND(t0, t0, a);
      __ OR(t0, t0, t2);
      __ add_w(h, h, t0);
      __ st_w(t1, SP, (i & 0xF) * 4);
    }

    // Add to state
    __ ld_w(t0, state, 0);
    __ ld_w(t1, state, 4);
    __ ld_w(t2, state, 8);
    __ ld_w(t3, state, 12);
    __ add_w(sa[0], sa[0], t0);
    __ add_w(sa[1], sa[1], t1);
    __ add_w(sa[2], sa[2], t2);
    __ add_w(sa[3], sa[3], t3);
    __ ld_w(t0, state, 16);
    __ ld_w(t1, state, 20);
    __ ld_w(t2, state, 24);
    __ ld_w(t3, state, 28);
    __ add_w(sa[4], sa[4], t0);
    __ add_w(sa[5], sa[5], t1);
    __ add_w(sa[6], sa[6], t2);
    __ add_w(sa[7], sa[7], t3);
    __ st_w(sa[0], state, 0);
    __ st_w(sa[1], state, 4);
    __ st_w(sa[2], state, 8);
    __ st_w(sa[3], state, 12);
    __ st_w(sa[4], state, 16);
    __ st_w(sa[5], state, 20);
    __ st_w(sa[6], state, 24);
    __ st_w(sa[7], state, 28);

    __ addi_w(ofs, ofs, 64);
    __ addi_d(buf, buf, 64);
    __ bge(limit, ofs, loop);
    __ move(V0, ofs); // return ofs

    __ addi_d(SP, SP, 64);
    __ jr(RA);
  }

  // Do NOT delete this node which stands for stub routine placeholder
  address generate_updateBytesCRC32() {
    assert(UseCRC32Intrinsics, "need CRC32 instructions support");

    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", "updateBytesCRC32");

    address start = __ pc();

    const Register crc = A0;  // crc
    const Register buf = A1;  // source java byte array address
    const Register len = A2;  // length
    const Register tmp = A3;

    __ enter(); // required for proper stackwalking of RuntimeStub frame

    __ kernel_crc32(crc, buf, len, tmp);

    __ leave(); // required for proper stackwalking of RuntimeStub frame
    __ jr(RA);

    return start;
  }

  // add a function to implement SafeFetch32 and SafeFetchN
  void generate_safefetch(const char* name, int size, address* entry,
                          address* fault_pc, address* continuation_pc) {
    // safefetch signatures:
    //   int      SafeFetch32(int*      adr, int      errValue);
    //   intptr_t SafeFetchN (intptr_t* adr, intptr_t errValue);
    //
    // arguments:
    //   A0 = adr
    //   A1 = errValue
    //
    // result:
    //   PPC_RET  = *adr or errValue
    StubCodeMark mark(this, "StubRoutines", name);

    // Entry point, pc or function descriptor.
    *entry = __ pc();

    // Load *adr into A1, may fault.
    *fault_pc = __ pc();
    switch (size) {
      case 4:
        // int32_t
        __ ld_w(A1, A0, 0);
        break;
      case 8:
        // int64_t
        __ ld_d(A1, A0, 0);
        break;
      default:
        ShouldNotReachHere();
    }

    // return errValue or *adr
    *continuation_pc = __ pc();
    __ add_d(V0, A1, R0);
    __ jr(RA);
  }


#undef __
#define __ masm->

  // Continuation point for throwing of implicit exceptions that are
  // not handled in the current activation. Fabricates an exception
  // oop and initiates normal exception dispatching in this
  // frame. Since we need to preserve callee-saved values (currently
  // only for C2, but done for C1 as well) we need a callee-saved oop
  // map and therefore have to make these stubs into RuntimeStubs
  // rather than BufferBlobs.  If the compiler needs all registers to
  // be preserved between the fault point and the exception handler
  // then it must assume responsibility for that in
  // AbstractCompiler::continuation_for_implicit_null_exception or
  // continuation_for_implicit_division_by_zero_exception. All other
  // implicit exceptions (e.g., NullPointerException or
  // AbstractMethodError on entry) are either at call sites or
  // otherwise assume that stack unwinding will be initiated, so
  // caller saved registers were assumed volatile in the compiler.
  address generate_throw_exception(const char* name,
                                   address runtime_entry,
                                   bool restore_saved_exception_pc) {
    // Information about frame layout at time of blocking runtime call.
    // Note that we only have to preserve callee-saved registers since
    // the compilers are responsible for supplying a continuation point
    // if they expect all registers to be preserved.
    enum layout {
      thread_off,    // last_java_sp
      S7_off,        // callee saved register      sp + 1
      S6_off,        // callee saved register      sp + 2
      S5_off,        // callee saved register      sp + 3
      S4_off,        // callee saved register      sp + 4
      S3_off,        // callee saved register      sp + 5
      S2_off,        // callee saved register      sp + 6
      S1_off,        // callee saved register      sp + 7
      S0_off,        // callee saved register      sp + 8
      FP_off,
      ret_address,
      framesize
    };

    int insts_size = 2048;
    int locs_size  = 32;

    //  CodeBuffer* code     = new CodeBuffer(insts_size, locs_size, 0, 0, 0, false,
    //  NULL, NULL, NULL, false, NULL, name, false);
    CodeBuffer code (name , insts_size, locs_size);
    OopMapSet* oop_maps  = new OopMapSet();
    MacroAssembler* masm = new MacroAssembler(&code);

    address start = __ pc();

    // This is an inlined and slightly modified version of call_VM
    // which has the ability to fetch the return PC out of
    // thread-local storage and also sets up last_Java_sp slightly
    // differently than the real call_VM
#ifndef OPT_THREAD
    Register java_thread = TREG;
    __ get_thread(java_thread);
#else
    Register java_thread = TREG;
#endif
    if (restore_saved_exception_pc) {
      __ ld_d(RA, java_thread, in_bytes(JavaThread::saved_exception_pc_offset()));
    }
    __ enter(); // required for proper stackwalking of RuntimeStub frame

    __ addi_d(SP, SP, (-1) * (framesize-2) * wordSize); // prolog
    __ st_d(S0, SP, S0_off * wordSize);
    __ st_d(S1, SP, S1_off * wordSize);
    __ st_d(S2, SP, S2_off * wordSize);
    __ st_d(S3, SP, S3_off * wordSize);
    __ st_d(S4, SP, S4_off * wordSize);
    __ st_d(S5, SP, S5_off * wordSize);
    __ st_d(S6, SP, S6_off * wordSize);
    __ st_d(S7, SP, S7_off * wordSize);

    int frame_complete = __ pc() - start;
    // push java thread (becomes first argument of C function)
    __ st_d(java_thread, SP, thread_off * wordSize);
    if (java_thread != A0)
      __ move(A0, java_thread);

    // Set up last_Java_sp and last_Java_fp
    Label before_call;
    address the_pc = __ pc();
    __ bind(before_call);
    __ set_last_Java_frame(java_thread, SP, FP, before_call);
    // Align stack
    __ li(AT, -(StackAlignmentInBytes));
    __ andr(SP, SP, AT);

    // Call runtime
    // TODO: confirm reloc
    __ call(runtime_entry, relocInfo::runtime_call_type);
    // Generate oop map
    OopMap* map =  new OopMap(framesize, 0);
    oop_maps->add_gc_map(the_pc - start,  map);

    // restore the thread (cannot use the pushed argument since arguments
    // may be overwritten by C code generated by an optimizing compiler);
    // however can use the register value directly if it is callee saved.
#ifndef OPT_THREAD
    __ get_thread(java_thread);
#endif

    __ ld_d(SP, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
    __ reset_last_Java_frame(java_thread, true);

    // Restore callee save registers.  This must be done after resetting the Java frame
    __ ld_d(S0, SP, S0_off * wordSize);
    __ ld_d(S1, SP, S1_off * wordSize);
    __ ld_d(S2, SP, S2_off * wordSize);
    __ ld_d(S3, SP, S3_off * wordSize);
    __ ld_d(S4, SP, S4_off * wordSize);
    __ ld_d(S5, SP, S5_off * wordSize);
    __ ld_d(S6, SP, S6_off * wordSize);
    __ ld_d(S7, SP, S7_off * wordSize);

    // discard arguments
    __ move(SP, FP); // epilog
    __ pop(FP);
    // check for pending exceptions
#ifdef ASSERT
    Label L;
    __ ld_d(AT, java_thread, in_bytes(Thread::pending_exception_offset()));
    __ bne(AT, R0, L);
    __ should_not_reach_here();
    __ bind(L);
#endif //ASSERT
    __ jmp(StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);

    RuntimeStub* stub = RuntimeStub::new_runtime_stub(name,
                                                      &code,
                                                      frame_complete,
                                                      framesize,
                                                      oop_maps, false);
    return stub->entry_point();
  }

  class MontgomeryMultiplyGenerator : public MacroAssembler {

    Register Pa_base, Pb_base, Pn_base, Pm_base, inv, Rlen, Rlen2, Ra, Rb, Rm,
      Rn, Iam, Ibn, Rhi_ab, Rlo_ab, Rhi_mn, Rlo_mn, t0, t1, t2, Ri, Rj;

    bool _squaring;

  public:
    MontgomeryMultiplyGenerator (Assembler *as, bool squaring)
      : MacroAssembler(as->code()), _squaring(squaring) {

      // Register allocation

      Register reg = A0;
      Pa_base = reg;      // Argument registers:
      if (squaring)
        Pb_base = Pa_base;
      else
        Pb_base = ++reg;
      Pn_base = ++reg;
      Rlen = ++reg;
      inv = ++reg;
      Rlen2 = inv;        // Reuse inv
      Pm_base = ++reg;

                          // Working registers:
      Ra = ++reg;         // The current digit of a, b, n, and m.
      Rb = ++reg;
      Rm = ++reg;
      Rn = ++reg;

      Iam = ++reg;        // Index to the current/next digit of a, b, n, and m.
      Ibn = ++reg;

      t0 = ++reg;         // Three registers which form a
      t1 = ++reg;         // triple-precision accumuator.
      t2 = ++reg;

      Ri = ++reg;         // Inner and outer loop indexes.
      Rj = ++reg;

      if (squaring) {
        Rhi_ab = ++reg;   // Product registers: low and high parts
        reg = S0;
        Rlo_ab = ++reg;   // of a*b and m*n.
      } else {
        reg = S0;
        Rhi_ab = reg;     // Product registers: low and high parts
        Rlo_ab = ++reg;   // of a*b and m*n.
      }

      Rhi_mn = ++reg;
      Rlo_mn = ++reg;
    }

  private:
    void enter() {
      addi_d(SP, SP, -6 * wordSize);
      st_d(FP, SP, 0 * wordSize);
      move(FP, SP);
    }

    void leave() {
      addi_d(T0, FP, 6 * wordSize);
      ld_d(FP, FP, 0 * wordSize);
      move(SP, T0);
    }

    void save_regs() {
      if (!_squaring)
        st_d(Rhi_ab, FP, 5 * wordSize);
      st_d(Rlo_ab, FP, 4 * wordSize);
      st_d(Rhi_mn, FP, 3 * wordSize);
      st_d(Rlo_mn, FP, 2 * wordSize);
      st_d(Pm_base, FP, 1 * wordSize);
    }

    void restore_regs() {
      if (!_squaring)
        ld_d(Rhi_ab, FP, 5 * wordSize);
      ld_d(Rlo_ab, FP, 4 * wordSize);
      ld_d(Rhi_mn, FP, 3 * wordSize);
      ld_d(Rlo_mn, FP, 2 * wordSize);
      ld_d(Pm_base, FP, 1 * wordSize);
    }

    template <typename T>
    void unroll_2(Register count, T block, Register tmp) {
      Label loop, end, odd;
      andi(tmp, count, 1);
      bnez(tmp, odd);
      beqz(count, end);
      align(16);
      bind(loop);
      (this->*block)();
      bind(odd);
      (this->*block)();
      addi_w(count, count, -2);
      blt(R0, count, loop);
      bind(end);
    }

    template <typename T>
    void unroll_2(Register count, T block, Register d, Register s, Register tmp) {
      Label loop, end, odd;
      andi(tmp, count, 1);
      bnez(tmp, odd);
      beqz(count, end);
      align(16);
      bind(loop);
      (this->*block)(d, s, tmp);
      bind(odd);
      (this->*block)(d, s, tmp);
      addi_w(count, count, -2);
      blt(R0, count, loop);
      bind(end);
    }

    void acc(Register Rhi, Register Rlo,
             Register t0, Register t1, Register t2, Register t, Register c) {
      add_d(t0, t0, Rlo);
      OR(t, t1, Rhi);
      sltu(c, t0, Rlo);
      add_d(t1, t1, Rhi);
      add_d(t1, t1, c);
      sltu(c, t1, t);
      add_d(t2, t2, c);
    }

    void pre1(Register i) {
      block_comment("pre1");
      // Iam = 0;
      // Ibn = i;

      slli_w(Ibn, i, LogBytesPerWord);

      // Ra = Pa_base[Iam];
      // Rb = Pb_base[Ibn];
      // Rm = Pm_base[Iam];
      // Rn = Pn_base[Ibn];

      ld_d(Ra, Pa_base, 0);
      ldx_d(Rb, Pb_base, Ibn);
      ld_d(Rm, Pm_base, 0);
      ldx_d(Rn, Pn_base, Ibn);

      move(Iam, R0);

      // Zero the m*n result.
      move(Rhi_mn, R0);
      move(Rlo_mn, R0);
    }

    // The core multiply-accumulate step of a Montgomery
    // multiplication.  The idea is to schedule operations as a
    // pipeline so that instructions with long latencies (loads and
    // multiplies) have time to complete before their results are
    // used.  This most benefits in-order implementations of the
    // architecture but out-of-order ones also benefit.
    void step() {
      block_comment("step");
      // MACC(Ra, Rb, t0, t1, t2);
      // Ra = Pa_base[++Iam];
      // Rb = Pb_base[--Ibn];
      addi_d(Iam, Iam, wordSize);
      addi_d(Ibn, Ibn, -wordSize);
      mul_d(Rlo_ab, Ra, Rb);
      mulh_du(Rhi_ab, Ra, Rb);
      acc(Rhi_mn, Rlo_mn, t0, t1, t2, Ra, Rb); // The pending m*n from the
                                               // previous iteration.
      ldx_d(Ra, Pa_base, Iam);
      ldx_d(Rb, Pb_base, Ibn);

      // MACC(Rm, Rn, t0, t1, t2);
      // Rm = Pm_base[Iam];
      // Rn = Pn_base[Ibn];
      mul_d(Rlo_mn, Rm, Rn);
      mulh_du(Rhi_mn, Rm, Rn);
      acc(Rhi_ab, Rlo_ab, t0, t1, t2, Rm, Rn);
      ldx_d(Rm, Pm_base, Iam);
      ldx_d(Rn, Pn_base, Ibn);
    }

    void post1() {
      block_comment("post1");

      // MACC(Ra, Rb, t0, t1, t2);
      mul_d(Rlo_ab, Ra, Rb);
      mulh_du(Rhi_ab, Ra, Rb);
      acc(Rhi_mn, Rlo_mn, t0, t1, t2, Ra, Rb);  // The pending m*n
      acc(Rhi_ab, Rlo_ab, t0, t1, t2, Ra, Rb);

      // Pm_base[Iam] = Rm = t0 * inv;
      mul_d(Rm, t0, inv);
      stx_d(Rm, Pm_base, Iam);

      // MACC(Rm, Rn, t0, t1, t2);
      // t0 = t1; t1 = t2; t2 = 0;
      mulh_du(Rhi_mn, Rm, Rn);

#ifndef PRODUCT
      // assert(m[i] * n[0] + t0 == 0, "broken Montgomery multiply");
      {
        mul_d(Rlo_mn, Rm, Rn);
        add_d(Rlo_mn, t0, Rlo_mn);
        Label ok;
        beqz(Rlo_mn, ok); {
          stop("broken Montgomery multiply");
        } bind(ok);
      }
#endif

      // We have very carefully set things up so that
      // m[i]*n[0] + t0 == 0 (mod b), so we don't have to calculate
      // the lower half of Rm * Rn because we know the result already:
      // it must be -t0.  t0 + (-t0) must generate a carry iff
      // t0 != 0.  So, rather than do a mul and an adds we just set
      // the carry flag iff t0 is nonzero.
      //
      // mul_d(Rlo_mn, Rm, Rn);
      // add_d(t0, t0, Rlo_mn);
      OR(Ra, t1, Rhi_mn);
      sltu(Rb, R0, t0);
      add_d(t0, t1, Rhi_mn);
      add_d(t0, t0, Rb);
      sltu(Rb, t0, Ra);
      add_d(t1, t2, Rb);
      move(t2, R0);
    }

    void pre2(Register i, Register len) {
      block_comment("pre2");

      // Rj == i-len
      sub_w(Rj, i, len);

      // Iam = i - len;
      // Ibn = len;
      slli_w(Iam, Rj, LogBytesPerWord);
      slli_w(Ibn, len, LogBytesPerWord);

      // Ra = Pa_base[++Iam];
      // Rb = Pb_base[--Ibn];
      // Rm = Pm_base[++Iam];
      // Rn = Pn_base[--Ibn];
      addi_d(Iam, Iam, wordSize);
      addi_d(Ibn, Ibn, -wordSize);

      ldx_d(Ra, Pa_base, Iam);
      ldx_d(Rb, Pb_base, Ibn);
      ldx_d(Rm, Pm_base, Iam);
      ldx_d(Rn, Pn_base, Ibn);

      move(Rhi_mn, R0);
      move(Rlo_mn, R0);
    }

    void post2(Register i, Register len) {
      block_comment("post2");

      sub_w(Rj, i, len);
      slli_w(Iam, Rj, LogBytesPerWord);

      add_d(t0, t0, Rlo_mn); // The pending m*n, low part

      // As soon as we know the least significant digit of our result,
      // store it.
      // Pm_base[i-len] = t0;
      stx_d(t0, Pm_base, Iam);

      // t0 = t1; t1 = t2; t2 = 0;
      OR(Ra, t1, Rhi_mn);
      sltu(Rb, t0, Rlo_mn);
      add_d(t0, t1, Rhi_mn); // The pending m*n, high part
      add_d(t0, t0, Rb);
      sltu(Rb, t0, Ra);
      add_d(t1, t2, Rb);
      move(t2, R0);
    }

    // A carry in t0 after Montgomery multiplication means that we
    // should subtract multiples of n from our result in m.  We'll
    // keep doing that until there is no carry.
    void normalize(Register len) {
      block_comment("normalize");
      // while (t0)
      //   t0 = sub(Pm_base, Pn_base, t0, len);
      Label loop, post, again;
      Register cnt = t1, i = t2, b = Ra, t = Rb; // Re-use registers; we're done with them now
      beqz(t0, post); {
        bind(again); {
          move(i, R0);
          move(b, R0);
          slli_w(cnt, len, LogBytesPerWord);
          align(16);
          bind(loop); {
            ldx_d(Rm, Pm_base, i);
            ldx_d(Rn, Pn_base, i);
            sltu(t, Rm, b);
            sub_d(Rm, Rm, b);
            sltu(b, Rm, Rn);
            sub_d(Rm, Rm, Rn);
            OR(b, b, t);
            stx_d(Rm, Pm_base, i);
            addi_w(i, i, BytesPerWord);
          } blt(i, cnt, loop);
          sub_d(t0, t0, b);
        } bnez(t0, again);
      } bind(post);
    }

    // Move memory at s to d, reversing words.
    //    Increments d to end of copied memory
    //    Destroys tmp1, tmp2, tmp3
    //    Preserves len
    //    Leaves s pointing to the address which was in d at start
    void reverse(Register d, Register s, Register len, Register tmp1, Register tmp2) {
      assert(tmp1 < S0 && tmp2 < S0, "register corruption");

      alsl_d(s, len, s, LogBytesPerWord - 1);
      move(tmp1, len);
      unroll_2(tmp1, &MontgomeryMultiplyGenerator::reverse1, d, s, tmp2);
      slli_w(s, len, LogBytesPerWord);
      sub_d(s, d, s);
    }

    // where
    void reverse1(Register d, Register s, Register tmp) {
      ld_d(tmp, s, -wordSize);
      addi_d(s, s, -wordSize);
      addi_d(d, d, wordSize);
      rotri_d(tmp, tmp, 32);
      st_d(tmp, d, -wordSize);
    }

  public:
    /**
     * Fast Montgomery multiplication.  The derivation of the
     * algorithm is in A Cryptographic Library for the Motorola
     * DSP56000, Dusse and Kaliski, Proc. EUROCRYPT 90, pp. 230-237.
     *
     * Arguments:
     *
     * Inputs for multiplication:
     *   A0   - int array elements a
     *   A1   - int array elements b
     *   A2   - int array elements n (the modulus)
     *   A3   - int length
     *   A4   - int inv
     *   A5   - int array elements m (the result)
     *
     * Inputs for squaring:
     *   A0   - int array elements a
     *   A1   - int array elements n (the modulus)
     *   A2   - int length
     *   A3   - int inv
     *   A4   - int array elements m (the result)
     *
     */
    address generate_multiply() {
      Label argh, nothing;
      bind(argh);
      stop("MontgomeryMultiply total_allocation must be <= 8192");

      align(CodeEntryAlignment);
      address entry = pc();

      beqz(Rlen, nothing);

      enter();

      // Make room.
      sltui(Ra, Rlen, 513);
      beqz(Ra, argh);
      slli_w(Ra, Rlen, exact_log2(4 * sizeof (jint)));
      sub_d(Ra, SP, Ra);

      srli_w(Rlen, Rlen, 1); // length in longwords = len/2

      {
        // Copy input args, reversing as we go.  We use Ra as a
        // temporary variable.
        reverse(Ra, Pa_base, Rlen, t0, t1);
        if (!_squaring)
          reverse(Ra, Pb_base, Rlen, t0, t1);
        reverse(Ra, Pn_base, Rlen, t0, t1);
      }

      // Push all call-saved registers and also Pm_base which we'll need
      // at the end.
      save_regs();

#ifndef PRODUCT
      // assert(inv * n[0] == -1UL, "broken inverse in Montgomery multiply");
      {
        ld_d(Rn, Pn_base, 0);
        li(t0, -1);
        mul_d(Rlo_mn, Rn, inv);
        Label ok;
        beq(Rlo_mn, t0, ok); {
          stop("broken inverse in Montgomery multiply");
        } bind(ok);
      }
#endif

      move(Pm_base, Ra);

      move(t0, R0);
      move(t1, R0);
      move(t2, R0);

      block_comment("for (int i = 0; i < len; i++) {");
      move(Ri, R0); {
        Label loop, end;
        bge(Ri, Rlen, end);

        bind(loop);
        pre1(Ri);

        block_comment("  for (j = i; j; j--) {"); {
          move(Rj, Ri);
          unroll_2(Rj, &MontgomeryMultiplyGenerator::step, Rlo_ab);
        } block_comment("  } // j");

        post1();
        addi_w(Ri, Ri, 1);
        blt(Ri, Rlen, loop);
        bind(end);
        block_comment("} // i");
      }

      block_comment("for (int i = len; i < 2*len; i++) {");
      move(Ri, Rlen);
      slli_w(Rlen2, Rlen, 1); {
        Label loop, end;
        bge(Ri, Rlen2, end);

        bind(loop);
        pre2(Ri, Rlen);

        block_comment("  for (j = len*2-i-1; j; j--) {"); {
          sub_w(Rj, Rlen2, Ri);
          addi_w(Rj, Rj, -1);
          unroll_2(Rj, &MontgomeryMultiplyGenerator::step, Rlo_ab);
        } block_comment("  } // j");

        post2(Ri, Rlen);
        addi_w(Ri, Ri, 1);
        blt(Ri, Rlen2, loop);
        bind(end);
      }
      block_comment("} // i");

      normalize(Rlen);

      move(Ra, Pm_base);  // Save Pm_base in Ra
      restore_regs();  // Restore caller's Pm_base

      // Copy our result into caller's Pm_base
      reverse(Pm_base, Ra, Rlen, t0, t1);

      leave();
      bind(nothing);
      jr(RA);

      return entry;
    }
    // In C, approximately:

    // void
    // montgomery_multiply(unsigned long Pa_base[], unsigned long Pb_base[],
    //                     unsigned long Pn_base[], unsigned long Pm_base[],
    //                     unsigned long inv, int len) {
    //   unsigned long t0 = 0, t1 = 0, t2 = 0; // Triple-precision accumulator
    //   unsigned long Ra, Rb, Rn, Rm;
    //   int i, Iam, Ibn;

    //   assert(inv * Pn_base[0] == -1UL, "broken inverse in Montgomery multiply");

    //   for (i = 0; i < len; i++) {
    //     int j;

    //     Iam = 0;
    //     Ibn = i;

    //     Ra = Pa_base[Iam];
    //     Rb = Pb_base[Iam];
    //     Rm = Pm_base[Ibn];
    //     Rn = Pn_base[Ibn];

    //     int iters = i;
    //     for (j = 0; iters--; j++) {
    //       assert(Ra == Pa_base[j] && Rb == Pb_base[i-j], "must be");
    //       MACC(Ra, Rb, t0, t1, t2);
    //       Ra = Pa_base[++Iam];
    //       Rb = pb_base[--Ibn];
    //       assert(Rm == Pm_base[j] && Rn == Pn_base[i-j], "must be");
    //       MACC(Rm, Rn, t0, t1, t2);
    //       Rm = Pm_base[++Iam];
    //       Rn = Pn_base[--Ibn];
    //     }

    //     assert(Ra == Pa_base[i] && Rb == Pb_base[0], "must be");
    //     MACC(Ra, Rb, t0, t1, t2);
    //     Pm_base[Iam] = Rm = t0 * inv;
    //     assert(Rm == Pm_base[i] && Rn == Pn_base[0], "must be");
    //     MACC(Rm, Rn, t0, t1, t2);

    //     assert(t0 == 0, "broken Montgomery multiply");

    //     t0 = t1; t1 = t2; t2 = 0;
    //   }

    //   for (i = len; i < 2*len; i++) {
    //     int j;

    //     Iam = i - len;
    //     Ibn = len;

    //     Ra = Pa_base[++Iam];
    //     Rb = Pb_base[--Ibn];
    //     Rm = Pm_base[++Iam];
    //     Rn = Pn_base[--Ibn];

    //     int iters = len*2-i-1;
    //     for (j = i-len+1; iters--; j++) {
    //       assert(Ra == Pa_base[j] && Rb == Pb_base[i-j], "must be");
    //       MACC(Ra, Rb, t0, t1, t2);
    //       Ra = Pa_base[++Iam];
    //       Rb = Pb_base[--Ibn];
    //       assert(Rm == Pm_base[j] && Rn == Pn_base[i-j], "must be");
    //       MACC(Rm, Rn, t0, t1, t2);
    //       Rm = Pm_base[++Iam];
    //       Rn = Pn_base[--Ibn];
    //     }

    //     Pm_base[i-len] = t0;
    //     t0 = t1; t1 = t2; t2 = 0;
    //   }

    //   while (t0)
    //     t0 = sub(Pm_base, Pn_base, t0, len);
    // }
  };

  // Initialization
  void generate_initial() {
    // Generates all stubs and initializes the entry points

    //-------------------------------------------------------------
    //-----------------------------------------------------------
    // entry points that exist in all platforms
    // Note: This is code that could be shared among different platforms - however the benefit seems to be smaller
    // than the disadvantage of having a much more complicated generator structure.
    // See also comment in stubRoutines.hpp.
    StubRoutines::_forward_exception_entry = generate_forward_exception();
    StubRoutines::_call_stub_entry = generate_call_stub(StubRoutines::_call_stub_return_address);
    // is referenced by megamorphic call
    StubRoutines::_catch_exception_entry = generate_catch_exception();

    StubRoutines::_handler_for_unsafe_access_entry = generate_handler_for_unsafe_access();

    StubRoutines::_throw_StackOverflowError_entry = generate_throw_exception("StackOverflowError throw_exception",
                                                                              CAST_FROM_FN_PTR(address, SharedRuntime::throw_StackOverflowError),   false);
  }

  void generate_all() {
    // Generates all stubs and initializes the entry points

    // These entry points require SharedInfo::stack0 to be set up in
    // non-core builds and need to be relocatable, so they each
    // fabricate a RuntimeStub internally.
    StubRoutines::_throw_AbstractMethodError_entry = generate_throw_exception("AbstractMethodError throw_exception",
                                                                               CAST_FROM_FN_PTR(address, SharedRuntime::throw_AbstractMethodError),  false);

    StubRoutines::_throw_IncompatibleClassChangeError_entry = generate_throw_exception("IncompatibleClassChangeError throw_exception",
                                                                               CAST_FROM_FN_PTR(address, SharedRuntime:: throw_IncompatibleClassChangeError), false);

    StubRoutines::_throw_NullPointerException_at_call_entry = generate_throw_exception("NullPointerException at call throw_exception",
                                                                                        CAST_FROM_FN_PTR(address, SharedRuntime::throw_NullPointerException_at_call), false);

    // entry points that are platform specific

    // support for verify_oop (must happen after universe_init)
    StubRoutines::_verify_oop_subroutine_entry     = generate_verify_oop();
#ifndef CORE
    // arraycopy stubs used by compilers
    generate_arraycopy_stubs();
#endif

    // Safefetch stubs.
    generate_safefetch("SafeFetch32", sizeof(int),     &StubRoutines::_safefetch32_entry,
                                                       &StubRoutines::_safefetch32_fault_pc,
                                                       &StubRoutines::_safefetch32_continuation_pc);
    generate_safefetch("SafeFetchN", sizeof(intptr_t), &StubRoutines::_safefetchN_entry,
                                                       &StubRoutines::_safefetchN_fault_pc,
                                                       &StubRoutines::_safefetchN_continuation_pc);

    if (UseMontgomeryMultiplyIntrinsic) {
      StubCodeMark mark(this, "StubRoutines", "montgomeryMultiply");
      MontgomeryMultiplyGenerator g(_masm, false /* squaring */);
      StubRoutines::_montgomeryMultiply = g.generate_multiply();
    }

    if (UseMontgomerySquareIntrinsic) {
      StubCodeMark mark(this, "StubRoutines", "montgomerySquare");
      MontgomeryMultiplyGenerator g(_masm, true /* squaring */);
      // We use generate_multiply() rather than generate_square()
      // because it's faster for the sizes of modulus we care about.
      StubRoutines::_montgomerySquare = g.generate_multiply();
    }

    if (UseAESIntrinsics) {
      StubRoutines::_aescrypt_encryptBlock = generate_aescrypt_encryptBlock(false);
      StubRoutines::_aescrypt_decryptBlock = generate_aescrypt_decryptBlock(false);
      StubRoutines::_cipherBlockChaining_encryptAESCrypt = generate_aescrypt_encryptBlock(true);
      StubRoutines::_cipherBlockChaining_decryptAESCrypt = generate_aescrypt_decryptBlock(true);
    }

    if (UseSHA1Intrinsics) {
      generate_sha1_implCompress("sha1_implCompress", StubRoutines::_sha1_implCompress, StubRoutines::_sha1_implCompressMB);
    }

    if (UseSHA256Intrinsics) {
      generate_sha256_implCompress("sha256_implCompress", StubRoutines::_sha256_implCompress, StubRoutines::_sha256_implCompressMB);
    }

    if (UseCRC32Intrinsics) {
      // set table address before stub generation which use it
      StubRoutines::_crc_table_adr = (address)StubRoutines::la::_crc_table;
      StubRoutines::_updateBytesCRC32 = generate_updateBytesCRC32();
    }
  }

 public:
  StubGenerator(CodeBuffer* code, bool all) : StubCodeGenerator(code) {
    if (all) {
      generate_all();
    } else {
      generate_initial();
    }
  }
}; // end class declaration

void StubGenerator_generate(CodeBuffer* code, bool all) {
  StubGenerator g(code, all);
}
