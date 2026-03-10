/*
 * Copyright (c) 1999, 2021, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, 2022, Loongson Technology. All rights reserved.
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
#include "asm/assembler.hpp"
#include "c1/c1_CodeStubs.hpp"
#include "c1/c1_Defs.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "compiler/disassembler.hpp"
#include "compiler/oopMap.hpp"
#include "interpreter/interpreter.hpp"
#include "memory/universe.hpp"
#include "nativeInst_loongarch.hpp"
#include "oops/compiledICHolder.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "register_loongarch.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/signature.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/vframe.hpp"
#include "runtime/vframeArray.hpp"
#include "vmreg_loongarch.inline.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#endif

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
#define T5 RT5
#define T6 RT6
#define T8 RT8

// Implementation of StubAssembler

int StubAssembler::call_RT(Register oop_result1, Register metadata_result, address entry, int args_size) {
  // setup registers
  assert(!(oop_result1->is_valid() || metadata_result->is_valid()) || oop_result1 != metadata_result,
         "registers must be different");
  assert(oop_result1 != TREG && metadata_result != TREG, "registers must be different");
  assert(args_size >= 0, "illegal args_size");
  bool align_stack = false;

  move(A0, TREG);
  set_num_rt_args(0); // Nothing on stack

  Label retaddr;
  set_last_Java_frame(SP, FP, retaddr);

  // do the call
  call(entry, relocInfo::runtime_call_type);
  bind(retaddr);
  int call_offset = offset();
  // verify callee-saved register
#ifdef ASSERT
  { Label L;
    get_thread(SCR1);
    beq(TREG, SCR1, L);
    stop("StubAssembler::call_RT: TREG not callee saved?");
    bind(L);
  }
#endif
  reset_last_Java_frame(true);

  // check for pending exceptions
  { Label L;
    // check for pending exceptions (java_thread is set upon return)
    ld_ptr(SCR1, Address(TREG, in_bytes(Thread::pending_exception_offset())));
    beqz(SCR1, L);
    // exception pending => remove activation and forward to exception handler
    // make sure that the vm_results are cleared
    if (oop_result1->is_valid()) {
      st_ptr(R0, Address(TREG, JavaThread::vm_result_offset()));
    }
    if (metadata_result->is_valid()) {
      st_ptr(R0, Address(TREG, JavaThread::vm_result_2_offset()));
    }
    if (frame_size() == no_frame_size) {
      leave();
      jmp(StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);
    } else if (_stub_id == Runtime1::forward_exception_id) {
      should_not_reach_here();
    } else {
      jmp(Runtime1::entry_for(Runtime1::forward_exception_id), relocInfo::runtime_call_type);
    }
    bind(L);
  }
  // get oop results if there are any and reset the values in the thread
  if (oop_result1->is_valid()) {
    get_vm_result(oop_result1, TREG);
  }
  if (metadata_result->is_valid()) {
    get_vm_result_2(metadata_result, TREG);
  }
  return call_offset;
}

int StubAssembler::call_RT(Register oop_result1, Register metadata_result,
                           address entry, Register arg1) {
  move(A1, arg1);
  return call_RT(oop_result1, metadata_result, entry, 1);
}

int StubAssembler::call_RT(Register oop_result1, Register metadata_result,
                           address entry, Register arg1, Register arg2) {
  if (A1 == arg2) {
    if (A2 == arg1) {
      move(SCR1, arg1);
      move(arg1, arg2);
      move(arg2, SCR1);
    } else {
      move(A2, arg2);
      move(A1, arg1);
    }
  } else {
    move(A1, arg1);
    move(A2, arg2);
  }
  return call_RT(oop_result1, metadata_result, entry, 2);
}

int StubAssembler::call_RT(Register oop_result1, Register metadata_result,
                           address entry, Register arg1, Register arg2, Register arg3) {
  // if there is any conflict use the stack
  if (arg1 == A2 || arg1 == A3 ||
      arg2 == A1 || arg2 == A3 ||
      arg3 == A1 || arg3 == A2) {
    addi_d(SP, SP, -4 * wordSize);
    st_ptr(arg1, Address(SP, 0 * wordSize));
    st_ptr(arg2, Address(SP, 1 * wordSize));
    st_ptr(arg3, Address(SP, 2 * wordSize));
    ld_ptr(arg1, Address(SP, 0 * wordSize));
    ld_ptr(arg2, Address(SP, 1 * wordSize));
    ld_ptr(arg3, Address(SP, 2 * wordSize));
    addi_d(SP, SP, 4 * wordSize);
  } else {
    move(A1, arg1);
    move(A2, arg2);
    move(A3, arg3);
  }
  return call_RT(oop_result1, metadata_result, entry, 3);
}

// Implementation of StubFrame

class StubFrame: public StackObj {
 private:
  StubAssembler* _sasm;

 public:
  StubFrame(StubAssembler* sasm, const char* name, bool must_gc_arguments);
  void load_argument(int offset_in_words, Register reg);

  ~StubFrame();
};;

#define __ _sasm->

StubFrame::StubFrame(StubAssembler* sasm, const char* name, bool must_gc_arguments) {
  _sasm = sasm;
  __ set_info(name, must_gc_arguments);
  __ enter();
}

// load parameters that were stored with LIR_Assembler::store_parameter
// Note: offsets for store_parameter and load_argument must match
void StubFrame::load_argument(int offset_in_words, Register reg) {
  __ load_parameter(offset_in_words, reg);
}

StubFrame::~StubFrame() {
  __ leave();
  __ jr(RA);
}

#undef __

// Implementation of Runtime1

#define __ sasm->

const int float_regs_as_doubles_size_in_slots = pd_nof_fpu_regs_frame_map * 2;

// Stack layout for saving/restoring  all the registers needed during a runtime
// call (this includes deoptimization)
// Note: note that users of this frame may well have arguments to some runtime
// while these values are on the stack. These positions neglect those arguments
// but the code in save_live_registers will take the argument count into
// account.
//

enum reg_save_layout {
  reg_save_frame_size = 32 /* float */ + 30 /* integer, except zr, tp */
};

