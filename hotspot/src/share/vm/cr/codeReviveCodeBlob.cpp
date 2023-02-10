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
#include "code/codeCache.hpp"
#include "compiler/abstractCompiler.hpp"
#include "compiler/compilerOracle.hpp"
#include "compiler/compileBroker.hpp"
#include "compiler/disassembler.hpp"
#include "cr/codeReviveCodeBlob.hpp"
#include "cr/codeReviveCodeSpace.hpp"
#include "cr/codeReviveDependencies.hpp"
#include "cr/codeReviveVMGlobals.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/revive.hpp"
#include "cr/codeReviveAuxInfo.hpp"
#include "utilities/align.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/sweeper.hpp"

const char* CodeReviveCodeBlob::_revive_result_name[5] = {
  "OK", "Fail on meta array", "Fail on oop array", "Fail on reloc info", "Fail on opt check"
};

OopMapSet* CodeReviveCodeBlob::_dummy_oopmapset = NULL;
OopMap*    CodeReviveCodeBlob::_dummy_oopmap = NULL;

/*
 * construtor to save cb
 * 1. init header
 * 2. memcopy CodeBlob body
 * 3. iterate CodeBlob relocations and emit aux reloc data
 * 4. iterate aux code revive relocation informations
 */
CodeReviveCodeBlob::CodeReviveCodeBlob(char* start, char* limit, CodeBlob* cb, CodeReviveMetaSpace* meta_space) {
  init(start, meta_space);
  _header->_next_version_offset = -1;
  _limit = limit;
  _cb = cb;
}

// used in restore only
CodeReviveCodeBlob::CodeReviveCodeBlob(char* start, CodeReviveMetaSpace* meta_space) {
  init(start, meta_space);
  _cur = _limit = (_start + _header->_size);
  _cb = NULL;
}

void CodeReviveCodeBlob::init(char* start, CodeReviveMetaSpace* meta_space) {
  _start = start;
  _header = (Header*)_start;
  _alignment = 8;
  _meta_space = meta_space;
}

bool CodeReviveCodeBlob::save() {
  // step1 init header
  _cur = _start + sizeof(Header);
  _cur = align_up(_cur, _alignment);
  guarantee(_cur <= _limit, "exceed limit after header");

  // step2 copy code blob body
  char* cb_start = _cur;
  _cur += align_up(_cb->size(), _alignment);
  guarantee(_cur <= _limit, "exceed limit after copy cb");
  memcpy(cb_start, (char*)_cb, _cb->size());
  _header->_cb_size = (_cur - cb_start);
  _header->_entry_offset = ((nmethod*)_cb)->entry_point() - (_cb->content_begin());
  _header->_verified_entry_offset = ((nmethod*)_cb)->verified_entry_point() - (_cb->content_begin());
  // step3 emit aux reloc data ( oops/metadatas array and relocations)
  if (save_revive_aux_info() == false) {
    return false;
  }
  guarantee(_cur <= _limit, "exceed limit after save_revive_aux_info");

  // copy oop map set
  char* oop_map_set_start = _cur;
  save_oop_map_set();
  _header->_oop_map_size = _cur - oop_map_set_start;
  guarantee(oop_maps_begin() == oop_map_set_start, "");
  guarantee(_cur <= _limit, "exceed limit after save_oop_map_set");

  _header->_size = (_cur - _start);

  // count Codeblob size;
  CodeRevive::add_size_counter(CodeRevive::S_CB_HEADER, sizeof(Header));
  CodeRevive::add_size_counter(CodeRevive::S_CB_BODY, _header->_cb_size);
  CodeRevive::add_size_counter(CodeRevive::S_AUX_INFO, _header->_aux_meta_array_size +
                               _header->_aux_oop_array_size + _header->_aux_reloc_data_size);
  CodeRevive::add_size_counter(CodeRevive::S_OOPMAP_SET, _header->_oop_map_size);

  return true;
}

