/*
 * Copyright (c) 2001, 2012, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2020, Loongson Technology. All rights reserved.
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

package sun.jvm.hotspot.runtime.loongarch64;

import java.util.*;
import sun.jvm.hotspot.code.*;
import sun.jvm.hotspot.compiler.*;
import sun.jvm.hotspot.debugger.*;
import sun.jvm.hotspot.oops.*;
import sun.jvm.hotspot.runtime.*;
import sun.jvm.hotspot.types.*;
import sun.jvm.hotspot.utilities.*;

/** Specialization of and implementation of abstract methods of the
    Frame class for the loongarch64 family of CPUs. */

public class LOONGARCH64Frame extends Frame {
  private static final boolean DEBUG;
  static {
    DEBUG = System.getProperty("sun.jvm.hotspot.runtime.loongarch64.LOONGARCH64Frame.DEBUG") != null;
  }

  // Java frames
  private static final int JAVA_FRAME_LINK_OFFSET             =  0;
  private static final int JAVA_FRAME_RETURN_ADDR_OFFSET      =  1;
  private static final int JAVA_FRAME_SENDER_SP_OFFSET        =  2;

  // Native frames
  private static final int NATIVE_FRAME_LINK_OFFSET           =  -2;
  private static final int NATIVE_FRAME_RETURN_ADDR_OFFSET    =  -1;
  private static final int NATIVE_FRAME_SENDER_SP_OFFSET      =  0;

  // Interpreter frames
  private static final int INTERPRETER_FRAME_MIRROR_OFFSET    =  2; // for native calls only
  private static final int INTERPRETER_FRAME_SENDER_SP_OFFSET = -1;
  private static final int INTERPRETER_FRAME_LAST_SP_OFFSET   = INTERPRETER_FRAME_SENDER_SP_OFFSET - 1;
  private static final int INTERPRETER_FRAME_LOCALS_OFFSET    = INTERPRETER_FRAME_LAST_SP_OFFSET - 1;
  private static final int INTERPRETER_FRAME_METHOD_OFFSET    = INTERPRETER_FRAME_LOCALS_OFFSET - 1;
  private static final int INTERPRETER_FRAME_MDX_OFFSET       = INTERPRETER_FRAME_MIRROR_OFFSET - 1;
  private static final int INTERPRETER_FRAME_CACHE_OFFSET     = INTERPRETER_FRAME_MDX_OFFSET - 1;
  private static final int INTERPRETER_FRAME_BCX_OFFSET       = INTERPRETER_FRAME_CACHE_OFFSET - 1;
  private static final int INTERPRETER_FRAME_INITIAL_SP_OFFSET = INTERPRETER_FRAME_BCX_OFFSET - 1;
  private static final int INTERPRETER_FRAME_MONITOR_BLOCK_TOP_OFFSET = INTERPRETER_FRAME_INITIAL_SP_OFFSET;
  private static final int INTERPRETER_FRAME_MONITOR_BLOCK_BOTTOM_OFFSET = INTERPRETER_FRAME_INITIAL_SP_OFFSET;

  // Entry frames
  private static final int ENTRY_FRAME_CALL_WRAPPER_OFFSET = -9;

  // Native frames
  private static final int NATIVE_FRAME_INITIAL_PARAM_OFFSET =  2;

  private static VMReg fp = new VMReg(22 << 1);

  // an additional field beyond sp and pc:
  Address raw_fp; // frame pointer
  private Address raw_unextendedSP;

  private LOONGARCH64Frame() {
  }

  private void adjustForDeopt() {
    if ( pc != null) {
      // Look for a deopt pc and if it is deopted convert to original pc
      CodeBlob cb = VM.getVM().getCodeCache().findBlob(pc);
      if (cb != null && cb.isJavaMethod()) {
        NMethod nm = (NMethod) cb;
        if (pc.equals(nm.deoptHandlerBegin())) {
          if (Assert.ASSERTS_ENABLED) {
            Assert.that(this.getUnextendedSP() != null, "null SP in Java frame");
          }
          // adjust pc if frame is deoptimized.
          pc = this.getUnextendedSP().getAddressAt(nm.origPCOffset());
          deoptimized = true;
        }
      }
    }
  }