// Save off registers which might be killed by calls into the runtime.
// Tries to smart of about FP registers.  In particular we separate
// saving and describing the FPU registers for deoptimization since we
// have to save the FPU registers twice if we describe them.  The
// deopt blob is the only thing which needs to describe FPU registers.
// In all other cases it should be sufficient to simply save their
// current value.

static int cpu_reg_save_offsets[FrameMap::nof_cpu_regs];
static int fpu_reg_save_offsets[FrameMap::nof_fpu_regs];
static int reg_save_size_in_words;
static int frame_size_in_bytes = -1;

static OopMap* generate_oop_map(StubAssembler* sasm, bool save_fpu_registers) {
  int frame_size_in_bytes = reg_save_frame_size * BytesPerWord;
  sasm->set_frame_size(frame_size_in_bytes / BytesPerWord);
  int frame_size_in_slots = frame_size_in_bytes / VMRegImpl::stack_slot_size;
  OopMap* oop_map = new OopMap(frame_size_in_slots, 0);

  for (int i = A0->encoding(); i <= T8->encoding(); i++) {
    Register r = as_Register(i);
    if (i != SCR1->encoding() && i != SCR2->encoding()) {
      int sp_offset = cpu_reg_save_offsets[i];
      oop_map->set_callee_saved(VMRegImpl::stack2reg(sp_offset), r->as_VMReg());
    }
  }

  if (save_fpu_registers) {
    for (int i = 0; i < FrameMap::nof_fpu_regs; i++) {
      FloatRegister r = as_FloatRegister(i);
      int sp_offset = fpu_reg_save_offsets[i];
      oop_map->set_callee_saved(VMRegImpl::stack2reg(sp_offset), r->as_VMReg());
    }
  }

  return oop_map;
}

static OopMap* save_live_registers(StubAssembler* sasm,
                                   bool save_fpu_registers = true) {
  __ block_comment("save_live_registers");

  // integer registers except zr & ra & tp & sp
  __ addi_d(SP, SP, -(32 - 4 + 32) * wordSize);

  for (int i = 4; i < 32; i++)
    __ st_ptr(as_Register(i), Address(SP, (32 + i - 4) * wordSize));

  if (save_fpu_registers) {
    for (int i = 0; i < 32; i++)
      __ fst_d(as_FloatRegister(i), Address(SP, i * wordSize));
  }

  return generate_oop_map(sasm, save_fpu_registers);
}

static void restore_live_registers(StubAssembler* sasm, bool restore_fpu_registers = true) {
  if (restore_fpu_registers) {
    for (int i = 0; i < 32; i ++)
      __ fld_d(as_FloatRegister(i), Address(SP, i * wordSize));
  }

  for (int i = 4; i < 32; i++)
    __ ld_ptr(as_Register(i), Address(SP, (32 + i - 4) * wordSize));

  __ addi_d(SP, SP, (32 - 4 + 32) * wordSize);
}

static void restore_live_registers_except_a0(StubAssembler* sasm, bool restore_fpu_registers = true)  {
  if (restore_fpu_registers) {
    for (int i = 0; i < 32; i ++)
      __ fld_d(as_FloatRegister(i), Address(SP, i * wordSize));
  }

  for (int i = 5; i < 32; i++)
    __ ld_ptr(as_Register(i), Address(SP, (32 + i - 4) * wordSize));

  __ addi_d(SP, SP, (32 - 4 + 32) * wordSize);
}

void Runtime1::initialize_pd() {
  int sp_offset = 0;
  int i;

  // all float registers are saved explicitly
  assert(FrameMap::nof_fpu_regs == 32, "double registers not handled here");
  for (i = 0; i < FrameMap::nof_fpu_regs; i++) {
    fpu_reg_save_offsets[i] = sp_offset;
    sp_offset += 2; // SP offsets are in halfwords
  }

  for (i = 4; i < FrameMap::nof_cpu_regs; i++) {
    Register r = as_Register(i);
    cpu_reg_save_offsets[i] = sp_offset;
    sp_offset += 2; // SP offsets are in halfwords
  }
}

// target: the entry point of the method that creates and posts the exception oop
// has_argument: true if the exception needs arguments (passed in SCR1 and SCR2)

