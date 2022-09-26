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
#include "precompiled.hpp"
#include "cr/codeReviveAuxInfo.hpp"
#include "cr/codeReviveCodeSpace.hpp"
#include "cr/codeReviveContainer.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/codeReviveFingerprint.hpp"
#include "cr/codeReviveJitMeta.hpp"
#include "cr/codeReviveLookupTable.hpp"
#include "cr/codeReviveMerge.hpp"
#include "cr/codeReviveOptRecords.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "cr/classPathEntryTable.hpp"
#include "cr/revive.hpp"
#include "memory/filemap.hpp"
#include "runtime/arguments.hpp"
#include "runtime/vm_operations.hpp"
#include "runtime/vmThread.hpp"
#include "utilities/align.hpp"
#include "utilities/defaultStream.hpp"

CodeReviveMetaSpace*  CodeReviveMerge::_global_meta_space = NULL; 
GrowableArray<char*>* CodeReviveMerge::_csa_filenames = NULL;
GrowableArray<const char*>* CodeReviveMerge::_cp_array = NULL;
GrowableArray<WildcardEntryInfo*>* CodeReviveMerge::_merged_cp_array = NULL;
GrowableArray<const char*>* CodeReviveMerge::_sorted_jars_in_wildcards = NULL;
Arena* CodeReviveMerge::_arena = NULL;

int CodeReviveMerge::_cp_array_size = 0;

class FilePathWalker {
 private:
  GrowableArray<char*>* _array;

  void add_file(char* filename) {
    size_t len = strlen(filename);
    if (len < 4 || strcmp(filename + (len - 4), ".csa")) {
      return;
    }
    _array->append(filename);
    CR_LOG(cr_merge, cr_trace, "Merge input file %s\n", filename);
  }

  void add_directory(const char* dir) {
    DIR* d = NULL;
    if (!(d = os::opendir(dir))) {
      CR_LOG(cr_merge, cr_fail, "Fail to open dir %s\n", dir);
      return;
    }

    struct dirent* dp = NULL;
    while((dp = os::readdir(d))) {
      if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
        continue;
      }
      size_t len = strlen(dir) + strlen(dp->d_name) + 2;
      char* full_name = NEW_RESOURCE_ARRAY(char, len);
      jio_snprintf(full_name, len, "%s/%s", dir, dp->d_name);
      add_file_or_directory(full_name);
    }

    os::closedir(d);
  }

 public:
  FilePathWalker(GrowableArray<char*>* array, char* path, char* list_file) : _array(array) {
    if (path != NULL) {
      add_file_or_directory(path);
    }
    if (list_file != NULL) {
      char line[1024];
      FILE* f = fopen(list_file, "r");
      if (f == NULL) {
        return;
      }
      while(fgets(line, 1024, f) != NULL) {
        size_t len = strlen(line);
        if (line[len - 1] == '\n') {
          line[len - 1] = 0;
        }
        char* file_name = NEW_RESOURCE_ARRAY(char, len);
        strncpy(file_name, line, len);
        file_name[len] = 0;
        add_file_or_directory(file_name);
      }
      fclose(f);
    }
  }

  void add_file_or_directory(char* file_or_dir) {
    struct stat sbuf;
    int32_t ret = os::stat(file_or_dir, &sbuf);
    if (ret != 0) {
      CR_LOG(cr_merge, cr_warning, "Fail to get status of %s\n", file_or_dir);
      return;
    }
    
    if ((sbuf.st_mode & S_IFMT) == S_IFDIR) {
      add_directory(file_or_dir);
    } else {
      add_file(file_or_dir);
    }
  }
};

/*
 * set the offset in csa file and code size
 * reset the ptr of codeblob to NULL
 */
void CandidateCodeBlob::set_offset_and_size(int32_t file_offset, int32_t code_size) {
  _file_offset = file_offset; 
  _code_size = code_size;
}

/*
 * initialize the below global table
 */