bool CodeReviveCodeBlob::save_revive_aux_info() {
  _header->_aux_meta_array_begin = (_cur - _start);
  char* aux_meta_array_start = _cur;
  char* aux_oop_array_start = NULL;
  char* aux_reloc_start = NULL;
  if (_cb->is_nmethod()) {
    ThreadInVMfromUnknown __tiv;
    ResourceMark rm;
    {
      ReviveAuxInfoTask task((nmethod*)_cb, _cur, _meta_space);
      task.emit_meta_array();
      if (task.success() == false) {
        return false;
      }
      _cur = task.pos();
      _header->_aux_oop_array_begin = (_cur - _start);
      _header->_aux_meta_array_size = _header->_aux_oop_array_begin - _header->_aux_meta_array_begin;
      aux_oop_array_start = _cur;

      task.emit_oop_array();
      if (task.success() == false) {
        return false;
      }
      _cur = task.pos();
      _header->_aux_reloc_data_begin = (_cur - _start);
      _header->_aux_oop_array_size = _header->_aux_reloc_data_begin - _header->_aux_oop_array_begin;
      aux_reloc_start = _cur;
    }

    {
      ReviveAuxInfoTask task((nmethod*)_cb, _cur, _meta_space);
      task.emit();
      if (task.success() == false) {
        return false;
      }
      _cur = task.pos();
      _header->_aux_reloc_data_size = (_cur - _start) - _header->_aux_reloc_data_begin;
    }
  } else {
    assert(false, "not nmethod");
  }
  guarantee(aux_meta_array_begin() == aux_meta_array_start, "should be");
  guarantee(aux_oop_array_begin() == aux_oop_array_start, "should be");
  guarantee(aux_reloc_begin() == aux_reloc_start, "should be");
  return true;
}

void CodeReviveCodeBlob::save_oop_map_set() {
  OopMapSet* oop_maps = _cb->oop_maps();
  assert(((size_t)_cur) % _alignment == 0, "");
  // emit _om_count
  // iterate OopMap*
  //    _pc_offset  emit
  //    _omv_count  emit
  //    _omv_data_size  emit
  //    _omv_data   copy
  //    align
  int32_t count = oop_maps->om_count();
  int32_t heap_size = oop_maps->heap_size();
  char* begin = _cur;
  emit_u4(heap_size);
  emit_u4(count);
  for (int i = 0; i < count; i++) {
    OopMap* map = oop_maps->at(i);
    emit_u4(map->offset());
    emit_u4(map->omv_count());
    emit_u4(map->omv_data_size());
    memcpy(_cur, (char*)map->omv_data(), map->omv_data_size());
    _cur += map->omv_data_size();
    _cur = align_up(_cur, _alignment);
  }
}

CodeBlob* CodeReviveCodeBlob::extract_cb() {
  assert(_cb == NULL, "unexpected");
  return (CodeBlob*)(align_up(_start + sizeof(Header), _alignment));
}

// consturct new OopMapSet when restore
OopMapSet* CodeReviveCodeBlob::restore_oop_map_set() {
  int align = sizeof(void *) - 1;
  _cur = oop_maps_begin();
  int32_t heap_size = read_u4();
  int32_t count = read_u4();
  address mem = (address)NEW_C_HEAP_ARRAY(char, heap_size, mtCode);
  OopMapSet* oop_maps = (OopMapSet*)mem;
  // initilaize oop_maps vtable and fields
  memcpy(mem, _dummy_oopmapset, sizeof(OopMapSet));
  oop_maps->set_om_count(count);
  oop_maps->set_om_size(count);
  mem += sizeof(OopMapSet);
  mem = (address)((intptr_t)(mem + align) & ~align);
  oop_maps->set_om_data((OopMap**)mem);
  mem += (count * sizeof(OopMap*));

  for(int i = 0; i < count; i++) {
    int pc_offset = read_u4();
    int omv_count = read_u4();
    int omv_data_size = read_u4();
    memcpy(mem, _dummy_oopmap, sizeof(OopMap));
    OopMap* map = (OopMap*)mem;
    oop_maps->set(i, map);
    map->set_offset(pc_offset);
    map->set_omv_count(omv_count);
    map->set_omv_data_size(omv_data_size);
    map->set_omv_data(mem + sizeof(OopMap));
    map->set_write_stream(NULL);
    memcpy(mem + sizeof(OopMap), _cur, omv_data_size);
    _cur += omv_data_size;
    _cur = align_up(_cur, _alignment);
    mem += map->heap_size();
  }
  oop_maps->set_om_size(-1);
  return oop_maps;
}

void CodeReviveCodeBlob::print_aux_info(char* begin, char* end) {
  _cur = begin;
  PrintAuxInfoTask task(_cur, _meta_space);
  task.print(end);
}

