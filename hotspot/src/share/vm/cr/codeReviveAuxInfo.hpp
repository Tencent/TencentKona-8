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

#ifndef SHARE_VM_CR_CODE_REVIVE_AUX_INFO_HPP
#define SHARE_VM_CR_CODE_REVIVE_AUX_INFO_HPP
#include "classfile/systemDictionary.hpp"
#include "code/codeBlob.hpp"
#include "code/nmethod.hpp"
#include "code/scopeDesc.hpp"
#include "cr/codeReviveVMGlobals.hpp"
#include "cr/revive.hpp"
#include "oops/instanceKlass.hpp"
#include "utilities/growableArray.hpp"

class ciBaseObject;
class ciObject;
class ciMetadata;

// tag used to restore relocation from reloc auxiliary info
enum RelocTypeTag {
  tag_invalid                      = 0,  // invalid tag
  tag_vm_global                    = 1,  // tag for external_word_type & runtime_call_type relocation
  tag_oop_str                      = 2,  // tag for oop relocation which embeds a string
  tag_oop_classloader              = 3,  // tag for oop relocation which embeds a classloader
  tag_global_oop                   = 4,  // tag for oop relocation which embeds a vm global oop
  tag_meta_method_data             = 5,  // tag for metadata relocation which embeds a method data
  tag_meta_self_method             = 6,  // tag for metadata relocation which embeds self method
  tag_internal_word                = 7,  // tag for internal word relocation and section word relocation
  tag_klass_by_name_classloader    = 8,  // tag for metadata relocation which embeds a klass
  tag_mirror_by_name_classloader   = 9,  // tag for oop relocation which embeds a java mirror
  tag_method_by_name_classloader   = 10, // tag for metadata relocation which embeds a method
  tag_skip                         = 11, // tag used to skip current relocation
  tag_non_oop                      = 12, // tag for noop
  tag_end                          = 13, // end of tag
};

// add more known oop if necessary
#define CR_KNOWN_OOP_DO(func) \
  func(the_min_jint_string, Universe::the_min_jint_string) \
  func(the_null_string, Universe::the_null_string) \
  func(null_ptr_exception_instance, Universe::null_ptr_exception_instance) \
  func(arithmetic_exception_instance, Universe::arithmetic_exception_instance) \
  func(virtual_machine_error_instance, Universe::virtual_machine_error_instance) \
  func(vm_exception, Universe::vm_exception) \
  func(main_thread_group, Universe::main_thread_group) \
  func(system_thread_group, Universe::system_thread_group) \
  func(the_empty_class_klass_array, Universe::the_empty_class_klass_array) \
  func(int_mirror, Universe::int_mirror) \
  func(float_mirror, Universe::float_mirror) \
  func(double_mirror, Universe::double_mirror) \
  func(byte_mirror, Universe::byte_mirror) \
  func(bool_mirror, Universe::bool_mirror) \
  func(char_mirror, Universe::char_mirror) \
  func(long_mirror, Universe::long_mirror) \
  func(short_mirror, Universe::short_mirror) \
  func(void_mirror, Universe::void_mirror)


#define CR_KNOWN_OOP_FUNC(name, ignore) \
  CR_KnownOop_##name,

enum CR_KnownOopKind {
  CR_KnownOop_invalid,

  #define WK_KLASS_ENUM(name, i1, i2) CR_KnownOop_##name,
  WK_KLASSES_DO(WK_KLASS_ENUM)
  #undef WK_KLASS_ENUM

  CR_KNOWN_OOP_DO(CR_KNOWN_OOP_FUNC)

  CR_KnownOop_ArrayIndexOutOfBoundsException,
  CR_KnownOop_ArrayStoreException,
  CR_KnownOop_ClassCastException,
  CR_KnownOop_AppLoader,
  CR_KnownOop_ExtLoader,
};
#undef CR_KNOWN_OOP_FUNC

