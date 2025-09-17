/*
 *
 * Copyright (C) 2022 Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
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
#include "ci/ciEnv.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "runtime/arguments.hpp"
#include "utilities/ostream.hpp"
#include "cr/codeReviveFile.hpp"
#include "cr/codeReviveCodeBlob.hpp"
#include "cr/codeReviveMetaSpace.hpp"
#include "cr/codeReviveVMGlobals.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "cr/revive.hpp"

/*
 * Class redefine support: baseline scheme
 * 1. When a class is loaded, only calculate crc32 of bytecodes, not the entirely identity.
 * 2. When a class' identity is read, and it hasn't been generated, generate it.
 *    IntanceKlass: generate self's and all super/interface's identities iteratively.
 *    ObjArrayKlass: generate self's, all lower-dimension arrays and base element class's identities.
 *    TypeArrayKlass: generate self's identity.
 * 3. When a class' identity is read, and some class has been redefined, check whether identity need to be re-generated.
 * 
 * The key step: how to check whether a class's identity need to be re-generated.
 * 1. class CodeRevive adds a field: class_redefine_epoch, and is initialized to 1.
 *    When a class is redefined, it is incremented by 1.
 * 2. class Klass adds a field: class_redefine_epoch, and is initialized to 0.
 *    Except for java.lang.Object, whose epoch is initialized to 1.
 * 3. When a class is redefined:
 *    (1) re-generate it's identity
 *    (2) increment it's epoch by 1
 *    (3) increment CodeRevive's epoch by 1
 * 4. When read a class's identity:
 *    refresh class's epoch to CodeRevive's epoch at last.
 * 5. When read a class's identity and it's epoch is less than CodeRevive's epoch,
 *    this class needs to check whether it's identity need to be re-generated.
 *    And there are two cases which can cause identity re-generation.
 *    (1) the first time to read identity
 *    (2) some class is redefined since last identity read
 * 6. How to confirm a class's identity need to be re-generated.
 *    InstanceKlass: if one of it's super/interface (iteratively) classes' epoch is larger than itself's.
 *    ObjArrayKlass: if one of it's base element class's super/interface (iteratively) classes' epoch is larger than itself's.
 *    TypeArrayKlass: never needed.
 *
 * The baseline scheme has a drawback is that:
 * After a class is redefined, first get identity of a class will cause iterate all its super classes and
 *   interfaces to check whether need to re-generate identity.
 * If the number of redefined classes is big, this may be a expense cost.
 *
 * Therefore, we have a optimized scheme:
 * 1. class CodeRevive adds a field: redefine_epoch, and is initialized to 0.
 * 2. class Klass adds two fields: redefine_epoch, and is initielized to 0.
 *                                 is_valid, and is initialized to false.
 * 3. When a class is redefined, set it and all direct/indirect subclasses to invalid.
 * 4. When a interface is redefined, set it and all direct/indirect subclasses to invalid.
 *    incrment CodeRevive.redefine_epoch.
 * 5. When read an InstanceKlass's identity:
 *    If is interface and is valid, return identity directly.
 *    If is invalid or epoch < CodeRevive.epoch, identity may be obsoleted.
 *    If is invalid or epoch < one of transitively implemented interfaces' epoch, identity should be regenerated.
 *    Set CodeRevive.epoch to epoch.
 * 6. When read an ObjArrayKlass's identiy:
 *    return (high 32bits of base element class's identity) << 32 | dimension;
 */

void CodeRevive::process_redefined_class(InstanceKlass* redefined_klass, InstanceKlass* scratch_klass) {
  if (!is_restore() && !is_save()) {
    return;
  }

  guarantee(SafepointSynchronize::is_at_safepoint(), "safepoint is needed.");

  // set new bytecode crc32
  redefined_klass->set_cr_identity(scratch_klass->cr_identity_no_check());

  MutexLockerEx ml(CodeReviveEpoch_lock, Mutex::_no_safepoint_check_flag);

  // Either set epoch invalid or increment CodeRevive.redefine_epoch
  // will make klass.redefine_epoch < CodeRevive.redefine_epoch;
  redefined_klass->set_redefine_epoch_invalid_with_subclass();

  if (redefined_klass->is_interface()) {
    // update class redefine epoch
    CodeRevive::next_redefine_epoch(redefined_klass);
  }
}

