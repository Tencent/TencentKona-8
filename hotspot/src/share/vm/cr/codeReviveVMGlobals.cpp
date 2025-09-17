/*
 * Copyright (C) 2022, 2023, Tencent. All rights reserved.
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
 */

#include "precompiled.hpp"
#include "cr/codeReviveVMGlobals.hpp"
#include "cr/revive.hpp"
#if !defined(ZERO)
#include "opto/runtime.hpp"
#endif
#include "runtime/os.hpp"
#include "runtime/sharedRuntime.hpp"
#include "utilities/ostream.hpp"

static const char* CR_SYMBOL_names[] = {
  // os
  "os::polling_page",
  "os::javaTimeMillis",
  "os::javaTimeNanos",
  "os::break_point",
  // heap
  "heap::card_table_base",
  "heap::top_addr",
  "heap::end_addr",
  // universe
  "Universe::narrow_ptrs_base_addr",
  // opto
  "OptoRuntime::exception_blob",
  "OptoRuntime::new_instance_Java",
  "OptoRuntime::new_array_Java",
  "OptoRuntime::new_array_nozero_Java",
  "OptoRuntime::multianewarray2_Java",
  "OptoRuntime::multianewarray3_Java",
  "OptoRuntime::multianewarray4_Java",
  "OptoRuntime::multianewarray5_Java",
  "OptoRuntime::multianewarrayN_Java",
  "OptoRuntime::g1_wb_pre_Java",
  "OptoRuntime::g1_wb_post_Java",
  "OptoRuntime::complete_monitor_locking_Java",
  "OptoRuntime::rethrow_Java",
  "OptoRuntime::slow_arraycopy_Java",
  "OptoRuntime::register_finalizer_Java",
#ifdef ENABLE_ZAP_DEAD_LOCALS
  "OptoRuntime::zap_dead_Java_locals_Java",
  "OptoRuntime::zap_dead_native_locals_Java",
#endif

  // shared runtime
  "SharedRuntime::g1_wb_pre",
  "SharedRuntime::g1_wb_post",
  "SharedRuntime::deopt_blob_unpack",
  "SharedRuntime::ic_miss_blob",
  "SharedRuntime::uncommon_trap_blob",
  "SharedRuntime::complete_monitor_unlocking_C",
  "SharedRuntime::dsin",
  "SharedRuntime::dcos",
  "SharedRuntime::dtan",
  "SharedRuntime::dlog",
  "SharedRuntime::dlog10",
  "SharedRuntime::dexp",
  "SharedRuntime::dpow",
#ifdef AMD64
#ifndef _WINDOWS
  "SharedRuntime::montgomery_multiply",
  "SharedRuntime::montgomery_square",
#endif
#endif
#ifndef PRODUCT
  "SharedRuntime::partial_subtype_ctr",
#endif
  // stub routine
#ifdef AMD64
  // no need include addresses in stubRoutines_x86.hpp
  "StubRoutine::f2i_fixup",
  "StubRoutine::f2l_fixup",
  "StubRoutine::d2i_fixup",
  "StubRoutine::d2l_fixup",
  "StubRoutine::float_sign_mask",
  "StubRoutine::float_sign_flip",
  "StubRoutine::double_sign_mask",
  "StubRoutine::double_sign_flip",
#endif
  "StubRoutine::forward_exception_entry",
  "StubRoutine::catch_exception_entry",
  "StubRoutine::throw_AbstractMethodError_entry",
  "StubRoutine::throw_IncompatibleClassChangeError_entry",
  "StubRoutine::throw_NullPointerException_at_call_entry",
  "StubRoutine::throw_StackOverflowError_entry",
  "StubRoutine::handler_for_unsafe_access_entry",
  "StubRoutine::atomic_xchg_entry",
  "StubRoutine::atomic_xchg_ptr_entry",
  "StubRoutine::atomic_store_entry",
  "StubRoutine::atomic_store_ptr_entry",
  "StubRoutine::atomic_cmpxchg_entry",
  "StubRoutine::atomic_cmpxchg_ptr_entry",
  "StubRoutine::atomic_cmpxchg_long_entry",
  "StubRoutine::atomic_add_entry",
  "StubRoutine::atomic_add_ptr_entry",
  "StubRoutine::fence_entry",
  "StubRoutine::d2i_wrapper",
  "StubRoutine::d2l_wrapper",
  "StubRoutine::addr_fpu_cntrl_wrd_std",
  "StubRoutine::addr_fpu_cntrl_wrd_24",
  "StubRoutine::addr_fpu_cntrl_wrd_64",
  "StubRoutine::addr_fpu_cntrl_wrd_trunc",
  "StubRoutine::addr_mxcsr_std",
  "StubRoutine::addr_fpu_subnormal_bias1",
  "StubRoutine::addr_fpu_subnormal_bias2",
  "StubRoutine::jbyte_arraycopy",
  "StubRoutine::jshort_arraycopy",
  "StubRoutine::jint_arraycopy",
  "StubRoutine::jlong_arraycopy",
  "StubRoutine::oop_arraycopy",
  "StubRoutine::oop_arraycopy_uninit",
  "StubRoutine::jbyte_disjoint_arraycopy",
  "StubRoutine::jshort_disjoint_arraycopy",
  "StubRoutine::jint_disjoint_arraycopy",
  "StubRoutine::jlong_disjoint_arraycopy",
  "StubRoutine::oop_disjoint_arraycopy",
  "StubRoutine::oop_disjoint_arraycopy_uninit",
  "StubRoutine::arrayof_jbyte_arraycopy",
  "StubRoutine::arrayof_jshort_arraycopy",
  "StubRoutine::arrayof_jint_arraycopy",
  "StubRoutine::arrayof_jlong_arraycopy",
  "StubRoutine::arrayof_oop_arraycopy",
  "StubRoutine::arrayof_oop_arraycopy_uninit",
  "StubRoutine::arrayof_jbyte_disjoint_arraycopy",
  "StubRoutine::arrayof_jshort_disjoint_arraycopy",
  "StubRoutine::arrayof_jint_disjoint_arraycopy",
  "StubRoutine::arrayof_jlong_disjoint_arraycopy",
  "StubRoutine::arrayof_oop_disjoint_arraycopy",
  "StubRoutine::arrayof_oop_disjoint_arraycopy_uninit",
  "StubRoutine::checkcast_arraycopy",
  "StubRoutine::checkcast_arraycopy_uninit",
  "StubRoutine::unsafe_arraycopy",
  "StubRoutine::generic_arraycopy",
  "StubRoutine::jbyte_fill",
  "StubRoutine::jshort_fill",
  "StubRoutine::jint_fill",
  "StubRoutine::arrayof_jbyte_fill",
  "StubRoutine::arrayof_jshort_fill",
  "StubRoutine::arrayof_jint_fill",
  "StubRoutine::zero_aligned_words",
  "StubRoutine::aescrypt_encryptBlock",
  "StubRoutine::aescrypt_decryptBlock",
  "StubRoutine::cipherBlockChaining_encryptAESCrypt",
  "StubRoutine::cipherBlockChaining_decryptAESCrypt",
  "StubRoutine::ghash_processBlocks",
  "StubRoutine::sha1_implCompress",
  "StubRoutine::sha1_implCompressMB",
  "StubRoutine::sha256_implCompress",
  "StubRoutine::sha256_implCompressMB",
  "StubRoutine::sha512_implCompress",
  "StubRoutine::sha512_implCompressMB",
  "StubRoutine::updateBytesCRC32",
#ifdef X86
  "StubRoutine::crc_table_addr",
#endif
  "StubRoutine::multiplyToLen",
  "StubRoutine::squareToLen",
  "StubRoutine::mulAdd",
#ifndef _WINDOWS
  "StubRoutine::montgomeryMultiply",
  "StubRoutine::montgomerySquare",
#endif
  "StubRoutine::utf16_to_utf8_decoder",
  "StubRoutine::utf8_to_utf16_decoder",
};