class RelocClosure : public Closure {
 public:
  virtual void do_reloc_oop_type(RelocIterator* iter, oop_Relocation* r) {};
  virtual void do_reloc_metadata_type(RelocIterator* iter, metadata_Relocation* r) {};
  virtual void do_reloc_data_type(RelocIterator* iter, DataRelocation* r) {};
  virtual void do_reloc_poll_type(RelocIterator* iter, Relocation* r) {};
  virtual void do_reloc_call_type(RelocIterator* iter, CallRelocation* r) {};
  virtual void do_reloc_static_stub_type(RelocIterator* iter, static_stub_Relocation* r) {};
  virtual void do_reloc_internal_word_type(RelocIterator* iter, DataRelocation* r) {};
};


// To restore relocation/metadata/oop, auxiliary information need to be generated.
// For different runtime object, a tag and a thunk of structured data are emitted,
// which will be used to restore the runtime object in subsequent restore jvm process.
class ReviveRelocClosure;
class EmitRelocClosure;
class CodeReviveMergedMetaInfo;
class ReviveAuxInfoTask : public StackObj {
 friend class ReviveRelocClosure;
 friend class EmitRelocClosure;
 private:
  nmethod* _nm;
  int _alignment;
  bool _success;
  union {
    oopDesc* _object;
    Metadata* _metadata;
  };

  inline oop object() {
    oop r = _object;
    return r;
  }

  // basic buffer read/write, advance _cur ptr when read/write
  void     emit_u2(uint16_t v);
  void     emit_u4(uint32_t v);
  void     emit_c_string(const char* str, bool with_align = true);
  uint16_t read_u2();
  uint32_t read_u4();
  char*    read_c_string();
  void     align();

  // utilities: global oops/metadata informations
  CR_KnownOopKind get_index_of_global_oop(oop obj); // can be static, need access CiEnv as friend class
  void relocations_do(RelocClosure* cl);

  // Emit revive informations on tag level, used during emit phase
  void emit_tag_only(RelocTypeTag tag);
  void emit_global_symbol(CR_GLOBAL_KIND kind, uint32_t offset=0);
  void emit_internal_word(uint32_t offset);
  bool emit_class(RelocTypeTag tag, Klass* k);
  bool emit_method(RelocTypeTag tag, Method* m);
  void emit_int_index(RelocTypeTag tag, int index);
  void emit_classloader(LoaderType type);
  bool emit_string(oop o);

  bool iterate_global_oops();
  bool emit_oop(oop o, oop_Relocation* r);
  bool emit_meta(Metadata* m, metadata_Relocation* r);
  bool try_emit_classloader(oop o);

  // revive
  bool revive_oop(oop_Relocation* r, GrowableArray<ciBaseObject*>* oops_metas, int& oops_metas_index);
  bool revive_meta(metadata_Relocation* r, GrowableArray<ciBaseObject*>* oops_metas, int& oops_metas_index);
  bool revive_internal_word(Relocation* r);
  bool revive_global_symbol(Relocation* r);
  bool revive_polling_symbol(Relocation* r);
  bool revive_virtual_call(Relocation* r);
  bool revive_opt_virtual_call(Relocation* r);
  bool revive_static_call_type(Relocation* r);
  bool revive_static_stub_type(RelocIterator& iter);
 protected:
  CodeReviveMetaSpace* _meta_space;
  Method* _method;
  char* _cur; // destination
  void fail();

  void     set_u4(uint32_t value, char* address);
  char*    get_retral_address(int len);
  // get the global index from CodeReviveMergedMetaInfo
  int      get_global_meta_index(int old_index, CodeReviveMergedMetaInfo* global_meta);
  LoaderType check_and_get_loader_type(Klass* k);
  oop get_loader(LoaderType type);
  oop get_method_holder_loader();

