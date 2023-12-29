/*
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_ELASTIC_MAX_HEAP_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_ELASTIC_MAX_HEAP_HPP

#include "gc_implementation/shared/vmGCOperations.hpp"

#define EMH_LOG(fmt, ...)                           \
  if (TraceElasticMaxHeap)  {                       \
    ResourceMark rm;                                \
    gclog_or_tty->print_cr(fmt, ##__VA_ARGS__);     \
  }

class VM_ElasticMaxHeapOp : public VM_GC_Operation {
 private:
  bool skip_operation() const;
 protected:
  size_t _new_max_heap;
  bool   _resize_success;
 public:
  VM_ElasticMaxHeapOp(size_t new_max_heap);
  VMOp_Type type() const {
    return VMOp_ElasticMaxHeap;
  }
  bool resize_success() const {
    return _resize_success;
  }
};

// For ParallelScavengeHeap
class PS_ElasticMaxHeapOp : public VM_ElasticMaxHeapOp {
 public:
  PS_ElasticMaxHeapOp(size_t new_max_heap);
  virtual void doit();
  bool ps_old_gen_can_shrink(size_t new_limit, bool print_logs);
  bool ps_young_gen_can_shrink(size_t new_limit);
};

// For GenCollectedHeap
class Gen_ElasticMaxHeapOp : public VM_ElasticMaxHeapOp {
 public:
  Gen_ElasticMaxHeapOp(size_t new_max_heap);
  virtual void doit();
};

// For G1CollectedHeap
class G1_ElasticMaxHeapOp : public VM_ElasticMaxHeapOp {
 public:
  G1_ElasticMaxHeapOp(size_t new_max_heap);
  virtual void doit();
  bool g1_can_shrink(size_t _new_max_heap,
                   double maximum_used_percentage,
                   size_t max_heap_size);
  void g1_shrink_without_full_gc(size_t _new_max_heap);
};

class ElasticMaxHeapChecker: AllStatic {
 public:
  static void check_common_options();
  static void check_PS_options();
  static void check_G1_options();
  static void check_GenCollected_options();
};

class ElasticMaxHeapConfig: AllStatic {
 private:
  static size_t _initial_max_heap_size;
 public:
  static size_t initial_max_heap_size() { return _initial_max_heap_size; }
  static void set_initial_max_heap_size(size_t new_size) {
    _initial_max_heap_size = new_size;
  }
};
#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_ELASTIC_MAX_HEAP_HPP
