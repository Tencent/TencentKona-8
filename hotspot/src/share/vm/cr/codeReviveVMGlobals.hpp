/*
 *
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#ifndef SHARE_VM_CR_CODE_REVIVE_VM_GLOBLAS_HPP
#define SHARE_VM_CR_CODE_REVIVE_VM_GLOBLAS_HPP
#include "memory/allocation.hpp"
/*
 * CodeReviveVMGlobals saves JVM internal globals used in all CodeBlobs.
 * Includes: stubs address, heap, method entries
 *
 * Using scenario:
 * 1. when load AOT result for stubs/compiled method, query new address with relocation info <CR_GLOBAL_KIND, ofst>.
 * 2. when dump AOT code for stubs/compiled method, query address's <CR_GLOBAL_KIND, ofst>.
      Generate relocation info <CR_GLOBAL_KIND, ofst>.
 * 3. After runtime start or creating stubs, remeber mapping between address and CR_GLOBAL_KIND.
 *
 * register time should be ASAP for AOT load.
 */
enum CR_GLOBAL_KIND {
  CR_GLOBAL_NONE = -1,
  CR_GLOBAL_FIRST = 0,

  // os related
  CR_OS_polling_page = 0,
  CR_OS_javaTimeMillis,
  CR_OS_javaTimeNanos,
  CR_OS_break_point,

  // heap related
  CR_HEAP_card_table_base,
  CR_HEAP_top_addr,
  CR_HEAP_end_addr,

  // universe
  CR_UNIVERSE_narrow_ptrs_base_addr,

  // opto runtime
  CR_OPTO_exception_blob,
  CR_OPTO_new_instance_Java,
  CR_OPTO_new_array_Java,
  CR_OPTO_new_array_nozero_Java,
  CR_OPTO_multianewarray2_Java,
  CR_OPTO_multianewarray3_Java,
  CR_OPTO_multianewarray4_Java,
  CR_OPTO_multianewarray5_Java,
  CR_OPTO_multianewarrayN_Java,
  CR_OPTO_g1_wb_pre_Java,
  CR_OPTO_g1_wb_post_Java,
  CR_OPTO_complete_monitor_locking_Java,
  CR_OPTO_rethrow_Java,
  CR_OPTO_slow_arraycopy_Java,
  CR_OPTO_register_finalizer_Java,
#ifdef ENABLE_ZAP_DEAD_LOCALS
  CR_OPTO_zap_dead_Java_locals_Java,
  CR_OPTO_zap_dead_native_locals_Java,
#endif

