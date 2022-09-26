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
#ifndef SHARE_VM_CR_CODE_REVIVE_VERSION_SELECT_POLICY_HPP
#define SHARE_VM_CR_CODE_REVIVE_VERSION_SELECT_POLICY_HPP

#include "code/codeBlob.hpp"
#include "cr/revive.hpp"
#include "cr/codeReviveCodeBlob.hpp"

/*
 * Class ReviveVersionSelector: select a proper version of a certain method.
 *
 * The execution flow of it is:
 *   1. Iterate all code version stored in CSA file.
 *      For each version, two steps are needed before create its nmethod.
 *      (1) check opt records to see whether deopt may occurs.
 *      (2) prepare oop/meta array and relcoinfo.
 *
 *   2. check opt record is designed as a lightweight check, and it is suggested to be invoked first.
 *      For versions passed opt check, they need to prepare oop/meta array and relcoinfo.
 *
 *   3. If a version failed in opt check, it will be marked as unusable.
 *      if a version triggers de-opt, it will be also marked as unusable.
 *      If all version of a method are unusable, it will be set aot-disabled.
 *
 *
 * class ReviveVersionSelectPolicy: policy to select revived version
 *   If a JIT method has many versions, we need to select a better one.
 *   Its sub classes are used to select version will be revived.
 *
 *   class ReviveFirstUsablePolicy
 *     The first version passed opt check and preprocess will be selected.
 *
 *   class ReviveRandomUsablePolicy
 *     Randomly select a version from all versions passed opt check and preprocess.
 *
 *   classReviveAppointedUsablePolicy
 *     Try to select the "N"th version. "N" is an input argument.
 *     If the "N"th version is usable, then it will be selected.
 *     Or the first usable version after "N"th will be selected.
 *     This policy is usually used in debugging.
 *     Usage: revive_policy=appointed=N
 *
 * To write a new policy, two functions need to be implemented:
 *   1. check_next_version: whether go on checking next version.
 *   2. select_version: select target version from candidates.
 *
 * The control flow of these two functions is:
 *  while(has_next_version) {
 *    check opt records
 *    prepare oop/meta array and relocinfo
 *    ......
 *    if (!policy->check_next_version()) break;
 *  }
 *  policy->select_version(); 
 */

class ReviveVersionSelector : public StackObj {
 protected:
  char* _start;
  Method* _method;
  CodeReviveMetaSpace* _meta_space;
  CodeReviveCodeBlob* _cur_cb;
  CodeReviveCodeBlob::JitVersionReviveState** _selected_version;
  const char* _method_name;

  char* has_next_version(); // return the start of next jit code
  bool revive_and_opt_record_check(CodeReviveCodeBlob::JitVersionReviveState* revive_state);
  bool revive_preprocess(CodeReviveCodeBlob::JitVersionReviveState* revive_state);

 public:
  ReviveVersionSelector(char* start, Method* method, CodeReviveMetaSpace* meta_space, CodeReviveCodeBlob::JitVersionReviveState** selected_version);

  void do_selection();
};

class ReviveVersionSelectPolicy : public CHeapObj<mtInternal> {
 private:
  static ReviveVersionSelectPolicy* _policy;
  char* _arg;

 public:
  ReviveVersionSelectPolicy(char* arg) : _arg(arg) {}

  // whether to check next version.
  virtual bool check_next_version(CodeReviveCodeBlob::JitVersionReviveState* revive_state,
                                  int32_t version_id, int32_t collected_versions) = 0;
  // select a proper verison from versions.
  virtual int32_t select_version(GrowableArray<CodeReviveCodeBlob::JitVersionReviveState*>* versions) = 0;

  static ReviveVersionSelectPolicy* get_policy();
};

class ReviveFirstUsablePolicy : public ReviveVersionSelectPolicy {
 public:
  ReviveFirstUsablePolicy(char* arg) : ReviveVersionSelectPolicy(arg) {}

  virtual bool check_next_version(CodeReviveCodeBlob::JitVersionReviveState* revive_state,
                                  int32_t version_id, int32_t collected_versions) {
    guarantee(collected_versions <= 1, "should be");
    return collected_versions != 1;
  }

  virtual int32_t select_version(GrowableArray<CodeReviveCodeBlob::JitVersionReviveState*>* versions) {
    guarantee(versions->length() == 1, "should be");
    return 0;
  }
};

class ReviveRandomUsablePolicy : public ReviveVersionSelectPolicy {
 public:
  ReviveRandomUsablePolicy(char* arg) : ReviveVersionSelectPolicy(arg) {}

  virtual bool check_next_version(CodeReviveCodeBlob::JitVersionReviveState* revive_state,
                                  int32_t version_id, int32_t collected_versions) {
    return true;
  }

  virtual int32_t select_version(GrowableArray<CodeReviveCodeBlob::JitVersionReviveState*>* versions) {
    // init with current nanotime for real random value
    os::init_random((long)os::javaTimeNanos());
    return (int32_t)(os::random()  % versions->length());
  }
};

// Select version appointed in arg, or first usable.
class ReviveAppointedUsablePolicy : public ReviveVersionSelectPolicy {
 private:
  int32_t _appointed_ver;

 public:
  ReviveAppointedUsablePolicy(char* arg) : ReviveVersionSelectPolicy(arg) {
    if (arg != NULL) {
      _appointed_ver = atoi(arg);
    } else {
      _appointed_ver = 0;
    }
  }

  virtual bool check_next_version(CodeReviveCodeBlob::JitVersionReviveState* revive_state,
                                  int32_t version_id, int32_t collected_versions) {
    if (version_id < _appointed_ver) {
      return true;
    }
    return collected_versions < 1;
  }

  virtual int32_t select_version(GrowableArray<CodeReviveCodeBlob::JitVersionReviveState*>* versions) {
    for (int i = 0; i < versions->length(); i++) {
      if (versions->at(i)->_version == _appointed_ver) {
        return i;
      }
    }
    return 0;
  }
};

#endif