// print all aux relocation information
void CodeReviveCodeBlob::print() {
  ResourceMark rm;

  CodeRevive::out()->print_cr("Print Aux Information in meta array:");
  print_aux_info(aux_meta_array_begin(), aux_meta_array_begin() + aux_meta_array_size());

  CodeRevive::out()->print_cr("Print Aux Information in oop array:");
  print_aux_info(aux_oop_array_begin(), aux_oop_array_begin() + aux_oop_array_size());

  CodeRevive::out()->print_cr("Print Aux Information in relocation:");
  print_aux_info(aux_reloc_begin(), aux_reloc_begin() + aux_reloc_size());

  // print oop map information
  {
    OopMapSet* set = restore_oop_map_set();
    set->print_on(CodeRevive::out());
    FREE_C_HEAP_ARRAY(char, set, mtCode);
  }
}

static int compare_opt_records_by_name(OptRecord** opt1, OptRecord** opt2) {
  return (*opt1)->compare_by_type_name(*opt2);
}

static int compare_dep_records_by_name(ReviveDepRecord** d1, ReviveDepRecord** d2) {
  return (*d1)->compare_by_type_name_or_index(*d2);
}

void CodeReviveCodeBlob::print_opt(char* method_name) {
  ResourceMark rm;
  // get meta_array name
  CollectMetadataArrayNameTask task(aux_meta_array_begin(), _meta_space, method_name);
  GrowableArray<char*>* meta_array_names = task.collect_names();

  // sort and dump dependencies
  GrowableArray<ReviveDepRecord*>* depends = get_revive_deps(meta_array_names);
  depends->sort(compare_dep_records_by_name);
  for (int i = 0; i < depends->length(); i++) {
    depends->at(i)->print_on(CodeRevive::out(), 1);
  }

  // sort and print opt info
  if (opt_records_size() > 0) {
    GrowableArray<OptRecord*>* opts = get_opt_records_for_dump(meta_array_names);
    opts->sort(compare_opt_records_by_name);
    for (int i = 0; i < opts->length(); i++) {
      opts->at(i)->print_on(CodeRevive::out(), 1);
    }
  }
}

GrowableArray<ciBaseObject*>* CodeReviveCodeBlob::preprocess_oop_and_meta(char* aux_reloc_begin, Method* method,
                                                                          bool data_array, GrowableArray<uint32_t>* global_oop_array) {
  PreReviveTask task(method, aux_reloc_begin, _meta_space);
  GrowableArray<ciBaseObject*>* oops_metas = task.pre_revive_oop_and_meta(data_array, global_oop_array);
  if (task.success()) {
    return oops_metas;
  } else {
    return NULL;
  }
}

char* CodeReviveCodeBlob::dependencies_begin() {
  assert(_cb == NULL, "must in restore");
  CodeBlob* saved_cb = extract_cb();
  return (char*)((nmethod*)saved_cb)->dependencies_begin();
}

int CodeReviveCodeBlob::dependencies_size() {
  assert(_cb == NULL, "must in restore");
  CodeBlob* saved_cb = extract_cb();
  return ((nmethod*)saved_cb)->dependencies_size();
}

address CodeReviveCodeBlob::opt_records_begin() {
  assert(_cb == NULL, "must in restore");
  CodeBlob* saved_cb = extract_cb();
  return (address)((nmethod*)saved_cb)->opt_records_begin();
}

int CodeReviveCodeBlob::opt_records_size() {
  assert(_cb == NULL, "must in restore");
  CodeBlob* saved_cb = extract_cb();
  return ((nmethod*)saved_cb)->opt_records_size();
}

bool CodeReviveCodeBlob::validate_aot_method_dependencies(ReviveDependencies* revive_deps) {
  for (Dependencies::DepStream deps(revive_deps); deps.next(); ) {
    Klass* witness = deps.check_dependency();
    if (witness != NULL) {
      if (CodeRevive::is_log_on(cr_restore, cr_fail)) {
        ResourceMark rm;
        CodeRevive::out()->print_cr("Fail when validate dependencies at %s", witness->external_name());
        deps.print_dependency(witness, false, CodeRevive::out());
      }
      return false;
    }
  }
  return true;
}
/*
 * iterate all opt records in nmethod and estimate benefit
 * return value:
 *  int_max if optimization will cause trap
 *  negative value for benefit compare without optimization
 *
 *  for de-virtualize: reduce virtual call but add extra check
 *    if not hit, extra check will have extra overhead, opt has negative effect
 *
 *  for unstable if, simpilfy graph, no extra overhead, give a fixed benefit?
 *
 *  for profiled receiver: reduce slow path check cost with fast check
 *
 *  for inline:
 */
