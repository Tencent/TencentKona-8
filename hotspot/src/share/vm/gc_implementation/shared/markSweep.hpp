/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_MARKSWEEP_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_MARKSWEEP_HPP

#include "gc_interface/collectedHeap.hpp"
#include "gc_implementation/concurrentMarkSweep/compactibleFreeListSpace.hpp"
#include "memory/genCollectedHeap.hpp"
#include "memory/space.hpp"
#include "memory/universe.hpp"
#include "oops/markOop.hpp"
#include "oops/oop.hpp"
#include "runtime/timer.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/stack.hpp"
#include "utilities/taskqueue.hpp"

class ReferenceProcessor;
class DataLayout;
class SerialOldTracer;
class STWGCTimer;

// MarkSweep takes care of global mark-compact garbage collection for a
// GenCollectedHeap using a four-phase pointer forwarding algorithm.  All
// generations are assumed to support marking; those that can also support
// compaction.
//
// Class unloading will only occur when a full gc is invoked.

// declared at end
class PreservedMark;
class PMSMarkBitMap;
class PMSRegion;
class PMSRegionArray;
class PMSRegionArraySet;
typedef OverflowTaskQueue<PMSRegion*, mtGC>                  PMSRegionTaskQueue;
typedef GenericTaskQueueSet<PMSRegionTaskQueue, mtGC>        PMSRegionTaskQueueSet;

class MarkSweep : AllStatic {
  //
  // Inline closure decls
  //
  class FollowRootClosure: public OopsInGenClosure {
   public:
    virtual void do_oop(oop* p);
    virtual void do_oop(narrowOop* p);
  };

  class MarkAndPushClosure: public OopClosure {
   public:
    virtual void do_oop(oop* p);
    virtual void do_oop(narrowOop* p);
  };

  class FollowStackClosure: public VoidClosure {
   public:
    virtual void do_void();
  };

  class AdjustPointerClosure: public OopsInGenClosure {
   public:
    virtual void do_oop(oop* p);
    virtual void do_oop(narrowOop* p);
  };

  // Used for java/lang/ref handling
  class IsAliveClosure: public BoolObjectClosure {
   public:
    virtual bool do_object_b(oop p);
  };

  class KeepAliveClosure: public OopClosure {
   protected:
    template <class T> void do_oop_work(T* p);
   public:
    virtual void do_oop(oop* p);
    virtual void do_oop(narrowOop* p);
  };

  //
  // Friend decls
  //
  friend class AdjustPointerClosure;
  friend class KeepAliveClosure;
  friend class VM_MarkSweep;
  friend class PMSRefProcTask;
  friend void marksweep_init();

 public:
  static void initialize_parallel_mark_sweep();
  //
  // Vars
  //
 protected:
  // Total invocations of a MarkSweep collection
  static uint _total_invocations;

  // Traversal stacks used during phase1
  static Stack<oop, mtGC>                      _marking_stack;
  static Stack<ObjArrayTask, mtGC>             _objarray_stack;

  // Space for storing/restoring mark word
  static Stack<markOop, mtGC>                  _preserved_mark_stack;
  static Stack<oop, mtGC>                      _preserved_oop_stack;
  static size_t                          _preserved_count;
  static size_t                          _preserved_count_max;
  static PreservedMark*                  _preserved_marks;

  // For parallel mark sweep (PMS)
  static PMSMarkBitMap*                  _pms_mark_bit_map;
  static PMSRegionArraySet*              _pms_region_array_set;
  static volatile intptr_t               _pms_mark_counter;
  static PMSRegionTaskQueueSet*          _pms_region_task_queues;

  // Reference processing (used in ...follow_contents)
  static ReferenceProcessor*             _ref_processor;

  static STWGCTimer*                     _gc_timer;
  static SerialOldTracer*                _gc_tracer;

  // Non public closures
  static KeepAliveClosure keep_alive;

  // Class unloading. Clear weak refs in MDO's (ProfileData)
  // at the end of the marking phase.
  static void follow_mdo_weak_refs();
  static void follow_mdo_weak_refs_helper(Stack<DataLayout*, mtGC>* revisit_mdo_stack);

  // Debugging
  static void trace(const char* msg) PRODUCT_RETURN;

 public:
  // Public closures
  static IsAliveClosure       is_alive;
  static FollowRootClosure    follow_root_closure;
  static MarkAndPushClosure   mark_and_push_closure;
  static FollowStackClosure   follow_stack_closure;
  static CLDToOopClosure      follow_cld_closure;
  static AdjustPointerClosure adjust_pointer_closure;
  static CLDToOopClosure      adjust_cld_closure;

  // Accessors
  static uint total_invocations() { return _total_invocations; }

  // Reference Processing
  static ReferenceProcessor* const ref_processor() { return _ref_processor; }

  static STWGCTimer* gc_timer() { return _gc_timer; }
  static SerialOldTracer* gc_tracer() { return _gc_tracer; }

  static PMSMarkBitMap*         pms_mark_bit_map()       { return _pms_mark_bit_map; }
  static PMSRegionArraySet*     pms_region_array_set()   { return _pms_region_array_set; }
  static PMSRegionTaskQueueSet* pms_region_task_queues() { return _pms_region_task_queues; }


  // Call backs for marking
  static void mark_object(oop obj);
  static bool par_mark_object(oop obj);
  static bool is_object_marked(oop obj);
  // Mark pointer and follow contents.  Empty marking stack afterwards.
  template <class T> static inline void follow_root(T* p);

  // Check mark and maybe push on marking stack
  template <class T> static void mark_and_push(T* p);

  static inline void push_objarray(oop obj, size_t index);

  static void follow_stack();   // Empty marking stack.

  static void follow_klass(Klass* klass);

  static void follow_class_loader(ClassLoaderData* cld);

  static void preserve_mark(oop p, markOop mark);
                                // Save the mark word so it can be restored later
  static void adjust_marks();   // Adjust the pointers in the preserved marks table
  static void restore_marks();  // Restore the marks that we saved in preserve_mark

  static void adjust_marks_helper(Stack<markOop, mtGC>* preserved_mark_stack,
                                  Stack<oop, mtGC>*     preserved_oop_stack,
                                  size_t          preserved_count,
                                  PreservedMark*  preserved_marks);
  static void restore_marks_helper(Stack<markOop, mtGC>* preserved_mark_stack,
                                   Stack<oop, mtGC>*     preserved_oop_stack,
                                   size_t          preserved_count,
                                   PreservedMark*  preserved_marks,
                                   bool            should_release);

  template <class T> static inline void adjust_pointer(T* p);
};

class PreservedMark VALUE_OBJ_CLASS_SPEC {
private:
  oop _obj;
  markOop _mark;

public:
  void init(oop obj, markOop mark) {
    _obj = obj;
    _mark = mark;
  }

  void adjust_pointer() {
    MarkSweep::adjust_pointer(&_obj);
  }

  void restore() {
    _obj->set_mark(_mark);
  }
};

//
// Parallel Mark Sweep (PMS) (aka CMSParallelFullGC) support code
//