  public LOONGARCH64Frame(Address raw_sp, Address raw_fp, Address pc) {
    this.raw_sp = raw_sp;
    this.raw_unextendedSP = raw_sp;
    this.raw_fp = raw_fp;
    this.pc = pc;
    adjustUnextendedSP();

    // Frame must be fully constructed before this call
    adjustForDeopt();

    if (DEBUG) {
      System.out.println("LOONGARCH64Frame(sp, fp, pc): " + this);
      dumpStack();
    }
  }

  public LOONGARCH64Frame(Address raw_sp, Address raw_fp) {
    this.raw_sp = raw_sp;
    this.raw_unextendedSP = raw_sp;
    this.raw_fp = raw_fp;
    this.pc = raw_fp.getAddressAt(1 * VM.getVM().getAddressSize());
    adjustUnextendedSP();

    // Frame must be fully constructed before this call
    adjustForDeopt();

    if (DEBUG) {
      System.out.println("LOONGARCH64Frame(sp, fp): " + this);
      dumpStack();
    }
  }

  public LOONGARCH64Frame(Address raw_sp, Address raw_unextendedSp, Address raw_fp, Address pc) {
    this.raw_sp = raw_sp;
    this.raw_unextendedSP = raw_unextendedSp;
    this.raw_fp = raw_fp;
    this.pc = pc;
    adjustUnextendedSP();

    // Frame must be fully constructed before this call
    adjustForDeopt();

    if (DEBUG) {
      System.out.println("LOONGARCH64Frame(sp, unextendedSP, fp, pc): " + this);
      dumpStack();
    }

  }

  public Object clone() {
    LOONGARCH64Frame frame = new LOONGARCH64Frame();
    frame.raw_sp = raw_sp;
    frame.raw_unextendedSP = raw_unextendedSP;
    frame.raw_fp = raw_fp;
    frame.pc = pc;
    frame.deoptimized = deoptimized;
    return frame;
  }

  public boolean equals(Object arg) {
    if (arg == null) {
      return false;
    }

    if (!(arg instanceof LOONGARCH64Frame)) {
      return false;
    }

    LOONGARCH64Frame other = (LOONGARCH64Frame) arg;

    return (AddressOps.equal(getSP(), other.getSP()) &&
            AddressOps.equal(getUnextendedSP(), other.getUnextendedSP()) &&
            AddressOps.equal(getFP(), other.getFP()) &&
            AddressOps.equal(getPC(), other.getPC()));
  }

  public int hashCode() {
    if (raw_sp == null) {
      return 0;
    }

    return raw_sp.hashCode();
  }

  public String toString() {
    return "sp: " + (getSP() == null? "null" : getSP().toString()) +
         ", unextendedSP: " + (getUnextendedSP() == null? "null" : getUnextendedSP().toString()) +
         ", fp: " + (getFP() == null? "null" : getFP().toString()) +
         ", pc: " + (pc == null? "null" : pc.toString());
  }

  // accessors for the instance variables
  public Address getFP() { return raw_fp; }
  public Address getSP() { return raw_sp; }
  public Address getID() { return raw_sp; }

  // FIXME: not implemented yet (should be done for Solaris/LOONGARCH)
  public boolean isSignalHandlerFrameDbg() { return false; }
  public int     getSignalNumberDbg()      { return 0;     }
  public String  getSignalNameDbg()        { return null;  }

  public boolean isInterpretedFrameValid() {
    if (Assert.ASSERTS_ENABLED) {
      Assert.that(isInterpretedFrame(), "Not an interpreted frame");
    }

    // These are reasonable sanity checks
    if (getFP() == null || getFP().andWithMask(0x3) != null) {
      return false;
    }

    if (getSP() == null || getSP().andWithMask(0x3) != null) {
      return false;
    }

    if (getFP().addOffsetTo(INTERPRETER_FRAME_INITIAL_SP_OFFSET * VM.getVM().getAddressSize()).lessThan(getSP())) {
      return false;
    }

    // These are hacks to keep us out of trouble.
    // The problem with these is that they mask other problems
    if (getFP().lessThanOrEqual(getSP())) {
      // this attempts to deal with unsigned comparison above
      return false;
    }

    if (getFP().minus(getSP()) > 4096 * VM.getVM().getAddressSize()) {
      // stack frames shouldn't be large.
      return false;
    }

    return true;
  }

