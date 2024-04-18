/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "memory/allocation.inline.hpp"
#include "prims/methodHandles.hpp"

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

#ifdef PRODUCT
#define BLOCK_COMMENT(str) /* nothing */
#define STOP(error) stop(error)
#else
#define BLOCK_COMMENT(str) __ block_comment(str)
#define STOP(error) block_comment(error); __ stop(error)
#endif

#define BIND(label) bind(label); BLOCK_COMMENT(#label ":")

void MethodHandles::load_klass_from_Class(MacroAssembler* _masm, Register klass_reg) {
  if (VerifyMethodHandles)
    verify_klass(_masm, klass_reg, SystemDictionary::WK_KLASS_ENUM_NAME(java_lang_Class),
                 "MH argument is a Class");
  __ ld(klass_reg, Address(klass_reg, java_lang_Class::klass_offset_in_bytes()));
}

#ifdef ASSERT
static int check_nonzero(const char* xname, int x) {
  assert(x != 0, err_msg("%s should be nonzero", xname));
  return x;
}
#define NONZERO(x) check_nonzero(#x, x)
#else //ASSERT
#define NONZERO(x) (x)
#endif //ASSERT

#ifdef ASSERT
void MethodHandles::verify_klass(MacroAssembler* _masm,
                                 Register obj, SystemDictionary::WKID klass_id,
                                 const char* error_message) {
}

void MethodHandles::verify_ref_kind(MacroAssembler* _masm, int ref_kind, Register member_reg, Register temp) {
  Label L;
  BLOCK_COMMENT("verify_ref_kind {");
  __ lw(temp, Address(member_reg, NONZERO(java_lang_invoke_MemberName::flags_offset_in_bytes())));
  __ sra(temp, temp, java_lang_invoke_MemberName::MN_REFERENCE_KIND_SHIFT);
  __ move(AT, java_lang_invoke_MemberName::MN_REFERENCE_KIND_MASK);
  __ andr(temp, temp, AT);
  __ move(AT, ref_kind);
  __ beq(temp, AT, L);
  __ delayed()->nop();
  { char* buf = NEW_C_HEAP_ARRAY(char, 100, mtInternal);
    jio_snprintf(buf, 100, "verify_ref_kind expected %x", ref_kind);
    if (ref_kind == JVM_REF_invokeVirtual ||
        ref_kind == JVM_REF_invokeSpecial)
      // could do this for all ref_kinds, but would explode assembly code size
      trace_method_handle(_masm, buf);
    __ STOP(buf);
  }
  BLOCK_COMMENT("} verify_ref_kind");
  __ bind(L);
}

#endif //ASSERT

void MethodHandles::jump_from_method_handle(MacroAssembler* _masm, Register method, Register temp,
                                            bool for_compiler_entry) {
  assert(method == Rmethod, "interpreter calling convention");

  Label L_no_such_method;
  __ beq(method, R0, L_no_such_method);
  __ delayed()->nop();

  __ verify_method_ptr(method);

  if (!for_compiler_entry && JvmtiExport::can_post_interpreter_events()) {
    Label run_compiled_code;
    // JVMTI events, such as single-stepping, are implemented partly by avoiding running
    // compiled code in threads for which the event is enabled.  Check here for
    // interp_only_mode if these events CAN be enabled.
    Register rthread = TREG;
    // interp_only is an int, on little endian it is sufficient to test the byte only
    // Is a cmpl faster?
    __ lbu(AT, rthread, in_bytes(JavaThread::interp_only_mode_offset()));
    __ beq(AT, R0, run_compiled_code);
    __ delayed()->nop();
    __ ld(T9, method, in_bytes(Method::interpreter_entry_offset()));
    __ jr(T9);
    __ delayed()->nop();
    __ BIND(run_compiled_code);
  }

  const ByteSize entry_offset = for_compiler_entry ? Method::from_compiled_offset() :
                                                     Method::from_interpreted_offset();
  __ ld(T9, method, in_bytes(entry_offset));
  __ jr(T9);
  __ delayed()->nop();

  __ bind(L_no_such_method);
  address wrong_method = StubRoutines::throw_AbstractMethodError_entry();
  __ jmp(wrong_method, relocInfo::runtime_call_type);
  __ delayed()->nop();
}