int CodeReviveCodeBlob::check_aot_method_opt_records(GrowableArray<ciBaseObject*>* metas) {
  int total = 0;
  if (opt_records_size() == 0) {
    return total;
  }
  ResourceMark rm;
  OptStream stream(OptRecord::REVIVE, metas, opt_records_begin());
  for (OptRecord* o = stream.next(); o != NULL; o = stream.next()) {
    if (CodeRevive::is_log_on(cr_restore, cr_info)) {
      CodeRevive::out()->print("OptRecord check: ");
      o->print_on(CodeRevive::out());
    }
    int result = o->calc_opt_score();
    if (result == max_jint) {
      return max_jint;
    }
    // Accumulated directly, may be handled with different policies.
    total += result;
  }
  CR_LOG(cr_restore, cr_trace, "Check opt records %s, score is %d\n", total < 0 ? "negative" : "positive", total);
  return total;
}

/*
 * Used to get dependency records.
 * use type name comparation to determine whether the two deps is equal.
 *   meta_array_names: metadata name string array
 */
GrowableArray<ReviveDepRecord*>* CodeReviveCodeBlob::get_revive_deps(GrowableArray<char*>* meta_array_names) {
  GrowableArray<ReviveDepRecord*>* depends = new GrowableArray<ReviveDepRecord*>();
  ReviveDependencies revive_deps(dependencies_begin(), dependencies_size(), NULL);
  for (Dependencies::DepStream deps(&revive_deps); deps.next(); ) {
    ReviveDepRecord* d = new ReviveDepRecord(deps.type(), meta_array_names);
    depends->append(d);
    for (int i = 0; i < deps.argument_count(); i++) {
      int index = deps.argument_index(i);
      // 0 means special encode using next argument's holder etc.
      d->add_argument_idx(index);
    }
  }
  return depends;
}

/*
 * Used to get dependency records.
 * use type index comparation to determine whether the two deps is equal.
 *   meta_array_index: metaspace global index array
 */
GrowableArray<ReviveDepRecord*>* CodeReviveCodeBlob::get_revive_deps(GrowableArray<int32_t>* meta_array_index) {
  GrowableArray<ReviveDepRecord*>* depends = new GrowableArray<ReviveDepRecord*>();
  ReviveDependencies revive_deps(dependencies_begin(), dependencies_size(), NULL);
  for (Dependencies::DepStream deps(&revive_deps); deps.next(); ) {
    ReviveDepRecord* d = new ReviveDepRecord(deps.type(), meta_array_index);
    depends->append(d);
    for (int i = 0; i < deps.argument_count(); i++) {
      int index = deps.argument_index(i);
      // 0 means special encode using next argument's holder etc.
      d->add_argument_idx(index);
    }
  }
  return depends;
}

/*
 * Used to get opt records,
 *   ctx_type: dump or merge stage
 *   ctx: meta name for dump stage, meta space for merge stage
 *   data_array_meta_index: meta indexes for data array in the code blob
 */
GrowableArray<OptRecord*>* CodeReviveCodeBlob::get_opt_records(OptRecord::CtxType ctx_type, void* ctx,
    GrowableArray<int32_t>* data_array_meta_index) {
  GrowableArray<OptRecord*>* result = new GrowableArray<OptRecord*>();
  if (opt_records_size() != 0) {
    OptStream stream(ctx_type, ctx, opt_records_begin());
    for (OptRecord* o = stream.next(); o != NULL; o = stream.next()) {
      if (data_array_meta_index != NULL) {
        o->nm_meta_index_to_meta_space_index(data_array_meta_index);
      }
      result->append(o);
    }
  }
  return result;
}

bool CodeReviveCodeBlob::create_global_oops(GrowableArray<uint32_t>* global_oop_array) {
  for (int i = 0; i < global_oop_array->length(); i++) {
    uint32_t value = global_oop_array->at(i);
    if (ReviveAuxInfoTask::get_global_oop((CR_KnownOopKind)value) == NULL) {
      return false;
    }
  }
  return true;
}

/*
 * Version Select Step 1.
 * Lightweight check for aot codes version select.
 *   Check whether opt records in certain version are valid.
 *   Metadata array pre-process is needed, for it is used in Opt Check.
 */
