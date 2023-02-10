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
#include "runtime/arguments.hpp"
#include "utilities/ostream.hpp"
#include "compiler/compileBroker.hpp"
#include "compiler/compilerOracle.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/codeReviveCodeBlob.hpp"
#include "cr/codeReviveVMGlobals.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "cr/revive.hpp"
#include "utilities/defaultStream.hpp"
#include "oops/method.hpp"

bool CodeRevive::_should_disable = false;
bool CodeRevive::_is_save = false;
bool CodeRevive::_is_restore = false;
bool CodeRevive::_is_merge = false;
bool CodeRevive::_print_opt = false;
bool CodeRevive::_fatal_on_fail = false;
bool CodeRevive::_perf_enable = false;
bool CodeRevive::_validate_check = true;
bool CodeRevive::_prepare_done = false;
bool CodeRevive::_disable_check_dir = false;
char* CodeRevive::_file_path = NULL;
char* CodeRevive::_log_file_path = NULL;
char* CodeRevive::_input_files = NULL;
char* CodeRevive::_input_list_file = NULL;
char* CodeRevive::_merge_wildcard_classpath = NULL;
int32_t CodeRevive::_percent = 100;
int32_t CodeRevive::_coverage = 100;
int32_t CodeRevive::_max_container_count = INT_MAX;
int32_t CodeRevive::_max_nmethod_versions = 16;
uint32_t CodeRevive::_log_kind = cr_number;
uint64_t CodeRevive::_max_file_size = G;
CodeRevive::MergePolicy CodeRevive::_merge_policy = CodeRevive::M_COVERAGE;
outputStream* CodeRevive::_log_file = NULL;
uint8_t CodeRevive::_log_ctrls[cr_number] = {0};
CodeReviveFile* CodeRevive::_load_file = NULL;
CodeReviveVMGlobals* CodeRevive::_vm_globals = NULL;
elapsedTimer CodeRevive::_t_aot_timers[CodeRevive::T_TOTAL_TIMERS];
uint CodeRevive::_aot_size_counters[CodeRevive::S_TOTAL_COUNTERS];
uint CodeRevive::_total_file_size = 0;
CodeRevive::RevivePolicy CodeRevive::_revive_policy = CodeRevive::REVIVE_POLICY_FIRST;
char* CodeRevive::_revive_policy_arg = NULL;
const char* CodeRevive::_revive_policy_name[CodeRevive::REVIVE_POLICY_NUM]= { "first", "random", "appoint" };


/*
 * Parsing option CodeRevive and set according actions
 * -XX:CodeReviveOptions=save,file=xxx
 * -XX:CodeReviveOptions=restore,file=xxx
 * -XX:CodeReviveOptions=save,file=xxx,log=all_reloc=fail,logfile=yyy
 * -XX:CodeReviveOptions=save,file=xxx,log=all_reloc=fail,method=trace,file=trace
 *
 * CodeRevive::log(kind, level, "fmt", arg1, arg2);
 * kind:
 *  relocation kinds: oop\metadata\external\runtime\call\internal\all_reloc
 *  archive: archive file load/store/summary information
 *  method: method save and revive trace information
 *  global: global runtime/external address
 *  all:
 * level: fail,trace
 *
 * debug option: debug_off=oop,meta,external,runtime
 *     if option is set, relocation type will be skipped when save code
 *
 * All options parsing fail will invalidate CodeRevive save/restore instead of fail and exit.
 *
 */

static inline size_t get_len_until_next_delimiter(const char* cur_pos, const char* end_pos) {
  const char* next_delimiter = strchr(cur_pos, ',');
  return next_delimiter == NULL ? end_pos - cur_pos : next_delimiter - cur_pos;
}

/*
 * Extract string from cur_pos and update dest. Return value is end_pos.
 */
static const char* parse_get_file_name(const char* cur_pos, const char* end_pos, char** dest) {
  ResourceMark rm;
  size_t total_len = get_len_until_next_delimiter(cur_pos, end_pos);
  if (*dest != NULL) {
    jio_fprintf(defaultStream::error_stream(), "duplicate file name specified, ignore previous %s\n", *dest);
    FREE_C_HEAP_ARRAY(char, *dest, mtInternal);
  }
  char* file_name = (char*)memcpy(NEW_RESOURCE_ARRAY(char, total_len + 1),
                                  cur_pos,
                                  total_len);
  file_name[total_len] = 0;
  *dest = (char*)make_log_name(file_name, NULL);
  if (*dest == NULL) {
    jio_fprintf(defaultStream::error_stream(), "convert %s fail, maybe too long\n", file_name);
    CodeRevive::set_should_disable();
  }
  return cur_pos + total_len;
}

