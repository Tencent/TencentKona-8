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
#ifndef SHARE_VM_CR_CODE_REVIVE_JIT_META_HPP
#define SHARE_VM_CR_CODE_REVIVE_JIT_META_HPP
#include "cr/codeReviveDependencies.hpp"
#include "cr/codeReviveMerge.hpp"

class CodeReviveMerge;
class OptRecord;

/*
 * class JitMetaInfo: used to record different JIT optimization information.
 *   Each JIT method has one corresponding JitMetaInfo object.
 *
 * class JitVersion: single JIT method optimization infomation.
 *   Each JitVersion object represents a new version of a JIT method.
 *   Currently it contains two different optimization records:
 *     1. Opt Record
 *     2. Dependency Record
 *   For two saved JIT methods, only and if only their OptRecords and Dependency Records
 *     are all the same, they are treated as the same version.
 *
 * The two classes are organized as following:
 *   JitMetaInfo1 ------ JitVersion1
 *                |----- JitVersion2
 *                |----- JitVersion3
 *                |----- ......
 *   JitMetaInfo2 ------ JitVersion1
 *                |----- ......
 *   ......
 *
 *
 * class JitSelectPolicy: policies to select target versions.
 *   In the merged CSA file, each JIT method has CodeRevive::max_nmethod_versions() versions at most.
 *   Its sub classes are used to select target versions.
 *
 *   class SimpleSelectPolicy: simple policy.
 *     The versions with higher occurrence count will be kept, and othres are dropped.
 *
 *   class CoverageBasedPolicy: try to combine versions when one version can cover another.
 *     Definition of cover: for version A and B, if replace B with A and no more deopt is generated,
 *       we say that A cover B.
 *     This policy checks all versions and tries to combine version with smaller occurrence count to bigger ones.
 *     Until all versions are checked or the number of versions is less than CodeRevive::max_nmethod_versions().
 */

class JitVersion : public ResourceObj {
 friend class CodeReviveMerge;
 private:
  // Opt Record in the jit code
  GrowableArray<OptRecord*>* _opts;
  // Dep Record in the jit code
  GrowableArray<ReviveDepRecord*>* _deps;
  // total count of current jit version
  int32_t _count;
  // infomation about current jit version, mainly file path and offset
  CandidateCodeBlob _candidate;
  int32_t _uniq_id;

 public:
  JitVersion(GrowableArray<OptRecord*>* opt, GrowableArray<ReviveDepRecord*>* deps, CandidateCodeBlob c, int32_t id)
    : _opts(opt), _deps(deps), _count(1), _candidate(c), _uniq_id(id) {}
  JitVersion(GrowableArray<OptRecord*>* opt, GrowableArray<ReviveDepRecord*>* deps, CandidateCodeBlob c, int32_t id, Arena* arena);
  bool equal(GrowableArray<OptRecord*>* opts, GrowableArray<ReviveDepRecord*>* deps);
  void inc();
  static int compare_by_count(JitVersion** v1, JitVersion** v2);
  bool can_cover(int32_t method_index, JitVersion* other);
  void combine(JitVersion* other);
  GrowableArray<OptRecord*>* get_opt_records_by_method(int32_t method_index);
  int32_t get_next_inlined_method(GrowableArray<int32_t>* visited);
};

class JitMetaInfo : public ResourceObj {
 friend class CodeReviveMerge;
 public:
  class JitSelectPolicy : public StackObj {
   protected:
    JitMetaInfo* _jmi;
   public:
    JitSelectPolicy(JitMetaInfo* jmi) : _jmi(jmi) {}
    virtual void select() = 0;
  };

  class SimpleSelectPolicy : public JitSelectPolicy {
   public:
    SimpleSelectPolicy(JitMetaInfo* jmi) : JitSelectPolicy(jmi) {}
    virtual void select();
  };

  class CoverageBasedPolicy : public SimpleSelectPolicy {
   public:
    CoverageBasedPolicy(JitMetaInfo* jmi) : SimpleSelectPolicy(jmi) {}
    virtual void select();
  };

 private:
  int32_t _method_index;
  // different jit versions in archive file
  GrowableArray<JitVersion*>* _versions;
  // total count appeared in different archive file
  int32_t _count;
  void sort();
  void combine_versions();
 public:
  JitMetaInfo(int32_t m, Arena* arena) : _method_index(m), _count(0)
             { _versions = new (arena) GrowableArray<JitVersion*>(arena, 10, 0, NULL); }
  void add(GrowableArray<OptRecord*>* opts, GrowableArray<ReviveDepRecord*>* deps, CandidateCodeBlob candi, Arena* arena);
  void select_best_versions();
  int32_t count() { return _count; }
  int32_t version_number() { return _versions->length(); }
};

#endif // SHARE_VM_CR_CODE_REVIVE_JIT_META_HPP
