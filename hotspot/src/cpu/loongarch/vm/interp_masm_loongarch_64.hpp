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

#ifndef CPU_LOONGARCH_VM_INTERP_MASM_LOONGARCH_64_HPP
#define CPU_LOONGARCH_VM_INTERP_MASM_LOONGARCH_64_HPP

#include "asm/assembler.hpp"
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/invocationCounter.hpp"
#include "runtime/frame.hpp"

// This file specializes the assember with interpreter-specific macros


class InterpreterMacroAssembler: public MacroAssembler {
#ifndef CC_INTERP
 private:

  Register _locals_register; // register that contains the pointer to the locals
  Register _bcp_register; // register that contains the bcp

 protected:
  // Interpreter specific version of call_VM_base
  virtual void call_VM_leaf_base(address entry_point,
                                 int number_of_arguments);

  virtual void call_VM_base(Register oop_result,
                            Register java_thread,
                            Register last_java_sp,
                            address  entry_point,
                            int number_of_arguments,
                            bool check_exceptions);

  virtual void check_and_handle_popframe(Register java_thread);
  virtual void check_and_handle_earlyret(Register java_thread);

  // base routine for all dispatches
  void dispatch_base(TosState state, address* table, bool verifyoop = true);
#endif // CC_INTERP

 public:
  // narrow int return value
  void narrow(Register result);

  InterpreterMacroAssembler(CodeBuffer* code) : MacroAssembler(code), _locals_register(LVP), _bcp_register(BCP) {}

  void  get_2_byte_integer_at_bcp(Register reg, Register tmp, int offset);
  void  get_4_byte_integer_at_bcp(Register reg, int offset);

  void load_earlyret_value(TosState state);

#ifdef CC_INTERP
  void save_bcp()                                          { /*  not needed in c++ interpreter and harmless */ }
  void restore_bcp()                                       { /*  not needed in c++ interpreter and harmless */ }

  // Helpers for runtime call arguments/results
  void get_method(Register reg);

#else

  // Interpreter-specific registers
  void save_bcp() {
    st_d(BCP, FP, frame::interpreter_frame_bcx_offset * wordSize);
  }

  void restore_bcp() {
    ld_d(BCP, FP, frame::interpreter_frame_bcx_offset * wordSize);
  }

  void restore_locals() {
    ld_d(LVP, FP, frame::interpreter_frame_locals_offset * wordSize);
  }

  // Helpers for runtime call arguments/results
  void get_method(Register reg) {
    ld_d(reg, FP, frame::interpreter_frame_method_offset * wordSize);
  }

  void get_const(Register reg){
    get_method(reg);
    ld_d(reg, reg, in_bytes(Method::const_offset()));
  }

  void get_constant_pool(Register reg) {
    get_const(reg);
    ld_d(reg, reg, in_bytes(ConstMethod::constants_offset()));
  }

  void get_constant_pool_cache(Register reg) {
    get_constant_pool(reg);
    ld_d(reg, reg, ConstantPool::cache_offset_in_bytes());
  }

  void get_cpool_and_tags(Register cpool, Register tags) {
    get_constant_pool(cpool);
    ld_d(tags, cpool, ConstantPool::tags_offset_in_bytes());
  }

  void get_unsigned_2_byte_index_at_bcp(Register reg, int bcp_offset);
  void get_cache_and_index_at_bcp(Register cache, Register index, int bcp_offset, size_t index_size = sizeof(u2));
  void get_cache_and_index_and_bytecode_at_bcp(Register cache, Register index, Register bytecode, int byte_no, int bcp_offset, size_t index_size = sizeof(u2));
  void get_cache_entry_pointer_at_bcp(Register cache, Register tmp, int bcp_offset, size_t index_size = sizeof(u2));
  void get_cache_index_at_bcp(Register index, int bcp_offset, size_t index_size = sizeof(u2));
  void get_method_counters(Register method, Register mcs, Label& skip);

  // load cpool->resolved_references(index);
  void load_resolved_reference_at_index(Register result, Register index);

  void pop_ptr(   Register r = FSR);
  void pop_i(     Register r = FSR);
  void pop_l(     Register r = FSR);
  void pop_f(FloatRegister r = FSF);
  void pop_d(FloatRegister r = FSF);

  void push_ptr(   Register r = FSR);
  void push_i(     Register r = FSR);
  void push_l(     Register r = FSR);
  void push_f(FloatRegister r = FSF);
  void push_d(FloatRegister r = FSF);

  void pop(Register r ) { ((MacroAssembler*)this)->pop(r); }

  void push(Register r ) { ((MacroAssembler*)this)->push(r); }

  void pop(TosState state); // transition vtos -> state
  void push(TosState state); // transition state -> vtos