OopMapSet* Runtime1::generate_exception_throw(StubAssembler* sasm, address target,
                                              bool has_argument) {
  // make a frame and preserve the caller's caller-save registers
  OopMap* oop_map = save_live_registers(sasm);
  int call_offset;
  if (!has_argument) {
    call_offset = __ call_RT(noreg, noreg, target);
  } else {
    __ move(A1, SCR1);
    __ move(A2, SCR2);
    call_offset = __ call_RT(noreg, noreg, target);
  }
  OopMapSet* oop_maps = new OopMapSet();
  oop_maps->add_gc_map(call_offset, oop_map);
  return oop_maps;
}

OopMapSet* Runtime1::generate_handle_exception(StubID id, StubAssembler *sasm) {
  __ block_comment("generate_handle_exception");

  // incoming parameters
  const Register exception_oop = A0;
  const Register exception_pc  = A1;
  // other registers used in this stub

  // Save registers, if required.
  OopMapSet* oop_maps = new OopMapSet();
  OopMap* oop_map = NULL;
  switch (id) {
  case forward_exception_id:
    // We're handling an exception in the context of a compiled frame.
    // The registers have been saved in the standard places.  Perform
    // an exception lookup in the caller and dispatch to the handler
    // if found.  Otherwise unwind and dispatch to the callers
    // exception handler.
    oop_map = generate_oop_map(sasm, 1 /*thread*/);

    // load and clear pending exception oop into A0
    __ ld_ptr(exception_oop, Address(TREG, Thread::pending_exception_offset()));
    __ st_ptr(R0, Address(TREG, Thread::pending_exception_offset()));

    // load issuing PC (the return address for this stub) into A1
    __ ld_ptr(exception_pc, Address(FP, 1 * BytesPerWord));

    // make sure that the vm_results are cleared (may be unnecessary)
    __ st_ptr(R0, Address(TREG, JavaThread::vm_result_offset()));
    __ st_ptr(R0, Address(TREG, JavaThread::vm_result_2_offset()));
    break;
  case handle_exception_nofpu_id:
  case handle_exception_id:
    // At this point all registers MAY be live.
    oop_map = save_live_registers(sasm, id != handle_exception_nofpu_id);
    break;
  case handle_exception_from_callee_id: {
    // At this point all registers except exception oop (A0) and
    // exception pc (RA) are dead.
    const int frame_size = 2 /*fp, return address*/;
    oop_map = new OopMap(frame_size * VMRegImpl::slots_per_word, 0);
    sasm->set_frame_size(frame_size);
    break;
  }
  default: ShouldNotReachHere();
  }

  // verify that only A0 and A1 are valid at this time
  __ invalidate_registers(false, true, true, true, true, true);
  // verify that A0 contains a valid exception
  __ verify_not_null_oop(exception_oop);

#ifdef ASSERT
  // check that fields in JavaThread for exception oop and issuing pc are
  // empty before writing to them
  Label oop_empty;
  __ ld_ptr(SCR1, Address(TREG, JavaThread::exception_oop_offset()));
  __ beqz(SCR1, oop_empty);
  __ stop("exception oop already set");
  __ bind(oop_empty);

  Label pc_empty;
  __ ld_ptr(SCR1, Address(TREG, JavaThread::exception_pc_offset()));
  __ beqz(SCR1, pc_empty);
  __ stop("exception pc already set");
  __ bind(pc_empty);
#endif

  // save exception oop and issuing pc into JavaThread
  // (exception handler will load it from here)
  __ st_ptr(exception_oop, Address(TREG, JavaThread::exception_oop_offset()));
  __ st_ptr(exception_pc, Address(TREG, JavaThread::exception_pc_offset()));

  // patch throwing pc into return address (has bci & oop map)
  __ st_ptr(exception_pc, Address(FP, 1 * BytesPerWord));

  // compute the exception handler.
  // the exception oop and the throwing pc are read from the fields in JavaThread
  int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, exception_handler_for_pc));
  oop_maps->add_gc_map(call_offset, oop_map);

  // A0: handler address
  //      will be the deopt blob if nmethod was deoptimized while we looked up
  //      handler regardless of whether handler existed in the nmethod.

  // only A0 is valid at this time, all other registers have been destroyed by the runtime call
  __ invalidate_registers(false, true, true, true, true, true);

  // patch the return address, this stub will directly return to the exception handler
  __ st_ptr(A0, Address(FP, 1 * BytesPerWord));

  switch (id) {
    case forward_exception_id:
    case handle_exception_nofpu_id:
    case handle_exception_id:
      // Restore the registers that were saved at the beginning.
      restore_live_registers(sasm, id != handle_exception_nofpu_id);
      break;
    case handle_exception_from_callee_id:
      break;
    default:  ShouldNotReachHere();
  }

  return oop_maps;
}

