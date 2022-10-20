/*
 * Copyright (c) 1999, 2014, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2022, Loongson Technology. All rights reserved.
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

// no precompiled headers
#include "asm/macroAssembler.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "code/icBuffer.hpp"
#include "code/vtableStubs.hpp"
#include "interpreter/interpreter.hpp"
#include "jvm_linux.h"
#include "memory/allocation.inline.hpp"
#include "mutex_linux.inline.hpp"
#include "os_share_linux.hpp"
#include "prims/jniFastGetField.hpp"
#include "prims/jvm.h"
#include "prims/jvm_misc.hpp"
#include "runtime/arguments.hpp"
#include "runtime/extendedPC.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/osThread.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/timer.hpp"
#include "utilities/events.hpp"
#include "utilities/vmError.hpp"
#include "utilities/debug.hpp"
#include "compiler/disassembler.hpp"
// put OS-includes here
# include <sys/types.h>
# include <sys/mman.h>
# include <pthread.h>
# include <signal.h>
# include <errno.h>
# include <dlfcn.h>
# include <stdlib.h>
# include <stdio.h>
# include <unistd.h>
# include <sys/resource.h>
# include <pthread.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <sys/utsname.h>
# include <sys/socket.h>
# include <sys/wait.h>
# include <pwd.h>
# include <poll.h>
# include <ucontext.h>
# include <fpu_control.h>

#define REG_SP 29
#define REG_FP 30

address os::current_stack_pointer() {
  register void *sp __asm__ ("$29");
  return (address) sp;
}

char* os::non_memory_address_word() {
  // Must never look like an address returned by reserve_memory,
  // even in its subfields (as defined by the CPU immediate fields,
  // if the CPU splits constants across multiple instructions).

  return (char*) -1;
}

void os::initialize_thread(Thread* thr) {
// Nothing to do.
}

address os::Linux::ucontext_get_pc(ucontext_t * uc) {
  //return (address)uc->uc_mcontext.gregs[REG_PC];
  return (address)uc->uc_mcontext.pc;
}

intptr_t* os::Linux::ucontext_get_sp(ucontext_t * uc) {
  return (intptr_t*)uc->uc_mcontext.gregs[REG_SP];
}

intptr_t* os::Linux::ucontext_get_fp(ucontext_t * uc) {
  return (intptr_t*)uc->uc_mcontext.gregs[REG_FP];
}

// For Forte Analyzer AsyncGetCallTrace profiling support - thread
// is currently interrupted by SIGPROF.
// os::Solaris::fetch_frame_from_ucontext() tries to skip nested signal
// frames. Currently we don't do that on Linux, so it's the same as
// os::fetch_frame_from_context().
ExtendedPC os::Linux::fetch_frame_from_ucontext(Thread* thread,
  ucontext_t* uc, intptr_t** ret_sp, intptr_t** ret_fp) {

  assert(thread != NULL, "just checking");
  assert(ret_sp != NULL, "just checking");
  assert(ret_fp != NULL, "just checking");

  return os::fetch_frame_from_context(uc, ret_sp, ret_fp);
}

ExtendedPC os::fetch_frame_from_context(void* ucVoid,
                    intptr_t** ret_sp, intptr_t** ret_fp) {

  ExtendedPC  epc;
  ucontext_t* uc = (ucontext_t*)ucVoid;

  if (uc != NULL) {
    epc = ExtendedPC(os::Linux::ucontext_get_pc(uc));
    if (ret_sp) *ret_sp = os::Linux::ucontext_get_sp(uc);
    if (ret_fp) *ret_fp = os::Linux::ucontext_get_fp(uc);
  } else {
    // construct empty ExtendedPC for return value checking
    epc = ExtendedPC(NULL);
    if (ret_sp) *ret_sp = (intptr_t *)NULL;
    if (ret_fp) *ret_fp = (intptr_t *)NULL;
  }

  return epc;
}

frame os::fetch_frame_from_context(void* ucVoid) {
  intptr_t* sp;
  intptr_t* fp;
  ExtendedPC epc = fetch_frame_from_context(ucVoid, &sp, &fp);
  return frame(sp, fp, epc.pc());
}

// By default, gcc always save frame pointer (%ebp/%rbp) on stack. It may get
// turned off by -fomit-frame-pointer,
frame os::get_sender_for_C_frame(frame* fr) {
  return frame(fr->sender_sp(), fr->link(), fr->sender_pc());
}

//intptr_t* _get_previous_fp() {
intptr_t* __attribute__((noinline)) os::get_previous_fp() {
  int *pc;
  intptr_t sp;
  int *pc_limit = (int*)(void*)&os::get_previous_fp;
  int insn;

  {
    l_pc:;
    pc = (int*)&&l_pc;
    __asm__ __volatile__ ("move %0,  $sp" : "=r" (sp));
  }

  do {
    insn = *pc;
    switch(bitfield(insn, 16, 16)) {
      case 0x27bd:  /* addiu $sp,$sp,-i */
      case 0x67bd:  /* daddiu $sp,$sp,-i */
        assert ((short)bitfield(insn, 0, 16)<0, "bad frame");
        sp -= (short)bitfield(insn, 0, 16);
        return (intptr_t*)sp;
    }
    --pc;
  } while (pc>=pc_limit); // The initial value of pc may be equal to pc_limit, because of GCC optimization.

  ShouldNotReachHere();
  return NULL; // mute compiler
}


