/*
 * Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "classfile/classFileStream.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/classLoaderExt.hpp"
#include "classfile/dictionary.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "classfile/verificationType.hpp"
#include "classfile/vmSymbols.hpp"
#include "memory/allocation.hpp"
#include "memory/filemap.hpp"
#include "memory/metadataFactory.hpp"
#include "memory/oopFactory.hpp"
#include "memory/resourceArea.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klass.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "utilities/hashtable.inline.hpp"
#include "utilities/stringUtils.hpp"


objArrayOop SystemDictionaryShared::_shared_protection_domains  =  NULL;
objArrayOop SystemDictionaryShared::_shared_jar_urls            =  NULL;
objArrayOop SystemDictionaryShared::_shared_jar_manifests       =  NULL;

void SystemDictionaryShared::initialize(TRAPS) {
  if (UseAppCDS) {
    SystemDictionary::ByteArrayInputStream_klass()->initialize(CHECK);
    SystemDictionary::URL_klass()->initialize(CHECK);
    SystemDictionary::ProtectionDomain_klass()->initialize(CHECK);
    SystemDictionary::File_klass()->initialize(CHECK);
    SystemDictionary::Jar_Manifest_klass()->initialize(CHECK);
    SystemDictionary::CodeSource_klass()->initialize(CHECK);
  }
}

void SystemDictionaryShared::atomic_set_array_index(objArrayOop array, int index, oop o) {
  // Benign race condition:  array.obj_at(index) may already be filled in.
  // The important thing here is that all threads pick up the same result.
  // It doesn't matter which racing thread wins, as long as only one
  // result is used by all threads, and all future queries.
  Thread* thread = Thread::current();
  HandleMark hm(thread);
  Handle obj = Handle(thread, o);
  MutexLocker mu(SystemDictionary_lock, thread);
  if (array->obj_at(index) == NULL) {
    OrderAccess::storestore();
    array->obj_at_put(index, obj());
  }
}


oop SystemDictionaryShared::shared_protection_domain(int index) {
  return _shared_protection_domains->obj_at(index);
}

oop SystemDictionaryShared::shared_jar_url(int index) {
  return _shared_jar_urls->obj_at(index);
}

oop SystemDictionaryShared::shared_jar_manifest(int index) {
  return _shared_jar_manifests->obj_at(index);
}


Handle SystemDictionaryShared::get_shared_jar_manifest(int shared_path_index, TRAPS) {
  Handle manifest;
  if (shared_jar_manifest(shared_path_index) == NULL) {
    SharedClassPathEntry* ent = FileMapInfo::shared_classpath(shared_path_index);
    long size = ent->manifest_size();
    if (size <= 0) {
      return Handle();
    }

    // ByteArrayInputStream bais = new ByteArrayInputStream(buf);
    const char* src = ent->manifest();
    assert(src != NULL, "No Manifest data");
    typeArrayOop buf = oopFactory::new_byteArray(size, CHECK_NH);
    typeArrayHandle bufhandle(THREAD, buf);
    char* dst = (char*)(buf->byte_at_addr(0));
    memcpy(dst, src, (size_t)size);

    Handle bais = JavaCalls::construct_new_instance(InstanceKlass::cast(SystemDictionary::ByteArrayInputStream_klass()),
                      vmSymbols::byte_array_void_signature(),
                      bufhandle, CHECK_NH);

    // manifest = new Manifest(bais)
    manifest = JavaCalls::construct_new_instance(InstanceKlass::cast(SystemDictionary::Jar_Manifest_klass()),
                      vmSymbols::input_stream_void_signature(),
                      bais, CHECK_NH);
    atomic_set_shared_jar_manifest(shared_path_index, manifest());
  }

  manifest = Handle(THREAD, shared_jar_manifest(shared_path_index));
  assert(manifest.not_null(), "sanity");
  return manifest;
}

Handle SystemDictionaryShared::get_shared_jar_url(int shared_path_index, TRAPS) {
  Handle url_h;
  if (shared_jar_url(shared_path_index) == NULL) {
    // File file = new File(path)
    InstanceKlass* file_klass = InstanceKlass::cast(SystemDictionary::File_klass());
    Handle file = file_klass->allocate_instance_handle(CHECK_(url_h));
    {
      const char* path = FileMapInfo::shared_classpath_name(shared_path_index);
      Handle path_string = java_lang_String::create_from_str(path, CHECK_(url_h));
      JavaValue result(T_VOID);
      JavaCalls::call_special(&result, file, KlassHandle(THREAD,file_klass),
                              vmSymbols::object_initializer_name(),
                              vmSymbols::string_void_signature(),
                              path_string, CHECK_(url_h));
    }

    // result = Launcher.getURL(file)
    KlassHandle launcher_klass(THREAD, SystemDictionary::sun_misc_Launcher_klass());
    JavaValue result(T_OBJECT);
    JavaCalls::call_static(&result, launcher_klass,
                           vmSymbols::getFileURL_name(),
                           vmSymbols::getFileURL_signature(),
                           file, CHECK_(url_h));

    atomic_set_shared_jar_url(shared_path_index, (oop)result.get_jobject());
  }

  url_h = Handle(THREAD, shared_jar_url(shared_path_index));
  assert(url_h.not_null(), "sanity");
  return url_h;
}

Handle SystemDictionaryShared::get_package_name(Symbol* class_name, TRAPS) {
  ResourceMark rm(THREAD);
  Handle pkgname_string;
  char* pkgname = (char*) ClassLoader::package_from_name((const char*) class_name->as_C_string());
  if (pkgname != NULL) { // Package prefix found
    StringUtils::replace_no_expand(pkgname, "/", ".");
    pkgname_string = java_lang_String::create_from_str(pkgname,
                                                       CHECK_(pkgname_string));
  }
  return pkgname_string;
}

// Define Package for shared app classes from JAR file and also checks for
// package sealing (all done in Java code)
// See http://docs.oracle.com/javase/tutorial/deployment/jar/sealman.html
void SystemDictionaryShared::define_shared_package(Symbol*  class_name,
                                                   Handle class_loader,
                                                   Handle manifest,
                                                   Handle url,
                                                   TRAPS) {
  // get_package_name() returns a NULL handle if the class is in unnamed package
  Handle pkgname_string = get_package_name(class_name, CHECK);
  if (pkgname_string.not_null()) {
    KlassHandle url_classLoader_klass(THREAD, SystemDictionary::URLClassLoader_klass());
    JavaValue result(T_VOID);
    JavaCallArguments args(3);
    args.set_receiver(class_loader);
    args.push_oop(pkgname_string);
    args.push_oop(manifest);
    args.push_oop(url);
    JavaCalls::call_virtual(&result, url_classLoader_klass,
                            vmSymbols::definePackageInternal_name(),
                            vmSymbols::definePackageInternal_signature(),
                            &args,
                            CHECK);
  }
}

// Get the ProtectionDomain associated with the CodeSource from the classloader.
Handle SystemDictionaryShared::get_protection_domain_from_classloader(Handle class_loader,
                                                                      Handle url, TRAPS) {
  // CodeSource cs = new CodeSource(url, null);
  Handle cs = JavaCalls::construct_new_instance(InstanceKlass::cast(SystemDictionary::CodeSource_klass()),
                  vmSymbols::url_code_signer_array_void_signature(),
                  url, Handle(), CHECK_NH);

  // protection_domain = SecureClassLoader.getProtectionDomain(cs);
  Klass* secureClassLoader_klass = SystemDictionary::SecureClassLoader_klass();
  JavaValue obj_result(T_OBJECT);
  JavaCalls::call_virtual(&obj_result, class_loader, secureClassLoader_klass,
                          vmSymbols::getProtectionDomain_name(),
                          vmSymbols::getProtectionDomain_signature(),
                          cs, CHECK_NH);
  return Handle(THREAD, (oop)obj_result.get_jobject());
}

// Returns the ProtectionDomain associated with the JAR file identified by the url.
Handle SystemDictionaryShared::get_shared_protection_domain(Handle class_loader,
                                                            int shared_path_index,
                                                            Handle url,
                                                            TRAPS) {
  Handle protection_domain;
  if (shared_protection_domain(shared_path_index) == NULL) {
    Handle pd = get_protection_domain_from_classloader(class_loader, url, THREAD);
    atomic_set_shared_protection_domain(shared_path_index, pd());
  }

  // Acquire from the cache because if another thread beats the current one to
  // set the shared protection_domain and the atomic_set fails, the current thread
  // needs to get the updated protection_domain from the cache.
  protection_domain = Handle(THREAD, shared_protection_domain(shared_path_index));
  assert(protection_domain.not_null(), "sanity");
  return protection_domain;
}

// Initializes the java.lang.Package and java.security.ProtectionDomain objects associated with
// the given InstanceKlass.
// Returns the ProtectionDomain for the InstanceKlass.
Handle SystemDictionaryShared::init_security_info(Handle class_loader, InstanceKlass* ik, TRAPS) {
  Handle pd;

  if (ik != NULL) {
    int index = ik->shared_classpath_index();
    assert(index >= 0, "Sanity");
    SharedClassPathEntry* ent = FileMapInfo::shared_classpath(index);
    Symbol* class_name = ik->name();

    // For shared app/platform classes originated from JAR files on the class path:
    //   Each of the 3 SystemDictionaryShared::_shared_xxx arrays has the same length
    //   as the shared classpath table in the shared archive (see
    //   FileMap::_shared_path_table in filemap.hpp for details).
    //
    //   If a shared InstanceKlass k is loaded from the class path, let
    //
    //     index = k->shared_classpath_index():
    //
    //   FileMap::_shared_path_table[index] identifies the JAR file that contains k.
    //
    //   k's protection domain is:
    //
    //     ProtectionDomain pd = _shared_protection_domains[index];
    //
    //   and k's Package is initialized using
    //
    //     manifest = _shared_jar_manifests[index];
    //     url = _shared_jar_urls[index];
    //     define_shared_package(class_name, class_loader, manifest, url, CHECK_(pd));
    //
    //   Note that if an element of these 3 _shared_xxx arrays is NULL, it will be initialized by
    //   the corresponding SystemDictionaryShared::get_shared_xxx() function.
    Handle manifest = get_shared_jar_manifest(index, CHECK_(pd));
    Handle url = get_shared_jar_url(index, CHECK_(pd));
    define_shared_package(class_name, class_loader, manifest, url, CHECK_(pd));
    pd = get_shared_protection_domain(class_loader, index, url, CHECK_(pd));
  }
  return pd;
}

// The following stack shows how this code is reached:
//
//   [0] SystemDictionaryShared::find_or_load_shared_class()
//   [1] JVM_FindLoadedClass
//   [2] java.lang.ClassLoader.findLoadedClass0()
//   [3] java.lang.ClassLoader.findLoadedClass()
//   [4] java.lang.ClassLoader.loadClass()
//   [5] java.net.URLClassLoader.loadClass()
//   [6] sun.misc.Launcher$ExtClassLoader.loadClass(), or
//       sun.misc.Launcher$AppClassLoader.loadClass()
//
// AppCDS supports fast class loading for these 2 built-in class loaders:
//    sun.misc.Launcher$ExtClassLoader
//    sun.misc.Launcher$AppClassLoader
// with the following assumptions (based on the JDK core library source code):
//
// [a] these two loaders use the ClassLoader.loadClass() to
//     load the named class.
// [b] ClassLoader.loadClass() first calls findLoadedClass(name).
//
// Given these assumptions, we intercept the findLoadedClass() call to invoke
// SystemDictionaryShared::find_or_load_shared_class() to load the shared class from
// the archive for the 2 built-in class loaders. This way,
// we can improve start-up because we avoid decoding the classfile,
// and avoid delegating to the parent loader.
//
// NOTE: there's a lot of assumption about the Java code. If any of that change, this
// needs to be redesigned.

InstanceKlass* SystemDictionaryShared::find_or_load_shared_class(
                 Symbol* name, Handle class_loader, TRAPS) {
  InstanceKlass* k = NULL;
  if (!UseAppCDS) {
    return NULL;
  }
  if (UseSharedSpaces) {
    if (!FileMapInfo::current_info()->header()->has_ext_or_app_classes()) {
      return NULL;
    }

    if (shared_dictionary() != NULL &&
        SystemDictionary::is_app_class_loader(class_loader())) {
      // Fix for 4474172; see evaluation for more details
      class_loader = Handle(
        THREAD, java_lang_ClassLoader::non_reflection_class_loader(class_loader()));
      ClassLoaderData *loader_data = register_loader(class_loader, CHECK_NULL);

      unsigned int d_hash = dictionary()->compute_hash(name, loader_data);
      int d_index = dictionary()->hash_to_index(d_hash);       
      bool DoObjectLock = true;
      if (is_parallelCapable(class_loader)) {
        DoObjectLock = false;
      }

      // Make sure we are synchronized on the class loader before we proceed
      //
      // Note: currently, find_or_load_shared_class is called only from
      // JVM_FindLoadedClass and used for ExtClassLoader and AppClassLoader,
      // which are parallel-capable loaders, so this lock is NOT taken.
      Handle lockObject = compute_loader_lock_object(class_loader, THREAD);
      check_loader_lock_contention(lockObject, THREAD);
      ObjectLocker ol(lockObject, THREAD, DoObjectLock);

      {
        MutexLocker mu(SystemDictionary_lock, THREAD);
        Klass* check = find_class(d_index, d_hash, name, loader_data);
        if (check != NULL) {
          return InstanceKlass::cast(check);
        }
      }

      k = load_shared_class_for_builtin_loader(name, class_loader, THREAD);
      if (k != NULL) {
        define_instance_class(k, CHECK_NULL);
      }
    }
  }
  return k;
}

InstanceKlass* SystemDictionaryShared::load_shared_class_for_builtin_loader(
                 Symbol* class_name, Handle class_loader, TRAPS) {
  assert(UseSharedSpaces, "must be");
  assert(shared_dictionary() != NULL, "already checked");
  Klass* k = find_shared_class(class_name);

  if (k != NULL) {
    InstanceKlass* ik = InstanceKlass::cast(k);
    if (ik->is_shared_app_class() &&
        SystemDictionary::is_app_class_loader(class_loader())) {
      Handle protection_domain =
        SystemDictionaryShared::init_security_info(class_loader, ik, CHECK_NULL);
      instanceKlassHandle instance(THREAD, k); 
      instanceKlassHandle ikh = load_shared_class(instance, class_loader, protection_domain, THREAD);
      return ikh();
    }
  }

  return NULL;
}

void SystemDictionaryShared::roots_oops_do(OopClosure* f) {
  if (!UseAppCDS) {
    return;
  }
  f->do_oop((oop*)&_shared_protection_domains);
  f->do_oop((oop*)&_shared_jar_urls);
  f->do_oop((oop*)&_shared_jar_manifests);
}

void SystemDictionaryShared::oops_do(OopClosure* f) {
  if (!UseAppCDS) {
    return;
  }
  f->do_oop((oop*)&_shared_protection_domains);
  f->do_oop((oop*)&_shared_jar_urls);
  f->do_oop((oop*)&_shared_jar_manifests);
}

void SystemDictionaryShared::allocate_shared_protection_domain_array(int size, TRAPS) {
  if (_shared_protection_domains == NULL) {
    _shared_protection_domains = oopFactory::new_objArray(
        SystemDictionary::ProtectionDomain_klass(), size, CHECK);
  }
}

void SystemDictionaryShared::allocate_shared_jar_url_array(int size, TRAPS) {
  if (_shared_jar_urls == NULL) {
    _shared_jar_urls = oopFactory::new_objArray(
        SystemDictionary::URL_klass(), size, CHECK);
  }
}

void SystemDictionaryShared::allocate_shared_jar_manifest_array(int size, TRAPS) {
  if (_shared_jar_manifests == NULL) {
    _shared_jar_manifests = oopFactory::new_objArray(
        SystemDictionary::Jar_Manifest_klass(), size, CHECK);
  }
}

void SystemDictionaryShared::allocate_shared_data_arrays(int size, TRAPS) {
  allocate_shared_protection_domain_array(size, CHECK);
  allocate_shared_jar_url_array(size, CHECK);
  allocate_shared_jar_manifest_array(size, CHECK);
}
