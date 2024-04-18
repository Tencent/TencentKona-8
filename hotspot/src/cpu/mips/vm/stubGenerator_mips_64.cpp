/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "interpreter/interpreter.hpp"
#include "nativeInst_mips.hpp"
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
#define T8 RT8
#define T9 RT9

#define TIMES_OOP (UseCompressedOops ? Address::times_4 : Address::times_8)
//#define a__ ((Assembler*)_masm)->

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

  // ABI mips n64
  // This fig is not MIPS ABI. It is call Java from C ABI.
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
  //  n64 does not save paras in sp.
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
  // Find a right place in the call_stub for GP.
  // GP will point to the starting point of Interpreter::dispatch_table(itos).
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
    GP_off             = -14,
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
    __ daddiu(SP, SP, total_off * wordSize);
    __ sd(BCP, FP, BCP_off * wordSize);
    __ sd(LVP, FP, LVP_off * wordSize);
    __ sd(TSR, FP, TSR_off * wordSize);
    __ sd(S1, FP, S1_off * wordSize);
    __ sd(S3, FP, S3_off * wordSize);
    __ sd(S4, FP, S4_off * wordSize);
    __ sd(S5, FP, S5_off * wordSize);
    __ sd(S6, FP, S6_off * wordSize);
    __ sd(A0, FP, call_wrapper_off * wordSize);
    __ sd(A1, FP, result_off * wordSize);
    __ sd(A2, FP, result_type_off * wordSize);
    __ sd(A7, FP, thread_off * wordSize);
    __ sd(GP, FP, GP_off * wordSize);

    __ set64(GP, (long)Interpreter::dispatch_table(itos));

#ifdef OPT_THREAD
    __ move(TREG, A7);
#endif
    //add for compressedoops
    __ reinit_heapbase();

#ifdef ASSERT
    // make sure we have no pending exceptions
    {
      Label L;
      __ ld(AT, A7, in_bytes(Thread::pending_exception_offset()));
      __ beq(AT, R0, L);
      __ delayed()->nop();
      /* FIXME: I do not know how to realize stop in mips arch, do it in the future */
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
    __ delayed()->nop();
    __ dsll(AT, A6, Interpreter::logStackElementSize);
    __ dsubu(SP, SP, AT);
    __ move(AT, -StackAlignmentInBytes);
    __ andr(SP, SP , AT);
    // Copy Java parameters in reverse order (receiver last)
    // Note that the argument order is inverted in the process
    Label loop;
    __ move(T0, A6);
    __ move(T2, R0);
    __ bind(loop);

    // get parameter
    __ dsll(T3, T0, LogBytesPerWord);
    __ daddu(T3, T3, A5);
    __ ld(AT, T3,  -wordSize);
    __ dsll(T3, T2, LogBytesPerWord);
    __ daddu(T3, T3, SP);
    __ sd(AT, T3, Interpreter::expr_offset_in_bytes(0));
    __ daddiu(T2, T2, 1);
    __ daddiu(T0, T0, -1);
    __ bne(T0, R0, loop);
    __ delayed()->nop();
    // advance to next parameter

    // call Java function
    __ bind(parameters_done);

    // receiver in V0, methodOop in Rmethod

    __ move(Rmethod, A3);
    __ move(Rsender, SP);             //set sender sp
    __ jalr(A4);
    __ delayed()->nop();
    return_address = __ pc();

    Label common_return;
    __ bind(common_return);

    // store result depending on type
    // (everything that is not T_LONG, T_FLOAT or T_DOUBLE is treated as T_INT)
    __ ld(T0, FP, result_off * wordSize);   // result --> T0
    Label is_long, is_float, is_double, exit;
    __ ld(T2, FP, result_type_off * wordSize);  // result_type --> T2
    __ daddiu(T3, T2, (-1) * T_LONG);
    __ beq(T3, R0, is_long);
    __ delayed()->daddiu(T3, T2, (-1) * T_FLOAT);
    __ beq(T3, R0, is_float);
    __ delayed()->daddiu(T3, T2, (-1) * T_DOUBLE);
    __ beq(T3, R0, is_double);
    __ delayed()->nop();

    // handle T_INT case
    __ sd(V0, T0, 0 * wordSize);
    __ bind(exit);

    // restore
    __ ld(BCP, FP, BCP_off * wordSize);
    __ ld(LVP, FP, LVP_off * wordSize);
    __ ld(GP, FP, GP_off * wordSize);
    __ ld(TSR, FP, TSR_off * wordSize);

    __ ld(S1, FP, S1_off * wordSize);
    __ ld(S3, FP, S3_off * wordSize);
    __ ld(S4, FP, S4_off * wordSize);
    __ ld(S5, FP, S5_off * wordSize);
    __ ld(S6, FP, S6_off * wordSize);

    __ leave();

    // return
    __ jr(RA);
    __ delayed()->nop();

    // handle return types different from T_INT
    __ bind(is_long);
    __ sd(V0, T0, 0 * wordSize);
    __ b(exit);
    __ delayed()->nop();

    __ bind(is_float);
    __ swc1(F0, T0, 0 * wordSize);
    __ b(exit);
    __ delayed()->nop();

    __ bind(is_double);
    __ sdc1(F0, T0, 0 * wordSize);
    __ b(exit);
    __ delayed()->nop();
    //FIXME, 1.6 mips version add operation of fpu here
    StubRoutines::gs2::set_call_stub_compiled_return(__ pc());
    __ b(common_return);
    __ delayed()->nop();
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
    __ ld(thread, FP, thread_off * wordSize);
#endif

#ifdef ASSERT
    // verify that threads correspond
    { Label L;
      __ get_thread(T8);
      __ beq(T8, thread, L);
      __ delayed()->nop();
      __ stop("StubRoutines::catch_exception: threads must correspond");
      __ bind(L);
    }
#endif
    // set pending exception
    __ verify_oop(V0);
    __ sd(V0, thread, in_bytes(Thread::pending_exception_offset()));
    __ li(AT, (long)__FILE__);
    __ sd(AT, thread, in_bytes(Thread::exception_file_offset   ()));
    __ li(AT, (long)__LINE__);
    __ sd(AT, thread, in_bytes(Thread::exception_line_offset   ()));

    // complete return to VM
    assert(StubRoutines::_call_stub_return_address != NULL, "_call_stub_return_address must have been generated before");
    __ jmp(StubRoutines::_call_stub_return_address, relocInfo::none);
    __ delayed()->nop();

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
      __ ld(AT, thread, in_bytes(Thread::pending_exception_offset()));
      __ bne(AT, R0, L);
      __ delayed()->nop();
      __ stop("StubRoutines::forward exception: no pending exception (1)");
      __ bind(L);
    }
