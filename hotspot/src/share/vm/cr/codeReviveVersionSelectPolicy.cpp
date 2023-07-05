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
#include "cr/codeReviveVersionSelectPolicy.hpp"
#include "cr/codeReviveDependencies.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/codeReviveCodeSpace.hpp"
#include "cr/revive.hpp"

ReviveVersionSelectPolicy* ReviveVersionSelectPolicy::_policy = NULL;

ReviveVersionSelector::ReviveVersionSelector(char* start, Method* method, CodeReviveMetaSpace* meta_space, CodeReviveCodeBlob::JitVersionReviveState** selected_version, CodeReviveCodeBlob* code_blob) :
    _start(start), _method(method), _meta_space(meta_space), _cur_cb(code_blob), _selected_version(selected_version), _method_name(NULL) {
  guarantee(_start != NULL, "should be");
  guarantee(_meta_space != NULL, "should be");

  _method_name = method->name_and_sig_as_C_string();

  do_selection();
}

char* ReviveVersionSelector::has_next_version() {
  int next_cb_ofst = _cur_cb->next_version_offset();
  if (next_cb_ofst == -1) {
    return NULL;
  }
  return CodeRevive::current_code_space()->get_code_address(next_cb_ofst);
}

bool ReviveVersionSelector::revive_and_opt_record_check(CodeReviveCodeBlob::JitVersionReviveState* revive_state) {
  CodeReviveCodeBlob::ReviveResult check_result = _cur_cb->revive_and_opt_record_check(revive_state);

  if (check_result == CodeReviveCodeBlob::REVIVE_FAILED_ON_OPT_RECORD) {
    _method->set_aot_version_unusable(revive_state->_version);
    CR_LOG(cr_restore, cr_warning, "Version %d of method %s is set unusable for failed on opt check.\n", revive_state->_version, _method_name)
    return false;
  }

  bool ret = (check_result == CodeReviveCodeBlob::REVIVE_OK);
  if (ret) {
    revive_state->add_result(CodeReviveCodeBlob::JitVersionReviveState::check_passed);
  }
  CR_LOG(cr_restore, cr_info, "%s for %s v%d on opt record check\n", CodeReviveCodeBlob::revive_result_name(check_result), _method_name, revive_state->_version);
  return ret;
}

bool ReviveVersionSelector::revive_preprocess(CodeReviveCodeBlob::JitVersionReviveState* revive_state) {
  CodeReviveCodeBlob::ReviveResult process_result = _cur_cb->revive_preprocess(revive_state);
  bool ret = (process_result == CodeReviveCodeBlob::REVIVE_OK);
  if (ret) {
    revive_state->add_result(CodeReviveCodeBlob::JitVersionReviveState::preproccess_passed);
  }
  CR_LOG(cr_restore, cr_info, "%s for %s v%d on revive preprocess\n", CodeReviveCodeBlob::revive_result_name(process_result), _method_name, revive_state->_version);
  return ret;
}

void ReviveVersionSelector::do_selection() {
  ReviveVersionSelectPolicy* policy = ReviveVersionSelectPolicy::get_policy();

  GrowableArray<CodeReviveCodeBlob::JitVersionReviveState*>* checked_versions = new GrowableArray<CodeReviveCodeBlob::JitVersionReviveState*>();
  // Check opt_records and collect version.
  int32_t cur_version = 0;
  bool disable_aot = true;
  while(true) {
    CodeReviveCodeBlob::JitVersionReviveState* revive_state = NULL;
    _cur_cb->reset(_start, _meta_space);

    // candidate selection
    if (_method->is_aot_version_usable(cur_version)) {
      revive_state = new CodeReviveCodeBlob::JitVersionReviveState(_method, _start, cur_version);

      if (revive_and_opt_record_check(revive_state) && revive_preprocess(revive_state)) {
        CR_LOG(cr_restore, cr_info, "Candidate Version %d of method %s\n", cur_version, _method_name);
        checked_versions->append(revive_state);
      }
    } else {
      CR_LOG(cr_restore, cr_info, "Unusable version %d of method %s\n", cur_version, _method_name)
    }
    disable_aot &= !_method->is_aot_version_usable(cur_version);

    _start = has_next_version();
    if (((revive_state != NULL) && !policy->check_next_version(revive_state, cur_version, checked_versions->length())) || _start == NULL) {
      if (_start != NULL) {
        // don't check all versions, assume has usable versions.
        disable_aot = false;
      }
      break;
    }
    cur_version++;
  }

  if (disable_aot) {
    CR_LOG(cr_restore, cr_fail, "All versions are unusable, AOT for method %s is disabled.\n", _method_name);
    _method->set_aot_disabled();
    return;
  }

  if (checked_versions->length() == 0) {
    CR_LOG(cr_restore, cr_warning, "No version found after version_collect_check for %s.\n", _method_name);
    return;
  }

  // select version from _checked versions;
  int32_t version_index = policy->select_version(checked_versions);
  guarantee(-1 <= version_index && version_index < checked_versions->length(), "Invalid version_index");
  if (version_index == -1) {
    CR_LOG(cr_restore, cr_warning, "No version selected for method %s.\n", _method_name);
    return;
  }
  CodeReviveCodeBlob::JitVersionReviveState* selected_version = checked_versions->at(version_index);
  guarantee(selected_version->is_valid(), "Either revive_and_opt_record_check or revive_preprocess not passed.");

  CR_LOG(cr_restore, cr_trace, "Select version %d for method %s.\n", selected_version->_version, _method_name);
  *_selected_version = selected_version;
}

ReviveVersionSelectPolicy* ReviveVersionSelectPolicy::get_policy() {
  if (_policy == NULL) {
    switch(CodeRevive::revive_poicy()) {
      case CodeRevive::REVIVE_POLICY_FIRST: {
        CR_LOG(cr_restore, cr_trace, "Select first-policy for reviving\n")
        _policy = new ReviveFirstUsablePolicy(CodeRevive::revive_policy_arg());
        break;
      }
      case CodeRevive::REVIVE_POLICY_RANDOM: {
        CR_LOG(cr_restore, cr_trace, "Select random-policy for reviving\n")
        _policy = new ReviveRandomUsablePolicy(CodeRevive::revive_policy_arg());
        break;
      }
      case CodeRevive::REVIVE_POLICY_APPOINT: {
        CR_LOG(cr_restore, cr_trace, "Select appoint-policy(%d) for reviving\n", CodeRevive::revive_policy_arg());
        _policy = new ReviveAppointedUsablePolicy(CodeRevive::revive_policy_arg());
        break;
      }
      default:
        guarantee(false, "Unknown revive_policy");
    }
  }
  return _policy;
}
