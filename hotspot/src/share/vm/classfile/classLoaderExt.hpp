/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_CLASSFILE_CLASSLOADEREXT_HPP
#define SHARE_VM_CLASSFILE_CLASSLOADEREXT_HPP

#include "classfile/classLoader.hpp"

class ClassLoaderExt: public ClassLoader { // AllStatic
public:
  enum SomeConstants {
    max_classpath_index = 0x7fff
  };
private:
#if INCLUDE_CDS
  static char* get_class_path_attr(const char* jar_path, char* manifest, jint manifest_size);
  static void setup_app_search_path(); // Only when -Xshare:dump
  static SharedPathsMiscInfo* shared_paths_misc_info() {
    return _shared_paths_misc_info;
  }
  // index of first app JAR in shared classpath entry table
  static jshort _app_class_paths_start_index;
  // the largest path index being used during CDS dump time
  static jshort _max_used_path_index;

  static bool _has_app_classes;
  static bool _has_ext_classes;
#endif
public:
  CDS_ONLY(static void process_jar_manifest(ClassPathEntry* entry, bool check_for_duplicates);)

  class Context {
    const char* _file_name;
  public:
    Context(const char* class_name, const char* file_name, TRAPS) {
      _file_name = file_name;
    }

    bool check(ClassFileStream* stream, const int classpath_index) {
      return true;
    }

    bool should_verify(int classpath_index) {
      return false;
    }

    instanceKlassHandle record_result(const int classpath_index,
                                      ClassPathEntry* e, instanceKlassHandle result, TRAPS) {
      if (ClassLoader::add_package(_file_name, classpath_index, THREAD)) {
        if (DumpSharedSpaces) {
          result->set_shared_classpath_index(classpath_index);
          result->set_class_loader_type(ClassLoader::BOOT_LOADER);
        }
        return result;
      } else {
        return instanceKlassHandle(); // NULL
      }
    }
  };

  static void add_class_path_entry(const char* path, bool check_for_duplicates,
                                   ClassPathEntry* new_entry) {
    ClassLoader::add_to_list(new_entry);
  }
  static void append_boot_classpath(ClassPathEntry* new_entry);
  static void setup_search_paths() NOT_CDS_RETURN;

  static void init_lookup_cache(TRAPS) {}
  static void copy_lookup_cache_to_archive(char** top, char* end) {}
  static char* restore_lookup_cache_from_archive(char* buffer) {return buffer;}
  static inline bool is_lookup_cache_enabled() {return false;}

  static bool known_to_not_exist(JNIEnv *env, jobject loader, const char *classname, TRAPS) {return false;}
  static jobjectArray get_lookup_cache_urls(JNIEnv *env, jobject loader, TRAPS) {return NULL;}
  static jintArray get_lookup_cache(JNIEnv *env, jobject loader, const char *pkgname, TRAPS) {return NULL;}

#if INCLUDE_CDS
private:
  static char* read_manifest(ClassPathEntry* entry, jint *manifest_size, bool clean_text, TRAPS);

public:
  static char* read_manifest(ClassPathEntry* entry, jint *manifest_size, TRAPS) {
    // Remove all the new-line continuations (which wrap long lines at 72 characters, see
    // http://docs.oracle.com/javase/6/docs/technotes/guides/jar/jar.html#JAR%20Manifest), so
    // that the manifest is easier to parse.
    return read_manifest(entry, manifest_size, true, THREAD);
  }
  static char* read_raw_manifest(ClassPathEntry* entry, jint *manifest_size, TRAPS) {
    // Do not remove new-line continuations, so we can easily pass it as an argument to
    // java.util.jar.Manifest.getManifest() at run-time.
    return read_manifest(entry, manifest_size, false, THREAD);
  }

  static jshort app_class_paths_start_index() { return _app_class_paths_start_index; }

  static jshort max_used_path_index() { return _max_used_path_index; }

  static void set_max_used_path_index(jshort used_index) {
    _max_used_path_index = used_index;
  }

  static bool has_ext_or_app_classes() {
    return _has_app_classes || _has_ext_classes;
  }

  static void record_result(const s2 classpath_index,
                            InstanceKlass* result, TRAPS);

  static void set_has_app_classes() {
    _has_app_classes = true;
  }
  static void set_has_ext_classes() {
    _has_ext_classes = true;
  }
#endif
};

#endif // SHARE_VM_CLASSFILE_CLASSLOADEREXT_HPP