static const char* parse_get_revive_policy(const char* cur_pos, const char* end_pos) {
  size_t total_len = get_len_until_next_delimiter(cur_pos, end_pos);

  bool ret = CodeRevive::set_revive_policy(cur_pos, total_len);
  if (!ret) {
    jio_fprintf(defaultStream::error_stream(), "unknown revive policy, use default first usable policy\n");
  }
  return cur_pos + total_len;
}

static const char* parse_get_merge_policy(const char* cur_pos, const char* end_pos) {
  size_t total_len = get_len_until_next_delimiter(cur_pos, end_pos);
  if (total_len == strlen("coverage") && strncmp(cur_pos, "coverage", total_len) == 0) {
    CodeRevive::set_merge_policy(CodeRevive::M_COVERAGE);
  } else if (total_len == strlen("simple") && strncmp(cur_pos, "simple", total_len) == 0) {
    CodeRevive::set_merge_policy(CodeRevive::M_SIMPLE);
  } else {
    jio_fprintf(defaultStream::error_stream(), "Invalid policy value: %s\n", cur_pos);
    vm_exit(1);
  }
  return cur_pos + total_len;
}

/*
 * Extract string from cur_pos and update dest. Return value is end_pos.
 */
static const char* parse_get_wildcard_classpath(const char* cur_pos, const char* end_pos, char** dest) {
  size_t total_len = get_len_until_next_delimiter(cur_pos, end_pos);
  if (*dest != NULL) {
    jio_fprintf(defaultStream::error_stream(), "duplicate merge wildcard classpath specified, ignore previous %s\n", *dest);
    FREE_C_HEAP_ARRAY(char, *dest, mtInternal);
  }
  char* classpath = (char*)memcpy(NEW_C_HEAP_ARRAY(char, total_len + 1, mtInternal),
                                  cur_pos,
                                  total_len);
  classpath[total_len] = 0;
  *dest = classpath;
  return cur_pos + total_len;
}

static int compute_integer_length(int32_t val) {
  assert(val >= 0, "shouldn't be negative");
  int digit_number = 0;
  do {
    val = val/10;
    digit_number++;
  } while(val);
  return digit_number;
}

static const char* parse_integer_value(const char* cur_pos, const char* end_pos, int32_t* value,
                                 int32_t expect_min, int32_t expect_max, const char* name) {
  ResourceMark rm;
  if (cur_pos != end_pos && *cur_pos == '0') {
    jio_fprintf(defaultStream::error_stream(), "Incorrect CodeReviveOptions:%s leading zero at \"%s\"\n", name, cur_pos);
    CodeRevive::set_should_disable();
    return cur_pos;
  }
  if (sscanf(cur_pos, "%d", value) != 1) {
    jio_fprintf(defaultStream::error_stream(), "Incorrect CodeReviveOptions:%s no integer at \"%s\"\n", name, cur_pos);
    CodeRevive::set_should_disable();
    return cur_pos;
  }
  if (*value > expect_max || *value < expect_min) {
    jio_fprintf(defaultStream::error_stream(), "Incorrect CodeReviveOptions:%s %d out of range [%d:%d]\n",
                                                name, *value, expect_min, expect_max);
    CodeRevive::set_should_disable();
    return cur_pos;
  }
  // forward according to digit number
  cur_pos += compute_integer_length(*value);
  return cur_pos;
}

static const char* parse_file_size(const char* cur_pos, const char* end_pos, uint64_t* file_size) {
  int32_t value;
  cur_pos = parse_integer_value(cur_pos, end_pos, &value, 1, INT_MAX, "max_file_size");
  if (CodeRevive::should_disable()) {
    return cur_pos;
  }
  if (*cur_pos != 'M' && *cur_pos != 'm') {
    jio_fprintf(defaultStream::error_stream(), "Incorrect CodeReviveOptions:max_file_size no ending with M or m\n", cur_pos);
    CodeRevive::set_should_disable();
    return cur_pos;
  }
  *file_size = value * M;

  if (*file_size > max_uintx || *file_size < M) {
    jio_fprintf(defaultStream::error_stream(), "Incorrect CodeReviveOptions:max_file_size %dM out of range [%d:%ld]\n",
                                                value, M, max_uintx);
    CodeRevive::set_should_disable();
    return cur_pos;
  }
  cur_pos += 1;
  return cur_pos;
}
/*
 * Find kind=level and update log ctrls
 */