void CodeRevive::next_redefine_epoch(Klass* k) {
  guarantee(CodeReviveEpoch_lock->owned_by_self(), "should be");
  OrderAccess::release_store(&_redefine_epoch, _redefine_epoch + 2);
  guarantee(_redefine_epoch > 0, "should be");

  CR_LOG(log_kind(), cr_trace, "Interface %s is redefined, set global redefine epoch to %d.\n", 
         k->external_name(), _redefine_epoch);
}

/* 
 * CodeRevive:: check identity consistency
 * Class redefined is processed by VM thread in safe point.
 * While metadata reviving is in native state, and can run concurrent with class redefine.
 * Compile thread revives a class, and then this class is redefined.
 * In such situation, CodeRevive will revive a deprecated class and cause error.
 * Therefore, this method is used to check whether all metadatas revived are up to date.
 * This methld should be called in VM state and locked properly.
 */
bool ciEnv::validate_aot_metadata_identities(GrowableArray<ciBaseObject*>* meta_array, Method* self_method) {
  ASSERT_IN_VM;
  assert(MethodCompileQueue_lock->owned_by_self(), "should be");
  assert(Compile_lock->owned_by_self(), "should be");
  CodeReviveMetaSpace* metaspace = CodeRevive::current_meta_space();
  for (int i = 0; i < meta_array->length(); i++) {
    Metadata* meta = meta_array->at(i)->as_metadata()->constant_encoding();

    if (CodeRevive::verify_redefined_identity()) {
      InstanceKlass* ik = (meta->is_klass()) ? InstanceKlass::cast((Klass*)meta) : ((Method*)meta)->method_holder();
      ik->verify_redefined_identity();
    }

    if (meta == self_method) {
      continue;
    }
    int32_t index = meta->csa_meta_index();
    guarantee(index != -1, "should be");
    int64_t identity_now = meta->cr_identity();
    if (identity_now != metaspace->metadata_identity(index)) {
      record_failure("Revived metadata changed by class redefine");
      return true;
    }
  }
  return false;
}

void Klass::set_cr_identity(int64_t id) {
#if defined(X86) && defined(LINUX)
  OrderAccess::release_store(&_cr_identity, id);
#endif
}

bool InstanceKlass::is_transitive_interfaces_redefined(int32_t epoch) {
  Array<Klass*>* ifaces = transitive_interfaces();
  if (ifaces != NULL) {
    for (int i = 0;i < ifaces->length(); i++) {
      InstanceKlass* iface = InstanceKlass::cast(ifaces->at(i));
      if (iface->is_redefine_epoch_valid() == false || epoch < iface->redefine_epoch()) {
        return true;
      }
    }
  }
  return false;
}

int64_t InstanceKlass::cr_identity() {
  if (!CodeRevive::is_on()) {
    return 0;
  }

#if defined(X86) && defined(LINUX)
  if (is_interface() && is_redefine_epoch_valid()) {
    return OrderAccess::load_acquire(&_cr_identity);
  }

  int32_t self_epoch;
  int32_t global_epoch;

  while ((self_epoch = redefine_epoch()) < (global_epoch = CodeRevive::redefine_epoch())) {
    // epoch invalid, or an interface has been redefined.

    CR_LOG(CodeRevive::log_kind(), cr_trace, "Identity for class %s may be deprecated. (Epoch: %d < %d)\n",
           external_name(), self_epoch, global_epoch);
    /*
     * Identity generation should execute before refresh epoch.
     * Or will get old identity in such case for class K:
     *   1. Thread 1 updates K's epoch before generating identity.
     *   2. Thread 2 tries to get K's identity, and finds K's epoch is same as CodeRevive epoch.
     *   3. Thread 2 returns K's old identity.
     *   4. Thread 1 generates K's identity.
     */
    if (// invalid epoch
        is_redefine_epoch_valid() == false ||
        // interface implemented transitively is redefined.
        is_transitive_interfaces_redefined(self_epoch)) {
      generate_cr_identity();
    }

    {
      /* 
       * The lock and re-recheck is necessary to avoid inconsistent identity and epoch.
       * For example:
       *   1. Thread 1 finds class K's epoch is less than CodeRevive'epoch.
       *   2. Thread 1 generates K's identity.
       *   3. Thread 2 redefines class K.
       *   4. Thread 1 refreshes K's epoch.
       *
       * Therefore before refresh class K's epoch, we must check whether a class redefine has happened.
       * And all write to epoch(CodeRevive or Class) must be locked.
       */
      MutexLockerEx ml(CodeReviveEpoch_lock, Mutex::_no_safepoint_check_flag);
      if (global_epoch != CodeRevive::redefine_epoch()) {
        continue;
      }
      set_redefine_epoch(global_epoch);
    }
    CR_LOG(CodeRevive::log_kind(), cr_trace, "%s: Set redefine epoch to %d.\n", external_name(), global_epoch);
  }

  return OrderAccess::load_acquire(&_cr_identity);
#else
  return 0;
#endif
}

