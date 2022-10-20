/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2021, Loongson Technology. All rights reserved.
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
#include "asm/assembler.hpp"
#include "asm/assembler.inline.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "interpreter/interpreter.hpp"
#include "memory/cardTableModRefBS.hpp"
#include "memory/resourceArea.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/os.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#include "gc_implementation/g1/heapRegion.hpp"
#endif // INCLUDE_ALL_GCS

#ifdef PRODUCT
#define BLOCK_COMMENT(str) /* nothing */
#define STOP(error) stop(error)
#else
#define BLOCK_COMMENT(str) block_comment(str)
#define STOP(error) block_comment(error); stop(error)
#endif

#define BIND(label) bind(label); BLOCK_COMMENT(#label ":")

// Implementation of AddressLiteral

AddressLiteral::AddressLiteral(address target, relocInfo::relocType rtype) {
  _is_lval = false;
  _target = target;
  _rspec = rspec_from_rtype(rtype, target);
}

// Implementation of Address

Address Address::make_array(ArrayAddress adr) {
  AddressLiteral base = adr.base();
  Address index = adr.index();
  assert(index._disp == 0, "must not have disp"); // maybe it can?
  Address array(index._base, index._index, index._scale, (intptr_t) base.target());
  array._rspec = base._rspec;
  return array;
}

// exceedingly dangerous constructor
Address::Address(address loc, RelocationHolder spec) {
  _base  = noreg;
  _index = noreg;
  _scale = no_scale;
  _disp  = (intptr_t) loc;
  _rspec = spec;
}


int Assembler::is_int_mask(int x) {
  int xx = x;
  int count = 0;

  while (x != 0) {
     x &= (x - 1);
     count++;
  }

  if ((1<<count) == (xx+1)) {
     return count;
  } else {
     return -1;
  }
}

int Assembler::is_jlong_mask(jlong x) {
  jlong  xx = x;
  int count = 0;

  while (x != 0) {
     x &= (x - 1);
     count++;
  }

  if ((1<<count) == (xx+1)) {
     return count;
  } else {
     return -1;
  }
}

int AbstractAssembler::code_fill_byte() {
  return 0x00;                  // illegal instruction 0x00000000
}

