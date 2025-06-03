/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_LOONGARCH_VM_MACROASSEMBLER_LOONGARCH_HPP
#define CPU_LOONGARCH_VM_MACROASSEMBLER_LOONGARCH_HPP

#include "asm/assembler.hpp"
#include "utilities/macros.hpp"
#include "runtime/rtmLocking.hpp"


// MacroAssembler extends Assembler by frequently used macros.
//
// Instructions for which a 'better' code sequence exists depending
// on arguments should also go in here.

class MacroAssembler: public Assembler {
  friend class LIR_Assembler;
  friend class Runtime1;      // as_Address()

 public:
  // Compare code
  typedef enum {
    EQ = 0x01,
    NE = 0x02,
    GT = 0x03,
    GE = 0x04,
    LT = 0x05,
    LE = 0x06
  } CMCompare;

 protected:

  Address as_Address(AddressLiteral adr);
  Address as_Address(ArrayAddress adr);

  // Support for VM calls
  //
  // This is the base routine called by the different versions of call_VM_leaf. The interpreter
  // may customize this version by overriding it for its purposes (e.g., to save/restore
  // additional registers when doing a VM call).
#ifdef CC_INTERP
  // c++ interpreter never wants to use interp_masm version of call_VM
  #define VIRTUAL
#else
  #define VIRTUAL virtual
#endif

  VIRTUAL void call_VM_leaf_base(
    address entry_point,               // the entry point
    int     number_of_arguments        // the number of arguments to pop after the call
  );

  // This is the base routine called by the different versions of call_VM. The interpreter
  // may customize this version by overriding it for its purposes (e.g., to save/restore
  // additional registers when doing a VM call).
  //
  // If no java_thread register is specified (noreg) than TREG will be used instead. call_VM_base
  // returns the register which contains the thread upon return. If a thread register has been
  // specified, the return value will correspond to that register. If no last_java_sp is specified
  // (noreg) than sp will be used instead.
  VIRTUAL void call_VM_base(           // returns the register containing the thread upon return
    Register oop_result,               // where an oop-result ends up if any; use noreg otherwise
    Register java_thread,              // the thread if computed before     ; use noreg otherwise
    Register last_java_sp,             // to set up last_Java_frame in stubs; use noreg otherwise
    address  entry_point,              // the entry point
    int      number_of_arguments,      // the number of arguments (w/o thread) to pop after the call
    bool     check_exceptions          // whether to check for pending exceptions after return
  );

  // These routines should emit JVMTI PopFrame and ForceEarlyReturn handling code.
  // The implementation is only non-empty for the InterpreterMacroAssembler,
  // as only the interpreter handles PopFrame and ForceEarlyReturn requests.
  virtual void check_and_handle_popframe(Register java_thread);
  virtual void check_and_handle_earlyret(Register java_thread);

  void call_VM_helper(Register oop_result, address entry_point, int number_of_arguments, bool check_exceptions = true);

  // helpers for FPU flag access
  // tmp is a temporary register, if none is available use noreg

 public:
  static intptr_t  i[32];
  static float  f[32];
  static void print(outputStream *s);

  static int i_offset(unsigned int k);
  static int f_offset(unsigned int k);

  static void save_registers(MacroAssembler *masm);
  static void restore_registers(MacroAssembler *masm);

  MacroAssembler(CodeBuffer* code) : Assembler(code) {}

  // Support for NULL-checks
  //
  // Generates code that causes a NULL OS exception if the content of reg is NULL.
  // If the accessed location is M[reg + offset] and the offset is known, provide the
  // offset. No explicit code generation is needed if the offset is within a certain
  // range (0 <= offset <= page_size).

  void null_check(Register reg, int offset = -1);
  static bool needs_explicit_null_check(intptr_t offset);

  // Required platform-specific helpers for Label::patch_instructions.
  // They _shadow_ the declarations in AbstractAssembler, which are undefined.
  void pd_patch_instruction(address branch, address target);

  address emit_trampoline_stub(int insts_call_instruction_offset, address target);

  // Support for inc/dec with optimal instruction selection depending on value
  // void incrementl(Register reg, int value = 1);
  // void decrementl(Register reg, int value = 1);


  // Alignment
  void align(int modulus);


