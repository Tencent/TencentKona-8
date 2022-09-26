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
#include "classfile/classLoaderExt.hpp"
#include "cr/classPathEntryTable.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/codeReviveCodeBlob.hpp"
#include "cr/codeReviveMerge.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "cr/revive.hpp"
#include "memory/filemap.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/align.hpp"
#include "cr/revive.hpp"
#include "runtime/arguments.hpp"
#include "runtime/handles.inline.hpp"

#ifdef TARGET_OS_FAMILY_linux
#include <stdlib.h>
#include <limits.h>
#endif

#ifndef _WINDOWS 
#define MAX_BUFFER_SIZE PATH_MAX
#else
#define MAX_BUFFER_SIZE MAX_PATH
#endif

static size_t class_path_entry_size = sizeof(ClassPathEntryTable::ClassPathEntry);

static const char* canonical_path(const char* orig, char* buf) {
#ifdef TARGET_OS_FAMILY_linux
  if (realpath(orig, buf) == buf) { // return buf means success
    return buf;
  }
#else
  fatal("NYI");
#endif
  return orig;
}

static char* get_canonical_path(const char* orig, char* buf) {
  const char* real_path = canonical_path(orig, buf);
  size_t path_len = strlen(real_path) + 1;
  char* new_path = NEW_RESOURCE_ARRAY(char, path_len);
  strncpy(new_path, real_path, path_len);
  return new_path;
}

static int compare_jarfile_name(const char** str1, const char** str2) {
  return strcmp(*str1, *str2);
}

ClassPathEntryTable::ClassPathEntryTable(char* start) {
  _start = start;
  _header = (ClassPathEntryTableHeader*)_start;
  _classpath_entry_table = ((char*)_header) + align_up(sizeof(ClassPathEntryTableHeader), CodeReviveFile::alignment());
  _cur_pos = _classpath_entry_table; // start writing from _classpath_entry_table start if necessary
}

// must invoke after map or finish setup
size_t ClassPathEntryTable::size() {
  return align_up(sizeof(ClassPathEntryTableHeader), CodeReviveFile::alignment()) +
         _header->_classpath_entry_table_size +
         _header->_classpath_size;
}

static ClassPathEntryTable::ClassPathEntry* shared_classpath(char* entry_table, int index) {
  char* p = entry_table;
  p += class_path_entry_size * index; 
  return (ClassPathEntryTable::ClassPathEntry*)p;
}

/*
 * store ClassPathEntry in csa file
 */
bool ClassPathEntryTable::setup_classpath_entry_table() {
  ResourceMark rm;
  char* strptr = NULL;

  // get application class path
  const char* appcp = Arguments::get_appclasspath();

  // get the entry array that includes the existing files in class path
  GrowableArray<const char*>* classpath_array = create_path_array(appcp, INT_MAX);

  int num_app_classpath_entries = classpath_array->length();
  strptr = _cur_pos + num_app_classpath_entries * class_path_entry_size;
  char buffer[MAX_BUFFER_SIZE];
  for (int cur_entry = 0; cur_entry < classpath_array->length(); cur_entry++) {
    const char* name = classpath_array->at(cur_entry);
    name = canonical_path(name, buffer);
    size_t name_bytes = strlen(name) + 1;
    ClassPathEntry* ent = shared_classpath(_classpath_entry_table, cur_entry);
    // get the file stat
    struct stat st;
    if (os::stat(name, &st) != 0) {
      // the name should be exist, since it passes the check in FileMapInfo::create_path_array;
      CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail in classpath setup: File %s doesn't exist.\n", name);
      return false;
    }

    // store timestamp and filesize
    ent->_timestamp = st.st_mtime;
    ent->_filesize  = st.st_size;
    // store offset
    ent->_name_offset = (int32_t) (strptr - _cur_pos);
    strncpy(strptr, name, name_bytes); // name_bytes includes trailing 0.
    strptr += name_bytes;
  }
  _header->_classpath_entry_table_count = num_app_classpath_entries;
  _header->_classpath_entry_table_size = strptr - _classpath_entry_table;

  // record application class path
  _header->_classpath_size = strlen(appcp) + 1;
  strncpy(strptr, appcp, _header->_classpath_size);
  _header->_classpath_offset = strptr - _cur_pos;
  return true;
}

/*
 * store ClassPathEntry in csa file during merge stage
 */