// Now the Assembler instruction (identical for 32/64 bits)
void Assembler::ld_b(Register rd, Address src) {
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          ldx_b(dst, base, index);
        } else {
          add_d(AT, base, index);
          ld_b(dst, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        ld_b(dst, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      ldx_b(dst, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      ld_b(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      ldx_b(dst, base, AT);
    }
  }
}

void Assembler::ld_bu(Register rd, Address src) {
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          ldx_bu(dst, base, index);
        } else {
          add_d(AT, base, index);
          ld_bu(dst, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        ld_bu(dst, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      ldx_bu(dst, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      ld_bu(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      ldx_bu(dst, base, AT);
    }
  }
}

void Assembler::ld_d(Register rd, Address src){
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          ldx_d(dst, base, index);
        } else {
          add_d(AT, base, index);
          ld_d(dst, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        ld_d(dst, AT, disp);
      }
    } else if (is_simm(disp, 16) && !(disp & 3)) {
      if (scale == 0) {
        add_d(AT, base, index);
      } else {
        alsl_d(AT, index, base, scale - 1);
      }
      ldptr_d(dst, AT, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      ldx_d(dst, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      ld_d(dst, base, disp);
    } else if (is_simm(disp, 16) && !(disp & 3)) {
      ldptr_d(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      ldx_d(dst, base, AT);
    }
  }
}

void Assembler::ld_h(Register rd, Address src){
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          ldx_h(dst, base, index);
        } else {
          add_d(AT, base, index);
          ld_h(dst, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        ld_h(dst, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      ldx_h(dst, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      ld_h(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      ldx_h(dst, base, AT);
    }
  }
}

void Assembler::ld_hu(Register rd, Address src){
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          ldx_hu(dst, base, index);
        } else {
          add_d(AT, base, index);
          ld_hu(dst, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        ld_hu(dst, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      ldx_hu(dst, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      ld_hu(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      ldx_hu(dst, base, AT);
    }
  }
}

void Assembler::ll_w(Register rd, Address src){
  assert(src.index() == NOREG, "index is unimplemented");
  ll_w(rd, src.base(), src.disp());
}

void Assembler::ll_d(Register rd, Address src){
  assert(src.index() == NOREG, "index is unimplemented");
  ll_d(rd, src.base(), src.disp());
}

void Assembler::ld_w(Register rd, Address src){
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          ldx_w(dst, base, index);
        } else {
          add_d(AT, base, index);
          ld_w(dst, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        ld_w(dst, AT, disp);
      }
    } else if (is_simm(disp, 16) && !(disp & 3)) {
      if (scale == 0) {
        add_d(AT, base, index);
      } else {
        alsl_d(AT, index, base, scale - 1);
      }
      ldptr_w(dst, AT, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      ldx_w(dst, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      ld_w(dst, base, disp);
    } else if (is_simm(disp, 16) && !(disp & 3)) {
      ldptr_w(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      ldx_w(dst, base, AT);
    }
  }
//Disassembler::decode(pc()-32, pc(), tty);
}

void Assembler::ld_wu(Register rd, Address src){
  Register dst   = rd;
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          ldx_wu(dst, base, index);
        } else {
          add_d(AT, base, index);
          ld_wu(dst, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        ld_wu(dst, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      ldx_wu(dst, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      ld_wu(dst, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      ldx_wu(dst, base, AT);
    }
  }
}

void Assembler::st_b(Register rd, Address dst) {
  Register src   = rd;
  Register base  = dst.base();
  Register index = dst.index();

  int scale = dst.scale();
  int disp  = dst.disp();

  if (index != noreg) {
    assert_different_registers(src, AT);
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          stx_b(src, base, index);
        } else {
          add_d(AT, base, index);
          st_b(src, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        st_b(src, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      stx_b(src, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      st_b(src, base, disp);
    } else {
      assert_different_registers(src, AT);
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      stx_b(src, base, AT);
    }
  }
}

void Assembler::sc_w(Register rd, Address dst) {
  assert(dst.index() == NOREG, "index is unimplemented");
  sc_w(rd, dst.base(), dst.disp());
}

void Assembler::sc_d(Register rd, Address dst) {
  assert(dst.index() == NOREG, "index is unimplemented");
  sc_d(rd, dst.base(), dst.disp());
}

void Assembler::st_d(Register rd, Address dst) {
  Register src   = rd;
  Register base  = dst.base();
  Register index = dst.index();

  int scale = dst.scale();
  int disp  = dst.disp();

  if (index != noreg) {
    assert_different_registers(src, AT);
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          stx_d(src, base, index);
        } else {
          add_d(AT, base, index);
          st_d(src, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        st_d(src, AT, disp);
      }
    } else if (is_simm(disp, 16) && !(disp & 3)) {
      if (scale == 0) {
        add_d(AT, base, index);
      } else {
        alsl_d(AT, index, base, scale - 1);
      }
      stptr_d(src, AT, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      stx_d(src, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      st_d(src, base, disp);
    } else if (is_simm(disp, 16) && !(disp & 3)) {
      stptr_d(src, base, disp);
    } else {
      assert_different_registers(src, AT);
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      stx_d(src, base, AT);
    }
  }
}

void Assembler::st_h(Register rd, Address dst) {
  Register src   = rd;
  Register base  = dst.base();
  Register index = dst.index();

  int scale = dst.scale();
  int disp  = dst.disp();

  if (index != noreg) {
    assert_different_registers(src, AT);
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          stx_h(src, base, index);
        } else {
          add_d(AT, base, index);
          st_h(src, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        st_h(src, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      stx_h(src, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      st_h(src, base, disp);
    } else {
      assert_different_registers(src, AT);
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      stx_h(src, base, AT);
    }
  }
}

void Assembler::st_w(Register rd, Address dst) {
  Register src   = rd;
  Register base  = dst.base();
  Register index = dst.index();

  int scale = dst.scale();
  int disp  = dst.disp();

  if (index != noreg) {
    assert_different_registers(src, AT);
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          stx_w(src, base, index);
        } else {
          add_d(AT, base, index);
          st_w(src, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        st_w(src, AT, disp);
      }
    } else if (is_simm(disp, 16) && !(disp & 3)) {
      if (scale == 0) {
        add_d(AT, base, index);
      } else {
        alsl_d(AT, index, base, scale - 1);
      }
      stptr_w(src, AT, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      stx_w(src, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      st_w(src, base, disp);
    } else if (is_simm(disp, 16) && !(disp & 3)) {
      stptr_w(src, base, disp);
    } else {
      assert_different_registers(src, AT);
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      stx_w(src, base, AT);
    }
  }
}

void Assembler::fld_s(FloatRegister fd, Address src) {
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          fldx_s(fd, base, index);
        } else {
          add_d(AT, base, index);
          fld_s(fd, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        fld_s(fd, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      fldx_s(fd, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      fld_s(fd, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      fldx_s(fd, base, AT);
    }
  }
}

void Assembler::fld_d(FloatRegister fd, Address src) {
  Register base  = src.base();
  Register index = src.index();

  int scale = src.scale();
  int disp  = src.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          fldx_d(fd, base, index);
        } else {
          add_d(AT, base, index);
          fld_d(fd, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        fld_d(fd, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      fldx_d(fd, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      fld_d(fd, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      fldx_d(fd, base, AT);
    }
  }
}

void Assembler::fst_s(FloatRegister fd, Address dst) {
  Register base  = dst.base();
  Register index = dst.index();

  int scale = dst.scale();
  int disp  = dst.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          fstx_s(fd, base, index);
        } else {
          add_d(AT, base, index);
          fst_s(fd, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        fst_s(fd, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      fstx_s(fd, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      fst_s(fd, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      fstx_s(fd, base, AT);
    }
  }
}

void Assembler::fst_d(FloatRegister fd, Address dst) {
  Register base  = dst.base();
  Register index = dst.index();

  int scale = dst.scale();
  int disp  = dst.disp();

  if (index != noreg) {
    if (is_simm(disp, 12)) {
      if (scale == 0) {
        if (disp == 0) {
          fstx_d(fd, base, index);
        } else {
          add_d(AT, base, index);
          fst_d(fd, AT, disp);
        }
      } else {
        alsl_d(AT, index, base, scale - 1);
        fst_d(fd, AT, disp);
      }
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));

      if (scale == 0) {
        add_d(AT, AT, index);
      } else {
        alsl_d(AT, index, AT, scale - 1);
      }
      fstx_d(fd, base, AT);
    }
  } else {
    if (is_simm(disp, 12)) {
      fst_d(fd, base, disp);
    } else {
      lu12i_w(AT, split_low20(disp >> 12));
      if (split_low12(disp))
        ori(AT, AT, split_low12(disp));
      fstx_d(fd, base, AT);
    }
  }
}
