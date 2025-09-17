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

#ifndef SHARE_VM_CR_REVIVE_HPP
#define SHARE_VM_CR_REVIVE_HPP

#include "runtime/timer.hpp"

class CodeReviveFile;
class CodeReviveVMGlobals;
class CodeReviveMetaSpace;
class CodeReviveCodeSpace;

class Method;
class ciEnv;

class FileMapInfo;
 
const uint32_t cr_archive     = 0; // log contents of CSA file in save or restore
const uint32_t cr_global      = 1; // log VM global symbols in save
const uint32_t cr_assembly    = 2; // print the native codes of saved methods
const uint32_t cr_opt         = 3; // log opt records during compilation
const uint32_t cr_save        = 4; // log during save
const uint32_t cr_restore     = 5; // log during restore
const uint32_t cr_merge       = 6; // log during merge
const uint32_t cr_number      = 7;

const uint8_t cr_none         = 0; // events should be logged always
const uint8_t cr_fail         = 1; // events those may cause code revive fail.
const uint8_t cr_warning      = 2; // events those may reduce the effect of code revive.
const uint8_t cr_trace        = 3; // key events during code revive process.
const uint8_t cr_info         = 4; // all events during code revive process.

/* Usage of log kind and log level:
 *   archive:
 *     - trace  : contents of CSA files
 *   global:
 *     - trace  : VM global symbols
 *   assembly:
 *     - trace  : native codes of saved methods
 *   opt:
 *     - trace  : opt records during compilation
 *   save:
 *     - info   : information of saved contents : oop, metadata, symbols, etc.
 *     - trace  : key steps of saving, all methods those saved successfully.
 *     - warning: events cause some methods can't be successfully saved.
 *     - fail   : why save CSA file failed, unsaved methods.
 *     - none   : save CSA file failed.
 *   restore:
 *     - info   : information of restored contents : oop, metadata, symbols, etc.
 *     - trace  : key steps of restoring, all methods those restored successfully.
 *     - warning: prepare oop/metadata or others failed during reviving, some version unusable.
 *     - fail   : why save CSA file failed, some saved method is completely unusable.
 *     - none   : load CSA file failed.
 *   merge:
 *     - info   : information of merge process.
 *     - trace  : key steps of merging.
 *     - warning: fail on merging some methods.
 *     - fail   : why merge process failed.
 *     - none   : no compatible CSA file or valid class path found.
 */

enum LoaderType {
  boot_loader          = 0x00,
  ext_loader           = 0x01,
  app_loader           = 0x02,
  custom_loader        = 0x04, // oop/metadata loaded by custom classloader.
  method_holder_loader = 0x08, // used in data array, marked as method holder's classloader.
};

class CodeRevive : AllStatic {
 public:
  enum {
    T_LOOKUP_METHOD,      // the time for lookup method
    T_RESTORE,            // total time to restore the method successfully
    T_RESTORE_FAIL,       // total time to restore the method fail
    T_SELECT,             // total time to select the method
    T_LIGHTWEIGHT_CHECK,  //   total time to check lightweight
    T_PREPROC,            //   total time to preprocess the code blob
    T_CREATE,             // total time to call create_nmethod
    T_REVIVE,             //   total time to revive the method in create_nmethod
    T_TOTAL_TIMERS,
  } Timers;

  enum {
    S_HEADER,
    S_CLASSPATH_TABLE,
    S_LOOKUP_TABLE,
    S_CB_HEADER,
    S_CB_BODY,
    S_AUX_INFO,
    S_OOPMAP_SET,
    S_TOTAL_COUNTERS,
  } SizeCounter;

  enum RevivePolicy {
    REVIVE_POLICY_FIRST,
    REVIVE_POLICY_RANDOM,
    REVIVE_POLICY_APPOINT,
    REVIVE_POLICY_NUM,
  };

  enum MergePolicy {
    M_SIMPLE,
    M_COVERAGE,
  };

  enum {
    REVIVE_SUCCESS,
    REVIVE_FAIL,
    REVIVE_NOT_IN_CSA,
    REVIVE_FOUND_WITH_NAME,
    REVIVE_TOTAL_STATUS,
  } ReviveStatus;

 private:
  static CodeReviveVMGlobals* _vm_globals;
  static CodeReviveFile*  _load_file;
  static bool             _should_disable;
  static bool             _is_save;
  static bool             _is_restore;
  static bool             _is_merge;
  static bool             _print_opt;
  static bool             _fatal_on_fail;
  static bool             _perf_enable;
  static bool             _validate_check;
  static bool             _prepare_done;
  static bool             _disable_constant_opt;
  static bool             _verify_redefined_identity;   // Test option: verify identity when creating nmethod
  static bool             _make_revive_fail_at_nmethod; // Test option: make revive failed when creating nmethod
  static char*            _file_path;
  static char*            _log_file_path;
  static char*            _input_files;
  static char*            _input_list_file;
  static int32_t          _percent;            // save/restore under this percent, range [1:99]
  static int32_t          _coverage;     // the coverage for merge csa, range [1:99]
  static int32_t          _max_container_count;
  static int32_t          _max_nmethod_versions;
  static volatile int32_t _redefine_epoch; // LSB: 0 invalid, 1 valid; Bit 1~62: epoch; Bit 63: unused.
  static uint32_t         _log_kind;
  static uint64_t         _max_file_size;
  static MergePolicy      _merge_policy;
  static outputStream*    _log_file;
  static uint8_t          _log_ctrls[cr_number];
  static elapsedTimer     _t_aot_timers[T_TOTAL_TIMERS];
  static int              _revive_counter[REVIVE_TOTAL_STATUS]; 
  static uint             _aot_size_counters[S_TOTAL_COUNTERS];
  static uint             _total_file_size;
  static RevivePolicy     _revive_policy;
  static char*            _revive_policy_arg;
  static const char*      _revive_policy_name[REVIVE_POLICY_NUM];
  static int64_t          _cds_identity;   // the identity for the CDS archive file