void CodeReviveMerge::init(Arena* arena) {
  _global_meta_space = new CodeReviveMetaSpace(false);
  const char* appcp = Arguments::get_appclasspath();
  int rp_len = FileMapInfo::num_paths(appcp);
  if (rp_len != 0) {
    _cp_array = ClassPathEntryTable::create_path_array(appcp, INT_MAX);
    if (_cp_array == NULL) {
      CR_LOG(cr_merge, cr_none, "Fail to create path array during merging\n");
      exit(1);
    }
    _cp_array_size = _cp_array->length();
    if (CodeRevive::merge_wildcard_classpath() != NULL) {
      _sorted_jars_in_wildcards = new GrowableArray<const char*>();
      _merged_cp_array = ClassPathEntryTable::create_merged_path_array(CodeRevive::merge_wildcard_classpath(), _sorted_jars_in_wildcards);
    }
  }
}

void CodeReviveMerge::merge_and_dump(TRAPS) {
  ResourceMark rm;

  // get all the csa files
  if (!acquire_input_files()) {
    exit(1);
  }

  Arena merge_arena(mtInternal);
  _arena = &merge_arena;

  // initialize
  init(_arena);

  GrowableArray<CodeReviveContainer*>* containers = check_and_group_files_by_fingerprint();
  if (containers->is_empty()) {
    exit(1);
  }

  // remove containers with fewer files based on coverage and container maximum
  remove_containers(containers);

  // create klass resolve cache table for resolve metaspace
  MergePhaseKlassResovleCacheTable::create_table();

  {
    // traverse all the csa files and collect the candidate nmethod
    CodeReviveContainer* container = NULL;
    for (int i = 0; i < containers->length(); i++) {
      HandleMark   hm;
      ResourceMark rm;
      // each container has own arena
      // after determine the candidate codeblob, the arena is released
      Arena container_arena(mtInternal);

      container = containers->at(i);

      // collect the jit meta info in container 
      CR_LOG(cr_merge, cr_trace, "Preprocess candidate nmethods for container %d\n", i);
      preprocess_candidate_nmethods(container, &container_arena);

      print_candidate_info_in_container(container, i);

      // determine the candidate codeblob in container
      CR_LOG(cr_merge, cr_trace, "Determine merged code for container %d\n", i);
      determine_merged_code_for_container(container, _arena);

      print_merged_code_info_in_container(container, i);
    }
  }

  // print the klass resolve information
  MergePhaseKlassResovleCacheTable::print_cache_table_information();

  CodeReviveMetaSpace::print_resolve_info();

  // save
  {
    CodeReviveFile output_file;
    if (output_file.save_merged(CodeRevive::file_path(), containers)) {
      CR_LOG(cr_merge, cr_none, "Succeed to generate merged CSA file %s\n", CodeRevive::file_path());
      output_file.print_file_info();
    }
  }

  exit(0);
}

bool CodeReviveMerge::acquire_input_files() {
  char* input_path = CodeRevive::input_files();
  char* input_list_file = CodeRevive::input_list_file();
  if (input_path == NULL && input_list_file == NULL) {
    jio_fprintf(defaultStream::error_stream(),
                "No input CSA files specified with -XX:CodeReviveOptions:input_list_file= or -XX:CodeReviveOptions:input_files=\n");
    return false;
  }

  // parse _input_files
  _csa_filenames = new GrowableArray<char*>();
  FilePathWalker walker(_csa_filenames, input_path, input_list_file);
  if (_csa_filenames->is_empty()) {
    jio_fprintf(defaultStream::error_stream(), "No input CSA files\n");
    return false;
  }
  return true;
}

/*
 * Check if CSA files are compatible with current JVM and given class path.
 *
 * Group valid CSA files by fingerprint (JVM options/configurations), containers
 * with same fingerprint means all configurations affect JIT compilation are same.
 *
 * Grouped result:
 * 1. Each group has a CodeReviveContainer: with an unique fingerprint
 * 2. Each CodeReviveContainer records all CSA files with same fingerprint
 */
