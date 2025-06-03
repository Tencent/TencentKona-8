/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2023, Loongson Technology. All rights reserved.
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
#include "code/debugInfoRec.hpp"
#include "code/icBuffer.hpp"
#include "code/vtableStubs.hpp"
#include "interpreter/interpreter.hpp"
#include "oops/compiledICHolder.hpp"
#include "prims/jvmtiRedefineClassesTrace.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/vframeArray.hpp"
#include "vmreg_loongarch.inline.hpp"
#ifdef COMPILER2
#include "opto/runtime.hpp"
#endif

#include <alloca.h>

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
#define T4 RT4
#define T5 RT5
#define T6 RT6
#define T7 RT7
#define T8 RT8

const int StackAlignmentInSlots = StackAlignmentInBytes / VMRegImpl::stack_slot_size;

class RegisterSaver {
  // Capture info about frame layout
  enum layout {
    fpr0_off = 0,
    fpr1_off,
    fpr2_off,
    fpr3_off,
    fpr4_off,
    fpr5_off,
    fpr6_off,
    fpr7_off,
    fpr8_off,
    fpr9_off,
    fpr10_off,
    fpr11_off,
    fpr12_off,
    fpr13_off,
    fpr14_off,
    fpr15_off,
    fpr16_off,
    fpr17_off,
    fpr18_off,
    fpr19_off,
    fpr20_off,
    fpr21_off,
    fpr22_off,
    fpr23_off,
    fpr24_off,
    fpr25_off,
    fpr26_off,
    fpr27_off,
    fpr28_off,
    fpr29_off,
    fpr30_off,
    fpr31_off,
    a0_off,
    a1_off,
    a2_off,
    a3_off,
    a4_off,
    a5_off,
    a6_off,
    a7_off,
    t0_off,
    t1_off,
    t2_off,
    t3_off,
    t4_off,
    t5_off,
    t6_off,
    t7_off,
    t8_off,
    s0_off,
    s1_off,
    s2_off,
    s3_off,
    s4_off,
    s5_off,
    s6_off,
    s7_off,
    s8_off,
    fp_off,
    ra_off,
    fpr_size = fpr31_off - fpr0_off + 1,
    gpr_size = ra_off - a0_off + 1,
  };

  const bool _save_vectors;
  public:
  RegisterSaver(bool save_vectors) : _save_vectors(save_vectors) {}

  OopMap* save_live_registers(MacroAssembler* masm, int additional_frame_words, int* total_frame_words);
  void restore_live_registers(MacroAssembler* masm);

  int slots_save() {
    int slots = gpr_size * VMRegImpl::slots_per_word;

    if (_save_vectors && UseLASX)
      slots += FloatRegisterImpl::slots_per_lasx_register * fpr_size;
    else if (_save_vectors && UseLSX)
      slots += FloatRegisterImpl::slots_per_lsx_register * fpr_size;
    else
      slots += FloatRegisterImpl::save_slots_per_register * fpr_size;

    return slots;
  }

  int gpr_offset(int off) {
      int slots_per_fpr = FloatRegisterImpl::save_slots_per_register;
      int slots_per_gpr = VMRegImpl::slots_per_word;

      if (_save_vectors && UseLASX)
        slots_per_fpr = FloatRegisterImpl::slots_per_lasx_register;
      else if (_save_vectors && UseLSX)
        slots_per_fpr = FloatRegisterImpl::slots_per_lsx_register;

      return (fpr_size * slots_per_fpr + (off - a0_off) * slots_per_gpr) * VMRegImpl::stack_slot_size;
  }

  int fpr_offset(int off) {
      int slots_per_fpr = FloatRegisterImpl::save_slots_per_register;

      if (_save_vectors && UseLASX)
        slots_per_fpr = FloatRegisterImpl::slots_per_lasx_register;
      else if (_save_vectors && UseLSX)
        slots_per_fpr = FloatRegisterImpl::slots_per_lsx_register;

      return off * slots_per_fpr * VMRegImpl::stack_slot_size;
  }

  int ra_offset() { return gpr_offset(ra_off); }
  int t5_offset() { return gpr_offset(t5_off); }
  int s3_offset() { return gpr_offset(s3_off); }
  int v0_offset() { return gpr_offset(a0_off); }
  int v1_offset() { return gpr_offset(a1_off); }

  int fpr0_offset() { return fpr_offset(fpr0_off); }
  int fpr1_offset() { return fpr_offset(fpr1_off); }

  // During deoptimization only the result register need to be restored
  // all the other values have already been extracted.
  void restore_result_registers(MacroAssembler* masm);
};

OopMap* RegisterSaver::save_live_registers(MacroAssembler* masm, int additional_frame_words, int* total_frame_words) {

  // Always make the frame size 16-byte aligned
  int frame_size_in_bytes = round_to(additional_frame_words * wordSize + slots_save() * VMRegImpl::stack_slot_size, StackAlignmentInBytes);
  // OopMap frame size is in compiler stack slots (jint's) not bytes or words
  int frame_size_in_slots = frame_size_in_bytes / VMRegImpl::stack_slot_size;
  // The caller will allocate additional_frame_words
  int additional_frame_slots = additional_frame_words * wordSize / VMRegImpl::stack_slot_size;
  // CodeBlob frame size is in words.
  int frame_size_in_words = frame_size_in_bytes / wordSize;

  *total_frame_words = frame_size_in_words;

  OopMapSet *oop_maps = new OopMapSet();
  OopMap* map =  new OopMap(frame_size_in_slots, 0);

  // save registers
  __ addi_d(SP, SP, -slots_save() * VMRegImpl::stack_slot_size);

  for (int i = 0; i < fpr_size; i++) {
    FloatRegister fpr = as_FloatRegister(i);
    int off = fpr_offset(i);

    if (_save_vectors && UseLASX)
      __ xvst(fpr, SP, off);
    else if (_save_vectors && UseLSX)
      __ vst(fpr, SP, off);
    else
      __ fst_d(fpr, SP, off);
    map->set_callee_saved(VMRegImpl::stack2reg(off / VMRegImpl::stack_slot_size + additional_frame_slots), fpr->as_VMReg());
  }

  for (int i = a0_off; i <= a7_off; i++) {
    Register gpr = as_Register(A0->encoding() + (i - a0_off));
    int off = gpr_offset(i);

    __ st_d(gpr, SP, gpr_offset(i));
    map->set_callee_saved(VMRegImpl::stack2reg(off / VMRegImpl::stack_slot_size + additional_frame_slots), gpr->as_VMReg());
  }

  for (int i = t0_off; i <= t6_off; i++) {
    Register gpr = as_Register(T0->encoding() + (i - t0_off));
    int off = gpr_offset(i);

    __ st_d(gpr, SP, gpr_offset(i));
    map->set_callee_saved(VMRegImpl::stack2reg(off / VMRegImpl::stack_slot_size + additional_frame_slots), gpr->as_VMReg());
  }
  __ st_d(T8, SP, gpr_offset(t8_off));
  map->set_callee_saved(VMRegImpl::stack2reg(gpr_offset(t8_off) / VMRegImpl::stack_slot_size + additional_frame_slots), T8->as_VMReg());

  for (int i = s0_off; i <= s8_off; i++) {
    Register gpr = as_Register(S0->encoding() + (i - s0_off));
    int off = gpr_offset(i);

    __ st_d(gpr, SP, gpr_offset(i));
    map->set_callee_saved(VMRegImpl::stack2reg(off / VMRegImpl::stack_slot_size + additional_frame_slots), gpr->as_VMReg());
  }

  __ st_d(FP, SP, gpr_offset(fp_off));
  map->set_callee_saved(VMRegImpl::stack2reg(gpr_offset(fp_off) / VMRegImpl::stack_slot_size + additional_frame_slots), FP->as_VMReg());
  __ st_d(RA, SP, gpr_offset(ra_off));
  map->set_callee_saved(VMRegImpl::stack2reg(gpr_offset(ra_off) / VMRegImpl::stack_slot_size + additional_frame_slots), RA->as_VMReg());

  __ addi_d(FP, SP, gpr_offset(fp_off));

  return map;
}


// Pop the current frame and restore all the registers that we
// saved.
void RegisterSaver::restore_live_registers(MacroAssembler* masm) {
  for (int i = 0; i < fpr_size; i++) {
    FloatRegister fpr = as_FloatRegister(i);
    int off = fpr_offset(i);

    if (_save_vectors && UseLASX)
      __ xvld(fpr, SP, off);
    else if (_save_vectors && UseLSX)
      __ vld(fpr, SP, off);
    else
      __ fld_d(fpr, SP, off);
  }

  for (int i = a0_off; i <= a7_off; i++) {
    Register gpr = as_Register(A0->encoding() + (i - a0_off));
    int off = gpr_offset(i);

    __ ld_d(gpr, SP, gpr_offset(i));
  }

  for (int i = t0_off; i <= t6_off; i++) {
    Register gpr = as_Register(T0->encoding() + (i - t0_off));
    int off = gpr_offset(i);

    __ ld_d(gpr, SP, gpr_offset(i));
  }
  __ ld_d(T8, SP, gpr_offset(t8_off));

  for (int i = s0_off; i <= s8_off; i++) {
    Register gpr = as_Register(S0->encoding() + (i - s0_off));
    int off = gpr_offset(i);

    __ ld_d(gpr, SP, gpr_offset(i));
  }

  __ ld_d(FP, SP, gpr_offset(fp_off));
  __ ld_d(RA, SP, gpr_offset(ra_off));

  __ addi_d(SP, SP, slots_save() * VMRegImpl::stack_slot_size);
}

// Pop the current frame and restore the registers that might be holding
// a result.
void RegisterSaver::restore_result_registers(MacroAssembler* masm) {
  // Just restore result register. Only used by deoptimization. By
  // now any callee save register that needs to be restore to a c2
  // caller of the deoptee has been extracted into the vframeArray
  // and will be stuffed into the c2i adapter we create for later
  // restoration so only result registers need to be restored here.

  __ ld_d(V0, SP, gpr_offset(a0_off));
  __ ld_d(V1, SP, gpr_offset(a1_off));

  __ fld_d(F0, SP, fpr_offset(fpr0_off));
  __ fld_d(F1, SP, fpr_offset(fpr1_off));

  __ addi_d(SP, SP, gpr_offset(ra_off));
}

// Is vector's size (in bytes) bigger than a size saved by default?
// 8 bytes registers are saved by default using fld/fst instructions.
bool SharedRuntime::is_wide_vector(int size) {
  return size > 8;
}

// The java_calling_convention describes stack locations as ideal slots on
// a frame with no abi restrictions. Since we must observe abi restrictions
// (like the placement of the register window) the slots must be biased by
// the following value.

static int reg2offset_in(VMReg r) {
  // Account for saved fp and return address
  // This should really be in_preserve_stack_slots
  return (r->reg2stack() + 2 * VMRegImpl::slots_per_word) * VMRegImpl::stack_slot_size;  // + 2 * VMRegImpl::stack_slot_size);
}

static int reg2offset_out(VMReg r) {
  return (r->reg2stack() + SharedRuntime::out_preserve_stack_slots()) * VMRegImpl::stack_slot_size;
}

// ---------------------------------------------------------------------------
// Read the array of BasicTypes from a signature, and compute where the
// arguments should go.  Values in the VMRegPair regs array refer to 4-byte
// quantities.  Values less than SharedInfo::stack0 are registers, those above
// refer to 4-byte stack slots.  All stack slots are based off of the stack pointer
// as framesizes are fixed.
// VMRegImpl::stack0 refers to the first slot 0(sp).
// and VMRegImpl::stack0+1 refers to the memory word 4-byes higher.  Register
// up to RegisterImpl::number_of_registers) are the 32-bit
// integer registers.

// Pass first five oop/int args in registers T0, A0 - A3.
// Pass float/double/long args in stack.
// Doubles have precedence, so if you pass a mix of floats and doubles
// the doubles will grab the registers before the floats will.

// Note: the INPUTS in sig_bt are in units of Java argument words, which are
// either 32-bit or 64-bit depending on the build.  The OUTPUTS are in 32-bit
// units regardless of build.


// ---------------------------------------------------------------------------
// The compiled Java calling convention.
// Pass first five oop/int args in registers T0, A0 - A3.
// Pass float/double/long args in stack.
// Doubles have precedence, so if you pass a mix of floats and doubles
// the doubles will grab the registers before the floats will.

