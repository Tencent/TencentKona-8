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

#include "precompiled.hpp"
#include "classfile/javaClasses.hpp"
#include "cr/codeReviveHashTable.hpp"
#include "cr/revive.hpp"

MergePhaseMetaTable* MergePhaseMetaTable::_the_table = NULL;

MergePhaseMetaTable::MergePhaseMetaTable() : Hashtable<Symbol*, mtSymbol>(4049, sizeof (MergePhaseMetaEntry)) {}

MergePhaseMetaEntry* MergePhaseMetaTable::new_entry(unsigned int hash, Symbol* name, int64_t identity, uint16_t loader_type) {
  MergePhaseMetaEntry* entry = (MergePhaseMetaEntry*)Hashtable<Symbol*, mtSymbol>::new_entry(hash, name);
  entry->set_identity(identity);
  entry->set_loader_type(loader_type);
  return entry;
}

MergePhaseMetaEntry* MergePhaseMetaTable::add_meta(Symbol* name, int64_t identity, uint16_t loader_type) {
  unsigned int hashValue = compute_hash(name);
  int index = the_table()->hash_to_index(hashValue);
  
  MergePhaseMetaEntry* e = the_table()->basic_add(index, name, hashValue, identity, loader_type);
  return e;
}

MergePhaseMetaEntry* MergePhaseMetaTable::lookup_only(Symbol* name, int64_t identity, uint16_t loader_type) {
  unsigned int hash = compute_hash(name);
  int index = the_table()->hash_to_index(hash);

  return the_table()->lookup(index, name, hash, identity, loader_type);
}

MergePhaseMetaEntry* MergePhaseMetaTable::lookup(int index, Symbol* name, unsigned int hash, int64_t identity, uint16_t loader_type) {
  int count = 0;
  for (MergePhaseMetaEntry* e = bucket(index); e != NULL; e = e->next()) {
    // name + identity + loader type
    if (e->hash() == hash && e->symbol() == name && e->identity() == identity && e->loader_type() == loader_type) {
      return e;
    }
  }
  return NULL;
}

MergePhaseMetaEntry* MergePhaseMetaTable::basic_add(int index, Symbol* name, unsigned int hashValue, int64_t identity, uint16_t loader_type) {
  MergePhaseMetaEntry* entry = new_entry(hashValue, name, identity, loader_type);
  Hashtable<Symbol*, mtSymbol>::add_entry(index, entry);
  return entry;
}