  virtual void process_vm_global(uint16_t kind, uint32_t offset) { }
  virtual void process_oop_str(uint16_t u2, char* str) { }
  virtual void process_meta_self_method() { }
  virtual void process_klass_by_name_classloader(int32_t meta_index, int32_t min_state) { }
  virtual void process_mirror_by_name_classloader(int32_t meta_index, int32_t min_state) { }
  virtual void process_method_by_name_classloader(int32_t meta_index) { }
  virtual void process_internal_word(uint32_t u4) { }
  virtual void process_oop_classloader(LoaderType loader_type) { }
  virtual void process_global_oop(uint16_t u2, uint32_t global_index) { }
  virtual void process_skip() { }
  virtual void process_non_oop() { }
  virtual bool iterate_condition() { return _success; }
 public:
  // use when emit/estimate
  ReviveAuxInfoTask(nmethod* nm, char* start, CodeReviveMetaSpace* meta_space) :
      _nm(nm), _method(nm->method()), _cur(start), _meta_space(meta_space), _alignment(8), _success(true) { }
  ReviveAuxInfoTask(Method* method, char* start, CodeReviveMetaSpace* meta_space) :
      _nm(NULL), _method(method), _cur(start), _meta_space(meta_space), _alignment(8), _success(true) { }
  // use when read
  ReviveAuxInfoTask(char* start, CodeReviveMetaSpace* meta_space) :
      _nm(NULL), _method(NULL), _cur(start), _meta_space(meta_space), _alignment(8), _success(true) { }

  char* pos() {
    return _cur;
  }

  void emit();
  void emit_meta_array();
  void emit_oop_array();

  void revive(const char* end, GrowableArray<ciBaseObject*>* oops_metas);
  void revive_data_array(GrowableArray<ciBaseObject*>* meta_array, GrowableArray<ciBaseObject*>* oop_array);

  void iterate_reloc_aux_info();
  bool success() const { return _success; }
  size_t estimate_emit_size();
  static oop get_exception_oop(CR_KnownOopKind kind);
  static oop get_global_oop(CR_KnownOopKind kind); // can be static, need access CiEnv as friend class
};

/* iterate reloc aux info and find oop & meta need resolutions.*/
class PreReviveTask : public ReviveAuxInfoTask {
 private:
  bool _prepare_data_array;
  GrowableArray<ciBaseObject*>* _oops_and_metas;
  GrowableArray<uint32_t>* _global_oop_array;
  virtual void process_oop_str(uint16_t u2, char* str);
  virtual void process_meta_self_method();
  virtual void process_klass_by_name_classloader(int32_t meta_index, int32_t min_state);
  virtual void process_mirror_by_name_classloader(int32_t meta_index, int32_t min_state);
  virtual void process_method_by_name_classloader(int32_t meta_index);
  virtual void process_oop_classloader(LoaderType loader_type);
  virtual void process_global_oop(uint16_t u2, uint32_t global_index);
  // null meta&oop need be add to meta array for usage in Opt and Dependency
  virtual void process_non_oop();

  // prepare oop & meta
  ciObject* prepare_oop_str(char* str);
  ciMetadata* prepare_klass_by_name_classloader(int32_t meta_index, int32_t min_state);
  ciObject* prepare_mirror_by_name_classloader(int32_t meta_index, int32_t min_state);
  ciMetadata* prepare_method_by_name_classloader(int32_t meta_index);

  bool check_instance_klass_state(InstanceKlass* k, int min_state);
  bool check_loader_consistency(oopDesc* cur_loader, LoaderType loader_type);
  bool check_klass_loader(Klass* k, LoaderType loader_type);
  bool check_method_loader(Method* m, LoaderType loader_type);
  Klass* revive_get_klass(Symbol* name, LoaderType loader_type, int min_state);
  Klass* revive_get_klass(const char* name, LoaderType loader_type, int min_state);
  Klass* revive_get_klass(int32_t meta_index, int min_state);
  Method* revive_get_method(int32_t meta_index);
 public:
  PreReviveTask(Method* method, char* start, CodeReviveMetaSpace* meta_space)
    : ReviveAuxInfoTask(method, start, meta_space) {
    guarantee(_method != NULL, "should be");
    _oops_and_metas = NULL;
    _prepare_data_array = false;
  }

  GrowableArray<ciBaseObject*>* pre_revive_oop_and_meta(bool prepare_data_array, GrowableArray<uint32_t>* global_oop_array);
};

/* iterate reloc aux info and get metadata array name.*/
class CollectMetadataArrayNameTask : public ReviveAuxInfoTask {
 private:
  GrowableArray<char*>* _names;
  char*                 _method_name;
  virtual void process_meta_self_method();
  virtual void process_klass_by_name_classloader(int32_t meta_index, int32_t min_state);
  virtual void process_method_by_name_classloader(int32_t meta_index);
  virtual void process_non_oop();
 public:
  CollectMetadataArrayNameTask(char* start, CodeReviveMetaSpace* meta_space, char* method_name)
    : ReviveAuxInfoTask(start, meta_space) {
    _names = NULL;
    _method_name = method_name;
  }

