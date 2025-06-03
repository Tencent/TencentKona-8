/*
 * Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.
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

package sun.jvm.hotspot.debugger.loongarch64;

import sun.jvm.hotspot.debugger.*;
import sun.jvm.hotspot.debugger.cdbg.*;

/** Specifies the thread context on loongarch64 platforms; only a sub-portion
    of the context is guaranteed to be present on all operating
    systems. */

public abstract class LOONGARCH64ThreadContext implements ThreadContext {

  // NOTE: the indices for the various registers must be maintained as
  // listed across various operating systems. However, only a small
  // subset of the registers' values are guaranteed to be present (and
  // must be present for the SA's stack walking to work): EAX, EBX,
  // ECX, EDX, ESI, EDI, EBP, ESP, and EIP.

  public static final int ZERO = 0;
  public static final int RA = 1;
  public static final int TP = 2;
  public static final int SP = 3;
  public static final int A0 = 4;
  public static final int A1 = 5;
  public static final int A2 = 6;
  public static final int A3 = 7;
  public static final int A4 = 8;
  public static final int A5 = 9;
  public static final int A6 = 10;
  public static final int A7 = 11;
  public static final int T0 = 12;
  public static final int T1 = 13;
  public static final int T2 = 14;
  public static final int T3 = 15;
  public static final int T4 = 16;
  public static final int T5 = 17;
  public static final int T6 = 18;
  public static final int T7 = 19;
  public static final int T8 = 20;
  public static final int RX = 21;
  public static final int FP = 22;
  public static final int S0 = 23;
  public static final int S1 = 24;
  public static final int S2 = 25;
  public static final int S3 = 26;
  public static final int S4 = 27;
  public static final int S5 = 28;
  public static final int S6 = 29;
  public static final int S7 = 30;
  public static final int S8 = 31;
  public static final int PC = 32;
  public static final int NPRGREG = 33;

  private static final String[] regNames = {
    "ZERO",    "RA",    "TP",    "SP",
    "A0",      "A1",    "A2",    "A3",
    "A4",      "A5",    "A6",    "A7",
    "T0",      "T1",    "T2",    "T3",
    "T4",      "T5",    "T6",    "T7",
    "T8",      "RX",    "FP",    "S0",
    "S1",      "S2",    "S3",    "S4",
    "S5",      "S6",    "S7",    "S8",
    "PC"
  };

  private long[] data;

  public LOONGARCH64ThreadContext() {
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
