/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef SHARE_VM_CR_CODE_REVIVE_OPT_RECORDS_HPP
#define SHARE_VM_CR_CODE_REVIVE_OPT_RECORDS_HPP

/*
 * records dynamic optimizations performed in JIT compilation
 * 1. profile guarded devirtualization:
 * 2. profile guarded inlining: success or fail
 * 3. profile based uncommon trap/deoptimize
 *
 * Generating and processsing steps
 * 1. JIT compilation generates OptRecord and add into CodeReviveOptRecords
 *    at compile time.
 *    1.1 remove duplicated decision
 *    1.2 resovle conflict decision? possible? (inline in different InlineTree)
 *    1.3 add related metadata into metadata array
 * 2. nmethod installation
 *    encoded with oop/metadata index
 *
 * Usage
 * CodeRevive optimzation validation check
 * 1. Check if saved nmethod will have deopt
 *    1. callee executes uncommon trap branch
 *    2. callee profile shows different target
 * 2. Detect if version is optimal for current profiling
 *    1. Based on profile factor and inline/devirtual decision
 */
#include "utilities/growableArray.hpp"

class ciEnv;

/*
 * OptRecord records optimization information and its related metadata index
 * Record is used in three process:
 * 1. JIT compile: index point into OopRecorder; create in optimization
 * 2. nmethod install: index point into nmethod's metadata, create from encoded binary
 * 3. revive check: index point into oop_meta_array(meta at front part); create from encoded binary
 *
 * OopRecorder Metadata* metadata_at(int index);
 * nmethod Metadata*     metadata_at(int index) const;
 *
 * Context information need record when create, otherwise need create thread local
 */
class OptRecord : public ResourceObj {
 public:
  enum CtxType {
    COMPILE = 0,
    NM_INSTALL,
    REVIVE,
    DUMP,
    MERGE
  };
  enum OptType {
    DeVirtual = 0,
    Inline,
    ProfiledReceiver,
    ProfiledUnstableIf,
    END_OPT
  };
 protected:
  OptType _opt_type;
  CtxType _ctx_type;
  void*   _ctx; // oopRecorder, nm, meta array for different context
  int32_t _method_idx;
  int32_t _bci;
 protected:
  Metadata*   get_meta(int idx);
  char*       get_meta_name(int idx);
  ciMetadata* get_ci_meta(int idx);
  int         compare_meta_name(int idx1, int idx2);
 public:
  OptRecord(OptType opt_type, CtxType ctx_type, void* ctx, int32_t method_idx, int32_t bci) :
    _opt_type(opt_type), _ctx_type(ctx_type), _ctx(ctx), _method_idx(method_idx), _bci(bci) {}
  OptType opt_type() const { return _opt_type; }
  int32_t method_index() { return _method_idx; }
  bool belongs_to_method(int32_t mi) { return mi == _method_idx; }
  virtual size_t estimate_size_in_bytes() = 0;
  virtual void write_to_stream(CompressedWriteStream* out) = 0;
  virtual void read_from_stream(CompressedReadStream* in) = 0;
  virtual int  calc_opt_score() = 0;
  virtual bool equal(OptRecord* other) = 0;
  virtual int  compare_by_type_name(OptRecord* other);
  virtual void print_on(outputStream* out, int indent=0) = 0;
  virtual void nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest);
  virtual OptRecord* duplicate_in_arena(Arena* arena) = 0;
};

/*
 * profile guarded devirtual optimization
 * _morphism = 0,1,2
 *   1 monomorphic or _klass2 is not devirtualized: assert _kass2 == NULL
 *   2 bimorphic and both receiver is devirtualized: assert _klass2 != NULL
 *   0 polymorphic or bimorphic, _klass1 is major: assert _klass2 != NULL
 */
class OptRecordDeVirtual : public OptRecord {
 private:
  int32_t _klass1_idx;
  int32_t _klass2_idx;
  int32_t _morphism;
  bool    _miss_is_trap;
 public:
  OptRecordDeVirtual(CtxType ctx_type, void* ctx) : OptRecord(DeVirtual, ctx_type, ctx, -1, -1),
    _klass1_idx(-1), _klass2_idx(-1), _morphism(-1), _miss_is_trap(false) {}
  OptRecordDeVirtual(CtxType ctx_type, void* ctx, int32_t ci, int32_t k1, int32_t k2, int bci, int morphism, bool miss_is_trap) :
    OptRecord(DeVirtual, ctx_type, ctx, ci, bci),
    _klass1_idx(k1),
    _klass2_idx(k2),
    _morphism(morphism),
    _miss_is_trap(miss_is_trap) {}

  virtual size_t estimate_size_in_bytes();
  virtual void write_to_stream(CompressedWriteStream* out);
  virtual void read_from_stream(CompressedReadStream* in);
  virtual int  calc_opt_score();
  virtual bool equal(OptRecord* other);
  virtual int  compare_by_type_name(OptRecord* other);
  virtual void print_on(outputStream* out, int indent=0);
  virtual void nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest);
  virtual OptRecord* duplicate_in_arena(Arena* arena) {
    return new (arena) OptRecordDeVirtual(_ctx_type, _ctx, _method_idx, _klass1_idx, _klass2_idx, _bci, _morphism, _miss_is_trap);
  }
};