GrowableArray<CodeReviveContainer*>* CodeReviveMerge::check_and_group_files_by_fingerprint() {
  GrowableArray<CodeReviveContainer*>* result = new GrowableArray<CodeReviveContainer*>;
  // traverse input files
  for (int i = 0; i < _csa_filenames->length(); i++) {
    char* name = _csa_filenames->at(i);
    CodeReviveFile* file = new CodeReviveFile();
    if (!file->map(name, true /* load container */, false /* init metaspace */, false /* select container */,
                   true /* check class path*/, cr_merge)) {
      CR_LOG(cr_merge, cr_fail, "Fail to merge file %s\n", name);
      delete file;
      continue;
    }
    // find container with the same fingerprint
    CodeReviveContainer* match = NULL;
    for (int j = 0; j < result->length(); j++) {
      CodeReviveContainer* container = result->at(j);
      if (container->has_same_fingerprint(file->container())) {
        match = container;
        break;
      }
    }
    if (match == NULL) {
      match = new CodeReviveContainer(_global_meta_space); // no writing for new mergeing container now
      match->init_for_merge(file->container(), _arena);
      result->append(match);
    }
    match->add_filename_index(i);
    delete file;
  }
  if (result->is_empty()) {
    CR_LOG(cr_merge, cr_none, "No compatible CSA file after fingerprint check\n");
  }
  return result;
}

void CodeReviveMerge::preprocess_candidate_nmethods(CodeReviveContainer* container, Arena* arena) {
  char* csa_file_name = NULL;
  GrowableArray<int>* file_indexes = container->file_indexes();
  container->init_jit_metas(arena);
  // traverse all the csa files
  for (int i = 0; i < file_indexes->length(); i++) {
    int index = file_indexes->at(i);
    csa_file_name = _csa_filenames->at(index);
    CodeReviveFile* csa_file = new CodeReviveFile();
    if (!csa_file->map(csa_file_name, true, true, false, false, cr_merge)) {
      CR_LOG(cr_merge, cr_fail, "Fail to merge file %s\n", csa_file_name);
      delete csa_file;
      continue;
    }
    
    ResourceMark rm;
    // collect the candidate method
    CodeReviveLookupTable* lookup_table = csa_file->lookup_table();
    CodeReviveCodeSpace*   code_space   = csa_file->code_space();
    CodeReviveMetaSpace*   meta_space   = csa_file->meta_space();

    // resolve all the meta in meta space
    meta_space->resolve_metadata(_global_meta_space);

    // iterate lookup table and find the candidate code
    CodeReviveLookupTableIterator iter(lookup_table);
    for (CodeReviveLookupTable::Entry* e = iter.first(); e != NULL; e = iter.next()) {
      if (e->_code_offset == -1) {
        continue;
      }
      char* names[3];
      int32_t* lens;
      Method* m = meta_space->unresolved_name_parts_or_method(e->_meta_index, names, &lens);
      if (m == NULL) {
        CR_LOG(cr_merge, cr_warning, "Unresolved method %s\n", names[0]);      
        continue;
      }
      CodeReviveCodeBlob cr_cb(code_space->get_code_address(e->_code_offset), meta_space);

      // check whether the code blob can be merged
      if (cr_cb.can_be_merged()) {
        Metadata* meta = meta_space->resolved_metadata_or_null(e->_meta_index);
        guarantee(meta != NULL, "Unresolved metadata");
        // collect jit code meta of all code in all file
        add_candidate_code_into_jit_metas(csa_file, &cr_cb, meta->csa_meta_index(), container, index, arena);
      } else {
        CR_LOG(cr_merge, cr_warning, "Fail to merge method %s\n", m->name_and_sig_as_C_string());
      }
    }
    // release CodeReviveFile
    delete csa_file;
  }
  container->reset_file_indexes();
}

void CodeReviveMerge::update_code_blob_info(CodeReviveMetaSpace* meta_space, char* start, int32_t next_offset) {
  CodeReviveCodeBlob cb(start, meta_space);

  // set the offset for next code blob
  cb.set_next_version_offset(next_offset);

  UpdateMetaIndexTask meta_array_task(cb.aux_meta_array_begin(), meta_space);
  meta_array_task.update_meta_space_index();

  UpdateMetaIndexTask oop_array_task(cb.aux_oop_array_begin(), meta_space);
  oop_array_task.update_meta_space_index();

  UpdateMetaIndexTask reloc_info_task(cb.aux_reloc_begin(), meta_space);
  reloc_info_task.update_meta_space_index();
}