frame os::current_frame() {
  intptr_t* fp = (intptr_t*)get_previous_fp();
  frame myframe((intptr_t*)os::current_stack_pointer(),
                (intptr_t*)fp,
                CAST_FROM_FN_PTR(address, os::current_frame));
  if (os::is_first_C_frame(&myframe)) {
    // stack is not walkable
    return frame();
  } else {
    return os::get_sender_for_C_frame(&myframe);
  }
}

//x86 add 2 new assemble function here!
extern "C" JNIEXPORT int
JVM_handle_linux_signal(int sig,
                        siginfo_t* info,
                        void* ucVoid,
                        int abort_if_unrecognized) {
#ifdef PRINT_SIGNAL_HANDLE
  tty->print_cr("Signal: signo=%d, sicode=%d, sierrno=%d, siaddr=%lx",
      info->si_signo,
      info->si_code,
      info->si_errno,
      info->si_addr);
#endif

  ucontext_t* uc = (ucontext_t*) ucVoid;

  Thread* t = ThreadLocalStorage::get_thread_slow();

  SignalHandlerMark shm(t);

  // Note: it's not uncommon that JNI code uses signal/sigset to install
  // then restore certain signal handler (e.g. to temporarily block SIGPIPE,
  // or have a SIGILL handler when detecting CPU type). When that happens,
  // JVM_handle_linux_signal() might be invoked with junk info/ucVoid. To
  // avoid unnecessary crash when libjsig is not preloaded, try handle signals
  // that do not require siginfo/ucontext first.

  if (sig == SIGPIPE/* || sig == SIGXFSZ*/) {
    // allow chained handler to go first
    if (os::Linux::chained_handler(sig, info, ucVoid)) {
      return true;
    } else {
      if (PrintMiscellaneous && (WizardMode || Verbose)) {
        warning("Ignoring SIGPIPE - see bug 4229104");
      }
      return true;
    }
  }

  JavaThread* thread = NULL;
  VMThread* vmthread = NULL;
  if (os::Linux::signal_handlers_are_installed) {
    if (t != NULL ){
      if(t->is_Java_thread()) {
#ifdef PRINT_SIGNAL_HANDLE
        tty->print_cr("this thread is a java thread");
#endif
        thread = (JavaThread*)t;
      }
      else if(t->is_VM_thread()){
#ifdef PRINT_SIGNAL_HANDLE
        tty->print_cr("this thread is a VM thread\n");
#endif
        vmthread = (VMThread *)t;
      }
    }
  }

  // decide if this trap can be handled by a stub
  address stub = NULL;
  address pc   = NULL;

  pc = (address) os::Linux::ucontext_get_pc(uc);
#ifdef PRINT_SIGNAL_HANDLE
  tty->print_cr("pc=%lx", pc);
  os::print_context(tty, uc);
#endif
  //%note os_trap_1
  if (info != NULL && uc != NULL && thread != NULL) {
    pc = (address) os::Linux::ucontext_get_pc(uc);
    // Handle ALL stack overflow variations here
    if (sig == SIGSEGV) {
      address addr = (address) info->si_addr;
#ifdef PRINT_SIGNAL_HANDLE
      tty->print("handle all stack overflow variations: ");
      /*tty->print("addr = %lx, stack base = %lx, stack top = %lx\n",
        addr,
        thread->stack_base(),
        thread->stack_base() - thread->stack_size());
        */
#endif

      // check if fault address is within thread stack
      if (addr < thread->stack_base() &&
          addr >= thread->stack_base() - thread->stack_size()) {
        // stack overflow
#ifdef PRINT_SIGNAL_HANDLE
        tty->print("stack exception check \n");
#endif
        if (thread->in_stack_yellow_zone(addr)) {
#ifdef PRINT_SIGNAL_HANDLE
          tty->print("exception addr is in yellow zone\n");
#endif
          thread->disable_stack_yellow_zone();
          if (thread->thread_state() == _thread_in_Java) {
            // Throw a stack overflow exception.  Guard pages will be reenabled
            // while unwinding the stack.
#ifdef PRINT_SIGNAL_HANDLE
            tty->print("this thread is in java\n");
#endif
            stub = SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::STACK_OVERFLOW);
          } else {
            // Thread was in the vm or native code.  Return and try to finish.
#ifdef PRINT_SIGNAL_HANDLE
            tty->print("this thread is in vm or native codes and return\n");
#endif
            return 1;
          }
        } else if (thread->in_stack_red_zone(addr)) {
          // Fatal red zone violation.  Disable the guard pages and fall through
          // to handle_unexpected_exception way down below.
#ifdef PRINT_SIGNAL_HANDLE
          tty->print("exception addr is in red zone\n");
#endif
          thread->disable_stack_red_zone();
          tty->print_raw_cr("An irrecoverable stack overflow has occurred.");

          // This is a likely cause, but hard to verify. Let's just print
          // it as a hint.
          tty->print_raw_cr("Please check if any of your loaded .so files has "
                            "enabled executable stack (see man page execstack(8))");
        } else {
          // Accessing stack address below sp may cause SEGV if current
          // thread has MAP_GROWSDOWN stack. This should only happen when
          // current thread was created by user code with MAP_GROWSDOWN flag
          // and then attached to VM. See notes in os_linux.cpp.
#ifdef PRINT_SIGNAL_HANDLE
          tty->print("exception addr is neither in yellow zone nor in the red one\n");
#endif
          if (thread->osthread()->expanding_stack() == 0) {
             thread->osthread()->set_expanding_stack();
             if (os::Linux::manually_expand_stack(thread, addr)) {
               thread->osthread()->clear_expanding_stack();
               return 1;
             }
             thread->osthread()->clear_expanding_stack();
          } else {
             fatal("recursive segv. expanding stack.");
          }
        }
      } //addr <
    } //sig == SIGSEGV

    if (thread->thread_state() == _thread_in_Java) {
      // Java thread running in Java code => find exception handler if any
      // a fault inside compiled code, the interpreter, or a stub
#ifdef PRINT_SIGNAL_HANDLE
      tty->print("java thread running in java code\n");
#endif

      // Handle signal from NativeJump::patch_verified_entry().
      if (sig == SIGILL & nativeInstruction_at(pc)->is_sigill_zombie_not_entrant()) {
#ifdef PRINT_SIGNAL_HANDLE
        tty->print_cr("verified entry = %lx, sig=%d", nativeInstruction_at(pc), sig);
#endif
        stub = SharedRuntime::get_handle_wrong_method_stub();
      } else if (sig == SIGSEGV && os::is_poll_address((address)info->si_addr)) {
#ifdef PRINT_SIGNAL_HANDLE
        tty->print_cr("polling address = %lx, sig=%d", os::get_polling_page(), sig);
#endif
        stub = SharedRuntime::get_poll_stub(pc);
      } else if (sig == SIGBUS /* && info->si_code == BUS_OBJERR */) {
        // BugId 4454115: A read from a MappedByteBuffer can fault
        // here if the underlying file has been truncated.
        // Do not crash the VM in such a case.
        CodeBlob* cb = CodeCache::find_blob_unsafe(pc);
        nmethod* nm = cb->is_nmethod() ? (nmethod*)cb : NULL;
#ifdef PRINT_SIGNAL_HANDLE
        tty->print("cb = %lx, nm = %lx\n", cb, nm);
#endif
        if (nm != NULL && nm->has_unsafe_access()) {
          stub = StubRoutines::handler_for_unsafe_access();
        }
      } else if (sig == SIGFPE /* && info->si_code == FPE_INTDIV */) {
        // HACK: si_code does not work on linux 2.2.12-20!!!
        int op = pc[0] & 0x3f;
        int op1 = pc[3] & 0x3f;
        //FIXME, Must port to mips code!!
        switch (op) {
          case 0x1e:  //ddiv
          case 0x1f:  //ddivu
          case 0x1a:  //div
          case 0x1b:  //divu
          case 0x34:  //trap
            /* In MIPS, div_by_zero exception can only be triggered by explicit 'trap'.
             * Ref: [c1_LIRAssembler_mips.cpp] arithmetic_idiv()
             */
            stub = SharedRuntime::continuation_for_implicit_exception(thread,
                                    pc,
                                    SharedRuntime::IMPLICIT_DIVIDE_BY_ZERO);
            break;
          default:
            // TODO: handle more cases if we are using other x86 instructions
            //   that can generate SIGFPE signal on linux.
            tty->print_cr("unknown opcode 0x%X -0x%X with SIGFPE.", op, op1);
            //fatal("please update this code.");
        }
      } else if (sig == SIGSEGV &&
          !MacroAssembler::needs_explicit_null_check((intptr_t)info->si_addr)) {
#ifdef PRINT_SIGNAL_HANDLE
        tty->print("continuation for implicit exception\n");
#endif
        // Determination of interpreter/vtable stub/compiled code null exception
        stub = SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::IMPLICIT_NULL);
#ifdef PRINT_SIGNAL_HANDLE
        tty->print_cr("continuation_for_implicit_exception stub: %lx", stub);
#endif
      } else if (/*thread->thread_state() == _thread_in_Java && */sig == SIGILL) {
        //Since kernel does not have emulation of PS instructions yet, the emulation must be handled here.
        //The method is to trigger kernel emulation of float emulation.
        int inst = *(int*)pc;
        int ops = (inst >> 26) & 0x3f;
        int ops_fmt = (inst >> 21) & 0x1f;
        int op = inst & 0x3f;
        if (ops == Assembler::cop1_op && ops_fmt == Assembler::ps_fmt) {
          int ft, fs, fd;
          ft = (inst >> 16) & 0x1f;
          fs = (inst >> 11) & 0x1f;
          fd = (inst >> 6) & 0x1f;
          float ft_upper, ft_lower, fs_upper, fs_lower, fd_upper, fd_lower;
          double ft_value, fs_value, fd_value;
          ft_value = uc->uc_mcontext.fpregs.fp_r.fp_dregs[ft];
          fs_value = uc->uc_mcontext.fpregs.fp_r.fp_dregs[fs];
          __asm__ __volatile__ (
            "cvt.s.pl %0, %4\n\t"
            "cvt.s.pu %1, %4\n\t"
            "cvt.s.pl %2, %5\n\t"
            "cvt.s.pu %3, %5\n\t"
            : "=f" (fs_lower), "=f" (fs_upper), "=f" (ft_lower), "=f" (ft_upper)
            : "f" (fs_value), "f" (ft_value)
          );

          switch (op) {
            case Assembler::fadd_op:
              __asm__ __volatile__ (
                "add.s  %1, %3, %5\n\t"
                "add.s  %2, %4, %6\n\t"
                "pll.ps %0, %1, %2\n\t"
                : "=f" (fd_value), "=f" (fd_upper), "=f" (fd_lower)
                : "f" (fs_upper), "f" (fs_lower), "f" (ft_upper), "f" (ft_lower)
              );
              uc->uc_mcontext.fpregs.fp_r.fp_dregs[fd] = fd_value;
              stub = pc + 4;
              break;
            case Assembler::fsub_op:
              //fd = fs - ft
              __asm__ __volatile__ (
                "sub.s  %1, %3, %5\n\t"
                "sub.s  %2, %4, %6\n\t"
                "pll.ps %0, %1, %2\n\t"
                : "=f" (fd_value), "=f" (fd_upper), "=f" (fd_lower)
                : "f" (fs_upper), "f" (fs_lower), "f" (ft_upper), "f" (ft_lower)
              );
              uc->uc_mcontext.fpregs.fp_r.fp_dregs[fd] = fd_value;
              stub = pc + 4;
              break;
            case Assembler::fmul_op:
              __asm__ __volatile__ (
                "mul.s  %1, %3, %5\n\t"
                "mul.s  %2, %4, %6\n\t"
                "pll.ps %0, %1, %2\n\t"
                : "=f" (fd_value), "=f" (fd_upper), "=f" (fd_lower)
                : "f" (fs_upper), "f" (fs_lower), "f" (ft_upper), "f" (ft_lower)
              );
              uc->uc_mcontext.fpregs.fp_r.fp_dregs[fd] = fd_value;
              stub = pc + 4;
              break;
            default:
              tty->print_cr("unknown cop1 opcode 0x%x with SIGILL.", op);
          }
        } else if (ops == Assembler::cop1x_op /*&& op == Assembler::nmadd_ps_op*/) {
          // madd.ps is not used, the code below were not tested
          int fr, ft, fs, fd;
          float fr_upper, fr_lower, fs_upper, fs_lower, ft_upper, ft_lower, fd_upper, fd_lower;
          double fr_value, ft_value, fs_value, fd_value;
          switch (op) {
            case Assembler::madd_ps_op:
              // fd = (fs * ft) + fr
              fr = (inst >> 21) & 0x1f;
              ft = (inst >> 16) & 0x1f;
              fs = (inst >> 11) & 0x1f;
              fd = (inst >> 6) & 0x1f;
              fr_value = uc->uc_mcontext.fpregs.fp_r.fp_dregs[fr];
              ft_value = uc->uc_mcontext.fpregs.fp_r.fp_dregs[ft];
              fs_value = uc->uc_mcontext.fpregs.fp_r.fp_dregs[fs];
              __asm__ __volatile__ (
                "cvt.s.pu %3, %9\n\t"
                "cvt.s.pl %4, %9\n\t"
                "cvt.s.pu %5, %10\n\t"
                "cvt.s.pl %6, %10\n\t"
                "cvt.s.pu %7, %11\n\t"
                "cvt.s.pl %8, %11\n\t"
                "madd.s %1, %3, %5, %7\n\t"
                "madd.s %2, %4, %6, %8\n\t"
                "pll.ps %0, %1, %2\n\t"
                : "=f" (fd_value), "=f" (fd_upper), "=f" (fd_lower), "=f" (fr_upper), "=f" (fr_lower), "=f" (fs_upper), "=f" (fs_lower), "=f" (ft_upper), "=f" (ft_lower)
                : "f" (fr_value)/*9*/, "f" (fs_value)/*10*/, "f" (ft_value)/*11*/
              );
              uc->uc_mcontext.fpregs.fp_r.fp_dregs[fd] = fd_value;
              stub = pc + 4;
              break;
            default:
              tty->print_cr("unknown cop1x opcode 0x%x with SIGILL.", op);
          }
        }
      } //SIGILL
    } else if (sig == SIGILL && VM_Version::is_determine_features_test_running()) {
      // thread->thread_state() != _thread_in_Java
      // SIGILL must be caused by VM_Version::determine_features().
      VM_Version::set_supports_cpucfg(false);
      stub = pc + 4;  // continue with next instruction.
    } else if (thread->thread_state() == _thread_in_vm &&
               sig == SIGBUS && /* info->si_code == BUS_OBJERR && */
               thread->doing_unsafe_access()) {
#ifdef PRINT_SIGNAL_HANDLE
      tty->print_cr("SIGBUS in vm thread \n");
#endif
      stub = StubRoutines::handler_for_unsafe_access();
    }

    // jni_fast_Get<Primitive>Field can trap at certain pc's if a GC kicks in
    // and the heap gets shrunk before the field access.
    if ((sig == SIGSEGV) || (sig == SIGBUS)) {
#ifdef PRINT_SIGNAL_HANDLE
      tty->print("jni fast get trap: ");
#endif
      address addr = JNI_FastGetField::find_slowcase_pc(pc);
      if (addr != (address)-1) {
        stub = addr;
      }
#ifdef PRINT_SIGNAL_HANDLE
      tty->print_cr("addr = %d, stub = %lx", addr, stub);
#endif
    }

    // Check to see if we caught the safepoint code in the
    // process of write protecting the memory serialization page.
    // It write enables the page immediately after protecting it
    // so we can just return to retry the write.
    if ((sig == SIGSEGV) &&
        os::is_memory_serialize_page(thread, (address) info->si_addr)) {
#ifdef PRINT_SIGNAL_HANDLE
      tty->print("write protecting the memory serialiazation page\n");
#endif
      // Block current thread until the memory serialize page permission restored.
      os::block_on_serialize_page_trap();
      return true;
    }
  }

  // Execution protection violation
  //
  // This should be kept as the last step in the triage.  We don't
  // have a dedicated trap number for a no-execute fault, so be
  // conservative and allow other handlers the first shot.
  //
  // Note: We don't test that info->si_code == SEGV_ACCERR here.
  // this si_code is so generic that it is almost meaningless; and
  // the si_code for this condition may change in the future.
  // Furthermore, a false-positive should be harmless.
  if (UnguardOnExecutionViolation > 0 &&
      //(sig == SIGSEGV || sig == SIGBUS) &&
      //uc->uc_mcontext.gregs[REG_TRAPNO] == trap_page_fault) {
    (sig == SIGSEGV || sig == SIGBUS
#ifdef OPT_RANGECHECK
     || sig == SIGSYS
#endif
    ) &&
      //(uc->uc_mcontext.cause == 2 || uc->uc_mcontext.cause == 3)) {
      (uc->uc_mcontext.hi1 == 2 || uc->uc_mcontext.hi1 == 3)) {
#ifdef PRINT_SIGNAL_HANDLE
    tty->print_cr("execution protection violation\n");
#endif

    int page_size = os::vm_page_size();
    address addr = (address) info->si_addr;
    address pc = os::Linux::ucontext_get_pc(uc);
    // Make sure the pc and the faulting address are sane.
    //
    // If an instruction spans a page boundary, and the page containing
    // the beginning of the instruction is executable but the following
    // page is not, the pc and the faulting address might be slightly
    // different - we still want to unguard the 2nd page in this case.
    //
    // 15 bytes seems to be a (very) safe value for max instruction size.
    bool pc_is_near_addr =
      (pointer_delta((void*) addr, (void*) pc, sizeof(char)) < 15);
    bool instr_spans_page_boundary =
      (align_size_down((intptr_t) pc ^ (intptr_t) addr,
                       (intptr_t) page_size) > 0);

    if (pc == addr || (pc_is_near_addr && instr_spans_page_boundary)) {
      static volatile address last_addr =
        (address) os::non_memory_address_word();

      // In conservative mode, don't unguard unless the address is in the VM
      if (addr != last_addr &&
          (UnguardOnExecutionViolation > 1 || os::address_is_in_vm(addr))) {

        // Set memory to RWX and retry
        address page_start =
          (address) align_size_down((intptr_t) addr, (intptr_t) page_size);
        bool res = os::protect_memory((char*) page_start, page_size,
                                      os::MEM_PROT_RWX);

        if (PrintMiscellaneous && Verbose) {
          char buf[256];
          jio_snprintf(buf, sizeof(buf), "Execution protection violation "
                       "at " INTPTR_FORMAT
                       ", unguarding " INTPTR_FORMAT ": %s, errno=%d", addr,
                       page_start, (res ? "success" : "failed"), errno);
          tty->print_raw_cr(buf);
        }
        stub = pc;

        // Set last_addr so if we fault again at the same address, we don't end
        // up in an endless loop.
        //
        // There are two potential complications here.  Two threads trapping at
        // the same address at the same time could cause one of the threads to
        // think it already unguarded, and abort the VM.  Likely very rare.
        //
        // The other race involves two threads alternately trapping at
        // different addresses and failing to unguard the page, resulting in
        // an endless loop.  This condition is probably even more unlikely than
        // the first.
        //
        // Although both cases could be avoided by using locks or thread local
        // last_addr, these solutions are unnecessary complication: this
        // handler is a best-effort safety net, not a complete solution.  It is
        // disabled by default and should only be used as a workaround in case
        // we missed any no-execute-unsafe VM code.

        last_addr = addr;
      }
    }
  }

  if (stub != NULL) {
#ifdef PRINT_SIGNAL_HANDLE
    tty->print_cr("resolved stub=%lx\n",stub);
#endif
    // save all thread context in case we need to restore it
    if (thread != NULL) thread->set_saved_exception_pc(pc);

    uc->uc_mcontext.pc = (greg_t)stub;
    return true;
  }

  // signal-chaining
  if (os::Linux::chained_handler(sig, info, ucVoid)) {
#ifdef PRINT_SIGNAL_HANDLE
     tty->print_cr("signal chaining\n");
#endif
     return true;
  }

  if (!abort_if_unrecognized) {
#ifdef PRINT_SIGNAL_HANDLE
    tty->print_cr("abort becauce of unrecognized\n");
#endif
    // caller wants another chance, so give it to him
    return false;
  }

  if (pc == NULL && uc != NULL) {
    pc = os::Linux::ucontext_get_pc(uc);
  }

  // unmask current signal
  sigset_t newset;
  sigemptyset(&newset);
  sigaddset(&newset, sig);
  sigprocmask(SIG_UNBLOCK, &newset, NULL);