  // Stack frame creation/removal
  void enter();
  void leave();

  // Frame creation and destruction shared between JITs.
  void build_frame(int framesize);
  void remove_frame(int framesize);

  // Support for getting the JavaThread pointer (i.e.; a reference to thread-local information)
  // The pointer will be loaded into the thread register.
  void get_thread(Register thread);


  // Support for VM calls
  //
  // It is imperative that all calls into the VM are handled via the call_VM macros.
  // They make sure that the stack linkage is setup correctly. call_VM's correspond
  // to ENTRY/ENTRY_X entry points while call_VM_leaf's correspond to LEAF entry points.


  void call_VM(Register oop_result,
               address entry_point,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               address entry_point,
               Register arg_1,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               address entry_point,
               Register arg_1, Register arg_2,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               address entry_point,
               Register arg_1, Register arg_2, Register arg_3,
               bool check_exceptions = true);

  // Overloadings with last_Java_sp
  void call_VM(Register oop_result,
               Register last_java_sp,
               address entry_point,
               int number_of_arguments = 0,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               Register last_java_sp,
               address entry_point,
               Register arg_1, bool
               check_exceptions = true);
  void call_VM(Register oop_result,
               Register last_java_sp,
               address entry_point,
               Register arg_1, Register arg_2,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               Register last_java_sp,
               address entry_point,
               Register arg_1, Register arg_2, Register arg_3,
               bool check_exceptions = true);

  void get_vm_result  (Register oop_result, Register thread);
  void get_vm_result_2(Register metadata_result, Register thread);
  void call_VM_leaf(address entry_point,
                    int number_of_arguments = 0);
  void call_VM_leaf(address entry_point,
                    Register arg_1);
  void call_VM_leaf(address entry_point,
                    Register arg_1, Register arg_2);
  void call_VM_leaf(address entry_point,
                    Register arg_1, Register arg_2, Register arg_3);

  // Super call_VM calls - correspond to MacroAssembler::call_VM(_leaf) calls
  void super_call_VM_leaf(address entry_point);
  void super_call_VM_leaf(address entry_point, Register arg_1);
  void super_call_VM_leaf(address entry_point, Register arg_1, Register arg_2);
  void super_call_VM_leaf(address entry_point, Register arg_1, Register arg_2, Register arg_3);

  // last Java Frame (fills frame anchor)
  void set_last_Java_frame(Register thread,
                           Register last_java_sp,
                           Register last_java_fp,
                           Label& last_java_pc);

  // thread in the default location (S6)
  void set_last_Java_frame(Register last_java_sp,
                           Register last_java_fp,
                           Label& last_java_pc);

  void reset_last_Java_frame(Register thread, bool clear_fp);

  // thread in the default location (S6)
  void reset_last_Java_frame(bool clear_fp);

  // Stores
  void store_check(Register obj);                // store check for obj - register is destroyed afterwards
  void store_check(Register obj, Address dst);   // same as above, dst is exact store location (reg. is destroyed)

 void resolve_jobject(Register value, Register thread, Register tmp);
 void clear_jweak_tag(Register possibly_jweak);

#if INCLUDE_ALL_GCS

  void g1_write_barrier_pre(Register obj,
                            Register pre_val,
                            Register thread,
                            Register tmp,
                            bool tosca_live,
                            bool expand_call);

  void g1_write_barrier_post(Register store_addr,
                             Register new_val,
                             Register thread,
                             Register tmp,
                             Register tmp2);

#endif // INCLUDE_ALL_GCS

  // split store_check(Register obj) to enhance instruction interleaving
  void store_check_part_1(Register obj);
  void store_check_part_2(Register obj);

  // C 'boolean' to Java boolean: x == 0 ? 0 : 1
  void c2bool(Register x);
  //add for compressedoops
  void load_klass(Register dst, Register src);
  void store_klass(Register dst, Register src);
  void load_prototype_header(Register dst, Register src);

  void store_klass_gap(Register dst, Register src);

  void load_heap_oop(Register dst, Address src);
  void store_heap_oop(Address dst, Register src);
  void store_heap_oop_null(Address dst);
  void encode_heap_oop(Register r);
  void encode_heap_oop(Register dst, Register src);
  void decode_heap_oop(Register r);
  void decode_heap_oop(Register dst, Register src);
  void encode_heap_oop_not_null(Register r);
  void decode_heap_oop_not_null(Register r);
  void encode_heap_oop_not_null(Register dst, Register src);
  void decode_heap_oop_not_null(Register dst, Register src);