bool ClassPathEntryTable::setup_merged_classpath_entry_table() {
  ResourceMark rm;
  char* strptr = NULL;

  // get application class path
  const char* appcp = Arguments::get_appclasspath();

  // get the entry array that includes the existing files in class path
  GrowableArray<const char*>* classpath_array = CodeReviveMerge::cp_array();
  GrowableArray<WildcardEntryInfo*>* merged_cp_array = CodeReviveMerge::merged_cp_array(); 

  int num_app_classpath_entries = classpath_array->length();
  strptr = _cur_pos + num_app_classpath_entries * class_path_entry_size;
  char buffer[MAX_BUFFER_SIZE];

  int cur_wildcard_entry = 0;
  int num_of_jar = 0;
  WildcardEntryInfo* wildcard_entry = NULL;
  if (merged_cp_array != NULL) {
    wildcard_entry = merged_cp_array->at(cur_wildcard_entry);
    num_of_jar = wildcard_entry->num_of_jar();
  } 

  for (int i = 0; i < classpath_array->length(); i++) {
    const char* name = classpath_array->at(i);
    name = canonical_path(name, buffer);
    size_t name_bytes = strlen(name) + 1;
    ClassPathEntry* ent = shared_classpath(_classpath_entry_table, i);
    // get the file stat
    struct stat st;
    if (os::stat(name, &st) != 0) {
      // the name should be exist, since it passes the check in FileMapInfo::create_path_array;
      CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail in classpath setup: File %s doesn't exist.\n", name);
      return false;
    }

    // store timestamp and filesize
    ent->_filesize  = st.st_size;

    // the merge wildcard classpath in revive option is specified
    if (merged_cp_array != NULL) {
      // num_of_jar = 0 means that the current entry has been checked, and need the next entry
      if (num_of_jar == 0) {
        cur_wildcard_entry++;
        if (cur_wildcard_entry < merged_cp_array->length()) {
          wildcard_entry = merged_cp_array->at(cur_wildcard_entry);
          num_of_jar = wildcard_entry->num_of_jar();
        } else {
          wildcard_entry = NULL;
          num_of_jar = -1;
        }
      }
      // the entry is under dir/*
      if (wildcard_entry != NULL && wildcard_entry->is_wildcard()) {
        if (strncmp(name, wildcard_entry->path_name(), strlen(wildcard_entry->path_name())) != 0) {
          // The directory where the jar file is located does not match the directory with wildcard
          CR_LOG(CodeRevive::log_kind(), cr_fail, "The directory of jar file %s isn't the same as the directory with wildcard %s.\n", name, wildcard_entry->path_name());
          return false;
        } 
        ent->_timestamp = 0;
      } else {
        ent->_timestamp = st.st_mtime;
      }
      num_of_jar--;
    } else {
      ent->_timestamp = st.st_mtime;
    }
    // store offset
    ent->_name_offset = (int32_t) (strptr - _cur_pos);
    strncpy(strptr, name, name_bytes); // name_bytes includes trailing 0.

    strptr += name_bytes;
  }
  _header->_classpath_entry_table_count = num_app_classpath_entries;
  _header->_classpath_entry_table_size = strptr - _classpath_entry_table;

  // record application class path
  _header->_classpath_size = strlen(appcp) + 1;
  strncpy(strptr, appcp, _header->_classpath_size);
  _header->_classpath_offset = strptr - _cur_pos;
  return true;
}


bool ClassPathEntryTable::validate() {
  if (!CodeRevive::validate_check()) {
    return true;
  }
  int shared_app_paths_len = _header->_classpath_entry_table_count;
  if (shared_app_paths_len > 0) {
    if (CodeRevive::is_merge()) {
      return validate_app_class_paths_for_merge(shared_app_paths_len);
    } else {
      return validate_app_class_paths(shared_app_paths_len);
    }
  }
  return true;
}

/*
 * Check whether the file is the same as the one in save/merge
 */
bool ClassPathEntryTable::validate_file(ClassPathEntry* ent, char* file_name, bool check_timestamp) {
  struct stat st;
  if (os::stat(file_name, &st) != 0) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail in classpath check: File %s doesn't exist.\n", file_name);
    return false;
  }
  if ((check_timestamp && ent->_timestamp != 0 && ent->_timestamp != st.st_mtime) || (ent->_filesize != st.st_size)) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail in classpath check: File %s is changed.\n", file_name);
    return false;
  }
  return true;
}