#ifdef PRINT_SIGNAL_HANDLE
  tty->print_cr("VMError in signal handler\n");
#endif
  VMError err(t, sig, pc, info, ucVoid);
  err.report_and_die();

  ShouldNotReachHere();
  return true; // Mute compiler
}

// FCSR:...|24| 23 |22|21|...
//      ...|FS|FCC0|FO|FN|...
void os::Linux::init_thread_fpu_state(void) {
  if (SetFSFOFN == 999)
    return;
  int fs = (SetFSFOFN / 100)? 1:0;
  int fo = ((SetFSFOFN % 100) / 10)? 1:0;
  int fn = (SetFSFOFN % 10)? 1:0;
  int mask = fs << 24 | fo << 22 | fn << 21;

  int fcsr = get_fpu_control_word();
  fcsr = fcsr | mask;
  set_fpu_control_word(fcsr);
  /*
  if (fcsr != get_fpu_control_word())
    tty->print_cr(" fail to set to %lx, get_fpu_control_word:%lx", fcsr, get_fpu_control_word());
  */
}

int os::Linux::get_fpu_control_word(void) {
  int fcsr;
  __asm__ __volatile__ (
      ".set noat;"
      "daddiu  %0, $0, 0;"
      "cfc1 %0, $31;"
      : "=r" (fcsr)
      );
  return fcsr;
}