const char* CodeReviveVMGlobals::name(CR_GLOBAL_KIND k) {
  assert(k >= CR_GLOBAL_FIRST && k <= CR_GLOBAL_LAST, "invalid kind");
  return CR_SYMBOL_names[k];
}

CodeReviveVMGlobals::CodeReviveVMGlobals() {
  memset(_table, 0, sizeof(Entry) * (CR_GLOBAL_SIZE));
  // initialize OS
  add(CR_OS_polling_page, os::get_polling_page());
  add(CR_OS_javaTimeMillis, CAST_FROM_FN_PTR(address, os::javaTimeMillis));
  add(CR_OS_javaTimeNanos, CAST_FROM_FN_PTR(address, os::javaTimeNanos));
  add(CR_OS_break_point, CAST_FROM_FN_PTR(address, os::breakpoint));

  // initialize Heap
  add(CR_HEAP_card_table_base, (address)((CardTableModRefBS*)(Universe::heap()->barrier_set()))->byte_map_base);
  if (UseG1GC == false) {
    add(CR_HEAP_top_addr, (address)Universe::heap()->top_addr());
    add(CR_HEAP_end_addr, (address)Universe::heap()->end_addr());
  }

  // initialize universe
  add(CR_UNIVERSE_narrow_ptrs_base_addr, (address)Universe::narrow_ptrs_base_addr());

#ifndef ZERO
  // initialize shared runtime
  add(CR_SR_g1_wb_pre, (address)&SharedRuntime::g1_wb_pre);
  add(CR_SR_g1_wb_post, (address)&SharedRuntime::g1_wb_post);
  add(CR_SR_deopt_blob_unpack, SharedRuntime::deopt_blob()->unpack());
  add(CR_SR_ic_miss_blob, SharedRuntime::get_ic_miss_stub());
  add(CR_SR_uncommon_trap_blob, SharedRuntime::uncommon_trap_blob()->entry_point());
  add(CR_SR_complete_monitor_unlocking_C, (address)&SharedRuntime::complete_monitor_unlocking_C);
  add(CR_SR_dsin, (address)&SharedRuntime::dsin);
  add(CR_SR_dcos, (address)&SharedRuntime::dcos);
  add(CR_SR_dtan, (address)&SharedRuntime::dtan);
  add(CR_SR_dlog, (address)&SharedRuntime::dlog);
  add(CR_SR_dlog10, (address)&SharedRuntime::dlog10);
  add(CR_SR_dexp, (address)&SharedRuntime::dexp);
  add(CR_SR_dpow, (address)&SharedRuntime::dpow);
#ifdef AMD64
#ifndef _WINDOWS
  add(CR_SR_montgomery_multiply, (address)&SharedRuntime::montgomery_multiply);
  add(CR_SR_montgomery_square, (address)&SharedRuntime::montgomery_square);
#endif
#endif
#ifndef PRODUCT
  add(CR_SR_partial_subtype_ctr, (address)&SharedRuntime::_partial_subtype_ctr);
#endif
  // initialize stub routines
  // not all method is necessary!!
#ifdef X86
  size_t crc_table_size = 1024; // fixed size
  // other platform will compile fail
#endif
#ifdef AMD64
  add(CR_STUB_f2i_fixup, StubRoutines::x86::f2i_fixup());
  add(CR_STUB_f2l_fixup, StubRoutines::x86::f2l_fixup());
  add(CR_STUB_d2i_fixup, StubRoutines::x86::d2i_fixup());
  add(CR_STUB_d2l_fixup, StubRoutines::x86::d2l_fixup());
  add(CR_STUB_float_sign_mask, StubRoutines::x86::float_sign_mask());
  add(CR_STUB_float_sign_flip, StubRoutines::x86::float_sign_flip());
  add(CR_STUB_double_sign_mask, StubRoutines::x86::double_sign_mask());
  add(CR_STUB_double_sign_flip, StubRoutines::x86::double_sign_flip());
#endif
  add(CR_STUB_forward_exception_entry, StubRoutines::forward_exception_entry());
  add(CR_STUB_catch_exception_entry, StubRoutines::catch_exception_entry());
  add(CR_STUB_throw_AbstractMethodError_entry, StubRoutines::throw_AbstractMethodError_entry());
  add(CR_STUB_throw_IncompatibleClassChangeError_entry, StubRoutines::throw_IncompatibleClassChangeError_entry());
  add(CR_STUB_throw_NullPointerException_at_call_entry, StubRoutines::throw_NullPointerException_at_call_entry());
  add(CR_STUB_throw_StackOverflowError_entry, StubRoutines::throw_StackOverflowError_entry());
  add(CR_STUB_handler_for_unsafe_access_entry, StubRoutines::handler_for_unsafe_access());
  add(CR_STUB_atomic_xchg_entry, StubRoutines::atomic_xchg_entry());
  add(CR_STUB_atomic_xchg_ptr_entry, StubRoutines::atomic_xchg_ptr_entry());
  add(CR_STUB_atomic_store_entry, StubRoutines::atomic_store_entry());
  add(CR_STUB_atomic_store_ptr_entry, StubRoutines::atomic_store_ptr_entry());
  add(CR_STUB_atomic_cmpxchg_entry, StubRoutines::atomic_cmpxchg_entry());
  add(CR_STUB_atomic_cmpxchg_ptr_entry, StubRoutines::atomic_cmpxchg_ptr_entry());
  add(CR_STUB_atomic_cmpxchg_long_entry, StubRoutines::atomic_cmpxchg_long_entry());
  add(CR_STUB_atomic_add_entry, StubRoutines::atomic_add_entry());
  add(CR_STUB_atomic_add_ptr_entry, StubRoutines::atomic_add_ptr_entry());
  add(CR_STUB_fence_entry, StubRoutines::fence_entry());
  add(CR_STUB_d2i_wrapper, StubRoutines::d2i_wrapper());
  add(CR_STUB_d2l_wrapper, StubRoutines::d2l_wrapper());
  add(CR_STUB_addr_fpu_cntrl_wrd_std, StubRoutines::addr_fpu_cntrl_wrd_std());
  add(CR_STUB_addr_fpu_cntrl_wrd_24, StubRoutines::addr_fpu_cntrl_wrd_24());
  add(CR_STUB_addr_fpu_cntrl_wrd_64, StubRoutines::addr_fpu_cntrl_wrd_64());
  add(CR_STUB_addr_fpu_cntrl_wrd_trunc, StubRoutines::addr_fpu_cntrl_wrd_trunc());
  add(CR_STUB_addr_mxcsr_std, StubRoutines::addr_mxcsr_std());
  add(CR_STUB_addr_fpu_subnormal_bias1, StubRoutines::addr_fpu_subnormal_bias1());
  add(CR_STUB_addr_fpu_subnormal_bias2, StubRoutines::addr_fpu_subnormal_bias2());
  add(CR_STUB_jbyte_arraycopy, StubRoutines::jbyte_arraycopy());
  add(CR_STUB_jshort_arraycopy, StubRoutines::jshort_arraycopy());
  add(CR_STUB_jint_arraycopy, StubRoutines::jint_arraycopy());
  add(CR_STUB_jlong_arraycopy, StubRoutines::jlong_arraycopy());
  add(CR_STUB_oop_arraycopy, StubRoutines::oop_arraycopy(false));
  add(CR_STUB_oop_arraycopy_uninit, StubRoutines::oop_arraycopy(true));
  add(CR_STUB_jbyte_disjoint_arraycopy, StubRoutines::jbyte_disjoint_arraycopy());
  add(CR_STUB_jshort_disjoint_arraycopy, StubRoutines::jshort_disjoint_arraycopy());
  add(CR_STUB_jint_disjoint_arraycopy, StubRoutines::jint_disjoint_arraycopy());
  add(CR_STUB_jlong_disjoint_arraycopy, StubRoutines::jlong_disjoint_arraycopy());
  add(CR_STUB_oop_disjoint_arraycopy, StubRoutines::oop_disjoint_arraycopy(false));
  add(CR_STUB_oop_disjoint_arraycopy_uninit, StubRoutines::oop_disjoint_arraycopy(true));
  add(CR_STUB_arrayof_jbyte_arraycopy, StubRoutines::arrayof_jbyte_arraycopy());
  add(CR_STUB_arrayof_jshort_arraycopy, StubRoutines::arrayof_jshort_arraycopy());
  add(CR_STUB_arrayof_jint_arraycopy, StubRoutines::arrayof_jint_arraycopy());
  add(CR_STUB_arrayof_jlong_arraycopy, StubRoutines::arrayof_jlong_arraycopy());
  add(CR_STUB_arrayof_oop_arraycopy, StubRoutines::arrayof_oop_arraycopy(false));
  add(CR_STUB_arrayof_oop_arraycopy_uninit, StubRoutines::arrayof_oop_arraycopy(true));
  add(CR_STUB_arrayof_jbyte_disjoint_arraycopy, StubRoutines::arrayof_jbyte_disjoint_arraycopy());
  add(CR_STUB_arrayof_jshort_disjoint_arraycopy, StubRoutines::arrayof_jshort_disjoint_arraycopy());
  add(CR_STUB_arrayof_jint_disjoint_arraycopy, StubRoutines::arrayof_jint_disjoint_arraycopy());
  add(CR_STUB_arrayof_jlong_disjoint_arraycopy, StubRoutines::arrayof_jlong_disjoint_arraycopy());
  add(CR_STUB_arrayof_oop_disjoint_arraycopy, StubRoutines::arrayof_oop_disjoint_arraycopy(false));
  add(CR_STUB_arrayof_oop_disjoint_arraycopy_uninit, StubRoutines::arrayof_oop_disjoint_arraycopy(true));
  add(CR_STUB_checkcast_arraycopy, StubRoutines::checkcast_arraycopy(false));
  add(CR_STUB_checkcast_arraycopy_uninit, StubRoutines::checkcast_arraycopy(true));
  add(CR_STUB_unsafe_arraycopy, StubRoutines::unsafe_arraycopy());
  add(CR_STUB_generic_arraycopy, StubRoutines::generic_arraycopy());
  add(CR_STUB_jbyte_fill, StubRoutines::jbyte_fill());
  add(CR_STUB_jshort_fill, StubRoutines::jshort_fill());
  add(CR_STUB_jint_fill, StubRoutines::jint_fill());
  add(CR_STUB_arrayof_jbyte_fill, StubRoutines::arrayof_jbyte_fill());
  add(CR_STUB_arrayof_jshort_fill, StubRoutines::arrayof_jshort_fill());
  add(CR_STUB_arrayof_jint_fill, StubRoutines::arrayof_jint_fill());
  add(CR_STUB_zero_aligned_words, StubRoutines::zero_aligned_words());
  add(CR_STUB_aescrypt_encryptBlock, StubRoutines::aescrypt_encryptBlock());
  add(CR_STUB_aescrypt_decryptBlock, StubRoutines::aescrypt_decryptBlock());
  add(CR_STUB_cipherBlockChaining_encryptAESCrypt, StubRoutines::cipherBlockChaining_encryptAESCrypt());
  add(CR_STUB_cipherBlockChaining_decryptAESCrypt, StubRoutines::cipherBlockChaining_decryptAESCrypt());
  add(CR_STUB_ghash_processBlocks, StubRoutines::ghash_processBlocks());
  add(CR_STUB_sha1_implCompress, StubRoutines::sha1_implCompress());
  add(CR_STUB_sha1_implCompressMB, StubRoutines::sha1_implCompressMB());
  add(CR_STUB_sha256_implCompress, StubRoutines::sha256_implCompress());
  add(CR_STUB_sha256_implCompressMB, StubRoutines::sha256_implCompressMB());
  add(CR_STUB_sha512_implCompress, StubRoutines::sha512_implCompress());
  add(CR_STUB_sha512_implCompressMB, StubRoutines::sha512_implCompressMB());
  if (UseCRC32Intrinsics) {
    add(CR_STUB_updateBytesCRC32, StubRoutines::updateBytesCRC32());
#ifdef X86
    add(CR_STUB_crc_table_addr, StubRoutines::crc_table_addr(), (address)(((char*)StubRoutines::crc_table_addr()) + crc_table_size));
#endif
  }
  add(CR_STUB_multiplyToLen, StubRoutines::multiplyToLen());
  add(CR_STUB_squareToLen, StubRoutines::squareToLen());
  add(CR_STUB_mulAdd, StubRoutines::mulAdd());
#ifndef _WINDOWS
  add(CR_STUB_montgomeryMultiply, StubRoutines::montgomeryMultiply());
  add(CR_STUB_montgomerySquare, StubRoutines::montgomerySquare());
#endif
  add(CR_STUB_utf16_to_utf8_encoder, StubRoutines::utf16_to_utf8_encoder());
  add(CR_STUB_utf8_to_utf16_decoder, StubRoutines::utf8_to_utf16_decoder());
#endif
}

