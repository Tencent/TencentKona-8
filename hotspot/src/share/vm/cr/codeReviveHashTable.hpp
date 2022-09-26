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
#ifndef SHARE_VM_CR_CODE_REVIVE_HASH_TABLE_HPP
#define SHARE_VM_CR_CODE_REVIVE_HASH_TABLE_HPP

#include "memory/allocation.hpp"
#include "utilities/hashtable.hpp"
#include "runtime/mutexLocker.hpp"

class MergePhaseKlassResovleCacheEntry : public HashtableEntry<Symbol*, mtSymbol> {
 private:
  // the value can be null. 
  // if it is null, it means that the klass can't be resolved in merge stage
  Klass* _klass;

 public:
  Symbol* symbol() const             { return literal(); }

  Klass*  klass()  const             { return _klass; }
  void    set_klass(Klass* k)        { _klass = k; }
          
  MergePhaseKlassResovleCacheEntry* next() const {
    return (MergePhaseKlassResovleCacheEntry*)HashtableEntry<Symbol*, mtSymbol>::next();
  }
  
  MergePhaseKlassResovleCacheEntry** next_addr() {
    return (MergePhaseKlassResovleCacheEntry**)HashtableEntry<Symbol*, mtSymbol>::next_addr();
  }
};

/*
 * record klass information in csa merge
 * if the klass can't be resolved, the klass name is still recorded, and klass is NULL. 
 *
 */
class MergePhaseKlassResovleCacheTable : public Hashtable<Symbol*, mtSymbol> {
 private:
  static MergePhaseKlassResovleCacheTable* _the_table;

  MergePhaseKlassResovleCacheEntry* bucket(int i) {
    return (MergePhaseKlassResovleCacheEntry*) Hashtable<Symbol*, mtSymbol>::bucket(i);
  }

  // the method is not MT-safe
  MergePhaseKlassResovleCacheEntry** bucket_addr(int i) {
    return (MergePhaseKlassResovleCacheEntry**) Hashtable<Symbol*, mtSymbol>::bucket_addr(i);
  }

  MergePhaseKlassResovleCacheEntry* lookup(int index, Symbol* name, unsigned int hash);

  // the method is not MT-safe
  MergePhaseKlassResovleCacheEntry* basic_add(int index, Symbol* name, unsigned int hashValue);

  static unsigned int compute_hash(Symbol* sym) {
    // Use the regular identity_hash.
    return (unsigned int) sym->identity_hash();
  }

  MergePhaseKlassResovleCacheEntry* new_entry(unsigned int hash, Symbol* symbol);

  void add_entry(int index, MergePhaseKlassResovleCacheEntry* new_entry) {
    Hashtable<Symbol*, mtSymbol>::add_entry(index,
      (HashtableEntry<Symbol*, mtSymbol>*)new_entry);
  }

  void print_information();

  MergePhaseKlassResovleCacheTable();

 public:
  static MergePhaseKlassResovleCacheTable* the_table() { return _the_table; }

  static void create_table() {
    _the_table = new MergePhaseKlassResovleCacheTable();
  }

  static void add_klass(Symbol* name, Klass* k);

  // lookup only, won't add
  static MergePhaseKlassResovleCacheEntry* lookup_only(Symbol* name);

  static void print_cache_table_information();
};

// hashtable for directores in class path
class DirWithClassEntry : public HashtableEntry<const char*, mtInternal> {
 public:
  const char* dir_path() const            { return literal(); }
  bool is_in_classpath()                  { return _is_in_classpath; }
  void set_in_classpath(bool value)       { _is_in_classpath = value; } 

  DirWithClassEntry* next() const {
    return (DirWithClassEntry*)HashtableEntry<const char*, mtInternal>::next();
  }
 private:
  bool _is_in_classpath;
};

/*
 * 1. save stage:
 *   record the directory from which the klass is loaded
 * 2. merge stage:
 *   record the directory in classpath
 */
class DirWithClassTable : public Hashtable<const char*, mtInternal> {
 private:
  // The table of the directory with class
  static DirWithClassTable* _the_table;

  DirWithClassEntry* bucket(int i) {
    return (DirWithClassEntry*) Hashtable<const char*, mtInternal>::bucket(i);
  }

  DirWithClassEntry* lookup(int index, const char* name, unsigned int hash);

  DirWithClassEntry* basic_add(int index, const char* name, size_t len, unsigned int hashValue);

  static unsigned int compute_hash(const char* name, int len);

  DirWithClassEntry* new_entry(unsigned int hash, const char* symbol);

  void add_entry(int index, DirWithClassEntry* new_entry) {
    Hashtable<const char*, mtInternal>::add_entry(index,
      (HashtableEntry<const char*, mtInternal>*)new_entry);
  }

  DirWithClassTable();

public:
  static DirWithClassTable* the_table() { return _the_table; }

  static void create_table() {
    _the_table = new DirWithClassTable();
  }

  static DirWithClassEntry* lookup(const char* name, size_t len, bool add_real, TRAPS);
  
  //lookup only, won't add
  static bool lookup_only(const char* name, size_t len);
};


#endif // SHARE_VM_CR_CODE_REVIVE_HASH_TABLE_HPP