void os::Linux::set_fpu_control_word(int fpu_control) {
  __asm__ __volatile__ (
      ".set noat;"
      "ctc1 %0, $31;"
      :
      : "r" (fpu_control)
      );
}

bool os::is_allocatable(size_t bytes) {

  if (bytes < 2 * G) {
    return true;
  }

  char* addr = reserve_memory(bytes, NULL);

  if (addr != NULL) {
    release_memory(addr, bytes);
  }

  return addr != NULL;
}

////////////////////////////////////////////////////////////////////////////////
// thread stack

size_t os::Linux::min_stack_allowed  = 96 * K;


// Test if pthread library can support variable thread stack size. LinuxThreads
// in fixed stack mode allocates 2M fixed slot for each thread. LinuxThreads
// in floating stack mode and NPTL support variable stack size.
bool os::Linux::supports_variable_stack_size() {
  if (os::Linux::is_NPTL()) {
     // NPTL, yes
     return true;

  } else {
    // Note: We can't control default stack size when creating a thread.
    // If we use non-default stack size (pthread_attr_setstacksize), both
    // floating stack and non-floating stack LinuxThreads will return the
    // same value. This makes it impossible to implement this function by
    // detecting thread stack size directly.
    //
    // An alternative approach is to check %gs. Fixed-stack LinuxThreads
    // do not use %gs, so its value is 0. Floating-stack LinuxThreads use
    // %gs (either as LDT selector or GDT selector, depending on kernel)
    // to access thread specific data.
    //
    // Note that %gs is a reserved glibc register since early 2001, so
    // applications are not allowed to change its value (Ulrich Drepper from
    // Redhat confirmed that all known offenders have been modified to use
    // either %fs or TSD). In the worst case scenario, when VM is embedded in
    // a native application that plays with %gs, we might see non-zero %gs
    // even LinuxThreads is running in fixed stack mode. As the result, we'll
    // return true and skip _thread_safety_check(), so we may not be able to
    // detect stack-heap collisions. But otherwise it's harmless.
    //
    return false;
  }
}