struct cr_log_kind {
  const char* _name;
  size_t      _len;
  uint32_t    _start_index;
  uint32_t    _count;
};

struct cr_log_kind log_kinds[] = {
  {"archive",   strlen("archive"),    cr_archive,  1},
  {"global",    strlen("global"),     cr_global,   1},
  {"assembly",  strlen("assembly"),   cr_assembly, 1},
  {"opt",       strlen("opt"),        cr_opt,      1},
  {"save",      strlen("save"),       cr_save,     1},
  {"restore",   strlen("restore"),    cr_restore,  1},
  {"merge",     strlen("merge"),      cr_merge,    1},
  {"all",       strlen("all"),        cr_archive,  cr_number},
};

struct cr_log_level {
  const char* _name;
  size_t      _len;
  uint8_t     _val;
};

struct cr_log_level log_levels[] = {
  {"none",     strlen("none"),    cr_none},
  {"fail",     strlen("fail"),    cr_fail},
  {"warning",  strlen("warning"), cr_warning},
  {"trace",    strlen("trace"),   cr_trace},
  {"info",     strlen("info"),    cr_info},
};

static const char* parse_log_options(const char* cur_pos, const char* end_pos, uint8_t ctrls[]) {
  const int kinds_length = sizeof(log_kinds) / sizeof(struct cr_log_kind);
  const int levels_length = sizeof(log_levels) / sizeof(struct cr_log_level);
  const char* return_pos = cur_pos;
  while (cur_pos < end_pos) {
    bool found = false;
    for (struct cr_log_kind* kind = log_kinds; kind < (log_kinds + kinds_length); kind++) {
      // match kind
      if (strncmp(cur_pos, kind->_name, kind->_len) == 0 && cur_pos[kind->_len] == '=') {
        const char* level_pos = cur_pos + kind->_len + 1;
        for (struct cr_log_level* level = log_levels; level < (log_levels + levels_length); level++) {
          // match level
          if (strncmp(level_pos, level->_name, level->_len) == 0) {
            found = true;
            for (uint32_t i = kind->_start_index; i < (kind->_start_index + kind->_count); i++) {
              ctrls[i] = level->_val;
            }
            cur_pos = level_pos + level->_len;
            break;
          }
        }
        if (found) {
          break;
        }
      }
    }
    if (found == false) {
      break; // fail
    }
    return_pos = cur_pos;
    if (*cur_pos != ',') {
      break;
    }
    cur_pos += 1;
  }
  return return_pos;
}

void CodeRevive::parse_option_file() {
  if (CodeReviveOptions != NULL) {
    jio_fprintf(defaultStream::error_stream(), "CodeReviveOptions and CodeReviveOptionsFile can not set at same time, ignore both\n");
    CodeReviveOptions = NULL; // can not use FLAG_SET_ERGO
    return;
  }
  // set CodeReviveOptions with CodeReviveOptionsFile's content
  FILE* stream = fopen(CodeReviveOptionsFile, "rt");
  if (stream == NULL) {
    jio_fprintf(defaultStream::error_stream(), "CodeReviveOptionsFile %s can not open\n", CodeReviveOptionsFile);
    return;
  }
  char token[1024];
  int  pos = 0;
  int  c = getc(stream);
  while(c != EOF && pos < (int)(sizeof(token)-1)) {
    if (c == '\n') {
      break;
    }
    token[pos++] = c;
    c = getc(stream);
  }
  token[pos++] = '\0';
  if (pos == (int)sizeof(token)) {
    jio_fprintf(defaultStream::error_stream(), "CodeReviveOptionsFile content exceeds limit 1024\n", CodeReviveOptionsFile);
    return;
  }
  if (pos == 1) {
    // empty file means no CodeReviveOptions option
    return;
  }
  char* str = (char*)NEW_C_HEAP_ARRAY(char, pos, mtInternal);
  memcpy(str, token, pos);
  fclose(stream);
  FLAG_SET_ERGO(ccstr, CodeReviveOptions, str);
}