  void encode_klass_not_null(Register r);
  void decode_klass_not_null(Register r);
  void encode_klass_not_null(Register dst, Register src);
  void decode_klass_not_null(Register dst, Register src);

  // Returns the byte size of the instructions generated by decode_klass_not_null()
  // when compressed klass pointers are being used.
  static int instr_size_for_decode_klass_not_null();

  // if heap base register is used - reinit it with the correct value
  void reinit_heapbase();

  DEBUG_ONLY(void verify_heapbase(const char* msg);)

  void set_narrow_klass(Register dst, Klass* k);
  void set_narrow_oop(Register dst, jobject obj);

  // Sign extension
  void sign_extend_short(Register reg) { ext_w_h(reg, reg); }
  void sign_extend_byte(Register reg)  { ext_w_b(reg, reg); }
  void rem_s(FloatRegister fd, FloatRegister fs, FloatRegister ft, FloatRegister tmp);
  void rem_d(FloatRegister fd, FloatRegister fs, FloatRegister ft, FloatRegister tmp);

  void trigfunc(char trig, int num_fpu_regs_in_use = 1);
  // allocation
  void eden_allocate(
    Register obj,                      // result: pointer to object after successful allocation
    Register var_size_in_bytes,        // object size in bytes if unknown at compile time; invalid otherwise
    int      con_size_in_bytes,        // object size in bytes if   known at compile time
    Register t1,                       // temp register
    Label&   slow_case                 // continuation point if fast allocation fails
  );
  void tlab_allocate(
    Register obj,                      // result: pointer to object after successful allocation
    Register var_size_in_bytes,        // object size in bytes if unknown at compile time; invalid otherwise
    int      con_size_in_bytes,        // object size in bytes if   known at compile time
    Register t1,                       // temp register
    Register t2,                       // temp register
    Label&   slow_case                 // continuation point if fast allocation fails
  );
  void incr_allocated_bytes(Register thread,
                            Register var_size_in_bytes, int con_size_in_bytes,
                            Register t1 = noreg);
  // interface method calling
  void lookup_interface_method(Register recv_klass,
                               Register intf_klass,
                               RegisterOrConstant itable_index,
                               Register method_result,
                               Register scan_temp,
                               Label& no_such_interface,
                               bool return_method = true);

  // virtual method calling
  void lookup_virtual_method(Register recv_klass,
                             RegisterOrConstant vtable_index,
                             Register method_result);

  // Test sub_klass against super_klass, with fast and slow paths.

  // The fast path produces a tri-state answer: yes / no / maybe-slow.
  // One of the three labels can be NULL, meaning take the fall-through.
  // If super_check_offset is -1, the value is loaded up from super_klass.
  // No registers are killed, except temp_reg.
  void check_klass_subtype_fast_path(Register sub_klass,
                                     Register super_klass,
                                     Register temp_reg,
                                     Label* L_success,
                                     Label* L_failure,
                                     Label* L_slow_path,
                RegisterOrConstant super_check_offset = RegisterOrConstant(-1));

  // The rest of the type check; must be wired to a corresponding fast path.
  // It does not repeat the fast path logic, so don't use it standalone.
  // The temp_reg and temp2_reg can be noreg, if no temps are available.
  // Updates the sub's secondary super cache as necessary.
  // If set_cond_codes, condition codes will be Z on success, NZ on failure.
  void check_klass_subtype_slow_path(Register sub_klass,
                                     Register super_klass,
                                     Register temp_reg,
                                     Register temp2_reg,
                                     Label* L_success,
                                     Label* L_failure,
                                     bool set_cond_codes = false);

  // Simplified, combined version, good for typical uses.
  // Falls through on failure.
  void check_klass_subtype(Register sub_klass,
                           Register super_klass,
                           Register temp_reg,
                           Label& L_success);


  // Debugging