// return default stack size for thr_type
size_t os::Linux::default_stack_size(os::ThreadType thr_type) {
  // default stack size (compiler thread needs larger stack)
  size_t s = (thr_type == os::compiler_thread ? 2 * M : 512 * K);
  return s;
}

size_t os::Linux::default_guard_size(os::ThreadType thr_type) {
  // Creating guard page is very expensive. Java thread has HotSpot
  // guard page, only enable glibc guard page for non-Java threads.
  return (thr_type == java_thread ? 0 : page_size());
}

// Java thread:
//
//   Low memory addresses
//    +------------------------+
//    |                        |\  JavaThread created by VM does not have glibc
//    |    glibc guard page    | - guard, attached Java thread usually has
//    |                        |/  1 page glibc guard.
// P1 +------------------------+ Thread::stack_base() - Thread::stack_size()
//    |                        |\
//    |  HotSpot Guard Pages   | - red and yellow pages
//    |                        |/
//    +------------------------+ JavaThread::stack_yellow_zone_base()
//    |                        |\
//    |      Normal Stack      | -
//    |                        |/
// P2 +------------------------+ Thread::stack_base()
//
// Non-Java thread:
//
//   Low memory addresses
//    +------------------------+
//    |                        |\
//    |  glibc guard page      | - usually 1 page
//    |                        |/
// P1 +------------------------+ Thread::stack_base() - Thread::stack_size()
//    |                        |\
//    |      Normal Stack      | -
//    |                        |/
// P2 +------------------------+ Thread::stack_base()
//
// ** P1 (aka bottom) and size ( P2 = P1 - size) are the address and stack size returned from
//    pthread_attr_getstack()