  GrowableArray<char*>* collect_names();
};

/* iterate reloc aux info and print info.*/
class PrintAuxInfoTask : public ReviveAuxInfoTask {
 private:
  const char* _end;
  outputStream* _out;
  virtual void process_vm_global(uint16_t kind, uint32_t offset);
  virtual void process_oop_str(uint16_t u2, char* str);
  virtual void process_meta_self_method();
  virtual void process_klass_by_name_classloader(int32_t meta_index, int32_t init_status);
  virtual void process_mirror_by_name_classloader(int32_t meta_index, int32_t init_status);
  virtual void process_method_by_name_classloader(int32_t meta_index);
  virtual void process_internal_word(uint32_t offset);
  virtual void process_oop_classloader(LoaderType loader_type);
  virtual void process_global_oop(uint16_t u2, uint32_t global_index);
  virtual void process_skip();
  virtual void process_non_oop();
  virtual bool iterate_condition() { return _cur < _end; }

 public:
  PrintAuxInfoTask(char* start, CodeReviveMetaSpace* meta_space)
    : ReviveAuxInfoTask(start, meta_space) {
    _end = NULL;
  }

  void print(const char* end) {
    _end = end;
    _out = CodeRevive::out();
    iterate_reloc_aux_info();
  }
};

/* iterate reloc aux info and check whether klass or meta has been resolved.*/
class CheckMetaResolveTask : public ReviveAuxInfoTask {
 private:
  bool _prepare_data_array;
  virtual void process_klass_by_name_classloader(int32_t meta_index, int32_t min_state);
  virtual void process_mirror_by_name_classloader(int32_t meta_index, int32_t min_state);
  virtual void process_method_by_name_classloader(int32_t meta_index);
 public:
  CheckMetaResolveTask(char* start, CodeReviveMetaSpace* meta_space, bool prepare_data_array = false)
    : ReviveAuxInfoTask(start, meta_space) {
    _prepare_data_array = prepare_data_array;
  }
};

/* iterate reloc aux info and update meta index with global meta index.*/
class UpdateMetaIndexTask : ReviveAuxInfoTask {
 private:
  CodeReviveMergedMetaInfo* _global_meta_info;
  int32_t update_one_meta_index(int32_t old_index, char* index_address);
  virtual void process_klass_by_name_classloader(int32_t meta_index, int32_t init_status);
  virtual void process_mirror_by_name_classloader(int32_t meta_index, int32_t init_status);
  virtual void process_method_by_name_classloader(int32_t meta_index);
 public:
  UpdateMetaIndexTask(char* start, CodeReviveMetaSpace* meta_space, CodeReviveMergedMetaInfo* meta_info)
    : ReviveAuxInfoTask(start, meta_space), _global_meta_info(meta_info) {
  }
  void update_meta_space_index() {
    iterate_reloc_aux_info();
  }
};

/* iterate reloc aux info and collect meta index in data array.*/
class CollectKlassAndMethodIndexTask : public ReviveAuxInfoTask {
 private:
  int32_t _self_method;
  GrowableArray<int32_t>* _indexes;
  CodeReviveMergedMetaInfo*     _global_meta_info;

  void process_meta_self_method();
  void process_klass_by_name_classloader(int32_t meta_index, int32_t min_state);
  void process_method_by_name_classloader(int32_t meta_index);
  void process_non_oop();

 public:
  CollectKlassAndMethodIndexTask(char* start, int32_t cur_method, GrowableArray<int32_t>* indexes, CodeReviveMetaSpace* meta_space, CodeReviveMergedMetaInfo* meta_info)
    : ReviveAuxInfoTask(start, meta_space), _self_method(cur_method), _indexes(indexes), _global_meta_info(meta_info) {}
}; 
#endif // SHARE_VM_CR_CODE_REVIVE_AUX_INFO_HPP