/*
 * check the consistency for classpath between save and restore
 * 1. check size: the length in merge should be longer than the length in save
 * 2. check the order in classpath
 * 3. check the timestamp and size for each file
 */
bool ClassPathEntryTable::validate_app_class_paths(int shared_app_paths_len) {
  const char* appcp = Arguments::get_appclasspath();
  int rp_len = FileMapInfo::num_paths(appcp);
  bool mismatch = false;
  if (rp_len < shared_app_paths_len) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, 
           "Run time APP classpath is shorter than the one at dump time. \nActual = %s\nExpected = %s\n", 
           appcp, _classpath_entry_table + _header->_classpath_offset);
    return false;
  }
  if (shared_app_paths_len != 0 && rp_len != 0) {
    // Prefix is OK: E.g., dump with -cp foo.jar, but run with -cp foo.jar:bar.jar.
    ResourceMark rm;
    // the max number of files in classpath is the number in the classpath of csa file
    GrowableArray<const char*>* rp_array = create_path_array(appcp, shared_app_paths_len);
    int array_size = rp_array->length();
    if (array_size == 0) {
      return false;
    }
    if (array_size < shared_app_paths_len) {
      return false;
    }
    mismatch = check_paths(shared_app_paths_len, rp_array);
  }
  return !mismatch;
}

// copy from filemap.cpp for implementing create_path_array
class CodeReviveClasspathStream : public StackObj {
  const char* _class_path;
  int _len;
  int _start;
  int _end;

public:
  CodeReviveClasspathStream(const char* class_path) {
    _class_path = class_path;
    _len = (int)strlen(class_path);
    _start = 0;
    _end = 0;
  }

  bool has_next() {
    return _start < _len;
  }

  char* get_next();
};

char* CodeReviveClasspathStream::get_next() {
  while (_class_path[_end] != '\0' && _class_path[_end] != os::path_separator()[0]) {
    _end++;
  }
  int path_len = _end - _start;
  char* path = NEW_RESOURCE_ARRAY(char, path_len + 1);
  strncpy(path, &_class_path[_start], path_len);
  path[path_len] = '\0';

  while (_class_path[_end] == os::path_separator()[0]) {
    _end++;
  }
  _start = _end;
  return path;
}


GrowableArray<const char*>* ClassPathEntryTable::create_path_array(const char* paths, int max_size) {
  GrowableArray<const char*>* path_array = new GrowableArray<const char*>(10);
  char buffer[MAX_BUFFER_SIZE];
  size_t path_len = 0;
  char* new_path = NULL;
  int n = 0;
  Thread* THREAD = Thread::current();
  CodeReviveClasspathStream cp_stream(paths);
  while (cp_stream.has_next()) {
    const char* path = cp_stream.get_next();
    struct stat st;
    if (os::stat(path, &st) == 0) {
      // check whether the path is directory
      // save stage:
      //   check whether there are some classes reading from the directory
      //   if yes, the subsequent files or directory are ignored.
      // restore stage:
      //   add the directories into hash table
      //   the directory should be before the expected jar file
      // merge stage:
      //   the directories are forbidden.

      if (!CodeRevive::disable_check_dir() && ((st.st_mode & S_IFMT) == S_IFDIR)) {
        if (CodeRevive::is_save()) {
          // get the real path
          path = canonical_path(path, buffer);
          // if there is classes reading from the path, the remaining paths will be ignored.
          if (CodeRevive::read_class_from_path(path, strlen(path), THREAD)) {
            CR_LOG(cr_save, cr_trace, "Load class from directory: %s\n", path);
            break;
          }
        } else if (CodeRevive::is_restore()) {
          // add the real path into directory hash table
          path = canonical_path(path, buffer);
          DirWithClassEntry* entry = DirWithClassTable::lookup(path, strlen(path), true, THREAD);
          entry->set_in_classpath(true); 
        } else if (CodeRevive::is_merge()) {
          CR_LOG(cr_merge, cr_fail,
                 "The class path can't have directory at merge time: %s\n", paths);
          return NULL;
        }
        continue;
      }
      if ((st.st_mode & S_IFREG) == S_IFREG) {
        // get the real path and put into path array
        new_path = get_canonical_path(path, buffer);
        path_array->append(new_path);
        // if number of files exceeds the maximum number of files
        // the remaining files in class path won't be compared.
        if (++n >= max_size) {
          break;
        }
      }
    }
  }
  return path_array;
}