void CodeReviveMerge::add_candidate_code_into_jit_metas(CodeReviveFile* file, CodeReviveCodeBlob* codeblob,
                                                        int32_t meta_index, CodeReviveContainer* container,
                                                        int32_t file_index, Arena *arena) {
  CR_LOG(cr_merge, cr_info, "Add candidate code %s\n", _global_meta_space->metadata_name(meta_index));
  guarantee(meta_index >= 0, "should be");
  CandidateCodeBlob candidate(file_index, meta_index);

  GrowableArray<JitMetaInfo*>* jit_metas = container->jit_metas();
  CodeReviveMetaSpace* meta_space = file->meta_space();
  CodeReviveCodeBlob cb(codeblob->start(), meta_space);

  // get global metadata indexs for klasses and methods in current candidate's data array
  GrowableArray<int32_t>* opt_meta_global_indexes = new GrowableArray<int32_t>();
  CollectKlassAndMethodIndexTask collect_meta_task(cb.aux_meta_array_begin(), candidate._meta_index, opt_meta_global_indexes, meta_space);
  collect_meta_task.iterate_reloc_aux_info();

  CollectKlassAndMethodIndexTask collect_oop_task(cb.aux_oop_array_begin(), candidate._meta_index, opt_meta_global_indexes, meta_space);
  collect_oop_task.iterate_reloc_aux_info();

  // get all opt records and revive deps
  GrowableArray<OptRecord*>* opts = cb.get_opt_records_for_merge(_global_meta_space, opt_meta_global_indexes);

  GrowableArray<ReviveDepRecord*>* deps = cb.get_revive_deps(opt_meta_global_indexes);

  // set file_offset and size in CandidateCodeBlob
  candidate.set_offset_and_size(codeblob->start() - file->start(), cb.size());

  // update JitOptRecords statistics information
  bool found = false;
  for (int i = 0; i < jit_metas->length(); i++) {
    if (jit_metas->at(i)->_method_index == candidate._meta_index) {
      found = true;
      jit_metas->at(i)->add(opts, deps, candidate, arena);
      break;
    }
  }

  // create a new jit meta for new jit code
  if (!found) {
    JitMetaInfo* jmi = new (arena) JitMetaInfo(candidate._meta_index, arena);
    jmi->add(opts, deps, candidate, arena);
    jit_metas->append(jmi);
  }
}

void CodeReviveMerge::determine_merged_code_for_container(CodeReviveContainer* container, Arena* arena) {
  GrowableArray<JitMetaInfo*>* jit_metas = container->jit_metas();

  // log before selection
  CR_LOG(cr_merge, cr_info, "Jit Version Dump Before Selection:\n");
  print_jit_versions(jit_metas);

  // final logic to choose candidate nmthod
  candidate_selection(container, arena);

  // log after selection();
  CR_LOG(cr_merge, cr_info, "Jit Version Dump After Selection:\n");
  print_jit_versions(jit_metas);

  container->reset_jit_metas();
}

void CodeReviveMerge::print_jit_versions(GrowableArray<JitMetaInfo*>* jit_metas) {
  // log log
  if (CodeRevive::is_log_on(cr_merge, cr_info)) {
    ResourceMark rm;
    for (int i = 0; i < jit_metas->length(); i++) {
      JitMetaInfo* jmi = jit_metas->at(i);
      CodeRevive::out()->print_cr("method %s, count %d",  _global_meta_space->metadata_name(jmi->_method_index), jmi->_count);
      for (int j = 0; j < jmi->_versions->length(); j++) {
        JitVersion* jv = jmi->_versions->at(j);
        CodeRevive::out()->print_cr("  version %d: id %d, count %d, offset %d, opt count %d, deps count %d:",
          j, jv->_uniq_id, jv->_count, jv->_candidate._file_offset,
          jv->_opts->length(), jv->_deps->length());
        for (int k = 0; k < jv->_opts->length(); k++) {
          CodeRevive::out()->print("    opt record %d: ", k);
          jv->_opts->at(k)->print_on(CodeRevive::out(), 0);
        }
        for (int k = 0; k < jv->_deps->length(); k++) {
          CodeRevive::out()->print("    dep record %d: ", k);
          jv->_deps->at(k)->print_on(CodeRevive::out(), 0);
        }
      }
    }
  }
}

