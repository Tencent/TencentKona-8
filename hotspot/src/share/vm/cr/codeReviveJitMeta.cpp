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

#include "precompiled.hpp"
#include "cr/codeReviveMerge.hpp"
#include "cr/codeReviveJitMeta.hpp"
#include "cr/codeReviveOptRecords.hpp"
#include "cr/revive.hpp"

void JitMetaInfo::add(GrowableArray<OptRecord*>* opts, GrowableArray<ReviveDepRecord*>* deps,
                      CandidateCodeBlob candi, Arena* arena) {
  _count++;
  bool found = false;
  for (int i = 0; i < _versions->length(); i++) {
    if (_versions->at(i)->equal(opts, deps)) {
      // the jit version has recorded before
      _versions->at(i)->inc();
      found = true;
      break;
    }
  }
  if (!found) {
    JitVersion* jv = new (arena) JitVersion(opts, deps, candi, _versions->length(), arena);
    _versions->append(jv);
  }
}

void JitMetaInfo::sort() {
  // naiive qsort
  _versions->sort(JitVersion::compare_by_count);
}

/*
 * Select the best jit version during merge.
 * Currently only select frequent jit versions.
 */
void JitMetaInfo::select_best_versions() {
  if (CodeRevive::merge_policy() == CodeRevive::M_COVERAGE) {
    JitMetaInfo::CoverageBasedPolicy policy(this);
    policy.select();
  } else {
    JitMetaInfo::SimpleSelectPolicy policy(this);
    policy.select();
  }
}

void JitMetaInfo::SimpleSelectPolicy::select() {
  _jmi->sort();
  for (int i = _jmi->_versions->length() - 1; i >= CodeRevive::max_nmethod_versions(); i--) {
    _jmi->_versions->pop();
  }
}

void JitMetaInfo::CoverageBasedPolicy::select() {
  _jmi->combine_versions();
  JitMetaInfo::SimpleSelectPolicy::select();
}

/*
 * Simple combination: just do version combination if the version number is bigger than Max_Version.
 *   When version A cover version B, just delete version B and add counter of B to A.
 */
void JitMetaInfo::combine_versions() {
  ResourceMark rm;
  sort();

  CR_LOG(cr_merge, cr_info, "Start version merge for method %s\n", CodeReviveMerge::metadata_name(_method_index));
  for (int i = _versions->length() - 1; i > 0; i--) {
    if (_versions->length() <= CodeRevive::max_nmethod_versions()) {
      break;
    }
    JitVersion* cur = _versions->at(i);
    for (int j = 0; j < i; j++) {
      JitVersion* prev = _versions->at(j);

      // if prev version can cover current version,
      // just combine current version and delete it
      if (prev->can_cover(_method_index, cur)) {
        prev->combine(cur);
        _versions->delete_at(i);
        break;
      }

      // if current version can cover prev version,
      // current version should combine prev version and put at prev position.
      if (cur->can_cover(_method_index, prev)) {
        cur->combine(prev);
        _versions->at_put(j, cur);
        _versions->delete_at(i);
        break;
      }
    }
  }
}

void JitVersion::combine(JitVersion* other) {
  _count += other->_count;
  CR_LOG(cr_merge, cr_info, "  Version id=%d cover and merge version id=%d\n", _uniq_id, other->_uniq_id);
}

/*
 * If the opt records in cur version are a subset of the other version,
 * then cur version can cover the other version.
 *   eg. empty optimization always cover versions with optimization.
 */
bool JitVersion::can_cover(int32_t method_index, JitVersion* other) {
  GrowableArray<int32_t>* visited = new GrowableArray<int32_t>();

  while (method_index != -1) {
    visited->append(method_index);
    GrowableArray<OptRecord*>* cur_opt = get_opt_records_by_method(method_index);
    if (cur_opt->length() != 0) {
      GrowableArray<OptRecord*>* other_opt = other->get_opt_records_by_method(method_index);
      if (other_opt->length() == 0) {
        return false;
      }
      for (int i = 0; i < cur_opt->length(); i++) {
        bool found = false;
        for (int j = 0; j < other_opt->length(); j++) {
          if (other_opt->at(j)->equal(cur_opt->at(i))) {
            found = true;
            break;
          }
        }
        if (!found) {
          return false;
        }
      }
    }
    method_index = get_next_inlined_method(visited);
  }

  return true;
}

/*
 * Get opt records inserted by the bytecode of current method
 */
GrowableArray<OptRecord*>* JitVersion::get_opt_records_by_method(int32_t method_index) {
  GrowableArray<OptRecord*>* result = new GrowableArray<OptRecord*>();
  for (int i = 0; i < _opts->length(); i++) {
    if (_opts->at(i)->belongs_to_method(method_index)) {
      result->append(_opts->at(i));
    }
  }
  return result;
}

int32_t JitVersion::get_next_inlined_method(GrowableArray<int32_t>* visited) {
  for (int i = 0; i < _opts->length(); i++) {
    if (_opts->at(i)->opt_type() == OptRecord::Inline) {
      int m = _opts->at(i)->method_index();
      if (!visited->contains(m)) {
        return m;
      }
    }
  }
  return -1;
}

// jit version with bigger count will appear first
int JitVersion::compare_by_count(JitVersion** v1, JitVersion** v2) {
  return  (*v2)->_count - (*v1)->_count;
}

bool JitVersion::equal(GrowableArray<OptRecord*>* opts, GrowableArray<ReviveDepRecord*>* deps) {
  if (_opts->length() != opts->length() || _deps->length() != deps->length()) {
    return false;
  }
  for (int i = 0; i < _opts->length(); i++) {
    if (!_opts->at(i)->equal(opts->at(i))) {
      return false;
    }
  }
  for (int i = 0; i < _deps->length(); i++) {
    if (_deps->at(i)->compare_by_type_name_or_index(deps->at(i)) != 0) {
      return false;
    }
  }
  return true;
}

void JitVersion::inc() {
  _count++;
}

JitVersion::JitVersion(GrowableArray<OptRecord*>* opts, GrowableArray<ReviveDepRecord*>* deps, CandidateCodeBlob c, int32_t id, Arena* arena) {
  _count = 1;
  _candidate = c;
  _uniq_id = id;
  _opts = CodeReviveMerge::duplicate_opt_record_array_in_arena(opts, arena);
  _deps = CodeReviveMerge::duplicate_dep_record_array_in_arena(deps, arena);
}
