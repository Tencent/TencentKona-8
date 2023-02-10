/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "ci/ciMethod.hpp"
#include "interpreter/interpreter.hpp"
#include "runtime/frame.inline.hpp"

#ifndef CC_INTERP

# define __ _masm->

// asm based interpreter deoptimization helpers
int AbstractInterpreter::size_activation(int max_stack,
                                         int temps,
                                         int extra_args,
                                         int monitors,
                                         int callee_params,
                                         int callee_locals,
                                         bool is_top_frame) {
  // Note: This calculation must exactly parallel the frame setup
  // in AbstractInterpreterGenerator::generate_method_entry.

  // fixed size of an interpreter frame:
  int overhead = frame::sender_sp_offset -
                 frame::interpreter_frame_initial_sp_offset;
  // Our locals were accounted for by the caller (or last_frame_adjust
  // on the transistion) Since the callee parameters already account
  // for the callee's params we only need to account for the extra
  // locals.
  int size = overhead +
         (callee_locals - callee_params)*Interpreter::stackElementWords +
         monitors * frame::interpreter_frame_monitor_size() +
         temps* Interpreter::stackElementWords + extra_args;

  return size;
}

void AbstractInterpreter::layout_activation(Method* method,
                                            int tempcount,
                                            int popframe_extra_args,
                                            int moncount,
                                            int caller_actual_parameters,
                                            int callee_param_count,
                                            int callee_locals,
                                            frame* caller,
                                            frame* interpreter_frame,
                                            bool is_top_frame,
                                            bool is_bottom_frame) {
  // The frame interpreter_frame is guaranteed to be the right size,
  // as determined by a previous call to the size_activation() method.
  // It is also guaranteed to be walkable even though it is in a
  // skeletal state

  int max_locals = method->max_locals() * Interpreter::stackElementWords;
  int extra_locals = (method->max_locals() - method->size_of_parameters()) *
    Interpreter::stackElementWords;

#ifdef ASSERT
  if (!EnableInvokeDynamic) {
    // @@@ FIXME: Should we correct interpreter_frame_sender_sp in the calling sequences?
    // Probably, since deoptimization doesn't work yet.
    assert(caller->unextended_sp() == interpreter_frame->interpreter_frame_sender_sp(), "Frame not properly walkable");
  }
  assert(caller->sp() == interpreter_frame->sender_sp(), "Frame not properly walkable(2)");
#endif

  interpreter_frame->interpreter_frame_set_method(method);
  // NOTE the difference in using sender_sp and
  // interpreter_frame_sender_sp interpreter_frame_sender_sp is
  // the original sp of the caller (the unextended_sp) and
  // sender_sp is fp+8/16 (32bit/64bit) XXX
  intptr_t* locals = interpreter_frame->sender_sp() + max_locals - 1;

#ifdef ASSERT
  if (caller->is_interpreted_frame()) {
    assert(locals < caller->fp() + frame::interpreter_frame_initial_sp_offset, "bad placement");
  }
#endif

  interpreter_frame->interpreter_frame_set_locals(locals);
  BasicObjectLock* montop = interpreter_frame->interpreter_frame_monitor_begin();
  BasicObjectLock* monbot = montop - moncount;
  interpreter_frame->interpreter_frame_set_monitor_end(monbot);

  // Set last_sp
  intptr_t*  esp = (intptr_t*) monbot -
    tempcount*Interpreter::stackElementWords -
    popframe_extra_args;
  interpreter_frame->interpreter_frame_set_last_sp(esp);

  // All frames but the initial (oldest) interpreter frame we fill in have
  // a value for sender_sp that allows walking the stack but isn't
  // truly correct. Correct the value here.
  if (extra_locals != 0 &&
      interpreter_frame->sender_sp() ==
      interpreter_frame->interpreter_frame_sender_sp()) {
    interpreter_frame->set_interpreter_frame_sender_sp(caller->sp() +
                                                       extra_locals);
  }
  *interpreter_frame->interpreter_frame_cache_addr() =
    method->constants()->cache();
}

void AbstractInterpreterGenerator::bang_stack_shadow_pages(bool native_call) {
  // Quick & dirty stack overflow checking: bang the stack & handle trap.
  // Note that we do the banging after the frame is setup, since the exception
  // handling code expects to find a valid interpreter frame on the stack.
  // Doing the banging earlier fails if the caller frame is not an interpreter
  // frame.
  // (Also, the exception throwing code expects to unlock any synchronized
  // method receiever, so do the banging after locking the receiver.)

  // Bang each page in the shadow zone. We can't assume it's been done for
  // an interpreter frame with greater than a page of locals, so each page
  // needs to be checked.  Only true for non-native.
  if (!UseStackBanging) {
    return;
  }
  const int shadow_zone_size = (int)(JavaThread::stack_shadow_zone_size());
  const int page_size = os::vm_page_size();
  const int n_shadow_pages = shadow_zone_size / page_size;
  const int start_page = native_call ? n_shadow_pages : 1;
  const ByteSize watermark_offset = native_call ? JavaThread::shadow_zone_growth_native_watermark_offset() :
                                                  JavaThread::shadow_zone_growth_watermark_offset();

  const Register thread = NOT_LP64(rsi) LP64_ONLY(r15_thread);
 #ifndef _LP64
  __ push(thread);
  __ get_thread(thread);
#endif

#ifdef ASSERT
  Label L_good_limit;
  __ cmpptr(Address(thread, JavaThread::shadow_zone_safe_limit_offset()), (int32_t)NULL_WORD);
  __ jcc(Assembler::notEqual, L_good_limit);
  __ stop("shadow zone safe limit is not initialized");
  __ bind(L_good_limit);

  Label L_good_watermark;
  __ cmpptr(Address(thread, watermark_offset), (int32_t)NULL_WORD);
  __ jcc(Assembler::notEqual, L_good_watermark);
  __ stop("shadow zone growth watermark is not initialized");
  __ bind(L_good_watermark);
#endif

  Label L_done;

  __ cmpptr(rsp, Address(thread, watermark_offset));
  __ jcc(Assembler::above, L_done);

  for (int p = start_page; p <= n_shadow_pages; p++) {
    __ bang_stack_with_offset(p*page_size);
  }

  // Record a new watermark, unless the update is above the safe limit.
  // Otherwise, the next time around a check above would pass the safe limit.
  __ cmpptr(rsp, Address(thread, JavaThread::shadow_zone_safe_limit_offset()));
  __ jccb(Assembler::belowEqual, L_done);
#ifdef _LP64
  __ movptr(rscratch1, rsp);
  __ andptr(rscratch1, ~(page_size - 1));
  __ movptr(Address(thread, watermark_offset), rscratch1);
#else
  __ movptr(Address(thread, watermark_offset), rsp);
#endif

  __ bind(L_done);

#ifndef _LP64
  __ pop(thread);
#endif
}

#endif // CC_INTERP
