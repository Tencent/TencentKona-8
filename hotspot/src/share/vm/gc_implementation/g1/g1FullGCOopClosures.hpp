/*
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCOOPCLOSURES_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCOOPCLOSURES_HPP

#include "memory/iterator.hpp"

class G1CollectedHeap;
class G1FullCollector;
class CMBitMap;
class G1FullGCMarker;

// Below are closures used by the G1 Full GC.
class G1IsAliveClosure : public BoolObjectClosure {
  CMBitMap* _bitmap;

public:
  G1IsAliveClosure(CMBitMap* bitmap) : _bitmap(bitmap) { }

  virtual bool do_object_b(oop p);
};

class G1FullKeepAliveClosure: public OopClosure {
  G1FullGCMarker* _marker;
  template <class T>
  inline void do_oop_nv(T* p);

public:
  G1FullKeepAliveClosure(G1FullGCMarker* pm) : _marker(pm) { }

  virtual void do_oop(oop* p);
  virtual void do_oop(narrowOop* p);
};

class G1MarkAndPushClosure : public MetadataAwareOopClosure/*ExtendedOopClosure*/ {
  G1FullGCMarker* _marker;
  uint _worker_id;

public:
  G1MarkAndPushClosure(uint worker, G1FullGCMarker* marker, ReferenceProcessor* ref) :
    _marker(marker),
    _worker_id(worker),
    MetadataAwareOopClosure(ref) { }

  template <class T> void do_oop_nv(T* p);
  virtual void do_oop(oop* p) { do_oop_nv(p);}
  virtual void do_oop(narrowOop* p) { do_oop_nv(p);}

  virtual bool do_metadata();
  virtual void do_klass(Klass* k);
  virtual void do_class_loader_data(ClassLoaderData* cld);
  virtual bool apply_to_weak_ref_discovered_field() { return true; }
  virtual ParallelFullGCPhase parallel_fullgc_phase() {return PARALLEL_FULLGC_MARK;}
};

class G1AdjustClosure : public ExtendedOopClosure {
public:
  template <class T> void do_oop_nv(T* p);
  virtual void do_oop(oop* p) { do_oop_nv(p);}
  virtual void do_oop(narrowOop* p) {do_oop_nv(p);}

  virtual bool apply_to_weak_ref_discovered_field() { return false; }
  virtual ParallelFullGCPhase parallel_fullgc_phase() {return PARALLEL_FULLGC_ADJUST;}
};

class G1FollowStackClosure: public VoidClosure {
  G1FullGCMarker* _marker;

public:
  G1FollowStackClosure(G1FullGCMarker* marker) : _marker(marker) {}
  virtual void do_void();
};

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_G1FULLGCOOPCLOSURES_HPP