  void empty_expression_stack() {
    ld_d(SP, FP, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    // NULL last_sp until next java call
    st_d(R0, FP, frame::interpreter_frame_last_sp_offset * wordSize);
  }

  // Super call_VM calls - correspond to MacroAssembler::call_VM(_leaf) calls
  void load_ptr(int n, Register val);
  void store_ptr(int n, Register val);

  // Generate a subtype check: branch to ok_is_subtype if sub_klass is
  // a subtype of super_klass.
  //void gen_subtype_check( Register sub_klass, Label &ok_is_subtype );
  void gen_subtype_check( Register Rsup_klass, Register sub_klass, Label &ok_is_subtype );

  // Dispatching
  void dispatch_prolog(TosState state, int step = 0);
  void dispatch_epilog(TosState state, int step = 0);
  void dispatch_only(TosState state);
  void dispatch_only_normal(TosState state);
  void dispatch_only_noverify(TosState state);
  void dispatch_next(TosState state, int step = 0);
  void dispatch_via (TosState state, address* table);

  // jump to an invoked target
  void prepare_to_jump_from_interpreted();
  void jump_from_interpreted(Register method, Register temp);


  // Returning from interpreted functions
  //
  // Removes the current activation (incl. unlocking of monitors)
  // and sets up the return address.  This code is also used for
  // exception unwindwing. In that case, we do not want to throw
  // IllegalMonitorStateExceptions, since that might get us into an
  // infinite rethrow exception loop.
  // Additionally this code is used for popFrame and earlyReturn.
  // In popFrame case we want to skip throwing an exception,
  // installing an exception, and notifying jvmdi.
  // In earlyReturn case we only want to skip throwing an exception
  // and installing an exception.
  void remove_activation(TosState state, Register ret_addr,
                         bool throw_monitor_exception = true,
                         bool install_monitor_exception = true,
                         bool notify_jvmdi = true);
#endif // CC_INTERP

  // Object locking
  void lock_object  (Register lock_reg);
  void unlock_object(Register lock_reg);

#ifndef CC_INTERP

  // Interpreter profiling operations
  void set_method_data_pointer_for_bcp();
  void test_method_data_pointer(Register mdp, Label& zero_continue);
  void verify_method_data_pointer();

  void set_mdp_data_at(Register mdp_in, int constant, Register value);
  void increment_mdp_data_at(Address data, bool decrement = false);
  void increment_mdp_data_at(Register mdp_in, int constant,
                             bool decrement = false);
  void increment_mdp_data_at(Register mdp_in, Register reg, int constant,
                             bool decrement = false);
  void increment_mask_and_jump(Address counter_addr,
                               int increment, int mask,
                               Register scratch, bool preloaded,
                               Condition cond, Label* where);
  void set_mdp_flag_at(Register mdp_in, int flag_constant);
  void test_mdp_data_at(Register mdp_in, int offset, Register value,
                        Register test_value_out,
                        Label& not_equal_continue);

  void record_klass_in_profile(Register receiver, Register mdp,
                               Register reg2, bool is_virtual_call);
  void record_klass_in_profile_helper(Register receiver, Register mdp,
                                      Register reg2, int start_row,
                                      Label& done, bool is_virtual_call);

  void update_mdp_by_offset(Register mdp_in, int offset_of_offset);
  void update_mdp_by_offset(Register mdp_in, Register reg, int offset_of_disp);
  void update_mdp_by_constant(Register mdp_in, int constant);
  void update_mdp_for_ret(Register return_bci);

  void profile_taken_branch(Register mdp, Register bumped_count);
  void profile_not_taken_branch(Register mdp);
  void profile_call(Register mdp);
  void profile_final_call(Register mdp);
  void profile_virtual_call(Register receiver, Register mdp,
                            Register scratch2,
                            bool receiver_can_be_null = false);
  void profile_ret(Register return_bci, Register mdp);
  void profile_null_seen(Register mdp);
  void profile_typecheck(Register mdp, Register klass, Register scratch);
  void profile_typecheck_failed(Register mdp);
  void profile_switch_default(Register mdp);
  void profile_switch_case(Register index_in_scratch, Register mdp,
                           Register scratch2);

  // Debugging
  // only if +VerifyOops && state == atos
  void verify_oop(Register reg, TosState state = atos);
  // only if +VerifyFPU  && (state == ftos || state == dtos)
  void verify_FPU(int stack_depth, TosState state = ftos);

  void profile_obj_type(Register obj, const Address& mdo_addr);
  void profile_arguments_type(Register mdp, Register callee, Register tmp, bool is_virtual);
  void profile_return_type(Register mdp, Register ret, Register tmp);
  void profile_parameters_type(Register mdp, Register tmp1, Register tmp2);
#endif // !CC_INTERP

  typedef enum { NotifyJVMTI, SkipNotifyJVMTI } NotifyMethodExitMode;

  // support for jvmti/dtrace
  void notify_method_entry();
  void notify_method_exit(TosState state, NotifyMethodExitMode mode);
};

#endif // CPU_LOONGARCH_VM_INTERP_MASM_LOONGARCH_64_HPP
