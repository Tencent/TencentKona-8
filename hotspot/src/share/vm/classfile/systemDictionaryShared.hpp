/*
 * Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.
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


#ifndef SHARE_VM_CLASSFILE_SYSTEMDICTIONARYSHARED_HPP
#define SHARE_VM_CLASSFILE_SYSTEMDICTIONARYSHARED_HPP

#include "oops/klass.hpp"
#include "classfile/dictionary.hpp"
#include "classfile/systemDictionary.hpp"
#include "memory/filemap.hpp"

class SystemDictionaryShared: public SystemDictionary {
private:
  // These _shared_xxxs arrays are used to initialize the java.lang.Package and
  // java.security.ProtectionDomain objects associated with each shared class.
  //
  // See SystemDictionaryShared::init_security_info for more info.
  static objArrayOop _shared_protection_domains;
  static objArrayOop _shared_jar_urls;
  static objArrayOop _shared_jar_manifests;

  static InstanceKlass* load_shared_class_for_builtin_loader(
                                               Symbol* class_name,
                                               Handle class_loader,
                                               TRAPS);
  static Handle get_package_name(Symbol*  class_name, TRAPS);


  // Package handling:
  //
  //    BOOT classes: Reuses the existing JVM_GetSystemPackage(s) interfaces to
  //                  get packages for shared boot classes in unnamed modules.
  //
  //    APP  classes: VM calls ClassLoaders.AppClassLoader::defineOrCheckPackage()
  //                  with with the manifest and url from archived data.
  //
  //    PLATFORM  classes: No package is defined.
  //
  // The following two define_shared_package() functions are used to define
  // package for shared APP and PLATFORM classes.
  static void define_shared_package(Symbol*  class_name,
                                    Handle class_loader,
                                    Handle manifest,
                                    Handle url,
                                    TRAPS);

  static Handle get_shared_jar_manifest(int shared_path_index, TRAPS);
  static Handle get_shared_jar_url(int shared_path_index, TRAPS);
  static Handle get_protection_domain_from_classloader(Handle class_loader,
                                                       Handle url, TRAPS);
  static Handle get_shared_protection_domain(Handle class_loader,
                                             int shared_path_index,
                                             Handle url,
                                             TRAPS);
  static Handle init_security_info(Handle class_loader, InstanceKlass* ik, TRAPS);

  static void atomic_set_array_index(objArrayOop array, int index, oop o);

  static oop shared_protection_domain(int index);
  static void atomic_set_shared_protection_domain(int index, oop pd) {
    atomic_set_array_index(_shared_protection_domains, index, pd);
  }
  static void allocate_shared_protection_domain_array(int size, TRAPS);
  static oop shared_jar_url(int index);
  static void atomic_set_shared_jar_url(int index, oop url) {
    atomic_set_array_index(_shared_jar_urls, index, url);
  }
  static void allocate_shared_jar_url_array(int size, TRAPS);
  static oop shared_jar_manifest(int index);
  static void atomic_set_shared_jar_manifest(int index, oop man) {
    atomic_set_array_index(_shared_jar_manifests, index, man);
  }
  static void allocate_shared_jar_manifest_array(int size, TRAPS);
public:
  static void initialize(TRAPS);
  static InstanceKlass* find_or_load_shared_class(Symbol* class_name,
                                                  Handle class_loader,
                                                  TRAPS);

  static void allocate_shared_data_arrays(int size, TRAPS);
  static void roots_oops_do(OopClosure* blk);
  static void oops_do(OopClosure* f);
  static bool is_sharing_possible(ClassLoaderData* loader_data) {
    oop class_loader = loader_data->class_loader();
    return (class_loader == NULL) || (UseAppCDS &&
           SystemDictionary::is_app_class_loader(class_loader));
  }

  static size_t dictionary_entry_size() {
    return sizeof(DictionaryEntry);
  }
  static void init_shared_dictionary_entry(Klass* k, DictionaryEntry* entry) {}

  // The (non-application) CDS implementation supports only classes in the boot
  // class loader, which ensures that the verification dependencies are the same
  // during archive creation time and runtime. Thus we can do the dependency checks
  // entirely during archive creation time.
  static void add_verification_dependency(Klass* k, Symbol* accessor_clsname,
                                          Symbol* target_clsname) {}
  static void finalize_verification_dependencies() {}
  static bool check_verification_dependencies(Klass* k, Handle class_loader,
                                              Handle protection_domain,
                                              char** message_buffer, TRAPS) {return true;}
};

#endif // SHARE_VM_CLASSFILE_SYSTEMDICTIONARYSHARED_HPP
