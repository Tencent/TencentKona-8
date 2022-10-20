/*
 * Copyright (c) 2002, 2003, Oracle and/or its affiliates. All rights reserved.
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

package sun.jvm.hotspot.debugger.proc.loongarch64;

import sun.jvm.hotspot.debugger.*;
import sun.jvm.hotspot.debugger.loongarch64.*;
import sun.jvm.hotspot.debugger.proc.*;
import sun.jvm.hotspot.utilities.*;

public class ProcLOONGARCH64Thread implements ThreadProxy {
  private ProcDebugger debugger;
  private int         id;

  public ProcLOONGARCH64Thread(ProcDebugger debugger, Address addr) {
    this.debugger = debugger;

    // FIXME: the size here should be configurable. However, making it
    // so would produce a dependency on the "types" package from the
    // debugger package, which is not desired.
    this.id       = (int) addr.getCIntegerAt(0, 4, true);
  }

  public ProcLOONGARCH64Thread(ProcDebugger debugger, long id) {
    this.debugger = debugger;
    this.id = (int) id;
  }

  public ThreadContext getContext() throws IllegalThreadStateException {
    ProcLOONGARCH64ThreadContext context = new ProcLOONGARCH64ThreadContext(debugger);
    long[] regs = debugger.getThreadIntegerRegisterSet(id);
    /*
       _NGREG in reg.h is defined to be 19. Because we have included
       debug registers LOONGARCH64ThreadContext.NPRGREG is 25.
    */

    if (Assert.ASSERTS_ENABLED) {
      Assert.that(regs.length <= LOONGARCH64ThreadContext.NPRGREG, "size of register set is greater than " + LOONGARCH64ThreadContext.NPRGREG);
    }
    for (int i = 0; i < regs.length; i++) {
      context.setRegister(i, regs[i]);
    }
    return context;
  }

  public boolean canSetContext() throws DebuggerException {
    return false;
  }

  public void setContext(ThreadContext context)
    throws IllegalThreadStateException, DebuggerException {
    throw new DebuggerException("Unimplemented");
  }

  public String toString() {
    return "t@" + id;
  }

  public boolean equals(Object obj) {
    if ((obj == null) || !(obj instanceof ProcLOONGARCH64Thread)) {
      return false;
    }

    return (((ProcLOONGARCH64Thread) obj).id == id);
  }

  public int hashCode() {
    return id;
  }
}