CodeReviveCodeBlob::ReviveResult CodeReviveCodeBlob::revive_and_opt_record_check(JitVersionReviveState* revive_state) {
  TraceTime t("AOT Lightweight Check", CodeRevive::get_aot_timer(CodeRevive::T_LIGHTWEIGHT_CHECK),
               CodeRevive::perf_enable());
  revive_state->_meta_array = preprocess_oop_and_meta(aux_meta_array_begin(), revive_state->_method,
                                                      true, NULL);
  if (revive_state->_meta_array == NULL) {
    return REVIVE_FAILED_ON_META_ARRAY;
  }

  // opt check depends on MethodData, must invoke in native state
  // return null if current profiling must cause trap
  // assumption: oop_meta_array put metadata before oop
  int result = check_aot_method_opt_records(revive_state->_meta_array);
  if (result == max_jint) {
    return REVIVE_FAILED_ON_OPT_RECORD;
  }
  return REVIVE_OK;
}

/*
 * Version Select Step 2.
 * Other pre-processes for versions passed OptRecord check.
 *   Include:
 *     pre-process Oop Array, RelocInfo.
 *     create global oops
 * If failed, back to Step 1.
 */
CodeReviveCodeBlob::ReviveResult CodeReviveCodeBlob::revive_preprocess(JitVersionReviveState* revive_state) {
  TraceTime t("AOT Preprocess", CodeRevive::get_aot_timer(CodeRevive::T_PREPROC), CodeRevive::perf_enable());
  revive_state->_global_oop_array = new GrowableArray<uint32_t>();
  revive_state->_oops_metas = preprocess_oop_and_meta(aux_reloc_begin(), revive_state->_method,
                                                      false, revive_state->_global_oop_array);
  if (revive_state->_oops_metas == NULL) {
    return REVIVE_FAILED_ON_RELOCINFO;
  }

  revive_state->_oop_array = preprocess_oop_and_meta(aux_oop_array_begin(), revive_state->_method,
                                                     true, revive_state->_global_oop_array);
  if (revive_state->_oop_array == NULL) {
    return REVIVE_FAILED_ON_OOP_ARRAY;
  }

  return REVIVE_OK;
}

/*
 * create a nmethod based on infomration in CodeReviveCodeBlob
 * 1. get nmethod size and create nmethod, copy all nmethod content, skip header part
 * 2. fix header: oop map/entry and others
 * 3. iterate relocations and aux info
 * 4. if some relocations not supported, fix relocations according to old nm
 * 5. reigster nmethod
 */