void Runtime1::generate_unwind_exception(StubAssembler *sasm) {
  // incoming parameters
  const Register exception_oop = A0;
  // callee-saved copy of exception_oop during runtime call
  const Register exception_oop_callee_saved = S0;
  // other registers used in this stub
  const Register exception_pc = A1;
  const Register handler_addr = A3;

  // verify that only A0, is valid at this time
  __ invalidate_registers(false, true, true, true, true, true);

#ifdef ASSERT
  // check that fields in JavaThread for exception oop and issuing pc are empty
  Label oop_empty;
  __ ld_ptr(SCR1, Address(TREG, JavaThread::exception_oop_offset()));
  __ beqz(SCR1, oop_empty);
  __ stop("exception oop must be empty");
  __ bind(oop_empty);

  Label pc_empty;
  __ ld_ptr(SCR1, Address(TREG, JavaThread::exception_pc_offset()));
  __ beqz(SCR1, pc_empty);
  __ stop("exception pc must be empty");
  __ bind(pc_empty);
#endif

  // Save our return address because
  // exception_handler_for_return_address will destroy it.  We also
  // save exception_oop
  __ addi_d(SP, SP, -2 * wordSize);
  __ st_ptr(RA, Address(SP, 0 * wordSize));
  __ st_ptr(exception_oop, Address(SP, 1 * wordSize));

  // search the exception handler address of the caller (using the return address)
  __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::exception_handler_for_return_address), TREG, RA);
  // V0: exception handler address of the caller

  // Only V0 is valid at this time; all other registers have been
  // destroyed by the call.
  __ invalidate_registers(false, true, true, true, false, true);

  // move result of call into correct register
  __ move(handler_addr, A0);

  // get throwing pc (= return address).
  // RA has been destroyed by the call
  __ ld_ptr(RA, Address(SP, 0 * wordSize));
  __ ld_ptr(exception_oop, Address(SP, 1 * wordSize));
  __ addi_d(SP, SP, 2 * wordSize);
  __ move(A1, RA);

  __ verify_not_null_oop(exception_oop);

  // continue at exception handler (return address removed)
  // note: do *not* remove arguments when unwinding the
  //       activation since the caller assumes having
  //       all arguments on the stack when entering the
  //       runtime to determine the exception handler
  //       (GC happens at call site with arguments!)
  // A0: exception oop
  // A1: throwing pc
  // A3: exception handler
  __ jr(handler_addr);
}

OopMapSet* Runtime1::generate_patching(StubAssembler* sasm, address target) {
  // use the maximum number of runtime-arguments here because it is difficult to
  // distinguish each RT-Call.
  // Note: This number affects also the RT-Call in generate_handle_exception because
  //       the oop-map is shared for all calls.
  DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
  assert(deopt_blob != NULL, "deoptimization blob must have been created");

  OopMap* oop_map = save_live_registers(sasm);

  __ move(A0, TREG);
  Label retaddr;
  __ set_last_Java_frame(SP, FP, retaddr);
  // do the call
  __ call(target, relocInfo::runtime_call_type);
  __ bind(retaddr);
  OopMapSet* oop_maps = new OopMapSet();
  oop_maps->add_gc_map(__ offset(), oop_map);
  // verify callee-saved register
#ifdef ASSERT
  { Label L;
    __ get_thread(SCR1);
    __ beq(TREG, SCR1, L);
    __ stop("StubAssembler::call_RT: rthread not callee saved?");
    __ bind(L);
  }
#endif

  __ reset_last_Java_frame(true);

#ifdef ASSERT
  // check that fields in JavaThread for exception oop and issuing pc are empty
  Label oop_empty;
  __ ld_ptr(SCR1, Address(TREG, Thread::pending_exception_offset()));
  __ beqz(SCR1, oop_empty);
  __ stop("exception oop must be empty");
  __ bind(oop_empty);

  Label pc_empty;
  __ ld_ptr(SCR1, Address(TREG, JavaThread::exception_pc_offset()));
  __ beqz(SCR1, pc_empty);
  __ stop("exception pc must be empty");
  __ bind(pc_empty);
#endif

  // Runtime will return true if the nmethod has been deoptimized, this is the
  // expected scenario and anything else is  an error. Note that we maintain a
  // check on the result purely as a defensive measure.
  Label no_deopt;
  __ beqz(A0, no_deopt); // Have we deoptimized?

  // Perform a re-execute. The proper return  address is already on the stack,
  // we just need  to restore registers, pop  all of our frame  but the return
  // address and jump to the deopt blob.
  restore_live_registers(sasm);
  __ leave();
  __ jmp(deopt_blob->unpack_with_reexecution(), relocInfo::runtime_call_type);

  __ bind(no_deopt);
  restore_live_registers(sasm);
  __ leave();
  __ jr(RA);

  return oop_maps;
}