bool CodeRevive::parse_options() {
  const char* cur_pos = CodeReviveOptions;
  const char* end_pos = CodeReviveOptions + strlen(CodeReviveOptions);
  while (cur_pos < end_pos) {
    if (strncmp(cur_pos, "save", strlen("save")) == 0) {
      _is_save = true;
      _log_kind = cr_save;
      cur_pos += strlen("save");
    } else if (strncmp(cur_pos, "restore", strlen("restore")) == 0) {
      _is_restore = true;
      _log_kind = cr_restore;
      cur_pos += strlen("restore");
    } else if (strncmp(cur_pos, "merge", strlen("merge")) == 0) {
      _is_merge = true;
      _log_kind = cr_merge;
      cur_pos += strlen("merge");
    } else if (strncmp(cur_pos, "print_opt", strlen("print_opt")) == 0) {
      if (!UnlockDiagnosticVMOptions) {
        jio_fprintf(defaultStream::error_stream(),
                    "CodeRevive option 'print_opt' is diagnostic and must be enabled via -XX:+UnlockDiagnosticVMOptions\n");
        CodeRevive::set_should_disable();
      } else {
        _print_opt = true;
      }
      cur_pos += strlen("print_opt");
    } else if (strncmp(cur_pos, "fatal_on_fail", strlen("fatal_on_fail")) == 0) {
      if (!UnlockDiagnosticVMOptions) {
        jio_fprintf(defaultStream::error_stream(),
                    "CodeRevive option 'fatal_on_fail' is diagnostic and must be enabled via -XX:+UnlockDiagnosticVMOptions\n");
        CodeRevive::set_should_disable();
      } else {
        _fatal_on_fail = true;
      }
      cur_pos += strlen("fatal_on_fail");
    } else if (strncmp(cur_pos, "perf", strlen("perf")) == 0) {
      _perf_enable = true;
      cur_pos += strlen("perf");
    } else if (strncmp(cur_pos, "disable_validate_check", strlen("disable_validate_check")) == 0) {
      _validate_check = false;
      cur_pos += strlen("disable_validate_check");
    } else if (strncmp(cur_pos, "disable_check_dir", strlen("disable_check_dir")) == 0) {
      _disable_check_dir = true;
      cur_pos += strlen("disable_check_dir");
    } else if (strncmp(cur_pos, "file=", strlen("file=")) == 0) {
      cur_pos = parse_get_file_name(cur_pos + strlen("file="), end_pos, &_file_path);
    } else if (strncmp(cur_pos, "logfile=", strlen("logfile=")) == 0) {
      cur_pos = parse_get_file_name(cur_pos + strlen("logfile="), end_pos, &_log_file_path);
    } else if (strncmp(cur_pos, "log=", strlen("log=")) == 0) {
      cur_pos = parse_log_options(cur_pos + strlen("log="), end_pos, _log_ctrls);
    } else if (strncmp(cur_pos, "input_files=", strlen("input_files=")) == 0) {
      cur_pos = parse_get_file_name(cur_pos + strlen("input_files="), end_pos, &_input_files);
    } else if (strncmp(cur_pos, "input_list_file=", strlen("input_list_file=")) == 0) {
      cur_pos = parse_get_file_name(cur_pos + strlen("input_list_file="), end_pos, &_input_list_file);
    } else if (strncmp(cur_pos, "revive_policy=", strlen("revive_policy=")) == 0) {
      cur_pos = parse_get_revive_policy(cur_pos + strlen("revive_policy="), end_pos);
    } else if (strncmp(cur_pos, "policy=", strlen("policy=")) == 0) {
      cur_pos = parse_get_merge_policy(cur_pos + strlen("policy="), end_pos);
    } else if (strncmp(cur_pos, "percent=", strlen("percent=")) == 0) {
      cur_pos = parse_integer_value(cur_pos + strlen("precent="), end_pos, &_percent, 1, 99, "percent");
    } else if (strncmp(cur_pos, "coverage=", strlen("coverage=")) == 0) {
      cur_pos = parse_integer_value(cur_pos + strlen("coverage="), end_pos, &_coverage, 1, 99, "coverage");
    } else if (strncmp(cur_pos, "max_container_count=", strlen("max_container_count=")) == 0) {
      cur_pos = parse_integer_value(cur_pos + strlen("max_container_count="), end_pos, &_max_container_count, 1, INT_MAX, "max_container_count");
    } else if (strncmp(cur_pos, "max_nmethod_versions=", strlen("max_nmethod_versions=")) == 0) {
      cur_pos = parse_integer_value(cur_pos + strlen("max_nmethod_versions="), end_pos, &_max_nmethod_versions, 1, Method::aot_max_versions(), "max_nmethod_versions");
    } else if (strncmp(cur_pos, "max_file_size=", strlen("max_file_size=")) == 0) {
      cur_pos = parse_file_size(cur_pos + strlen("max_file_size="), end_pos, &_max_file_size);
    } else if (strncmp(cur_pos, "wildcard_classpath=", strlen("wildcard_classpath=")) == 0) {
      // During merging, if the classpath has wildcard and the order of jars in the directory with wildcard doesn't matter,
      // pass the classpath with wildcard_classpath option, and the order in the directory will be ignore.
      cur_pos = parse_get_wildcard_classpath(cur_pos + strlen("wildcard_classpath="), end_pos, &_merge_wildcard_classpath);
    } else {
      break;
    }
    if (CodeRevive::should_disable()) {
      return false;
    }
    if (*cur_pos != ',') {
      break;
    }
    cur_pos += 1;
  }

  // parsing fail
  if (cur_pos < end_pos) {
    jio_fprintf(defaultStream::error_stream(), "Incorrect CodeReviveOptions: starting from \"%s\"\n", cur_pos);
    CodeRevive::set_should_disable();
    return false;
  }
  return true;
}

