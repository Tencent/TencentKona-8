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

#ifndef SHARE_VM_CR_CODE_REVIVE_HASH_TABLE_HPP
#define SHARE_VM_CR_CODE_REVIVE_HASH_TABLE_HPP

#include "memory/allocation.hpp"
#include "utilities/hashtable.hpp"
#include "runtime/mutexLocker.hpp"

/*
 * entry for meta information for method and class, include:
 * 1. symbol name 
 * 2. identity
 * 3. loader type
 * 4. global index
 */
class MergePhaseMetaEntry : public HashtableEntry<Symbol*, mtSymbol> {
 private:
  int64_t  _identity;
  uint16_t _loader_type;
  int      _global_index;

 public:
  Symbol* symbol() const             { return literal(); }

  int64_t identity()  const          { return _identity; }
  void    set_identity(int64_t identity)        { _identity = identity; }
  int     global_index()             { return _global_index; }
  void    set_index(int index)       { _global_index = index; }
  uint16_t loader_type()             { return _loader_type; }
  void set_loader_type(uint16_t type)           { _loader_type = type; }
  
          
  MergePhaseMetaEntry* next() const {
    return (MergePhaseMetaEntry*)HashtableEntry<Symbol*, mtSymbol>::next();
  }
  
  MergePhaseMetaEntry** next_addr() {
    return (MergePhaseMetaEntry**)HashtableEntry<Symbol*, mtSymbol>::next_addr();
  }
};

/*
 * Record meta information in csa merge, include method and klass
 * use symbol and identity for each entry
 */
class MergePhaseMetaTable : public Hashtable<Symbol*, mtSymbol> {
 private:
  static MergePhaseMetaTable* _the_table;

  MergePhaseMetaEntry* bucket(int i) {
    return (MergePhaseMetaEntry*) Hashtable<Symbol*, mtSymbol>::bucket(i);
  }

  // the method is not MT-safe
  MergePhaseMetaEntry** bucket_addr(int i) {
    return (MergePhaseMetaEntry**) Hashtable<Symbol*, mtSymbol>::bucket_addr(i);
  }

  // lookup with name + identity + loader type
  MergePhaseMetaEntry* lookup(int index, Symbol* name, unsigned int hash, int64_t identity, uint16_t loader_type);

  // the method is not MT-safe
  MergePhaseMetaEntry* basic_add(int index, Symbol* name, unsigned int hashValue, int64_t identity, uint16_t loader_type);

  static unsigned int compute_hash(Symbol* sym) {
    // Use the regular identity_hash.
    return (unsigned int) sym->identity_hash();
  }

  MergePhaseMetaEntry* new_entry(unsigned int hash, Symbol* symbol, int64_t identity, uint16_t loader_type);

  void add_entry(int index, MergePhaseMetaEntry* new_entry) {
    Hashtable<Symbol*, mtSymbol>::add_entry(index,
      (HashtableEntry<Symbol*, mtSymbol>*)new_entry);
  }

  MergePhaseMetaTable();

 public:
  static MergePhaseMetaTable* the_table() { return _the_table; }

  static void create_table() {
    _the_table = new MergePhaseMetaTable();
  }

  static MergePhaseMetaEntry* add_meta(Symbol* name, int64_t identity, uint16_t loader_type);

  // lookup only, won't add
  static MergePhaseMetaEntry* lookup_only(Symbol* name, int64_t identity, uint16_t loader_type);
};

#endif // SHARE_VM_CR_CODE_REVIVE_HASH_TABLE_HPP