static int is_jar_file_name(const char *filename)
{
  int len = (int) strlen(filename);
  return (len >= 4) &&
         (filename[len - 4] == '.') &&
         (strcmp(filename + len - 3, "jar") == 0 ||
          strcmp(filename + len - 3, "JAR") == 0);
}

static char *wildcard_concat(const char *wildcard, const char *basename)
{
  size_t wildlen = strlen(wildcard);
  size_t baselen = strlen(basename);
  char *filename = NEW_RESOURCE_ARRAY(char, wildlen + baselen + 2);
  /* Replace the trailing '*' with basename */
  memcpy(filename, wildcard, wildlen);
  filename[wildlen] = os::file_separator()[0];
  memcpy(filename + wildlen + 1, basename, baselen + 1);
  return filename;
}

static int compute_number_of_jar_in_dir(const char* name, GrowableArray<const char*>* jar_files) {
  char* new_path = NULL;
  int number_of_jar = 0;
  DIR *dir = NULL;
  if (!(dir = os::opendir(name))) {
    CR_LOG(cr_merge, cr_fail, "Fail to open dir %s\n", dir);
    return number_of_jar;
  }

  struct dirent* dp = NULL;
  while((dp = os::readdir(dir))) {
    if (is_jar_file_name(dp->d_name)) {
      new_path = wildcard_concat(name, dp->d_name);
      jar_files->append(new_path);
      number_of_jar++;
    }
  }

  os::closedir(dir);
  return number_of_jar;
}

GrowableArray<WildcardEntryInfo*>* ClassPathEntryTable::create_merged_path_array(const char* paths,
                                                                                 GrowableArray<const char*>* jar_files) {
  GrowableArray<WildcardEntryInfo*>* path_array = new GrowableArray<WildcardEntryInfo*>(10);
  char buffer[MAX_BUFFER_SIZE];
  char* new_path = NULL;
  WildcardEntryInfo* entry = NULL;
  int n = 0;
  CodeReviveClasspathStream cp_stream(paths);
  while (cp_stream.has_next()) {
    char* path = cp_stream.get_next();
    size_t len = strlen(path);
    // the path end with wildcard
    if (path[len - 1] == '*' && (len == 1 || path[len - 2] == os::file_separator()[0])) {
      if (len == 1) {
        CR_LOG(cr_merge, cr_fail, "The path %s isn't supported at merge time\n", path);
        return NULL;
      }
      // compute the number of jar files
      path[len - 1] = '\0';
      new_path = get_canonical_path(path, buffer);
      int num_of_jar = compute_number_of_jar_in_dir(new_path, jar_files);
      // if there is no jar file, ignore the path
      if (num_of_jar != 0) {
        entry = new WildcardEntryInfo(new_path, true);
        entry->set_num_of_jar(num_of_jar);
        path_array->append(entry);
      } else {
        CR_LOG(cr_merge, cr_trace, "There is no jar file in the path: %s\n", path);
      }
    } else {
      struct stat st;
      if (os::stat(path, &st) == 0) {
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
          CR_LOG(cr_merge, cr_fail,
                 "The class path can't have directory at merge time: %s\n", paths);
          return NULL;
        }
        if ((st.st_mode & S_IFREG) == S_IFREG) {
          new_path = get_canonical_path(path, buffer);
          entry = new WildcardEntryInfo(new_path, false);
          path_array->append(entry);
        }
      }
    }
  }
  jar_files->sort(compare_jarfile_name);
  return path_array;
}

/*
 * check the consistency for classpath between save and merge
 * 1. check size: the length in merge should be shorter than the length in save 
 * 2. check the order in classpath
 * 3. check the timestamp and size for each file
 */