void InstanceKlass::verify_redefined_identity() {
#if defined(X86) && defined(LINUX)
  if (!CodeRevive::verify_redefined_identity()) {
    return;
  }

  bool is_epoch_valid = is_redefine_epoch_valid();

  int64_t cur_idenetity = OrderAccess::load_acquire(&_cr_identity);

  // verify super/interfaces first, to make sure when calling generate_cr_identity()
  // identity of all super classes/interfaces have been verified.
  if (super() != NULL) {
    InstanceKlass::cast(super())->verify_redefined_identity();
  }

  Array<Klass*>* ifaces = local_interfaces();
  if (ifaces != NULL) {
    for (int i = 0;i < ifaces->length(); i++) {
      InstanceKlass::cast(ifaces->at(i))->verify_redefined_identity();
    }
  }
  generate_cr_identity();

  guarantee(!is_epoch_valid || cur_idenetity == _cr_identity, "verify_redefined_identity failed");
#endif
}

int32_t InstanceKlass::redefine_epoch() {
#if defined(X86) && defined(LINUX)
  return OrderAccess::load_acquire(&_redefine_epoch);
#else
  return 0;
#endif
}

void InstanceKlass::set_redefine_epoch(int32_t new_epoch) {
#if defined(X86) && defined(LINUX)
  guarantee(CodeReviveEpoch_lock->owned_by_self(), "should be");
  OrderAccess::release_store(&_redefine_epoch, new_epoch);
#endif
}

void InstanceKlass::set_redefine_epoch_invalid() {
#if defined(X86) && defined(LINUX)
  guarantee(CodeReviveEpoch_lock->owned_by_self(), "should be");
  OrderAccess::release_store(&_redefine_epoch, _redefine_epoch & ~1);
#endif
}

bool InstanceKlass::is_redefine_epoch_valid() {
  return (redefine_epoch() & 0x1) == 1;
}

void InstanceKlass::set_redefine_epoch_invalid_with_subclass() {
  set_redefine_epoch_invalid();
  CR_LOG(CodeRevive::log_kind(), cr_trace, "Redefine caused set redefine epoch of %s to be invalid.\n", external_name());

  for (Klass* subclass = subklass(); subclass != NULL; subclass = subclass->next_sibling()) {
    if (subclass->oop_is_array()) {
      // Only can be true when java.lang.Object is redefined.
      assert(this == SystemDictionary::well_known_klass(SystemDictionary::WK_KLASS_ENUM_NAME(Object_klass)), "should be");
      CR_LOG(CodeRevive::log_kind(), cr_trace, "Subclass %s is array, skip it.\n", subclass->external_name());
      continue;
    }
    InstanceKlass* ik = InstanceKlass::cast(subclass);
    if (ik->is_redefine_epoch_valid() == false) {
      CR_LOG(CodeRevive::log_kind(), cr_trace, "Subclass %s is invalid, stop searching.\n", ik->external_name());
      continue;
    }
    ik->set_redefine_epoch_invalid_with_subclass();
  }
}

int64_t ObjArrayKlass::cr_identity() {
  if (!CodeRevive::is_on()) {
    return 0;
  }

  int64_t cur_identity = ((_bottom_klass->cr_identity() >> 32) << 32) | dimension();

  if (_cr_identity != cur_identity) {
    CR_LOG(CodeRevive::log_kind(), cr_info, "CodeRevive Identity for class %s is %lx.\n" ,
           external_name(), cur_identity);
    _cr_identity = cur_identity;
  }

  return _cr_identity;
}

int64_t TypeArrayKlass::cr_identity() {
  if (!CodeRevive::is_on()) {
    return 0;
  }

  return _cr_identity;
}