typedef OverflowTaskQueue<oop, mtGC>                         ObjTaskQueue;
typedef GenericTaskQueueSet<ObjTaskQueue, mtGC>              ObjTaskQueueSet;
typedef OverflowTaskQueue<ObjArrayTask, mtGC>                ObjArrayTaskQueue;
typedef GenericTaskQueueSet<ObjArrayTaskQueue, mtGC>         ObjArrayTaskQueueSet;

// A region for PMS. A unit of parallelism. It's a fixed-size (1 MB by
// default) region (except for the last part of a space) of the space
// of the heap.
class PMSRegion {
 public:
  class RegionDest : public CHeapObj<mtGC> {
    // CHeapObj because it's accessed by different threads (workers)
    // and does not have a scoped lifetime. Deallocated by
    // PMSRegion::cleanup().
   public:
    RegionDest() :
        _from_mr(MemRegion()), _to_space(NULL),
        _to_space_off(0), _to_space_size(0) {}
    RegionDest(MemRegion from_mr, CompactibleSpace* to_space,
               size_t to_space_off, size_t to_space_size) :
        _from_mr(from_mr), _to_space(to_space),
        _to_space_off(to_space_off), _to_space_size(to_space_size) {}
    MemRegion         _from_mr;              // the address range of the subregion
    CompactibleSpace* _to_space;             // the to-space
    size_t            _to_space_off;         // the word offset in the to-space
    size_t            _to_space_size;        // the word size in the to_space
    void log(outputStream* os) {
      os->print_cr("RegionDest(from:" PTR_FORMAT "-" PTR_FORMAT "(" SIZE_FORMAT "),"
                   "to:" PTR_FORMAT "-" PTR_FORMAT "(" SIZE_FORMAT "))",
                   p2i(_from_mr.start()), p2i(_from_mr.end()), _from_mr.byte_size(),
                   p2i(_to_space->bottom() + _to_space_off),
                   p2i(_to_space->bottom() + _to_space_off + _to_space_size),
                   _to_space_size * HeapWordSize);
    }
  };
  enum EvacState { NOT_EVAC, BEING_EVAC, HAS_BEEN_EVAC };
  // Evacuation state for parallel compaction
  jbyte _evac_state;
  // Used for synchronization in the parallel compact phase
  Monitor* _monitor;

 private:
  intptr_t  _index;
  MemRegion _mr;
  CompactibleSpace* _space;

  // The total word size of all the live objects in this region. This
  // includes all the live objects whose starting addresses are in the
  // region (but objects' tails can be outside of this region).
  intptr_t  _live_size;      // for YG where block_size == obj->size()
  intptr_t  _cfls_live_size; // for CFLS (OG and PG) where
                             // block_size = adjustObjectSize(obj->size())

  HeapWord* _beginning_of_live; // The beginning of the first live object in a region
  HeapWord* _end_of_live;       // The end (exclusive) end of the last live object in a region.
  HeapWord* _first_moved;       // The first live object that moves (for compaction) in a region.

  // Where the live objects in this region are evacuated.
  GrowableArray<RegionDest*>* _destinations;

  // The (other) regions that must be evacuated before this region can
  // be evacuated. In parallel compaction, a region cannot be
  // evacuated until the destination region is evacuated so that no
  // objects are overwritten before they are copied.
  GrowableArray<PMSRegion*>* _dependencies;

 public:
  PMSRegion() : _index(-1), _mr(MemRegion(NULL, (size_t)0)), _evac_state(HAS_BEEN_EVAC),
                _monitor(NULL), _destinations(NULL), _dependencies(NULL) {}
  PMSRegion(intptr_t index, HeapWord* start, size_t size, CompactibleSpace* space) :
      _index(index), _mr(MemRegion(start, size)), _space(space),
      _live_size(0), _cfls_live_size(0), _destinations(new GrowableArray<RegionDest*>()),
      _dependencies(new GrowableArray<PMSRegion*>()),
      _beginning_of_live(start + size), _end_of_live(start), _first_moved(start + size),
      _evac_state(HAS_BEEN_EVAC) {
    _monitor = new Monitor(Mutex::barrier,               // rank
                           "MarkSweep Region monitor",   // name
                           Mutex::_allow_vm_block_flag); // allow_vm_block
  }
  // Because this object is resource-allocated, a destructor won't be
  // called. Use this function to reclaim resources.
  void cleanup() {
    if (_monitor != NULL) {
      delete _monitor;
    }
    if (_destinations != NULL) {
      int len = _destinations->length();
      for (int i = 0; i < len; i++) {
        delete _destinations->at(i);
      }
    }
  }
  intptr_t  index() { return _index; }
  HeapWord* start() { return _mr.start(); }
  HeapWord* end()   { return _mr.end(); }
  size_t    size()  { return _mr.word_size(); }
  CompactibleSpace* space() { return _space; }
  size_t    live_size() { return (size_t)_live_size; }
  size_t    cfls_live_size() { return (size_t)_cfls_live_size; }
  inline void      add_to_live_size(size_t word_size) {
    Atomic::add_ptr((intptr_t)word_size, &_live_size);
  }
  inline void      add_to_cfls_live_size(size_t word_size) {
    Atomic::add_ptr((intptr_t)word_size, &_cfls_live_size);
  }
  GrowableArray<RegionDest*>* destinations() { return _destinations; }
  void      add_destination(MemRegion from_mr, CompactibleSpace* to_space,
                            size_t to_space_off, size_t to_space_size) {
    RegionDest* d = new RegionDest(from_mr, to_space,
                                   to_space_off, to_space_size);
    _destinations->append(d);
  }
  RegionDest* find_destination(HeapWord* addr) {
    assert(_mr.start() <= addr && addr < _mr.end(), "Must be in the region");
    int len = _destinations->length();
    for (int i = 0; i < len; i++) {
      RegionDest* d = _destinations->at(i);
      if (d->_from_mr.contains(addr)) {
        return d;
      }
    }
    ShouldNotReachHere();
    return NULL;
  }

  void log_destinations(outputStream* os) {
    gclog_or_tty->print("Forwarding region " PTR_FORMAT "-" PTR_FORMAT "(" SIZE_FORMAT "): ",
                        p2i(_mr.start()), p2i(_mr.end()), _mr.byte_size());
    for (int i = 0; i < _destinations->length(); i++) {
      RegionDest* d = _destinations->at(i);
      MemRegion dest_mr = MemRegion(d->_to_space->bottom() + d->_to_space_off,
                                    d->_to_space_size);
      gclog_or_tty->print("%d: " PTR_FORMAT "-" PTR_FORMAT "(" SIZE_FORMAT ") -> "
                          PTR_FORMAT "-" PTR_FORMAT "(" SIZE_FORMAT ") ",
                          i, p2i(d->_from_mr.start()), p2i(d->_from_mr.end()), d->_from_mr.byte_size(),
                          p2i(dest_mr.start()), p2i(dest_mr.end()), dest_mr.byte_size());
    }
    gclog_or_tty->cr();
  }

  GrowableArray<PMSRegion*>* dependencies() { return _dependencies; }