  // only if +VerifyOops
  void verify_oop(Register reg, const char* s = "broken oop");
  void verify_oop_addr(Address addr, const char * s = "broken oop addr");
  void verify_oop_subroutine();
  // TODO: verify method and klass metadata (compare against vptr?)
  void _verify_method_ptr(Register reg, const char * msg, const char * file, int line) {}
  void _verify_klass_ptr(Register reg, const char * msg, const char * file, int line){}

  #define verify_method_ptr(reg) _verify_method_ptr(reg, "broken method " #reg, __FILE__, __LINE__)
  #define verify_klass_ptr(reg) _verify_klass_ptr(reg, "broken klass " #reg, __FILE__, __LINE__)

  // only if +VerifyFPU
  void verify_FPU(int stack_depth, const char* s = "illegal FPU state");

  // prints msg, dumps registers and stops execution
  void stop(const char* msg);

  // prints msg and continues
  void warn(const char* msg);

  static void debug(char* msg/*, RegistersForDebugging* regs*/);
  static void debug64(char* msg, int64_t pc, int64_t regs[]);

  void untested()                                { stop("untested"); }

  void unimplemented(const char* what = "")      { char* b = new char[1024];  jio_snprintf(b, sizeof(b), "unimplemented: %s", what);  stop(b); }

  void should_not_reach_here()                   { stop("should not reach here"); }

  void print_CPU_state();

  // Stack overflow checking
  void bang_stack_with_offset(int offset) {
    // stack grows down, caller passes positive offset
    assert(offset > 0, "must bang with negative offset");
    if (offset <= 2048) {
      st_w(RA0, SP, -offset);
    } else if (offset <= 32768 && !(offset & 3)) {
      stptr_w(RA0, SP, -offset);
    } else {
      li(AT, offset);
      sub_d(AT, SP, AT);
      st_w(RA0, AT, 0);
    }
  }

  // Writes to stack successive pages until offset reached to check for
  // stack overflow + shadow pages.  Also, clobbers tmp
  void bang_stack_size(Register size, Register tmp);

  virtual RegisterOrConstant delayed_value_impl(intptr_t* delayed_value_addr,
                                                Register tmp,
                                                int offset);

  // Support for serializing memory accesses between threads
  void serialize_memory(Register thread, Register tmp);

  //void verify_tlab();
  void verify_tlab(Register t1, Register t2);

  // Biased locking support
  // lock_reg and obj_reg must be loaded up with the appropriate values.
  // tmp_reg is optional. If it is supplied (i.e., != noreg) it will
  // be killed; if not supplied, push/pop will be used internally to
  // allocate a temporary (inefficient, avoid if possible).
  // Optional slow case is for implementations (interpreter and C1) which branch to
  // slow case directly. Leaves condition codes set for C2's Fast_Lock node.
  // Returns offset of first potentially-faulting instruction for null
  // check info (currently consumed only by C1). If
  // swap_reg_contains_mark is true then returns -1 as it is assumed
  // the calling code has already passed any potential faults.
  int biased_locking_enter(Register lock_reg, Register obj_reg,
                           Register swap_reg, Register tmp_reg,
                           bool swap_reg_contains_mark,
                           Label& done, Label* slow_case = NULL,
                           BiasedLockingCounters* counters = NULL);
  void biased_locking_exit (Register obj_reg, Register temp_reg, Label& done);
#ifdef COMPILER2
  void fast_lock(Register obj, Register box, Register res, Register tmp, Register scr);
  void fast_unlock(Register obj, Register box, Register res, Register tmp, Register scr);
#endif

  void round_to(Register reg, int modulus) {
  //TODO: LA
  guarantee(0, "LA not implemented yet");
#if 0
    assert_different_registers(reg, AT);
    increment(reg, modulus - 1);
    move(AT, - modulus);
    andr(reg, reg, AT);
#endif
  }

  // the follow two might use AT register, be sure you have no meanful data in AT before you call them
  void increment(Register reg, int imm);
  void decrement(Register reg, int imm);
  void increment(Address addr, int imm = 1);
  void decrement(Address addr, int imm = 1);
  void shl(Register reg, int sa)        { slli_d(reg, reg, sa); }
  void shr(Register reg, int sa)        { srli_d(reg, reg, sa); }
  void sar(Register reg, int sa)        { srai_d(reg, reg, sa); }
  // Helper functions for statistics gathering.
  void atomic_inc32(address counter_addr, int inc, Register tmp_reg1, Register tmp_reg2);