  static bool parse_options();
  static double get_ms_time(int timer) {
    return (double)get_aot_timer(timer)->ticks() / os::elapsed_frequency() * (double)1000.0;
  }

 public:
  static void early_init();
  static bool is_save() {
    return _is_save;
  }
  static bool is_restore() {
    return _is_restore;
  }
  static bool is_merge() {
    return _is_merge;
  }
  static bool print_opt() {
    return _print_opt;
  }
  static bool is_on() {
    return _is_save || _is_restore;
  }
  static bool is_fatal_on_fail() {
    return _fatal_on_fail;
  }
  static bool is_log_on(uint32_t kind, uint8_t level) {
    return _log_ctrls[kind] >= level;
  }

  static uint32_t log_kind() {
    return _log_kind;
  }

  static void log(const char* format, ...);
  static outputStream* out() {
    return _log_file;
  }
  static char* file_path() {
    return _file_path;
  }
  static char* input_files() {
    return _input_files;
  }
  static char* input_list_file() {
    return _input_list_file;
  }
  static void set_merge_policy(MergePolicy p) {
    _merge_policy = p;
  }
  static MergePolicy merge_policy() {
    return _merge_policy;
  }
  static bool disable_constant_opt() {
    return _disable_constant_opt;
  }
  static void set_should_disable() {
    _should_disable = true;
  }
  static bool should_disable() {
    return _should_disable;
  }
  static bool is_revive_candidate(Method* target, int task_level);
  static void disable();
  static bool is_disabled();
  static void parse_option_file();
  static CodeReviveVMGlobals* vm_globals() { return _vm_globals; }
  static char* find_revive_code(Method* m, bool only_use_name = false);
  static CodeReviveMetaSpace* current_meta_space();
  static CodeReviveCodeSpace* current_code_space();
  static void on_vm_start();
  static void on_vm_shutdown();
  static elapsedTimer* get_aot_timer(uint timer) {
    assert(timer < T_TOTAL_TIMERS, "should be");
    return &_t_aot_timers[timer];
  }
  static void add_size_counter(uint counter, uint size) {
    assert(counter < S_TOTAL_COUNTERS, "should be");
    _aot_size_counters[counter] += size;
  }
  static uint get_size_counter(uint counter) {
    assert(counter < S_TOTAL_COUNTERS, "should be");
    return _aot_size_counters[counter];
  }
  static double get_size_percent(uint counter) {
    return (double)get_size_counter(counter) * 100.0 / _total_file_size;
  }
  static void total_file_size(uint total_size) {
    _total_file_size = total_size;
  }
  static uint get_file_size() {
    return _total_file_size;
  }
  static bool perf_enable() {
    return _perf_enable;
  }
  static bool validate_check() {
    return _validate_check;
  }
  static bool prepare_done() {
    return _prepare_done;
  }
  static void set_prepare_done() {
    _prepare_done = true;
  }
  static int32_t max_container_count() {
    return _max_container_count;
  }
  static int32_t max_nmethod_versions() {
    return _max_nmethod_versions;
  }
  static int32_t coverage() {
    return _coverage;
  }
  static uint32_t max_file_size() {
    return _max_file_size;
  }
  static bool set_revive_policy(const char* str, size_t len) {
    for (int i = 0; i < REVIVE_POLICY_NUM; i++) {
      size_t policy_len = strlen(_revive_policy_name[i]);
      if (!strncmp(str, _revive_policy_name[i], policy_len)) {
        _revive_policy = (RevivePolicy)i;
        if (str[policy_len] == '=') {
          _revive_policy_arg = NEW_C_HEAP_ARRAY(char, len - policy_len, mtInternal);
          strncpy(_revive_policy_arg, str + policy_len + 1, len - policy_len - 1);
          _revive_policy_arg[len - policy_len - 1] = 0;
        }
        return true;
      }
    }
    return false;
  }
  static RevivePolicy revive_poicy() {
    return _revive_policy;
  }
  static char* revive_policy_arg() {
    return _revive_policy_arg;
  }
  static bool is_expected_level(int level);
  static void print_statistics();
  static void collect_statistics(Method* m, elapsedTimer time, int revive_status);

  static LoaderType loader_type(oop loader);
  static LoaderType klass_loader_type(Klass* k);
  static LoaderType nmethod_loader_type(nmethod* nm);
  static bool is_save_candidate(CodeBlob* cb);
  static bool is_unsupported(ciEnv* env);
  static int64_t get_cds_identity() {
    return _cds_identity;
  }
  static void compute_cds_identity(FileMapInfo* mapinfo);
  // class redefine support
  static bool verify_redefined_identity() { return _verify_redefined_identity; }
  static bool make_revive_fail_at_nmethod() { return _make_revive_fail_at_nmethod; }
  static int32_t redefine_epoch() {
    return OrderAccess::load_acquire(&_redefine_epoch);
  }
  static void next_redefine_epoch(Klass* k);
  static void process_redefined_class(InstanceKlass* redefined_klass, InstanceKlass* scratch_klass);
};

#define CR_LOG(kind, level, fmt, ...)               \
  if (CodeRevive::is_log_on(kind, level))  {        \
    ResourceMark rm;                                \
    CodeRevive::log(fmt, ##__VA_ARGS__);            \
  }

#endif // SHARE_VM_CR_REVIVE_HPP