  // FIXME: not applicable in current system
  //  void    patch_pc(Thread* thread, address pc);

  public Frame sender(RegisterMap regMap, CodeBlob cb) {
    LOONGARCH64RegisterMap map = (LOONGARCH64RegisterMap) regMap;

    if (Assert.ASSERTS_ENABLED) {
      Assert.that(map != null, "map must be set");
    }

    // Default is we done have to follow them. The sender_for_xxx will
    // update it accordingly
    map.setIncludeArgumentOops(false);

    if (isEntryFrame())       return senderForEntryFrame(map);
    if (isInterpretedFrame()) return senderForInterpreterFrame(map);

    if(cb == null) {
      cb = VM.getVM().getCodeCache().findBlob(getPC());
    } else {
      if (Assert.ASSERTS_ENABLED) {
        Assert.that(cb.equals(VM.getVM().getCodeCache().findBlob(getPC())), "Must be the same");
      }
    }

    if (cb != null) {
      return senderForCompiledFrame(map, cb);
    }

    // Must be native-compiled frame, i.e. the marshaling code for native
    // methods that exists in the core system.
    return new LOONGARCH64Frame(getSenderSP(), getLink(), getSenderPC());
  }

  private Frame senderForEntryFrame(LOONGARCH64RegisterMap map) {
    if (DEBUG) {
      System.out.println("senderForEntryFrame");
    }
    if (Assert.ASSERTS_ENABLED) {
      Assert.that(map != null, "map must be set");
    }
    // Java frame called from C; skip all C frames and return top C
    // frame of that chunk as the sender
    LOONGARCH64JavaCallWrapper jcw = (LOONGARCH64JavaCallWrapper) getEntryFrameCallWrapper();
    if (Assert.ASSERTS_ENABLED) {
      Assert.that(!entryFrameIsFirst(), "next Java fp must be non zero");
      Assert.that(jcw.getLastJavaSP().greaterThan(getSP()), "must be above this frame on stack");
    }
    LOONGARCH64Frame fr;
    if (jcw.getLastJavaPC() != null) {
      fr = new LOONGARCH64Frame(jcw.getLastJavaSP(), jcw.getLastJavaFP(), jcw.getLastJavaPC());
    } else {
      fr = new LOONGARCH64Frame(jcw.getLastJavaSP(), jcw.getLastJavaFP());
    }
    map.clear();
    if (Assert.ASSERTS_ENABLED) {
      Assert.that(map.getIncludeArgumentOops(), "should be set by clear");
    }
    return fr;
  }

  //------------------------------------------------------------------------------
  // frame::adjust_unextended_sp
  private void adjustUnextendedSP() {
    // On loongarch, sites calling method handle intrinsics and lambda forms are treated
    // as any other call site. Therefore, no special action is needed when we are
    // returning to any of these call sites.

    CodeBlob cb = cb();
    NMethod senderNm = (cb == null) ? null : cb.asNMethodOrNull();
    if (senderNm != null) {
      // If the sender PC is a deoptimization point, get the original PC.
      if (senderNm.isDeoptEntry(getPC()) ||
          senderNm.isDeoptMhEntry(getPC())) {
        // DEBUG_ONLY(verifyDeoptriginalPc(senderNm, raw_unextendedSp));
      }
    }
  }

  private Frame senderForInterpreterFrame(LOONGARCH64RegisterMap map) {
    if (DEBUG) {
      System.out.println("senderForInterpreterFrame");
    }
    Address unextendedSP = addressOfStackSlot(INTERPRETER_FRAME_SENDER_SP_OFFSET).getAddressAt(0);
    Address sp = getSenderSP();
    // We do not need to update the callee-save register mapping because above
    // us is either another interpreter frame or a converter-frame, but never
    // directly a compiled frame.
    // 11/24/04 SFG. With the removal of adapter frames this is no longer true.
    // However c2 no longer uses callee save register for java calls so there
    // are no callee register to find.

    if (map.getUpdateMap())
      updateMapWithSavedLink(map, addressOfStackSlot(JAVA_FRAME_LINK_OFFSET));

    return new LOONGARCH64Frame(sp, unextendedSP, getLink(), getSenderPC());
  }