  // Calls
  void call(address entry);
  void call(address entry, relocInfo::relocType rtype);
  void call(address entry, RelocationHolder& rh);
  void call_long(address entry);

  address trampoline_call(AddressLiteral entry, CodeBuffer *cbuf = NULL);

  static const unsigned long branch_range = NOT_DEBUG(128 * M) DEBUG_ONLY(2 * M);

  static bool far_branches() {
    if (ForceUnreachable) {
      return true;
    } else {
      return ReservedCodeCacheSize > branch_range;
    }
  }

  // Emit the CompiledIC call idiom
  address ic_call(address entry);

  // Jumps
  void jmp(address entry);
  void jmp(address entry, relocInfo::relocType rtype);
  void jmp_far(Label& L); // patchable

  /* branches may exceed 16-bit offset */
  void b_far(address entry);
  void b_far(Label& L);

  void bne_far    (Register rs, Register rt, address entry);
  void bne_far    (Register rs, Register rt, Label& L);

  void beq_far    (Register rs, Register rt, address entry);
  void beq_far    (Register rs, Register rt, Label& L);

  void blt_far    (Register rs, Register rt, address entry, bool is_signed);
  void blt_far    (Register rs, Register rt, Label& L, bool is_signed);

  void bge_far    (Register rs, Register rt, address entry, bool is_signed);
  void bge_far    (Register rs, Register rt, Label& L, bool is_signed);

  // For C2 to support long branches
  void beq_long   (Register rs, Register rt, Label& L);
  void bne_long   (Register rs, Register rt, Label& L);
  void blt_long   (Register rs, Register rt, Label& L, bool is_signed);
  void bge_long   (Register rs, Register rt, Label& L, bool is_signed);
  void bc1t_long  (Label& L);
  void bc1f_long  (Label& L);

  static bool patchable_branches() {
    const unsigned long branch_range = NOT_DEBUG(128 * M) DEBUG_ONLY(2 * M);
    return ReservedCodeCacheSize > branch_range;
  }

  static bool reachable_from_branch_short(jlong offs);

  void patchable_jump_far(Register ra, jlong offs);
  void patchable_jump(address target, bool force_patchable = false);
  void patchable_call(address target, address call_size = 0);

  // Floating
  // Data

  // Load and store values by size and signed-ness
  void load_sized_value(Register dst, Address src, size_t size_in_bytes, bool is_signed, Register dst2 = noreg);
  void store_sized_value(Address dst, Register src, size_t size_in_bytes, Register src2 = noreg);

  // ld_ptr will perform lw for 32 bit VMs and ld for 64 bit VMs
  inline void ld_ptr(Register rt, Address a) {
    ld_d(rt, a);
  }

  inline void ld_ptr(Register rt, Register base, int offset16) {
    ld_d(rt, base, offset16);
  }

  // st_ptr will perform sw for 32 bit VMs and sd for 64 bit VMs
  inline void st_ptr(Register rt, Address a) {
    st_d(rt, a);
  }

  inline void st_ptr(Register rt, Register base, int offset16) {
    st_d(rt, base, offset16);
  }

  void ld_ptr(Register rt, Register base, Register offset);
  void st_ptr(Register rt, Register base, Register offset);

  // ld_long will perform lw for 32 bit VMs and ld for 64 bit VMs
  // st_long will perform sw for 32 bit VMs and sd for 64 bit VMs
  inline void ld_long(Register rt, Register base, int offset16);
  inline void st_long(Register rt, Register base, int offset16);
  inline void ld_long(Register rt, Address a);
  inline void st_long(Register rt, Address a);
  void ld_long(Register rt, Register offset, Register base);
  void st_long(Register rt, Register offset, Register base);

  // swap the two byte of the low 16-bit halfword
  // this directive will use AT, be sure the high 16-bit of reg is zero
  void hswap(Register reg);
  void huswap(Register reg);

  // convert big endian integer to little endian integer
  void swap(Register reg);