  void      record_stats(HeapWord* beginning_of_live,
                         HeapWord* end_of_live,
                         HeapWord* first_moved) {
    _beginning_of_live = beginning_of_live;
    _end_of_live = end_of_live;
    _first_moved = first_moved;
  }
  HeapWord* beginning_of_live() { return _beginning_of_live; }
  HeapWord* end_of_live() { return _end_of_live; }
  HeapWord* first_moved() { return _first_moved; }
  MemRegion live_range() {
    if (_beginning_of_live < _end_of_live) {
      return MemRegion(_beginning_of_live, _end_of_live);
    }
    return MemRegion();
  }
};

// An array of PMSRegions. There is one per space. The relevant spaces
// under CMS are eden, two survivor spaces (they are the three
// ContiguousSpace's in the young gen), the old gen (the entire
// generation is one CompactibleFreeListSpace). Note that the
// last PMSRegion of a space can be smaller than the standard fixed
// size (1 MB by default) (as in a remainder.)
class PMSRegionArray {
 private:
  CompactibleSpace* _space;
  MemRegion         _mr;
  PMSRegion*        _regions;
  size_t            _region_word_size;
  size_t            _n_regions;

  void reset_regions(MemRegion mr, size_t region_byte_size) {
    _mr = mr;
    _region_word_size = region_byte_size/HeapWordSize;
    assert(region_byte_size % HeapWordSize == 0, "Not word aligned?");
    size_t mr_size = mr.word_size();
    _n_regions = mr_size / _region_word_size;
    if (mr_size % _region_word_size != 0) {
      // The last region is of an odd size
      _n_regions++;
    }
    _regions = NEW_RESOURCE_ARRAY(PMSRegion, _n_regions);
    size_t index = 0;
    HeapWord* p = mr.start();
    for (; p < mr.end(); p += _region_word_size) {
      HeapWord* r_start = p;
      HeapWord* r_end = p + _region_word_size;
      if (mr.end() < r_end) {
        // The last region is of an odd size
        r_end = mr.end();
        assert(pointer_delta(r_end, r_start) < _region_word_size,
               "the remainder should be smaller than the standard region size");
      }
      _regions[index] = PMSRegion(index, r_start, r_end - r_start, _space);
      index++;
    }
    assert(index == _n_regions, "sanity check");
  }

 public:
  PMSRegionArray() : _space(NULL) {}
  PMSRegionArray(CompactibleSpace* space, size_t region_byte_size) : _space(space) {
    reset_regions(MemRegion(space->bottom(), space->end()), region_byte_size);
  }
  // Because this object is resource-allocated, a destructor won't be
  // called. Use this function to reclaim resources.
  void cleanup() {
    for (size_t i = 0; i < _n_regions; i++) {
      _regions[i].cleanup();
    }
  }
  inline bool contains(HeapWord* addr) {
    return _mr.start() <= addr && addr < _mr.end();
  }
  CompactibleSpace* space() { return _space; }
  // The first region
  PMSRegion* begin() { return &_regions[0]; }
  // The last region
  PMSRegion* end()   { return &_regions[_n_regions - 1]; }
  size_t region_word_size() { return _region_word_size; }
  size_t region_byte_size() { return _region_word_size * HeapWordSize; }
  size_t n_regions() { return _n_regions; }
  inline PMSRegion* region_for_index(size_t i) {
    // Use '<' below so we don't allow the sentinel end region here
    assert(0 <= i && i < _n_regions, "Out of bounds");
    return &_regions[i];
  }
  inline PMSRegion* region_for_addr(HeapWord* addr) {
    return region_for_index(addr_to_index(addr));
  }
  // Returns the index of the region that contains addr
  inline size_t addr_to_index(HeapWord* addr) {
    assert(_mr.start() <= addr && addr < _mr.end(), "Out of bounds");
    size_t index = pointer_delta(addr, _mr.start()) / _region_word_size;
    assert(0 <= index && index < _n_regions, "Index out of bounds");
    return index;
  }
  void record_space_live_range_for_compaction() {
    HeapWord* beginning_of_live = _space->end();
    HeapWord* end_of_live = _space->bottom();
    HeapWord* first_moved = _space->end();
    PMSRegion* first_r = begin();
    PMSRegion* last_r = end();
    for (PMSRegion* r = first_r; r <= last_r; r++) {
      if (r->beginning_of_live() < r->end()) {
        beginning_of_live = r->beginning_of_live();
        break;
      }
    }
    for (PMSRegion* r = last_r; r >= first_r; r--) {
      if (r->start() < r->end_of_live()) {
        end_of_live = r->end_of_live();
        break;
      }
    }
    for (PMSRegion* r = first_r; r <= last_r; r++) {
      if (r->first_moved() < r->end()) {
        first_moved = r->first_moved();
        break;
      }
    }
    if (LogCMSParallelFullGC) {
      gclog_or_tty->print_cr("beginning_of_live=" PTR_FORMAT,
                             p2i(beginning_of_live));
      gclog_or_tty->print_cr("end_of_live=" PTR_FORMAT,
                             p2i(end_of_live));
      gclog_or_tty->print_cr("first_moved=" PTR_FORMAT,
                             p2i(first_moved));
    }
    assert(beginning_of_live <= first_moved, "sanity check");
    _space->set_live_range_for_compaction(beginning_of_live,
                                          end_of_live,
                                          first_moved);
  }
  void compute_compact_dependencies();
  void mark_dense_prefix_as_evacuated() {
    if (_space->end_of_live() <= _space->first_moved()) {
      // No objects moved. Nothing to compact.
      return;
    }
    // Mark the 'dense prefix' regions, which have no objects that
    // will move in this compaction, evacuated. Note that the last
    // such region may be a destination of an object from a
    // region. If we don't mark them evacuated here, the compaction
    // routine (phase 4) may hang because no one will mark the last
    // such region evacuated.
    PMSRegion* first_r = region_for_addr(_space->first_moved());
    for (PMSRegion* r = begin(); r < first_r; r++) {
      r->_evac_state = PMSRegion::HAS_BEEN_EVAC;
    }
  }
};

// A set of PMSRegionArray's. There is one for the entire heap.
class PMSRegionArraySet : public ResourceObj {
 private:
  class GetSpacesClosure : public SpaceClosure {
   private:
    GrowableArray<Space*>* _spaces;
   public:
    GetSpacesClosure() {
      _spaces = new GrowableArray<Space*>();
    }
    void do_space(Space* s) {
      _spaces->append(s);
    }
    GrowableArray<Space*>* spaces() { return _spaces; }
  };
  class SizeLiveObjectClosure : public BitMapClosure {
   private:
    PMSMarkBitMap* _mark_bit_map;
    Space*      _space;
    size_t      _live_size;
    size_t      _cfls_live_size;
   public:
    SizeLiveObjectClosure(PMSMarkBitMap* mark_bit_map, Space* space)
        : _mark_bit_map(mark_bit_map), _space(space),
          _live_size(0), _cfls_live_size(0) {
    }
    bool do_bit(size_t offset);
    size_t live_size() { return _live_size; }
    size_t cfls_live_size() { return _cfls_live_size; }
  };

  enum space_index {
    EDEN_SPACE = 0,
    S0_SPACE   = 1,
    S1_SPACE   = 2,
    CMS_SPACE  = 3,
    N_SPACES   = 4  // The number of spaces
  };