bool ClassPathEntryTable::validate_app_class_paths_for_merge(int csa_app_paths_len) {
  int merge_app_paths_len = CodeReviveMerge::cp_array_size();
  GrowableArray<const char*>* merge_app_path_array = CodeReviveMerge::cp_array();
  GrowableArray<WildcardEntryInfo*>* merged_cp_array = CodeReviveMerge::merged_cp_array();

  bool mismatch = false;
  if (merge_app_paths_len > csa_app_paths_len) {
    CR_LOG(cr_merge, cr_fail, 
           "Merge time APP classpath is longer than the path in csa file. \nActual = %s\nExpected = %s\n",
            Arguments::get_appclasspath(), _classpath_entry_table + _header->_classpath_offset);
    return false;
  }
  if (csa_app_paths_len != 0 && merge_app_paths_len != 0) {
    // Prefix is OK: E.g., dump with -cp foo.jar:bar.jar, but merge with -cp foo.jar
    ResourceMark rm;
    if (merged_cp_array != NULL) {
      mismatch = check_paths_with_wildcard(merge_app_paths_len, CodeReviveMerge::sorted_jars_in_wildcards(), merged_cp_array);
    } else {
      mismatch = check_paths(merge_app_paths_len, merge_app_path_array);
    }
  }
  return !mismatch;
}

// check whether the jar files in wildcard directory are the same as the jar files in class path
static bool compare_jar_array(GrowableArray<const char*>* csa_jarfiles, GrowableArray<const char*>* cp_jarfiles) {
  csa_jarfiles->sort(compare_jarfile_name);
  cp_jarfiles->sort(compare_jarfile_name);
  for (int i = 0; i < csa_jarfiles->length(); i++) {
    if (strcmp(csa_jarfiles->at(i), cp_jarfiles->at(i)) != 0) {
      CR_LOG(CodeRevive::log_kind(), cr_fail, "APP classpath mismatch: file in csa = %s, file in classpath = %s\n",
                                               csa_jarfiles->at(i), cp_jarfiles->at(i));
      return false;
    } 
  }
  return true;
}

/*
 * check the order for classpath between save and restore
 * 1. need to check the order:
 *    compare the jar file name
 *    compare the timestamp and file size
 * 2. ignore the order:
 *    compare whether they are in the same directory
 *    compare the file size
 *    compare whether there are the same files in the directory
 */
bool ClassPathEntryTable::check_paths(int num_paths, GrowableArray<const char*>* rp_array) {
  int i = 0;
  bool mismatch = false;
  GrowableArray<const char*>* jarfiles_in_csa = NULL;
  GrowableArray<const char*>* jarfiles_in_dir_with_wildcard = NULL;
  bool in_directory_with_wildcard = false;

  while (i < num_paths && !mismatch) {
    if (i >= _header->_classpath_entry_table_count) {
      break;
    }
    ClassPathEntry* ent = shared_classpath(_classpath_entry_table, i);
    char* save_name = _classpath_entry_table + ent->_name_offset;
    const char* app_name = rp_array->at(i);
    // need to check the order in class path
    if (ent->need_order_check() == true) {
      if (strcmp(save_name, app_name) != 0) {
        CR_LOG(CodeRevive::log_kind(), cr_fail, "APP classpath mismatch: file in csa = %s, file in classpath = %s\n", save_name, rp_array->at(i));
        mismatch = true;
        break;
      }
    } else {
      // 1. compare whether the directory is the same
      char* save_ptr = strrchr(save_name, '/');
      size_t save_dir_len = save_ptr - save_name;
      const char* app_ptr = strrchr(app_name, '/');
      size_t app_dir_len = app_ptr - app_name;
      if (save_dir_len != app_dir_len || strncmp(save_name, app_name, save_dir_len) != 0) {
        CR_LOG(CodeRevive::log_kind(), cr_fail, "APP classpath mismatch: file in csa = %s, file in classpath = %s\n", save_name, rp_array->at(i));
        mismatch = true;
        break;
      }
      // 2. add the jar files into array which is used to compare at the end
      // it means that the entry is the first jar in the directory
      if (in_directory_with_wildcard == false) {
        jarfiles_in_csa = new GrowableArray<const char*>();
        jarfiles_in_dir_with_wildcard = new GrowableArray<const char*>();
        in_directory_with_wildcard = true;
      }
      // add jar file name into array
      jarfiles_in_csa->append(save_name);
      jarfiles_in_dir_with_wildcard->append(app_name);
    }

    if (!validate_file(ent, save_name, true)) {
      CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail to validate file %s\n", save_name);
      mismatch = true;
    }  
    i++;
  }
  // 3. if there are jar files in the directory with wildcard, check whether the recorded files
  // is consistent with the files in the directories with wildcard
  if (!mismatch && in_directory_with_wildcard) {
    if (compare_jar_array(jarfiles_in_csa, jarfiles_in_dir_with_wildcard) == false) {
      mismatch = true;
    }
  }
  if (mismatch) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "APP classpath mismatch. \nActual = %s\nExpected = %s\n",
           Arguments::get_appclasspath(), _classpath_entry_table + _header->_classpath_offset);
  }
  return mismatch;
}