  private void updateMapWithSavedLink(RegisterMap map, Address savedFPAddr) {
    map.setLocation(fp, savedFPAddr);
  }

  private Frame senderForCompiledFrame(LOONGARCH64RegisterMap map, CodeBlob cb) {
    if (DEBUG) {
      System.out.println("senderForCompiledFrame");
    }

    //
    // NOTE: some of this code is (unfortunately) duplicated in LOONGARCH64CurrentFrameGuess
    //

    if (Assert.ASSERTS_ENABLED) {
      Assert.that(map != null, "map must be set");
    }

    // frame owned by optimizing compiler
    if (Assert.ASSERTS_ENABLED) {
        Assert.that(cb.getFrameSize() >= 0, "must have non-zero frame size");
    }
    Address senderSP = getUnextendedSP().addOffsetTo(cb.getFrameSize());

    // On Intel the return_address is always the word on the stack
    Address senderPC = senderSP.getAddressAt(-1 * VM.getVM().getAddressSize());

    // This is the saved value of EBP which may or may not really be an FP.
    // It is only an FP if the sender is an interpreter frame (or C1?).
    Address savedFPAddr = senderSP.addOffsetTo(- JAVA_FRAME_SENDER_SP_OFFSET * VM.getVM().getAddressSize());

    if (map.getUpdateMap()) {
      // Tell GC to use argument oopmaps for some runtime stubs that need it.
      // For C1, the runtime stub might not have oop maps, so set this flag
      // outside of update_register_map.
      map.setIncludeArgumentOops(cb.callerMustGCArguments());

      if (cb.getOopMaps() != null) {
        OopMapSet.updateRegisterMap(this, cb, map, true);
      }

      // Since the prolog does the save and restore of EBP there is no oopmap
      // for it so we must fill in its location as if there was an oopmap entry
      // since if our caller was compiled code there could be live jvm state in it.
      updateMapWithSavedLink(map, savedFPAddr);
    }

    return new LOONGARCH64Frame(senderSP, savedFPAddr.getAddressAt(0), senderPC);
  }

  protected boolean hasSenderPD() {
    // FIXME
    // Check for null ebp? Need to do some tests.
    return true;
  }

  public long frameSize() {
    return (getSenderSP().minus(getSP()) / VM.getVM().getAddressSize());
  }

  public Address getLink() {
    if (isJavaFrame())
      return addressOfStackSlot(JAVA_FRAME_LINK_OFFSET).getAddressAt(0);
    return addressOfStackSlot(NATIVE_FRAME_LINK_OFFSET).getAddressAt(0);
  }

  public Address getUnextendedSP() { return raw_unextendedSP; }

  // Return address:
  public Address getSenderPCAddr() {
    if (isJavaFrame())
      return addressOfStackSlot(JAVA_FRAME_RETURN_ADDR_OFFSET);
    return addressOfStackSlot(NATIVE_FRAME_RETURN_ADDR_OFFSET);
  }

  public Address getSenderPC()     { return getSenderPCAddr().getAddressAt(0);      }

  public Address getSenderSP()     {
    if (isJavaFrame())
      return addressOfStackSlot(JAVA_FRAME_SENDER_SP_OFFSET);
    return addressOfStackSlot(NATIVE_FRAME_SENDER_SP_OFFSET);
  }

  // return address of param, zero origin index.
  public Address getNativeParamAddr(int idx) {
    return addressOfStackSlot(NATIVE_FRAME_INITIAL_PARAM_OFFSET + idx);
  }

  public Address addressOfInterpreterFrameLocals() {
    return addressOfStackSlot(INTERPRETER_FRAME_LOCALS_OFFSET);
  }

  private Address addressOfInterpreterFrameBCX() {
    return addressOfStackSlot(INTERPRETER_FRAME_BCX_OFFSET);
  }

  public int getInterpreterFrameBCI() {
    // FIXME: this is not atomic with respect to GC and is unsuitable
    // for use in a non-debugging, or reflective, system. Need to
    // figure out how to express this.
    Address bcp = addressOfInterpreterFrameBCX().getAddressAt(0);
    Address methodHandle = addressOfInterpreterFrameMethod().getAddressAt(0);
    Method method = (Method)Metadata.instantiateWrapperFor(methodHandle);
    return bcpToBci(bcp, method);
  }