OopMapSet* Runtime1::generate_code_for(StubID id, StubAssembler* sasm) {
  // for better readability
  const bool must_gc_arguments = true;
  const bool dont_gc_arguments = false;

  // default value; overwritten for some optimized stubs that are called
  // from methods that do not use the fpu
  bool save_fpu_registers = true;

  // stub code & info for the different stubs
  OopMapSet* oop_maps = NULL;
  OopMap* oop_map = NULL;
  switch (id) {
    {
    case forward_exception_id:
      {
        oop_maps = generate_handle_exception(id, sasm);
        __ leave();
        __ jr(RA);
      }
      break;

    case throw_div0_exception_id:
      {
        StubFrame f(sasm, "throw_div0_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_div0_exception), false);
      }
      break;

    case throw_null_pointer_exception_id:
      {
        StubFrame f(sasm, "throw_null_pointer_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_null_pointer_exception), false);
      }
      break;

    case new_instance_id:
    case fast_new_instance_id:
    case fast_new_instance_init_check_id:
      {
        Register klass = A3; // Incoming
        Register obj   = A0; // Result

        if (id == new_instance_id) {
          __ set_info("new_instance", dont_gc_arguments);
        } else if (id == fast_new_instance_id) {
          __ set_info("fast new_instance", dont_gc_arguments);
        } else {
          assert(id == fast_new_instance_init_check_id, "bad StubID");
          __ set_info("fast new_instance init check", dont_gc_arguments);
        }

        // If TLAB is disabled, see if there is support for inlining contiguous
        // allocations.
        // Otherwise, just go to the slow path.
        if ((id == fast_new_instance_id || id == fast_new_instance_init_check_id) &&
            !UseTLAB && Universe::heap()->supports_inline_contig_alloc()) {
          Label slow_path;
          Register obj_size = S0;
          Register t1       = T0;
          Register t2       = T1;
          assert_different_registers(klass, obj, obj_size, t1, t2);

          __ addi_d(SP, SP, -2 * wordSize);
          __ st_ptr(S0, Address(SP, 0));

          if (id == fast_new_instance_init_check_id) {
            // make sure the klass is initialized
            __ ld_bu(SCR1, Address(klass, InstanceKlass::init_state_offset()));
            __ li(SCR2, InstanceKlass::fully_initialized);
            __ bne_far(SCR1, SCR2, slow_path);
          }

#ifdef ASSERT
          // assert object can be fast path allocated
          {
            Label ok, not_ok;
            __ ld_w(obj_size, Address(klass, Klass::layout_helper_offset()));
            __ bge(R0, obj_size, not_ok); // make sure it's an instance (LH > 0)
            __ andi(SCR1, obj_size, Klass::_lh_instance_slow_path_bit);
            __ beqz(SCR1, ok);
            __ bind(not_ok);
            __ stop("assert(can be fast path allocated)");
            __ should_not_reach_here();
            __ bind(ok);
          }
#endif // ASSERT

          // get the instance size (size is postive so movl is fine for 64bit)
          __ ld_w(obj_size, Address(klass, Klass::layout_helper_offset()));

          __ eden_allocate(obj, obj_size, 0, t1, slow_path);

          __ initialize_object(obj, klass, obj_size, 0, t1, t2, /* is_tlab_allocated */ false);
          __ verify_oop(obj);
          __ ld_ptr(S0, Address(SP, 0));
          __ addi_d(SP, SP, 2 * wordSize);
          __ jr(RA);

          __ bind(slow_path);
          __ ld_ptr(S0, Address(SP, 0));
          __ addi_d(SP, SP, 2 * wordSize);
        }

        __ enter();
        OopMap* map = save_live_registers(sasm);
        int call_offset = __ call_RT(obj, noreg, CAST_FROM_FN_PTR(address, new_instance), klass);
        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, map);
        restore_live_registers_except_a0(sasm);
        __ verify_oop(obj);
        __ leave();
        __ jr(RA);

        // A0,: new instance
      }

      break;

    case counter_overflow_id:
      {
        Register bci = A0, method = A1;
        __ enter();
        OopMap* map = save_live_registers(sasm);
        // Retrieve bci
        __ ld_w(bci, Address(FP, 2 * BytesPerWord));
        // And a pointer to the Method*
        __ ld_d(method, Address(FP, 3 * BytesPerWord));
        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, counter_overflow), bci, method);
        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, map);
        restore_live_registers(sasm);
        __ leave();
        __ jr(RA);
      }
      break;

    case new_type_array_id:
    case new_object_array_id:
      {
        Register length   = S0; // Incoming
        Register klass    = A3; // Incoming
        Register obj      = A0; // Result

        if (id == new_type_array_id) {
          __ set_info("new_type_array", dont_gc_arguments);
        } else {
          __ set_info("new_object_array", dont_gc_arguments);
        }

#ifdef ASSERT
        // assert object type is really an array of the proper kind
        {
          Label ok;
          Register t0 = obj;
          __ ld_w(t0, Address(klass, Klass::layout_helper_offset()));
          __ srai_w(t0, t0, Klass::_lh_array_tag_shift);
          int tag = ((id == new_type_array_id)
                     ? Klass::_lh_array_tag_type_value
                     : Klass::_lh_array_tag_obj_value);
          __ li(SCR1, tag);
          __ beq(t0, SCR1, ok);
          __ stop("assert(is an array klass)");
          __ should_not_reach_here();
          __ bind(ok);
        }
#endif // ASSERT

        // If TLAB is disabled, see if there is support for inlining contiguous
        // allocations.
        // Otherwise, just go to the slow path.
        if (!UseTLAB && Universe::heap()->supports_inline_contig_alloc()) {
          Register arr_size = A5;
          Register t1       = T0;
          Register t2       = T1;
          Label slow_path;
          assert_different_registers(length, klass, obj, arr_size, t1, t2);

          // check that array length is small enough for fast path.
          __ li(SCR1, C1_MacroAssembler::max_array_allocation_length);
          __ blt_far(SCR1, length, slow_path, false);

          // get the allocation size: round_up(hdr + length << (layout_helper & 0x1F))
          // since size is positive ldrw does right thing on 64bit
          __ ld_w(t1, Address(klass, Klass::layout_helper_offset()));
          // since size is positive movw does right thing on 64bit
          __ move(arr_size, length);
          __ sll_w(arr_size, length, t1);
          __ bstrpick_d(t1, t1, Klass::_lh_header_size_shift +
                        exact_log2(Klass::_lh_header_size_mask + 1) - 1,
                        Klass::_lh_header_size_shift);
          __ add_d(arr_size, arr_size, t1);
          __ addi_d(arr_size, arr_size, MinObjAlignmentInBytesMask); // align up
          __ bstrins_d(arr_size, R0, exact_log2(MinObjAlignmentInBytesMask + 1) - 1, 0);

          __ eden_allocate(obj, arr_size, 0, t1, slow_path); // preserves arr_size

          __ initialize_header(obj, klass, length, t1, t2);
          __ ld_bu(t1, Address(klass, in_bytes(Klass::layout_helper_offset()) + (Klass::_lh_header_size_shift / BitsPerByte)));
          assert(Klass::_lh_header_size_shift % BitsPerByte == 0, "bytewise");
          assert(Klass::_lh_header_size_mask <= 0xFF, "bytewise");
          __ andi(t1, t1, Klass::_lh_header_size_mask);
          __ sub_d(arr_size, arr_size, t1); // body length
          __ add_d(t1, t1, obj); // body start
          __ initialize_body(t1, arr_size, 0, t1, t2);
          __ membar(Assembler::StoreStore);
          __ verify_oop(obj);

          __ jr(RA);

          __ bind(slow_path);
        }

        __ enter();
        OopMap* map = save_live_registers(sasm);
        int call_offset;
        if (id == new_type_array_id) {
          call_offset = __ call_RT(obj, noreg, CAST_FROM_FN_PTR(address, new_type_array), klass, length);
        } else {
          call_offset = __ call_RT(obj, noreg, CAST_FROM_FN_PTR(address, new_object_array), klass, length);
        }

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, map);
        restore_live_registers_except_a0(sasm);

        __ verify_oop(obj);
        __ leave();
        __ jr(RA);

        // A0: new array
      }
      break;

    case new_multi_array_id:
      {
        StubFrame f(sasm, "new_multi_array", dont_gc_arguments);
        // A0,: klass
        // S0,: rank
        // A2: address of 1st dimension
        OopMap* map = save_live_registers(sasm);
        __ move(A1, A0);
        __ move(A3, A2);
        __ move(A2, S0);
        int call_offset = __ call_RT(A0, noreg, CAST_FROM_FN_PTR(address, new_multi_array), A1, A2, A3);

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, map);
        restore_live_registers_except_a0(sasm);

        // A0,: new multi array
        __ verify_oop(A0);
      }
      break;

    case register_finalizer_id:
      {
        __ set_info("register_finalizer", dont_gc_arguments);

        // This is called via call_runtime so the arguments
        // will be place in C abi locations

        __ verify_oop(A0);

        // load the klass and check the has finalizer flag
        Label register_finalizer;
        Register t = A5;
        __ load_klass(t, A0);
        __ ld_w(t, Address(t, Klass::access_flags_offset()));
        __ li(SCR1, JVM_ACC_HAS_FINALIZER);
        __ andr(SCR1, t, SCR1);
        __ bnez(SCR1, register_finalizer);
        __ jr(RA);

        __ bind(register_finalizer);
        __ enter();
        OopMap* oop_map = save_live_registers(sasm);
        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, SharedRuntime::register_finalizer), A0);
        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, oop_map);

        // Now restore all the live registers
        restore_live_registers(sasm);

        __ leave();
        __ jr(RA);
      }
      break;

    case throw_class_cast_exception_id:
      {
        StubFrame f(sasm, "throw_class_cast_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_class_cast_exception), true);
      }
      break;

    case throw_incompatible_class_change_error_id:
      {
        StubFrame f(sasm, "throw_incompatible_class_cast_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_incompatible_class_change_error), false);
      }
      break;

    case slow_subtype_check_id:
      {
        // Typical calling sequence:
        // __ push(klass_RInfo);  // object klass or other subclass
        // __ push(sup_k_RInfo);  // array element klass or other superclass
        // __ bl(slow_subtype_check);
        // Note that the subclass is pushed first, and is therefore deepest.
        enum layout {
          a0_off, a0_off_hi,
          a2_off, a2_off_hi,
          a4_off, a4_off_hi,
          a5_off, a5_off_hi,
          sup_k_off, sup_k_off_hi,
          klass_off, klass_off_hi,
          framesize,
          result_off = sup_k_off
        };

        __ set_info("slow_subtype_check", dont_gc_arguments);
        __ addi_d(SP, SP, -4 * wordSize);
        __ st_ptr(A0, Address(SP, a0_off * VMRegImpl::stack_slot_size));
        __ st_ptr(A2, Address(SP, a2_off * VMRegImpl::stack_slot_size));
        __ st_ptr(A4, Address(SP, a4_off * VMRegImpl::stack_slot_size));
        __ st_ptr(A5, Address(SP, a5_off * VMRegImpl::stack_slot_size));

        // This is called by pushing args and not with C abi
        __ ld_ptr(A4, Address(SP, klass_off * VMRegImpl::stack_slot_size)); // subclass
        __ ld_ptr(A0, Address(SP, sup_k_off * VMRegImpl::stack_slot_size)); // superclass

        Label miss;
        __ check_klass_subtype_slow_path(A4, A0, A2, A5, NULL, &miss);

        // fallthrough on success:
        __ li(SCR1, 1);
        __ st_ptr(SCR1, Address(SP, result_off * VMRegImpl::stack_slot_size)); // result
        __ ld_ptr(A0, Address(SP, a0_off * VMRegImpl::stack_slot_size));
        __ ld_ptr(A2, Address(SP, a2_off * VMRegImpl::stack_slot_size));
        __ ld_ptr(A4, Address(SP, a4_off * VMRegImpl::stack_slot_size));
        __ ld_ptr(A5, Address(SP, a5_off * VMRegImpl::stack_slot_size));
        __ addi_d(SP, SP, 4 * wordSize);
        __ jr(RA);

        __ bind(miss);
        __ st_ptr(R0, Address(SP, result_off * VMRegImpl::stack_slot_size)); // result
        __ ld_ptr(A0, Address(SP, a0_off * VMRegImpl::stack_slot_size));
        __ ld_ptr(A2, Address(SP, a2_off * VMRegImpl::stack_slot_size));
        __ ld_ptr(A4, Address(SP, a4_off * VMRegImpl::stack_slot_size));
        __ ld_ptr(A5, Address(SP, a5_off * VMRegImpl::stack_slot_size));
        __ addi_d(SP, SP, 4 * wordSize);
        __ jr(RA);
      }
      break;

    case monitorenter_nofpu_id:
      save_fpu_registers = false;
      // fall through
    case monitorenter_id:
      {
        StubFrame f(sasm, "monitorenter", dont_gc_arguments);
        OopMap* map = save_live_registers(sasm, save_fpu_registers);

        // Called with store_parameter and not C abi

        f.load_argument(1, A0); // A0,: object
        f.load_argument(0, A1); // A1,: lock address

        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, monitorenter), A0, A1);

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, map);
        restore_live_registers(sasm, save_fpu_registers);
      }
      break;

    case monitorexit_nofpu_id:
      save_fpu_registers = false;
      // fall through
    case monitorexit_id:
      {
        StubFrame f(sasm, "monitorexit", dont_gc_arguments);
        OopMap* map = save_live_registers(sasm, save_fpu_registers);

        // Called with store_parameter and not C abi

        f.load_argument(0, A0); // A0,: lock address

        // note: really a leaf routine but must setup last java sp
        //       => use call_RT for now (speed can be improved by
        //       doing last java sp setup manually)
        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, monitorexit), A0);

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, map);
        restore_live_registers(sasm, save_fpu_registers);
      }
      break;

    case deoptimize_id:
      {
        StubFrame f(sasm, "deoptimize", dont_gc_arguments);
        OopMap* oop_map = save_live_registers(sasm);
        f.load_argument(0, A1);
        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, deoptimize), A1);

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, oop_map);
        restore_live_registers(sasm);
        DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
        assert(deopt_blob != NULL, "deoptimization blob must have been created");
        __ leave();
        __ jmp(deopt_blob->unpack_with_reexecution(), relocInfo::runtime_call_type);
      }
      break;

    case throw_range_check_failed_id:
      {
        StubFrame f(sasm, "range_check_failed", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_range_check_exception), true);
      }
      break;

    case unwind_exception_id:
      {
        __ set_info("unwind_exception", dont_gc_arguments);
        // note: no stubframe since we are about to leave the current
        //       activation and we are calling a leaf VM function only.
        generate_unwind_exception(sasm);
      }
      break;

    case access_field_patching_id:
      {
        StubFrame f(sasm, "access_field_patching", dont_gc_arguments);
        // we should set up register map
        oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, access_field_patching));
      }
      break;

    case load_klass_patching_id:
      {
        StubFrame f(sasm, "load_klass_patching", dont_gc_arguments);
        // we should set up register map
        oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_klass_patching));
      }
      break;

    case load_mirror_patching_id:
      {
        StubFrame f(sasm, "load_mirror_patching", dont_gc_arguments);
        // we should set up register map
        oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_mirror_patching));
      }
      break;

    case load_appendix_patching_id:
      {
        StubFrame f(sasm, "load_appendix_patching", dont_gc_arguments);
        // we should set up register map
        oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_appendix_patching));
      }
      break;

    case handle_exception_nofpu_id:
    case handle_exception_id:
      {
        StubFrame f(sasm, "handle_exception", dont_gc_arguments);
        oop_maps = generate_handle_exception(id, sasm);
      }
      break;

    case handle_exception_from_callee_id:
      {
        StubFrame f(sasm, "handle_exception_from_callee", dont_gc_arguments);
        oop_maps = generate_handle_exception(id, sasm);
      }
      break;

    case throw_index_exception_id:
      {
        StubFrame f(sasm, "index_range_check_failed", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_index_exception), true);
      }
      break;

    case throw_array_store_exception_id:
      {
        StubFrame f(sasm, "throw_array_store_exception", dont_gc_arguments);
        // tos + 0: link
        //     + 1: return address
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_array_store_exception), true);
      }
      break;