void MethodHandles::jump_to_lambda_form(MacroAssembler* _masm,
                                        Register recv, Register method_temp,
                                        Register temp2,
                                        bool for_compiler_entry) {
  BLOCK_COMMENT("jump_to_lambda_form {");
  // This is the initial entry point of a lazy method handle.
  // After type checking, it picks up the invoker from the LambdaForm.
  assert_different_registers(recv, method_temp, temp2);
  assert(recv != noreg, "required register");
  assert(method_temp == Rmethod, "required register for loading method");

  //NOT_PRODUCT({ FlagSetting fs(TraceMethodHandles, true); trace_method_handle(_masm, "LZMH"); });

  // Load the invoker, as MH -> MH.form -> LF.vmentry
  __ verify_oop(recv);
  __ load_heap_oop(method_temp, Address(recv, NONZERO(java_lang_invoke_MethodHandle::form_offset_in_bytes())));
  __ verify_oop(method_temp);
  __ load_heap_oop(method_temp, Address(method_temp, NONZERO(java_lang_invoke_LambdaForm::vmentry_offset_in_bytes())));
  __ verify_oop(method_temp);
  // the following assumes that a Method* is normally compressed in the vmtarget field:
  __ ld(method_temp, Address(method_temp, NONZERO(java_lang_invoke_MemberName::vmtarget_offset_in_bytes())));

  if (VerifyMethodHandles && !for_compiler_entry) {
    // make sure recv is already on stack
    __ ld(temp2, Address(method_temp, Method::const_offset()));
    __ load_sized_value(temp2,
                        Address(temp2, ConstMethod::size_of_parameters_offset()),
                        sizeof(u2), false);
    // assert(sizeof(u2) == sizeof(Method::_size_of_parameters), "");
    Label L;
    Address recv_addr = __ argument_address(temp2, -1);
    __ ld(AT, recv_addr);
    __ beq(recv, AT, L);
    __ delayed()->nop();

    recv_addr = __ argument_address(temp2, -1);
    __ ld(V0, recv_addr);
    __ STOP("receiver not on stack");
    __ BIND(L);
  }

  jump_from_method_handle(_masm, method_temp, temp2, for_compiler_entry);
  BLOCK_COMMENT("} jump_to_lambda_form");
}


// Code generation
address MethodHandles::generate_method_handle_interpreter_entry(MacroAssembler* _masm,
                                                                vmIntrinsics::ID iid) {
  const bool not_for_compiler_entry = false;  // this is the interpreter entry
  assert(is_signature_polymorphic(iid), "expected invoke iid");
  if (iid == vmIntrinsics::_invokeGeneric ||
      iid == vmIntrinsics::_compiledLambdaForm) {
    // Perhaps surprisingly, the symbolic references visible to Java are not directly used.
    // They are linked to Java-generated adapters via MethodHandleNatives.linkMethod.
    // They all allow an appendix argument.
    __ stop("empty stubs make SG sick");
    return NULL;
  }

  // Rmethod: Method*
  // T9: argument locator (parameter slot count, added to sp)
  // S7: used as temp to hold mh or receiver
  Register t9_argp   = T9;   // argument list ptr, live on error paths
  Register s7_mh     = S7;   // MH receiver; dies quickly and is recycled
  Register rm_method = Rmethod;   // eventual target of this invocation

  // here's where control starts out:
  __ align(CodeEntryAlignment);
  address entry_point = __ pc();

  if (VerifyMethodHandles) {
    Label L;
    BLOCK_COMMENT("verify_intrinsic_id {");
    __ lbu(AT, rm_method, Method::intrinsic_id_offset_in_bytes());
    guarantee(Assembler::is_simm16(iid), "Oops, iid is not simm16! Change the instructions.");
    __ addiu(AT, AT, -1 * (int) iid);
    __ beq(AT, R0, L);
    __ delayed()->nop();
    if (iid == vmIntrinsics::_linkToVirtual ||
        iid == vmIntrinsics::_linkToSpecial) {
      // could do this for all kinds, but would explode assembly code size
      trace_method_handle(_masm, "bad Method*::intrinsic_id");
    }
    __ STOP("bad Method*::intrinsic_id");
    __ bind(L);
    BLOCK_COMMENT("} verify_intrinsic_id");
  }

  // First task:  Find out how big the argument list is.
  Address t9_first_arg_addr;
  int ref_kind = signature_polymorphic_intrinsic_ref_kind(iid);
  assert(ref_kind != 0 || iid == vmIntrinsics::_invokeBasic, "must be _invokeBasic or a linkTo intrinsic");
  if (ref_kind == 0 || MethodHandles::ref_kind_has_receiver(ref_kind)) {
    __ ld(t9_argp, Address(rm_method, Method::const_offset()));
    __ load_sized_value(t9_argp,
                        Address(t9_argp, ConstMethod::size_of_parameters_offset()),
                        sizeof(u2), false);
    // assert(sizeof(u2) == sizeof(Method::_size_of_parameters), "");
    t9_first_arg_addr = __ argument_address(t9_argp, -1);
  } else {
    DEBUG_ONLY(t9_argp = noreg);
  }

  if (!is_signature_polymorphic_static(iid)) {
    __ ld(s7_mh, t9_first_arg_addr);
    DEBUG_ONLY(t9_argp = noreg);
  }

  // t9_first_arg_addr is live!

  trace_method_handle_interpreter_entry(_masm, iid);

  if (iid == vmIntrinsics::_invokeBasic) {
    generate_method_handle_dispatch(_masm, iid, s7_mh, noreg, not_for_compiler_entry);

  } else {
    // Adjust argument list by popping the trailing MemberName argument.
    Register r_recv = noreg;
    if (MethodHandles::ref_kind_has_receiver(ref_kind)) {
      // Load the receiver (not the MH; the actual MemberName's receiver) up from the interpreter stack.
      __ ld(r_recv = T2, t9_first_arg_addr);
    }
    DEBUG_ONLY(t9_argp = noreg);
    Register rm_member = rm_method;  // MemberName ptr; incoming method ptr is dead now
    __ pop(rm_member);         // extract last argument
    generate_method_handle_dispatch(_masm, iid, r_recv, rm_member, not_for_compiler_entry);
  }

  return entry_point;
}