#endif

    // compute exception handler into T9
    __ ld(A1, SP, 0);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::exception_handler_for_return_address), thread, A1);
    __ move(T9, V0);
    __ pop(V1);

#ifndef OPT_THREAD
    __ get_thread(thread);
#endif
    __ ld(V0, thread, in_bytes(Thread::pending_exception_offset()));
    __ sd(R0, thread, in_bytes(Thread::pending_exception_offset()));

#ifdef ASSERT
    // make sure exception is set
    {
      Label L;
      __ bne(V0, R0, L);
      __ delayed()->nop();
      __ stop("StubRoutines::forward exception: no pending exception (2)");
      __ bind(L);
    }
#endif

    // continue at exception handler (return address removed)
    // V0: exception
    // T9: exception handler
    // V1: throwing pc
    __ verify_oop(V0);
    __ jr(T9);
    __ delayed()->nop();

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
    __ delayed()->nop();
    __ popad_except_v0();
    __ move(RA, V0);
    __ pop(V0);
    __ jr(RA);
    __ delayed()->nop();
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
  //  Generate overlap test for array copy stubs
  //
  //  Input:
  //     A0    -  array1
  //     A1    -  array2
  //     A2    -  element count
  //

 // use T9 as temp
  void array_overlap_test(address no_overlap_target, int log2_elem_size) {
    int elem_size = 1 << log2_elem_size;
    Address::ScaleFactor sf = Address::times_1;

    switch (log2_elem_size) {
      case 0: sf = Address::times_1; break;
      case 1: sf = Address::times_2; break;
      case 2: sf = Address::times_4; break;
      case 3: sf = Address::times_8; break;
    }

    __ dsll(AT, A2, sf);
    __ daddu(AT, AT, A0);
    __ daddiu(T9, AT, -elem_size);
    __ dsubu(AT, A1, A0);
    __ blez(AT, no_overlap_target);
    __ delayed()->nop();
    __ dsubu(AT, A1, T9);
    __ bgtz(AT, no_overlap_target);
    __ delayed()->nop();

    // If A0 = 0xf... and A1 = 0x0..., than goto no_overlap_target
    Label L;
    __ bgez(A0, L);
    __ delayed()->nop();
    __ bgtz(A1, no_overlap_target);
    __ delayed()->nop();
    __ bind(L);

  }

  //
  //  Generate store check for array
  //
  //  Input:
  //     T0    -  starting address
  //     T1    -  element count
  //
  //  The 2 input registers are overwritten
  //


  void array_store_check(Register tmp) {
    assert_different_registers(tmp, AT, T0, T1);
    BarrierSet* bs = Universe::heap()->barrier_set();
    assert(bs->kind() == BarrierSet::CardTableModRef, "Wrong barrier set kind");
    CardTableModRefBS* ct = (CardTableModRefBS*)bs;
    assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");
    Label l_0;

    if (UseConcMarkSweepGC) __ sync();

    __ set64(tmp, (long)ct->byte_map_base);

    __ dsll(AT, T1, TIMES_OOP);
    __ daddu(AT, T0, AT);
    __ daddiu(T1, AT, - BytesPerHeapOop);

    __ shr(T0, CardTableModRefBS::card_shift);
    __ shr(T1, CardTableModRefBS::card_shift);

    __ dsubu(T1, T1, T0);   // end --> cards count
    __ bind(l_0);

    __ daddu(AT, tmp, T0);
    if (UseLEXT1) {
      __ gssbx(R0, AT, T1, 0);
    } else {
      __ daddu(AT, AT, T1);
      __ sb(R0, AT, 0);
    }

    __ bgtz(T1, l_0);
    __ delayed()->daddiu(T1, T1, - 1);
  }

  // Generate code for an array write pre barrier
  //
  //     addr    -  starting address
  //     count   -  element count
  //     tmp     - scratch register
  //
  //     Destroy no registers!
  //
  void  gen_write_ref_array_pre_barrier(Register addr, Register count, bool dest_uninitialized) {
    BarrierSet* bs = Universe::heap()->barrier_set();
    switch (bs->kind()) {
      case BarrierSet::G1SATBCT:
      case BarrierSet::G1SATBCTLogging:
        // With G1, don't generate the call if we statically know that the target in uninitialized
        if (!dest_uninitialized) {
           __ pushad();                      // push registers
           if (count == A0) {
             if (addr == A1) {
               // exactly backwards!!
               //__ xchgptr(c_rarg1, c_rarg0);
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
           __ popad();
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
  //  The input registers are overwritten.
  //
  void  gen_write_ref_array_post_barrier(Register start, Register count, Register scratch) {
    assert_different_registers(start, count, scratch, AT);
    BarrierSet* bs = Universe::heap()->barrier_set();
    switch (bs->kind()) {
      case BarrierSet::G1SATBCT:
      case BarrierSet::G1SATBCTLogging:
        {
          __ pushad();             // push registers (overkill)
          if (count == A0) {
            if (start == A1) {
              // exactly backwards!!
              //__ xchgptr(c_rarg1, c_rarg0);
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
          __ popad();
        }
        break;
      case BarrierSet::CardTableModRef:
      case BarrierSet::CardTableExtension:
        {
          CardTableModRefBS* ct = (CardTableModRefBS*)bs;
          assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");

          Label L_loop;
          const Register end = count;

          if (UseConcMarkSweepGC) __ sync();

          int64_t disp = (int64_t) ct->byte_map_base;
          __ set64(scratch, disp);

          __ lea(end, Address(start, count, TIMES_OOP, 0));  // end == start+count*oop_size
          __ daddiu(end, end, -BytesPerHeapOop); // end - 1 to make inclusive
          __ shr(start, CardTableModRefBS::card_shift);
          __ shr(end,   CardTableModRefBS::card_shift);
          __ dsubu(end, end, start); // end --> cards count

          __ daddu(start, start, scratch);

          __ bind(L_loop);
          if (UseLEXT1) {
            __ gssbx(R0, start, count, 0);
          } else {
            __ daddu(AT, start, count);
            __ sb(R0, AT, 0);
          }
          __ daddiu(count, count, -1);
          __ slt(AT, count, R0);
          __ beq(AT, R0, L_loop);
          __ delayed()->nop();
        }
        break;
      default:
        ShouldNotReachHere();
    }
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
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
  address generate_disjoint_byte_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);


    Register tmp1 = T0;
    Register tmp2 = T1;
    Register tmp3 = T3;

    address start = __ pc();

    __ push(tmp1);
    __ push(tmp2);
    __ push(tmp3);
    __ move(tmp1, A0);
    __ move(tmp2, A1);
    __ move(tmp3, A2);


    Label l_1, l_2, l_3, l_4, l_5, l_6, l_7, l_8, l_9, l_10, l_11;
    Label l_debug;

    __ daddiu(AT, tmp3, -9); //why the number is 9 ?
    __ blez(AT, l_9);
    __ delayed()->nop();

    if (!aligned) {
      __ xorr(AT, tmp1, tmp2);
      __ andi(AT, AT, 1);
      __ bne(AT, R0, l_9); // if arrays don't have the same alignment mod 2, do 1 element copy
      __ delayed()->nop();

      __ andi(AT, tmp1, 1);
      __ beq(AT, R0, l_10); //copy 1 enlement if necessary to aligh to 2 bytes
      __ delayed()->nop();

      __ lb(AT, tmp1, 0);
      __ daddiu(tmp1, tmp1, 1);
      __ sb(AT, tmp2, 0);
      __ daddiu(tmp2, tmp2, 1);
      __ daddiu(tmp3, tmp3, -1);
      __ bind(l_10);

      __ xorr(AT, tmp1, tmp2);
      __ andi(AT, AT, 3);
      __ bne(AT, R0, l_1); // if arrays don't have the same alignment mod 4, do 2 elements copy
      __ delayed()->nop();

      // At this point it is guaranteed that both, from and to have the same alignment mod 4.

      // Copy 2 elements if necessary to align to 4 bytes.
      __ andi(AT, tmp1, 3);
      __ beq(AT, R0, l_2);
      __ delayed()->nop();

      __ lhu(AT, tmp1, 0);
      __ daddiu(tmp1, tmp1, 2);
      __ sh(AT, tmp2, 0);
      __ daddiu(tmp2, tmp2, 2);
      __ daddiu(tmp3, tmp3, -2);
      __ bind(l_2);

      // At this point the positions of both, from and to, are at least 4 byte aligned.

      // Copy 4 elements at a time.
      // Align to 8 bytes, but only if both, from and to, have same alignment mod 8.
      __ xorr(AT, tmp1, tmp2);
      __ andi(AT, AT, 7);
      __ bne(AT, R0, l_6); // not same alignment mod 8 -> copy 2, either from or to will be unaligned
      __ delayed()->nop();

      // Copy a 4 elements if necessary to align to 8 bytes.
      __ andi(AT, tmp1, 7);
      __ beq(AT, R0, l_7);
      __ delayed()->nop();

      __ lw(AT, tmp1, 0);
      __ daddiu(tmp3, tmp3, -4);
      __ sw(AT, tmp2, 0);
      { // FasterArrayCopy
        __ daddiu(tmp1, tmp1, 4);
        __ daddiu(tmp2, tmp2, 4);
      }
    }

    __ bind(l_7);

    // Copy 4 elements at a time; either the loads or the stores can
    // be unaligned if aligned == false.

    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -7);
      __ blez(AT, l_6); // copy 4 at a time if less than 4 elements remain
      __ delayed()->nop();

      __ bind(l_8);
      // For Loongson, there is 128-bit memory access. TODO
      __ ld(AT, tmp1, 0);
      __ sd(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 8);
      __ daddiu(tmp2, tmp2, 8);
      __ daddiu(tmp3, tmp3, -8);
      __ daddiu(AT, tmp3, -8);
      __ bgez(AT, l_8);
      __ delayed()->nop();
    }
    __ bind(l_6);

    // copy 4 bytes at a time
    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -3);
      __ blez(AT, l_1);
      __ delayed()->nop();

      __ bind(l_3);
      __ lw(AT, tmp1, 0);
      __ sw(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 4);
      __ daddiu(tmp2, tmp2, 4);
      __ daddiu(tmp3, tmp3, -4);
      __ daddiu(AT, tmp3, -4);
      __ bgez(AT, l_3);
      __ delayed()->nop();

    }

    // do 2 bytes copy
    __ bind(l_1);
    {
      __ daddiu(AT, tmp3, -1);
      __ blez(AT, l_9);
      __ delayed()->nop();

      __ bind(l_5);
      __ lhu(AT, tmp1, 0);
      __ daddiu(tmp3, tmp3, -2);
      __ sh(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 2);
      __ daddiu(tmp2, tmp2, 2);
      __ daddiu(AT, tmp3, -2);
      __ bgez(AT, l_5);
      __ delayed()->nop();
    }

    //do 1 element copy--byte
    __ bind(l_9);
    __ beq(R0, tmp3, l_4);
    __ delayed()->nop();

    {
      __ bind(l_11);
      __ lb(AT, tmp1, 0);
      __ daddiu(tmp3, tmp3, -1);
      __ sb(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 1);
      __ daddiu(tmp2, tmp2, 1);
      __ daddiu(AT, tmp3, -1);
      __ bgez(AT, l_11);
      __ delayed()->nop();
    }

    __ bind(l_4);
    __ pop(tmp3);
    __ pop(tmp2);
    __ pop(tmp1);

    __ jr(RA);
    __ delayed()->nop();

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   A0   - source array address
  //   A1   - destination array address
  //   A2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  address generate_conjoint_byte_copy(bool aligned, const char *name) {
    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ pc();

    Label l_copy_4_bytes_loop, l_copy_suffix, l_copy_suffix_loop, l_exit;
    Label l_copy_byte, l_from_unaligned, l_unaligned, l_4_bytes_aligned;

    address nooverlap_target = aligned ?
      StubRoutines::arrayof_jbyte_disjoint_arraycopy() :
      StubRoutines::jbyte_disjoint_arraycopy();

    array_overlap_test(nooverlap_target, 0);

    const Register from      = A0;   // source array address
    const Register to        = A1;   // destination array address
    const Register count     = A2;   // elements count
    const Register end_from  = T3;   // source array end address
    const Register end_to    = T0;   // destination array end address
    const Register end_count = T1;   // destination array end address

    __ push(end_from);
    __ push(end_to);
    __ push(end_count);
    __ push(T8);

    // copy from high to low
    __ move(end_count, count);
    __ daddu(end_from, from, end_count);
    __ daddu(end_to, to, end_count);

    // If end_from and end_to has differante alignment, unaligned copy is performed.
    __ andi(AT, end_from, 3);
    __ andi(T8, end_to, 3);
    __ bne(AT, T8, l_copy_byte);
    __ delayed()->nop();

    // First deal with the unaligned data at the top.
    __ bind(l_unaligned);
    __ beq(end_count, R0, l_exit);
    __ delayed()->nop();

    __ andi(AT, end_from, 3);
    __ bne(AT, R0, l_from_unaligned);
    __ delayed()->nop();

    __ andi(AT, end_to, 3);
    __ beq(AT, R0, l_4_bytes_aligned);
    __ delayed()->nop();

    __ bind(l_from_unaligned);
    __ lb(AT, end_from, -1);
    __ sb(AT, end_to, -1);
    __ daddiu(end_from, end_from, -1);
    __ daddiu(end_to, end_to, -1);
    __ daddiu(end_count, end_count, -1);
    __ b(l_unaligned);
    __ delayed()->nop();

    // now end_to, end_from point to 4-byte aligned high-ends
    //     end_count contains byte count that is not copied.
    // copy 4 bytes at a time
    __ bind(l_4_bytes_aligned);

    __ move(T8, end_count);
    __ daddiu(AT, end_count, -3);
    __ blez(AT, l_copy_suffix);
    __ delayed()->nop();

    //__ andi(T8, T8, 3);
    __ lea(end_from, Address(end_from, -4));
    __ lea(end_to, Address(end_to, -4));

    __ dsrl(end_count, end_count, 2);
    __ align(16);
    __ bind(l_copy_4_bytes_loop); //l_copy_4_bytes
    __ lw(AT, end_from, 0);
    __ sw(AT, end_to, 0);
    __ addiu(end_from, end_from, -4);
    __ addiu(end_to, end_to, -4);
    __ addiu(end_count, end_count, -1);
    __ bne(end_count, R0, l_copy_4_bytes_loop);
    __ delayed()->nop();

    __ b(l_copy_suffix);
    __ delayed()->nop();
    // copy dwords aligned or not with repeat move
    // l_copy_suffix
    // copy suffix (0-3 bytes)
    __ bind(l_copy_suffix);
    __ andi(T8, T8, 3);
    __ beq(T8, R0, l_exit);
    __ delayed()->nop();
    __ addiu(end_from, end_from, 3);
    __ addiu(end_to, end_to, 3);
    __ bind(l_copy_suffix_loop);
    __ lb(AT, end_from, 0);
    __ sb(AT, end_to, 0);
    __ addiu(end_from, end_from, -1);
    __ addiu(end_to, end_to, -1);
    __ addiu(T8, T8, -1);
    __ bne(T8, R0, l_copy_suffix_loop);
    __ delayed()->nop();

    __ bind(l_copy_byte);
    __ beq(end_count, R0, l_exit);
    __ delayed()->nop();
    __ lb(AT, end_from, -1);
    __ sb(AT, end_to, -1);
    __ daddiu(end_from, end_from, -1);
    __ daddiu(end_to, end_to, -1);
    __ daddiu(end_count, end_count, -1);
    __ b(l_copy_byte);
    __ delayed()->nop();

    __ bind(l_exit);
    __ pop(T8);
    __ pop(end_count);
    __ pop(end_to);
    __ pop(end_from);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  // Generate stub for disjoint short copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  A0
  //      to:    A1
  //  elm.count: A2 treated as signed
  //  one element: 2 bytes
  //
  // Strategy for aligned==true:
  //
  //  If length <= 9:
  //     1. copy 1 elements at a time (l_5)
  //
  //  If length > 9:
  //     1. copy 4 elements at a time until less than 4 elements are left (l_7)
  //     2. copy 2 elements at a time until less than 2 elements are left (l_6)
  //     3. copy last element if one was left in step 2. (l_1)
  //
  //
  // Strategy for aligned==false:
  //
  //  If length <= 9: same as aligned==true case
  //
  //  If length > 9:
  //     1. continue with step 7. if the alignment of from and to mod 4
  //        is different.
  //     2. align from and to to 4 bytes by copying 1 element if necessary
  //     3. at l_2 from and to are 4 byte aligned; continue with
  //        6. if they cannot be aligned to 8 bytes because they have
  //        got different alignment mod 8.
  //     4. at this point we know that both, from and to, have the same
  //        alignment mod 8, now copy one element if necessary to get
  //        8 byte alignment of from and to.
  //     5. copy 4 elements at a time until less than 4 elements are
  //        left; depending on step 3. all load/stores are aligned.
  //     6. copy 2 elements at a time until less than 2 elements are
  //        left. (l_6)
  //     7. copy 1 element at a time. (l_5)
  //     8. copy last element if one was left in step 6. (l_1)

  address generate_disjoint_short_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);

    Register tmp1 = T0;
    Register tmp2 = T1;
    Register tmp3 = T3;
    Register tmp4 = T8;
    Register tmp5 = T9;
    Register tmp6 = T2;

    address start = __ pc();

    __ push(tmp1);
    __ push(tmp2);
    __ push(tmp3);
    __ move(tmp1, A0);
    __ move(tmp2, A1);
    __ move(tmp3, A2);

    Label l_1, l_2, l_3, l_4, l_5, l_6, l_7, l_8, l_9, l_10, l_11, l_12, l_13, l_14;
    Label l_debug;
    // don't try anything fancy if arrays don't have many elements
    __ daddiu(AT, tmp3, -23);
    __ blez(AT, l_14);
    __ delayed()->nop();
    // move push here
    __ push(tmp4);
    __ push(tmp5);
    __ push(tmp6);

    if (!aligned) {
      __ xorr(AT, A0, A1);
      __ andi(AT, AT, 1);
      __ bne(AT, R0, l_debug); // if arrays don't have the same alignment mod 2, can this happen?
      __ delayed()->nop();

      __ xorr(AT, A0, A1);
      __ andi(AT, AT, 3);
      __ bne(AT, R0, l_1); // if arrays don't have the same alignment mod 4, do 1 element copy
      __ delayed()->nop();

      // At this point it is guaranteed that both, from and to have the same alignment mod 4.

      // Copy 1 element if necessary to align to 4 bytes.
      __ andi(AT, A0, 3);
      __ beq(AT, R0, l_2);
      __ delayed()->nop();

      __ lhu(AT, tmp1, 0);
      __ daddiu(tmp1, tmp1, 2);
      __ sh(AT, tmp2, 0);
      __ daddiu(tmp2, tmp2, 2);
      __ daddiu(tmp3, tmp3, -1);
      __ bind(l_2);

      // At this point the positions of both, from and to, are at least 4 byte aligned.

      // Copy 4 elements at a time.
      // Align to 8 bytes, but only if both, from and to, have same alignment mod 8.
      __ xorr(AT, tmp1, tmp2);
      __ andi(AT, AT, 7);
      __ bne(AT, R0, l_6); // not same alignment mod 8 -> copy 2, either from or to will be unaligned
      __ delayed()->nop();

      // Copy a 2-element word if necessary to align to 8 bytes.
      __ andi(AT, tmp1, 7);
      __ beq(AT, R0, l_7);
      __ delayed()->nop();

      __ lw(AT, tmp1, 0);
      __ daddiu(tmp3, tmp3, -2);
      __ sw(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 4);
      __ daddiu(tmp2, tmp2, 4);
    }// end of if (!aligned)

    __ bind(l_7);
    // At this time the position of both, from and to, are at least 8 byte aligned.
    // Copy 8 elemnets at a time.
    // Align to 16 bytes, but only if both from and to have same alignment mod 8.
    __ xorr(AT, tmp1, tmp2);
    __ andi(AT, AT, 15);
    __ bne(AT, R0, l_9);
    __ delayed()->nop();

    // Copy 4-element word if necessary to align to 16 bytes,
    __ andi(AT, tmp1, 15);
    __ beq(AT, R0, l_10);
    __ delayed()->nop();

    __ ld(AT, tmp1, 0);
    __ daddiu(tmp3, tmp3, -4);
    __ sd(AT, tmp2, 0);
    __ daddiu(tmp1, tmp1, 8);
    __ daddiu(tmp2, tmp2, 8);

    __ bind(l_10);

    // Copy 8 elements at a time; either the loads or the stores can
    // be unalligned if aligned == false

    { // FasterArrayCopy
      __ bind(l_11);
      // For loongson the 128-bit memory access instruction is gslq/gssq
      if (UseLEXT1) {
        __ gslq(AT, tmp4, tmp1, 0);
        __ gslq(tmp5, tmp6, tmp1, 16);
        __ daddiu(tmp1, tmp1, 32);
        __ daddiu(tmp2, tmp2, 32);
        __ gssq(AT, tmp4, tmp2, -32);
        __ gssq(tmp5, tmp6, tmp2, -16);
      } else {
        __ ld(AT, tmp1, 0);
        __ ld(tmp4, tmp1, 8);
        __ ld(tmp5, tmp1, 16);
        __ ld(tmp6, tmp1, 24);
        __ daddiu(tmp1, tmp1, 32);
        __ sd(AT, tmp2, 0);
        __ sd(tmp4, tmp2, 8);
        __ sd(tmp5, tmp2, 16);
        __ sd(tmp6, tmp2, 24);
        __ daddiu(tmp2, tmp2, 32);
      }
      __ daddiu(tmp3, tmp3, -16);
      __ daddiu(AT, tmp3, -16);
      __ bgez(AT, l_11);
      __ delayed()->nop();
    }
    __ bind(l_9);

    // Copy 4 elements at a time; either the loads or the stores can
    // be unaligned if aligned == false.
    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -15);// loop unrolling 4 times, so if the elements should not be less than 16
      __ blez(AT, l_4); // copy 2 at a time if less than 16 elements remain
      __ delayed()->nop();

      __ bind(l_8);
      __ ld(AT, tmp1, 0);
      __ ld(tmp4, tmp1, 8);
      __ ld(tmp5, tmp1, 16);
      __ ld(tmp6, tmp1, 24);
      __ sd(AT, tmp2, 0);
      __ sd(tmp4, tmp2, 8);
      __ sd(tmp5, tmp2,16);
      __ daddiu(tmp1, tmp1, 32);
      __ daddiu(tmp2, tmp2, 32);
      __ daddiu(tmp3, tmp3, -16);
      __ daddiu(AT, tmp3, -16);
      __ bgez(AT, l_8);
      __ delayed()->sd(tmp6, tmp2, -8);
    }
    __ bind(l_6);

    // copy 2 element at a time
    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -7);
      __ blez(AT, l_4);
      __ delayed()->nop();

      __ bind(l_3);
      __ lw(AT, tmp1, 0);
      __ lw(tmp4, tmp1, 4);
      __ lw(tmp5, tmp1, 8);
      __ lw(tmp6, tmp1, 12);
      __ sw(AT, tmp2, 0);
      __ sw(tmp4, tmp2, 4);
      __ sw(tmp5, tmp2, 8);
      __ daddiu(tmp1, tmp1, 16);
      __ daddiu(tmp2, tmp2, 16);
      __ daddiu(tmp3, tmp3, -8);
      __ daddiu(AT, tmp3, -8);
      __ bgez(AT, l_3);
      __ delayed()->sw(tmp6, tmp2, -4);
    }

    __ bind(l_1);
    // do single element copy (8 bit), can this happen?
    { // FasterArrayCopy
      __ daddiu(AT, tmp3, -3);
      __ blez(AT, l_4);
      __ delayed()->nop();

      __ bind(l_5);
      __ lhu(AT, tmp1, 0);
      __ lhu(tmp4, tmp1, 2);
      __ lhu(tmp5, tmp1, 4);
      __ lhu(tmp6, tmp1, 6);
      __ sh(AT, tmp2, 0);
      __ sh(tmp4, tmp2, 2);
      __ sh(tmp5, tmp2, 4);
      __ daddiu(tmp1, tmp1, 8);
      __ daddiu(tmp2, tmp2, 8);
      __ daddiu(tmp3, tmp3, -4);
      __ daddiu(AT, tmp3, -4);
      __ bgez(AT, l_5);
      __ delayed()->sh(tmp6, tmp2, -2);
    }
    // single element
    __ bind(l_4);

    __ pop(tmp6);
    __ pop(tmp5);
    __ pop(tmp4);

    __ bind(l_14);
    { // FasterArrayCopy
      __ beq(R0, tmp3, l_13);
      __ delayed()->nop();

      __ bind(l_12);
      __ lhu(AT, tmp1, 0);
      __ sh(AT, tmp2, 0);
      __ daddiu(tmp1, tmp1, 2);
      __ daddiu(tmp2, tmp2, 2);
      __ daddiu(tmp3, tmp3, -1);
      __ daddiu(AT, tmp3, -1);
      __ bgez(AT, l_12);
      __ delayed()->nop();
    }

    __ bind(l_13);
    __ pop(tmp3);
    __ pop(tmp2);
    __ pop(tmp1);

    __ jr(RA);
    __ delayed()->nop();

    __ bind(l_debug);
    __ stop("generate_disjoint_short_copy should not reach here");
    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4- or 2-byte boundaries, we
  // let the hardware handle it.  The two or four words within dwords
  // or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  address generate_conjoint_short_copy(bool aligned, const char *name) {
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    Label l_exit, l_copy_short, l_from_unaligned, l_unaligned, l_4_bytes_aligned;

    address nooverlap_target = aligned ?
            StubRoutines::arrayof_jshort_disjoint_arraycopy() :
            StubRoutines::jshort_disjoint_arraycopy();

    array_overlap_test(nooverlap_target, 1);

    const Register from      = A0;   // source array address
    const Register to        = A1;   // destination array address
    const Register count     = A2;   // elements count
    const Register end_from  = T3;   // source array end address
    const Register end_to    = T0;   // destination array end address
    const Register end_count = T1;   // destination array end address

    __ push(end_from);
    __ push(end_to);
    __ push(end_count);
    __ push(T8);

    // copy from high to low
    __ move(end_count, count);
    __ sll(AT, end_count, Address::times_2);
    __ daddu(end_from, from, AT);
    __ daddu(end_to, to, AT);

    // If end_from and end_to has differante alignment, unaligned copy is performed.
    __ andi(AT, end_from, 3);
    __ andi(T8, end_to, 3);
    __ bne(AT, T8, l_copy_short);
    __ delayed()->nop();

    // First deal with the unaligned data at the top.
    __ bind(l_unaligned);
    __ beq(end_count, R0, l_exit);
    __ delayed()->nop();

    __ andi(AT, end_from, 3);
    __ bne(AT, R0, l_from_unaligned);
    __ delayed()->nop();

    __ andi(AT, end_to, 3);
    __ beq(AT, R0, l_4_bytes_aligned);
    __ delayed()->nop();

    // Copy 1 element if necessary to align to 4 bytes.
    __ bind(l_from_unaligned);
    __ lhu(AT, end_from, -2);
    __ sh(AT, end_to, -2);
    __ daddiu(end_from, end_from, -2);
    __ daddiu(end_to, end_to, -2);
    __ daddiu(end_count, end_count, -1);
    __ b(l_unaligned);
    __ delayed()->nop();

    // now end_to, end_from point to 4-byte aligned high-ends
    //     end_count contains byte count that is not copied.
    // copy 4 bytes at a time
    __ bind(l_4_bytes_aligned);

    __ daddiu(AT, end_count, -1);
    __ blez(AT, l_copy_short);
    __ delayed()->nop();

    __ lw(AT, end_from, -4);
    __ sw(AT, end_to, -4);
    __ addiu(end_from, end_from, -4);
    __ addiu(end_to, end_to, -4);
    __ addiu(end_count, end_count, -2);
    __ b(l_4_bytes_aligned);
    __ delayed()->nop();

    // copy 1 element at a time
    __ bind(l_copy_short);
    __ beq(end_count, R0, l_exit);
    __ delayed()->nop();
    __ lhu(AT, end_from, -2);
    __ sh(AT, end_to, -2);
    __ daddiu(end_from, end_from, -2);
    __ daddiu(end_to, end_to, -2);
    __ daddiu(end_count, end_count, -1);
    __ b(l_copy_short);
    __ delayed()->nop();

    __ bind(l_exit);
    __ pop(T8);
    __ pop(end_count);
    __ pop(end_to);
    __ pop(end_from);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  // Side Effects:
  //   disjoint_int_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_int_oop_copy().
  //
  address generate_disjoint_int_oop_copy(bool aligned, bool is_oop, const char *name, bool dest_uninitialized = false) {
    Label l_3, l_4, l_5, l_6, l_7;
    StubCodeMark mark(this, "StubRoutines", name);

    __ align(CodeEntryAlignment);
    address start = __ pc();
    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ push(T8);
    __ push(T9);
    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);

    if (is_oop) {
      gen_write_ref_array_pre_barrier(A1, A2, dest_uninitialized);
    }

    if(!aligned) {
      __ xorr(AT, T3, T0);
      __ andi(AT, AT, 7);
      __ bne(AT, R0, l_5); // not same alignment mod 8 -> copy 1 element each time
      __ delayed()->nop();

      __ andi(AT, T3, 7);
      __ beq(AT, R0, l_6); //copy 2 elements each time
      __ delayed()->nop();

      __ lw(AT, T3, 0);
      __ daddiu(T1, T1, -1);
      __ sw(AT, T0, 0);
      __ daddiu(T3, T3, 4);
      __ daddiu(T0, T0, 4);
    }

    {
      __ bind(l_6);
      __ daddiu(AT, T1, -1);
      __ blez(AT, l_5);
      __ delayed()->nop();

      __ bind(l_7);
      __ ld(AT, T3, 0);
      __ sd(AT, T0, 0);
      __ daddiu(T3, T3, 8);
      __ daddiu(T0, T0, 8);
      __ daddiu(T1, T1, -2);
      __ daddiu(AT, T1, -2);
      __ bgez(AT, l_7);
      __ delayed()->nop();
    }

    __ bind(l_5);
    __ beq(T1, R0, l_4);
    __ delayed()->nop();

    __ align(16);
    __ bind(l_3);
    __ lw(AT, T3, 0);
    __ sw(AT, T0, 0);
    __ addiu(T3, T3, 4);
    __ addiu(T0, T0, 4);
    __ addiu(T1, T1, -1);
    __ bne(T1, R0, l_3);
    __ delayed()->nop();

    // exit
    __ bind(l_4);
    if (is_oop) {
      gen_write_ref_array_post_barrier(A1, A2, T1);
    }
    __ pop(T9);
    __ pop(T8);
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  address generate_conjoint_int_oop_copy(bool aligned, bool is_oop, const char *name, bool dest_uninitialized = false) {
    Label l_2, l_4;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();
    address nooverlap_target;

    if (is_oop) {
      nooverlap_target = aligned ?
              StubRoutines::arrayof_oop_disjoint_arraycopy() :
              StubRoutines::oop_disjoint_arraycopy();
    } else {
      nooverlap_target = aligned ?
              StubRoutines::arrayof_jint_disjoint_arraycopy() :
              StubRoutines::jint_disjoint_arraycopy();
    }

    array_overlap_test(nooverlap_target, 2);

    if (is_oop) {
      gen_write_ref_array_pre_barrier(A1, A2, dest_uninitialized);
    }

    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ push(T8);
    __ push(T9);

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);

    // T3: source array address
    // T0: destination array address
    // T1: element count

    __ sll(AT, T1, Address::times_4);
    __ addu(AT, T3, AT);
    __ daddiu(T3, AT, -4);
    __ sll(AT, T1, Address::times_4);
    __ addu(AT, T0, AT);
    __ daddiu(T0, AT, -4);

    __ beq(T1, R0, l_4);
    __ delayed()->nop();

    __ align(16);
    __ bind(l_2);
    __ lw(AT, T3, 0);
    __ sw(AT, T0, 0);
    __ addiu(T3, T3, -4);
    __ addiu(T0, T0, -4);
    __ addiu(T1, T1, -1);
    __ bne(T1, R0, l_2);
    __ delayed()->nop();

    __ bind(l_4);
    if (is_oop) {
      gen_write_ref_array_post_barrier(A1, A2, T1);
    }
    __ pop(T9);
    __ pop(T8);
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();

    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  // Side Effects:
  //   disjoint_int_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_int_oop_copy().
  //
  address generate_disjoint_long_oop_copy(bool aligned, bool is_oop, const char *name, bool dest_uninitialized = false) {
    Label l_3, l_4;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    if (is_oop) {
      gen_write_ref_array_pre_barrier(A1, A2, dest_uninitialized);
    }

    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ push(T8);
    __ push(T9);

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);

    // T3: source array address
    // T0: destination array address
    // T1: element count

    __ beq(T1, R0, l_4);
    __ delayed()->nop();

    __ align(16);
    __ bind(l_3);
    __ ld(AT, T3, 0);
    __ sd(AT, T0, 0);
    __ addiu(T3, T3, 8);
    __ addiu(T0, T0, 8);
    __ addiu(T1, T1, -1);
    __ bne(T1, R0, l_3);
    __ delayed()->nop();

    // exit
    __ bind(l_4);
    if (is_oop) {
      gen_write_ref_array_post_barrier(A1, A2, T1);
    }
    __ pop(T9);
    __ pop(T8);
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  address generate_conjoint_long_oop_copy(bool aligned, bool is_oop, const char *name, bool dest_uninitialized = false) {
    Label l_2, l_4;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();
    address nooverlap_target;

    if (is_oop) {
      nooverlap_target = aligned ?
              StubRoutines::arrayof_oop_disjoint_arraycopy() :
              StubRoutines::oop_disjoint_arraycopy();
    } else {
      nooverlap_target = aligned ?
              StubRoutines::arrayof_jlong_disjoint_arraycopy() :
              StubRoutines::jlong_disjoint_arraycopy();
    }

    array_overlap_test(nooverlap_target, 3);

    if (is_oop) {
      gen_write_ref_array_pre_barrier(A1, A2, dest_uninitialized);
    }

    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ push(T8);
    __ push(T9);

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);

    __ sll(AT, T1, Address::times_8);
    __ addu(AT, T3, AT);
    __ daddiu(T3, AT, -8);
    __ sll(AT, T1, Address::times_8);
    __ addu(AT, T0, AT);
    __ daddiu(T0, AT, -8);

    __ beq(T1, R0, l_4);
    __ delayed()->nop();

    __ align(16);
    __ bind(l_2);
    __ ld(AT, T3, 0);
    __ sd(AT, T0, 0);
    __ addiu(T3, T3, -8);
    __ addiu(T0, T0, -8);
    __ addiu(T1, T1, -1);
    __ bne(T1, R0, l_2);
    __ delayed()->nop();

    // exit
    __ bind(l_4);
    if (is_oop) {
      gen_write_ref_array_post_barrier(A1, A2, T1);
    }
    __ pop(T9);
    __ pop(T8);
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  //FIXME
  address generate_disjoint_long_copy(bool aligned, const char *name) {
    Label l_1, l_2;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);
    __ push(T3);
    __ push(T0);
    __ push(T1);
    __ b(l_2);
    __ delayed()->nop();
    __ align(16);
    __ bind(l_1);
    __ ld(AT, T3, 0);
    __ sd (AT, T0, 0);
    __ addiu(T3, T3, 8);
    __ addiu(T0, T0, 8);
    __ bind(l_2);
    __ addiu(T1, T1, -1);
    __ bgez(T1, l_1);
    __ delayed()->nop();
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }


  address generate_conjoint_long_copy(bool aligned, const char *name) {
    Label l_1, l_2;
    StubCodeMark mark(this, "StubRoutines", name);
    __ align(CodeEntryAlignment);
    address start = __ pc();
    address nooverlap_target = aligned ?
      StubRoutines::arrayof_jlong_disjoint_arraycopy() :
      StubRoutines::jlong_disjoint_arraycopy();
    array_overlap_test(nooverlap_target, 3);

    __ push(T3);
    __ push(T0);
    __ push(T1);

    __ move(T1, A2);
    __ move(T3, A0);
    __ move(T0, A1);
    __ sll(AT, T1, Address::times_8);
    __ addu(AT, T3, AT);
    __ daddiu(T3, AT, -8);
    __ sll(AT, T1, Address::times_8);
    __ addu(AT, T0, AT);
    __ daddiu(T0, AT, -8);

    __ b(l_2);
    __ delayed()->nop();
    __ align(16);
    __ bind(l_1);
    __ ld(AT, T3, 0);
    __ sd (AT, T0, 0);
    __ addiu(T3, T3, -8);
    __ addiu(T0, T0,-8);
    __ bind(l_2);
    __ addiu(T1, T1, -1);
    __ bgez(T1, l_1);
    __ delayed()->nop();
    __ pop(T1);
    __ pop(T0);
    __ pop(T3);
    __ jr(RA);
    __ delayed()->nop();
    return start;
  }

  void generate_arraycopy_stubs() {
    if (UseCompressedOops) {
      StubRoutines::_oop_disjoint_arraycopy          = generate_disjoint_int_oop_copy(false, true,
                                                                                      "oop_disjoint_arraycopy");
      StubRoutines::_oop_arraycopy                   = generate_conjoint_int_oop_copy(false, true,
                                                                                      "oop_arraycopy");
      StubRoutines::_oop_disjoint_arraycopy_uninit   = generate_disjoint_int_oop_copy(false, true,
                                                                                      "oop_disjoint_arraycopy_uninit", true);
      StubRoutines::_oop_arraycopy_uninit            = generate_conjoint_int_oop_copy(false, true,
                                                                                      "oop_arraycopy_uninit", true);
    } else {
      StubRoutines::_oop_disjoint_arraycopy          = generate_disjoint_long_oop_copy(false, true,
                                                                                       "oop_disjoint_arraycopy");
      StubRoutines::_oop_arraycopy                   = generate_conjoint_long_oop_copy(false, true,
                                                                                       "oop_arraycopy");
      StubRoutines::_oop_disjoint_arraycopy_uninit   = generate_disjoint_long_oop_copy(false, true,
                                                                                       "oop_disjoint_arraycopy_uninit", true);
      StubRoutines::_oop_arraycopy_uninit            = generate_conjoint_long_oop_copy(false, true,
                                                                                       "oop_arraycopy_uninit", true);
    }

    StubRoutines::_jbyte_disjoint_arraycopy          = generate_disjoint_byte_copy(false, "jbyte_disjoint_arraycopy");
    StubRoutines::_jshort_disjoint_arraycopy         = generate_disjoint_short_copy(false, "jshort_disjoint_arraycopy");
    StubRoutines::_jint_disjoint_arraycopy           = generate_disjoint_int_oop_copy(false, false, "jint_disjoint_arraycopy");
    StubRoutines::_jlong_disjoint_arraycopy          = generate_disjoint_long_copy(false, "jlong_disjoint_arraycopy");

    StubRoutines::_jbyte_arraycopy  = generate_conjoint_byte_copy(false, "jbyte_arraycopy");
    StubRoutines::_jshort_arraycopy = generate_conjoint_short_copy(false, "jshort_arraycopy");
    StubRoutines::_jint_arraycopy   = generate_conjoint_int_oop_copy(false, false, "jint_arraycopy");
    StubRoutines::_jlong_arraycopy  = generate_conjoint_long_copy(false, "jlong_arraycopy");

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
        __ lw(A1, A0, 0);
        break;
      case 8:
        // int64_t
        __ ld(A1, A0, 0);
        break;
      default:
        ShouldNotReachHere();
    }

    // return errValue or *adr
    *continuation_pc = __ pc();
    __ addu(V0,A1,R0);
    __ jr(RA);
    __ delayed()->nop();
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
      __ ld(RA, java_thread, in_bytes(JavaThread::saved_exception_pc_offset()));
    }

    __ enter(); // required for proper stackwalking of RuntimeStub frame

    __ addiu(SP, SP, (-1) * (framesize-2) * wordSize); // prolog
    __ sd(S0, SP, S0_off * wordSize);
    __ sd(S1, SP, S1_off * wordSize);
    __ sd(S2, SP, S2_off * wordSize);
    __ sd(S3, SP, S3_off * wordSize);
    __ sd(S4, SP, S4_off * wordSize);
    __ sd(S5, SP, S5_off * wordSize);
    __ sd(S6, SP, S6_off * wordSize);
    __ sd(S7, SP, S7_off * wordSize);

    int frame_complete = __ pc() - start;
    // push java thread (becomes first argument of C function)
    __ sd(java_thread, SP, thread_off * wordSize);
    if (java_thread != A0)
      __ move(A0, java_thread);

    // Set up last_Java_sp and last_Java_fp
    __ set_last_Java_frame(java_thread, SP, FP, NULL);
    // Align stack
    __ set64(AT, -(StackAlignmentInBytes));
    __ andr(SP, SP, AT);

    __ relocate(relocInfo::internal_pc_type);
    {
      intptr_t save_pc = (intptr_t)__ pc() +  NativeMovConstReg::instruction_size + 28;
      __ patchable_set48(AT, save_pc);
    }
    __ sd(AT, java_thread, in_bytes(JavaThread::last_Java_pc_offset()));

    // Call runtime
    __ call(runtime_entry);
    __ delayed()->nop();
    // Generate oop map
    OopMap* map =  new OopMap(framesize, 0);
    oop_maps->add_gc_map(__ offset(),  map);

    // restore the thread (cannot use the pushed argument since arguments
    // may be overwritten by C code generated by an optimizing compiler);
    // however can use the register value directly if it is callee saved.