  void cmpxchg(Address addr, Register oldval, Register newval, Register resflag,
               bool retold, bool barrier);
  void cmpxchg(Address addr, Register oldval, Register newval, Register tmp,
               bool retold, bool barrier, Label& succ, Label* fail = NULL);
  void cmpxchg32(Address addr, Register oldval, Register newval, Register resflag,
                 bool sign, bool retold, bool barrier);
  void cmpxchg32(Address addr, Register oldval, Register newval, Register tmp,
                 bool sign, bool retold, bool barrier, Label& succ, Label* fail = NULL);

  void extend_sign(Register rh, Register rl) { /*stop("extend_sign");*/ guarantee(0, "LA not implemented yet");}
  void neg(Register reg) { /*dsubu(reg, R0, reg);*/ guarantee(0, "LA not implemented yet");}
  void push (Register reg)      { addi_d(SP, SP, -8); st_d  (reg, SP, 0); }
  void push (FloatRegister reg) { addi_d(SP, SP, -8); fst_d (reg, SP, 0); }
  void pop  (Register reg)      { ld_d  (reg, SP, 0);  addi_d(SP, SP, 8); }
  void pop  (FloatRegister reg) { fld_d (reg, SP, 0);  addi_d(SP, SP, 8); }
  void pop  ()                  { addi_d(SP, SP, 8); }
  void pop2 ()                  { addi_d(SP, SP, 16); }
  void push2(Register reg1, Register reg2);
  void pop2 (Register reg1, Register reg2);
  //we need 2 fun to save and resotre general register
  void pushad();
  void popad();
  void pushad_except_v0();
  void popad_except_v0();

  void li(Register rd, jlong value);
  void li(Register rd, address addr) { li(rd, (long)addr); }
  void patchable_li52(Register rd, jlong value);
  void lipc(Register rd, Label& L);
  void move(Register rd, Register rs)   { orr(rd, rs, R0); }
  void move_u32(Register rd, Register rs)   { add_w(rd, rs, R0); }
  void mov_metadata(Register dst, Metadata* obj);
  void mov_metadata(Address dst, Metadata* obj);

  // Load the base of the cardtable byte map into reg.
  void load_byte_map_base(Register reg);

  //FIXME
  void empty_FPU_stack(){/*need implemented*/};


  // method handles (JSR 292)
  Address argument_address(RegisterOrConstant arg_slot, int extra_slot_offset = 0);


  // LA added:
  void jr  (Register reg)        { jirl(R0, reg, 0); }
  void jalr(Register reg)        { jirl(RA, reg, 0); }
  void nop ()                    { andi(R0, R0, 0); }
  void andr(Register rd, Register rj, Register rk) { AND(rd, rj, rk); }
  void xorr(Register rd, Register rj, Register rk) { XOR(rd, rj, rk); }
  void orr (Register rd, Register rj, Register rk) {  OR(rd, rj, rk); }
  void lea (Register rd, Address src);
  void lea (Register dst, AddressLiteral adr);
  static int  patched_branch(int dest_pos, int inst, int inst_pos);

  // Conditional move
  void cmp_cmov(Register        op1,
                Register        op2,
                Register        dst,
                Register        src,
                CMCompare       cmp = EQ,
                bool      is_signed = true);
  void cmp_cmov(FloatRegister   op1,
                FloatRegister   op2,
                Register        dst,
                Register        src,
                FloatRegister   tmp1,
                FloatRegister   tmp2,
                CMCompare       cmp = EQ,
                bool       is_float = true);
  void cmp_cmov(FloatRegister   op1,
                FloatRegister   op2,
                FloatRegister   dst,
                FloatRegister   src,
                CMCompare       cmp = EQ,
                bool       is_float = true);
  void cmp_cmov(Register        op1,
                Register        op2,
                FloatRegister   dst,
                FloatRegister   src,
                FloatRegister   tmp1,
                FloatRegister   tmp2,
                CMCompare       cmp = EQ);

  // CRC32 code for java.util.zip.CRC32::update() instrinsic.
  void update_byte_crc32(Register crc, Register val, Register table);

  // CRC32 code for java.util.zip.CRC32::updateBytes() instrinsic.
  void kernel_crc32(Register crc, Register buf, Register len, Register tmp);

  // CRC32C code for java.util.zip.CRC32C::updateBytes() instrinsic.
  void kernel_crc32c(Register crc, Register buf, Register len, Register tmp);

#undef VIRTUAL

