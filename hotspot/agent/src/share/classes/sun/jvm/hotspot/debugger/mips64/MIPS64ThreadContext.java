/*
 * Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2018, Loongson Technology. All rights reserved.
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

package sun.jvm.hotspot.debugger.mips64;

import sun.jvm.hotspot.debugger.*;
import sun.jvm.hotspot.debugger.cdbg.*;

/** Specifies the thread context on mips64 platforms; only a sub-portion
    of the context is guaranteed to be present on all operating
    systems. */

public abstract class MIPS64ThreadContext implements ThreadContext {

  // NOTE: the indices for the various registers must be maintained as
  // listed across various operating systems. However, only a small
  // subset of the registers' values are guaranteed to be present (and
  // must be present for the SA's stack walking to work): EAX, EBX,
  // ECX, EDX, ESI, EDI, EBP, ESP, and EIP.

  public static final int ZERO = 0;
  public static final int AT = 1;
  public static final int V0 = 2;
  public static final int V1 = 3;
  public static final int A0 = 4;
  public static final int A1 = 5;
  public static final int A2 = 6;
  public static final int A3 = 7;
  public static final int T0 = 8;
  public static final int T1 = 9;
  public static final int T2 = 10;
  public static final int T3 = 11;
  public static final int T4 = 12;
  public static final int T5 = 13;
  public static final int T6 = 14;
  public static final int T7 = 15;
  public static final int S0 = 16;
  public static final int S1 = 17;
  public static final int S2 = 18;
  public static final int S3 = 19;
  public static final int S4 = 20;
  public static final int S5 = 21;
  public static final int S6 = 22;
  public static final int S7 = 23;
  public static final int T8 = 24;
  public static final int T9 = 25;
  public static final int K0 = 26;
  public static final int K1 = 27;
  public static final int GP = 28;
  public static final int SP = 29;
  public static final int FP = 30;
  public static final int RA = 31;
  public static final int PC = 32;
  public static final int NPRGREG = 33;

  private static final String[] regNames = {
    "ZERO",    "AT",    "V0",    "V1",
    "A0",      "A1",    "A2",    "A3",
    "T0",      "T1",    "T2",    "T3",
    "T4",      "T5",    "T6",    "T7",
    "S0",      "S1",    "S2",    "S3",
    "S4",      "S5",    "S6",    "S7",
    "T8",      "T9",    "K0",    "K1",
    "GP",      "SP",    "FP",    "RA",
    "PC"
  };

  private long[] data;

  public MIPS64ThreadContext() {
    data = new long[NPRGREG];
  }

  public int getNumRegisters() {
    return NPRGREG;
  }

  public String getRegisterName(int index) {
    return regNames[index];
  }

  public void setRegister(int index, long value) {
    data[index] = value;
  }

  public long getRegister(int index) {
    return data[index];
  }

  public CFrame getTopFrame(Debugger dbg) {
    return null;
  }

  /** This can't be implemented in this class since we would have to
      tie the implementation to, for example, the debugging system */
  public abstract void setRegisterAsAddress(int index, Address value);

  /** This can't be implemented in this class since we would have to
      tie the implementation to, for example, the debugging system */
  public abstract Address getRegisterAsAddress(int index);
}