PRAGMA_DIAG_PUSH
PRAGMA_FORMAT_NONLITERAL_IGNORED
void CodeRevive::log(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  _log_file->vprint(format, ap);
  va_end(ap);
}
PRAGMA_DIAG_POP

/*
 * Disable all CodeRevive actions, this might happen given options are wrong
 */
void CodeRevive::disable() {
  _is_save = false;
  _is_restore = false;
  _is_merge = false;
  _perf_enable = false;
}

void CodeRevive::on_vm_start() {
  if (CodeReviveOptions == NULL) {
    return;
  }
#if !defined(X86) || !defined(LINUX) || defined(ZERO)
  jio_fprintf(defaultStream::error_stream(), "CodeRevive isn't supported on this platform yet.\n");
  return;
#endif

  if (JvmtiExport::should_post_class_file_load_hook()) {
    return;
  }
  if (ExtendedDTraceProbes || DTraceMonitorProbes || DTraceMethodProbes || DTraceAllocProbes) {
    return;
  }
  if (parse_options() == false) {
    disable();
    return;
  }
  /*
   * Rules:
   * 1. must have file
   */
  if (_file_path == NULL) {
    jio_fprintf(defaultStream::error_stream(), "Incorrect CodeReviveOptions: no file specified\n");
    disable();
    return;
  }
  // default is 100 means must perform save/restore operation
  if (_percent != 100) {
    guarantee(_is_merge == false, "can not specify merge with percent");
    os::init_random((long)os::javaTimeNanos());
    int32_t r = (int32_t)(os::random() % 100); // value is in [0:99]
    if (r >= _percent) {
      disable();
      return;
    }
  }
  _log_file = tty;
  if (_log_file_path != NULL) {
    fileStream* cr_log_file = new (ResourceObj::C_HEAP, mtInternal) fileStream(_log_file_path);
    if (cr_log_file != NULL) {
      if (cr_log_file->is_open()) {
        _log_file = cr_log_file;
      } else {
        delete cr_log_file;
      }
    }
  }
  if (_is_save || _is_restore) {
    _vm_globals = new CodeReviveVMGlobals();
    DirWithClassTable::create_table();
  }
  // init global dummy oopMap and oopMapSet
  CodeReviveCodeBlob::init_dummy_oopmap();

  if (_is_restore || _print_opt) {
    _load_file = new CodeReviveFile();
    // _print_opt means only print, it will iterator all containers, no need get container now
    if(_load_file->map(_file_path, _print_opt == false /* need container*/, true, true, true, cr_restore) == false) {
      CR_LOG(cr_restore, cr_none, "Fail to load CSA file %s\n", _file_path);
      delete _load_file;
      _load_file = NULL;
      _is_restore = false;
      FREE_C_HEAP_ARRAY(char, _file_path, mtInternal);
      _file_path = NULL;
    } else if (is_log_on(cr_archive, cr_trace)) {
      _load_file->print();
    }
    // dump information for easy version compare
    // sort all method and dump in order
    // dump sorted dependency
    // dump sorted opts records
    if (_load_file != NULL && _print_opt) {
      _load_file->print_opt();
      vm_exit(0);
    }
  }
}