void MethodHandles::generate_method_handle_dispatch(MacroAssembler* _masm,
                                                    vmIntrinsics::ID iid,
                                                    Register receiver_reg,
                                                    Register member_reg,
                                                    bool for_compiler_entry) {
  assert(is_signature_polymorphic(iid), "expected invoke iid");
  Register rm_method = Rmethod;   // eventual target of this invocation
  // temps used in this code are not used in *either* compiled or interpreted calling sequences
  Register j_rarg0 = T0;
  Register j_rarg1 = A0;
  Register j_rarg2 = A1;
  Register j_rarg3 = A2;
  Register j_rarg4 = A3;
  Register j_rarg5 = A4;

  Register temp1 = T8;
  Register temp2 = T9;
  Register temp3 = V0;
  if (for_compiler_entry) {
    assert(receiver_reg == (iid == vmIntrinsics::_linkToStatic ? noreg : j_rarg0), "only valid assignment");
    assert_different_registers(temp1,        j_rarg0, j_rarg1, j_rarg2, j_rarg3, j_rarg4, j_rarg5);
    assert_different_registers(temp2,        j_rarg0, j_rarg1, j_rarg2, j_rarg3, j_rarg4, j_rarg5);
    assert_different_registers(temp3,        j_rarg0, j_rarg1, j_rarg2, j_rarg3, j_rarg4, j_rarg5);
  }
  else {
    assert_different_registers(temp1, temp2, temp3, saved_last_sp_register());  // don't trash lastSP
  }
  assert_different_registers(temp1, temp2, temp3, receiver_reg);
  assert_different_registers(temp1, temp2, temp3, member_reg);

  if (iid == vmIntrinsics::_invokeBasic) {
    // indirect through MH.form.vmentry.vmtarget
    jump_to_lambda_form(_masm, receiver_reg, rm_method, temp1, for_compiler_entry);

  } else {
    // The method is a member invoker used by direct method handles.
    if (VerifyMethodHandles) {
      // make sure the trailing argument really is a MemberName (caller responsibility)
      verify_klass(_masm, member_reg, SystemDictionary::WK_KLASS_ENUM_NAME(java_lang_invoke_MemberName),
                   "MemberName required for invokeVirtual etc.");
    }

    Address member_clazz(    member_reg, NONZERO(java_lang_invoke_MemberName::clazz_offset_in_bytes()));
    Address member_vmindex(  member_reg, NONZERO(java_lang_invoke_MemberName::vmindex_offset_in_bytes()));
    Address member_vmtarget( member_reg, NONZERO(java_lang_invoke_MemberName::vmtarget_offset_in_bytes()));

    Register temp1_recv_klass = temp1;
    if (iid != vmIntrinsics::_linkToStatic) {
      __ verify_oop(receiver_reg);
      if (iid == vmIntrinsics::_linkToSpecial) {
        // Don't actually load the klass; just null-check the receiver.
        __ null_check(receiver_reg);
      } else {
        // load receiver klass itself
        __ null_check(receiver_reg, oopDesc::klass_offset_in_bytes());
        __ load_klass(temp1_recv_klass, receiver_reg);
        __ verify_klass_ptr(temp1_recv_klass);
      }
      BLOCK_COMMENT("check_receiver {");
      // The receiver for the MemberName must be in receiver_reg.
      // Check the receiver against the MemberName.clazz
      if (VerifyMethodHandles && iid == vmIntrinsics::_linkToSpecial) {
        // Did not load it above...
        __ load_klass(temp1_recv_klass, receiver_reg);
        __ verify_klass_ptr(temp1_recv_klass);
      }
      if (VerifyMethodHandles && iid != vmIntrinsics::_linkToInterface) {
        Label L_ok;
        Register temp2_defc = temp2;
        __ load_heap_oop(temp2_defc, member_clazz);
        load_klass_from_Class(_masm, temp2_defc);
        __ verify_klass_ptr(temp2_defc);
        __ check_klass_subtype(temp1_recv_klass, temp2_defc, temp3, L_ok);
        // If we get here, the type check failed!
        __ STOP("receiver class disagrees with MemberName.clazz");
        __ bind(L_ok);
      }
      BLOCK_COMMENT("} check_receiver");
    }
    if (iid == vmIntrinsics::_linkToSpecial ||
        iid == vmIntrinsics::_linkToStatic) {
      DEBUG_ONLY(temp1_recv_klass = noreg);  // these guys didn't load the recv_klass
    }

    // Live registers at this point:
    //  member_reg - MemberName that was the trailing argument
    //  temp1_recv_klass - klass of stacked receiver, if needed

    Label L_incompatible_class_change_error;
    switch (iid) {
    case vmIntrinsics::_linkToSpecial:
      if (VerifyMethodHandles) {
        verify_ref_kind(_masm, JVM_REF_invokeSpecial, member_reg, temp3);
      }
      __ ld(rm_method, member_vmtarget);
      break;

    case vmIntrinsics::_linkToStatic:
      if (VerifyMethodHandles) {
        verify_ref_kind(_masm, JVM_REF_invokeStatic, member_reg, temp3);
      }
      __ ld(rm_method, member_vmtarget);
      break;

    case vmIntrinsics::_linkToVirtual:
    {
      // same as TemplateTable::invokevirtual,
      // minus the CP setup and profiling:

      if (VerifyMethodHandles) {
        verify_ref_kind(_masm, JVM_REF_invokeVirtual, member_reg, temp3);
      }

      // pick out the vtable index from the MemberName, and then we can discard it:
      Register temp2_index = temp2;
      __ ld(temp2_index, member_vmindex);

      if (VerifyMethodHandles) {
        Label L_index_ok;
        __ slt(AT, R0, temp2_index);
        __ bne(AT, R0, L_index_ok);
        __ delayed()->nop();
        __ STOP("no virtual index");
        __ BIND(L_index_ok);
      }

      // Note:  The verifier invariants allow us to ignore MemberName.clazz and vmtarget
      // at this point.  And VerifyMethodHandles has already checked clazz, if needed.

      // get target Method* & entry point
      __ lookup_virtual_method(temp1_recv_klass, temp2_index, rm_method);
      break;
    }

    case vmIntrinsics::_linkToInterface:
    {
      // same as TemplateTable::invokeinterface
      // (minus the CP setup and profiling, with different argument motion)
      if (VerifyMethodHandles) {
        verify_ref_kind(_masm, JVM_REF_invokeInterface, member_reg, temp3);
      }

      Register temp3_intf = temp3;
      __ load_heap_oop(temp3_intf, member_clazz);
      load_klass_from_Class(_masm, temp3_intf);
      __ verify_klass_ptr(temp3_intf);

      Register rm_index = rm_method;
      __ ld(rm_index, member_vmindex);
      if (VerifyMethodHandles) {
        Label L;
        __ slt(AT, rm_index, R0);
        __ beq(AT, R0, L);
        __ delayed()->nop();
        __ STOP("invalid vtable index for MH.invokeInterface");
        __ bind(L);
      }

      // given intf, index, and recv klass, dispatch to the implementation method
      __ lookup_interface_method(temp1_recv_klass, temp3_intf,
                                 // note: next two args must be the same:
                                 rm_index, rm_method,
                                 temp2,
                                 L_incompatible_class_change_error);
      break;
    }

    default:
      fatal(err_msg_res("unexpected intrinsic %d: %s", iid, vmIntrinsics::name_at(iid)));
      break;
    }

    // Live at this point:
    //   rm_method

    // After figuring out which concrete method to call, jump into it.
    // Note that this works in the interpreter with no data motion.
    // But the compiled version will require that r_recv be shifted out.
    __ verify_method_ptr(rm_method);
    jump_from_method_handle(_masm, rm_method, temp1, for_compiler_entry);

    if (iid == vmIntrinsics::_linkToInterface) {
      __ bind(L_incompatible_class_change_error);
      address icce_entry= StubRoutines::throw_IncompatibleClassChangeError_entry();
      __ jmp(icce_entry, relocInfo::runtime_call_type);
      __ delayed()->nop();
    }
  }
}