// simple selection : select high frequency jit versions
void CodeReviveMerge::candidate_selection(CodeReviveContainer* container, Arena* arena) {
  GrowableArray<MergedCodeBlob*>* candidate_codeblobs = container->candidate_codeblobs();
  GrowableArray<JitMetaInfo*>* jit_metas = container->jit_metas();
  size_t offset_in_codespace = 0;
  if (CodeRevive::merge_policy() == CodeRevive::M_COVERAGE) {
    CR_LOG(cr_merge, cr_trace, "Use coverage base version select policy\n");
  } else {
    CR_LOG(cr_merge, cr_trace, "Use simple version select policy\n");
  }
  guarantee(candidate_codeblobs->length() == 0, "candidate_codeblobs should be empty.");
  for (int i = 0; i < jit_metas->length(); i++) {
    JitMetaInfo* jmi = jit_metas->at(i);
    guarantee(jmi->_count > 0, "must be");
    jmi->select_best_versions();
    for (int j = 0; j < jmi->_versions->length(); j++) {
      JitVersion* jv = jmi->_versions->at(j);
      MergedCodeBlob* mcb = new (arena) MergedCodeBlob(jv->_candidate._file_index, jmi->_method_index,
                                               jv->_candidate._file_offset, jv->_candidate._code_size,
                                               (int)offset_in_codespace);
      offset_in_codespace += align_up((size_t)mcb->code_size(), CodeReviveFile::alignment());
      add_merged_codeblob(mcb, candidate_codeblobs);
    }
  }
  // estimate the size of container:
  size_t lookup_table_size = sizeof(CodeReviveLookupTable::Entry) * jit_metas->length(); 
  container->compute_estimated_size(offset_in_codespace, lookup_table_size); 
}

// add the MergedCodeBlob into candidate_codeblobs
void CodeReviveMerge::add_merged_codeblob(MergedCodeBlob* mcb, GrowableArray<MergedCodeBlob*>* candidate_codeblobs) {
  // check whether there is a MergeCodeBlob in the same csa file
  for (int i = 0; i < candidate_codeblobs->length(); i++) {
    MergedCodeBlob* collected_mcb = candidate_codeblobs->at(i);
    if (collected_mcb->_file_index == mcb->_file_index) {
      mcb->_next = collected_mcb;
      candidate_codeblobs->at_put(i, mcb);
      return;
    }
  }
  candidate_codeblobs->append(mcb);
}

void CodeReviveMerge::print_candidate_info_in_container(CodeReviveContainer* container, int index) {
  outputStream* output = CodeRevive::out();
  GrowableArray<JitMetaInfo*>* jit_metas = container->jit_metas();
  output->print_cr("Fingerprint of container %d", index);
  container->fingerprint()->print(output);
  output->print_cr("Method info in container %d before merge", index);
  output->print_cr("  Number of method      : %d", jit_metas->length());
  int max_version_count = 0;
  int version_count = 0;
  int total_candidate = 0;
  for (int i = 0; i < jit_metas->length(); i++) {
    JitMetaInfo* jitMeta = jit_metas->at(i);
    int version_number = jitMeta->version_number();
    total_candidate += jitMeta->count();
    if (version_number > max_version_count) {
      max_version_count = version_number;
    }
    version_count += version_number;
  }
  output->print_cr("  Number of candidate   : %d", total_candidate);
  output->print_cr("  Number of jit version : %d", version_count);
  output->print_cr("  Max of jit version    : %d", max_version_count);
} 