  public Address addressOfInterpreterFrameMDX() {
    return addressOfStackSlot(INTERPRETER_FRAME_MDX_OFFSET);
  }

  // FIXME
  //inline int frame::interpreter_frame_monitor_size() {
  //  return BasicObjectLock::size();
  //}

  // expression stack
  // (the max_stack arguments are used by the GC; see class FrameClosure)

  public Address addressOfInterpreterFrameExpressionStack() {
    Address monitorEnd = interpreterFrameMonitorEnd().address();
    return monitorEnd.addOffsetTo(-1 * VM.getVM().getAddressSize());
  }

  public int getInterpreterFrameExpressionStackDirection() { return -1; }

  // top of expression stack
  public Address addressOfInterpreterFrameTOS() {
    return getSP();
  }

  /** Expression stack from top down */
  public Address addressOfInterpreterFrameTOSAt(int slot) {
    return addressOfInterpreterFrameTOS().addOffsetTo(slot * VM.getVM().getAddressSize());
  }

  public Address getInterpreterFrameSenderSP() {
    if (Assert.ASSERTS_ENABLED) {
      Assert.that(isInterpretedFrame(), "interpreted frame expected");
    }
    return addressOfStackSlot(INTERPRETER_FRAME_SENDER_SP_OFFSET).getAddressAt(0);
  }

  // Monitors
  public BasicObjectLock interpreterFrameMonitorBegin() {
    return new BasicObjectLock(addressOfStackSlot(INTERPRETER_FRAME_MONITOR_BLOCK_BOTTOM_OFFSET));
  }

  public BasicObjectLock interpreterFrameMonitorEnd() {
    Address result = addressOfStackSlot(INTERPRETER_FRAME_MONITOR_BLOCK_TOP_OFFSET).getAddressAt(0);
    if (Assert.ASSERTS_ENABLED) {
      // make sure the pointer points inside the frame
      Assert.that(AddressOps.gt(getFP(), result), "result must <  than frame pointer");
      Assert.that(AddressOps.lte(getSP(), result), "result must >= than stack pointer");
    }
    return new BasicObjectLock(result);
  }

  public int interpreterFrameMonitorSize() {
    return BasicObjectLock.size();
  }

  // Method
  public Address addressOfInterpreterFrameMethod() {
    return addressOfStackSlot(INTERPRETER_FRAME_METHOD_OFFSET);
  }

  // Constant pool cache
  public Address addressOfInterpreterFrameCPCache() {
    return addressOfStackSlot(INTERPRETER_FRAME_CACHE_OFFSET);
  }

  // Entry frames
  public JavaCallWrapper getEntryFrameCallWrapper() {
    return new LOONGARCH64JavaCallWrapper(addressOfStackSlot(ENTRY_FRAME_CALL_WRAPPER_OFFSET).getAddressAt(0));
  }

  protected Address addressOfSavedOopResult() {
    // offset is 2 for compiler2 and 3 for compiler1
    return getSP().addOffsetTo((VM.getVM().isClientCompiler() ? 2 : 3) *
                               VM.getVM().getAddressSize());
  }

  protected Address addressOfSavedReceiver() {
    return getSP().addOffsetTo(-4 * VM.getVM().getAddressSize());
  }

  private void dumpStack() {
    if (getFP() != null) {
      for (Address addr = getSP().addOffsetTo(-5 * VM.getVM().getAddressSize());
           AddressOps.lte(addr, getFP().addOffsetTo(5 * VM.getVM().getAddressSize()));
           addr = addr.addOffsetTo(VM.getVM().getAddressSize())) {
        System.out.println(addr + ": " + addr.getAddressAt(0));
      }
    } else {
      for (Address addr = getSP().addOffsetTo(-5 * VM.getVM().getAddressSize());
           AddressOps.lte(addr, getSP().addOffsetTo(20 * VM.getVM().getAddressSize()));
           addr = addr.addOffsetTo(VM.getVM().getAddressSize())) {
        System.out.println(addr + ": " + addr.getAddressAt(0));
      }
    }
  }
}