#ifndef OPT_THREAD
    __ get_thread(java_thread);
#endif

    __ ld(SP, java_thread, in_bytes(JavaThread::last_Java_sp_offset()));
    __ reset_last_Java_frame(java_thread, true);

    // Restore callee save registers.  This must be done after resetting the Java frame
    __ ld(S0, SP, S0_off * wordSize);
    __ ld(S1, SP, S1_off * wordSize);
    __ ld(S2, SP, S2_off * wordSize);
    __ ld(S3, SP, S3_off * wordSize);
    __ ld(S4, SP, S4_off * wordSize);
    __ ld(S5, SP, S5_off * wordSize);
    __ ld(S6, SP, S6_off * wordSize);
    __ ld(S7, SP, S7_off * wordSize);

    // discard arguments
    __ move(SP, FP); // epilog
    __ pop(FP);
    // check for pending exceptions
#ifdef ASSERT
    Label L;
    __ ld(AT, java_thread, in_bytes(Thread::pending_exception_offset()));
    __ bne(AT, R0, L);
    __ delayed()->nop();
    __ should_not_reach_here();
    __ bind(L);
#endif //ASSERT
    __ jmp(StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);
    __ delayed()->nop();
    RuntimeStub* stub = RuntimeStub::new_runtime_stub(name,
                                                      &code,
                                                      frame_complete,
                                                      framesize,
                                                      oop_maps, false);
    return stub->entry_point();
  }

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
      StubRoutines::_montgomeryMultiply
        = CAST_FROM_FN_PTR(address, SharedRuntime::montgomery_multiply);
    }
    if (UseMontgomerySquareIntrinsic) {
      StubRoutines::_montgomerySquare
        = CAST_FROM_FN_PTR(address, SharedRuntime::montgomery_square);
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