void CodeReviveMerge::print_merged_code_info_in_container(CodeReviveContainer* container, int index) {
  outputStream* output = CodeRevive::out();
  GrowableArray<MergedCodeBlob*>* candidate_codeblobs = container->candidate_codeblobs();
  output->print_cr("Method info in container %d after merge", index);
  int total_nmethods = 0;
  for (int i = 0; i < candidate_codeblobs->length(); i++) {
    MergedCodeBlob* mcb = candidate_codeblobs->at(i);
    while (mcb != NULL) {
      mcb = mcb->next();
      total_nmethods++;
    }
  }
  output->print_cr("  Number of CSA files         : %d", candidate_codeblobs->length());
  output->print_cr("  Number of merged nmethods   : %d", total_nmethods);
}

GrowableArray<OptRecord*>* CodeReviveMerge::duplicate_opt_record_array_in_arena(GrowableArray<OptRecord*>* opts, Arena* arena) {
  if (opts == NULL) {
    return NULL;
  }
  GrowableArray<OptRecord*>* new_opts = new (arena) GrowableArray<OptRecord*>(arena, 10, 0, NULL);
  for (int i = 0; i < opts->length(); i++) {
    OptRecord* opt = opts->at(i);
    new_opts->append(opt->duplicate_in_arena(arena));
  }
  return new_opts;
}

GrowableArray<ReviveDepRecord*>* CodeReviveMerge::duplicate_dep_record_array_in_arena(GrowableArray<ReviveDepRecord*>* deps, Arena* arena) {
  if (deps == NULL) {
    return NULL;
  }
  GrowableArray<ReviveDepRecord*>* new_deps = new (arena) GrowableArray<ReviveDepRecord*>(arena, 10, 0, NULL);
  if (deps->length() == 0) {
    return new_deps;
  }
  GrowableArray<int32_t>* origin_meta_array_index = deps->at(0)->get_meta_array_index();
  GrowableArray<int32_t>* meta_array_index = new (arena) GrowableArray<int32_t>(arena, 10, 0, 0);
  for (int i = 0; i < origin_meta_array_index->length(); i++) {
    meta_array_index->append(origin_meta_array_index->at(i));
  }
  for (int i = 0; i < deps->length(); i++) {
    ReviveDepRecord* dep = deps->at(i);
    new_deps->append(dep->duplicate_in_arena(arena, meta_array_index));
  }
  return new_deps;
}

void CodeReviveMerge::remove_containers(GrowableArray<CodeReviveContainer*>* containers) {
  outputStream* output = CodeRevive::out();
  int valid_csa_count = 0;
  for (int i = 0; i < containers->length(); i++) {
    valid_csa_count += containers->at(i)->file_indexes()->length();
  }

  output->print_cr("Remove uncessary containers based on converage %d%%", CodeRevive::coverage());
  output->print_cr("Before:");
  output->print_cr("  Total valid csa file number   : %d", valid_csa_count);
  output->print_cr("  Total container number        : %d", containers->length());

  // compute the expected count of csa file by coverage
  int expected_csa_count = valid_csa_count * CodeRevive::coverage() / 100;
  int candidate_csa_count = 0;
  int reserved_index;

  // sort the containers using the file number in each container
  containers->sort(CodeReviveContainer::compare_by_count);
  
  for (reserved_index = 0; reserved_index < containers->length(); reserved_index++) {
    // if the number of candidate files exceeds the expected number of csa files
    // or the number of containers exceeds the maximum of container,
    // remove the remaining containers
    candidate_csa_count += containers->at(reserved_index)->file_indexes()->length();
    if (candidate_csa_count >= expected_csa_count || reserved_index >= CodeRevive::max_container_count()) {
      break;
    }
  }
  // remove the container
  for (int i = containers->length() - 1; i > reserved_index; i--) {
    CodeReviveContainer* container = containers->at(i); 
    containers->remove_at(i);
  }

  output->print_cr("After:");
  output->print_cr("  Total valid csa file number   : %d", expected_csa_count);
  output->print_cr("  Total container number        : %d", containers->length());
}