static void current_stack_region(address * bottom, size_t * size) {
  if (os::is_primordial_thread()) {
     // primordial thread needs special handling because pthread_getattr_np()
     // may return bogus value.
     *bottom = os::Linux::initial_thread_stack_bottom();
     *size   = os::Linux::initial_thread_stack_size();
  } else {
     pthread_attr_t attr;

     int rslt = pthread_getattr_np(pthread_self(), &attr);

     // JVM needs to know exact stack location, abort if it fails
     if (rslt != 0) {
       if (rslt == ENOMEM) {
         vm_exit_out_of_memory(0, OOM_MMAP_ERROR, "pthread_getattr_np");
       } else {
         fatal(err_msg("pthread_getattr_np failed with errno = %d", rslt));
       }
     }

     if (pthread_attr_getstack(&attr, (void **)bottom, size) != 0) {
         fatal("Can not locate current stack attributes!");
     }

     pthread_attr_destroy(&attr);

  }
  assert(os::current_stack_pointer() >= *bottom &&
         os::current_stack_pointer() < *bottom + *size, "just checking");
}

address os::current_stack_base() {
  address bottom;
  size_t size;
  current_stack_region(&bottom, &size);
  return (bottom + size);
}

size_t os::current_stack_size() {
  // stack size includes normal stack and HotSpot guard pages
  address bottom;
  size_t size;
  current_stack_region(&bottom, &size);
  return size;
}