void CodeReviveVMGlobals::add_opto_runtime() {
#ifndef ZERO
  // initialize opt runtime
  add(CR_OPTO_exception_blob, OptoRuntime::exception_blob()->entry_point());
  add(CR_OPTO_new_instance_Java, OptoRuntime::new_instance_Java());
  add(CR_OPTO_new_array_Java, OptoRuntime::new_array_Java());
  add(CR_OPTO_new_array_nozero_Java, OptoRuntime::new_array_nozero_Java());
  add(CR_OPTO_multianewarray2_Java, OptoRuntime::multianewarray2_Java());
  add(CR_OPTO_multianewarray3_Java, OptoRuntime::multianewarray3_Java());
  add(CR_OPTO_multianewarray4_Java, OptoRuntime::multianewarray4_Java());
  add(CR_OPTO_multianewarray5_Java, OptoRuntime::multianewarray5_Java());
  add(CR_OPTO_multianewarrayN_Java, OptoRuntime::multianewarrayN_Java());
  add(CR_OPTO_g1_wb_pre_Java, OptoRuntime::g1_wb_pre_Java());
  add(CR_OPTO_g1_wb_post_Java, OptoRuntime::g1_wb_post_Java());
  add(CR_OPTO_complete_monitor_locking_Java, OptoRuntime::complete_monitor_locking_Java());
  add(CR_OPTO_rethrow_Java, OptoRuntime::rethrow_stub());
  add(CR_OPTO_slow_arraycopy_Java, OptoRuntime::slow_arraycopy_Java());
  add(CR_OPTO_register_finalizer_Java, OptoRuntime::register_finalizer_Java());
#ifdef ENABLE_ZAP_DEAD_LOCALS
  add(CR_OPTO_zap_dead_Java_locals_Java, OptoRuntime::zap_dead_locals_stub(false));
  add(CR_OPTO_zap_dead_native_locals_Java, OptoRuntime::zap_dead_locals_stub(true));
#endif
#endif
}