  // Spaces and their bottom addresses
  PMSRegionArray          _arrays[N_SPACES];
  CompactibleSpace*       _spaces[N_SPACES];
  HeapWord*               _space_bottoms[N_SPACES];

 public:
  PMSRegionArraySet() {
    GetSpacesClosure cl;
    GenCollectedHeap* gch = GenCollectedHeap::heap();
#ifdef ASSERT
    MemRegion reserved = gch->reserved_region();
    MemRegion yg_reserved = gch->get_gen(0)->reserved();
    MemRegion og_reserved = gch->get_gen(1)->reserved();
    if (LogCMSParallelFullGC) {
      gclog_or_tty->print_cr("reserved: " PTR_FORMAT "-" PTR_FORMAT "(" SIZE_FORMAT ")",
                             p2i(reserved.start()), p2i(reserved.end()), reserved.byte_size());
      gclog_or_tty->print_cr("yg_reserved: " PTR_FORMAT "-" PTR_FORMAT "(" SIZE_FORMAT ")",
                             p2i(yg_reserved.start()), p2i(yg_reserved.end()), yg_reserved.byte_size());
      gclog_or_tty->print_cr("og_reserved: " PTR_FORMAT "-" PTR_FORMAT "(" SIZE_FORMAT ")",
                             p2i(og_reserved.start()), p2i(og_reserved.end()), og_reserved.byte_size());
    }
    assert(reserved.contains(yg_reserved), "Huh?");
    assert(reserved.contains(og_reserved), "Huh?");
    assert(reserved.start() == yg_reserved.start(), "Huh?");
    assert(yg_reserved.end() == og_reserved.start(), "Huh?");
    assert(og_reserved.end() == reserved.end(), "Huh?");
#endif
    gch->space_iterate(&cl);
    GrowableArray<Space*>* spaces = cl.spaces();
    // Five spaces (eden, s0, s1, OG, and PG.)
    assert(spaces->length() == N_SPACES, "wrong number of spaces");
    for (int i = 0; i < N_SPACES; i++) {
      Space* s = spaces->at(i);
      CompactibleSpace* cs = s->toCompactibleSpace();
      assert(cs != NULL, "Must be a CompactibleSpace");
      _spaces[i] = cs;
    }
    // The spaces come in the ascending address order of 0:eden, 1:s0,
    // 2:s1, 3:og. Note s0 and s1 can be in a reverse order. Fix it if so.
    if (_spaces[2]->bottom() < _spaces[1]->bottom()) {
      CompactibleSpace* tmp = _spaces[2];
      _spaces[2] = _spaces[1];
      _spaces[1] = tmp;
    }
#ifdef ASSERT
    for (int i = 0; i < N_SPACES - 1; i++) {
      assert(_spaces[i]->bottom() < _spaces[i + 1]->bottom(),
             "The spaces must be in the ascending address order");
    }
#endif
    for (int i = 0; i < N_SPACES; i++) {
      _arrays[i] = PMSRegionArray(_spaces[i], CMSParallelFullGCHeapRegionSize);
      _space_bottoms[i] = _spaces[i]->bottom();
    }
  }

  // Because this object is resource-allocated, a destructor won't be
  // called. Use this function to reclaim resources.
  void cleanup() {
    for (int i = 0; i < N_SPACES; i++) {
      _arrays[i].cleanup();
    }
  }

  CompactibleSpace* cms_space() { return _spaces[CMS_SPACE]; }

  inline PMSRegionArray* region_array_for(HeapWord* addr) {
    for (int i = 0; i < N_SPACES; i++) {
      PMSRegionArray* ra = &_arrays[i];
      if (ra->contains(addr)) {
        return ra;
      }
    }
    return NULL;
  }

  // A faster region array lookup. This is important for minimizing
  // marking overhead (MarkSweep::par_mark_object()).
  inline PMSRegionArray* fast_region_array_for(HeapWord* addr) {
    PMSRegionArray* res;
    if (_space_bottoms[CMS_SPACE] <= addr) {
      res = &_arrays[CMS_SPACE];
    } else {
      if (_space_bottoms[S0_SPACE] <= addr) {
        if (_space_bottoms[S1_SPACE] <= addr) {
          res = &_arrays[S1_SPACE];
        } else {
          res = &_arrays[S0_SPACE];
        }
      } else {
        res = &_arrays[EDEN_SPACE];
      }
    }
    assert(res->contains(addr), "The result region array must contain the address");
    return res;
  }

  PMSRegionArray* region_array_for(Space* space) {
    for (int i = 0; i < N_SPACES; i++) {
      PMSRegionArray* ra = &_arrays[i];
      if (ra->space() == space) {
        return ra;
      }
    }
    return NULL;
  }

  void verify_live_size();
};

// This is an AbstractGangTask used in Phase 1 (marking) of the
// parallel mark sweep.
class PMSMarkTask : public AbstractGangTask {
  int                    _level;
  WorkGang*              _workers;
  int                    _n_workers;
  ObjTaskQueueSet*       _task_queues;
  ObjArrayTaskQueueSet*  _objarray_task_queues;
  ParallelTaskTerminator _terminator;
  ParallelTaskTerminator _objarray_terminator;
 public:
  PMSMarkTask(int level, int n_workers, WorkGang* workers,
              ObjTaskQueueSet* task_queues,
              ObjArrayTaskQueueSet* objarray_task_queues) :
      AbstractGangTask("genMarkSweep parallel mark"),
      _level(level), _n_workers(n_workers), _workers(workers),
      _task_queues(task_queues), _objarray_task_queues(objarray_task_queues),
      _terminator(ParallelTaskTerminator(n_workers, task_queues)),
      _objarray_terminator(ParallelTaskTerminator(n_workers, objarray_task_queues)) {}
  void work(uint worker_id);
  void do_steal_work(uint worker_id);
};

// This is an AbstractGangTask used in Phase 3 (adjust pointers) of
// the parallel mark sweep.
class PMSParAdjustTask : public AbstractGangTask {
 private:
  WorkGang*              _workers;
  int                    _n_workers;
  PMSRegionTaskQueueSet*    _task_queues;
  ParallelTaskTerminator _terminator;
  BitMapClosure*         _cl;
 public:
  PMSParAdjustTask(int n_workers, WorkGang* workers,
                   PMSRegionTaskQueueSet* task_queues,
                   BitMapClosure* cl) :
      AbstractGangTask("genMarkSweep parallel adjust"),
      _n_workers(n_workers), _workers(workers),
      _task_queues(task_queues), _cl(cl),
      _terminator(ParallelTaskTerminator(n_workers, task_queues)) {
    assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");
  }
  void work(uint worker_id);
  void do_steal_work(uint worker_id);
};

// This is an AbstractGangTask used in Phase 3 (adjust pointers) of
// the parallel mark sweep where it adjuts the pointers in the GC
// roots.
class PMSAdjustRootsTask : public AbstractGangTask {
  int                    _level;
  WorkGang*              _workers;
  int                    _n_workers;
 public:
  PMSAdjustRootsTask(int level, int n_workers, WorkGang* workers) :
      AbstractGangTask("genMarkSweep parallel adjust roots"),
      _level(level), _n_workers(n_workers), _workers(workers) {}
  void work(uint worker_id);
};