class OptRecordInline : public OptRecord {
 private:
  int32_t _callee_idx;
 public:
  OptRecordInline(CtxType ctx_type, void* ctx) : OptRecord(Inline, ctx_type, ctx, -1, -1),
    _callee_idx(-1) {}
  OptRecordInline(CtxType ctx_type, void* ctx, int32_t caller_idx, int32_t callee_idx, int32_t bci) :
    OptRecord(Inline, ctx_type, ctx, caller_idx, bci),
    _callee_idx(callee_idx) {}
  virtual size_t estimate_size_in_bytes();
  virtual void write_to_stream(CompressedWriteStream* out);
  virtual void read_from_stream(CompressedReadStream* in);
  virtual int  calc_opt_score();
  virtual bool equal(OptRecord* other);
  virtual int  compare_by_type_name(OptRecord* other);
  virtual void print_on(outputStream* out, int indent=0);
  virtual void nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest);
  virtual OptRecord* duplicate_in_arena(Arena* arena) {
    return new (arena) OptRecordInline(_ctx_type, _ctx, _method_idx, _callee_idx, _bci);
  }
};

class OptProfiledReceiver : public OptRecord {
 private:
  int32_t _klass_idx;
 public:
  OptProfiledReceiver(CtxType ctx_type, void* ctx) : OptRecord(ProfiledReceiver, ctx_type, ctx, -1, -1),
    _klass_idx(-1) {}
  OptProfiledReceiver(CtxType ctx_type, void* ctx, int mi, int bci, int ki) :
    OptRecord(ProfiledReceiver, ctx_type, ctx, mi, bci), _klass_idx(ki) {}
  virtual size_t estimate_size_in_bytes();
  virtual void write_to_stream(CompressedWriteStream* out);
  virtual void read_from_stream(CompressedReadStream* in);
  virtual int  calc_opt_score();
  virtual bool equal(OptRecord* other);
  virtual void print_on(outputStream* out, int indent=0);
  virtual void nm_meta_index_to_meta_space_index(GrowableArray<int32_t>* dest);
  virtual OptRecord* duplicate_in_arena(Arena* arena) {
    return new (arena) OptProfiledReceiver(_ctx_type, _ctx, _method_idx, _bci, _klass_idx);
  }
};

class OptProfiledUnstableIf : public OptRecord {
 private:
  bool    _taken_branch_trap;
 public:
  OptProfiledUnstableIf(CtxType ctx_type, void* ctx) : OptRecord(ProfiledUnstableIf, ctx_type, ctx, -1, -1),
    _taken_branch_trap(false) {}
  OptProfiledUnstableIf(CtxType ctx_type, void* ctx, int mi, int bci, bool taken) :
    OptRecord(ProfiledUnstableIf, ctx_type, ctx, mi, bci), _taken_branch_trap(taken) {}
  virtual size_t estimate_size_in_bytes();
  virtual void write_to_stream(CompressedWriteStream* out);
  virtual void read_from_stream(CompressedReadStream* in);
  virtual int  calc_opt_score();
  virtual bool equal(OptRecord* other);
  virtual void print_on(outputStream* out, int indent=0);
  virtual OptRecord* duplicate_in_arena(Arena* arena) {
    return new (arena) OptProfiledUnstableIf(_ctx_type, _ctx, _method_idx, _bci, _taken_branch_trap);
  }
};

class CodeReviveOptRecords : public ResourceObj {
 private:
  Arena*                     _arena;
  OopRecorder*               _oop_recorder;
  GrowableArray<OptRecord*>* _opts;
  address                    _content_bytes;
  size_t                     _size_in_bytes;

  void insert(OptRecord* r);
  size_t estimate_size_in_bytes();
 public:
  CodeReviveOptRecords(ciEnv* env);
  void add_DeVirtualRecord(ciMethod* caller, ciKlass* klass1, ciKlass* klass2, int bci, int morphism, bool miss_is_trap);
  void add_InlineRecord(ciMethod* caller, ciMethod* callee, int bci);
  void add_ProfiledReceiverRecord(ciMethod* method, int bci, ciKlass* klass);
  void add_ProfiledUnstableIf(ciMethod* method, int bci, bool taken);
  void encode_content_bytes();
  void copy_to(nmethod* nm);
  address content_bytes() {
    return _content_bytes;
  }
  size_t size_in_bytes() {
    return _size_in_bytes;
  }
};

class OptStream {
 private:
  OptRecord::CtxType   _ctx_type;
  void*                _ctx; // oopRecorder, nm, meta array for different context
  CompressedReadStream _bytes;
 public:
  OptStream(OptRecord::CtxType ctx_type, void* ctx, u_char* input) :
    _ctx_type(ctx_type), _ctx(ctx), _bytes(input) {}
  OptRecord* next();
};
#endif // SHARE_VM_CR_CODE_REVIVE_OPT_RECORDS_HPP