  // shared runtime
  CR_SR_g1_wb_pre,
  CR_SR_g1_wb_post,
  CR_SR_deopt_blob_unpack,
  CR_SR_ic_miss_blob,
  CR_SR_uncommon_trap_blob,
  CR_SR_complete_monitor_unlocking_C,
  CR_SR_dsin,
  CR_SR_dcos,
  CR_SR_dtan,
  CR_SR_dlog,
  CR_SR_dlog10,
  CR_SR_dexp,
  CR_SR_dpow,
#ifdef AMD64
#ifndef _WINDOWS
  CR_SR_montgomery_multiply,
  CR_SR_montgomery_square,
#endif
#endif
#ifndef PRODUCT
  CR_SR_partial_subtype_ctr,
#endif
  // stub routine
#ifdef AMD64
  CR_STUB_f2i_fixup,
  CR_STUB_f2l_fixup,
  CR_STUB_d2i_fixup,
  CR_STUB_d2l_fixup,
  CR_STUB_float_sign_mask,
  CR_STUB_float_sign_flip,
  CR_STUB_double_sign_mask,
  CR_STUB_double_sign_flip,
#endif
  CR_STUB_forward_exception_entry,
  CR_STUB_catch_exception_entry,
  CR_STUB_throw_AbstractMethodError_entry,
  CR_STUB_throw_IncompatibleClassChangeError_entry,
  CR_STUB_throw_NullPointerException_at_call_entry,
  CR_STUB_throw_StackOverflowError_entry,
  CR_STUB_handler_for_unsafe_access_entry,
  CR_STUB_atomic_xchg_entry,
  CR_STUB_atomic_xchg_ptr_entry,
  CR_STUB_atomic_store_entry,
  CR_STUB_atomic_store_ptr_entry,
  CR_STUB_atomic_cmpxchg_entry,
  CR_STUB_atomic_cmpxchg_ptr_entry,
  CR_STUB_atomic_cmpxchg_long_entry,
  CR_STUB_atomic_add_entry,
  CR_STUB_atomic_add_ptr_entry,
  CR_STUB_fence_entry,
  CR_STUB_d2i_wrapper,
  CR_STUB_d2l_wrapper,
  CR_STUB_addr_fpu_cntrl_wrd_std,
  CR_STUB_addr_fpu_cntrl_wrd_24,
  CR_STUB_addr_fpu_cntrl_wrd_64,
  CR_STUB_addr_fpu_cntrl_wrd_trunc,
  CR_STUB_addr_mxcsr_std,
  CR_STUB_addr_fpu_subnormal_bias1,
  CR_STUB_addr_fpu_subnormal_bias2,
  CR_STUB_jbyte_arraycopy,
  CR_STUB_jshort_arraycopy,
  CR_STUB_jint_arraycopy,
  CR_STUB_jlong_arraycopy,
  CR_STUB_oop_arraycopy,
  CR_STUB_oop_arraycopy_uninit,
  CR_STUB_jbyte_disjoint_arraycopy,
  CR_STUB_jshort_disjoint_arraycopy,
  CR_STUB_jint_disjoint_arraycopy,
  CR_STUB_jlong_disjoint_arraycopy,
  CR_STUB_oop_disjoint_arraycopy,
  CR_STUB_oop_disjoint_arraycopy_uninit,
  CR_STUB_arrayof_jbyte_arraycopy,
  CR_STUB_arrayof_jshort_arraycopy,
  CR_STUB_arrayof_jint_arraycopy,
  CR_STUB_arrayof_jlong_arraycopy,
  CR_STUB_arrayof_oop_arraycopy,
  CR_STUB_arrayof_oop_arraycopy_uninit,
  CR_STUB_arrayof_jbyte_disjoint_arraycopy,
  CR_STUB_arrayof_jshort_disjoint_arraycopy,
  CR_STUB_arrayof_jint_disjoint_arraycopy,
  CR_STUB_arrayof_jlong_disjoint_arraycopy,
  CR_STUB_arrayof_oop_disjoint_arraycopy,
  CR_STUB_arrayof_oop_disjoint_arraycopy_uninit,
  CR_STUB_checkcast_arraycopy,
  CR_STUB_checkcast_arraycopy_uninit,
  CR_STUB_unsafe_arraycopy,
  CR_STUB_generic_arraycopy,
  CR_STUB_jbyte_fill,
  CR_STUB_jshort_fill,
  CR_STUB_jint_fill,
  CR_STUB_arrayof_jbyte_fill,
  CR_STUB_arrayof_jshort_fill,
  CR_STUB_arrayof_jint_fill,
  CR_STUB_zero_aligned_words,
  CR_STUB_aescrypt_encryptBlock,
  CR_STUB_aescrypt_decryptBlock,
  CR_STUB_cipherBlockChaining_encryptAESCrypt,
  CR_STUB_cipherBlockChaining_decryptAESCrypt,
  CR_STUB_ghash_processBlocks,
  CR_STUB_sha1_implCompress,
  CR_STUB_sha1_implCompressMB,
  CR_STUB_sha256_implCompress,
  CR_STUB_sha256_implCompressMB,
  CR_STUB_sha512_implCompress,
  CR_STUB_sha512_implCompressMB,
  CR_STUB_updateBytesCRC32,
#ifdef X86
  CR_STUB_crc_table_addr,
#endif
  CR_STUB_multiplyToLen,
  CR_STUB_squareToLen,
  CR_STUB_mulAdd,
#ifndef _WINDOWS
  CR_STUB_montgomeryMultiply,
  CR_STUB_montgomerySquare,
#endif
  CR_STUB_utf16_to_utf8_encoder,
  CR_STUB_utf8_to_utf16_decoder,

  // c1 runtime
  CR_GLOBAL_LAST           = CR_STUB_utf8_to_utf16_decoder,
  CR_GLOBAL_SIZE           = CR_GLOBAL_LAST + 1
};

class CodeReviveVMGlobals : public CHeapObj<mtInternal> {
 public:
  struct Entry {
    address  _addr; // start address
    address  _end;  // default is _addr + sizeof(address)
                    // if global represents an array, like _crc_table, records array end.
  };
  CodeReviveVMGlobals();

  CR_GLOBAL_KIND find(address addr, size_t* ofst); // find CR_GLOBAL_KIND and offset when dump

  address find(CR_GLOBAL_KIND k, size_t ofst);     // find adress when load

  void add(CR_GLOBAL_KIND k, address addr, address end=NULL); // register table entry when code revive is on

  void print();
  static const char* name(CR_GLOBAL_KIND k);

  void add_opto_runtime();
 private:
  Entry _table[CR_GLOBAL_SIZE];
};
#endif // SHARE_VM_CR_CODE_REVIVE_VM_GLOBLAS_HPP