int SharedRuntime::java_calling_convention(const BasicType *sig_bt,
                                           VMRegPair *regs,
                                           int total_args_passed,
                                           int is_outgoing) {

  // Create the mapping between argument positions and registers.
  static const Register INT_ArgReg[Argument::n_register_parameters + 1] = {
    T0, A0, A1, A2, A3, A4, A5, A6, A7
  };
  static const FloatRegister FP_ArgReg[Argument::n_float_register_parameters] = {
    FA0, FA1, FA2, FA3, FA4, FA5, FA6, FA7
  };

  uint int_args = 0;
  uint fp_args = 0;
  uint stk_args = 0; // inc by 2 each time

  for (int i = 0; i < total_args_passed; i++) {
    switch (sig_bt[i]) {
    case T_VOID:
      // halves of T_LONG or T_DOUBLE
      assert(i != 0 && (sig_bt[i - 1] == T_LONG || sig_bt[i - 1] == T_DOUBLE), "expecting half");
      regs[i].set_bad();
      break;
    case T_BOOLEAN:
    case T_CHAR:
    case T_BYTE:
    case T_SHORT:
    case T_INT:
      if (int_args < Argument::n_register_parameters + 1) {
        regs[i].set1(INT_ArgReg[int_args++]->as_VMReg());
      } else {
        regs[i].set1(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_LONG:
      assert(sig_bt[i + 1] == T_VOID, "expecting half");
      // fall through
    case T_OBJECT:
    case T_ARRAY:
    case T_ADDRESS:
      if (int_args < Argument::n_register_parameters + 1) {
        regs[i].set2(INT_ArgReg[int_args++]->as_VMReg());
      } else {
        regs[i].set2(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_FLOAT:
      if (fp_args < Argument::n_float_register_parameters) {
        regs[i].set1(FP_ArgReg[fp_args++]->as_VMReg());
      } else {
        regs[i].set1(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_DOUBLE:
      assert(sig_bt[i + 1] == T_VOID, "expecting half");
      if (fp_args < Argument::n_float_register_parameters) {
        regs[i].set2(FP_ArgReg[fp_args++]->as_VMReg());
      } else {
        regs[i].set2(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  }

  return round_to(stk_args, 2);
}

// Patch the callers callsite with entry to compiled code if it exists.
static void patch_callers_callsite(MacroAssembler *masm) {
  Label L;
  __ verify_oop(Rmethod);
  __ ld_ptr(AT, Rmethod, in_bytes(Method::code_offset()));
  __ beq(AT, R0, L);
  // Schedule the branch target address early.
  // Call into the VM to patch the caller, then jump to compiled callee
  // T5 isn't live so capture return address while we easily can
  __ move(T5, RA);

  __ pushad();
#ifdef COMPILER2
  // C2 may leave the stack dirty if not in SSE2+ mode
  __ empty_FPU_stack();
#endif

  // VM needs caller's callsite
  // VM needs target method

  __ move(A0, Rmethod);
  __ move(A1, T5);
  // we should preserve the return address
  __ verify_oop(Rmethod);
  __ move(S0, SP);
  __ li(AT, -(StackAlignmentInBytes));   // align the stack
  __ andr(SP, SP, AT);
  __ call(CAST_FROM_FN_PTR(address, SharedRuntime::fixup_callers_callsite),
          relocInfo::runtime_call_type);

  __ move(SP, S0);
  __ popad();
  __ bind(L);
}

static void gen_c2i_adapter(MacroAssembler *masm,
                            int total_args_passed,
                            int comp_args_on_stack,
                            const BasicType *sig_bt,
                            const VMRegPair *regs,
                            Label& skip_fixup) {

  // Before we get into the guts of the C2I adapter, see if we should be here
  // at all.  We've come from compiled code and are attempting to jump to the
  // interpreter, which means the caller made a static call to get here
  // (vcalls always get a compiled target if there is one).  Check for a
  // compiled target.  If there is one, we need to patch the caller's call.
  // However we will run interpreted if we come thru here. The next pass
  // thru the call site will run compiled. If we ran compiled here then
  // we can (theorectically) do endless i2c->c2i->i2c transitions during
  // deopt/uncommon trap cycles. If we always go interpreted here then
  // we can have at most one and don't need to play any tricks to keep
  // from endlessly growing the stack.
  //
  // Actually if we detected that we had an i2c->c2i transition here we
  // ought to be able to reset the world back to the state of the interpreted
  // call and not bother building another interpreter arg area. We don't
  // do that at this point.

  patch_callers_callsite(masm);
  __ bind(skip_fixup);

#ifdef COMPILER2
  __ empty_FPU_stack();
#endif
  //this is for native ?
  // Since all args are passed on the stack, total_args_passed * interpreter_
  // stack_element_size  is the
  // space we need.
  int extraspace = total_args_passed * Interpreter::stackElementSize;

  // stack is aligned, keep it that way
  extraspace = round_to(extraspace, 2*wordSize);

  // Get return address
  __ move(T5, RA);
  // set senderSP value
  //refer to interpreter_loongarch.cpp:generate_asm_entry
  __ move(Rsender, SP);
  __ addi_d(SP, SP, -extraspace);

  // Now write the args into the outgoing interpreter space
  for (int i = 0; i < total_args_passed; i++) {
    if (sig_bt[i] == T_VOID) {
      assert(i > 0 && (sig_bt[i-1] == T_LONG || sig_bt[i-1] == T_DOUBLE), "missing half");
      continue;
    }

    // st_off points to lowest address on stack.
    int st_off = ((total_args_passed - 1) - i) * Interpreter::stackElementSize;
    // Say 4 args:
    // i   st_off
    // 0   12 T_LONG
    // 1    8 T_VOID
    // 2    4 T_OBJECT
    // 3    0 T_BOOL
    VMReg r_1 = regs[i].first();
    VMReg r_2 = regs[i].second();
    if (!r_1->is_valid()) {
      assert(!r_2->is_valid(), "");
      continue;
    }
    if (r_1->is_stack()) {
      // memory to memory use fpu stack top
      int ld_off = r_1->reg2stack() * VMRegImpl::stack_slot_size + extraspace;
      if (!r_2->is_valid()) {
        __ ld_ptr(AT, Address(SP, ld_off));
        __ st_ptr(AT, Address(SP, st_off));

      } else {


        int next_off = st_off - Interpreter::stackElementSize;
        __ ld_ptr(AT, Address(SP, ld_off));
        __ st_ptr(AT, Address(SP, st_off));

        // Ref to is_Register condition
        if(sig_bt[i] == T_LONG || sig_bt[i] == T_DOUBLE)
          __ st_ptr(AT, SP, st_off - 8);
      }
    } else if (r_1->is_Register()) {
      Register r = r_1->as_Register();
      if (!r_2->is_valid()) {
          __ st_d(r, SP, st_off);
      } else {
        //FIXME, LA will not enter here
        // long/double in gpr
        __ st_d(r, SP, st_off);
        // In [java/util/zip/ZipFile.java]
        //
        //    private static native long open(String name, int mode, long lastModified);
        //    private static native int getTotal(long jzfile);
        //
        // We need to transfer T_LONG paramenters from a compiled method to a native method.
        // It's a complex process:
        //
        // Caller -> lir_static_call -> gen_resolve_stub
        //      -> -- resolve_static_call_C
        //         `- gen_c2i_adapter()  [*]
        //             |
        //       `- AdapterHandlerLibrary::get_create_apapter_index
        //      -> generate_native_entry
        //      -> InterpreterRuntime::SignatureHandlerGenerator::pass_long [**]
        //
        // In [**], T_Long parameter is stored in stack as:
        //
        //   (high)
        //    |         |
        //    -----------
        //    | 8 bytes |
        //    | (void)  |
        //    -----------
        //    | 8 bytes |
        //    | (long)  |
        //    -----------
        //    |         |
        //   (low)
        //
        // However, the sequence is reversed here:
        //
        //   (high)
        //    |         |
        //    -----------
        //    | 8 bytes |
        //    | (long)  |
        //    -----------
        //    | 8 bytes |
        //    | (void)  |
        //    -----------
        //    |         |
        //   (low)
        //
        // So I stored another 8 bytes in the T_VOID slot. It then can be accessed from generate_native_entry().
        //
        if (sig_bt[i] == T_LONG)
          __ st_d(r, SP, st_off - 8);
      }
    } else if (r_1->is_FloatRegister()) {
      assert(sig_bt[i] == T_FLOAT || sig_bt[i] == T_DOUBLE, "Must be a float register");

      FloatRegister fr = r_1->as_FloatRegister();
      if (sig_bt[i] == T_FLOAT)
        __ fst_s(fr, SP, st_off);
      else {
        __ fst_d(fr, SP, st_off);
        __ fst_d(fr, SP, st_off - 8);  // T_DOUBLE needs two slots
      }
    }
  }

  // Schedule the branch target address early.
  __ ld_ptr(AT, Rmethod, in_bytes(Method::interpreter_entry_offset()) );
  // And repush original return address
  __ move(RA, T5);
  __ jr (AT);
}

static void gen_i2c_adapter(MacroAssembler *masm,
                            int total_args_passed,
                            int comp_args_on_stack,
                            const BasicType *sig_bt,
                            const VMRegPair *regs) {

  // Generate an I2C adapter: adjust the I-frame to make space for the C-frame
  // layout.  Lesp was saved by the calling I-frame and will be restored on
  // return.  Meanwhile, outgoing arg space is all owned by the callee
  // C-frame, so we can mangle it at will.  After adjusting the frame size,
  // hoist register arguments and repack other args according to the compiled
  // code convention.  Finally, end in a jump to the compiled code.  The entry
  // point address is the start of the buffer.

  // We will only enter here from an interpreted frame and never from after
  // passing thru a c2i. Azul allowed this but we do not. If we lose the
  // race and use a c2i we will remain interpreted for the race loser(s).
  // This removes all sorts of headaches on the LA side and also eliminates
  // the possibility of having c2i -> i2c -> c2i -> ... endless transitions.

  __ move(T4, SP);

  // Cut-out for having no stack args.  Since up to 2 int/oop args are passed
  // in registers, we will occasionally have no stack args.
  int comp_words_on_stack = 0;
  if (comp_args_on_stack) {
    // Sig words on the stack are greater-than VMRegImpl::stack0.  Those in
    // registers are below.  By subtracting stack0, we either get a negative
    // number (all values in registers) or the maximum stack slot accessed.
    // int comp_args_on_stack = VMRegImpl::reg2stack(max_arg);
    // Convert 4-byte stack slots to words.
    // did LA need round? FIXME
    comp_words_on_stack = round_to(comp_args_on_stack*4, wordSize)>>LogBytesPerWord;
    // Round up to miminum stack alignment, in wordSize
    comp_words_on_stack = round_to(comp_words_on_stack, 2);
    __ addi_d(SP, SP, -comp_words_on_stack * wordSize);
  }

  // Align the outgoing SP
  __ li(AT, -(StackAlignmentInBytes));
  __ andr(SP, SP, AT);
  // push the return address on the stack (note that pushing, rather
  // than storing it, yields the correct frame alignment for the callee)
  // Put saved SP in another register
  const Register saved_sp = T5;
  __ move(saved_sp, T4);


  // Will jump to the compiled code just as if compiled code was doing it.
  // Pre-load the register-jump target early, to schedule it better.
  __ ld_d(T4, Rmethod, in_bytes(Method::from_compiled_offset()));

  // Now generate the shuffle code.  Pick up all register args and move the
  // rest through the floating point stack top.
  for (int i = 0; i < total_args_passed; i++) {
    if (sig_bt[i] == T_VOID) {
      // Longs and doubles are passed in native word order, but misaligned
      // in the 32-bit build.
      assert(i > 0 && (sig_bt[i-1] == T_LONG || sig_bt[i-1] == T_DOUBLE), "missing half");
      continue;
    }

    // Pick up 0, 1 or 2 words from SP+offset.

    assert(!regs[i].second()->is_valid() || regs[i].first()->next() == regs[i].second(), "scrambled load targets?");
    // Load in argument order going down.
    int ld_off = (total_args_passed -1 - i)*Interpreter::stackElementSize;
    // Point to interpreter value (vs. tag)
    int next_off = ld_off - Interpreter::stackElementSize;
    VMReg r_1 = regs[i].first();
    VMReg r_2 = regs[i].second();
    if (!r_1->is_valid()) {
      assert(!r_2->is_valid(), "");
      continue;
    }
    if (r_1->is_stack()) {
      // Convert stack slot to an SP offset (+ wordSize to
      // account for return address )
      // NOTICE HERE!!!! I sub a wordSize here
      int st_off = regs[i].first()->reg2stack()*VMRegImpl::stack_slot_size;
      //+ wordSize;

      if (!r_2->is_valid()) {
        __ ld_d(AT, saved_sp, ld_off);
        __ st_d(AT, SP, st_off);
      } else {
        // Interpreter local[n] == MSW, local[n+1] == LSW however locals
        // are accessed as negative so LSW is at LOW address

        // ld_off is MSW so get LSW
        // st_off is LSW (i.e. reg.first())

        // [./org/eclipse/swt/graphics/GC.java]
        // void drawImageXRender(Image srcImage, int srcX, int srcY, int srcWidth, int srcHeight,
        //  int destX, int destY, int destWidth, int destHeight,
        //  boolean simple,
        //  int imgWidth, int imgHeight,
        //  long maskPixmap,  <-- Pass T_LONG in stack
        //  int maskType);
        // Before this modification, Eclipse displays icons with solid black background.
        //
        __ ld_d(AT, saved_sp, ld_off);
        if (sig_bt[i] == T_LONG || sig_bt[i] == T_DOUBLE)
          __ ld_d(AT, saved_sp, ld_off - 8);
        __ st_d(AT, SP, st_off);
      }
    } else if (r_1->is_Register()) {  // Register argument
      Register r = r_1->as_Register();
      if (r_2->is_valid()) {
        // Remember r_1 is low address (and LSB on LA)
        // So r_2 gets loaded from high address regardless of the platform
        assert(r_2->as_Register() == r_1->as_Register(), "");
        __ ld_d(r, saved_sp, ld_off);

        //
        // For T_LONG type, the real layout is as below:
        //
        //   (high)
        //    |         |
        //    -----------
        //    | 8 bytes |
        //    | (void)  |
        //    -----------
        //    | 8 bytes |
        //    | (long)  |
        //    -----------
        //    |         |
        //   (low)
        //
        // We should load the low-8 bytes.
        //
        if (sig_bt[i] == T_LONG)
          __ ld_d(r, saved_sp, ld_off - 8);
      } else {
        __ ld_w(r, saved_sp, ld_off);
      }
    } else if (r_1->is_FloatRegister()) { // Float Register
      assert(sig_bt[i] == T_FLOAT || sig_bt[i] == T_DOUBLE, "Must be a float register");

      FloatRegister fr = r_1->as_FloatRegister();
      if (sig_bt[i] == T_FLOAT)
          __ fld_s(fr, saved_sp, ld_off);
      else {
          __ fld_d(fr, saved_sp, ld_off);
          __ fld_d(fr, saved_sp, ld_off - 8);
      }
    }
  }

  // 6243940 We might end up in handle_wrong_method if
  // the callee is deoptimized as we race thru here. If that
  // happens we don't want to take a safepoint because the
  // caller frame will look interpreted and arguments are now
  // "compiled" so it is much better to make this transition
  // invisible to the stack walking code. Unfortunately if
  // we try and find the callee by normal means a safepoint
  // is possible. So we stash the desired callee in the thread
  // and the vm will find there should this case occur.
  __ get_thread(T8);
  __ st_d(Rmethod, T8, in_bytes(JavaThread::callee_target_offset()));

  // move methodOop to T5 in case we end up in an c2i adapter.
  // the c2i adapters expect methodOop in T5 (c2) because c2's
  // resolve stubs return the result (the method) in T5.
  // I'd love to fix this.
  __ move(T5, Rmethod);
  __ jr(T4);
}

// ---------------------------------------------------------------
AdapterHandlerEntry* SharedRuntime::generate_i2c2i_adapters(MacroAssembler *masm,
                                                            int total_args_passed,
                                                            int comp_args_on_stack,
                                                            const BasicType *sig_bt,
                                                            const VMRegPair *regs,
                                                            AdapterFingerPrint* fingerprint) {
  address i2c_entry = __ pc();

  gen_i2c_adapter(masm, total_args_passed, comp_args_on_stack, sig_bt, regs);

  // -------------------------------------------------------------------------
  // Generate a C2I adapter.  On entry we know G5 holds the methodOop.  The
  // args start out packed in the compiled layout.  They need to be unpacked
  // into the interpreter layout.  This will almost always require some stack
  // space.  We grow the current (compiled) stack, then repack the args.  We
  // finally end in a jump to the generic interpreter entry point.  On exit
  // from the interpreter, the interpreter will restore our SP (lest the
  // compiled code, which relys solely on SP and not FP, get sick).

  address c2i_unverified_entry = __ pc();
  Label skip_fixup;
  {
    Register holder = T1;
    Register receiver = T0;
    Register temp = T8;
    address ic_miss = SharedRuntime::get_ic_miss_stub();

    Label missed;

    __ verify_oop(holder);
    //add for compressedoops
    __ load_klass(temp, receiver);
    __ verify_oop(temp);

    __ ld_ptr(AT, holder, CompiledICHolder::holder_klass_offset());
    __ ld_ptr(Rmethod, holder, CompiledICHolder::holder_metadata_offset());
    __ bne(AT, temp, missed);
    // Method might have been compiled since the call site was patched to
    // interpreted if that is the case treat it as a miss so we can get
    // the call site corrected.
    __ ld_ptr(AT, Rmethod, in_bytes(Method::code_offset()));
    __ beq(AT, R0, skip_fixup);
    __ bind(missed);

    __ jmp(ic_miss, relocInfo::runtime_call_type);
  }
  address c2i_entry = __ pc();

  gen_c2i_adapter(masm, total_args_passed, comp_args_on_stack, sig_bt, regs, skip_fixup);

  __ flush();
  return  AdapterHandlerLibrary::new_entry(fingerprint, i2c_entry, c2i_entry, c2i_unverified_entry);
}

int SharedRuntime::c_calling_convention(const BasicType *sig_bt,
                                         VMRegPair *regs,
                                         VMRegPair *regs2,
                                         int total_args_passed) {
  assert(regs2 == NULL, "not needed on LA");
  // Return the number of VMReg stack_slots needed for the args.
  // This value does not include an abi space (like register window
  // save area).

  // We return the amount of VMReg stack slots we need to reserve for all
  // the arguments NOT counting out_preserve_stack_slots. Since we always
  // have space for storing at least 6 registers to memory we start with that.
  // See int_stk_helper for a further discussion.
  // We return the amount of VMRegImpl stack slots we need to reserve for all
  // the arguments NOT counting out_preserve_stack_slots.
  static const Register INT_ArgReg[Argument::n_register_parameters] = {
    A0, A1, A2, A3, A4, A5, A6, A7
  };
  static const FloatRegister FP_ArgReg[Argument::n_float_register_parameters] = {
    FA0, FA1, FA2, FA3, FA4, FA5, FA6, FA7
  };
  uint int_args = 0;
  uint fp_args = 0;
  uint stk_args = 0; // inc by 2 each time

// Example:
//    n   java.lang.UNIXProcess::forkAndExec
//     private native int forkAndExec(byte[] prog,
//                                    byte[] argBlock, int argc,
//                                    byte[] envBlock, int envc,
//                                    byte[] dir,
//                                    boolean redirectErrorStream,
//                                    FileDescriptor stdin_fd,
//                                    FileDescriptor stdout_fd,
//                                    FileDescriptor stderr_fd)
// JNIEXPORT jint JNICALL
// Java_java_lang_UNIXProcess_forkAndExec(JNIEnv *env,
//                                        jobject process,
//                                        jbyteArray prog,
//                                        jbyteArray argBlock, jint argc,
//                                        jbyteArray envBlock, jint envc,
//                                        jbyteArray dir,
//                                        jboolean redirectErrorStream,
//                                        jobject stdin_fd,
//                                        jobject stdout_fd,
//                                        jobject stderr_fd)
//
// ::c_calling_convention
//  0:      // env                 <--       a0
//  1: L    // klass/obj           <-- t0 => a1
//  2: [    // prog[]              <-- a0 => a2
//  3: [    // argBlock[]          <-- a1 => a3
//  4: I    // argc                <-- a2 => a4
//  5: [    // envBlock[]          <-- a3 => a5
//  6: I    // envc                <-- a4 => a5
//  7: [    // dir[]               <-- a5 => a7
//  8: Z    // redirectErrorStream <-- a6 => sp[0]
//  9: L    // stdin               <-- a7 => sp[8]
// 10: L    // stdout              fp[16] => sp[16]
// 11: L    // stderr              fp[24] => sp[24]
//
  for (int i = 0; i < total_args_passed; i++) {
    switch (sig_bt[i]) {
    case T_VOID: // Halves of longs and doubles
      assert(i != 0 && (sig_bt[i - 1] == T_LONG || sig_bt[i - 1] == T_DOUBLE), "expecting half");
      regs[i].set_bad();
      break;
    case T_BOOLEAN:
    case T_CHAR:
    case T_BYTE:
    case T_SHORT:
    case T_INT:
      if (int_args < Argument::n_register_parameters) {
        regs[i].set1(INT_ArgReg[int_args++]->as_VMReg());
      } else {
        regs[i].set1(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_LONG:
      assert(sig_bt[i + 1] == T_VOID, "expecting half");
      // fall through
    case T_OBJECT:
    case T_ARRAY:
    case T_ADDRESS:
    case T_METADATA:
      if (int_args < Argument::n_register_parameters) {
        regs[i].set2(INT_ArgReg[int_args++]->as_VMReg());
      } else {
        regs[i].set2(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_FLOAT:
      if (fp_args < Argument::n_float_register_parameters) {
        regs[i].set1(FP_ArgReg[fp_args++]->as_VMReg());
      } else if (int_args < Argument::n_register_parameters) {
        regs[i].set1(INT_ArgReg[int_args++]->as_VMReg());
      } else {
        regs[i].set1(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_DOUBLE:
      assert(sig_bt[i + 1] == T_VOID, "expecting half");
      if (fp_args < Argument::n_float_register_parameters) {
        regs[i].set2(FP_ArgReg[fp_args++]->as_VMReg());
      } else if (int_args < Argument::n_register_parameters) {
        regs[i].set2(INT_ArgReg[int_args++]->as_VMReg());
      } else {
        regs[i].set2(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  }

  return round_to(stk_args, 2);
}

// ---------------------------------------------------------------------------
void SharedRuntime::save_native_result(MacroAssembler *masm, BasicType ret_type, int frame_slots) {
  // We always ignore the frame_slots arg and just use the space just below frame pointer
  // which by this time is free to use
  switch (ret_type) {
    case T_FLOAT:
      __ fst_s(FSF, FP, -wordSize);
      break;
    case T_DOUBLE:
      __ fst_d(FSF, FP, -wordSize );
      break;
    case T_VOID:  break;
    case T_LONG:
      __ st_d(V0, FP, -wordSize);
      break;
    case T_OBJECT:
    case T_ARRAY:
      __ st_d(V0, FP, -wordSize);
      break;
    default: {
      __ st_w(V0, FP, -wordSize);
      }
  }
}

void SharedRuntime::restore_native_result(MacroAssembler *masm, BasicType ret_type, int frame_slots) {
  // We always ignore the frame_slots arg and just use the space just below frame pointer
  // which by this time is free to use
  switch (ret_type) {
    case T_FLOAT:
      __ fld_s(FSF, FP, -wordSize);
      break;
    case T_DOUBLE:
      __ fld_d(FSF, FP, -wordSize );
      break;
    case T_LONG:
      __ ld_d(V0, FP, -wordSize);
      break;
    case T_VOID:  break;
    case T_OBJECT:
    case T_ARRAY:
      __ ld_d(V0, FP, -wordSize);
      break;
    default: {
      __ ld_w(V0, FP, -wordSize);
      }
  }
}

static void save_args(MacroAssembler *masm, int arg_count, int first_arg, VMRegPair *args) {
  for ( int i = first_arg ; i < arg_count ; i++ ) {
    if (args[i].first()->is_Register()) {
      __ push(args[i].first()->as_Register());
    } else if (args[i].first()->is_FloatRegister()) {
      __ push(args[i].first()->as_FloatRegister());
    }
  }
}

static void restore_args(MacroAssembler *masm, int arg_count, int first_arg, VMRegPair *args) {
  for ( int i = arg_count - 1 ; i >= first_arg ; i-- ) {
    if (args[i].first()->is_Register()) {
      __ pop(args[i].first()->as_Register());
    } else if (args[i].first()->is_FloatRegister()) {
      __ pop(args[i].first()->as_FloatRegister());
    }
  }
}

// A simple move of integer like type
static void simple_move32(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {
  if (src.first()->is_stack()) {
    if (dst.first()->is_stack()) {
      // stack to stack
      __ ld_w(AT, FP, reg2offset_in(src.first()));
      __ st_d(AT, SP, reg2offset_out(dst.first()));
    } else {
      // stack to reg
      __ ld_w(dst.first()->as_Register(),  FP, reg2offset_in(src.first()));
    }
  } else if (dst.first()->is_stack()) {
    // reg to stack
    __ st_d(src.first()->as_Register(), SP, reg2offset_out(dst.first()));
  } else {
    if (dst.first() != src.first()){
      __ move(dst.first()->as_Register(), src.first()->as_Register());
    }
  }
}

// An oop arg. Must pass a handle not the oop itself
static void object_move(MacroAssembler* masm,
                        OopMap* map,
                        int oop_handle_offset,
                        int framesize_in_slots,
                        VMRegPair src,
                        VMRegPair dst,
                        bool is_receiver,
                        int* receiver_offset) {

  // must pass a handle. First figure out the location we use as a handle

  if (src.first()->is_stack()) {
    // Oop is already on the stack as an argument
    Register rHandle = T5;
    Label nil;
    __ xorr(rHandle, rHandle, rHandle);
    __ ld_d(AT, FP, reg2offset_in(src.first()));
    __ beq(AT, R0, nil);
    __ lea(rHandle, Address(FP, reg2offset_in(src.first())));
    __ bind(nil);
    if(dst.first()->is_stack())__ st_d( rHandle, SP, reg2offset_out(dst.first()));
    else                       __ move( (dst.first())->as_Register(), rHandle);

    int offset_in_older_frame = src.first()->reg2stack() + SharedRuntime::out_preserve_stack_slots();
    map->set_oop(VMRegImpl::stack2reg(offset_in_older_frame + framesize_in_slots));
    if (is_receiver) {
      *receiver_offset = (offset_in_older_frame + framesize_in_slots) * VMRegImpl::stack_slot_size;
    }
  } else {
    // Oop is in an a register we must store it to the space we reserve
    // on the stack for oop_handles
    const Register rOop = src.first()->as_Register();
    assert( (rOop->encoding() >= A0->encoding()) && (rOop->encoding() <= T0->encoding()),"wrong register");
    const Register rHandle = T5;
    //Important: refer to java_calling_convertion
    int oop_slot = (rOop->encoding() - A0->encoding()) * VMRegImpl::slots_per_word + oop_handle_offset;
    int offset = oop_slot*VMRegImpl::stack_slot_size;
    Label skip;
    __ st_d( rOop , SP, offset );
    map->set_oop(VMRegImpl::stack2reg(oop_slot));
    __ xorr( rHandle, rHandle, rHandle);
    __ beq(rOop, R0, skip);
    __ lea(rHandle, Address(SP, offset));
    __ bind(skip);
    // Store the handle parameter
    if(dst.first()->is_stack())__ st_d( rHandle, SP, reg2offset_out(dst.first()));
    else                       __ move((dst.first())->as_Register(), rHandle);

    if (is_receiver) {
      *receiver_offset = offset;
    }
  }
}

// A float arg may have to do float reg int reg conversion
static void float_move(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {
  assert(!src.second()->is_valid() && !dst.second()->is_valid(), "bad float_move");
  if (src.first()->is_stack()) {
    // stack to stack/reg
    if (dst.first()->is_stack()) {
      __ ld_w(AT, FP, reg2offset_in(src.first()));
      __ st_w(AT, SP, reg2offset_out(dst.first()));
    } else if (dst.first()->is_FloatRegister()) {
      __ fld_s(dst.first()->as_FloatRegister(), FP, reg2offset_in(src.first()));
    } else {
      __ ld_w(dst.first()->as_Register(), FP, reg2offset_in(src.first()));
    }
  } else {
    // reg to stack/reg
    if(dst.first()->is_stack()) {
      __ fst_s(src.first()->as_FloatRegister(), SP, reg2offset_out(dst.first()));
    } else if (dst.first()->is_FloatRegister()) {
      __ fmov_s(dst.first()->as_FloatRegister(), src.first()->as_FloatRegister());
    } else {
      __ movfr2gr_s(dst.first()->as_Register(), src.first()->as_FloatRegister());
    }
  }
}

// A long move
static void long_move(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {

  // The only legal possibility for a long_move VMRegPair is:
  // 1: two stack slots (possibly unaligned)
  // as neither the java  or C calling convention will use registers
  // for longs.
  if (src.first()->is_stack()) {
    assert(src.second()->is_stack() && dst.second()->is_stack(), "must be all stack");
    if( dst.first()->is_stack()){
      __ ld_d(AT, FP, reg2offset_in(src.first()));
      __ st_d(AT, SP, reg2offset_out(dst.first()));
    } else {
      __ ld_d(dst.first()->as_Register(), FP, reg2offset_in(src.first()));
    }
  } else {
    if( dst.first()->is_stack()){
      __ st_d(src.first()->as_Register(), SP, reg2offset_out(dst.first()));
    } else {
      __ move(dst.first()->as_Register(), src.first()->as_Register());
    }
  }
}

// A double move
static void double_move(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {

  // The only legal possibilities for a double_move VMRegPair are:
  // The painful thing here is that like long_move a VMRegPair might be

  // Because of the calling convention we know that src is either
  //   1: a single physical register (xmm registers only)
  //   2: two stack slots (possibly unaligned)
  // dst can only be a pair of stack slots.

  if (src.first()->is_stack()) {
    // source is all stack
    if( dst.first()->is_stack()){
      __ ld_d(AT, FP, reg2offset_in(src.first()));
      __ st_d(AT, SP, reg2offset_out(dst.first()));
    } else if (dst.first()->is_FloatRegister()) {
      __ fld_d(dst.first()->as_FloatRegister(), FP, reg2offset_in(src.first()));
    } else {
      __ ld_d(dst.first()->as_Register(), FP, reg2offset_in(src.first()));
    }
  } else {
    // reg to stack/reg
    // No worries about stack alignment
    if( dst.first()->is_stack()){
      __ fst_d(src.first()->as_FloatRegister(), SP, reg2offset_out(dst.first()));
    } else if (dst.first()->is_FloatRegister()) {
      __ fmov_d(dst.first()->as_FloatRegister(), src.first()->as_FloatRegister());
    } else {
      __ movfr2gr_d(dst.first()->as_Register(), src.first()->as_FloatRegister());
    }
  }
}

static void verify_oop_args(MacroAssembler* masm,
                            methodHandle method,
                            const BasicType* sig_bt,
                            const VMRegPair* regs) {
  Register temp_reg = T4;  // not part of any compiled calling seq
  if (VerifyOops) {
    for (int i = 0; i < method->size_of_parameters(); i++) {
      if (sig_bt[i] == T_OBJECT ||
          sig_bt[i] == T_ARRAY) {
        VMReg r = regs[i].first();
        assert(r->is_valid(), "bad oop arg");
        if (r->is_stack()) {
          __ ld_d(temp_reg, Address(SP, r->reg2stack() * VMRegImpl::stack_slot_size + wordSize));
          __ verify_oop(temp_reg);
        } else {
          __ verify_oop(r->as_Register());
        }
      }
    }
  }
}

static void gen_special_dispatch(MacroAssembler* masm,
                                 methodHandle method,
                                 const BasicType* sig_bt,
                                 const VMRegPair* regs) {
  verify_oop_args(masm, method, sig_bt, regs);
  vmIntrinsics::ID iid = method->intrinsic_id();

  // Now write the args into the outgoing interpreter space
  bool     has_receiver   = false;
  Register receiver_reg   = noreg;
  int      member_arg_pos = -1;
  Register member_reg     = noreg;
  int      ref_kind       = MethodHandles::signature_polymorphic_intrinsic_ref_kind(iid);
  if (ref_kind != 0) {
    member_arg_pos = method->size_of_parameters() - 1;  // trailing MemberName argument
    member_reg = S3;  // known to be free at this point
    has_receiver = MethodHandles::ref_kind_has_receiver(ref_kind);
  } else if (iid == vmIntrinsics::_invokeBasic) {
    has_receiver = true;
  } else {
    fatal(err_msg_res("unexpected intrinsic id %d", iid));
  }

  if (member_reg != noreg) {
    // Load the member_arg into register, if necessary.
    SharedRuntime::check_member_name_argument_is_last_argument(method, sig_bt, regs);
    VMReg r = regs[member_arg_pos].first();
    if (r->is_stack()) {
      __ ld_d(member_reg, Address(SP, r->reg2stack() * VMRegImpl::stack_slot_size));
    } else {
      // no data motion is needed
      member_reg = r->as_Register();
    }
  }

  if (has_receiver) {
    // Make sure the receiver is loaded into a register.
    assert(method->size_of_parameters() > 0, "oob");
    assert(sig_bt[0] == T_OBJECT, "receiver argument must be an object");
    VMReg r = regs[0].first();
    assert(r->is_valid(), "bad receiver arg");
    if (r->is_stack()) {
      // Porting note:  This assumes that compiled calling conventions always
      // pass the receiver oop in a register.  If this is not true on some
      // platform, pick a temp and load the receiver from stack.
      fatal("receiver always in a register");
      receiver_reg = SSR;  // known to be free at this point
      __ ld_d(receiver_reg, Address(SP, r->reg2stack() * VMRegImpl::stack_slot_size));
    } else {
      // no data motion is needed
      receiver_reg = r->as_Register();
    }
  }

  // Figure out which address we are really jumping to:
  MethodHandles::generate_method_handle_dispatch(masm, iid,
                                                 receiver_reg, member_reg, /*for_compiler_entry:*/ true);
}

// ---------------------------------------------------------------------------
// Generate a native wrapper for a given method.  The method takes arguments
// in the Java compiled code convention, marshals them to the native
// convention (handlizes oops, etc), transitions to native, makes the call,
// returns to java state (possibly blocking), unhandlizes any result and
// returns.
nmethod *SharedRuntime::generate_native_wrapper(MacroAssembler* masm,
                                                methodHandle method,
                                                int compile_id,
                                                BasicType* in_sig_bt,
                                                VMRegPair* in_regs,
                                                BasicType ret_type) {
  if (method->is_method_handle_intrinsic()) {
    vmIntrinsics::ID iid = method->intrinsic_id();
    intptr_t start = (intptr_t)__ pc();
    int vep_offset = ((intptr_t)__ pc()) - start;
    gen_special_dispatch(masm,
                         method,
                         in_sig_bt,
                         in_regs);
    assert(((intptr_t)__ pc() - start - vep_offset) >= 1 * BytesPerInstWord,
           "valid size for make_non_entrant");
    int frame_complete = ((intptr_t)__ pc()) - start;  // not complete, period
    __ flush();
    int stack_slots = SharedRuntime::out_preserve_stack_slots();  // no out slots at all, actually
    return nmethod::new_native_nmethod(method,
                                       compile_id,
                                       masm->code(),
                                       vep_offset,
                                       frame_complete,
                                       stack_slots / VMRegImpl::slots_per_word,
                                       in_ByteSize(-1),
                                       in_ByteSize(-1),
                                       (OopMapSet*)NULL);
  }

  bool is_critical_native = true;
  address native_func = method->critical_native_function();
  if (native_func == NULL) {
    native_func = method->native_function();
    is_critical_native = false;
  }
  assert(native_func != NULL, "must have function");

  // Native nmethod wrappers never take possesion of the oop arguments.
  // So the caller will gc the arguments. The only thing we need an
  // oopMap for is if the call is static
  //
  // An OopMap for lock (and class if static), and one for the VM call itself
  OopMapSet *oop_maps = new OopMapSet();

  // We have received a description of where all the java arg are located
  // on entry to the wrapper. We need to convert these args to where
  // the jni function will expect them. To figure out where they go
  // we convert the java signature to a C signature by inserting
  // the hidden arguments as arg[0] and possibly arg[1] (static method)

  const int total_in_args = method->size_of_parameters();
  int total_c_args = total_in_args;
  if (!is_critical_native) {
    total_c_args += 1;
    if (method->is_static()) {
      total_c_args++;
    }
  } else {
    for (int i = 0; i < total_in_args; i++) {
      if (in_sig_bt[i] == T_ARRAY) {
        total_c_args++;
      }
    }
  }

  BasicType* out_sig_bt = NEW_RESOURCE_ARRAY(BasicType, total_c_args);
  VMRegPair* out_regs   = NEW_RESOURCE_ARRAY(VMRegPair, total_c_args);
  BasicType* in_elem_bt = NULL;

  int argc = 0;
  if (!is_critical_native) {
    out_sig_bt[argc++] = T_ADDRESS;
    if (method->is_static()) {
      out_sig_bt[argc++] = T_OBJECT;
    }

    for (int i = 0; i < total_in_args ; i++ ) {
      out_sig_bt[argc++] = in_sig_bt[i];
    }
  } else {
    Thread* THREAD = Thread::current();
    in_elem_bt = NEW_RESOURCE_ARRAY(BasicType, total_in_args);
    SignatureStream ss(method->signature());
    for (int i = 0; i < total_in_args ; i++ ) {
      if (in_sig_bt[i] == T_ARRAY) {
        // Arrays are passed as int, elem* pair
        out_sig_bt[argc++] = T_INT;
        out_sig_bt[argc++] = T_ADDRESS;
        Symbol* atype = ss.as_symbol(CHECK_NULL);
        const char* at = atype->as_C_string();
        if (strlen(at) == 2) {
          assert(at[0] == '[', "must be");
          switch (at[1]) {
            case 'B': in_elem_bt[i]  = T_BYTE; break;
            case 'C': in_elem_bt[i]  = T_CHAR; break;
            case 'D': in_elem_bt[i]  = T_DOUBLE; break;
            case 'F': in_elem_bt[i]  = T_FLOAT; break;
            case 'I': in_elem_bt[i]  = T_INT; break;
            case 'J': in_elem_bt[i]  = T_LONG; break;
            case 'S': in_elem_bt[i]  = T_SHORT; break;
            case 'Z': in_elem_bt[i]  = T_BOOLEAN; break;
            default: ShouldNotReachHere();
          }
        }
      } else {
        out_sig_bt[argc++] = in_sig_bt[i];
        in_elem_bt[i] = T_VOID;
      }
      if (in_sig_bt[i] != T_VOID) {
        assert(in_sig_bt[i] == ss.type(), "must match");
        ss.next();
      }
    }
  }

  // Now figure out where the args must be stored and how much stack space
  // they require (neglecting out_preserve_stack_slots but space for storing
  // the 1st six register arguments). It's weird see int_stk_helper.
  //
  int out_arg_slots;
  out_arg_slots = c_calling_convention(out_sig_bt, out_regs, NULL, total_c_args);

  // Compute framesize for the wrapper.  We need to handlize all oops in
  // registers. We must create space for them here that is disjoint from
  // the windowed save area because we have no control over when we might
  // flush the window again and overwrite values that gc has since modified.
  // (The live window race)
  //
  // We always just allocate 6 word for storing down these object. This allow
  // us to simply record the base and use the Ireg number to decide which
  // slot to use. (Note that the reg number is the inbound number not the
  // outbound number).
  // We must shuffle args to match the native convention, and include var-args space.

  // Calculate the total number of stack slots we will need.

  // First count the abi requirement plus all of the outgoing args
  int stack_slots = SharedRuntime::out_preserve_stack_slots() + out_arg_slots;

  // Now the space for the inbound oop handle area
  int total_save_slots = 9 * VMRegImpl::slots_per_word;  // 9 arguments passed in registers
  if (is_critical_native) {
    // Critical natives may have to call out so they need a save area
    // for register arguments.
    int double_slots = 0;
    int single_slots = 0;
    for ( int i = 0; i < total_in_args; i++) {
      if (in_regs[i].first()->is_Register()) {
        const Register reg = in_regs[i].first()->as_Register();
        switch (in_sig_bt[i]) {
          case T_BOOLEAN:
          case T_BYTE:
          case T_SHORT:
          case T_CHAR:
          case T_INT:  single_slots++; break;
          case T_ARRAY:
          case T_LONG: double_slots++; break;
          default:  ShouldNotReachHere();
        }
      } else if (in_regs[i].first()->is_FloatRegister()) {
        switch (in_sig_bt[i]) {
          case T_FLOAT:  single_slots++; break;
          case T_DOUBLE: double_slots++; break;
          default:  ShouldNotReachHere();
        }
      }
    }
    total_save_slots = double_slots * 2 + single_slots;
    // align the save area
    if (double_slots != 0) {
      stack_slots = round_to(stack_slots, 2);
    }
  }

  int oop_handle_offset = stack_slots;
  stack_slots += total_save_slots;

  // Now any space we need for handlizing a klass if static method

  int klass_slot_offset = 0;
  int klass_offset = -1;
  int lock_slot_offset = 0;
  bool is_static = false;

  if (method->is_static()) {
    klass_slot_offset = stack_slots;
    stack_slots += VMRegImpl::slots_per_word;
    klass_offset = klass_slot_offset * VMRegImpl::stack_slot_size;
    is_static = true;
  }

  // Plus a lock if needed

  if (method->is_synchronized()) {
    lock_slot_offset = stack_slots;
    stack_slots += VMRegImpl::slots_per_word;
  }

  // Now a place to save return value or as a temporary for any gpr -> fpr moves
  // + 2 for return address (which we own) and saved fp
  stack_slots += 2 + 9 * VMRegImpl::slots_per_word;  // (T0, A0, A1, A2, A3, A4, A5, A6, A7)

  // Ok The space we have allocated will look like:
  //
  //
  // FP-> |                     |
  //      |---------------------|
  //      | 2 slots for moves   |
  //      |---------------------|
  //      | lock box (if sync)  |
  //      |---------------------| <- lock_slot_offset
  //      | klass (if static)   |
  //      |---------------------| <- klass_slot_offset
  //      | oopHandle area      |
  //      |---------------------| <- oop_handle_offset
  //      | outbound memory     |
  //      | based arguments     |
  //      |                     |
  //      |---------------------|
  //      | vararg area         |
  //      |---------------------|
  //      |                     |
  // SP-> | out_preserved_slots |
  //
  //


  // Now compute actual number of stack words we need rounding to make
  // stack properly aligned.
  stack_slots = round_to(stack_slots, StackAlignmentInSlots);

  int stack_size = stack_slots * VMRegImpl::stack_slot_size;

  intptr_t start = (intptr_t)__ pc();



  // First thing make an ic check to see if we should even be here
  address ic_miss = SharedRuntime::get_ic_miss_stub();

  // We are free to use all registers as temps without saving them and
  // restoring them except fp. fp is the only callee save register
  // as far as the interpreter and the compiler(s) are concerned.

  //refer to register_loongarch.hpp:IC_Klass
  const Register ic_reg = T1;
  const Register receiver = T0;

  Label hit;
  Label exception_pending;

  __ verify_oop(receiver);
  //add for compressedoops
  __ load_klass(T4, receiver);
  __ beq(T4, ic_reg, hit);
  __ jmp(ic_miss, relocInfo::runtime_call_type);
  __ bind(hit);

  int vep_offset = ((intptr_t)__ pc()) - start;

  // Generate stack overflow check
  if (UseStackBanging) {
    __ bang_stack_with_offset(StackShadowPages*os::vm_page_size());
  }

  // The instruction at the verified entry point must be 4 bytes or longer
  // because it can be patched on the fly by make_non_entrant.
  if (((intptr_t)__ pc() - start - vep_offset) < 1 * BytesPerInstWord) {
    __ nop();
  }

  // Generate a new frame for the wrapper.
  // do LA need this ?
#ifndef OPT_THREAD
  __ get_thread(TREG);
#endif
  __ st_ptr(SP, TREG, in_bytes(JavaThread::last_Java_sp_offset()));
  __ li(AT, -(StackAlignmentInBytes));
  __ andr(SP, SP, AT);

  __ enter();
  // -2 because return address is already present and so is saved fp
  __ addi_d(SP, SP, -1 * (stack_size - 2*wordSize));

  // Frame is now completed as far a size and linkage.

  int frame_complete = ((intptr_t)__ pc()) - start;

  // Calculate the difference between sp and fp. We need to know it
  // after the native call because on windows Java Natives will pop
  // the arguments and it is painful to do sp relative addressing
  // in a platform independent way. So after the call we switch to
  // fp relative addressing.
  //FIXME actually , the fp_adjustment may not be the right, because andr(sp, sp, at) may change
  //the SP
  int fp_adjustment = stack_size - 2*wordSize;

#ifdef COMPILER2
  // C2 may leave the stack dirty if not in SSE2+ mode
  __ empty_FPU_stack();
#endif

  // Compute the fp offset for any slots used after the jni call

  int lock_slot_fp_offset = (lock_slot_offset*VMRegImpl::stack_slot_size) - fp_adjustment;
  // We use TREG as a thread pointer because it is callee save and
  // if we load it once it is usable thru the entire wrapper
  const Register thread = TREG;

  // We use S4 as the oop handle for the receiver/klass
  // It is callee save so it survives the call to native

  const Register oop_handle_reg = S4;
  if (is_critical_native) {
    Unimplemented();
    // check_needs_gc_for_critical_native(masm, stack_slots, total_c_args, total_in_args,
    //                                   oop_handle_offset, oop_maps, in_regs, in_sig_bt);
  }

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif

  //
  // We immediately shuffle the arguments so that any vm call we have to
  // make from here on out (sync slow path, jvmpi, etc.) we will have
  // captured the oops from our caller and have a valid oopMap for
  // them.

  // -----------------
  // The Grand Shuffle
  //
  // Natives require 1 or 2 extra arguments over the normal ones: the JNIEnv*
  // and, if static, the class mirror instead of a receiver.  This pretty much
  // guarantees that register layout will not match (and LA doesn't use reg
  // parms though amd does).  Since the native abi doesn't use register args
  // and the java conventions does we don't have to worry about collisions.
  // All of our moved are reg->stack or stack->stack.
  // We ignore the extra arguments during the shuffle and handle them at the
  // last moment. The shuffle is described by the two calling convention
  // vectors we have in our possession. We simply walk the java vector to
  // get the source locations and the c vector to get the destinations.

  int c_arg = method->is_static() ? 2 : 1 ;

  // Record sp-based slot for receiver on stack for non-static methods
  int receiver_offset = -1;

  // This is a trick. We double the stack slots so we can claim
  // the oops in the caller's frame. Since we are sure to have
  // more args than the caller doubling is enough to make
  // sure we can capture all the incoming oop args from the
  // caller.
  //
  OopMap* map = new OopMap(stack_slots * 2, 0 /* arg_slots*/);

  // Mark location of fp (someday)
  // map->set_callee_saved(VMRegImpl::stack2reg( stack_slots - 2), stack_slots * 2, 0, vmreg(fp));

#ifdef ASSERT
  bool reg_destroyed[RegisterImpl::number_of_registers];
  bool freg_destroyed[FloatRegisterImpl::number_of_registers];
  for ( int r = 0 ; r < RegisterImpl::number_of_registers ; r++ ) {
    reg_destroyed[r] = false;
  }
  for ( int f = 0 ; f < FloatRegisterImpl::number_of_registers ; f++ ) {
    freg_destroyed[f] = false;
  }

#endif /* ASSERT */

  // This may iterate in two different directions depending on the
  // kind of native it is.  The reason is that for regular JNI natives
  // the incoming and outgoing registers are offset upwards and for
  // critical natives they are offset down.
  GrowableArray<int> arg_order(2 * total_in_args);
  VMRegPair tmp_vmreg;
  tmp_vmreg.set2(T8->as_VMReg());

  if (!is_critical_native) {
    for (int i = total_in_args - 1, c_arg = total_c_args - 1; i >= 0; i--, c_arg--) {
      arg_order.push(i);
      arg_order.push(c_arg);
    }
  } else {
    // Compute a valid move order, using tmp_vmreg to break any cycles
    Unimplemented();
    // ComputeMoveOrder cmo(total_in_args, in_regs, total_c_args, out_regs, in_sig_bt, arg_order, tmp_vmreg);
  }

  int temploc = -1;
  for (int ai = 0; ai < arg_order.length(); ai += 2) {
    int i = arg_order.at(ai);
    int c_arg = arg_order.at(ai + 1);
    __ block_comment(err_msg("move %d -> %d", i, c_arg));
    if (c_arg == -1) {
      assert(is_critical_native, "should only be required for critical natives");
      // This arg needs to be moved to a temporary
      __ move(tmp_vmreg.first()->as_Register(), in_regs[i].first()->as_Register());
      in_regs[i] = tmp_vmreg;
      temploc = i;
      continue;
    } else if (i == -1) {
      assert(is_critical_native, "should only be required for critical natives");
      // Read from the temporary location
      assert(temploc != -1, "must be valid");
      i = temploc;
      temploc = -1;
    }
#ifdef ASSERT
    if (in_regs[i].first()->is_Register()) {
      assert(!reg_destroyed[in_regs[i].first()->as_Register()->encoding()], "destroyed reg!");
    } else if (in_regs[i].first()->is_FloatRegister()) {
      assert(!freg_destroyed[in_regs[i].first()->as_FloatRegister()->encoding()], "destroyed reg!");
    }
    if (out_regs[c_arg].first()->is_Register()) {
      reg_destroyed[out_regs[c_arg].first()->as_Register()->encoding()] = true;
    } else if (out_regs[c_arg].first()->is_FloatRegister()) {
      freg_destroyed[out_regs[c_arg].first()->as_FloatRegister()->encoding()] = true;
    }
#endif /* ASSERT */
    switch (in_sig_bt[i]) {
      case T_ARRAY:
        if (is_critical_native) {
          Unimplemented();
          // unpack_array_argument(masm, in_regs[i], in_elem_bt[i], out_regs[c_arg + 1], out_regs[c_arg]);
          c_arg++;
#ifdef ASSERT
          if (out_regs[c_arg].first()->is_Register()) {
            reg_destroyed[out_regs[c_arg].first()->as_Register()->encoding()] = true;
          } else if (out_regs[c_arg].first()->is_FloatRegister()) {
            freg_destroyed[out_regs[c_arg].first()->as_FloatRegister()->encoding()] = true;
          }
#endif
          break;
        }
      case T_OBJECT:
        assert(!is_critical_native, "no oop arguments");
        object_move(masm, map, oop_handle_offset, stack_slots, in_regs[i], out_regs[c_arg],
                    ((i == 0) && (!is_static)),
                    &receiver_offset);
        break;
      case T_VOID:
        break;

      case T_FLOAT:
        float_move(masm, in_regs[i], out_regs[c_arg]);
          break;

      case T_DOUBLE:
        assert( i + 1 < total_in_args &&
                in_sig_bt[i + 1] == T_VOID &&
                out_sig_bt[c_arg+1] == T_VOID, "bad arg list");
        double_move(masm, in_regs[i], out_regs[c_arg]);
        break;

      case T_LONG :
        long_move(masm, in_regs[i], out_regs[c_arg]);
        break;

      case T_ADDRESS: assert(false, "found T_ADDRESS in java args");

      default:
        simple_move32(masm, in_regs[i], out_regs[c_arg]);
    }
  }

  // point c_arg at the first arg that is already loaded in case we
  // need to spill before we call out
  c_arg = total_c_args - total_in_args;
  // Pre-load a static method's oop.  Used both by locking code and
  // the normal JNI call code.

  __ move(oop_handle_reg, A1);

  if (method->is_static() && !is_critical_native) {

    //  load opp into a register
    int oop_index = __ oop_recorder()->find_index(JNIHandles::make_local(
          (method->method_holder())->java_mirror()));


    RelocationHolder rspec = oop_Relocation::spec(oop_index);
    __ relocate(rspec);
    __ patchable_li52(oop_handle_reg, (long)JNIHandles::make_local((method->method_holder())->java_mirror()));
    // Now handlize the static class mirror it's known not-null.
    __ st_d( oop_handle_reg, SP, klass_offset);
    map->set_oop(VMRegImpl::stack2reg(klass_slot_offset));

    // Now get the handle
    __ lea(oop_handle_reg, Address(SP, klass_offset));
    // store the klass handle as second argument
    __ move(A1, oop_handle_reg);
    // and protect the arg if we must spill
    c_arg--;
  }

  // Change state to native (we save the return address in the thread, since it might not
  // be pushed on the stack when we do a a stack traversal). It is enough that the pc()
  // points into the right code segment. It does not have to be the correct return pc.
  // We use the same pc/oopMap repeatedly when we call out

  Label native_return;
  __ set_last_Java_frame(SP, noreg, native_return);

  // We have all of the arguments setup at this point. We must not touch any register
  // argument registers at this point (what if we save/restore them there are no oop?
  {
    SkipIfEqual skip_if(masm, &DTraceMethodProbes, 0);
    save_args(masm, total_c_args, c_arg, out_regs);
    int metadata_index = __ oop_recorder()->find_index(method());
    RelocationHolder rspec = metadata_Relocation::spec(metadata_index);
    __ relocate(rspec);
    __ patchable_li52(AT, (long)(method()));

    __ call_VM_leaf(
      CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_entry),
      thread, AT);

    restore_args(masm, total_c_args, c_arg, out_regs);
  }

  // These are register definitions we need for locking/unlocking
  const Register swap_reg = T8;  // Must use T8 for cmpxchg instruction
  const Register obj_reg  = T4;  // Will contain the oop
  //const Register lock_reg = T6;  // Address of compiler lock object (BasicLock)
  const Register lock_reg = c_rarg0;  // Address of compiler lock object (BasicLock)



  Label slow_path_lock;
  Label lock_done;

  // Lock a synchronized method
  if (method->is_synchronized()) {
    assert(!is_critical_native, "unhandled");

    const int mark_word_offset = BasicLock::displaced_header_offset_in_bytes();

    // Get the handle (the 2nd argument)
    __ move(oop_handle_reg, A1);

    // Get address of the box
    __ lea(lock_reg, Address(FP, lock_slot_fp_offset));

    // Load the oop from the handle
    __ ld_d(obj_reg, oop_handle_reg, 0);

    if (UseBiasedLocking) {
      // Note that oop_handle_reg is trashed during this call
      __ biased_locking_enter(lock_reg, obj_reg, swap_reg, A1, false, lock_done, &slow_path_lock);
    }

    // Load immediate 1 into swap_reg %T8
    __ li(swap_reg, 1);

    __ ld_d(AT, obj_reg, 0);
    __ orr(swap_reg, swap_reg, AT);

    __ st_d(swap_reg, lock_reg, mark_word_offset);
    __ cmpxchg(Address(obj_reg, 0), swap_reg, lock_reg, AT, true, false, lock_done);
    // Test if the oopMark is an obvious stack pointer, i.e.,
    //  1) (mark & 3) == 0, and
    //  2) sp <= mark < mark + os::pagesize()
    // These 3 tests can be done by evaluating the following
    // expression: ((mark - sp) & (3 - os::vm_page_size())),
    // assuming both stack pointer and pagesize have their
    // least significant 2 bits clear.
    // NOTE: the oopMark is in swap_reg %T8 as the result of cmpxchg

    __ sub_d(swap_reg, swap_reg, SP);
    __ li(AT, 3 - os::vm_page_size());
    __ andr(swap_reg , swap_reg, AT);
    // Save the test result, for recursive case, the result is zero
    __ st_d(swap_reg, lock_reg, mark_word_offset);
    __ bne(swap_reg, R0, slow_path_lock);
    // Slow path will re-enter here
    __ bind(lock_done);

    if (UseBiasedLocking) {
      // Re-fetch oop_handle_reg as we trashed it above
      __ move(A1, oop_handle_reg);
    }
  }


  // Finally just about ready to make the JNI call


  // get JNIEnv* which is first argument to native
  if (!is_critical_native) {
    __ addi_d(A0, thread, in_bytes(JavaThread::jni_environment_offset()));
  }

  // Example: Java_java_lang_ref_Finalizer_invokeFinalizeMethod(JNIEnv *env, jclass clazz, jobject ob)
  // Load the second arguments into A1
  //__ ld(A1, SP , wordSize );   // klass

  // Now set thread in native
  __ addi_d(AT, R0, _thread_in_native);
  if (os::is_MP()) {
    __ membar(Assembler::Membar_mask_bits(__ LoadStore|__ StoreStore)); // store release
  }
  __ st_w(AT, thread, in_bytes(JavaThread::thread_state_offset()));
  // do the call
  __ call(native_func, relocInfo::runtime_call_type);
  __ bind(native_return);

  oop_maps->add_gc_map(((intptr_t)__ pc()) - start, map);

  // WARNING - on Windows Java Natives use pascal calling convention and pop the
  // arguments off of the stack. We could just re-adjust the stack pointer here
  // and continue to do SP relative addressing but we instead switch to FP
  // relative addressing.

  // Unpack native results.
  switch (ret_type) {
  case T_BOOLEAN: __ c2bool(V0);                break;
  case T_CHAR   : __ bstrpick_d(V0, V0, 15, 0); break;
  case T_BYTE   : __ sign_extend_byte (V0);     break;
  case T_SHORT  : __ sign_extend_short(V0);     break;
  case T_INT    : // nothing to do         break;
  case T_DOUBLE :
  case T_FLOAT  :
  // Result is in st0 we'll save as needed
  break;
  case T_ARRAY:                 // Really a handle
  case T_OBJECT:                // Really a handle
  break; // can't de-handlize until after safepoint check
  case T_VOID: break;
  case T_LONG: break;
  default       : ShouldNotReachHere();
  }
  // Switch thread to "native transition" state before reading the synchronization state.
  // This additional state is necessary because reading and testing the synchronization
  // state is not atomic w.r.t. GC, as this scenario demonstrates:
  //     Java thread A, in _thread_in_native state, loads _not_synchronized and is preempted.
  //     VM thread changes sync state to synchronizing and suspends threads for GC.
  //     Thread A is resumed to finish this native method, but doesn't block here since it
  //     didn't see any synchronization is progress, and escapes.
  __ addi_d(AT, R0, _thread_in_native_trans);
  if (os::is_MP()) {
    __ membar(Assembler::Membar_mask_bits(__ LoadStore|__ StoreStore)); // store release
  }
  __ st_w(AT, thread, in_bytes(JavaThread::thread_state_offset()));

  if(os::is_MP()) {
    if (UseMembar) {
      // Force this write out before the read below
      __ membar(__ AnyAny);
    } else {
      // Write serialization page so VM thread can do a pseudo remote membar.
      // We use the current thread pointer to calculate a thread specific
      // offset to write to within the page. This minimizes bus traffic
      // due to cache line collision.
      __ serialize_memory(thread, T5);
    }
  }

  Label after_transition;

  // check for safepoint operation in progress and/or pending suspend requests
  {
    Label Continue;
    __ li(AT, SafepointSynchronize::address_of_state());
    __ ld_w(T5, AT, 0);
    __ addi_d(AT, T5, -SafepointSynchronize::_not_synchronized);
    Label L;
    __ bne(AT, R0, L);
    __ ld_w(AT, thread, in_bytes(JavaThread::suspend_flags_offset()));
    __ beq(AT, R0, Continue);
    __ bind(L);

    // Don't use call_VM as it will see a possible pending exception and forward it
    // and never return here preventing us from clearing _last_native_pc down below.
    //
    save_native_result(masm, ret_type, stack_slots);
    __ move(A0, thread);
    __ addi_d(SP, SP, -wordSize);
    __ push(S2);
    __ li(AT, -(StackAlignmentInBytes));
    __ move(S2, SP);     // use S2 as a sender SP holder
    __ andr(SP, SP, AT); // align stack as required by ABI
    if (!is_critical_native) {
      __ call(CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans), relocInfo::runtime_call_type);
    } else {
      __ call(CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans_and_transition), relocInfo::runtime_call_type);
    }
    __ move(SP, S2);     // use S2 as a sender SP holder
    __ pop(S2);
    __ addi_d(SP, SP, wordSize);
    //add for compressedoops
    __ reinit_heapbase();
    // Restore any method result value
    restore_native_result(masm, ret_type, stack_slots);

    if (is_critical_native) {
      // The call above performed the transition to thread_in_Java so
      // skip the transition logic below.
      __ beq(R0, R0, after_transition);
    }

    __ bind(Continue);
  }

  // change thread state
  __ addi_d(AT, R0, _thread_in_Java);
  if (os::is_MP()) {
    __ membar(Assembler::Membar_mask_bits(__ LoadStore|__ StoreStore)); // store release
  }
  __ st_w(AT,  thread, in_bytes(JavaThread::thread_state_offset()));
  __ bind(after_transition);
  Label reguard;
  Label reguard_done;
  __ ld_w(AT, thread, in_bytes(JavaThread::stack_guard_state_offset()));
  __ addi_d(AT, AT, -JavaThread::stack_guard_yellow_disabled);
  __ beq(AT, R0, reguard);
  // slow path reguard  re-enters here
  __ bind(reguard_done);

  // Handle possible exception (will unlock if necessary)

  // native result if any is live

  // Unlock
  Label slow_path_unlock;
  Label unlock_done;
  if (method->is_synchronized()) {

    Label done;

    // Get locked oop from the handle we passed to jni
    __ ld_d( obj_reg, oop_handle_reg, 0);
    if (UseBiasedLocking) {
      __ biased_locking_exit(obj_reg, T8, done);

    }

    // Simple recursive lock?

    __ ld_d(AT, FP, lock_slot_fp_offset);
    __ beq(AT, R0, done);
    // Must save FSF if if it is live now because cmpxchg must use it
    if (ret_type != T_FLOAT && ret_type != T_DOUBLE && ret_type != T_VOID) {
      save_native_result(masm, ret_type, stack_slots);
    }

    //  get old displaced header
    __ ld_d (T8, FP, lock_slot_fp_offset);
    // get address of the stack lock
    __ addi_d (c_rarg0, FP, lock_slot_fp_offset);
    // Atomic swap old header if oop still contains the stack lock
    __ cmpxchg(Address(obj_reg, 0), c_rarg0, T8, AT, false, false, unlock_done, &slow_path_unlock);

    // slow path re-enters here
    __ bind(unlock_done);
    if (ret_type != T_FLOAT && ret_type != T_DOUBLE && ret_type != T_VOID) {
      restore_native_result(masm, ret_type, stack_slots);
    }

    __ bind(done);

  }
  {
    SkipIfEqual skip_if(masm, &DTraceMethodProbes, 0);
    // Tell dtrace about this method exit
    save_native_result(masm, ret_type, stack_slots);
    int metadata_index = __ oop_recorder()->find_index( (method()));
    RelocationHolder rspec = metadata_Relocation::spec(metadata_index);
    __ relocate(rspec);
    __ patchable_li52(AT, (long)(method()));

    __ call_VM_leaf(
         CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_exit),
         thread, AT);
    restore_native_result(masm, ret_type, stack_slots);
  }

  // We can finally stop using that last_Java_frame we setup ages ago

  __ reset_last_Java_frame(false);

  // Unpack oop result, e.g. JNIHandles::resolve value.
  if (ret_type == T_OBJECT || ret_type == T_ARRAY) {
    __ resolve_jobject(V0, thread, T4);
  }

  if (!is_critical_native) {
    // reset handle block
    __ ld_d(AT, thread, in_bytes(JavaThread::active_handles_offset()));
    __ st_w(R0, AT, JNIHandleBlock::top_offset_in_bytes());
  }

  if (!is_critical_native) {
    // Any exception pending?
    __ ld_d(AT, thread, in_bytes(Thread::pending_exception_offset()));
    __ bne(AT, R0, exception_pending);
  }
  // no exception, we're almost done

  // check that only result value is on FPU stack
  __ verify_FPU(ret_type == T_FLOAT || ret_type == T_DOUBLE ? 1 : 0, "native_wrapper normal exit");

  // Return
#ifndef OPT_THREAD
  __ get_thread(TREG);
#endif
  //__ ld_ptr(SP, TREG, in_bytes(JavaThread::last_Java_sp_offset()));
  __ leave();

  __ jr(RA);
  // Unexpected paths are out of line and go here
  // Slow path locking & unlocking
  if (method->is_synchronized()) {

    // BEGIN Slow path lock
    __ bind(slow_path_lock);

    // protect the args we've loaded
    save_args(masm, total_c_args, c_arg, out_regs);

    // has last_Java_frame setup. No exceptions so do vanilla call not call_VM
    // args are (oop obj, BasicLock* lock, JavaThread* thread)

    __ move(A0, obj_reg);
    __ move(A1, lock_reg);
    __ move(A2, thread);
    __ addi_d(SP, SP, - 3*wordSize);

    __ li(AT, -(StackAlignmentInBytes));
    __ move(S2, SP);     // use S2 as a sender SP holder
    __ andr(SP, SP, AT); // align stack as required by ABI

    __ call(CAST_FROM_FN_PTR(address, SharedRuntime::complete_monitor_locking_C), relocInfo::runtime_call_type);
    __ move(SP, S2);
    __ addi_d(SP, SP, 3*wordSize);

    restore_args(masm, total_c_args, c_arg, out_regs);

#ifdef ASSERT
    { Label L;
      __ ld_d(AT, thread, in_bytes(Thread::pending_exception_offset()));
      __ beq(AT, R0, L);
      __ stop("no pending exception allowed on exit from monitorenter");
      __ bind(L);
    }
#endif
    __ b(lock_done);
    // END Slow path lock

    // BEGIN Slow path unlock
    __ bind(slow_path_unlock);

    // Slow path unlock

    if (ret_type == T_FLOAT || ret_type == T_DOUBLE ) {
      save_native_result(masm, ret_type, stack_slots);
    }
    // Save pending exception around call to VM (which contains an EXCEPTION_MARK)

    __ ld_d(AT, thread, in_bytes(Thread::pending_exception_offset()));
    __ push(AT);
    __ st_d(R0, thread, in_bytes(Thread::pending_exception_offset()));

    __ li(AT, -(StackAlignmentInBytes));
    __ move(S2, SP);     // use S2 as a sender SP holder
    __ andr(SP, SP, AT); // align stack as required by ABI

    // should be a peal
    // +wordSize because of the push above
    __ addi_d(A1, FP, lock_slot_fp_offset);

    __ move(A0, obj_reg);
    __ addi_d(SP,SP, -2*wordSize);
    __ call(CAST_FROM_FN_PTR(address, SharedRuntime::complete_monitor_unlocking_C),
        relocInfo::runtime_call_type);
    __ addi_d(SP, SP, 2*wordSize);
    __ move(SP, S2);
    //add for compressedoops
    __ reinit_heapbase();
#ifdef ASSERT
    {
      Label L;
      __ ld_d( AT, thread, in_bytes(Thread::pending_exception_offset()));
      __ beq(AT, R0, L);
      __ stop("no pending exception allowed on exit complete_monitor_unlocking_C");
      __ bind(L);
    }
#endif /* ASSERT */

    __ pop(AT);
    __ st_d(AT, thread, in_bytes(Thread::pending_exception_offset()));
    if (ret_type == T_FLOAT || ret_type == T_DOUBLE ) {
      restore_native_result(masm, ret_type, stack_slots);
    }
    __ b(unlock_done);
    // END Slow path unlock

  }

  // SLOW PATH Reguard the stack if needed

  __ bind(reguard);
  save_native_result(masm, ret_type, stack_slots);
  __ call(CAST_FROM_FN_PTR(address, SharedRuntime::reguard_yellow_pages),
      relocInfo::runtime_call_type);
  //add for compressedoops
  __ reinit_heapbase();
  restore_native_result(masm, ret_type, stack_slots);
  __ b(reguard_done);

  // BEGIN EXCEPTION PROCESSING
  if (!is_critical_native) {
    // Forward  the exception
    __ bind(exception_pending);

    // remove possible return value from FPU register stack
    __ empty_FPU_stack();

    // pop our frame
    //forward_exception_entry need return address on stack
    __ move(SP, FP);
    __ pop(FP);

    // and forward the exception
    __ jmp(StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);
  }
  __ flush();

  nmethod *nm = nmethod::new_native_nmethod(method,
                                            compile_id,
                                            masm->code(),
                                            vep_offset,
                                            frame_complete,
                                            stack_slots / VMRegImpl::slots_per_word,
                                            (is_static ? in_ByteSize(klass_offset) : in_ByteSize(receiver_offset)),
                                            in_ByteSize(lock_slot_offset*VMRegImpl::stack_slot_size),
                                            oop_maps);

  if (is_critical_native) {
    nm->set_lazy_critical_native(true);
  }
  return nm;
}

#ifdef HAVE_DTRACE_H
// ---------------------------------------------------------------------------
// Generate a dtrace nmethod for a given signature.  The method takes arguments
// in the Java compiled code convention, marshals them to the native
// abi and then leaves nops at the position you would expect to call a native
// function. When the probe is enabled the nops are replaced with a trap
// instruction that dtrace inserts and the trace will cause a notification
// to dtrace.
//
// The probes are only able to take primitive types and java/lang/String as
// arguments.  No other java types are allowed. Strings are converted to utf8
// strings so that from dtrace point of view java strings are converted to C
// strings. There is an arbitrary fixed limit on the total space that a method
// can use for converting the strings. (256 chars per string in the signature).
// So any java string larger then this is truncated.

static int  fp_offset[ConcreteRegisterImpl::number_of_registers] = { 0 };
static bool offsets_initialized = false;

static VMRegPair reg64_to_VMRegPair(Register r) {
  VMRegPair ret;
  if (wordSize == 8) {
    ret.set2(r->as_VMReg());
  } else {
    ret.set_pair(r->successor()->as_VMReg(), r->as_VMReg());
  }
  return ret;
}


nmethod *SharedRuntime::generate_dtrace_nmethod(MacroAssembler *masm,
                                                methodHandle method) {


  // generate_dtrace_nmethod is guarded by a mutex so we are sure to
  // be single threaded in this method.
  assert(AdapterHandlerLibrary_lock->owned_by_self(), "must be");

  // Fill in the signature array, for the calling-convention call.
  int total_args_passed = method->size_of_parameters();

  BasicType* in_sig_bt  = NEW_RESOURCE_ARRAY(BasicType, total_args_passed);
  VMRegPair  *in_regs   = NEW_RESOURCE_ARRAY(VMRegPair, total_args_passed);

  // The signature we are going to use for the trap that dtrace will see
  // java/lang/String is converted. We drop "this" and any other object
  // is converted to NULL.  (A one-slot java/lang/Long object reference
  // is converted to a two-slot long, which is why we double the allocation).
  BasicType* out_sig_bt = NEW_RESOURCE_ARRAY(BasicType, total_args_passed * 2);
  VMRegPair* out_regs   = NEW_RESOURCE_ARRAY(VMRegPair, total_args_passed * 2);

  int i=0;
  int total_strings = 0;
  int first_arg_to_pass = 0;
  int total_c_args = 0;

  // Skip the receiver as dtrace doesn't want to see it
  if( !method->is_static() ) {
    in_sig_bt[i++] = T_OBJECT;
    first_arg_to_pass = 1;
  }

  SignatureStream ss(method->signature());
  for ( ; !ss.at_return_type(); ss.next()) {
    BasicType bt = ss.type();
    in_sig_bt[i++] = bt;  // Collect remaining bits of signature
    out_sig_bt[total_c_args++] = bt;
    if( bt == T_OBJECT) {
      symbolOop s = ss.as_symbol_or_null();
      if (s == vmSymbols::java_lang_String()) {
        total_strings++;
        out_sig_bt[total_c_args-1] = T_ADDRESS;
      } else if (s == vmSymbols::java_lang_Boolean() ||
                 s == vmSymbols::java_lang_Byte()) {
        out_sig_bt[total_c_args-1] = T_BYTE;
      } else if (s == vmSymbols::java_lang_Character() ||
                 s == vmSymbols::java_lang_Short()) {
        out_sig_bt[total_c_args-1] = T_SHORT;
      } else if (s == vmSymbols::java_lang_Integer() ||
                 s == vmSymbols::java_lang_Float()) {
        out_sig_bt[total_c_args-1] = T_INT;
      } else if (s == vmSymbols::java_lang_Long() ||
                 s == vmSymbols::java_lang_Double()) {
        out_sig_bt[total_c_args-1] = T_LONG;
        out_sig_bt[total_c_args++] = T_VOID;
      }
    } else if ( bt == T_LONG || bt == T_DOUBLE ) {
      in_sig_bt[i++] = T_VOID;   // Longs & doubles take 2 Java slots
      // We convert double to long
      out_sig_bt[total_c_args-1] = T_LONG;
      out_sig_bt[total_c_args++] = T_VOID;
    } else if ( bt == T_FLOAT) {
      // We convert float to int
      out_sig_bt[total_c_args-1] = T_INT;
    }
  }

  assert(i==total_args_passed, "validly parsed signature");

  // Now get the compiled-Java layout as input arguments
  int comp_args_on_stack;
  comp_args_on_stack = SharedRuntime::java_calling_convention(
      in_sig_bt, in_regs, total_args_passed, false);

  // We have received a description of where all the java arg are located
  // on entry to the wrapper. We need to convert these args to where
  // the a  native (non-jni) function would expect them. To figure out
  // where they go we convert the java signature to a C signature and remove
  // T_VOID for any long/double we might have received.


  // Now figure out where the args must be stored and how much stack space
  // they require (neglecting out_preserve_stack_slots but space for storing
  // the 1st six register arguments). It's weird see int_stk_helper.

  int out_arg_slots;
  out_arg_slots = c_calling_convention(out_sig_bt, out_regs, NULL, total_c_args);

  // Calculate the total number of stack slots we will need.

  // First count the abi requirement plus all of the outgoing args
  int stack_slots = SharedRuntime::out_preserve_stack_slots() + out_arg_slots;

  // Plus a temp for possible converion of float/double/long register args

  int conversion_temp = stack_slots;
  stack_slots += 2;


  // Now space for the string(s) we must convert

  int string_locs = stack_slots;
  stack_slots += total_strings *
                   (max_dtrace_string_size / VMRegImpl::stack_slot_size);

  // Ok The space we have allocated will look like:
  //
  //
  // FP-> |                     |
  //      |---------------------|
  //      | string[n]           |
  //      |---------------------| <- string_locs[n]
  //      | string[n-1]         |
  //      |---------------------| <- string_locs[n-1]
  //      | ...                 |
  //      | ...                 |
  //      |---------------------| <- string_locs[1]
  //      | string[0]           |
  //      |---------------------| <- string_locs[0]
  //      | temp                |
  //      |---------------------| <- conversion_temp
  //      | outbound memory     |
  //      | based arguments     |
  //      |                     |
  //      |---------------------|
  //      |                     |
  // SP-> | out_preserved_slots |
  //
  //

  // Now compute actual number of stack words we need rounding to make
  // stack properly aligned.
  stack_slots = round_to(stack_slots, 4 * VMRegImpl::slots_per_word);

  int stack_size = stack_slots * VMRegImpl::stack_slot_size;
  intptr_t start = (intptr_t)__ pc();

  // First thing make an ic check to see if we should even be here

  {
    Label L;
    const Register temp_reg = G3_scratch;
    Address ic_miss(temp_reg, SharedRuntime::get_ic_miss_stub());
    __ verify_oop(O0);
    __ ld_ptr(O0, oopDesc::klass_offset_in_bytes(), temp_reg);
    __ cmp(temp_reg, G5_inline_cache_reg);
    __ brx(Assembler::equal, true, Assembler::pt, L);

    __ jump_to(ic_miss, 0);
    __ align(CodeEntryAlignment);
    __ bind(L);
  }

  int vep_offset = ((intptr_t)__ pc()) - start;

  // The instruction at the verified entry point must be 4 bytes or longer
  // because it can be patched on the fly by make_non_entrant. The stack bang
  // instruction fits that requirement.

  // Generate stack overflow check before creating frame
  __ generate_stack_overflow_check(stack_size);

  assert(((intptr_t)__ pc() - start - vep_offset) >= 1 * BytesPerInstWord,
         "valid size for make_non_entrant");

  // Generate a new frame for the wrapper.
  __ save(SP, -stack_size, SP);

  // Frame is now completed as far a size and linkage.

  int frame_complete = ((intptr_t)__ pc()) - start;

#ifdef ASSERT
  bool reg_destroyed[RegisterImpl::number_of_registers];
  bool freg_destroyed[FloatRegisterImpl::number_of_registers];
  for ( int r = 0 ; r < RegisterImpl::number_of_registers ; r++ ) {
    reg_destroyed[r] = false;
  }
  for ( int f = 0 ; f < FloatRegisterImpl::number_of_registers ; f++ ) {
    freg_destroyed[f] = false;
  }

#endif /* ASSERT */

  VMRegPair zero;
  const Register g0 = G0; // without this we get a compiler warning (why??)
  zero.set2(g0->as_VMReg());

  int c_arg, j_arg;

  Register conversion_off = noreg;

  for (j_arg = first_arg_to_pass, c_arg = 0 ;
       j_arg < total_args_passed ; j_arg++, c_arg++ ) {

    VMRegPair src = in_regs[j_arg];
    VMRegPair dst = out_regs[c_arg];

#ifdef ASSERT
    if (src.first()->is_Register()) {
      assert(!reg_destroyed[src.first()->as_Register()->encoding()], "ack!");
    } else if (src.first()->is_FloatRegister()) {
      assert(!freg_destroyed[src.first()->as_FloatRegister()->encoding(
                                               FloatRegisterImpl::S)], "ack!");
    }
    if (dst.first()->is_Register()) {
      reg_destroyed[dst.first()->as_Register()->encoding()] = true;
    } else if (dst.first()->is_FloatRegister()) {
      freg_destroyed[dst.first()->as_FloatRegister()->encoding(
                                                 FloatRegisterImpl::S)] = true;
    }
#endif /* ASSERT */

    switch (in_sig_bt[j_arg]) {
      case T_ARRAY:
      case T_OBJECT:
        {
          if (out_sig_bt[c_arg] == T_BYTE  || out_sig_bt[c_arg] == T_SHORT ||
              out_sig_bt[c_arg] == T_INT || out_sig_bt[c_arg] == T_LONG) {
            // need to unbox a one-slot value
            Register in_reg = L0;
            Register tmp = L2;
            if ( src.first()->is_reg() ) {
              in_reg = src.first()->as_Register();
            } else {
              assert(Assembler::is_simm13(reg2offset(src.first()) + STACK_BIAS),
                     "must be");
              __ ld_ptr(FP, reg2offset(src.first()) + STACK_BIAS, in_reg);
            }
            // If the final destination is an acceptable register
            if ( dst.first()->is_reg() ) {
              if ( dst.is_single_phys_reg() || out_sig_bt[c_arg] != T_LONG ) {
                tmp = dst.first()->as_Register();
              }
            }

            Label skipUnbox;
            if ( wordSize == 4 && out_sig_bt[c_arg] == T_LONG ) {
              __ mov(G0, tmp->successor());
            }
            __ mov(G0, tmp);
            __ br_null(in_reg, true, Assembler::pn, skipUnbox);

            BasicType bt = out_sig_bt[c_arg];
            int box_offset = java_lang_boxing_object::value_offset_in_bytes(bt);
            switch (bt) {
                case T_BYTE:
                  __ ldub(in_reg, box_offset, tmp); break;
                case T_SHORT:
                  __ lduh(in_reg, box_offset, tmp); break;
                case T_INT:
                  __ ld(in_reg, box_offset, tmp); break;
                case T_LONG:
                  __ ld_long(in_reg, box_offset, tmp); break;
                default: ShouldNotReachHere();
            }

            __ bind(skipUnbox);
            // If tmp wasn't final destination copy to final destination
            if (tmp == L2) {
              VMRegPair tmp_as_VM = reg64_to_VMRegPair(L2);
              if (out_sig_bt[c_arg] == T_LONG) {
                long_move(masm, tmp_as_VM, dst);
              } else {
                move32_64(masm, tmp_as_VM, out_regs[c_arg]);
              }
            }
            if (out_sig_bt[c_arg] == T_LONG) {
              assert(out_sig_bt[c_arg+1] == T_VOID, "must be");
              ++c_arg; // move over the T_VOID to keep the loop indices in sync
            }
          } else if (out_sig_bt[c_arg] == T_ADDRESS) {
            Register s =
                src.first()->is_reg() ? src.first()->as_Register() : L2;
            Register d =
                dst.first()->is_reg() ? dst.first()->as_Register() : L2;

            // We store the oop now so that the conversion pass can reach
            // while in the inner frame. This will be the only store if
            // the oop is NULL.
            if (s != L2) {
              // src is register
              if (d != L2) {
                // dst is register
                __ mov(s, d);
              } else {
                assert(Assembler::is_simm13(reg2offset(dst.first()) +
                          STACK_BIAS), "must be");
                __ st_ptr(s, SP, reg2offset(dst.first()) + STACK_BIAS);
              }
            } else {
                // src not a register
                assert(Assembler::is_simm13(reg2offset(src.first()) +
                           STACK_BIAS), "must be");
                __ ld_ptr(FP, reg2offset(src.first()) + STACK_BIAS, d);
                if (d == L2) {
                  assert(Assembler::is_simm13(reg2offset(dst.first()) +
                             STACK_BIAS), "must be");
                  __ st_ptr(d, SP, reg2offset(dst.first()) + STACK_BIAS);
                }
            }
          } else if (out_sig_bt[c_arg] != T_VOID) {
            // Convert the arg to NULL
            if (dst.first()->is_reg()) {
              __ mov(G0, dst.first()->as_Register());
            } else {
              assert(Assembler::is_simm13(reg2offset(dst.first()) +
                         STACK_BIAS), "must be");
              __ st_ptr(G0, SP, reg2offset(dst.first()) + STACK_BIAS);
            }
          }
        }
        break;
      case T_VOID:
        break;

      case T_FLOAT:
        if (src.first()->is_stack()) {
          // Stack to stack/reg is simple
          move32_64(masm, src, dst);
        } else {
          if (dst.first()->is_reg()) {
            // freg -> reg
            int off =
              STACK_BIAS + conversion_temp * VMRegImpl::stack_slot_size;
            Register d = dst.first()->as_Register();
            if (Assembler::is_simm13(off)) {
              __ stf(FloatRegisterImpl::S, src.first()->as_FloatRegister(),
                     SP, off);
              __ ld(SP, off, d);
            } else {
              if (conversion_off == noreg) {
                __ set(off, L6);
                conversion_off = L6;
              }
              __ stf(FloatRegisterImpl::S, src.first()->as_FloatRegister(),
                     SP, conversion_off);
              __ ld(SP, conversion_off , d);
            }
          } else {
            // freg -> mem
            int off = STACK_BIAS + reg2offset(dst.first());
            if (Assembler::is_simm13(off)) {
              __ stf(FloatRegisterImpl::S, src.first()->as_FloatRegister(),
                     SP, off);
            } else {
              if (conversion_off == noreg) {
                __ set(off, L6);
                conversion_off = L6;
              }
              __ stf(FloatRegisterImpl::S, src.first()->as_FloatRegister(),
                     SP, conversion_off);
            }
          }
        }
        break;

      case T_DOUBLE:
        assert( j_arg + 1 < total_args_passed &&
                in_sig_bt[j_arg + 1] == T_VOID &&
                out_sig_bt[c_arg+1] == T_VOID, "bad arg list");
        if (src.first()->is_stack()) {
          // Stack to stack/reg is simple
          long_move(masm, src, dst);
        } else {
          Register d = dst.first()->is_reg() ? dst.first()->as_Register() : L2;

          // Destination could be an odd reg on 32bit in which case
          // we can't load direct to the destination.

          if (!d->is_even() && wordSize == 4) {
            d = L2;
          }
          int off = STACK_BIAS + conversion_temp * VMRegImpl::stack_slot_size;
          if (Assembler::is_simm13(off)) {
            __ stf(FloatRegisterImpl::D, src.first()->as_FloatRegister(),
                   SP, off);
            __ ld_long(SP, off, d);
          } else {
            if (conversion_off == noreg) {
              __ set(off, L6);
              conversion_off = L6;
            }
            __ stf(FloatRegisterImpl::D, src.first()->as_FloatRegister(),
                   SP, conversion_off);
            __ ld_long(SP, conversion_off, d);
          }
          if (d == L2) {
            long_move(masm, reg64_to_VMRegPair(L2), dst);
          }
        }
        break;

      case T_LONG :
        // 32bit can't do a split move of something like g1 -> O0, O1
        // so use a memory temp
        if (src.is_single_phys_reg() && wordSize == 4) {
          Register tmp = L2;
          if (dst.first()->is_reg() &&
              (wordSize == 8 || dst.first()->as_Register()->is_even())) {
            tmp = dst.first()->as_Register();
          }

          int off = STACK_BIAS + conversion_temp * VMRegImpl::stack_slot_size;
          if (Assembler::is_simm13(off)) {
            __ stx(src.first()->as_Register(), SP, off);
            __ ld_long(SP, off, tmp);
          } else {
            if (conversion_off == noreg) {
              __ set(off, L6);
              conversion_off = L6;
            }
            __ stx(src.first()->as_Register(), SP, conversion_off);
            __ ld_long(SP, conversion_off, tmp);
          }

          if (tmp == L2) {
            long_move(masm, reg64_to_VMRegPair(L2), dst);
          }
        } else {
          long_move(masm, src, dst);
        }
        break;

      case T_ADDRESS: assert(false, "found T_ADDRESS in java args");

      default:
        move32_64(masm, src, dst);
    }
  }


  // If we have any strings we must store any register based arg to the stack
  // This includes any still live xmm registers too.

  if (total_strings > 0 ) {

    // protect all the arg registers
    __ save_frame(0);
    __ mov(G2_thread, L7_thread_cache);
    const Register L2_string_off = L2;

    // Get first string offset
    __ set(string_locs * VMRegImpl::stack_slot_size, L2_string_off);

    for (c_arg = 0 ; c_arg < total_c_args ; c_arg++ ) {
      if (out_sig_bt[c_arg] == T_ADDRESS) {

        VMRegPair dst = out_regs[c_arg];
        const Register d = dst.first()->is_reg() ?
            dst.first()->as_Register()->after_save() : noreg;

        // It's a string the oop and it was already copied to the out arg
        // position
        if (d != noreg) {
          __ mov(d, O0);
        } else {
          assert(Assembler::is_simm13(reg2offset(dst.first()) + STACK_BIAS),
                 "must be");
          __ ld_ptr(FP,  reg2offset(dst.first()) + STACK_BIAS, O0);
        }
        Label skip;

        __ add_d(FP, L2_string_off, O1);
        __ br_null(O0, false, Assembler::pn, skip);

        if (d != noreg) {
          __ mov(O1, d);
        } else {
          assert(Assembler::is_simm13(reg2offset(dst.first()) + STACK_BIAS),
                 "must be");
          __ st_ptr(O1, FP,  reg2offset(dst.first()) + STACK_BIAS);
        }

        __ addi_d(L2_string_off, max_dtrace_string_size, L2_string_off);
        __ call(CAST_FROM_FN_PTR(address, SharedRuntime::get_utf),
                relocInfo::runtime_call_type);

        __ bind(skip);

      }

    }
    __ mov(L7_thread_cache, G2_thread);
    __ restore();

  }


  // Ok now we are done. Need to place the nop that dtrace wants in order to
  // patch in the trap

  int patch_offset = ((intptr_t)__ pc()) - start;

  __ nop();


  // Return

  __ restore();
  __ ret();

  __ flush();
  nmethod *nm = nmethod::new_dtrace_nmethod(
      method, masm->code(), vep_offset, patch_offset, frame_complete,
      stack_slots / VMRegImpl::slots_per_word);
  return nm;
}

#endif // HAVE_DTRACE_H

// this function returns the adjust size (in number of words) to a c2i adapter
// activation for use during deoptimization
int Deoptimization::last_frame_adjust(int callee_parameters, int callee_locals) {
  return (callee_locals - callee_parameters) * Interpreter::stackElementWords;
}

// "Top of Stack" slots that may be unused by the calling convention but must
// otherwise be preserved.
// On Intel these are not necessary and the value can be zero.
// On Sparc this describes the words reserved for storing a register window
// when an interrupt occurs.
uint SharedRuntime::out_preserve_stack_slots() {
   return 0;
}

//------------------------------generate_deopt_blob----------------------------
// Ought to generate an ideal graph & compile, but here's some SPARC ASM
// instead.
void SharedRuntime::generate_deopt_blob() {
  // allocate space for the code
  ResourceMark rm;
  // setup code generation tools
  //CodeBuffer     buffer ("deopt_blob", 4000, 2048);
  CodeBuffer     buffer ("deopt_blob", 8000, 2048); // FIXME for debug
  MacroAssembler* masm  = new MacroAssembler( & buffer);
  int frame_size_in_words;
  OopMap* map = NULL;
  // Account for the extra args we place on the stack
  // by the time we call fetch_unroll_info
  const int additional_words = 2; // deopt kind, thread

  OopMapSet *oop_maps = new OopMapSet();
  RegisterSaver reg_save(false);

  address start = __ pc();
  Label cont;
  // we use S3 for DeOpt reason register
  Register reason = S3;
  // use S6 for thread register
  Register thread = TREG;
  // use S7 for fetch_unroll_info returned UnrollBlock
  Register unroll = S7;
  // Prolog for non exception case!

  // We have been called from the deopt handler of the deoptee.
  //
  // deoptee:
  //                      ...
  //                      call X
  //                      ...
  //  deopt_handler:      call_deopt_stub
  //  cur. return pc  --> ...
  //
  // So currently RA points behind the call in the deopt handler.
  // We adjust it such that it points to the start of the deopt handler.
  // The return_pc has been stored in the frame of the deoptee and
  // will replace the address of the deopt_handler in the call
  // to Deoptimization::fetch_unroll_info below.

  // HandlerImpl::size_deopt_handler()
  __ addi_d(RA, RA, - NativeFarCall::instruction_size);
  // Save everything in sight.
  map = reg_save.save_live_registers(masm, additional_words, &frame_size_in_words);
  // Normal deoptimization
  __ li(reason, Deoptimization::Unpack_deopt);
  __ b(cont);

  int reexecute_offset = __ pc() - start;

  // Reexecute case
  // return address is the pc describes what bci to do re-execute at

  // No need to update map as each call to save_live_registers will produce identical oopmap
  (void) reg_save.save_live_registers(masm, additional_words, &frame_size_in_words);
  __ li(reason, Deoptimization::Unpack_reexecute);
  __ b(cont);

  int   exception_offset = __ pc() - start;
  // Prolog for exception case

  // all registers are dead at this entry point, except for V0 and
  // V1 which contain the exception oop and exception pc
  // respectively.  Set them in TLS and fall thru to the
  // unpack_with_exception_in_tls entry point.

  __ get_thread(thread);
  __ st_ptr(V1, thread, in_bytes(JavaThread::exception_pc_offset()));
  __ st_ptr(V0, thread, in_bytes(JavaThread::exception_oop_offset()));
  int exception_in_tls_offset = __ pc() - start;
  // new implementation because exception oop is now passed in JavaThread

  // Prolog for exception case
  // All registers must be preserved because they might be used by LinearScan
  // Exceptiop oop and throwing PC are passed in JavaThread
  // tos: stack at point of call to method that threw the exception (i.e. only
  // args are on the stack, no return address)

  // Return address will be patched later with the throwing pc. The correct value is not
  // available now because loading it from memory would destroy registers.
  // Save everything in sight.
  // No need to update map as each call to save_live_registers will produce identical oopmap
  (void) reg_save.save_live_registers(masm, additional_words, &frame_size_in_words);

  // Now it is safe to overwrite any register
  // store the correct deoptimization type
  __ li(reason, Deoptimization::Unpack_exception);
  // load throwing pc from JavaThread and patch it as the return address
  // of the current frame. Then clear the field in JavaThread
  __ get_thread(thread);
  __ ld_ptr(V1, thread, in_bytes(JavaThread::exception_pc_offset()));
  __ st_ptr(V1, SP, reg_save.ra_offset()); //save ra
  __ st_ptr(R0, thread, in_bytes(JavaThread::exception_pc_offset()));


#ifdef ASSERT
  // verify that there is really an exception oop in JavaThread
  __ ld_ptr(AT, thread, in_bytes(JavaThread::exception_oop_offset()));
  __ verify_oop(AT);
  // verify that there is no pending exception
  Label no_pending_exception;
  __ ld_ptr(AT, thread, in_bytes(Thread::pending_exception_offset()));
  __ beq(AT, R0, no_pending_exception);
  __ stop("must not have pending exception here");
  __ bind(no_pending_exception);
#endif
  __ bind(cont);
  // Compiled code leaves the floating point stack dirty, empty it.
  __ empty_FPU_stack();


  // Call C code.  Need thread and this frame, but NOT official VM entry
  // crud.  We cannot block on this call, no GC can happen.
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif

  __ move(A0, thread);
  __ addi_d(SP, SP, -additional_words * wordSize);

  Label retaddr;
  __ set_last_Java_frame(NOREG, NOREG, retaddr);

  // Call fetch_unroll_info().  Need thread and this frame, but NOT official VM entry - cannot block on
  // this call, no GC can happen.  Call should capture return values.

  // TODO: confirm reloc
  __ call(CAST_FROM_FN_PTR(address, Deoptimization::fetch_unroll_info), relocInfo::runtime_call_type);
  __ bind(retaddr);
  oop_maps->add_gc_map(__ pc() - start, map);
  __ addi_d(SP, SP, additional_words * wordSize);
  __ get_thread(thread);
  __ reset_last_Java_frame(false);

  // Load UnrollBlock into S7
  __ move(unroll, V0);


  // Move the unpack kind to a safe place in the UnrollBlock because
  // we are very short of registers

  Address unpack_kind(unroll, Deoptimization::UnrollBlock::unpack_kind_offset_in_bytes());
  __ st_w(reason, unpack_kind);
  // save the unpack_kind value
  // Retrieve the possible live values (return values)
  // All callee save registers representing jvm state
  // are now in the vframeArray.

  Label noException;
  __ li(AT, Deoptimization::Unpack_exception);
  __ bne(AT, reason, noException);// Was exception pending?
  __ ld_ptr(V0, thread, in_bytes(JavaThread::exception_oop_offset()));
  __ ld_ptr(V1, thread, in_bytes(JavaThread::exception_pc_offset()));
  __ st_ptr(R0, thread, in_bytes(JavaThread::exception_pc_offset()));
  __ st_ptr(R0, thread, in_bytes(JavaThread::exception_oop_offset()));

  __ verify_oop(V0);

  // Overwrite the result registers with the exception results.
  __ st_ptr(V0, SP, reg_save.v0_offset());
  __ st_ptr(V1, SP, reg_save.v1_offset());

  __ bind(noException);


  // Stack is back to only having register save data on the stack.
  // Now restore the result registers. Everything else is either dead or captured
  // in the vframeArray.

  reg_save.restore_result_registers(masm);
  // All of the register save area has been popped of the stack. Only the
  // return address remains.
  // Pop all the frames we must move/replace.
  // Frame picture (youngest to oldest)
  // 1: self-frame (no frame link)
  // 2: deopting frame  (no frame link)
  // 3: caller of deopting frame (could be compiled/interpreted).
  //
  // Note: by leaving the return address of self-frame on the stack
  // and using the size of frame 2 to adjust the stack
  // when we are done the return to frame 3 will still be on the stack.

  // register for the sender's sp
  Register sender_sp = Rsender;
  // register for frame pcs
  Register pcs = T0;
  // register for frame sizes
  Register sizes = T1;
  // register for frame count
  Register count = T3;

  // Pop deoptimized frame
  __ ld_w(AT, unroll, Deoptimization::UnrollBlock::size_of_deoptimized_frame_offset_in_bytes());
  __ add_d(SP, SP, AT);
  // sp should be pointing at the return address to the caller (3)

  // Load array of frame pcs into pcs
  __ ld_ptr(pcs, unroll, Deoptimization::UnrollBlock::frame_pcs_offset_in_bytes());
  __ addi_d(SP, SP, wordSize);  // trash the old pc
  // Load array of frame sizes into T6
  __ ld_ptr(sizes, unroll, Deoptimization::UnrollBlock::frame_sizes_offset_in_bytes());



  // Load count of frams into T3
  __ ld_w(count, unroll, Deoptimization::UnrollBlock::number_of_frames_offset_in_bytes());
  // Pick up the initial fp we should save
  __ ld_d(FP, unroll,  Deoptimization::UnrollBlock::initial_info_offset_in_bytes());
   // Now adjust the caller's stack to make up for the extra locals
  // but record the original sp so that we can save it in the skeletal interpreter
  // frame and the stack walking of interpreter_sender will get the unextended sp
  // value and not the "real" sp value.
  __ move(sender_sp, SP);
  __ ld_w(AT, unroll, Deoptimization::UnrollBlock::caller_adjustment_offset_in_bytes());
  __ sub_d(SP, SP, AT);

    Label loop;
  __ bind(loop);
  __ ld_d(T2, sizes, 0);    // Load frame size
  __ ld_ptr(AT, pcs, 0);           // save return address
  __ addi_d(T2, T2, -2 * wordSize);           // we'll push pc and fp, by hand
  __ push2(AT, FP);
  __ move(FP, SP);
  __ sub_d(SP, SP, T2);       // Prolog!
  // This value is corrected by layout_activation_impl
  __ st_d(R0, FP, frame::interpreter_frame_last_sp_offset * wordSize);
  __ st_d(sender_sp, FP, frame::interpreter_frame_sender_sp_offset * wordSize);// Make it walkable
  __ move(sender_sp, SP);  // pass to next frame
  __ addi_d(count, count, -1);   // decrement counter
  __ addi_d(sizes, sizes, wordSize);   // Bump array pointer (sizes)
  __ addi_d(pcs, pcs, wordSize);   // Bump array pointer (pcs)
  __ bne(count, R0, loop);
  __ ld_d(AT, pcs, 0);      // frame_pcs[number_of_frames] = Interpreter::deopt_entry(vtos, 0);
  // Re-push self-frame
  __ push2(AT, FP);
  __ move(FP, SP);
  __ st_d(R0, FP, frame::interpreter_frame_last_sp_offset * wordSize);
  __ st_d(sender_sp, FP, frame::interpreter_frame_sender_sp_offset * wordSize);
  __ addi_d(SP, SP, -(frame_size_in_words - 2 - additional_words) * wordSize);

  // Restore frame locals after moving the frame
  __ st_d(V0, SP, reg_save.v0_offset());
  __ st_d(V1, SP, reg_save.v1_offset());
  __ fst_d(F0, SP, reg_save.fpr0_offset());
  __ fst_d(F1, SP, reg_save.fpr1_offset());

  // Call unpack_frames().  Need thread and this frame, but NOT official VM entry - cannot block on
  // this call, no GC can happen.
  __ move(A1, reason);  // exec_mode
  __ get_thread(thread);
  __ move(A0, thread);  // thread
  __ addi_d(SP, SP, (-additional_words) *wordSize);

  // set last_Java_sp, last_Java_fp
  Label L;
  address the_pc = __ pc();
  __ bind(L);
  __ set_last_Java_frame(NOREG, FP, L);

  __ li(AT, -(StackAlignmentInBytes));
  __ andr(SP, SP, AT);   // Fix stack alignment as required by ABI

  __ call(CAST_FROM_FN_PTR(address, Deoptimization::unpack_frames), relocInfo::runtime_call_type);
  // Revert SP alignment after call since we're going to do some SP relative addressing below
  __ ld_d(SP, thread, in_bytes(JavaThread::last_Java_sp_offset()));
  // Set an oopmap for the call site
  oop_maps->add_gc_map(the_pc - start, new OopMap(frame_size_in_words, 0));

  __ push(V0);

  __ get_thread(thread);
  __ reset_last_Java_frame(true);

  // Collect return values
  __ ld_d(V0, SP, reg_save.v0_offset() + (additional_words + 1) * wordSize);
  __ ld_d(V1, SP, reg_save.v1_offset() + (additional_words + 1) * wordSize);
  // Pop float stack and store in local
  __ fld_d(F0, SP, reg_save.fpr0_offset() + (additional_words + 1) * wordSize);
  __ fld_d(F1, SP, reg_save.fpr1_offset() + (additional_words + 1) * wordSize);

  //FIXME,
  // Clear floating point stack before returning to interpreter
  __ empty_FPU_stack();
  //FIXME, we should consider about float and double
  // Push a float or double return value if necessary.
  __ leave();

  // Jump to interpreter
  __ jr(RA);

  masm->flush();
  _deopt_blob = DeoptimizationBlob::create(&buffer, oop_maps, 0, exception_offset, reexecute_offset, frame_size_in_words);
  _deopt_blob->set_unpack_with_exception_in_tls_offset(exception_in_tls_offset);
}

#ifdef COMPILER2

//------------------------------generate_uncommon_trap_blob--------------------
// Ought to generate an ideal graph & compile, but here's some SPARC ASM
// instead.
void SharedRuntime::generate_uncommon_trap_blob() {
  // allocate space for the code
  ResourceMark rm;
  // setup code generation tools
  CodeBuffer  buffer ("uncommon_trap_blob", 512*80 , 512*40 );
  MacroAssembler* masm = new MacroAssembler(&buffer);

  enum frame_layout {
    fp_off, fp_off2,
    return_off, return_off2,
    framesize
  };
  assert(framesize % 4 == 0, "sp not 16-byte aligned");
  address start = __ pc();

  // Push self-frame.
  __ addi_d(SP, SP, -framesize * BytesPerInt);

  __ st_d(RA, SP, return_off * BytesPerInt);
  __ st_d(FP, SP, fp_off * BytesPerInt);

  __ addi_d(FP, SP, fp_off * BytesPerInt);

  // Clear the floating point exception stack
  __ empty_FPU_stack();

  Register thread = TREG;

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  // set last_Java_sp
  Label retaddr;
  __ set_last_Java_frame(NOREG, FP, retaddr);
  // Call C code.  Need thread but NOT official VM entry
  // crud.  We cannot block on this call, no GC can happen.  Call should
  // capture callee-saved registers as well as return values.
  __ move(A0, thread);
  // argument already in T0
  __ move(A1, T0);
  __ call((address)Deoptimization::uncommon_trap, relocInfo::runtime_call_type);
  __ bind(retaddr);

  // Set an oopmap for the call site
  OopMapSet *oop_maps = new OopMapSet();
  OopMap* map =  new OopMap( framesize, 0 );

  oop_maps->add_gc_map(__ pc() - start, map);

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif
  __ reset_last_Java_frame(false);

  // Load UnrollBlock into S7
  Register unroll = S7;
  __ move(unroll, V0);

  // Pop all the frames we must move/replace.
  //
  // Frame picture (youngest to oldest)
  // 1: self-frame (no frame link)
  // 2: deopting frame  (no frame link)
  // 3: possible-i2c-adapter-frame
  // 4: caller of deopting frame (could be compiled/interpreted. If interpreted we will create an
  //    and c2i here)

  __ addi_d(SP, SP, framesize * BytesPerInt);

  // Pop deoptimized frame
  __ ld_w(AT, unroll, Deoptimization::UnrollBlock::size_of_deoptimized_frame_offset_in_bytes());
  __ add_d(SP, SP, AT);

  // register for frame pcs
  Register pcs = T8;
  // register for frame sizes
  Register sizes = T4;
  // register for frame count
  Register count = T3;
  // register for the sender's sp
  Register sender_sp = T1;

  // sp should be pointing at the return address to the caller (4)
  // Load array of frame pcs
  __ ld_d(pcs, unroll, Deoptimization::UnrollBlock::frame_pcs_offset_in_bytes());

  // Load array of frame sizes
  __ ld_d(sizes, unroll, Deoptimization::UnrollBlock::frame_sizes_offset_in_bytes());
  __ ld_wu(count, unroll, Deoptimization::UnrollBlock::number_of_frames_offset_in_bytes());

  // Pick up the initial fp we should save
  __ ld_d(FP, unroll, Deoptimization::UnrollBlock::initial_info_offset_in_bytes());
  // Now adjust the caller's stack to make up for the extra locals
  // but record the original sp so that we can save it in the skeletal interpreter
  // frame and the stack walking of interpreter_sender will get the unextended sp
  // value and not the "real" sp value.

  __ move(sender_sp, SP);
  __ ld_w(AT, unroll, Deoptimization::UnrollBlock::caller_adjustment_offset_in_bytes());
  __ sub_d(SP, SP, AT);
  // Push interpreter frames in a loop
  Label loop;
  __ bind(loop);
  __ ld_d(T2, sizes, 0);          // Load frame size
  __ ld_d(AT, pcs, 0);           // save return address
  __ addi_d(T2, T2, -2*wordSize);           // we'll push pc and fp, by hand
  __ push2(AT, FP);
  __ move(FP, SP);
  __ sub_d(SP, SP, T2);                   // Prolog!
  // This value is corrected by layout_activation_impl
  __ st_d(R0, FP, frame::interpreter_frame_last_sp_offset * wordSize);
  __ st_d(sender_sp, FP, frame::interpreter_frame_sender_sp_offset * wordSize);// Make it walkable
  __ move(sender_sp, SP);       // pass to next frame
  __ addi_d(count, count, -1);    // decrement counter
  __ addi_d(sizes, sizes, wordSize);     // Bump array pointer (sizes)
  __ addi_d(pcs, pcs, wordSize);      // Bump array pointer (pcs)
  __ bne(count, R0, loop);

  __ ld_d(RA, pcs, 0);

  // Re-push self-frame
  // save old & set new FP
  // save final return address
  __ enter();

  // Use FP because the frames look interpreted now
  // Save "the_pc" since it cannot easily be retrieved using the last_java_SP after we aligned SP.
  // Don't need the precise return PC here, just precise enough to point into this code blob.
  Label L;
  address the_pc = __ pc();
  __ bind(L);
  __ set_last_Java_frame(NOREG, FP, L);

  __ li(AT, -(StackAlignmentInBytes));
  __ andr(SP, SP, AT);   // Fix stack alignment as required by ABI

  // Call C code.  Need thread but NOT official VM entry
  // crud.  We cannot block on this call, no GC can happen.  Call should
  // restore return values to their stack-slots with the new SP.
  __ move(A0, thread);
  __ li(A1, Deoptimization::Unpack_uncommon_trap);
  __ call((address)Deoptimization::unpack_frames, relocInfo::runtime_call_type);
  // Set an oopmap for the call site
  oop_maps->add_gc_map(the_pc - start, new OopMap(framesize, 0));

  __ reset_last_Java_frame(true);

  // Pop self-frame.
  __ leave();     // Epilog!

  // Jump to interpreter
  __ jr(RA);
  // -------------
  // make sure all code is generated
  masm->flush();
  _uncommon_trap_blob = UncommonTrapBlob::create(&buffer, oop_maps, framesize / 2);
}

#endif // COMPILER2

//------------------------------generate_handler_blob-------------------
//
// Generate a special Compile2Runtime blob that saves all registers, and sets
// up an OopMap and calls safepoint code to stop the compiled code for
// a safepoint.
//
// This blob is jumped to (via a breakpoint and the signal handler) from a
// safepoint in compiled code.

SafepointBlob* SharedRuntime::generate_handler_blob(address call_ptr, int poll_type) {

  // Account for thread arg in our frame
  const int additional_words = 0;
  int frame_size_in_words;

  assert (StubRoutines::forward_exception_entry() != NULL, "must be generated before");

  ResourceMark rm;
  OopMapSet *oop_maps = new OopMapSet();
  OopMap* map;

  // allocate space for the code
  // setup code generation tools
  CodeBuffer  buffer ("handler_blob", 2048, 512);
  MacroAssembler* masm = new MacroAssembler( &buffer);

  const Register thread = TREG;
  address start   = __ pc();
  bool cause_return = (poll_type == POLL_AT_RETURN);
  RegisterSaver reg_save(poll_type == POLL_AT_VECTOR_LOOP /* save_vectors */);

  // If cause_return is true we are at a poll_return and there is
  // the return address in RA to the caller on the nmethod
  // that is safepoint. We can leave this return in RA and
  // effectively complete the return and safepoint in the caller.
  // Otherwise we load exception pc to RA.
  __ push(thread);
#ifndef OPT_THREAD
  __ get_thread(thread);
#endif

  if(!cause_return) {
    __ ld_ptr(RA, Address(thread, JavaThread::saved_exception_pc_offset()));
  }

  __ pop(thread);
  map = reg_save.save_live_registers(masm, additional_words, &frame_size_in_words);

#ifndef OPT_THREAD
  __ get_thread(thread);
#endif

  // The following is basically a call_VM. However, we need the precise
  // address of the call in order to generate an oopmap. Hence, we do all the
  // work outselvs.

  __ move(A0, thread);
  Label retaddr;
  __ set_last_Java_frame(NOREG, NOREG, retaddr);

  // Do the call
  // TODO: confirm reloc
  __ call(call_ptr, relocInfo::runtime_call_type);
  __ bind(retaddr);

  // Set an oopmap for the call site.  This oopmap will map all
  // oop-registers and debug-info registers as callee-saved.  This
  // will allow deoptimization at this safepoint to find all possible
  // debug-info recordings, as well as let GC find all oops.
  oop_maps->add_gc_map(__ pc() - start, map);

  Label noException;

  // Clear last_Java_sp again
  __ reset_last_Java_frame(false);

  __ ld_ptr(AT, thread, in_bytes(Thread::pending_exception_offset()));
  __ beq(AT, R0, noException);

  // Exception pending

  reg_save.restore_live_registers(masm);
  //forward_exception_entry need return address on the stack
  __ push(RA);
  // TODO: confirm reloc
  __ jmp((address)StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);

  // No exception case
  __ bind(noException);
  // Normal exit, register restoring and exit
  reg_save.restore_live_registers(masm);
  __ jr(RA);

  masm->flush();
  // Fill-out other meta info
  return SafepointBlob::create(&buffer, oop_maps, frame_size_in_words);
}

//
// generate_resolve_blob - call resolution (static/virtual/opt-virtual/ic-miss
//
// Generate a stub that calls into vm to find out the proper destination
// of a java call. All the argument registers are live at this point
// but since this is generic code we don't know what they are and the caller
// must do any gc of the args.
//
RuntimeStub* SharedRuntime::generate_resolve_blob(address destination, const char* name) {
  assert (StubRoutines::forward_exception_entry() != NULL, "must be generated before");

  // allocate space for the code
  ResourceMark rm;

  //CodeBuffer buffer(name, 1000, 512);
  //FIXME. code_size
  CodeBuffer buffer(name, 2000, 2048);
  MacroAssembler* masm  = new MacroAssembler(&buffer);

  int frame_size_words;
  RegisterSaver reg_save(false /* save_vectors */);
  //we put the thread in A0

  OopMapSet *oop_maps = new OopMapSet();
  OopMap* map = NULL;

  address start = __ pc();
  map = reg_save.save_live_registers(masm, 0, &frame_size_words);


  int frame_complete = __ offset();
  const Register thread = T8;
  __ get_thread(thread);

  __ move(A0, thread);
  Label retaddr;
  __ set_last_Java_frame(noreg, FP, retaddr);
  // align the stack before invoke native
  __ li(AT, -(StackAlignmentInBytes));
  __ andr(SP, SP, AT);

  // TODO: confirm reloc
  __ call(destination, relocInfo::runtime_call_type);
  __ bind(retaddr);

  // Set an oopmap for the call site.
  // We need this not only for callee-saved registers, but also for volatile
  // registers that the compiler might be keeping live across a safepoint.
  oop_maps->add_gc_map(__ pc() - start, map);
  // V0 contains the address we are going to jump to assuming no exception got installed
  __ get_thread(thread);
  __ ld_ptr(SP, thread, in_bytes(JavaThread::last_Java_sp_offset()));
  // clear last_Java_sp
  __ reset_last_Java_frame(true);
  // check for pending exceptions
  Label pending;
  __ ld_ptr(AT, thread, in_bytes(Thread::pending_exception_offset()));
  __ bne(AT, R0, pending);
  // get the returned Method*
  __ get_vm_result_2(Rmethod, thread);
  __ st_ptr(Rmethod, SP, reg_save.s3_offset());
  __ st_ptr(V0, SP, reg_save.t5_offset());
  reg_save.restore_live_registers(masm);

  // We are back the the original state on entry and ready to go the callee method.
  __ jr(T5);
  // Pending exception after the safepoint

  __ bind(pending);

  reg_save.restore_live_registers(masm);

  // exception pending => remove activation and forward to exception handler
  //forward_exception_entry need return address on the stack
  __ push(RA);
  __ get_thread(thread);
  __ st_ptr(R0, thread, in_bytes(JavaThread::vm_result_offset()));
  __ ld_ptr(V0, thread, in_bytes(Thread::pending_exception_offset()));
  __ jmp(StubRoutines::forward_exception_entry(), relocInfo::runtime_call_type);
  //
  // make sure all code is generated
  masm->flush();
  RuntimeStub* tmp= RuntimeStub::new_runtime_stub(name, &buffer, frame_complete, frame_size_words, oop_maps, true);
  return tmp;
}

extern "C" int SpinPause() {return 0;}