void CodeReviveVMGlobals::print() {
  for (size_t i = CR_GLOBAL_FIRST; i <= CR_GLOBAL_LAST; i++) {
    CodeRevive::out()->print_cr(SIZE_FORMAT " %s, [%p, %p)", i, CR_SYMBOL_names[i], _table[i]._addr, _table[i]._end);
  }
}

CR_GLOBAL_KIND CodeReviveVMGlobals::find(address addr, size_t* ofst) {
  // slow but only used when save
  for (size_t i = CR_GLOBAL_FIRST; i <= CR_GLOBAL_LAST; i++) {
    address start = _table[i]._addr;
    address end   = _table[i]._end;
    if (end == NULL) {
      end = start + 1;
    }
    if (addr >= start && addr < end) {
      *ofst = (size_t)(addr - start);
      return (CR_GLOBAL_KIND)i;
    }
  }
  *ofst = 0;
  return CR_GLOBAL_NONE;
}

address CodeReviveVMGlobals::find(CR_GLOBAL_KIND k, size_t ofst) {
  assert(k >= CR_GLOBAL_FIRST && k <= CR_GLOBAL_LAST, "invalid kind");
  address addr = _table[k]._addr;
  if (addr == NULL) {
    guarantee(false, "not regsitered yet");
    return NULL;
  }
  return addr + ofst;
}

void CodeReviveVMGlobals::add(CR_GLOBAL_KIND k, address addr, address end) {
  assert(k >= CR_GLOBAL_FIRST && k <= CR_GLOBAL_LAST, "invalid kind");
  //assert(addr != NULL, "invalid addr");
  assert(end == NULL || addr < end, "invalid end");
  assert(addr != NULL || end == NULL, "addr is NULL and ent not NULL");
  _table[k]._addr = addr;
  _table[k]._end = end;
}