/////////////////////////////////////////////////////////////////////////////
// helper functions for fatal error handler
void os::print_register_info(outputStream *st, void *context) {
  if (context == NULL) return;

  ucontext_t *uc = (ucontext_t*)context;

  st->print_cr("Register to memory mapping:");
  st->cr();
  // this is horrendously verbose but the layout of the registers in the
  //   // context does not match how we defined our abstract Register set, so
  //     // we can't just iterate through the gregs area
  //
  //       // this is only for the "general purpose" registers
  st->print("R0=" ); print_location(st, uc->uc_mcontext.gregs[0]);
  st->print("AT=" ); print_location(st, uc->uc_mcontext.gregs[1]);
  st->print("V0=" ); print_location(st, uc->uc_mcontext.gregs[2]);
  st->print("V1=" ); print_location(st, uc->uc_mcontext.gregs[3]);
  st->cr();
  st->print("A0=" ); print_location(st, uc->uc_mcontext.gregs[4]);
  st->print("A1=" ); print_location(st, uc->uc_mcontext.gregs[5]);
  st->print("A2=" ); print_location(st, uc->uc_mcontext.gregs[6]);
  st->print("A3=" ); print_location(st, uc->uc_mcontext.gregs[7]);
  st->cr();
  st->print("A4=" ); print_location(st, uc->uc_mcontext.gregs[8]);
  st->print("A5=" ); print_location(st, uc->uc_mcontext.gregs[9]);
  st->print("A6=" ); print_location(st, uc->uc_mcontext.gregs[10]);
  st->print("A7=" ); print_location(st, uc->uc_mcontext.gregs[11]);
  st->cr();
  st->print("T0=" ); print_location(st, uc->uc_mcontext.gregs[12]);
  st->print("T1=" ); print_location(st, uc->uc_mcontext.gregs[13]);
  st->print("T2=" ); print_location(st, uc->uc_mcontext.gregs[14]);
  st->print("T3=" ); print_location(st, uc->uc_mcontext.gregs[15]);
  st->cr();
  st->print("S0=" ); print_location(st, uc->uc_mcontext.gregs[16]);
  st->print("S1=" ); print_location(st, uc->uc_mcontext.gregs[17]);
  st->print("S2=" ); print_location(st, uc->uc_mcontext.gregs[18]);
  st->print("S3=" ); print_location(st, uc->uc_mcontext.gregs[19]);
  st->cr();
  st->print("S4=" ); print_location(st, uc->uc_mcontext.gregs[20]);
  st->print("S5=" ); print_location(st, uc->uc_mcontext.gregs[21]);
  st->print("S6=" ); print_location(st, uc->uc_mcontext.gregs[22]);
  st->print("S7=" ); print_location(st, uc->uc_mcontext.gregs[23]);
  st->cr();
  st->print("T8=" ); print_location(st, uc->uc_mcontext.gregs[24]);
  st->print("T9=" ); print_location(st, uc->uc_mcontext.gregs[25]);
  st->print("K0=" ); print_location(st, uc->uc_mcontext.gregs[26]);
  st->print("K1=" ); print_location(st, uc->uc_mcontext.gregs[27]);
  st->cr();
  st->print("GP=" ); print_location(st, uc->uc_mcontext.gregs[28]);
  st->print("SP=" ); print_location(st, uc->uc_mcontext.gregs[29]);
  st->print("FP=" ); print_location(st, uc->uc_mcontext.gregs[30]);
  st->print("RA=" ); print_location(st, uc->uc_mcontext.gregs[31]);
  st->cr();

}
void os::print_context(outputStream *st, void *context) {
  if (context == NULL) return;

  ucontext_t *uc = (ucontext_t*)context;
  st->print_cr("Registers:");
  st->print(  "R0=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[0]);
  st->print(", AT=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[1]);
  st->print(", V0=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[2]);
  st->print(", V1=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[3]);
  st->cr();
  st->print(  "A0=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[4]);
  st->print(", A1=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[5]);
  st->print(", A2=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[6]);
  st->print(", A3=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[7]);
  st->cr();
  st->print(  "A4=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[8]);
  st->print(", A5=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[9]);
  st->print(", A6=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[10]);
  st->print(", A7=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[11]);
  st->cr();
  st->print(  "T0=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[12]);
  st->print(", T1=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[13]);
  st->print(", T2=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[14]);
  st->print(", T3=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[15]);
  st->cr();
  st->print(  "S0=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[16]);
  st->print(", S1=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[17]);
  st->print(", S2=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[18]);
  st->print(", S3=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[19]);
  st->cr();
  st->print(  "S4=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[20]);
  st->print(", S5=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[21]);
  st->print(", S6=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[22]);
  st->print(", S7=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[23]);
  st->cr();
  st->print(  "T8=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[24]);
  st->print(", T9=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[25]);
  st->print(", K0=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[26]);
  st->print(", K1=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[27]);
  st->cr();
  st->print(  "GP=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[28]);
  st->print(", SP=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[29]);
  st->print(", FP=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[30]);
  st->print(", RA=" INTPTR_FORMAT, (intptr_t)uc->uc_mcontext.gregs[31]);
  st->cr();
  st->cr();

  intptr_t *sp = (intptr_t *)os::Linux::ucontext_get_sp(uc);
  st->print_cr("Top of Stack: (sp=" PTR_FORMAT ")", p2i(sp));
  //print_hex_dump(st, (address)sp, (address)(sp + 8*sizeof(intptr_t)), sizeof(intptr_t));
  print_hex_dump(st, (address)sp-32, (address)(sp + 32), sizeof(intptr_t));
  st->cr();

  // Note: it may be unsafe to inspect memory near pc. For example, pc may
  // point to garbage if entry point in an nmethod is corrupted. Leave
  // this at the end, and hope for the best.
  address pc = os::Linux::ucontext_get_pc(uc);
  st->print_cr("Instructions: (pc=" PTR_FORMAT ")", p2i(pc));
  print_hex_dump(st, pc - 64, pc + 64, sizeof(char));
  Disassembler::decode(pc - 80, pc + 80, st);
}

void os::setup_fpu() {
  /*
  //no use for MIPS
  int fcsr;
  address fpu_cntrl = StubRoutines::addr_fpu_cntrl_wrd_std();
  __asm__ __volatile__ (
      ".set noat;"
      "cfc1 %0, $31;"
      "sw   %0, 0(%1);"
      : "=r" (fcsr)
      : "r" (fpu_cntrl)
      : "memory"
  );
  printf("fpu_cntrl:  %lx\n", fpu_cntrl);
  */
}

#ifndef PRODUCT
void os::verify_stack_alignment() {
  assert(((intptr_t)os::current_stack_pointer() & (StackAlignmentInBytes-1)) == 0, "incorrect stack alignment");
}
#endif

bool os::is_ActiveCoresMP() {
  return UseActiveCoresMP && _initial_active_processor_count == 1;
}