/*
 * when vm exit, perform save action.
 */
void CodeRevive::on_vm_shutdown() {
  if (_is_save) {
    if (is_log_on(cr_global, cr_trace)) {
      CR_LOG(cr_global, cr_trace, "VM globals:\n");
      _vm_globals->print();
    }
    CodeReviveFile* file = new CodeReviveFile();
    if (file->save(_file_path) == false) {
      CR_LOG(cr_save, cr_none, "Fail to save CSA file %s\n", _file_path);
      return;
    } else {
      CR_LOG(cr_save, cr_trace, "Succeed to save CSA file %s\n", _file_path);
      if (is_log_on(cr_archive, cr_trace)) {
        file->print();
      }
    }
  }

  if (perf_enable()) {
    print_statistics();
  }

  if (_log_file != tty && _log_file != NULL) {
    // when vm exit, c2 compiler is still running and may use the log file, just flush the content
    _log_file->flush();
  }
}

char* CodeRevive::find_revive_code(Method* m) {
  return _load_file->find_revive_code(m);
}

CodeReviveMetaSpace* CodeRevive::current_meta_space() {
  return _load_file->meta_space();
}

CodeReviveCodeSpace* CodeRevive::current_code_space() {
  return _load_file->code_space();
}

LoaderType CodeRevive::loader_type(oop loader) {
  LoaderType type = custom_loader;
  if (loader == NULL) {
    type = boot_loader;
  } else if (loader == SystemDictionary::java_system_loader()) {
    type = app_loader;
  } else if (loader == SystemDictionary::ext_loader()) {
    type = ext_loader;
  }
  return type;
}

LoaderType CodeRevive::klass_loader_type(Klass* k) {
  return loader_type(k->class_loader());
}

LoaderType CodeRevive::nmethod_loader_type(nmethod* nm) {
  return loader_type(nm->method()->method_holder()->class_loader());
}

void CodeRevive::print_statistics() {
  // print the statistic into log file
  outputStream* output = _log_file;
  if (is_save()) {
    output->cr();
    output->print_cr("  AOT save Information:");
    output->print_cr("    File header size: %d %6.3f%%", get_size_counter(S_HEADER), get_size_percent(S_HEADER));
    output->print_cr("    Classpath entry table size: %d %6.3f%%", get_size_counter(S_CLASSPATH_TABLE), get_size_percent(S_CLASSPATH_TABLE));
    output->print_cr("    Lookup table size: %d %6.3f%%", get_size_counter(S_LOOKUP_TABLE), get_size_percent(S_LOOKUP_TABLE));
    output->print_cr("    CodeBlob size:");
    output->print_cr("      Header size: %d %6.3f%%", get_size_counter(S_CB_HEADER), get_size_percent(S_CB_HEADER));
    output->print_cr("      Body size: %d %6.3f%%", get_size_counter(S_CB_BODY), get_size_percent(S_CB_BODY));
    output->print_cr("      Aux info size: %d %6.3f%%", get_size_counter(S_AUX_INFO), get_size_percent(S_AUX_INFO));
    output->print_cr("      Oopmap set size: %d %6.3f%%", get_size_counter(S_OOPMAP_SET), get_size_percent(S_OOPMAP_SET));
  } else if (is_restore()) {
    output->cr();
    output->print_cr("  Compile Information:");
    // print the compilation information from perf data
    CompileBroker::print_aot_times(output);
    // print the AOT restore detail information
    output->print_cr("  AOT restore Information:");
    output->print_cr("    Restore Method time   : %6.3f ms", get_ms_time(T_RESTORE));
    output->print_cr("      Lookup method time  : %6.3f ms", get_ms_time(T_LOOKUP_METHOD));
    // time for select method
    output->print_cr("      Select method time  : %6.3f ms", get_ms_time(T_SELECT));
    output->print_cr("        Weight check time : %6.3f ms", get_ms_time(T_LIGHTWEIGHT_CHECK));
    output->print_cr("        Preprocess time   : %6.3f ms", get_ms_time(T_PREPROC));
    // time for CodeReviveCodeBlob::create_nmethod
    output->print_cr("      Create nmethod time : %6.3f ms", get_ms_time(T_CREATE));
    output->print_cr("        Revive time       : %6.3f ms", get_ms_time(T_REVIVE));
    // time for restore aot method failure
    output->print_cr("    Fail to restore time  : %6.3f ms", get_ms_time(T_RESTORE_FAIL));
  } else {
    output->cr();
    output->print_cr("  Compile Information:");
    // print the compilation information from perf data
    CompileBroker::print_aot_times(output);
  }
}

