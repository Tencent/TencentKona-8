/*
 * Copyright (c) 2014, 2020, Oracle and/or its affiliates. All rights reserved.
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
#include "gc_implementation/shared/gcId.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/thread.inline.hpp"

uint GCId::_next_id = 0;

NamedThread* currentNamedthread() {
  assert(Thread::current()->is_Named_thread(), "This thread must be NamedThread");
  return (NamedThread*)Thread::current();
}

const GCId GCId::create() {
  return GCId(_next_id++);
}
const GCId GCId::peek() {
  return GCId(_next_id);
}
const GCId GCId::undefined() {
  return GCId(UNDEFINED);
}
const uint GCId::undefined_id() {
  return UNDEFINED;
}
bool GCId::is_undefined() const {
  return _id == UNDEFINED;
}

GCIdMark::GCIdMark() : _previous_gc_id(currentNamedthread()->gc_id()) {
  currentNamedthread()->set_gc_id(GCId::create().id());
}

GCIdMark::GCIdMark(uint gc_id) : _previous_gc_id(currentNamedthread()->gc_id()) {
  currentNamedthread()->set_gc_id(gc_id);
}

GCIdMark::~GCIdMark() {
  currentNamedthread()->set_gc_id(_previous_gc_id);
}