nmethod* CodeReviveCodeBlob::create_nmethod(methodHandle method,
                                            uint compile_id,
                                            AbstractCompiler* compiler,
                                            JitVersionReviveState* selected_code) {
  assert_locked_or_safepoint(MethodCompileQueue_lock);
  assert_locked_or_safepoint(Compile_lock);

  // step0 check dependencies
  ReviveDependencies revive_deps(dependencies_begin(), dependencies_size(), selected_code->_meta_array);
  if (!validate_aot_method_dependencies(&revive_deps)) {
    ciEnv::current()->record_failure("validate_dependencies");
    return NULL;
  }
  // lock for safety, same with other nmethod::new_nmethod
  MutexLockerEx mu(CodeCache_lock, Mutex::_no_safepoint_check_flag);

  // step1
  nmethod* nm = nmethod::new_nmethod(cb_size());
  if (nm == NULL) {
    ciEnv::current()->record_failure("code_cache_full");
    guarantee(false, "can not allocate space");
  }
  CodeBlob* saved_cb = extract_cb();
  address dest  = ((address)nm)       + in_bytes(CodeBlob::name_offset());
  address src   = ((address)saved_cb) + in_bytes(CodeBlob::name_offset());
  int copy_size = cb_size()           - in_bytes(CodeBlob::name_offset());
  memcpy(dest, src, copy_size);

  // step2, init header
  // CodeBlob
  nm->_name = "nmethod";
  nm->_oop_maps = restore_oop_map_set();
#ifndef PRODUCT
  nm->_strings._strings = NULL;
#ifdef ASSERT
  nm->_strings._defunct = false;
#endif
#endif
  // nmethod
  nm->_method = method();
  nm->_jmethod_id = NULL;
  nm->_entry_point = nm->content_begin() + _header->_entry_offset;
  nm->_verified_entry_point = nm->content_begin() + _header->_verified_entry_offset;
  nm->_osr_entry_point = NULL;
  guarantee(nm->_state == nmethod::in_use, "");
  guarantee(nm->_entry_bci == InvocationEntryBci, "");
  if (UseG1GC) {
    nm->_unloading_next = NULL;
  } else {
    nm->_scavenge_root_link = NULL;
  }
  guarantee(nm->_oops_do_mark_link == NULL, "");
  nm->_compiler = compiler;
  nm->_compile_id = compile_id;
  nm->_hotness_counter = NMethodSweeper::hotness_counter_reset_val();
  guarantee(nm->_has_flushed_dependencies == false, "");
  guarantee(nm->_marked_for_reclamation == false, "");
  guarantee(nm->_marked_for_deoptimization == false, "");
  guarantee(nm->_unload_reported == false, "");
  guarantee(nm->_stack_traversal_mark == 0, "");
  nm->_lock_count = 0;
  nm->_scavenge_root_state = 0;
  nm->_exception_cache = NULL;
  nm->_pc_desc_cache.reset_to(nm->scopes_pcs_begin());
  nm->set_load_from_aot();

  // step3 restore aux information
  {
    TraceTime t("AOT Revive", CodeRevive::get_aot_timer(CodeRevive::T_REVIVE), CodeRevive::perf_enable());

    ResourceMark rm;
    {
      // must be called before ReviveAuxInfoTask.revive, because data array will be used when revive relocations.
      ReviveAuxInfoTask task_data(nm, NULL, _meta_space);
      task_data.revive_data_array(selected_code->_meta_array, selected_code->_oop_array);
      guarantee(task_data.success(), "should be");
    }
    {
      ReviveAuxInfoTask task_reloc(nm, aux_reloc_begin(), _meta_space);
      task_reloc.revive(aux_reloc_begin() + aux_reloc_size(), selected_code->_oops_metas);
      guarantee(task_reloc.success(), "should be");
    }
  }

  // step4, finalize copy from nmethod::nmethod
  if (ScavengeRootsInCode) {
    if (nm->detect_scavenge_root_oops()) {
      CodeCache::add_scavenge_root_nmethod(nm);
    }
    Universe::heap()->register_nmethod(nm);
  }
  debug_only(nm->verify_scavenge_root_oops());

  CodeCache::commit(nm);
  assert(compiler->is_c2() ||
         method->is_static() == (nm->entry_point() == nm->_verified_entry_point),
         " entry points must be same for static methods and vice versa");

  bool printnmethods = PrintNMethods
    || CompilerOracle::should_print(method)
    || CompilerOracle::has_option_string(method, "PrintNMethods");
  if (printnmethods || PrintDebugInfo || PrintRelocations || PrintDependencies || PrintOptRecords || PrintExceptionHandlers || PrintNMCodeValue) {
    nm->print_nmethod(printnmethods);
  }

  // step6, finalize copy from new_nmethod
  for (Dependencies::DepStream deps(nm); deps.next(); ) {
    Klass* klass = deps.context_type();
    if (klass == NULL) {
      continue;  // ignore things like evol_method
    }

    // record this nmethod as dependent on this klass
    InstanceKlass::cast(klass)->add_dependent_nmethod(nm);
  }
  nmethod::post_test_revive_replace(nm);
  if (PrintAssembly || CompilerOracle::has_option_string(method, "PrintAssembly")) {
    Disassembler::decode(nm);
  }
  DEBUG_ONLY(nm->verify();)
  nm->log_new_nmethod();
  return nm;
}

bool CodeReviveCodeBlob::check_meta_resolve(char* aux_reloc_begin, bool data_array) {
  CheckMetaResolveTask task(aux_reloc_begin, _meta_space);
  task.iterate_reloc_aux_info();
  return task.success();
}

bool CodeReviveCodeBlob::can_be_merged() {
  //check meta
  if (!check_meta_resolve(aux_reloc_begin())) {
    return false;
  }
  // check meta array
  if (!check_meta_resolve(aux_meta_array_begin())) {
    return false;
  }
  return true;
}

void CodeReviveCodeBlob::init_dummy_oopmap() {
  _dummy_oopmapset = new (ResourceObj::C_HEAP, mtInternal) OopMapSet(true);
  _dummy_oopmap = new (ResourceObj::C_HEAP, mtInternal) OopMap(0, 0, true);
}