 public:
// Memory Data Type
#define INT_TYPE 0x100
#define FLOAT_TYPE 0x200
#define SIGNED_TYPE 0x10
#define UNSIGNED_TYPE 0x20

  typedef enum {
    LOAD_BYTE        = INT_TYPE | SIGNED_TYPE | 0x1,
    LOAD_CHAR        = INT_TYPE | SIGNED_TYPE | 0x2,
    LOAD_SHORT       = INT_TYPE | SIGNED_TYPE | 0x3,
    LOAD_INT         = INT_TYPE | SIGNED_TYPE | 0x4,
    LOAD_LONG        = INT_TYPE | SIGNED_TYPE | 0x5,
    STORE_BYTE       = INT_TYPE | SIGNED_TYPE | 0x6,
    STORE_CHAR       = INT_TYPE | SIGNED_TYPE | 0x7,
    STORE_SHORT      = INT_TYPE | SIGNED_TYPE | 0x8,
    STORE_INT        = INT_TYPE | SIGNED_TYPE | 0x9,
    STORE_LONG       = INT_TYPE | SIGNED_TYPE | 0xa,
    LOAD_LINKED_LONG = INT_TYPE | SIGNED_TYPE | 0xb,

    LOAD_U_BYTE      = INT_TYPE | UNSIGNED_TYPE | 0x1,
    LOAD_U_SHORT     = INT_TYPE | UNSIGNED_TYPE | 0x2,
    LOAD_U_INT       = INT_TYPE | UNSIGNED_TYPE | 0x3,

    LOAD_FLOAT       = FLOAT_TYPE | SIGNED_TYPE | 0x1,
    LOAD_DOUBLE      = FLOAT_TYPE | SIGNED_TYPE | 0x2,
    LOAD_VECTORX     = FLOAT_TYPE | SIGNED_TYPE | 0x3,
    LOAD_VECTORY     = FLOAT_TYPE | SIGNED_TYPE | 0x4,
    STORE_FLOAT      = FLOAT_TYPE | SIGNED_TYPE | 0x5,
    STORE_DOUBLE     = FLOAT_TYPE | SIGNED_TYPE | 0x6,
    STORE_VECTORX    = FLOAT_TYPE | SIGNED_TYPE | 0x7,
    STORE_VECTORY    = FLOAT_TYPE | SIGNED_TYPE | 0x8
  } CMLoadStoreDataType;

  void loadstore_enc(Register reg, int base, int index, int scale, int disp, int type) {
    assert((type & INT_TYPE), "must be General reg type");
    loadstore_t(reg, base, index, scale, disp, type);
  }

  void loadstore_enc(FloatRegister reg, int base, int index, int scale, int disp, int type) {
    assert((type & FLOAT_TYPE), "must be Float reg type");
    loadstore_t(reg, base, index, scale, disp, type);
  }

private:
  template <typename T>
  void loadstore_t(T reg, int base, int index, int scale, int disp, int type) {
    if (index != 0) {
        assert(((scale==0)&&(disp==0)), "only support base+index");
        loadstore(reg, as_Register(base), as_Register(index), type);
    } else {
      loadstore(reg, as_Register(base), disp, type);
    }
  }
  void loadstore(Register reg, Register base, int disp, int type);
  void loadstore(Register reg, Register base, Register disp, int type);
  void loadstore(FloatRegister reg, Register base, int disp, int type);
  void loadstore(FloatRegister reg, Register base, Register disp, int type);
};

/**
 * class SkipIfEqual:
 *
 * Instantiating this class will result in assembly code being output that will
 * jump around any code emitted between the creation of the instance and it's
 * automatic destruction at the end of a scope block, depending on the value of
 * the flag passed to the constructor, which will be checked at run-time.
 */
class SkipIfEqual {
 private:
  MacroAssembler* _masm;
  Label _label;

 public:
   SkipIfEqual(MacroAssembler*, const bool* flag_addr, bool value);
   ~SkipIfEqual();
};

#ifdef ASSERT
inline bool AbstractAssembler::pd_check_instruction_mark() { return true; }
#endif

struct tableswitch {
  Register _reg;
  int _insn_index; jint _first_key; jint _last_key;
  Label _after;
  Label _branches;
};

#endif // CPU_LOONGARCH_VM_MACROASSEMBLER_LOONGARCH_HPP