#ifndef PRODUCT
void trace_method_handle_stub(const char* adaptername,
                              oop mh,
                              intptr_t* saved_regs,
                              intptr_t* entry_sp) {
  // called as a leaf from native code: do not block the JVM!
  bool has_mh = (strstr(adaptername, "/static") == NULL &&
                 strstr(adaptername, "linkTo") == NULL);    // static linkers don't have MH
  const char* mh_reg_name = has_mh ? "s7_mh" : "s7";
  tty->print_cr("MH %s %s=" PTR_FORMAT " sp=" PTR_FORMAT,
                adaptername, mh_reg_name,
                p2i(mh), p2i(entry_sp));

  if (Verbose) {
    tty->print_cr("Registers:");
    const int saved_regs_count = RegisterImpl::number_of_registers;
    for (int i = 0; i < saved_regs_count; i++) {
      Register r = as_Register(i);
      // The registers are stored in reverse order on the stack (by pusha).
      tty->print("%3s=" PTR_FORMAT, r->name(), saved_regs[((saved_regs_count - 1) - i)]);
      if ((i + 1) % 4 == 0) {
        tty->cr();
      } else {
        tty->print(", ");
      }
    }
    tty->cr();

    {
     // dumping last frame with frame::describe

      JavaThread* p = JavaThread::active();

      ResourceMark rm;
      PRESERVE_EXCEPTION_MARK; // may not be needed by safer and unexpensive here
      FrameValues values;

      // Note: We want to allow trace_method_handle from any call site.
      // While trace_method_handle creates a frame, it may be entered
      // without a PC on the stack top (e.g. not just after a call).
      // Walking that frame could lead to failures due to that invalid PC.
      // => carefully detect that frame when doing the stack walking

      // Current C frame
      frame cur_frame = os::current_frame();

      // Robust search of trace_calling_frame (independant of inlining).
      // Assumes saved_regs comes from a pusha in the trace_calling_frame.
      assert(cur_frame.sp() < saved_regs, "registers not saved on stack ?");
      frame trace_calling_frame = os::get_sender_for_C_frame(&cur_frame);
      while (trace_calling_frame.fp() < saved_regs) {
        trace_calling_frame = os::get_sender_for_C_frame(&trace_calling_frame);
      }

      // safely create a frame and call frame::describe
      intptr_t *dump_sp = trace_calling_frame.sender_sp();
      intptr_t *dump_fp = trace_calling_frame.link();

      bool walkable = has_mh; // whether the traced frame shoud be walkable

      if (walkable) {
        // The previous definition of walkable may have to be refined
        // if new call sites cause the next frame constructor to start
        // failing. Alternatively, frame constructors could be
        // modified to support the current or future non walkable
        // frames (but this is more intrusive and is not considered as
        // part of this RFE, which will instead use a simpler output).
        frame dump_frame = frame(dump_sp, dump_fp);
        dump_frame.describe(values, 1);
      } else {
        // Stack may not be walkable (invalid PC above FP):
        // Add descriptions without building a Java frame to avoid issues
        values.describe(-1, dump_fp, "fp for #1 <not parsed, cannot trust pc>");
        values.describe(-1, dump_sp, "sp for #1");
      }
      values.describe(-1, entry_sp, "raw top of stack");

      tty->print_cr("Stack layout:");
      values.print(p);
    }
    if (has_mh && mh->is_oop()) {
      mh->print();
      if (java_lang_invoke_MethodHandle::is_instance(mh)) {
        if (java_lang_invoke_MethodHandle::form_offset_in_bytes() != 0)
          java_lang_invoke_MethodHandle::form(mh)->print();
      }
    }
  }
}

// The stub wraps the arguments in a struct on the stack to avoid
// dealing with the different calling conventions for passing 6
// arguments.
struct MethodHandleStubArguments {
  const char* adaptername;
  oopDesc* mh;
  intptr_t* saved_regs;
  intptr_t* entry_sp;
};
void trace_method_handle_stub_wrapper(MethodHandleStubArguments* args) {
  trace_method_handle_stub(args->adaptername,
                           args->mh,
                           args->saved_regs,
                           args->entry_sp);
}

void MethodHandles::trace_method_handle(MacroAssembler* _masm, const char* adaptername) {
}
#endif //PRODUCT