// This is an AbstractGangTask used in Phase 3 (adjust pointers) of
// the parallel mark sweep where it adjusts the pointers in the
// 'preserved marks' (the mark header word for each object is
// saved/restored before/after the parallel mark sweep.)
class PMSParAdjustPreservedMarksTask : public AbstractGangTask {
  WorkGang*              _workers;
  int                    _n_workers;
 public:
  PMSParAdjustPreservedMarksTask(int n_workers, WorkGang* workers) :
      AbstractGangTask("MarkSweep parallel adjust preserved marks"),
      _n_workers(n_workers), _workers(workers) {
    assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");
  }
  void work(uint worker_id);
};

// This is an AbstractGangTask used in Phase 4 (compaction/evacuation)
// of the parallel mark sweep.
class PMSParEvacTask : public AbstractGangTask {
 private:
  WorkGang*              _workers;
  int                    _n_workers;
  CompactibleSpace*      _space;
  BitMapClosure*         _cl;
 public:
  PMSParEvacTask(int n_workers, WorkGang* workers,
                 CompactibleSpace* space, BitMapClosure* cl) :
      AbstractGangTask("genMarkSweep parallel evac"),
      _n_workers(n_workers), _workers(workers), _space(space), _cl(cl) {
    assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");
  }
  void work(uint worker_id);
  //    void do_steal_work(uint worker_id);
};

// The mark bit map used for the parallel mark sweep.
class PMSMarkBitMap : public CHeapObj<mtGC> {
 private:
  int       _shifter; // We allocate one bit per 2^_shifter in the bit map
  HeapWord* _start;   // The beginning address of the region covered by this mark bit map
  size_t    _size;    // The word size of the covered region == The number of bits
  BitMap    _bits;    // The underlying BitMap
 public:
  PMSMarkBitMap();
  PMSMarkBitMap(MemRegion underlying_memory);
  inline size_t addr_to_bit(HeapWord* addr) const;
  inline HeapWord* bit_to_addr(size_t bit) const;
  inline bool mark(oop obj);
  inline bool is_marked(oop obj) const;
  inline bool iterate(BitMapClosure* cl);
  inline bool iterate(BitMapClosure* cl, size_t left, size_t right);
  inline size_t count_one_bits() const;
  inline void clear();
  inline bool isAllClear() const;
};

// The BitMapClosure used in Phase 3 (adjust pointers) of the parallel
// mark sweep.
class PMSAdjustClosure : public BitMapClosure {
 private:
  PMSMarkBitMap* _mark_bit_map;
 public:
  PMSAdjustClosure(PMSMarkBitMap* mark_bit_map) :
      _mark_bit_map(mark_bit_map) {
    assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");
  }
  bool do_bit(size_t offset) {
    HeapWord* addr = _mark_bit_map->bit_to_addr(offset);
    oop obj = oop(addr);
    assert(_mark_bit_map->is_marked(obj), "sanity check");
    const intx interval = PrefetchScanIntervalInBytes;
    Prefetch::write(addr, interval);
    size_t size = obj->adjust_pointers();
    return true;
  }
};

// This BitMapClosure is used in Phase 2 (forwarding) of the parallel
// mark sweep for finding a split point of a PMSRegion. A region may
// need to be split into multiple subparts in terms of what space they
// are evacuated into (copied to) when one destination space is
// completely filled.
class PMSFindRegionSplitPointClosure : public BitMapClosure {
 private:
  PMSMarkBitMap*    _mark_bit_map;
  HeapWord*         _compact_top;
  CompactibleSpace* _dst_space;
  HeapWord*         _dst_space_end;
  size_t            _dst_space_minimum_free_block_size;
  bool              _is_dst_cfls;
  HeapWord*         _split_point;
  size_t            _live_size_up_to_split_point;
  size_t            _cfls_live_size_up_to_split_point;
 public:
  PMSFindRegionSplitPointClosure(PMSMarkBitMap* mark_bit_map,
                                 HeapWord* compact_top,
                                 CompactibleSpace* dst_space)
      : _mark_bit_map(mark_bit_map), _dst_space(dst_space),
        _dst_space_end(dst_space->end()),
        _dst_space_minimum_free_block_size(
            dst_space->minimum_free_block_size()),
        _is_dst_cfls(dst_space->isCompactibleFreeListSpace()),
        _compact_top(compact_top), _split_point(NULL),
        _live_size_up_to_split_point(0),
        _cfls_live_size_up_to_split_point(0) {
  }
  bool do_bit(size_t offset) {
    HeapWord* addr = _mark_bit_map->bit_to_addr(offset);
    oop obj = oop(addr);
    size_t size = obj->size();
    size_t cfls_size = CompactibleFreeListSpace::adjustObjectSize(obj->size());
    size_t dst_size = _is_dst_cfls ? cfls_size : size;
    HeapWord* dst_space_end = _dst_space_end;
    if (_compact_top + dst_size > dst_space_end) {
      _split_point = addr;
      return false; /* exit out of iteration */
    }
    if (_is_dst_cfls &&
        _compact_top + dst_size +
          _dst_space_minimum_free_block_size > dst_space_end &&
        _compact_top + dst_size != _dst_space_end) {
      /* CFLS cannot leave a residual fragment smaller than */
      /* MinChunkSize.  So split right there. */
      _split_point = addr;
      return false;
    }
    _compact_top += dst_size;
    _live_size_up_to_split_point += size;
    _cfls_live_size_up_to_split_point += cfls_size;
    return true; /* continue iteration */
  }
  HeapWord* split_point() { return _split_point; }
  HeapWord* compact_top() { return _compact_top; }
  size_t live_size_up_to_split_point() {
    return _live_size_up_to_split_point;
  }
  size_t cfls_live_size_up_to_split_point() {
    return _cfls_live_size_up_to_split_point;
  }
};

#define pms_cfls_obj_size(q) CompactibleFreeListSpace::adjustObjectSize(oop(q)->size())