void CodeRevive::collect_statistics(elapsedTimer time, bool success) {
  if (success) {
    get_aot_timer(T_RESTORE)->add(time);
  } else {
    get_aot_timer(T_RESTORE_FAIL)->add(time);
  }
}

bool CodeRevive::is_expected_level(int level) {
  return level == CompLevel_full_optimization;
}

bool CodeRevive::is_save_candidate(CodeBlob* cb) {
  if (cb->is_nmethod() == false) {
    return false;
  }
  nmethod* nm = (nmethod*)cb;
  if (nm->is_java_method() == false) {
    return false;
  }
  if (nm->is_osr_method()) {
    return false;
  }
  if (nm->is_compiled_by_c2() == false) {
    return false;
  }
  if (nm->is_in_use() == false) {
    return false;
  }
  if (nm->has_call_site_target_value()) {
    return false;
  }

  // skip nm for methods in custom class loader
  if (CodeRevive::nmethod_loader_type(nm) == custom_loader) {
    return false;
  }

  if (nm->method()->method_holder()->is_anonymous()) {
    return false;
  }

  if (CompilerOracle::should_revive(methodHandle(nm->method())) == false) {
    return false;
  }
  return true;
}

void CodeRevive::check_class_dir_path(const char* dir_path, size_t len, TRAPS) {
  DirWithClassEntry* entry = DirWithClassTable::lookup(dir_path, len, false, THREAD);
  if (entry->is_in_classpath()) {
    CR_LOG(cr_restore, cr_fail, "Load class from directory: %s\n", dir_path);
    // disable aot
    _is_restore = false;
  }
}

void CodeRevive::add_class_dir_path(const char* dir_path, size_t len, TRAPS) {
  DirWithClassTable::lookup(dir_path, len, true, THREAD);
}

bool CodeRevive::read_class_from_path(const char* dir_path, size_t len, TRAPS) {
  return DirWithClassTable::lookup_only(dir_path, len);
}

bool CodeRevive::is_revive_candidate(Method* target, int task_level) {
  if (!_is_restore) {
    return false;
  }
  if (!_prepare_done) {
    return false;
  }
  if (!CodeRevive::is_expected_level(task_level)) {
    return false;
  }
  if (klass_loader_type(target->method_holder()) == custom_loader) {
    return false;
  }
  if (!CompilerOracle::should_revive(methodHandle(target))) {
    return false;
  }
  if (target->aot_disabled()) {
    return false;
  }
  return true;
}

bool CodeRevive::is_unsupported(ciEnv* env) {
  if (env->should_retain_local_variables() || env->jvmti_can_hotswap_or_post_breakpoint() ||
      env->jvmti_can_post_on_exceptions()) {
    return true;
  }
  if (env->dtrace_extended_probes() || env->dtrace_monitor_probes() ||
      env->dtrace_method_probes() || env->dtrace_alloc_probes()) {
    return true;
  }
  return false;
}

// check whether the class files are in directory
void CodeRevive::check_dir_path(const char* source_path, TRAPS) {
  if (CodeRevive::disable_check_dir()) {
    return;
  }
  size_t len = strlen(source_path);
  // check whether the source is directory
  // the source format is:
  // file:directory end with /
  // file:<name>.jar
  if ((source_path[len - 1] == os::file_separator()[0]) && strncmp(source_path, "file:", 5) == 0) {
    const char* dir_path = source_path + 5;
    len = len - 5;
    if (CodeRevive::is_save()) {
      // record the directory
      CodeRevive::add_class_dir_path(dir_path, len, THREAD);
    } else {
      // check whether the directory is in the AOT class path
      CodeRevive::check_class_dir_path(dir_path, len, THREAD);
    }
  }
}