#if INCLUDE_ALL_GCS

    case g1_pre_barrier_slow_id:
      {
        StubFrame f(sasm, "g1_pre_barrier", dont_gc_arguments);
        // arg0 : previous value of memory

        BarrierSet* bs = Universe::heap()->barrier_set();
        if (bs->kind() != BarrierSet::G1SATBCTLogging) {
          __ li(A0, (int)id);
          __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, unimplemented_entry), A0);
          __ should_not_reach_here();
          break;
        }

        const Register pre_val = A0;
        const Register thread = TREG;
        const Register tmp = SCR2;

        Address in_progress(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                             PtrQueue::byte_offset_of_active()));

        Address queue_index(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                             PtrQueue::byte_offset_of_index()));
        Address buffer(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                        PtrQueue::byte_offset_of_buf()));

        Label done;
        Label runtime;

        // Can we store original value in the thread's buffer?
        __ ld_ptr(tmp, queue_index);
        __ beqz(tmp, runtime);

        __ addi_d(tmp, tmp, -wordSize);
        __ st_ptr(tmp, queue_index);
        __ ld_ptr(SCR1, buffer);
        __ add_d(tmp, tmp, SCR1);
        f.load_argument(0, SCR1);
        __ st_ptr(SCR1, Address(tmp, 0));
        __ b(done);

        __ bind(runtime);
        __ pushad();
        f.load_argument(0, pre_val);
        __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::g1_wb_pre), pre_val, thread);
        __ popad();
        __ bind(done);
      }
      break;
    case g1_post_barrier_slow_id:
      {
        StubFrame f(sasm, "g1_post_barrier", dont_gc_arguments);

        // arg0: store_address
        Address store_addr(FP, 2*BytesPerWord);

        BarrierSet* bs = Universe::heap()->barrier_set();
        CardTableModRefBS* ct = (CardTableModRefBS*)bs;
        assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");

        Label done;
        Label runtime;

        // At this point we know new_value is non-NULL and the new_value crosses regions.
        // Must check to see if card is already dirty

        const Register thread = TREG;

        Address queue_index(thread, in_bytes(JavaThread::dirty_card_queue_offset() +
                                             PtrQueue::byte_offset_of_index()));
        Address buffer(thread, in_bytes(JavaThread::dirty_card_queue_offset() +
                                        PtrQueue::byte_offset_of_buf()));

        const Register card_offset = SCR2;
        // RA is free here, so we can use it to hold the byte_map_base.
        const Register byte_map_base = RA;

        assert_different_registers(card_offset, byte_map_base, SCR1);

        f.load_argument(0, card_offset);
        __ srli_d(card_offset, card_offset, CardTableModRefBS::card_shift);
        __ load_byte_map_base(byte_map_base);
        __ ldx_bu(SCR1, byte_map_base, card_offset);
        __ addi_d(SCR1, SCR1, -(int)G1SATBCardTableModRefBS::g1_young_card_val());
        __ beqz(SCR1, done);

        assert((int)CardTableModRefBS::dirty_card_val() == 0, "must be 0");

        __ membar(Assembler::StoreLoad);
        __ ldx_bu(SCR1, byte_map_base, card_offset);
        __ beqz(SCR1, done);

        // storing region crossing non-NULL, card is clean.
        // dirty card and log.
        __ stx_b(R0, byte_map_base, card_offset);

        // Convert card offset into an address in card_addr
        Register card_addr = card_offset;
        __ add_d(card_addr, byte_map_base, card_addr);

        __ ld_ptr(SCR1, queue_index);
        __ beqz(SCR1, runtime);
        __ addi_d(SCR1, SCR1, -wordSize);
        __ st_ptr(SCR1, queue_index);

        // Reuse RA to hold buffer_addr
        const Register buffer_addr = RA;

        __ ld_ptr(buffer_addr, buffer);
        __ stx_d(card_addr, buffer_addr, SCR1);
        __ b(done);

        __ bind(runtime);
        __ pushad();
        __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::g1_wb_post), card_addr, thread);
        __ popad();
        __ bind(done);

      }
      break;
#endif

    case predicate_failed_trap_id:
      {
        StubFrame f(sasm, "predicate_failed_trap", dont_gc_arguments);

        OopMap* map = save_live_registers(sasm);

        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, predicate_failed_trap));
        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, map);
        restore_live_registers(sasm);
        __ leave();
        DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
        assert(deopt_blob != NULL, "deoptimization blob must have been created");

        __ jmp(deopt_blob->unpack_with_reexecution(), relocInfo::runtime_call_type);
      }
      break;

    case dtrace_object_alloc_id:
      {
        // A0: object
        StubFrame f(sasm, "dtrace_object_alloc", dont_gc_arguments);
        save_live_registers(sasm);

        __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_object_alloc), A0);

        restore_live_registers(sasm);
      }
      break;

    default:
      {
        StubFrame f(sasm, "unimplemented entry", dont_gc_arguments);
        __ li(A0, (int)id);
        __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, unimplemented_entry), A0);
        __ should_not_reach_here();
      }
      break;
    }
  }
  return oop_maps;
}

#undef __

const char *Runtime1::pd_name_for_address(address entry) {
  Unimplemented();
  return 0;
}