/*
 * check the order for classpath between save and merge with wildcard
 * merge_app_path_array from -cp <>
 * merged_cp_array from -XX:CodeReviveOptions=merge_wildcard_classpath:<>
 */
bool ClassPathEntryTable::check_paths_with_wildcard(int num_paths, GrowableArray<const char*>* sorted_jars_in_wildcards, 
                                                    GrowableArray<WildcardEntryInfo*>* merged_cp_array) {
  int i = 0;
  int cur_wildcard_entry = 0;
  WildcardEntryInfo* wildcard_entry = merged_cp_array->at(cur_wildcard_entry);
  int num_of_jar = wildcard_entry->num_of_jar();
  bool mismatch = false;
  GrowableArray<const char*>* csa_jarfiles = new GrowableArray<const char*>();
  while (i < num_paths && !mismatch) {
    if (i >= _header->_classpath_entry_table_count) {
      break;
    }
    ClassPathEntry* ent = shared_classpath(_classpath_entry_table, i);
    char* save_name = _classpath_entry_table + ent->_name_offset;
    if (wildcard_entry != NULL && num_of_jar == 0) {
      cur_wildcard_entry++;
      wildcard_entry = merged_cp_array->at(cur_wildcard_entry);
      num_of_jar = wildcard_entry->num_of_jar();
    }
    char* path_name = wildcard_entry->path_name();
    // need to check the order in class path
    if (!wildcard_entry->is_wildcard()) {
      // check whether the name of jar is the same
      if (strcmp(save_name, path_name) != 0) {
        CR_LOG(CodeRevive::log_kind(), cr_fail, "APP classpath mismatch: file in csa = %s, file in classpath = %s\n", save_name, path_name);
        mismatch = true;
        break;
      }
    } else {
      // compare whether the directory is the same
      char* save_ptr = strrchr(save_name, '/');
      size_t save_dir_len = save_ptr - save_name;
      size_t app_dir_len = strlen(path_name);
      if (save_dir_len != app_dir_len || strncmp(save_name, path_name, save_dir_len) != 0) {
        CR_LOG(CodeRevive::log_kind(), cr_fail, "APP classpath mismatch: file in csa = %s, path in classpath = %s\n", save_name, path_name);
        mismatch = true;
        break;
      }
      csa_jarfiles->append(save_name);
    }

    if (!validate_file(ent, save_name, false)) {
      CR_LOG(CodeRevive::log_kind(), cr_fail, "Fail to validate file %s\n", save_name);
      mismatch = true;
    }  
    i++;
    num_of_jar--;
  }
  if (!mismatch) {
    csa_jarfiles->sort(compare_jarfile_name);
    for (int i = 0; i < csa_jarfiles->length(); i++) {
      if (strcmp(csa_jarfiles->at(i), sorted_jars_in_wildcards->at(i)) != 0) {
        CR_LOG(CodeRevive::log_kind(), cr_fail, "APP classpath mismatch: file in csa = %s, file in classpath = %s\n",
                                                csa_jarfiles->at(i), sorted_jars_in_wildcards->at(i));
        mismatch = true;
        break;
      }
    } 
  }
  if (mismatch) {
    CR_LOG(CodeRevive::log_kind(), cr_fail, "APP classpath mismatch. \nActual = %s\nExpected = %s\n",
           Arguments::get_appclasspath(), _classpath_entry_table + _header->_classpath_offset);
  }
  return mismatch;
}



void ClassPathEntryTable::print_on(outputStream* out) {
  out->print_cr("ClassPathEntryTable size %d, class path %s",
                _header->_classpath_entry_table_count,
                _classpath_entry_table + _header->_classpath_offset);
  for (int i = 0; i < _header->_classpath_entry_table_count; i++) {
    ClassPathEntry* ent = shared_classpath(_classpath_entry_table, i);
    char* name = _classpath_entry_table + ent->_name_offset;
    out->print_cr("\t%3d %s %d", i, name, (int)ent->_filesize);
  }
}
