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
#include "classfile/javaClasses.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "cr/revive.hpp"

#ifndef _WINDOWS 
#define MAX_BUFFER_SIZE PATH_MAX
#else
#define MAX_BUFFER_SIZE MAX_PATH
#endif

MergePhaseKlassResovleCacheTable* MergePhaseKlassResovleCacheTable::_the_table = NULL;

MergePhaseKlassResovleCacheTable::MergePhaseKlassResovleCacheTable() : Hashtable<Symbol*, mtSymbol>(1009, sizeof (MergePhaseKlassResovleCacheEntry)) {}

MergePhaseKlassResovleCacheEntry* MergePhaseKlassResovleCacheTable::new_entry(unsigned int hash, Symbol* name) {
  MergePhaseKlassResovleCacheEntry* entry = (MergePhaseKlassResovleCacheEntry*)Hashtable<Symbol*, mtSymbol>::new_entry(hash, name);
  return entry;
}

void MergePhaseKlassResovleCacheTable::add_klass(Symbol* name, Klass* k) {
  unsigned int hashValue = compute_hash(name);
  int index = the_table()->hash_to_index(hashValue);
  
  MergePhaseKlassResovleCacheEntry* e = the_table()->basic_add(index, name, hashValue);
  e->set_klass(k);
}

MergePhaseKlassResovleCacheEntry* MergePhaseKlassResovleCacheTable::lookup_only(Symbol* name) {
  unsigned int hash = compute_hash(name);
  int index = the_table()->hash_to_index(hash);

  return the_table()->lookup(index, name, hash);
}

MergePhaseKlassResovleCacheEntry* MergePhaseKlassResovleCacheTable::lookup(int index, Symbol* name, unsigned int hash) {
  int count = 0;
  for (MergePhaseKlassResovleCacheEntry* e = bucket(index); e != NULL; e = e->next()) {
    if (e->hash() == hash && e->symbol() == name) {
      return e;
    }
  }
  return NULL;
}

MergePhaseKlassResovleCacheEntry* MergePhaseKlassResovleCacheTable::basic_add(int index, Symbol* name, unsigned int hashValue) {
  MergePhaseKlassResovleCacheEntry* entry = new_entry(hashValue, name);
  Hashtable<Symbol*, mtSymbol>::add_entry(index, entry);
  return entry;
}

void MergePhaseKlassResovleCacheTable::print_information() {
  outputStream* output = CodeRevive::out();
  int resolved_klass_count = 0;
  int unresolved_klass_count = 0;
  for (int i = 0; i < table_size(); ++i) {
    MergePhaseKlassResovleCacheEntry* entry = bucket(i);
    while (entry != NULL) {
      if (entry->klass() != NULL) {
        resolved_klass_count++;
      } else {
        unresolved_klass_count++;
      }
      entry = entry->next();
    }
  }
  output->print_cr("Global klass table information:");
  output->print_cr("  resolved klass   %d", resolved_klass_count);
  output->print_cr("  unresolved klass %d", unresolved_klass_count);
}

void MergePhaseKlassResovleCacheTable::print_cache_table_information() {
  the_table()->print_information();  
}

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

DirWithClassTable* DirWithClassTable::_the_table = NULL;

DirWithClassTable::DirWithClassTable() : Hashtable<const char*, mtInternal>(20, sizeof (DirWithClassEntry)) {}

DirWithClassEntry* DirWithClassTable::new_entry(unsigned int hash, const char* name) {
  DirWithClassEntry* entry = (DirWithClassEntry*)Hashtable<const char*, mtInternal>::new_entry(hash, name);
  entry->set_in_classpath(false);
  return entry;
}

// 1. lookup the dir path in hashtble
//    if found, return entry
// 2. lookup or add real path in hashtable
//    for lookup, if found, return entry which is in classpath
// 3. add the dir path into hashtable
DirWithClassEntry* DirWithClassTable::lookup(const char* name, size_t len, bool add_real, TRAPS) {
  unsigned int hashValue = compute_hash(name, (int)len);
  int index = the_table()->hash_to_index(hashValue);
  
  DirWithClassEntry* e = the_table()->lookup(index, name, hashValue);
  
  // Found the directory path in hashtable
  if (e != NULL) return e;

  // Grab DirWithClassTable_lock first.
  MutexLocker ml(DirWithClassTable_lock, THREAD);

  // find the real path name
  // 1. add the real path into hashtable in save stage
  // 2. look up the real path in hashtable in restore stage
 
  char buffer[MAX_BUFFER_SIZE];
  const char* real_path = canonical_path(name, buffer);
  if (real_path != name) {
    size_t real_path_len = strlen(real_path);
    hashValue = compute_hash(real_path, (int)real_path_len);
    index = the_table()->hash_to_index(hashValue);
    if (add_real) {
      // add the real path into hashtable
      e = the_table()->basic_add(index, real_path, real_path_len, hashValue);
    } else {
      e = the_table()->lookup(index, real_path, hashValue);
      // if the real path is in classpath, directly return the entry
      if (e != NULL) {
        return e;
      }
    }
  }
  // add the name into hashtable
  e = the_table()->basic_add(index, name, len, hashValue);
  return e;
}

bool DirWithClassTable::lookup_only(const char* name, size_t len) {
  unsigned int hash = compute_hash(name, (int)len);
  int index = the_table()->hash_to_index(hash);

  return the_table()->lookup(index, name, hash) != NULL;
}

DirWithClassEntry* DirWithClassTable::lookup(int index, const char* name, unsigned int hash) {
  int count = 0;
  for (DirWithClassEntry* e = bucket(index); e != NULL; e = e->next()) {
    if (e->hash() == hash && strcmp(e->dir_path(), name) == 0) {
      return e;
    }
  }
  return NULL;
}

DirWithClassEntry* DirWithClassTable::basic_add(int index, const char* name, size_t len, unsigned int hashValue) {
  // One was added while aquiring the lock
  DirWithClassEntry* entry = lookup(index, name, hashValue);
  if (entry != NULL) {
    return entry;
  }
  char* dir_path = NEW_C_HEAP_ARRAY(char, len + 1, mtInternal);
  memcpy(dir_path, name, len + 1); 
  entry = new_entry(hashValue, dir_path);
  Hashtable<const char*, mtInternal>::add_entry(index, entry);
  return entry;
}

unsigned int DirWithClassTable::compute_hash(const char* name, int len) {
  return java_lang_String::hash_code(name, len);
}