// This macro defines specialized support code for parallel mark
// sweep. It's defined as a macro to de-virtualize (specialize) the
// object size routine (obj_size) for ContiguousSpace (space.cpp) and
// CompactibleFreeListSpace (compactibleFreeListSpace.cpp),
// respectively, like SCAN_AND_xxx in space.hpp.
//
// This macro contains three classes and two functions related to 'class_name'.
//
// The three classes are:
//    PMSPerRegionForwardClosure_##class_name        (used in the forward phase)
//    PMSForwardTask_##class_name                    (used in the forward phase)
//    PMSCompactClosure_##class_name                 (used in the compact phase)
//
// The two functions are:
//    class_name::pms_prepare_for_compaction_work()  (used in the forward phase)
//    class_name::pms_compact_work()                 (used in the compact phase)
//
// where 'class_name' is either ContiguousSpace or CompactibleFreeListSpace.
//
#define DECLARE_PMS_SPECIALIZED_CODE(class_name, obj_size)                      \
/* This is the BitMapClosure used for Phase 2 (forwarding) of PMS.     */       \
/* This is used to forward (objects in) a region (PMSRegion) at a time */       \
/* for parallelism.                                                    */       \
class PMSPerRegionForwardClosure_##class_name : public BitMapClosure {          \
 private:                                                                       \
  PMSMarkBitMap*         _mark_bit_map;                                         \
  PMSRegion*             _region;                                               \
  PMSRegion::RegionDest* _dest;                                                 \
  CompactibleSpace*      _dest_space;                                           \
  bool                   _is_dest_cfls;                                         \
  HeapWord*              _compact_top;                                          \
  HeapWord*          _beginning_of_live; /* the first live object */            \
  HeapWord*          _end_of_live;       /* right after the last live object */ \
  HeapWord*          _first_moved;       /* the first moved (live) object */    \
                                         /* where the compaction phase */       \
                                         /* starts from */                      \
  bool               _beginning_of_live_set;                                    \
  bool               _first_moved_set;                                          \
  void update_dest(HeapWord* addr) {                                            \
    _dest = _region->find_destination(addr);                                    \
    assert(_dest->_from_mr.contains(addr), "sanity check");                     \
    _dest_space = _dest->_to_space;                                             \
    _is_dest_cfls = _dest_space->isCompactibleFreeListSpace();                  \
    _compact_top = _dest_space->bottom() + _dest->_to_space_off;                \
  }                                                                             \
 public:                                                                        \
  PMSPerRegionForwardClosure_##class_name(PMSMarkBitMap* mark_bit_map,          \
                                          PMSRegion* region) :                  \
      _mark_bit_map(mark_bit_map), _region(region),                             \
      _beginning_of_live(region->end()), _end_of_live(region->start()),         \
      _first_moved(region->end()), _beginning_of_live_set(false),               \
      _first_moved_set(false) {                                                 \
    assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");                \
    update_dest(region->start());                                               \
  }                                                                             \
  HeapWord* compact_top()       { return _compact_top; }                        \
  HeapWord* beginning_of_live() { return _beginning_of_live; }                  \
  HeapWord* end_of_live()       { return _end_of_live; }                        \
  HeapWord* first_moved()       { return _first_moved; }                        \
  bool do_bit(size_t offset) {                                                  \
    HeapWord* addr = _mark_bit_map->bit_to_addr(offset);                        \
    assert(_region->start() <= addr && addr < _region->end(), "out of region"); \
    if (_dest->_from_mr.end() <= addr) {                                        \
      update_dest(addr);                                                        \
    }                                                                           \
    assert(_dest->_from_mr.contains(addr), "out of bounds");                    \
    if (!_beginning_of_live_set) {                                              \
      _beginning_of_live_set = true;                                            \
      _beginning_of_live = addr;                                                \
      /* Tighten the from_mr to the live portion only */                        \
      _dest->_from_mr = MemRegion(addr, _dest->_from_mr.end());                 \
    }                                                                           \
    oop obj = oop(addr);                                                        \
    assert(_mark_bit_map->is_marked(obj), "sanity check");                      \
    assert((intptr_t)addr % MinObjAlignmentInBytes == 0, "obj alignemnt check");\
    const intx interval = PrefetchScanIntervalInBytes;                          \
    Prefetch::write(addr, interval);                                            \
    size_t ssize = obj_size(addr);                                              \
    size_t dsize = _is_dest_cfls ? pms_cfls_obj_size(obj) : obj->size();        \
    if (addr != _compact_top) {                                                 \
      obj->forward_to(oop(_compact_top));                                       \
      assert(obj->is_gc_marked(),                                               \
             "encoding the pointer should preserve the mark");                  \
    } else {                                                                    \
      obj->init_mark();                                                         \
      assert(obj->forwardee() == NULL, "should be forwarded to NULL");          \
    }                                                                           \
    /* Update the BOT. Make sure this is MT-safe */                             \
    _dest_space->cross_threshold(_compact_top, _compact_top + dsize);           \
    _compact_top += dsize;                                                      \
    _end_of_live = addr + ssize;                                                \
    /* Tighten the from_mr only to the live portion. Especially, */             \
    /* for the last object whose end sticks out of the end of the region */     \
    /* we need to update the end of from_mr. */                                 \
    if (_region->end() < _end_of_live) {                                        \
      _dest->_from_mr = MemRegion(_dest->_from_mr.start(), _end_of_live);       \
    }                                                                           \
    assert(_end_of_live <= _region->space()->end(), "Out of space bound");      \
    if (!_first_moved_set && obj->forwardee() != NULL) {                        \
      /* Non-moving object's forwarding pointer is set to NULL */               \
      _first_moved_set = true;                                                  \
      _first_moved = addr;                                                      \
    }                                                                           \
    return true;                                                                \
  }                                                                             \
  void record() {                                                               \
    _region->record_stats(_beginning_of_live, _end_of_live, _first_moved);      \
  }                                                                             \
};                                                                              \
/* This is an AbstractGangTask used for Phase 2 (forwarding) of PMS.   */       \
/* This is used to forward (objects in) a region (PMSRegion) at a time */       \
/* for parallelism.                                                    */       \
class PMSForwardTask_##class_name : public AbstractGangTask {                   \
 private:                                                                       \
  typedef OverflowTaskQueue<PMSRegion*, mtGC>           PMSRegionTaskQueue;     \
  typedef GenericTaskQueueSet<PMSRegionTaskQueue, mtGC> PMSRegionTaskQueueSet;  \
  WorkGang*              _workers;                                              \
  int                    _n_workers;                                            \
  PMSRegionTaskQueueSet* _task_queues;                                          \
  ParallelTaskTerminator _terminator;                                           \
  PMSMarkBitMap*         _mark_bit_map;                                         \
 public:                                                                        \
  PMSForwardTask_##class_name(int n_workers, WorkGang* workers,                 \
                              PMSRegionTaskQueueSet* task_queues) :             \
      AbstractGangTask("genMarkSweep parallel forward"),                        \
      _n_workers(n_workers), _workers(workers),                                 \
      _task_queues(task_queues),                                                \
      _terminator(ParallelTaskTerminator(n_workers, task_queues)),              \
      _mark_bit_map(MarkSweep::pms_mark_bit_map()) {                            \
    assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");                \
  }                                                                             \
  void work(uint worker_id) {                                                   \
    elapsedTimer timer;                                                         \
    ResourceMark rm;                                                            \
    HandleMark   hm;                                                            \
    timer.start();                                                              \
    if (LogCMSParallelFullGC) {                                                 \
      gclog_or_tty->print_cr("Parallel forward worker %d started...",           \
                             worker_id);                                        \
    }                                                                           \
    PMSRegionTaskQueue* task_queue = _task_queues->queue(worker_id);            \
    GenCollectedHeap* gch = GenCollectedHeap::heap();                           \
    elapsedTimer timer1;                                                        \
    timer1.start();                                                             \
    PMSRegion* r;                                                               \
    do {                                                                        \
      while (task_queue->pop_overflow(r)) {                                     \
        PMSPerRegionForwardClosure_##class_name cl(_mark_bit_map, r);           \
        _mark_bit_map->iterate(&cl,                                             \
                               _mark_bit_map->addr_to_bit(r->start()),          \
                               _mark_bit_map->addr_to_bit(r->end()));           \
        cl.record();                                                            \
      }                                                                         \
      while (task_queue->pop_local(r)) {                                        \
        PMSPerRegionForwardClosure_##class_name cl(_mark_bit_map, r);           \
        _mark_bit_map->iterate(&cl,                                             \
                               _mark_bit_map->addr_to_bit(r->start()),          \
                               _mark_bit_map->addr_to_bit(r->end()));           \
        cl.record();                                                            \
      }                                                                         \
    } while (!task_queue->is_empty());                                          \
    timer1.stop();                                                              \
    if (LogCMSParallelFullGC) {                                                 \
      gclog_or_tty->print_cr("Parallel forward worker %d finished scanning "    \
                             "(%3.5f sec)",                                     \
                             worker_id, timer1.seconds());                      \
    }                                                                           \
    do_steal_work(worker_id);                                                   \
    timer.stop();                                                               \
    if (LogCMSParallelFullGC) {                                                 \
      gclog_or_tty->print_cr("Parallel forward worker %d finished (%3.5f sec)", \
                             worker_id, timer.seconds());                       \
    }                                                                           \
  }                                                                             \
  void do_steal_work(uint worker_id) {                                          \
    elapsedTimer timer;                                                         \
    timer.start();                                                              \
    int seed = 17;                                                              \
    int num_steals = 0;                                                         \
    PMSRegionTaskQueue* task_queue = _task_queues->queue(worker_id);            \
    assert(task_queue == _task_queues->queue(worker_id), "Sanity check");       \
    PMSRegion* r;                                                               \
    do {                                                                        \
      assert(task_queue->is_empty(),                                            \
             "Task queue should be empty before work stealing");                \
      while (_task_queues->steal(worker_id, r)) {                               \
        num_steals++;                                                           \
        PMSPerRegionForwardClosure_##class_name cl(_mark_bit_map, r);           \
        _mark_bit_map->iterate(&cl,                                             \
                               _mark_bit_map->addr_to_bit(r->start()),          \
                               _mark_bit_map->addr_to_bit(r->end()));           \
        cl.record();                                                            \
      }                                                                         \
    } while (!_terminator.offer_termination());                                 \
    assert(task_queue->is_empty(), "Check my task queue is empty again");       \
    timer.stop();                                                               \
    if (LogCMSParallelFullGC) {                                                 \
      gclog_or_tty->print_cr(                                                   \
        "Parallel forward worker %d finished work-stealing "                    \
        "(%d steals, %3.5f sec)",                                               \
        worker_id, num_steals, timer.seconds());                                \
    }                                                                           \
  }                                                                             \
};                                                                              \
/* This is the BitMapClosure used for Phase 4 (compaction/evacuation) of PMS.*/ \
/* This is used to copy objects.                                             */ \
class PMSCompactClosure_##class_name : public BitMapClosure {                   \
 private:                                                                       \
  PMSMarkBitMap* _mark_bit_map;                                                 \
 public:                                                                        \
  PMSCompactClosure_##class_name(PMSMarkBitMap* mark_bit_map) :                 \
      _mark_bit_map(mark_bit_map) {                                             \
    assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");                \
  }                                                                             \
  bool do_bit(size_t offset) {                                                  \
    HeapWord* addr = _mark_bit_map->bit_to_addr(offset);                        \
    oop obj = oop(addr);                                                        \
    assert(_mark_bit_map->is_marked(obj), "sanity check");                      \
    const intx scan_interval = PrefetchScanIntervalInBytes;                     \
    const intx copy_interval = PrefetchCopyIntervalInBytes;                     \
    Prefetch::read(addr, scan_interval);                                        \
    size_t size = obj_size(addr);                                               \
    HeapWord* compaction_top = (HeapWord*)obj->forwardee();                     \
    assert(compaction_top != NULL, "Non-moving object should not be seen here");\
    Prefetch::write(compaction_top, copy_interval);                             \
    assert(addr != compaction_top, "everything in this pass should be moving"); \
    Copy::aligned_conjoint_words(addr, compaction_top, size);                   \
    oop(compaction_top)->init_mark();                                           \
    assert(oop(compaction_top)->klass() != NULL, "should have a class");        \
    return true;                                                                \
  }                                                                             \
};                                                                              \
/* Parallel version of prepare_for_compaction() used for Phase 2 (forwarding)*/ \
/* of PMS.                                                                   */ \
void class_name::pms_prepare_for_compaction_work(CompactPoint* cp) {            \
  assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");                  \
  HeapWord* compact_top;                                                        \
  set_compaction_top(bottom());                                                 \
  if (cp->space == NULL) {                                                      \
    assert(cp->gen != NULL, "need a generation");                               \
    assert(cp->threshold == NULL, "just checking");                             \
    assert(cp->gen->first_compaction_space() == this, "just checking");         \
    cp->space = cp->gen->first_compaction_space();                              \
    compact_top = cp->space->bottom();                                          \
    cp->space->set_compaction_top(compact_top);                                 \
    cp->threshold = cp->space->initialize_threshold();                          \
  } else {                                                                      \
    compact_top = cp->space->compaction_top();                                  \
  }                                                                             \
  PMSMarkBitMap* mark_bit_map = MarkSweep::pms_mark_bit_map();                  \
  GenCollectedHeap* gch = GenCollectedHeap::heap();                             \
  PMSRegionTaskQueueSet* task_queues = MarkSweep::pms_region_task_queues();     \
  WorkGang* workers = gch->workers();                                           \
  int n_workers = workers->total_workers();                                     \
  PMSRegionArraySet* region_array_set = MarkSweep::pms_region_array_set();      \
  PMSRegionArray* regions = region_array_set->region_array_for(this);           \
  assert(regions != NULL, "Must be non-null");                                  \
  PMSRegion* first_r = regions->begin();                                        \
  PMSRegion* last_r = regions->end(); /* Inclusive */                           \
  int i = 0;                                                                    \
  for (PMSRegion* r = first_r; r <= last_r; r++) {                              \
    size_t live_size = r->live_size();                                          \
    size_t cfls_live_size = r->cfls_live_size();                                \
    if (live_size == 0) {                                                       \
      r->_evac_state = PMSRegion::HAS_BEEN_EVAC;                                \
      assert(cfls_live_size == 0, "cfls_live_size must be zero, too");          \
      continue;                                                                 \
    }                                                                           \
    r->_evac_state = PMSRegion::NOT_EVAC;                                       \
    bool is_cfls = cp->space->isCompactibleFreeListSpace();                     \
    size_t size = is_cfls ? cfls_live_size : live_size;                         \
    size_t compaction_max_size = pointer_delta(cp->space->end(), compact_top);  \
    HeapWord* split_point = r->start();                                         \
    /* If the current compaction space is not large enough or the */            \
    /* remaining fragment is smaller than MinChunkSize (which is not */         \
    /* allowed for the CMS space), we need to use the next */                   \
    /* compaction space and split the region. */                                \
    while (size > compaction_max_size ||                                        \
           (size + cp->space->minimum_free_block_size() > compaction_max_size &&\
            size != compaction_max_size)) {                                     \
      /* First split the region */                                              \
      HeapWord* prev_compact_top = compact_top;                                 \
      HeapWord* prev_split_point = split_point;                                 \
      PMSFindRegionSplitPointClosure cl(mark_bit_map, compact_top, cp->space);  \
      mark_bit_map->iterate(&cl,                                                \
                            mark_bit_map->addr_to_bit(split_point),             \
                            mark_bit_map->addr_to_bit(r->end()));               \
      split_point = cl.split_point();                                           \
      assert(split_point != NULL, "Region must be split");                      \
      compact_top = cl.compact_top();                                           \
      size_t live_size_up_to_split_point = cl.live_size_up_to_split_point();    \
      size_t cfls_live_size_up_to_split_point =                                 \
        cl.cfls_live_size_up_to_split_point();                                  \
      if (prev_split_point != split_point) {                                    \
        /* This is for the first split piece of the region */                   \
        r->add_destination(MemRegion(prev_split_point, split_point),            \
                           cp->space, prev_compact_top - cp->space->bottom(),   \
                           is_cfls ? cfls_live_size_up_to_split_point :         \
                                     live_size_up_to_split_point);              \
      } else {                                                                  \
        assert(compact_top == prev_compact_top,                                 \
               "the space didn't accommodate even one object");                 \
      }                                                                         \
      /* switch to next compaction space */                                     \
      cp->space->set_compaction_top(compact_top);                               \
      cp->space = cp->space->next_compaction_space();                           \
      if (cp->space == NULL) {                                                  \
        cp->gen = GenCollectedHeap::heap()->prev_gen(cp->gen);                  \
        assert(cp->gen != NULL, "compaction must succeed");                     \
        cp->space = cp->gen->first_compaction_space();                          \
        assert(cp->space != NULL,                                               \
               "generation must have a first compaction space");                \
      }                                                                         \
      is_cfls = cp->space->isCompactibleFreeListSpace();                        \
      compact_top = cp->space->bottom();                                        \
      cp->space->set_compaction_top(compact_top);                               \
      cp->threshold = cp->space->initialize_threshold();                        \
      compaction_max_size = pointer_delta(cp->space->end(), compact_top);       \
      live_size -= live_size_up_to_split_point;                                 \
      cfls_live_size -= cfls_live_size_up_to_split_point;                       \
      size = is_cfls ? cfls_live_size : live_size;                              \
    }                                                                           \
    assert(size <= compaction_max_size,                                         \
           "The whole region (if no split) "                                    \
           "or the last split piece must fit the current compact space");       \
    /* This is for the whole region (if no split) or the last split piece   */  \
    /* of the region. Note the from_mr will be 'tightened' later in         */  \
    /* PMSPerRegionForwardClosure_### so that it will be the                */  \
    /* smallest memory range that contains only the live portion of the     */  \
    /* region. Note that the last object's tail can stick out of the region.*/  \
    r->add_destination(MemRegion(split_point, r->end()),                        \
                       cp->space, compact_top - cp->space->bottom(), size);     \
    compact_top += size;                                                        \
    assert(compact_top <= cp->space->end(), "Must fit");                        \
    assert((intptr_t)compact_top % MinObjAlignmentInBytes == 0,                 \
           "obj alignemnt check");                                              \
    task_queues->queue(i)->push(r);                                             \
    i = (i + 1) % n_workers;                                                    \
    if (LogCMSParallelFullGC) {                                                 \
      r->log_destinations(gclog_or_tty);                                        \
    }                                                                           \
  }                                                                             \
  /* save the compaction_top of the compaction space. */                        \
  cp->space->set_compaction_top(compact_top);                                   \
  PMSForwardTask_##class_name tsk(n_workers, workers, task_queues);             \
  GCTraceTime tm1("par-forward",                                                \
                  (PrintGC && Verbose) || LogCMSParallelFullGC,                 \
                  true, NULL, GCId::undefined());                               \
  gch->set_par_threads(n_workers);                                              \
  if (n_workers > 1) {                                                          \
    workers->run_task(&tsk);                                                    \
  } else {                                                                      \
    tsk.work(0);                                                                \
  }                                                                             \
  gch->set_par_threads(0);  /* 0 ==> non-parallel.*/                            \
  /* Save the global info from the regions. Note _first_dead is unused. */      \
  assert(regions->space() == this, "sanity check");                             \
  /* TODO: get rid of the parameter below */                                    \
  regions->record_space_live_range_for_compaction();                            \
  regions->compute_compact_dependencies();                                      \
  regions->mark_dense_prefix_as_evacuated();                                    \
}                                                                               \
/* Parallel version of compact() used for Phase 4 (compaction/evacuation) */    \
/* of PMS.                                                                */    \
void class_name::pms_compact_work() {                                           \
  assert(CMSParallelFullGC, "Used only if CMSParallelFullGC");                  \
  if (_first_moved < _end_of_live) { /* Unless there is no moving live object*/ \
    PMSMarkBitMap* mark_bit_map = MarkSweep::pms_mark_bit_map();                \
    PMSCompactClosure_##class_name compact_cl(mark_bit_map);                    \
    GenCollectedHeap* gch = GenCollectedHeap::heap();                           \
    WorkGang* workers = gch->workers();                                         \
    int n_workers = workers->total_workers();                                   \
    PMSParEvacTask tsk(n_workers, workers, this, &compact_cl);                  \
    GCTraceTime tm1("par-compact",                                              \
                  (PrintGC && Verbose) || LogCMSParallelFullGC,                 \
                  true, NULL, GCId::undefined());                               \
    gch->set_par_threads(n_workers);                                            \
    if (n_workers > 1) {                                                        \
      workers->run_task(&tsk);                                                  \
    } else {                                                                    \
      tsk.work(0);                                                              \
    }                                                                           \
    gch->set_par_threads(0);  /* 0 ==> non-parallel. */                         \
  }                                                                             \
  /* Let's remember if we were empty before we did the compaction. */           \
  bool was_empty = used_region().is_empty();                                    \
  /* Reset space after compaction is complete */                                \
  reset_after_compaction();                                                     \
  /* We do this clear, below, since it has overloaded meanings for some */      \
  /* space subtypes.  For example, OffsetTableContigSpace's that were   */      \
  /* compacted into will have had their offset table thresholds updated */      \
  /* continuously, but those that weren't need to have their thresholds */      \
  /* re-initialized.  Also mangles unused area for debugging.           */      \
  if (used_region().is_empty()) {                                               \
    if (!was_empty) clear(SpaceDecorator::Mangle);                              \
  } else {                                                                      \
    if (ZapUnusedHeapArea) mangle_unused_area();                                \
  }                                                                             \
}

class PMSRefProcTaskExecutor : public AbstractRefProcTaskExecutor {
 private:
  ObjTaskQueueSet*      _task_queues;
  ObjArrayTaskQueueSet* _objarray_task_queues;
 public:
  PMSRefProcTaskExecutor(ObjTaskQueueSet* task_queues,
                         ObjArrayTaskQueueSet* objarray_task_queues)
      : _task_queues(task_queues), _objarray_task_queues(objarray_task_queues) {}

  // Executes a task using worker threads.
  virtual void execute(ProcessTask& task);
  virtual void execute(EnqueueTask& task);
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_MARKSWEEP_HPP
