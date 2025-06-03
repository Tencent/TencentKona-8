/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_LOONGARCH_VM_ASSEMBLER_LOONGARCH_HPP
#define CPU_LOONGARCH_VM_ASSEMBLER_LOONGARCH_HPP

#include "asm/register.hpp"

class BiasedLockingCounters;


// Note: A register location is represented via a Register, not
//       via an address for efficiency & simplicity reasons.

class ArrayAddress;

class Address VALUE_OBJ_CLASS_SPEC {
 public:
  enum ScaleFactor {
    no_scale = -1,
    times_1  =  0,
    times_2  =  1,
    times_4  =  2,
    times_8  =  3,
    times_ptr = times_8
  };
  static ScaleFactor times(int size) {
    assert(size >= 1 && size <= 8 && is_power_of_2(size), "bad scale size");
    if (size == 8)  return times_8;
    if (size == 4)  return times_4;
    if (size == 2)  return times_2;
    return times_1;
  }

 private:
  Register         _base;
  Register         _index;
  ScaleFactor      _scale;
  int              _disp;
  RelocationHolder _rspec;

  // Easily misused constructors make them private
  Address(address loc, RelocationHolder spec);
  Address(int disp, address loc, relocInfo::relocType rtype);
  Address(int disp, address loc, RelocationHolder spec);

 public:

  // creation
  Address()
    : _base(noreg),
      _index(noreg),
      _scale(no_scale),
      _disp(0) {
  }

  // No default displacement otherwise Register can be implicitly
  // converted to 0(Register) which is quite a different animal.

  Address(Register base, int disp = 0)
    : _base(base),
      _index(noreg),
      _scale(no_scale),
      _disp(disp) {
    assert_different_registers(_base, AT);
  }

  Address(Register base, Register index, ScaleFactor scale, int disp = 0)
    : _base (base),
      _index(index),
      _scale(scale),
      _disp (disp) {
    assert(!index->is_valid() == (scale == Address::no_scale), "inconsistent address");
    assert_different_registers(_base, _index, AT);
  }

  // The following two overloads are used in connection with the
  // ByteSize type (see sizes.hpp).  They simplify the use of
  // ByteSize'd arguments in assembly code. Note that their equivalent
  // for the optimized build are the member functions with int disp
  // argument since ByteSize is mapped to an int type in that case.
  //
  // Note: DO NOT introduce similar overloaded functions for WordSize
  // arguments as in the optimized mode, both ByteSize and WordSize
  // are mapped to the same type and thus the compiler cannot make a
  // distinction anymore (=> compiler errors).

#ifdef ASSERT
  Address(Register base, ByteSize disp)
    : _base(base),
      _index(noreg),
      _scale(no_scale),
      _disp(in_bytes(disp)) {
    assert_different_registers(_base, AT);
  }

  Address(Register base, Register index, ScaleFactor scale, ByteSize disp)
    : _base(base),
      _index(index),
      _scale(scale),
      _disp(in_bytes(disp)) {
    assert(!index->is_valid() == (scale == Address::no_scale), "inconsistent address");
    assert_different_registers(_base, _index, AT);
  }
#endif // ASSERT

  // accessors
  bool        uses(Register reg) const { return _base == reg || _index == reg; }
  Register    base()             const { return _base;  }
  Register    index()            const { return _index; }
  ScaleFactor scale()            const { return _scale; }
  int         disp()             const { return _disp;  }

  static Address make_array(ArrayAddress);

  friend class Assembler;
  friend class MacroAssembler;
  friend class LIR_Assembler; // base/index/scale/disp
};

// Calling convention
class Argument VALUE_OBJ_CLASS_SPEC {
 public:
  enum {
    n_register_parameters = 8,   // 8 integer registers used to pass parameters
    n_float_register_parameters = 8   // 8 float registers used to pass parameters
  };
};

//
// AddressLiteral has been split out from Address because operands of this type
// need to be treated specially on 32bit vs. 64bit platforms. By splitting it out
// the few instructions that need to deal with address literals are unique and the
// MacroAssembler does not have to implement every instruction in the Assembler
// in order to search for address literals that may need special handling depending
// on the instruction and the platform. As small step on the way to merging i486/amd64
// directories.
//
class AddressLiteral VALUE_OBJ_CLASS_SPEC {
  friend class ArrayAddress;
  RelocationHolder _rspec;
  // Typically we use AddressLiterals we want to use their rval
  // However in some situations we want the lval (effect address) of the item.
  // We provide a special factory for making those lvals.
  bool _is_lval;

  // If the target is far we'll need to load the ea of this to
  // a register to reach it. Otherwise if near we can do rip
  // relative addressing.

  address          _target;

 protected:
  // creation
  AddressLiteral()
    : _is_lval(false),
      _target(NULL)
  {}

  public:


  AddressLiteral(address target, relocInfo::relocType rtype);

  AddressLiteral(address target, RelocationHolder const& rspec)
    : _rspec(rspec),
      _is_lval(false),
      _target(target)
  {}
   // 32-bit complains about a multiple declaration for int*.
   AddressLiteral(intptr_t* addr, relocInfo::relocType rtype = relocInfo::none)
     : _target((address) addr),
       _rspec(rspec_from_rtype(rtype, (address) addr)) {}

  AddressLiteral addr() {
    AddressLiteral ret = *this;
    ret._is_lval = true;
    return ret;
  }


 private:

  address target() { return _target; }
  bool is_lval() { return _is_lval; }

  relocInfo::relocType reloc() const { return _rspec.type(); }
  const RelocationHolder& rspec() const { return _rspec; }

  friend class Assembler;
  friend class MacroAssembler;
  friend class Address;
  friend class LIR_Assembler;
  RelocationHolder rspec_from_rtype(relocInfo::relocType rtype, address addr) {
    switch (rtype) {
      case relocInfo::external_word_type:
        return external_word_Relocation::spec(addr);
      case relocInfo::internal_word_type:
        return internal_word_Relocation::spec(addr);
      case relocInfo::opt_virtual_call_type:
        return opt_virtual_call_Relocation::spec();
      case relocInfo::static_call_type:
        return static_call_Relocation::spec();
      case relocInfo::runtime_call_type:
        return runtime_call_Relocation::spec();
      case relocInfo::poll_type:
      case relocInfo::poll_return_type:
        return Relocation::spec_simple(rtype);
      case relocInfo::none:
      case relocInfo::oop_type:
        // Oops are a special case. Normally they would be their own section
        // but in cases like icBuffer they are literals in the code stream that
        // we don't have a section for. We use none so that we get a literal address
        // which is always patchable.
        return RelocationHolder();
      default:
        ShouldNotReachHere();
        return RelocationHolder();
    }
  }

};

// Convience classes
class RuntimeAddress: public AddressLiteral {

 public:

  RuntimeAddress(address target) : AddressLiteral(target, relocInfo::runtime_call_type) {}

};

class OopAddress: public AddressLiteral {

 public:

  OopAddress(address target) : AddressLiteral(target, relocInfo::oop_type){}

};

class ExternalAddress: public AddressLiteral {

 public:

  ExternalAddress(address target) : AddressLiteral(target, relocInfo::external_word_type){}

};

class InternalAddress: public AddressLiteral {

 public:

  InternalAddress(address target) : AddressLiteral(target, relocInfo::internal_word_type) {}

};

// x86 can do array addressing as a single operation since disp can be an absolute
// address amd64 can't. We create a class that expresses the concept but does extra
// magic on amd64 to get the final result

class ArrayAddress VALUE_OBJ_CLASS_SPEC {
  private:

  AddressLiteral _base;
  Address        _index;

  public:

  ArrayAddress() {};
  ArrayAddress(AddressLiteral base, Address index): _base(base), _index(index) {};
  AddressLiteral base() { return _base; }
  Address index() { return _index; }

};

// The LoongArch Assembler: Pure assembler doing NO optimizations on the instruction
// level ; i.e., what you write is what you get. The Assembler is generating code into
// a CodeBuffer.

class Assembler : public AbstractAssembler  {
  friend class AbstractAssembler; // for the non-virtual hack
  friend class LIR_Assembler; // as_Address()
  friend class StubGenerator;

 public:
  // 22-bit opcode, highest 22 bits: bits[31...10]
  enum ops22 {
    clo_w_op           = 0b0000000000000000000100,
    clz_w_op           = 0b0000000000000000000101,
    cto_w_op           = 0b0000000000000000000110,
    ctz_w_op           = 0b0000000000000000000111,
    clo_d_op           = 0b0000000000000000001000,
    clz_d_op           = 0b0000000000000000001001,
    cto_d_op           = 0b0000000000000000001010,
    ctz_d_op           = 0b0000000000000000001011,
    revb_2h_op         = 0b0000000000000000001100,
    revb_4h_op         = 0b0000000000000000001101,
    revb_2w_op         = 0b0000000000000000001110,
    revb_d_op          = 0b0000000000000000001111,
    revh_2w_op         = 0b0000000000000000010000,
    revh_d_op          = 0b0000000000000000010001,
    bitrev_4b_op       = 0b0000000000000000010010,
    bitrev_8b_op       = 0b0000000000000000010011,
    bitrev_w_op        = 0b0000000000000000010100,
    bitrev_d_op        = 0b0000000000000000010101,
    ext_w_h_op         = 0b0000000000000000010110,
    ext_w_b_op         = 0b0000000000000000010111,
    rdtimel_w_op       = 0b0000000000000000011000,
    rdtimeh_w_op       = 0b0000000000000000011001,
    rdtime_d_op        = 0b0000000000000000011010,
    cpucfg_op          = 0b0000000000000000011011,
    fabs_s_op          = 0b0000000100010100000001,
    fabs_d_op          = 0b0000000100010100000010,
    fneg_s_op          = 0b0000000100010100000101,
    fneg_d_op          = 0b0000000100010100000110,
    flogb_s_op         = 0b0000000100010100001001,
    flogb_d_op         = 0b0000000100010100001010,
    fclass_s_op        = 0b0000000100010100001101,
    fclass_d_op        = 0b0000000100010100001110,
    fsqrt_s_op         = 0b0000000100010100010001,
    fsqrt_d_op         = 0b0000000100010100010010,
    frecip_s_op        = 0b0000000100010100010101,
    frecip_d_op        = 0b0000000100010100010110,
    frsqrt_s_op        = 0b0000000100010100011001,
    frsqrt_d_op        = 0b0000000100010100011010,
    fmov_s_op          = 0b0000000100010100100101,
    fmov_d_op          = 0b0000000100010100100110,
    movgr2fr_w_op      = 0b0000000100010100101001,
    movgr2fr_d_op      = 0b0000000100010100101010,
    movgr2frh_w_op     = 0b0000000100010100101011,
    movfr2gr_s_op      = 0b0000000100010100101101,
    movfr2gr_d_op      = 0b0000000100010100101110,
    movfrh2gr_s_op     = 0b0000000100010100101111,
    movgr2fcsr_op      = 0b0000000100010100110000,
    movfcsr2gr_op      = 0b0000000100010100110010,
    movfr2cf_op        = 0b0000000100010100110100,
    movcf2fr_op        = 0b0000000100010100110101,
    movgr2cf_op        = 0b0000000100010100110110,
    movcf2gr_op        = 0b0000000100010100110111,
    fcvt_s_d_op        = 0b0000000100011001000110,
    fcvt_d_s_op        = 0b0000000100011001001001,
    ftintrm_w_s_op     = 0b0000000100011010000001,
    ftintrm_w_d_op     = 0b0000000100011010000010,
    ftintrm_l_s_op     = 0b0000000100011010001001,
    ftintrm_l_d_op     = 0b0000000100011010001010,
    ftintrp_w_s_op     = 0b0000000100011010010001,
    ftintrp_w_d_op     = 0b0000000100011010010010,
    ftintrp_l_s_op     = 0b0000000100011010011001,
    ftintrp_l_d_op     = 0b0000000100011010011010,
    ftintrz_w_s_op     = 0b0000000100011010100001,
    ftintrz_w_d_op     = 0b0000000100011010100010,
    ftintrz_l_s_op     = 0b0000000100011010101001,
    ftintrz_l_d_op     = 0b0000000100011010101010,
    ftintrne_w_s_op    = 0b0000000100011010110001,
    ftintrne_w_d_op    = 0b0000000100011010110010,
    ftintrne_l_s_op    = 0b0000000100011010111001,
    ftintrne_l_d_op    = 0b0000000100011010111010,
    ftint_w_s_op       = 0b0000000100011011000001,
    ftint_w_d_op       = 0b0000000100011011000010,
    ftint_l_s_op       = 0b0000000100011011001001,
    ftint_l_d_op       = 0b0000000100011011001010,
    ffint_s_w_op       = 0b0000000100011101000100,
    ffint_s_l_op       = 0b0000000100011101000110,
    ffint_d_w_op       = 0b0000000100011101001000,
    ffint_d_l_op       = 0b0000000100011101001010,
    frint_s_op         = 0b0000000100011110010001,
    frint_d_op         = 0b0000000100011110010010,
    iocsrrd_b_op       = 0b0000011001001000000000,
    iocsrrd_h_op       = 0b0000011001001000000001,
    iocsrrd_w_op       = 0b0000011001001000000010,
    iocsrrd_d_op       = 0b0000011001001000000011,
    iocsrwr_b_op       = 0b0000011001001000000100,
    iocsrwr_h_op       = 0b0000011001001000000101,
    iocsrwr_w_op       = 0b0000011001001000000110,
    iocsrwr_d_op       = 0b0000011001001000000111,
    vpcnt_b_op         = 0b0111001010011100001000,
    vpcnt_h_op         = 0b0111001010011100001001,
    vpcnt_w_op         = 0b0111001010011100001010,
    vpcnt_d_op         = 0b0111001010011100001011,
    vneg_b_op          = 0b0111001010011100001100,
    vneg_h_op          = 0b0111001010011100001101,
    vneg_w_op          = 0b0111001010011100001110,
    vneg_d_op          = 0b0111001010011100001111,
    vfclass_s_op       = 0b0111001010011100110101,
    vfclass_d_op       = 0b0111001010011100110110,
    vfsqrt_s_op        = 0b0111001010011100111001,
    vfsqrt_d_op        = 0b0111001010011100111010,
    vfrint_s_op        = 0b0111001010011101001101,
    vfrint_d_op        = 0b0111001010011101001110,
    vfrintrm_s_op      = 0b0111001010011101010001,
    vfrintrm_d_op      = 0b0111001010011101010010,
    vfrintrp_s_op      = 0b0111001010011101010101,
    vfrintrp_d_op      = 0b0111001010011101010110,
    vfrintrz_s_op      = 0b0111001010011101011001,
    vfrintrz_d_op      = 0b0111001010011101011010,
    vfrintrne_s_op     = 0b0111001010011101011101,
    vfrintrne_d_op     = 0b0111001010011101011110,
    vfcvtl_s_h_op      = 0b0111001010011101111010,
    vfcvth_s_h_op      = 0b0111001010011101111011,
    vfcvtl_d_s_op      = 0b0111001010011101111100,
    vfcvth_d_s_op      = 0b0111001010011101111101,
    vffint_s_w_op      = 0b0111001010011110000000,
    vffint_s_wu_op     = 0b0111001010011110000001,
    vffint_d_l_op      = 0b0111001010011110000010,
    vffint_d_lu_op     = 0b0111001010011110000011,
    vffintl_d_w_op     = 0b0111001010011110000100,
    vffinth_d_w_op     = 0b0111001010011110000101,
    vftint_w_s_op      = 0b0111001010011110001100,
    vftint_l_d_op      = 0b0111001010011110001101,
    vftintrm_w_s_op    = 0b0111001010011110001110,
    vftintrm_l_d_op    = 0b0111001010011110001111,
    vftintrp_w_s_op    = 0b0111001010011110010000,
    vftintrp_l_d_op    = 0b0111001010011110010001,
    vftintrz_w_s_op    = 0b0111001010011110010010,
    vftintrz_l_d_op    = 0b0111001010011110010011,
    vftintrne_w_s_op   = 0b0111001010011110010100,
    vftintrne_l_d_op   = 0b0111001010011110010101,
    vftint_wu_s        = 0b0111001010011110010110,
    vftint_lu_d        = 0b0111001010011110010111,
    vftintrz_wu_f      = 0b0111001010011110011100,
    vftintrz_lu_d      = 0b0111001010011110011101,
    vftintl_l_s_op     = 0b0111001010011110100000,
    vftinth_l_s_op     = 0b0111001010011110100001,
    vftintrml_l_s_op   = 0b0111001010011110100010,
    vftintrmh_l_s_op   = 0b0111001010011110100011,
    vftintrpl_l_s_op   = 0b0111001010011110100100,
    vftintrph_l_s_op   = 0b0111001010011110100101,
    vftintrzl_l_s_op   = 0b0111001010011110100110,
    vftintrzh_l_s_op   = 0b0111001010011110100111,
    vftintrnel_l_s_op  = 0b0111001010011110101000,
    vftintrneh_l_s_op  = 0b0111001010011110101001,
    vreplgr2vr_b_op    = 0b0111001010011111000000,
    vreplgr2vr_h_op    = 0b0111001010011111000001,
    vreplgr2vr_w_op    = 0b0111001010011111000010,
    vreplgr2vr_d_op    = 0b0111001010011111000011,
    xvpcnt_b_op        = 0b0111011010011100001000,
    xvpcnt_h_op        = 0b0111011010011100001001,
    xvpcnt_w_op        = 0b0111011010011100001010,
    xvpcnt_d_op        = 0b0111011010011100001011,
    xvneg_b_op         = 0b0111011010011100001100,
    xvneg_h_op         = 0b0111011010011100001101,
    xvneg_w_op         = 0b0111011010011100001110,
    xvneg_d_op         = 0b0111011010011100001111,
    xvfclass_s_op      = 0b0111011010011100110101,
    xvfclass_d_op      = 0b0111011010011100110110,
    xvfsqrt_s_op       = 0b0111011010011100111001,
    xvfsqrt_d_op       = 0b0111011010011100111010,
    xvfrint_s_op       = 0b0111011010011101001101,
    xvfrint_d_op       = 0b0111011010011101001110,
    xvfrintrm_s_op     = 0b0111011010011101010001,
    xvfrintrm_d_op     = 0b0111011010011101010010,
    xvfrintrp_s_op     = 0b0111011010011101010101,
    xvfrintrp_d_op     = 0b0111011010011101010110,
    xvfrintrz_s_op     = 0b0111011010011101011001,
    xvfrintrz_d_op     = 0b0111011010011101011010,
    xvfrintrne_s_op    = 0b0111011010011101011101,
    xvfrintrne_d_op    = 0b0111011010011101011110,
    xvfcvtl_s_h_op     = 0b0111011010011101111010,
    xvfcvth_s_h_op     = 0b0111011010011101111011,
    xvfcvtl_d_s_op     = 0b0111011010011101111100,
    xvfcvth_d_s_op     = 0b0111011010011101111101,
    xvffint_s_w_op     = 0b0111011010011110000000,
    xvffint_s_wu_op    = 0b0111011010011110000001,
    xvffint_d_l_op     = 0b0111011010011110000010,
    xvffint_d_lu_op    = 0b0111011010011110000011,
    xvffintl_d_w_op    = 0b0111011010011110000100,
    xvffinth_d_w_op    = 0b0111011010011110000101,
    xvftint_w_s_op     = 0b0111011010011110001100,
    xvftint_l_d_op     = 0b0111011010011110001101,
    xvftintrm_w_s_op   = 0b0111011010011110001110,
    xvftintrm_l_d_op   = 0b0111011010011110001111,
    xvftintrp_w_s_op   = 0b0111011010011110010000,
    xvftintrp_l_d_op   = 0b0111011010011110010001,
    xvftintrz_w_s_op   = 0b0111011010011110010010,
    xvftintrz_l_d_op   = 0b0111011010011110010011,
    xvftintrne_w_s_op  = 0b0111011010011110010100,
    xvftintrne_l_d_op  = 0b0111011010011110010101,
    xvftint_wu_s       = 0b0111011010011110010110,
    xvftint_lu_d       = 0b0111011010011110010111,
    xvftintrz_wu_f     = 0b0111011010011110011100,
    xvftintrz_lu_d     = 0b0111011010011110011101,
    xvftintl_l_s_op    = 0b0111011010011110100000,
    xvftinth_l_s_op    = 0b0111011010011110100001,
    xvftintrml_l_s_op  = 0b0111011010011110100010,
    xvftintrmh_l_s_op  = 0b0111011010011110100011,
    xvftintrpl_l_s_op  = 0b0111011010011110100100,
    xvftintrph_l_s_op  = 0b0111011010011110100101,
    xvftintrzl_l_s_op  = 0b0111011010011110100110,
    xvftintrzh_l_s_op  = 0b0111011010011110100111,
    xvftintrnel_l_s_op = 0b0111011010011110101000,
    xvftintrneh_l_s_op = 0b0111011010011110101001,
    xvreplgr2vr_b_op   = 0b0111011010011111000000,
    xvreplgr2vr_h_op   = 0b0111011010011111000001,
    xvreplgr2vr_w_op   = 0b0111011010011111000010,
    xvreplgr2vr_d_op   = 0b0111011010011111000011,
    vext2xv_h_b_op     = 0b0111011010011111000100,
    vext2xv_w_b_op     = 0b0111011010011111000101,
    vext2xv_d_b_op     = 0b0111011010011111000110,
    vext2xv_w_h_op     = 0b0111011010011111000111,
    vext2xv_d_h_op     = 0b0111011010011111001000,
    vext2xv_d_w_op     = 0b0111011010011111001001,
    vext2xv_hu_bu_op   = 0b0111011010011111001010,
    vext2xv_wu_bu_op   = 0b0111011010011111001011,
    vext2xv_du_bu_op   = 0b0111011010011111001100,
    vext2xv_wu_hu_op   = 0b0111011010011111001101,
    vext2xv_du_hu_op   = 0b0111011010011111001110,
    vext2xv_du_wu_op   = 0b0111011010011111001111,
    xvreplve0_b_op     = 0b0111011100000111000000,
    xvreplve0_h_op     = 0b0111011100000111100000,
    xvreplve0_w_op     = 0b0111011100000111110000,
    xvreplve0_d_op     = 0b0111011100000111111000,
    xvreplve0_q_op     = 0b0111011100000111111100,

    unknow_ops22       = 0b1111111111111111111111
  };

  // 21-bit opcode, highest 21 bits: bits[31...11]
  enum ops21 {
    vinsgr2vr_d_op     = 0b011100101110101111110,
    vpickve2gr_d_op    = 0b011100101110111111110,
    vpickve2gr_du_op   = 0b011100101111001111110,
    vreplvei_d_op      = 0b011100101111011111110,

    unknow_ops21       = 0b111111111111111111111
  };

  // 20-bit opcode, highest 20 bits: bits[31...12]
  enum ops20 {
    vinsgr2vr_w_op     = 0b01110010111010111110,
    vpickve2gr_w_op    = 0b01110010111011111110,
    vpickve2gr_wu_op   = 0b01110010111100111110,
    vreplvei_w_op      = 0b01110010111101111110,
    xvinsgr2vr_d_op    = 0b01110110111010111110,
    xvpickve2gr_d_op   = 0b01110110111011111110,
    xvpickve2gr_du_op  = 0b01110110111100111110,
    xvinsve0_d_op      = 0b01110110111111111110,
    xvpickve_d_op      = 0b01110111000000111110,

    unknow_ops20       = 0b11111111111111111111
  };

  // 19-bit opcode, highest 19 bits: bits[31...13]
  enum ops19 {
    vrotri_b_op        = 0b0111001010100000001,
    vinsgr2vr_h_op     = 0b0111001011101011110,
    vpickve2gr_h_op    = 0b0111001011101111110,
    vpickve2gr_hu_op   = 0b0111001011110011110,
    vreplvei_h_op      = 0b0111001011110111110,
    vbitclri_b_op      = 0b0111001100010000001,
    vbitseti_b_op      = 0b0111001100010100001,
    vbitrevi_b_op      = 0b0111001100011000001,
    vslli_b_op         = 0b0111001100101100001,
    vsrli_b_op         = 0b0111001100110000001,
    vsrai_b_op         = 0b0111001100110100001,
    xvrotri_b_op       = 0b0111011010100000001,
    xvinsgr2vr_w_op    = 0b0111011011101011110,
    xvpickve2gr_w_op   = 0b0111011011101111110,
    xvpickve2gr_wu_op  = 0b0111011011110011110,
    xvinsve0_w_op      = 0b0111011011111111110,
    xvpickve_w_op      = 0b0111011100000011110,
    xvbitclri_b_op     = 0b0111011100010000001,
    xvbitseti_b_op     = 0b0111011100010100001,
    xvbitrevi_b_op     = 0b0111011100011000001,
    xvslli_b_op        = 0b0111011100101100001,
    xvsrli_b_op        = 0b0111011100110000001,
    xvsrai_b_op        = 0b0111011100110100001,

    unknow_ops19       = 0b1111111111111111111
  };

  // 18-bit opcode, highest 18 bits: bits[31...14]
  enum ops18 {
    vrotri_h_op        = 0b011100101010000001,
    vinsgr2vr_b_op     = 0b011100101110101110,
    vpickve2gr_b_op    = 0b011100101110111110,
    vpickve2gr_bu_op   = 0b011100101111001110,
    vreplvei_b_op      = 0b011100101111011110,
    vbitclri_h_op      = 0b011100110001000001,
    vbitseti_h_op      = 0b011100110001010001,
    vbitrevi_h_op      = 0b011100110001100001,
    vslli_h_op         = 0b011100110010110001,
    vsrli_h_op         = 0b011100110011000001,
    vsrai_h_op         = 0b011100110011010001,
    vsrlni_b_h_op      = 0b011100110100000001,
    xvrotri_h_op       = 0b011101101010000001,
    xvbitclri_h_op     = 0b011101110001000001,
    xvbitseti_h_op     = 0b011101110001010001,
    xvbitrevi_h_op     = 0b011101110001100001,
    xvslli_h_op        = 0b011101110010110001,
    xvsrli_h_op        = 0b011101110011000001,
    xvsrai_h_op        = 0b011101110011010001,

    unknow_ops18       = 0b111111111111111111
  };

  // 17-bit opcode, highest 17 bits: bits[31...15]
  enum ops17 {
    asrtle_d_op        = 0b00000000000000010,
    asrtgt_d_op        = 0b00000000000000011,
    add_w_op           = 0b00000000000100000,
    add_d_op           = 0b00000000000100001,
    sub_w_op           = 0b00000000000100010,
    sub_d_op           = 0b00000000000100011,
    slt_op             = 0b00000000000100100,
    sltu_op            = 0b00000000000100101,
    maskeqz_op         = 0b00000000000100110,
    masknez_op         = 0b00000000000100111,
    nor_op             = 0b00000000000101000,
    and_op             = 0b00000000000101001,
    or_op              = 0b00000000000101010,
    xor_op             = 0b00000000000101011,
    orn_op             = 0b00000000000101100,
    andn_op            = 0b00000000000101101,
    sll_w_op           = 0b00000000000101110,
    srl_w_op           = 0b00000000000101111,
    sra_w_op           = 0b00000000000110000,
    sll_d_op           = 0b00000000000110001,
    srl_d_op           = 0b00000000000110010,
    sra_d_op           = 0b00000000000110011,
    rotr_w_op          = 0b00000000000110110,
    rotr_d_op          = 0b00000000000110111,
    mul_w_op           = 0b00000000000111000,
    mulh_w_op          = 0b00000000000111001,
    mulh_wu_op         = 0b00000000000111010,
    mul_d_op           = 0b00000000000111011,
    mulh_d_op          = 0b00000000000111100,
    mulh_du_op         = 0b00000000000111101,
    mulw_d_w_op        = 0b00000000000111110,
    mulw_d_wu_op       = 0b00000000000111111,
    div_w_op           = 0b00000000001000000,
    mod_w_op           = 0b00000000001000001,
    div_wu_op          = 0b00000000001000010,
    mod_wu_op          = 0b00000000001000011,
    div_d_op           = 0b00000000001000100,
    mod_d_op           = 0b00000000001000101,
    div_du_op          = 0b00000000001000110,
    mod_du_op          = 0b00000000001000111,
    crc_w_b_w_op       = 0b00000000001001000,
    crc_w_h_w_op       = 0b00000000001001001,
    crc_w_w_w_op       = 0b00000000001001010,
    crc_w_d_w_op       = 0b00000000001001011,
    crcc_w_b_w_op      = 0b00000000001001100,
    crcc_w_h_w_op      = 0b00000000001001101,
    crcc_w_w_w_op      = 0b00000000001001110,
    crcc_w_d_w_op      = 0b00000000001001111,
    break_op           = 0b00000000001010100,
    fadd_s_op          = 0b00000001000000001,
    fadd_d_op          = 0b00000001000000010,
    fsub_s_op          = 0b00000001000000101,
    fsub_d_op          = 0b00000001000000110,
    fmul_s_op          = 0b00000001000001001,
    fmul_d_op          = 0b00000001000001010,
    fdiv_s_op          = 0b00000001000001101,
    fdiv_d_op          = 0b00000001000001110,
    fmax_s_op          = 0b00000001000010001,
    fmax_d_op          = 0b00000001000010010,
    fmin_s_op          = 0b00000001000010101,
    fmin_d_op          = 0b00000001000010110,
    fmaxa_s_op         = 0b00000001000011001,
    fmaxa_d_op         = 0b00000001000011010,
    fmina_s_op         = 0b00000001000011101,
    fmina_d_op         = 0b00000001000011110,
    fscaleb_s_op       = 0b00000001000100001,
    fscaleb_d_op       = 0b00000001000100010,
    fcopysign_s_op     = 0b00000001000100101,
    fcopysign_d_op     = 0b00000001000100110,
    ldx_b_op           = 0b00111000000000000,
    ldx_h_op           = 0b00111000000001000,
    ldx_w_op           = 0b00111000000010000,
    ldx_d_op           = 0b00111000000011000,
    stx_b_op           = 0b00111000000100000,
    stx_h_op           = 0b00111000000101000,
    stx_w_op           = 0b00111000000110000,
    stx_d_op           = 0b00111000000111000,
    ldx_bu_op          = 0b00111000001000000,
    ldx_hu_op          = 0b00111000001001000,
    ldx_wu_op          = 0b00111000001010000,
    fldx_s_op          = 0b00111000001100000,
    fldx_d_op          = 0b00111000001101000,
    fstx_s_op          = 0b00111000001110000,
    fstx_d_op          = 0b00111000001111000,
    vldx_op            = 0b00111000010000000,
    vstx_op            = 0b00111000010001000,
    xvldx_op           = 0b00111000010010000,
    xvstx_op           = 0b00111000010011000,
    amswap_w_op        = 0b00111000011000000,
    amswap_d_op        = 0b00111000011000001,
    amadd_w_op         = 0b00111000011000010,
    amadd_d_op         = 0b00111000011000011,
    amand_w_op         = 0b00111000011000100,
    amand_d_op         = 0b00111000011000101,
    amor_w_op          = 0b00111000011000110,
    amor_d_op          = 0b00111000011000111,
    amxor_w_op         = 0b00111000011001000,
    amxor_d_op         = 0b00111000011001001,
    ammax_w_op         = 0b00111000011001010,
    ammax_d_op         = 0b00111000011001011,
    ammin_w_op         = 0b00111000011001100,
    ammin_d_op         = 0b00111000011001101,
    ammax_wu_op        = 0b00111000011001110,
    ammax_du_op        = 0b00111000011001111,
    ammin_wu_op        = 0b00111000011010000,
    ammin_du_op        = 0b00111000011010001,
    amswap_db_w_op     = 0b00111000011010010,
    amswap_db_d_op     = 0b00111000011010011,
    amadd_db_w_op      = 0b00111000011010100,
    amadd_db_d_op      = 0b00111000011010101,
    amand_db_w_op      = 0b00111000011010110,
    amand_db_d_op      = 0b00111000011010111,
    amor_db_w_op       = 0b00111000011011000,
    amor_db_d_op       = 0b00111000011011001,
    amxor_db_w_op      = 0b00111000011011010,
    amxor_db_d_op      = 0b00111000011011011,
    ammax_db_w_op      = 0b00111000011011100,
    ammax_db_d_op      = 0b00111000011011101,
    ammin_db_w_op      = 0b00111000011011110,
    ammin_db_d_op      = 0b00111000011011111,
    ammax_db_wu_op     = 0b00111000011100000,
    ammax_db_du_op     = 0b00111000011100001,
    ammin_db_wu_op     = 0b00111000011100010,
    ammin_db_du_op     = 0b00111000011100011,
    dbar_op            = 0b00111000011100100,
    ibar_op            = 0b00111000011100101,
    fldgt_s_op         = 0b00111000011101000,
    fldgt_d_op         = 0b00111000011101001,
    fldle_s_op         = 0b00111000011101010,
    fldle_d_op         = 0b00111000011101011,
    fstgt_s_op         = 0b00111000011101100,
    fstgt_d_op         = 0b00111000011101101,
    fstle_s_op         = 0b00111000011101110,
    fstle_d_op         = 0b00111000011101111,
    ldgt_b_op          = 0b00111000011110000,
    ldgt_h_op          = 0b00111000011110001,
    ldgt_w_op          = 0b00111000011110010,
    ldgt_d_op          = 0b00111000011110011,
    ldle_b_op          = 0b00111000011110100,
    ldle_h_op          = 0b00111000011110101,
    ldle_w_op          = 0b00111000011110110,
    ldle_d_op          = 0b00111000011110111,
    stgt_b_op          = 0b00111000011111000,
    stgt_h_op          = 0b00111000011111001,
    stgt_w_op          = 0b00111000011111010,
    stgt_d_op          = 0b00111000011111011,
    stle_b_op          = 0b00111000011111100,
    stle_h_op          = 0b00111000011111101,
    stle_w_op          = 0b00111000011111110,
    stle_d_op          = 0b00111000011111111,
    vseq_b_op          = 0b01110000000000000,
    vseq_h_op          = 0b01110000000000001,
    vseq_w_op          = 0b01110000000000010,
    vseq_d_op          = 0b01110000000000011,
    vsle_b_op          = 0b01110000000000100,
    vsle_h_op          = 0b01110000000000101,
    vsle_w_op          = 0b01110000000000110,
    vsle_d_op          = 0b01110000000000111,
    vsle_bu_op         = 0b01110000000001000,
    vsle_hu_op         = 0b01110000000001001,
    vsle_wu_op         = 0b01110000000001010,
    vsle_du_op         = 0b01110000000001011,
    vslt_b_op          = 0b01110000000001100,
    vslt_h_op          = 0b01110000000001101,
    vslt_w_op          = 0b01110000000001110,
    vslt_d_op          = 0b01110000000001111,
    vslt_bu_op         = 0b01110000000010000,
    vslt_hu_op         = 0b01110000000010001,
    vslt_wu_op         = 0b01110000000010010,
    vslt_du_op         = 0b01110000000010011,
    vadd_b_op          = 0b01110000000010100,
    vadd_h_op          = 0b01110000000010101,
    vadd_w_op          = 0b01110000000010110,
    vadd_d_op          = 0b01110000000010111,
    vsub_b_op          = 0b01110000000011000,
    vsub_h_op          = 0b01110000000011001,
    vsub_w_op          = 0b01110000000011010,
    vsub_d_op          = 0b01110000000011011,
    vabsd_b_op         = 0b01110000011000000,
    vabsd_h_op         = 0b01110000011000001,
    vabsd_w_op         = 0b01110000011000010,
    vabsd_d_op         = 0b01110000011000011,
    vmax_b_op          = 0b01110000011100000,
    vmax_h_op          = 0b01110000011100001,
    vmax_w_op          = 0b01110000011100010,
    vmax_d_op          = 0b01110000011100011,
    vmin_b_op          = 0b01110000011100100,
    vmin_h_op          = 0b01110000011100101,
    vmin_w_op          = 0b01110000011100110,
    vmin_d_op          = 0b01110000011100111,
    vmul_b_op          = 0b01110000100001000,
    vmul_h_op          = 0b01110000100001001,
    vmul_w_op          = 0b01110000100001010,
    vmul_d_op          = 0b01110000100001011,
    vmuh_b_op          = 0b01110000100001100,
    vmuh_h_op          = 0b01110000100001101,
    vmuh_w_op          = 0b01110000100001110,
    vmuh_d_op          = 0b01110000100001111,
    vmuh_bu_op         = 0b01110000100010000,
    vmuh_hu_op         = 0b01110000100010001,
    vmuh_wu_op         = 0b01110000100010010,
    vmuh_du_op         = 0b01110000100010011,
    vmulwev_h_b_op     = 0b01110000100100000,
    vmulwev_w_h_op     = 0b01110000100100001,
    vmulwev_d_w_op     = 0b01110000100100010,
    vmulwev_q_d_op     = 0b01110000100100011,
    vmulwod_h_b_op     = 0b01110000100100100,
    vmulwod_w_h_op     = 0b01110000100100101,
    vmulwod_d_w_op     = 0b01110000100100110,
    vmulwod_q_d_op     = 0b01110000100100111,
    vmadd_b_op         = 0b01110000101010000,
    vmadd_h_op         = 0b01110000101010001,
    vmadd_w_op         = 0b01110000101010010,
    vmadd_d_op         = 0b01110000101010011,
    vmsub_b_op         = 0b01110000101010100,
    vmsub_h_op         = 0b01110000101010101,
    vmsub_w_op         = 0b01110000101010110,
    vmsub_d_op         = 0b01110000101010111,
    vsll_b_op          = 0b01110000111010000,
    vsll_h_op          = 0b01110000111010001,
    vsll_w_op          = 0b01110000111010010,
    vsll_d_op          = 0b01110000111010011,
    vsrl_b_op          = 0b01110000111010100,
    vsrl_h_op          = 0b01110000111010101,
    vsrl_w_op          = 0b01110000111010110,
    vsrl_d_op          = 0b01110000111010111,
    vsra_b_op          = 0b01110000111011000,
    vsra_h_op          = 0b01110000111011001,
    vsra_w_op          = 0b01110000111011010,
    vsra_d_op          = 0b01110000111011011,
    vrotr_b_op         = 0b01110000111011100,
    vrotr_h_op         = 0b01110000111011101,
    vrotr_w_op         = 0b01110000111011110,
    vrotr_d_op         = 0b01110000111011111,
    vbitclr_b_op       = 0b01110001000011000,
    vbitclr_h_op       = 0b01110001000011001,
    vbitclr_w_op       = 0b01110001000011010,
    vbitclr_d_op       = 0b01110001000011011,
    vbitset_b_op       = 0b01110001000011100,
    vbitset_h_op       = 0b01110001000011101,
    vbitset_w_op       = 0b01110001000011110,
    vbitset_d_op       = 0b01110001000011111,
    vbitrev_b_op       = 0b01110001000100000,
    vbitrev_h_op       = 0b01110001000100001,
    vbitrev_w_op       = 0b01110001000100010,
    vbitrev_d_op       = 0b01110001000100011,
    vand_v_op          = 0b01110001001001100,
    vor_v_op           = 0b01110001001001101,
    vxor_v_op          = 0b01110001001001110,
    vnor_v_op          = 0b01110001001001111,
    vandn_v_op         = 0b01110001001010000,
    vorn_v_op          = 0b01110001001010001,
    vadd_q_op          = 0b01110001001011010,
    vsub_q_op          = 0b01110001001011011,
    vfadd_s_op         = 0b01110001001100001,
    vfadd_d_op         = 0b01110001001100010,
    vfsub_s_op         = 0b01110001001100101,
    vfsub_d_op         = 0b01110001001100110,
    vfmul_s_op         = 0b01110001001110001,
    vfmul_d_op         = 0b01110001001110010,
    vfdiv_s_op         = 0b01110001001110101,
    vfdiv_d_op         = 0b01110001001110110,
    vfmax_s_op         = 0b01110001001111001,
    vfmax_d_op         = 0b01110001001111010,
    vfmin_s_op         = 0b01110001001111101,
    vfmin_d_op         = 0b01110001001111110,
    vfcvt_h_s_op       = 0b01110001010001100,
    vfcvt_s_d_op       = 0b01110001010001101,
    vffint_s_l_op      = 0b01110001010010000,
    vftint_w_d_op      = 0b01110001010010011,
    vftintrm_w_d_op    = 0b01110001010010100,
    vftintrp_w_d_op    = 0b01110001010010101,
    vftintrz_w_d_op    = 0b01110001010010110,
    vftintrne_w_d_op   = 0b01110001010010111,
    vshuf_h_op         = 0b01110001011110101,
    vshuf_w_op         = 0b01110001011110110,
    vshuf_d_op         = 0b01110001011110111,
    vslti_bu_op        = 0b01110010100010000,
    vslti_hu_op        = 0b01110010100010001,
    vslti_wu_op        = 0b01110010100010010,
    vslti_du_op        = 0b01110010100010011,
    vaddi_bu_op        = 0b01110010100010100,
    vaddi_hu_op        = 0b01110010100010101,
    vaddi_wu_op        = 0b01110010100010110,
    vaddi_du_op        = 0b01110010100010111,
    vsubi_bu_op        = 0b01110010100011000,
    vsubi_hu_op        = 0b01110010100011001,
    vsubi_wu_op        = 0b01110010100011010,
    vsubi_du_op        = 0b01110010100011011,
    vrotri_w_op        = 0b01110010101000001,
    vbitclri_w_op      = 0b01110011000100001,
    vbitseti_w_op      = 0b01110011000101001,
    vbitrevi_w_op      = 0b01110011000110001,
    vslli_w_op         = 0b01110011001011001,
    vsrli_w_op         = 0b01110011001100001,
    vsrai_w_op         = 0b01110011001101001,
    vsrlni_h_w_op      = 0b01110011010000001,
    xvseq_b_op         = 0b01110100000000000,
    xvseq_h_op         = 0b01110100000000001,
    xvseq_w_op         = 0b01110100000000010,
    xvseq_d_op         = 0b01110100000000011,
    xvsle_b_op         = 0b01110100000000100,
    xvsle_h_op         = 0b01110100000000101,
    xvsle_w_op         = 0b01110100000000110,
    xvsle_d_op         = 0b01110100000000111,
    xvsle_bu_op        = 0b01110100000001000,
    xvsle_hu_op        = 0b01110100000001001,
    xvsle_wu_op        = 0b01110100000001010,
    xvsle_du_op        = 0b01110100000001011,
    xvslt_b_op         = 0b01110100000001100,
    xvslt_h_op         = 0b01110100000001101,
    xvslt_w_op         = 0b01110100000001110,
    xvslt_d_op         = 0b01110100000001111,
    xvslt_bu_op        = 0b01110100000010000,
    xvslt_hu_op        = 0b01110100000010001,
    xvslt_wu_op        = 0b01110100000010010,
    xvslt_du_op        = 0b01110100000010011,
    xvadd_b_op         = 0b01110100000010100,
    xvadd_h_op         = 0b01110100000010101,
    xvadd_w_op         = 0b01110100000010110,
    xvadd_d_op         = 0b01110100000010111,
    xvsub_b_op         = 0b01110100000011000,
    xvsub_h_op         = 0b01110100000011001,
    xvsub_w_op         = 0b01110100000011010,
    xvsub_d_op         = 0b01110100000011011,
    xvabsd_b_op        = 0b01110100011000000,
    xvabsd_h_op        = 0b01110100011000001,
    xvabsd_w_op        = 0b01110100011000010,
    xvabsd_d_op        = 0b01110100011000011,
    xvmax_b_op         = 0b01110100011100000,
    xvmax_h_op         = 0b01110100011100001,
    xvmax_w_op         = 0b01110100011100010,
    xvmax_d_op         = 0b01110100011100011,
    xvmin_b_op         = 0b01110100011100100,
    xvmin_h_op         = 0b01110100011100101,
    xvmin_w_op         = 0b01110100011100110,
    xvmin_d_op         = 0b01110100011100111,
    xvmul_b_op         = 0b01110100100001000,
    xvmul_h_op         = 0b01110100100001001,
    xvmul_w_op         = 0b01110100100001010,
    xvmul_d_op         = 0b01110100100001011,
    xvmuh_b_op         = 0b01110100100001100,
    xvmuh_h_op         = 0b01110100100001101,
    xvmuh_w_op         = 0b01110100100001110,
    xvmuh_d_op         = 0b01110100100001111,
    xvmuh_bu_op        = 0b01110100100010000,
    xvmuh_hu_op        = 0b01110100100010001,
    xvmuh_wu_op        = 0b01110100100010010,
    xvmuh_du_op        = 0b01110100100010011,
    xvmulwev_h_b_op    = 0b01110100100100000,
    xvmulwev_w_h_op    = 0b01110100100100001,
    xvmulwev_d_w_op    = 0b01110100100100010,
    xvmulwev_q_d_op    = 0b01110100100100011,
    xvmulwod_h_b_op    = 0b01110100100100100,
    xvmulwod_w_h_op    = 0b01110100100100101,
    xvmulwod_d_w_op    = 0b01110100100100110,
    xvmulwod_q_d_op    = 0b01110100100100111,
    xvmadd_b_op        = 0b01110100101010000,
    xvmadd_h_op        = 0b01110100101010001,
    xvmadd_w_op        = 0b01110100101010010,
    xvmadd_d_op        = 0b01110100101010011,
    xvmsub_b_op        = 0b01110100101010100,
    xvmsub_h_op        = 0b01110100101010101,
    xvmsub_w_op        = 0b01110100101010110,
    xvmsub_d_op        = 0b01110100101010111,
    xvsll_b_op         = 0b01110100111010000,
    xvsll_h_op         = 0b01110100111010001,
    xvsll_w_op         = 0b01110100111010010,
    xvsll_d_op         = 0b01110100111010011,
    xvsrl_b_op         = 0b01110100111010100,
    xvsrl_h_op         = 0b01110100111010101,
    xvsrl_w_op         = 0b01110100111010110,
    xvsrl_d_op         = 0b01110100111010111,
    xvsra_b_op         = 0b01110100111011000,
    xvsra_h_op         = 0b01110100111011001,
    xvsra_w_op         = 0b01110100111011010,
    xvsra_d_op         = 0b01110100111011011,
    xvrotr_b_op        = 0b01110100111011100,
    xvrotr_h_op        = 0b01110100111011101,
    xvrotr_w_op        = 0b01110100111011110,
    xvrotr_d_op        = 0b01110100111011111,
    xvbitclr_b_op      = 0b01110101000011000,
    xvbitclr_h_op      = 0b01110101000011001,
    xvbitclr_w_op      = 0b01110101000011010,
    xvbitclr_d_op      = 0b01110101000011011,
    xvbitset_b_op      = 0b01110101000011100,
    xvbitset_h_op      = 0b01110101000011101,
    xvbitset_w_op      = 0b01110101000011110,
    xvbitset_d_op      = 0b01110101000011111,
    xvbitrev_b_op      = 0b01110101000100000,
    xvbitrev_h_op      = 0b01110101000100001,
    xvbitrev_w_op      = 0b01110101000100010,
    xvbitrev_d_op      = 0b01110101000100011,
    xvand_v_op         = 0b01110101001001100,
    xvor_v_op          = 0b01110101001001101,
    xvxor_v_op         = 0b01110101001001110,
    xvnor_v_op         = 0b01110101001001111,
    xvandn_v_op        = 0b01110101001010000,
    xvorn_v_op         = 0b01110101001010001,
    xvadd_q_op         = 0b01110101001011010,
    xvsub_q_op         = 0b01110101001011011,
    xvfadd_s_op        = 0b01110101001100001,
    xvfadd_d_op        = 0b01110101001100010,
    xvfsub_s_op        = 0b01110101001100101,
    xvfsub_d_op        = 0b01110101001100110,
    xvfmul_s_op        = 0b01110101001110001,
    xvfmul_d_op        = 0b01110101001110010,
    xvfdiv_s_op        = 0b01110101001110101,
    xvfdiv_d_op        = 0b01110101001110110,
    xvfmax_s_op        = 0b01110101001111001,
    xvfmax_d_op        = 0b01110101001111010,
    xvfmin_s_op        = 0b01110101001111101,
    xvfmin_d_op        = 0b01110101001111110,
    xvfcvt_h_s_op      = 0b01110101010001100,
    xvfcvt_s_d_op      = 0b01110101010001101,
    xvffint_s_l_op     = 0b01110101010010000,
    xvftint_w_d_op     = 0b01110101010010011,
    xvftintrm_w_d_op   = 0b01110101010010100,
    xvftintrp_w_d_op   = 0b01110101010010101,
    xvftintrz_w_d_op   = 0b01110101010010110,
    xvftintrne_w_d_op  = 0b01110101010010111,
    xvshuf_h_op        = 0b01110101011110101,
    xvshuf_w_op        = 0b01110101011110110,
    xvshuf_d_op        = 0b01110101011110111,
    xvperm_w_op        = 0b01110101011111010,
    xvslti_bu_op       = 0b01110110100010000,
    xvslti_hu_op       = 0b01110110100010001,
    xvslti_wu_op       = 0b01110110100010010,
    xvslti_du_op       = 0b01110110100010011,
    xvaddi_bu_op       = 0b01110110100010100,
    xvaddi_hu_op       = 0b01110110100010101,
    xvaddi_wu_op       = 0b01110110100010110,
    xvaddi_du_op       = 0b01110110100010111,
    xvsubi_bu_op       = 0b01110110100011000,
    xvsubi_hu_op       = 0b01110110100011001,
    xvsubi_wu_op       = 0b01110110100011010,
    xvsubi_du_op       = 0b01110110100011011,
    xvrotri_w_op       = 0b01110110101000001,
    xvbitclri_w_op     = 0b01110111000100001,
    xvbitseti_w_op     = 0b01110111000101001,
    xvbitrevi_w_op     = 0b01110111000110001,
    xvslli_w_op        = 0b01110111001011001,
    xvsrli_w_op        = 0b01110111001100001,
    xvsrai_w_op        = 0b01110111001101001,

    unknow_ops17       = 0b11111111111111111
  };

  // 16-bit opcode, highest 16 bits: bits[31...16]
  enum ops16 {
    vrotri_d_op        = 0b0111001010100001,
    vbitclri_d_op      = 0b0111001100010001,
    vbitseti_d_op      = 0b0111001100010101,
    vbitrevi_d_op      = 0b0111001100011001,
    vslli_d_op         = 0b0111001100101101,
    vsrli_d_op         = 0b0111001100110001,
    vsrai_d_op         = 0b0111001100110101,
    vsrlni_w_d_op      = 0b0111001101000001,
    xvrotri_d_op       = 0b0111011010100001,
    xvbitclri_d_op     = 0b0111011100010001,
    xvbitseti_d_op     = 0b0111011100010101,
    xvbitrevi_d_op     = 0b0111011100011001,
    xvslli_d_op        = 0b0111011100101101,
    xvsrli_d_op        = 0b0111011100110001,
    xvsrai_d_op        = 0b0111011100110101,

    unknow_ops16       = 0b1111111111111111
  };

  // 15-bit opcode, highest 15 bits: bits[31...17]
  enum ops15 {
    vsrlni_d_q_op      = 0b011100110100001,

    unknow_ops15       = 0b111111111111111
  };

  // 14-bit opcode, highest 14 bits: bits[31...18]
  enum ops14 {
    alsl_w_op          = 0b00000000000001,
    bytepick_w_op      = 0b00000000000010,
    bytepick_d_op      = 0b00000000000011,
    alsl_d_op          = 0b00000000001011,
    slli_op            = 0b00000000010000,
    srli_op            = 0b00000000010001,
    srai_op            = 0b00000000010010,
    rotri_op           = 0b00000000010011,
    lddir_op           = 0b00000110010000,
    ldpte_op           = 0b00000110010001,
    vshuf4i_b_op       = 0b01110011100100,
    vshuf4i_h_op       = 0b01110011100101,
    vshuf4i_w_op       = 0b01110011100110,
    vshuf4i_d_op       = 0b01110011100111,
    vandi_b_op         = 0b01110011110100,
    vori_b_op          = 0b01110011110101,
    vxori_b_op         = 0b01110011110110,
    vnori_b_op         = 0b01110011110111,
    vldi_op            = 0b01110011111000,
    vpermi_w_op        = 0b01110011111001,
    xvshuf4i_b_op      = 0b01110111100100,
    xvshuf4i_h_op      = 0b01110111100101,
    xvshuf4i_w_op      = 0b01110111100110,
    xvshuf4i_d_op      = 0b01110111100111,
    xvandi_b_op        = 0b01110111110100,
    xvori_b_op         = 0b01110111110101,
    xvxori_b_op        = 0b01110111110110,
    xvnori_b_op        = 0b01110111110111,
    xvldi_op           = 0b01110111111000,
    xvpermi_w_op       = 0b01110111111001,
    xvpermi_d_op       = 0b01110111111010,
    xvpermi_q_op       = 0b01110111111011,

    unknow_ops14       = 0b11111111111111
  };

  // 12-bit opcode, highest 12 bits: bits[31...20]
  enum ops12 {
    fmadd_s_op         = 0b000010000001,
    fmadd_d_op         = 0b000010000010,
    fmsub_s_op         = 0b000010000101,
    fmsub_d_op         = 0b000010000110,
    fnmadd_s_op        = 0b000010001001,
    fnmadd_d_op        = 0b000010001010,
    fnmsub_s_op        = 0b000010001101,
    fnmsub_d_op        = 0b000010001110,
    vfmadd_s_op        = 0b000010010001,
    vfmadd_d_op        = 0b000010010010,
    vfmsub_s_op        = 0b000010010101,
    vfmsub_d_op        = 0b000010010110,
    vfnmadd_s_op       = 0b000010011001,
    vfnmadd_d_op       = 0b000010011010,
    vfnmsub_s_op       = 0b000010011101,
    vfnmsub_d_op       = 0b000010011110,
    xvfmadd_s_op       = 0b000010100001,
    xvfmadd_d_op       = 0b000010100010,
    xvfmsub_s_op       = 0b000010100101,
    xvfmsub_d_op       = 0b000010100110,
    xvfnmadd_s_op      = 0b000010101001,
    xvfnmadd_d_op      = 0b000010101010,
    xvfnmsub_s_op      = 0b000010101101,
    xvfnmsub_d_op      = 0b000010101110,
    fcmp_cond_s_op     = 0b000011000001,
    fcmp_cond_d_op     = 0b000011000010,
    vfcmp_cond_s_op    = 0b000011000101,
    vfcmp_cond_d_op    = 0b000011000110,
    xvfcmp_cond_s_op   = 0b000011001001,
    xvfcmp_cond_d_op   = 0b000011001010,
    fsel_op            = 0b000011010000,
    vbitsel_v_op       = 0b000011010001,
    xvbitsel_v_op      = 0b000011010010,
    vshuf_b_op         = 0b000011010101,
    xvshuf_b_op        = 0b000011010110,

    unknow_ops12       = 0b111111111111
  };

  // 10-bit opcode, highest 10 bits: bits[31...22]
  enum ops10 {
    bstr_w_op          = 0b0000000001,
    bstrins_d_op       = 0b0000000010,
    bstrpick_d_op      = 0b0000000011,
    slti_op            = 0b0000001000,
    sltui_op           = 0b0000001001,
    addi_w_op          = 0b0000001010,
    addi_d_op          = 0b0000001011,
    lu52i_d_op         = 0b0000001100,
    andi_op            = 0b0000001101,
    ori_op             = 0b0000001110,
    xori_op            = 0b0000001111,
    ld_b_op            = 0b0010100000,
    ld_h_op            = 0b0010100001,
    ld_w_op            = 0b0010100010,
    ld_d_op            = 0b0010100011,
    st_b_op            = 0b0010100100,
    st_h_op            = 0b0010100101,
    st_w_op            = 0b0010100110,
    st_d_op            = 0b0010100111,
    ld_bu_op           = 0b0010101000,
    ld_hu_op           = 0b0010101001,
    ld_wu_op           = 0b0010101010,
    preld_op           = 0b0010101011,
    fld_s_op           = 0b0010101100,
    fst_s_op           = 0b0010101101,
    fld_d_op           = 0b0010101110,
    fst_d_op           = 0b0010101111,
    vld_op             = 0b0010110000,
    vst_op             = 0b0010110001,
    xvld_op            = 0b0010110010,
    xvst_op            = 0b0010110011,
    ldl_w_op           = 0b0010111000,
    ldr_w_op           = 0b0010111001,

    unknow_ops10       = 0b1111111111
  };

  // 8-bit opcode, highest 8 bits: bits[31...22]
  enum ops8 {
    ll_w_op            = 0b00100000,
    sc_w_op            = 0b00100001,
    ll_d_op            = 0b00100010,
    sc_d_op            = 0b00100011,
    ldptr_w_op         = 0b00100100,
    stptr_w_op         = 0b00100101,
    ldptr_d_op         = 0b00100110,
    stptr_d_op         = 0b00100111,

    unknow_ops8        = 0b11111111
  };

  // 7-bit opcode, highest 7 bits: bits[31...25]
  enum ops7 {
    lu12i_w_op         = 0b0001010,
    lu32i_d_op         = 0b0001011,
    pcaddi_op          = 0b0001100,
    pcalau12i_op       = 0b0001101,
    pcaddu12i_op       = 0b0001110,
    pcaddu18i_op       = 0b0001111,

    unknow_ops7        = 0b1111111
  };

  // 6-bit opcode, highest 6 bits: bits[31...25]
  enum ops6 {
    addu16i_d_op       = 0b000100,
    beqz_op            = 0b010000,
    bnez_op            = 0b010001,
    bccondz_op         = 0b010010,
    jirl_op            = 0b010011,
    b_op               = 0b010100,
    bl_op              = 0b010101,
    beq_op             = 0b010110,
    bne_op             = 0b010111,
    blt_op             = 0b011000,
    bge_op             = 0b011001,
    bltu_op            = 0b011010,
    bgeu_op            = 0b011011,

    unknow_ops6        = 0b111111
  };

  enum fcmp_cond {
    fcmp_caf           = 0x00,
    fcmp_cun           = 0x08,
    fcmp_ceq           = 0x04,
    fcmp_cueq          = 0x0c,
    fcmp_clt           = 0x02,
    fcmp_cult          = 0x0a,
    fcmp_cle           = 0x06,
    fcmp_cule          = 0x0e,
    fcmp_cne           = 0x10,
    fcmp_cor           = 0x14,
    fcmp_cune          = 0x18,
    fcmp_saf           = 0x01,
    fcmp_sun           = 0x09,
    fcmp_seq           = 0x05,
    fcmp_sueq          = 0x0d,
    fcmp_slt           = 0x03,
    fcmp_sult          = 0x0b,
    fcmp_sle           = 0x07,
    fcmp_sule          = 0x0f,
    fcmp_sne           = 0x11,
    fcmp_sor           = 0x15,
    fcmp_sune          = 0x19
  };

  enum Condition {
    zero         ,
    notZero      ,
    equal        ,
    notEqual     ,
    less         ,
    lessEqual    ,
    greater      ,
    greaterEqual ,
    below        ,
    belowEqual   ,
    above        ,
    aboveEqual
  };

  static const int LogInstructionSize = 2;
  static const int InstructionSize    = 1 << LogInstructionSize;

  enum WhichOperand {
    // input to locate_operand, and format code for relocations
    imm_operand  = 0,            // embedded 32-bit|64-bit immediate operand
    disp32_operand = 1,          // embedded 32-bit displacement or address
    call32_operand = 2,          // embedded 32-bit self-relative displacement
    narrow_oop_operand = 3,      // embedded 32-bit immediate narrow oop
    _WhichOperand_limit = 4
  };

  static int low  (int x, int l) { return bitfield(x, 0, l); }
  static int low16(int x)        { return low(x, 16); }
  static int low26(int x)        { return low(x, 26); }

  static int high  (int x, int l) { return bitfield(x, 32-l, l); }
  static int high16(int x)        { return high(x, 16); }
  static int high6 (int x)        { return high(x, 6); }


 protected:
  // help methods for instruction ejection

  // 2R-type
  //  31                          10 9      5 4     0
  // |   opcode                     |   rj   |  rd   |
  static inline int insn_RR   (int op, int rj, int rd) { return (op<<10) | (rj<<5) | rd; }

  // 3R-type
  //  31                    15 14 10 9      5 4     0
  // |   opcode               |  rk |   rj   |  rd   |
  static inline int insn_RRR  (int op, int rk, int rj, int rd)  { return (op<<15) | (rk<<10) | (rj<<5) | rd; }

  // 4R-type
  //  31             20 19  15 14  10 9     5 4     0
  // |   opcode        |  ra  |  rk |    rj  |  rd   |
  static inline int insn_RRRR (int op, int ra,  int rk, int rj, int rd)  { return (op<<20) | (ra << 15) | (rk<<10) | (rj<<5) | rd; }

  // 2RI1-type
  //  31                11     10    9      5 4     0
  // |   opcode           |    I1   |    vj  |  rd   |
  static inline int insn_I1RR (int op, int ui1, int vj, int rd)  { assert(is_uimm(ui1, 1), "not a unsigned 1-bit int"); return (op<<11) | (low(ui1, 1)<<10) | (vj<<5) | rd; }

  // 2RI2-type
  //  31                12 11     10 9      5 4     0
  // |   opcode           |    I2   |    vj  |  rd   |
  static inline int insn_I2RR (int op, int ui2, int vj, int rd)  { assert(is_uimm(ui2, 2), "not a unsigned 2-bit int"); return (op<<12) | (low(ui2, 2)<<10) | (vj<<5) | rd; }

  // 2RI3-type
  //  31                13 12     10 9      5 4     0
  // |   opcode           |    I3   |    vj  |  vd   |
  static inline int insn_I3RR (int op, int ui3, int vj, int vd)  { assert(is_uimm(ui3, 3), "not a unsigned 3-bit int"); return (op<<13) | (low(ui3, 3)<<10) | (vj<<5) | vd; }

  // 2RI4-type
  //  31                14 13     10 9      5 4     0
  // |   opcode           |    I4   |    vj  |  vd   |
  static inline int insn_I4RR (int op, int ui4, int vj, int vd)  { assert(is_uimm(ui4, 4), "not a unsigned 4-bit int"); return (op<<14) | (low(ui4, 4)<<10) | (vj<<5) | vd; }

  // 2RI5-type
  //  31                15 14     10 9      5 4     0
  // |   opcode           |    I5   |    vj  |  vd   |
  static inline int insn_I5RR (int op, int ui5, int vj, int vd)  { assert(is_uimm(ui5, 5), "not a unsigned 5-bit int"); return (op<<15) | (low(ui5, 5)<<10) | (vj<<5) | vd; }

  // 2RI6-type
  //  31                16 15     10 9      5 4     0
  // |   opcode           |    I6   |    vj  |  vd   |
  static inline int insn_I6RR (int op, int ui6, int vj, int vd)  { assert(is_uimm(ui6, 6), "not a unsigned 6-bit int"); return (op<<16) | (low(ui6, 6)<<10) | (vj<<5) | vd; }

  // 2RI7-type
  //  31                17 16     10 9      5 4     0
  // |   opcode           |    I7   |    vj  |  vd   |
  static inline int insn_I7RR (int op, int ui7, int vj, int vd)  { assert(is_uimm(ui7, 7), "not a unsigned 7-bit int"); return (op<<17) | (low(ui7, 6)<<10) | (vj<<5) | vd; }

  // 2RI8-type
  //  31                18 17     10 9      5 4     0
  // |   opcode           |    I8   |    rj  |  rd   |
  static inline int insn_I8RR (int op, int imm8, int rj, int rd)  { /*assert(is_simm(imm8, 8), "not a signed 8-bit int");*/ return (op<<18) | (low(imm8, 8)<<10) | (rj<<5) | rd; }

  // 2RI12-type
  //  31           22 21          10 9      5 4     0
  // |   opcode      |     I12      |    rj  |  rd   |
  static inline int insn_I12RR(int op, int imm12, int rj, int rd) { /* assert(is_simm(imm12, 12), "not a signed 12-bit int");*/  return (op<<22) | (low(imm12, 12)<<10) | (rj<<5) | rd; }


  // 2RI14-type
  //  31         24 23            10 9      5 4     0
  // |   opcode    |      I14       |    rj  |  rd   |
  static inline int insn_I14RR(int op, int imm14, int rj, int rd) { assert(is_simm(imm14, 14), "not a signed 14-bit int"); return (op<<24) | (low(imm14, 14)<<10) | (rj<<5) | rd; }

  // 2RI16-type
  //  31       26 25              10 9      5 4     0
  // |   opcode  |       I16        |    rj  |  rd   |
  static inline int insn_I16RR(int op, int imm16, int rj, int rd) { assert(is_simm16(imm16), "not a signed 16-bit int"); return (op<<26) | (low16(imm16)<<10) | (rj<<5) | rd; }

  // 1RI13-type (?)
  //  31        18 17                      5 4     0
  // |   opcode   |               I13        |  vd   |
  static inline int insn_I13R (int op, int imm13, int vd) { assert(is_simm(imm13, 13), "not a signed 13-bit int"); return (op<<18) | (low(imm13, 13)<<5) | vd; }

  // 1RI20-type (?)
  //  31        25 24                      5 4     0
  // |   opcode   |               I20        |  rd   |
  static inline int insn_I20R (int op, int imm20, int rd) { assert(is_simm(imm20, 20), "not a signed 20-bit int"); return (op<<25) | (low(imm20, 20)<<5) | rd; }

  // 1RI21-type
  //  31       26 25              10 9     5 4        0
  // |   opcode  |     I21[15:0]    |   rj   |I21[20:16]|
  static inline int insn_IRI(int op, int imm21, int rj) { assert(is_simm(imm21, 21), "not a signed 21-bit int"); return (op << 26) | (low16(imm21) << 10) | (rj << 5) | low(imm21 >> 16, 5); }

  // I26-type
  //  31       26 25              10 9               0
  // |   opcode  |     I26[15:0]    |    I26[25:16]   |
  static inline int insn_I26(int op, int imm26) { assert(is_simm(imm26, 26), "not a signed 26-bit int"); return (op << 26) | (low16(imm26) << 10) | low(imm26 >> 16, 10); }

  // imm15
  //  31                    15 14                    0
  // |         opcode         |          I15          |
  static inline int insn_I15  (int op, int imm15) { assert(is_uimm(imm15, 15), "not a unsigned 15-bit int"); return (op<<15) | low(imm15, 15); }


  // get the offset field of beq, bne, blt[u], bge[u] instruction
  int offset16(address entry) {
    assert(is_simm16((entry - pc()) / 4), "change this code");
    if (!is_simm16((entry - pc()) / 4)) {
      tty->print_cr("!!! is_simm16: %lx", (entry - pc()) / 4);
    }
    return (entry - pc()) / 4;
  }

  // get the offset field of beqz, bnez instruction
  int offset21(address entry) {
    assert(is_simm((int)(entry - pc()) / 4, 21), "change this code");
    if (!is_simm((int)(entry - pc()) / 4, 21)) {
      tty->print_cr("!!! is_simm21: %lx", (entry - pc()) / 4);
    }
    return (entry - pc()) / 4;
  }

  // get the offset field of b instruction
  int offset26(address entry) {
    assert(is_simm((int)(entry - pc()) / 4, 26), "change this code");
    if (!is_simm((int)(entry - pc()) / 4, 26)) {
      tty->print_cr("!!! is_simm26: %lx", (entry - pc()) / 4);
    }
    return (entry - pc()) / 4;
  }

public:
  using AbstractAssembler::offset;

  //sign expand with the sign bit is h
  static int expand(int x, int h) { return -(x & (1<<h)) | x;  }

  // If x is a mask, return the number of one-bit in x.
  // else return -1.
  static int is_int_mask(int x);

  // If x is a mask, return the number of one-bit in x.
  // else return -1.
  static int is_jlong_mask(jlong x);

  static int split_low16(int x) {
    return (x & 0xffff);
  }

  // Convert 16-bit x to a sign-extended 16-bit integer
  static int simm16(int x) {
    assert(x == (x & 0xFFFF), "must be 16-bit only");
    return (x << 16) >> 16;
  }

  static int split_high16(int x) {
    return ( (x >> 16) + ((x & 0x8000) != 0) ) & 0xffff;
  }

  static int split_low20(int x) {
    return (x & 0xfffff);
  }

  // Convert 20-bit x to a sign-extended 20-bit integer
  static int simm20(int x) {
    assert(x == (x & 0xFFFFF), "must be 20-bit only");
    return (x << 12) >> 12;
  }

  static int split_low12(int x) {
    return (x & 0xfff);
  }

  static inline void split_simm38(jlong si38, jint& si18, jint& si20) {
    si18 = ((jint)(si38 & 0x3ffff) << 14) >> 14;
    si38 += (si38 & 0x20000) << 1;
    si20 = si38 >> 18;
  }

  // Convert 12-bit x to a sign-extended 12-bit integer
  static int simm12(int x) {
    assert(x == (x & 0xFFF), "must be 12-bit only");
    return (x << 20) >> 20;
  }

  // Convert 26-bit x to a sign-extended 26-bit integer
  static int simm26(int x) {
    assert(x == (x & 0x3FFFFFF), "must be 26-bit only");
    return (x << 6) >> 6;
  }

  static intptr_t merge(intptr_t x0, intptr_t x12) {
    //lu12i, ori
    return (((x12 << 12) | x0) << 32) >> 32;
  }

  static intptr_t merge(intptr_t x0, intptr_t x12, intptr_t x32) {
    //lu32i, lu12i, ori
    return (((x32 << 32) | (x12 << 12) | x0) << 12) >> 12;
  }

  static intptr_t merge(intptr_t x0, intptr_t x12, intptr_t x32, intptr_t x52) {
    //lu52i, lu32i, lu12i, ori
    return (x52 << 52) | (x32 << 32) | (x12 << 12) | x0;
  }

  // Test if x is within signed immediate range for nbits.
  static bool is_simm  (int x, unsigned int nbits) {
    assert(0 < nbits && nbits < 32, "out of bounds");
    const int   min      = -( ((int)1) << nbits-1 );
    const int   maxplus1 =  ( ((int)1) << nbits-1 );
    return min <= x && x < maxplus1;
  }

  static bool is_simm(jlong x, unsigned int nbits) {
    assert(0 < nbits && nbits < 64, "out of bounds");
    const jlong min      = -( ((jlong)1) << nbits-1 );
    const jlong maxplus1 =  ( ((jlong)1) << nbits-1 );
    return min <= x && x < maxplus1;
  }

  static bool is_simm16(int x)            { return is_simm(x, 16); }
  static bool is_simm16(long x)           { return is_simm((jlong)x, (unsigned int)16); }

  // Test if x is within unsigned immediate range for nbits
  static bool is_uimm(int x, unsigned int nbits) {
    assert(0 < nbits && nbits < 32, "out of bounds");
    const int   maxplus1 = ( ((int)1) << nbits );
    return 0 <= x && x < maxplus1;
  }

  static bool is_uimm(jlong x, unsigned int nbits) {
    assert(0 < nbits && nbits < 64, "out of bounds");
    const jlong maxplus1 =  ( ((jlong)1) << nbits );
    return 0 <= x && x < maxplus1;
  }

public:

  void flush() {
    AbstractAssembler::flush();
  }

  inline void emit_data(int x) { emit_int32(x); }
  inline void emit_data(int x, relocInfo::relocType rtype) {
    relocate(rtype);
    emit_int32(x);
  }

  inline void emit_data(int x, RelocationHolder const& rspec) {
    relocate(rspec);
    emit_int32(x);
  }

  // Generic instructions
  // Does 32bit or 64bit as needed for the platform. In some sense these
  // belong in macro assembler but there is no need for both varieties to exist

  void clo_w  (Register rd, Register rj) { emit_int32(insn_RR(clo_w_op, (int)rj->encoding(), (int)rd->encoding())); }
  void clz_w  (Register rd, Register rj) { emit_int32(insn_RR(clz_w_op, (int)rj->encoding(), (int)rd->encoding())); }
  void cto_w  (Register rd, Register rj) { emit_int32(insn_RR(cto_w_op, (int)rj->encoding(), (int)rd->encoding())); }
  void ctz_w  (Register rd, Register rj) { emit_int32(insn_RR(ctz_w_op, (int)rj->encoding(), (int)rd->encoding())); }
  void clo_d  (Register rd, Register rj) { emit_int32(insn_RR(clo_d_op, (int)rj->encoding(), (int)rd->encoding())); }
  void clz_d  (Register rd, Register rj) { emit_int32(insn_RR(clz_d_op, (int)rj->encoding(), (int)rd->encoding())); }
  void cto_d  (Register rd, Register rj) { emit_int32(insn_RR(cto_d_op, (int)rj->encoding(), (int)rd->encoding())); }
  void ctz_d  (Register rd, Register rj) { emit_int32(insn_RR(ctz_d_op, (int)rj->encoding(), (int)rd->encoding())); }

  void revb_2h(Register rd, Register rj) { emit_int32(insn_RR(revb_2h_op, (int)rj->encoding(), (int)rd->encoding())); }
  void revb_4h(Register rd, Register rj) { emit_int32(insn_RR(revb_4h_op, (int)rj->encoding(), (int)rd->encoding())); }
  void revb_2w(Register rd, Register rj) { emit_int32(insn_RR(revb_2w_op, (int)rj->encoding(), (int)rd->encoding())); }
  void revb_d (Register rd, Register rj) { emit_int32(insn_RR( revb_d_op, (int)rj->encoding(), (int)rd->encoding())); }
  void revh_2w(Register rd, Register rj) { emit_int32(insn_RR(revh_2w_op, (int)rj->encoding(), (int)rd->encoding())); }
  void revh_d (Register rd, Register rj) { emit_int32(insn_RR( revh_d_op, (int)rj->encoding(), (int)rd->encoding())); }

  void bitrev_4b(Register rd, Register rj) { emit_int32(insn_RR(bitrev_4b_op, (int)rj->encoding(), (int)rd->encoding())); }
  void bitrev_8b(Register rd, Register rj) { emit_int32(insn_RR(bitrev_8b_op, (int)rj->encoding(), (int)rd->encoding())); }
  void bitrev_w (Register rd, Register rj) { emit_int32(insn_RR(bitrev_w_op,  (int)rj->encoding(), (int)rd->encoding())); }
  void bitrev_d (Register rd, Register rj) { emit_int32(insn_RR(bitrev_d_op,  (int)rj->encoding(), (int)rd->encoding())); }

  void ext_w_h(Register rd, Register rj) { emit_int32(insn_RR(ext_w_h_op, (int)rj->encoding(), (int)rd->encoding())); }
  void ext_w_b(Register rd, Register rj) { emit_int32(insn_RR(ext_w_b_op, (int)rj->encoding(), (int)rd->encoding())); }

  void rdtimel_w(Register rd, Register rj) { emit_int32(insn_RR(rdtimel_w_op, (int)rj->encoding(), (int)rd->encoding())); }
  void rdtimeh_w(Register rd, Register rj) { emit_int32(insn_RR(rdtimeh_w_op, (int)rj->encoding(), (int)rd->encoding())); }
  void rdtime_d(Register rd, Register rj)  { emit_int32(insn_RR(rdtime_d_op, (int)rj->encoding(), (int)rd->encoding())); }

  void cpucfg(Register rd, Register rj) { emit_int32(insn_RR(cpucfg_op, (int)rj->encoding(), (int)rd->encoding())); }

  void asrtle_d (Register rj, Register rk) { emit_int32(insn_RRR(asrtle_d_op , (int)rk->encoding(), (int)rj->encoding(), 0)); }
  void asrtgt_d (Register rj, Register rk) { emit_int32(insn_RRR(asrtgt_d_op , (int)rk->encoding(), (int)rj->encoding(), 0)); }

  void alsl_w(Register rd, Register rj, Register rk, int sa2)  { assert(is_uimm(sa2, 2), "not a unsigned 2-bit int");  emit_int32(insn_I8RR(alsl_w_op, ( (0 << 7) | (sa2 << 5) | (int)rk->encoding() ), (int)rj->encoding(), (int)rd->encoding())); }
  void alsl_wu(Register rd, Register rj, Register rk, int sa2) { assert(is_uimm(sa2, 2), "not a unsigned 2-bit int"); emit_int32(insn_I8RR(alsl_w_op, ( (1 << 7) | (sa2 << 5) | (int)rk->encoding() ), (int)rj->encoding(), (int)rd->encoding())); }
  void bytepick_w(Register rd, Register rj, Register rk, int sa2) { assert(is_uimm(sa2, 2), "not a unsigned 2-bit int"); emit_int32(insn_I8RR(bytepick_w_op, ( (0 << 7) | (sa2 << 5) | (int)rk->encoding() ), (int)rj->encoding(), (int)rd->encoding())); }
  void bytepick_d(Register rd, Register rj, Register rk, int sa3) { assert(is_uimm(sa3, 3), "not a unsigned 3-bit int"); emit_int32(insn_I8RR(bytepick_d_op, ( (sa3 << 5) | (int)rk->encoding() ), (int)rj->encoding(), (int)rd->encoding())); }

  void add_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(add_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void add_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(add_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void sub_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(sub_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void sub_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(sub_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void slt  (Register rd, Register rj, Register rk)  { emit_int32(insn_RRR(slt_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void sltu (Register rd, Register rj, Register rk)  { emit_int32(insn_RRR(sltu_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void maskeqz (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(maskeqz_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void masknez (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(masknez_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void nor (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(nor_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void AND (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(and_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void OR  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(or_op,   (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void XOR (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(xor_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void orn (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(orn_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void andn(Register rd, Register rj, Register rk) { emit_int32(insn_RRR(andn_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void sll_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(sll_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void srl_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(srl_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void sra_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(sra_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void sll_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(sll_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void srl_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(srl_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void sra_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(sra_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void rotr_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(rotr_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void rotr_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(rotr_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void mul_w     (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mul_w_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mulh_w    (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mulh_w_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mulh_wu   (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mulh_wu_op,   (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mul_d     (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mul_d_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mulh_d    (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mulh_d_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mulh_du   (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mulh_du_op,   (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mulw_d_w  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mulw_d_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mulw_d_wu (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mulw_d_wu_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void div_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(div_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mod_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mod_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void div_wu(Register rd, Register rj, Register rk) { emit_int32(insn_RRR(div_wu_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mod_wu(Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mod_wu_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void div_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(div_d_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mod_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mod_d_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void div_du(Register rd, Register rj, Register rk) { emit_int32(insn_RRR(div_du_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void mod_du(Register rd, Register rj, Register rk) { emit_int32(insn_RRR(mod_du_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void crc_w_b_w  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(crc_w_b_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void crc_w_h_w  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(crc_w_h_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void crc_w_w_w  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(crc_w_w_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void crc_w_d_w  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(crc_w_d_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void crcc_w_b_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(crcc_w_b_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void crcc_w_h_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(crcc_w_h_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void crcc_w_w_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(crcc_w_w_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void crcc_w_d_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(crcc_w_d_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void brk(int code)      { assert(is_uimm(code, 15), "not a unsigned 15-bit int"); emit_int32(insn_I15(break_op, code)); }

  void alsl_d(Register rd, Register rj, Register rk, int sa2)  { assert(is_uimm(sa2, 2), "not a unsigned 2-bit int");  emit_int32(insn_I8RR(alsl_d_op, ( (sa2 << 5) | (int)rk->encoding() ), (int)rj->encoding(), (int)rd->encoding())); }

  void slli_w(Register rd, Register rj, int ui5)  { assert(is_uimm(ui5, 5), "not a unsigned 5-bit int"); emit_int32(insn_I8RR(slli_op, ( (0b001 << 5) | ui5 ), (int)rj->encoding(), (int)rd->encoding())); }
  void slli_d(Register rd, Register rj, int ui6)  { assert(is_uimm(ui6, 6), "not a unsigned 6-bit int"); emit_int32(insn_I8RR(slli_op, ( (0b01  << 6) | ui6 ), (int)rj->encoding(), (int)rd->encoding())); }
  void srli_w(Register rd, Register rj, int ui5)  { assert(is_uimm(ui5, 5), "not a unsigned 5-bit int"); emit_int32(insn_I8RR(srli_op, ( (0b001 << 5) | ui5 ), (int)rj->encoding(), (int)rd->encoding())); }
  void srli_d(Register rd, Register rj, int ui6)  { assert(is_uimm(ui6, 6), "not a unsigned 6-bit int"); emit_int32(insn_I8RR(srli_op, ( (0b01  << 6) | ui6 ), (int)rj->encoding(), (int)rd->encoding())); }
  void srai_w(Register rd, Register rj, int ui5)  { assert(is_uimm(ui5, 5), "not a unsigned 5-bit int"); emit_int32(insn_I8RR(srai_op, ( (0b001 << 5) | ui5 ), (int)rj->encoding(), (int)rd->encoding())); }
  void srai_d(Register rd, Register rj, int ui6)  { assert(is_uimm(ui6, 6), "not a unsigned 6-bit int"); emit_int32(insn_I8RR(srai_op, ( (0b01  << 6) | ui6 ), (int)rj->encoding(), (int)rd->encoding())); }
  void rotri_w(Register rd, Register rj, int ui5) { assert(is_uimm(ui5, 5), "not a unsigned 5-bit int"); emit_int32(insn_I8RR(rotri_op, ( (0b001 << 5) | ui5 ), (int)rj->encoding(), (int)rd->encoding())); }
  void rotri_d(Register rd, Register rj, int ui6) { assert(is_uimm(ui6, 6), "not a unsigned 6-bit int"); emit_int32(insn_I8RR(rotri_op, ( (0b01  << 6) | ui6 ), (int)rj->encoding(), (int)rd->encoding())); }

  void bstrins_w  (Register rd, Register rj, int msbw, int lsbw)  { assert(is_uimm(msbw, 5) && is_uimm(lsbw, 5), "not a unsigned 5-bit int"); emit_int32(insn_I12RR(bstr_w_op, ( (1<<11) | (low(msbw, 5)<<6) | (0<<5) | low(lsbw, 5) ), (int)rj->encoding(), (int)rd->encoding())); }
  void bstrpick_w  (Register rd, Register rj, int msbw, int lsbw) { assert(is_uimm(msbw, 5) && is_uimm(lsbw, 5), "not a unsigned 5-bit int"); emit_int32(insn_I12RR(bstr_w_op, ( (1<<11) | (low(msbw, 5)<<6) | (1<<5) | low(lsbw, 5) ), (int)rj->encoding(), (int)rd->encoding())); }
  void bstrins_d  (Register rd, Register rj, int msbd, int lsbd)  { assert(is_uimm(msbd, 6) && is_uimm(lsbd, 6), "not a unsigned 6-bit int"); emit_int32(insn_I12RR(bstrins_d_op, ( (low(msbd, 6)<<6) | low(lsbd, 6) ), (int)rj->encoding(), (int)rd->encoding())); }
  void bstrpick_d  (Register rd, Register rj, int msbd, int lsbd) { assert(is_uimm(msbd, 6) && is_uimm(lsbd, 6), "not a unsigned 6-bit int"); emit_int32(insn_I12RR(bstrpick_d_op, ( (low(msbd, 6)<<6) | low(lsbd, 6) ), (int)rj->encoding(), (int)rd->encoding())); }

  void fadd_s  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fadd_s_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fadd_d  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fadd_d_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fsub_s  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fsub_s_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fsub_d  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fsub_d_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmul_s  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmul_s_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmul_d  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmul_d_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fdiv_s  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fdiv_s_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fdiv_d  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fdiv_d_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmax_s  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmax_s_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmax_d  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmax_d_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmin_s  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmin_s_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmin_d  (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmin_d_op,  (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmaxa_s (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmaxa_s_op, (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmaxa_d (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmaxa_d_op, (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmina_s (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmina_s_op, (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmina_d (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fmina_d_op, (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }

  void fscaleb_s (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fscaleb_s_op, (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fscaleb_d (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fscaleb_d_op, (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fcopysign_s (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fcopysign_s_op, (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fcopysign_d (FloatRegister fd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRR(fcopysign_d_op, (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }

  void fabs_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fabs_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fabs_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fabs_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fneg_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fneg_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fneg_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fneg_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void flogb_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(flogb_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void flogb_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(flogb_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fclass_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fclass_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fclass_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fclass_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fsqrt_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fsqrt_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fsqrt_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fsqrt_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void frecip_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(frecip_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void frecip_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(frecip_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void frsqrt_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(frsqrt_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void frsqrt_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(frsqrt_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fmov_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fmov_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fmov_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fmov_d_op, (int)fj->encoding(), (int)fd->encoding())); }

  void movgr2fr_w (FloatRegister fd, Register rj)  { emit_int32(insn_RR(movgr2fr_w_op,  (int)rj->encoding(), (int)fd->encoding())); }
  void movgr2fr_d (FloatRegister fd, Register rj)  { emit_int32(insn_RR(movgr2fr_d_op,  (int)rj->encoding(), (int)fd->encoding())); }
  void movgr2frh_w(FloatRegister fd, Register rj)  { emit_int32(insn_RR(movgr2frh_w_op, (int)rj->encoding(), (int)fd->encoding())); }
  void movfr2gr_s (Register rd, FloatRegister fj)  { emit_int32(insn_RR(movfr2gr_s_op,  (int)fj->encoding(), (int)rd->encoding())); }
  void movfr2gr_d (Register rd, FloatRegister fj)  { emit_int32(insn_RR(movfr2gr_d_op,  (int)fj->encoding(), (int)rd->encoding())); }
  void movfrh2gr_s(Register rd, FloatRegister fj)  { emit_int32(insn_RR(movfrh2gr_s_op, (int)fj->encoding(), (int)rd->encoding())); }
  void movgr2fcsr (int fcsr, Register rj)  { assert(is_uimm(fcsr, 2), "not a unsigned 2-bit init: fcsr0-fcsr3"); emit_int32(insn_RR(movgr2fcsr_op,  (int)rj->encoding(), fcsr)); }
  void movfcsr2gr (Register rd, int fcsr)  { assert(is_uimm(fcsr, 2), "not a unsigned 2-bit init: fcsr0-fcsr3"); emit_int32(insn_RR(movfcsr2gr_op,  fcsr, (int)rd->encoding())); }
  void movfr2cf   (ConditionalFlagRegister cd, FloatRegister fj)  { emit_int32(insn_RR(movfr2cf_op,    (int)fj->encoding(), (int)cd->encoding())); }
  void movcf2fr   (FloatRegister fd, ConditionalFlagRegister cj)  { emit_int32(insn_RR(movcf2fr_op,    (int)cj->encoding(), (int)fd->encoding())); }
  void movgr2cf   (ConditionalFlagRegister cd, Register rj)  { emit_int32(insn_RR(movgr2cf_op,    (int)rj->encoding(), (int)cd->encoding())); }
  void movcf2gr   (Register rd, ConditionalFlagRegister cj)  { emit_int32(insn_RR(movcf2gr_op,    (int)cj->encoding(), (int)rd->encoding())); }

  void fcvt_s_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fcvt_s_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void fcvt_d_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(fcvt_d_s_op, (int)fj->encoding(), (int)fd->encoding())); }

  void ftintrm_w_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrm_w_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrm_w_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrm_w_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrm_l_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrm_l_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrm_l_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrm_l_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrp_w_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrp_w_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrp_w_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrp_w_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrp_l_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrp_l_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrp_l_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrp_l_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrz_w_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrz_w_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrz_w_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrz_w_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrz_l_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrz_l_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrz_l_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrz_l_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrne_w_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrne_w_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrne_w_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrne_w_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrne_l_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrne_l_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftintrne_l_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftintrne_l_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftint_w_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftint_w_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftint_w_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftint_w_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftint_l_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftint_l_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ftint_l_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ftint_l_d_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ffint_s_w(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ffint_s_w_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ffint_s_l(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ffint_s_l_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ffint_d_w(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ffint_d_w_op, (int)fj->encoding(), (int)fd->encoding())); }
  void ffint_d_l(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(ffint_d_l_op, (int)fj->encoding(), (int)fd->encoding())); }
  void frint_s(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(frint_s_op, (int)fj->encoding(), (int)fd->encoding())); }
  void frint_d(FloatRegister fd, FloatRegister fj)  { emit_int32(insn_RR(frint_d_op, (int)fj->encoding(), (int)fd->encoding())); }

  void slti  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(slti_op,   si12, (int)rj->encoding(), (int)rd->encoding())); }
  void sltui (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(sltui_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void addi_w(Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(addi_w_op, si12, (int)rj->encoding(), (int)rd->encoding())); }
  void addi_d(Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(addi_d_op, si12, (int)rj->encoding(), (int)rd->encoding())); }
  void lu52i_d(Register rd, Register rj, int si12) { /*assert(is_simm(si12, 12), "not a signed 12-bit int");*/ emit_int32(insn_I12RR(lu52i_d_op,  simm12(si12), (int)rj->encoding(), (int)rd->encoding())); }
  void andi  (Register rd, Register rj, int ui12)  { assert(is_uimm(ui12, 12), "not a unsigned 12-bit int"); emit_int32(insn_I12RR(andi_op,   ui12, (int)rj->encoding(), (int)rd->encoding())); }
  void ori   (Register rd, Register rj, int ui12)  { assert(is_uimm(ui12, 12), "not a unsigned 12-bit int"); emit_int32(insn_I12RR(ori_op,    ui12, (int)rj->encoding(), (int)rd->encoding())); }
  void xori  (Register rd, Register rj, int ui12)  { assert(is_uimm(ui12, 12), "not a unsigned 12-bit int"); emit_int32(insn_I12RR(xori_op,   ui12, (int)rj->encoding(), (int)rd->encoding())); }

  void fmadd_s (FloatRegister fd, FloatRegister fj, FloatRegister fk, FloatRegister fa) { emit_int32(insn_RRRR(fmadd_s_op , (int)fa->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmadd_d (FloatRegister fd, FloatRegister fj, FloatRegister fk, FloatRegister fa) { emit_int32(insn_RRRR(fmadd_d_op , (int)fa->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmsub_s (FloatRegister fd, FloatRegister fj, FloatRegister fk, FloatRegister fa) { emit_int32(insn_RRRR(fmsub_s_op , (int)fa->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fmsub_d (FloatRegister fd, FloatRegister fj, FloatRegister fk, FloatRegister fa) { emit_int32(insn_RRRR(fmsub_d_op , (int)fa->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fnmadd_s (FloatRegister fd, FloatRegister fj, FloatRegister fk, FloatRegister fa) { emit_int32(insn_RRRR(fnmadd_s_op , (int)fa->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fnmadd_d (FloatRegister fd, FloatRegister fj, FloatRegister fk, FloatRegister fa) { emit_int32(insn_RRRR(fnmadd_d_op , (int)fa->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fnmsub_s (FloatRegister fd, FloatRegister fj, FloatRegister fk, FloatRegister fa)  { emit_int32(insn_RRRR(fnmsub_s_op , (int)fa->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }
  void fnmsub_d (FloatRegister fd, FloatRegister fj, FloatRegister fk, FloatRegister fa)  { emit_int32(insn_RRRR(fnmsub_d_op , (int)fa->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }

  void fcmp_caf_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_caf, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cun_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_cun , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_ceq_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_ceq , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cueq_s (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_cueq, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_clt_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_clt , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cult_s (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_cult, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cle_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_cle , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cule_s (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_cule, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cne_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_cne , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cor_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_cor , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cune_s (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_cune, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_saf_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_saf , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sun_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_sun , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_seq_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_seq , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sueq_s (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_sueq, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_slt_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_slt , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sult_s (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_sult, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sle_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_sle , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sule_s (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_sule, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sne_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_sne , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sor_s  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_sor , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sune_s (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_s_op, fcmp_sune, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }

  void fcmp_caf_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_caf, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cun_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_cun , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_ceq_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_ceq , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cueq_d (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_cueq, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_clt_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_clt , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cult_d (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_cult, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cle_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_cle , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cule_d (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_cule, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cne_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_cne , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cor_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_cor , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_cune_d (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_cune, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_saf_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_saf , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sun_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_sun , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_seq_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_seq , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sueq_d (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_sueq, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_slt_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_slt , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sult_d (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_sult, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sle_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_sle , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sule_d (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_sule, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sne_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_sne , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sor_d  (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_sor , (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }
  void fcmp_sune_d (ConditionalFlagRegister cd, FloatRegister fj, FloatRegister fk) { emit_int32(insn_RRRR(fcmp_cond_d_op, fcmp_sune, (int)fk->encoding(), (int)fj->encoding(), (int)cd->encoding())); }

  void fsel (FloatRegister fd, FloatRegister fj, FloatRegister fk, ConditionalFlagRegister ca) { emit_int32(insn_RRRR(fsel_op, (int)ca->encoding(), (int)fk->encoding(), (int)fj->encoding(), (int)fd->encoding())); }

  void addu16i_d(Register rj, Register rd, int si16)      { assert(is_simm(si16, 16), "not a signed 16-bit int"); emit_int32(insn_I16RR(addu16i_d_op, si16, (int)rj->encoding(), (int)rd->encoding())); }

  void lu12i_w(Register rj, int si20)      { /*assert(is_simm(si20, 20), "not a signed 20-bit int");*/ emit_int32(insn_I20R(lu12i_w_op, simm20(si20), (int)rj->encoding())); }
  void lu32i_d(Register rj, int si20)      { /*assert(is_simm(si20, 20), "not a signed 20-bit int");*/ emit_int32(insn_I20R(lu32i_d_op, simm20(si20), (int)rj->encoding())); }
  void pcaddi(Register rj, int si20)      { assert(is_simm(si20, 20), "not a signed 20-bit int"); emit_int32(insn_I20R(pcaddi_op, si20, (int)rj->encoding())); }
  void pcalau12i(Register rj, int si20)      { assert(is_simm(si20, 20), "not a signed 20-bit int"); emit_int32(insn_I20R(pcalau12i_op, si20, (int)rj->encoding())); }
  void pcaddu12i(Register rj, int si20)      { assert(is_simm(si20, 20), "not a signed 20-bit int"); emit_int32(insn_I20R(pcaddu12i_op, si20, (int)rj->encoding())); }
  void pcaddu18i(Register rj, int si20)      { assert(is_simm(si20, 20), "not a signed 20-bit int"); emit_int32(insn_I20R(pcaddu18i_op, si20, (int)rj->encoding())); }

  void ll_w  (Register rd, Register rj, int si16)   { assert(is_simm(si16, 16) && ((si16 & 0x3) == 0), "not a signed 16-bit int"); emit_int32(insn_I14RR(ll_w_op, si16>>2, (int)rj->encoding(), (int)rd->encoding())); }
  void sc_w  (Register rd, Register rj, int si16)   { assert(is_simm(si16, 16) && ((si16 & 0x3) == 0), "not a signed 16-bit int"); emit_int32(insn_I14RR(sc_w_op, si16>>2, (int)rj->encoding(), (int)rd->encoding())); }
  void ll_d  (Register rd, Register rj, int si16)   { assert(is_simm(si16, 16) && ((si16 & 0x3) == 0), "not a signed 16-bit int"); emit_int32(insn_I14RR(ll_d_op, si16>>2, (int)rj->encoding(), (int)rd->encoding())); }
  void sc_d  (Register rd, Register rj, int si16)   { assert(is_simm(si16, 16) && ((si16 & 0x3) == 0), "not a signed 16-bit int"); emit_int32(insn_I14RR(sc_d_op, si16>>2, (int)rj->encoding(), (int)rd->encoding())); }
  void ldptr_w  (Register rd, Register rj, int si16)  { assert(is_simm(si16, 16) && ((si16 & 0x3) == 0), "not a signed 16-bit int"); emit_int32(insn_I14RR(ldptr_w_op, si16>>2, (int)rj->encoding(), (int)rd->encoding())); }
  void stptr_w  (Register rd, Register rj, int si16)  { assert(is_simm(si16, 16) && ((si16 & 0x3) == 0), "not a signed 16-bit int"); emit_int32(insn_I14RR(stptr_w_op, si16>>2, (int)rj->encoding(), (int)rd->encoding())); }
  void ldptr_d  (Register rd, Register rj, int si16)  { assert(is_simm(si16, 16) && ((si16 & 0x3) == 0), "not a signed 16-bit int"); emit_int32(insn_I14RR(ldptr_d_op, si16>>2, (int)rj->encoding(), (int)rd->encoding())); }
  void stptr_d  (Register rd, Register rj, int si16)  { assert(is_simm(si16, 16) && ((si16 & 0x3) == 0), "not a signed 16-bit int"); emit_int32(insn_I14RR(stptr_d_op, si16>>2, (int)rj->encoding(), (int)rd->encoding())); }

  void ld_b  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ld_b_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void ld_h  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ld_h_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void ld_w  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ld_w_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void ld_d  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ld_d_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void st_b  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(st_b_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void st_h  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(st_h_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void st_w  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(st_w_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void st_d  (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(st_d_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void ld_bu (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ld_bu_op, si12, (int)rj->encoding(), (int)rd->encoding())); }
  void ld_hu (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ld_hu_op, si12, (int)rj->encoding(), (int)rd->encoding())); }
  void ld_wu (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ld_wu_op, si12, (int)rj->encoding(), (int)rd->encoding())); }
  void preld (int hint, Register rj, int si12)  { assert(is_uimm(hint, 5), "not a unsigned 5-bit int"); assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(preld_op, si12, (int)rj->encoding(), hint)); }
  void fld_s (FloatRegister fd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(fld_s_op, si12, (int)rj->encoding(), (int)fd->encoding())); }
  void fst_s (FloatRegister fd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(fst_s_op, si12, (int)rj->encoding(), (int)fd->encoding())); }
  void fld_d (FloatRegister fd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(fld_d_op, si12, (int)rj->encoding(), (int)fd->encoding())); }
  void fst_d (FloatRegister fd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(fst_d_op, si12, (int)rj->encoding(), (int)fd->encoding())); }
  void ldl_w (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ldl_w_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }
  void ldr_w (Register rd, Register rj, int si12)  { assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(ldr_w_op,  si12, (int)rj->encoding(), (int)rd->encoding())); }

  void ldx_b  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldx_b_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldx_h  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldx_h_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldx_w  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldx_w_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldx_d  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldx_d_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stx_b  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stx_b_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stx_h  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stx_h_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stx_w  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stx_w_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stx_d  (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stx_d_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldx_bu (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldx_bu_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldx_hu (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldx_hu_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldx_wu (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldx_wu_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void fldx_s (FloatRegister fd, Register rj, Register rk) { emit_int32(insn_RRR(fldx_s_op,    (int)rk->encoding(), (int)rj->encoding(), (int)fd->encoding())); }
  void fldx_d (FloatRegister fd, Register rj, Register rk) { emit_int32(insn_RRR(fldx_d_op,    (int)rk->encoding(), (int)rj->encoding(), (int)fd->encoding())); }
  void fstx_s (FloatRegister fd, Register rj, Register rk) { emit_int32(insn_RRR(fstx_s_op,    (int)rk->encoding(), (int)rj->encoding(), (int)fd->encoding())); }
  void fstx_d (FloatRegister fd, Register rj, Register rk) { emit_int32(insn_RRR(fstx_d_op,    (int)rk->encoding(), (int)rj->encoding(), (int)fd->encoding())); }

  void ld_b  (Register rd, Address src);
  void ld_bu (Register rd, Address src);
  void ld_d  (Register rd, Address src);
  void ld_h  (Register rd, Address src);
  void ld_hu (Register rd, Address src);
  void ll_w  (Register rd, Address src);
  void ll_d  (Register rd, Address src);
  void ld_wu (Register rd, Address src);
  void ld_w  (Register rd, Address src);
  void st_b  (Register rd, Address dst);
  void st_d  (Register rd, Address dst);
  void st_w  (Register rd, Address dst);
  void sc_w  (Register rd, Address dst);
  void sc_d  (Register rd, Address dst);
  void st_h  (Register rd, Address dst);
  void fld_s (FloatRegister fd, Address src);
  void fld_d (FloatRegister fd, Address src);
  void fst_s (FloatRegister fd, Address dst);
  void fst_d (FloatRegister fd, Address dst);

  void amswap_w   (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amswap_w_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amswap_d   (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amswap_d_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amadd_w    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amadd_w_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amadd_d    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amadd_d_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rj->encoding())); }
  void amand_w    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amand_w_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amand_d    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amand_d_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amor_w     (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amor_w_op,      (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amor_d     (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amor_d_op,      (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amxor_w    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amxor_w_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amxor_d    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amxor_d_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammax_w    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammax_w_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammax_d    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammax_d_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammin_w    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammin_w_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammin_d    (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammin_d_op,     (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammax_wu   (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammax_wu_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammax_du   (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammax_du_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammin_wu   (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammin_wu_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammin_du   (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammin_du_op,    (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amswap_db_w(Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amswap_db_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amswap_db_d(Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amswap_db_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amadd_db_w (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amadd_db_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amadd_db_d (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amadd_db_d_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amand_db_w (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amand_db_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amand_db_d (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amand_db_d_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amor_db_w  (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amor_db_w_op,   (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amor_db_d  (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amor_db_d_op,   (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amxor_db_w (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amxor_db_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void amxor_db_d (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(amxor_db_d_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammax_db_w (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammax_db_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammax_db_d (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammax_db_d_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammin_db_w (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammin_db_w_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammin_db_d (Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammin_db_d_op,  (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammax_db_wu(Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammax_db_wu_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammax_db_du(Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammax_db_du_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammin_db_wu(Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammin_db_wu_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ammin_db_du(Register rd, Register rk, Register rj) { assert_different_registers(rd, rj); assert_different_registers(rd, rk); emit_int32(insn_RRR(ammin_db_du_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void dbar(int hint)      {
    assert(is_uimm(hint, 15), "not a unsigned 15-bit int");

    if (os::is_ActiveCoresMP())
      andi(R0, R0, 0);
    else
      emit_int32(insn_I15(dbar_op, hint));
  }
  void ibar(int hint)      { assert(is_uimm(hint, 15), "not a unsigned 15-bit int"); emit_int32(insn_I15(ibar_op, hint)); }

  void fldgt_s (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(fldgt_s_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void fldgt_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(fldgt_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void fldle_s (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(fldle_s_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void fldle_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(fldle_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void fstgt_s (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(fstgt_s_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void fstgt_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(fstgt_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void fstle_s (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(fstle_s_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void fstle_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(fstle_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void ldgt_b (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldgt_b_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldgt_h (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldgt_h_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldgt_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldgt_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldgt_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldgt_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldle_b (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldle_b_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldle_h (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldle_h_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldle_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldle_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void ldle_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(ldle_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stgt_b (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stgt_b_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stgt_h (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stgt_h_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stgt_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stgt_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stgt_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stgt_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stle_b (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stle_b_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stle_h (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stle_h_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stle_w (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stle_w_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }
  void stle_d (Register rd, Register rj, Register rk) { emit_int32(insn_RRR(stle_d_op, (int)rk->encoding(), (int)rj->encoding(), (int)rd->encoding())); }

  void beqz(Register rj, int offs)      { assert(is_simm(offs, 21), "not a signed 21-bit int"); emit_int32(insn_IRI(beqz_op, offs, (int)rj->encoding())); }
  void bnez(Register rj, int offs)      { assert(is_simm(offs, 21), "not a signed 21-bit int"); emit_int32(insn_IRI(bnez_op, offs, (int)rj->encoding())); }
  void bceqz(ConditionalFlagRegister cj, int offs)     { assert(is_simm(offs, 21), "not a signed 21-bit int"); emit_int32(insn_IRI(bccondz_op, offs, ( (0b00<<3) | (int)cj->encoding()))); }
  void bcnez(ConditionalFlagRegister cj, int offs)     { assert(is_simm(offs, 21), "not a signed 21-bit int"); emit_int32(insn_IRI(bccondz_op, offs, ( (0b01<<3) | (int)cj->encoding()))); }

  void jirl(Register rd, Register rj, int offs)      { assert(is_simm(offs, 18) && ((offs & 3) == 0), "not a signed 18-bit int"); emit_int32(insn_I16RR(jirl_op, offs >> 2, (int)rj->encoding(), (int)rd->encoding())); }

  void b(int offs)      { assert(is_simm(offs, 26), "not a signed 26-bit int"); emit_int32(insn_I26(b_op, offs)); }
  void bl(int offs)     { assert(is_simm(offs, 26), "not a signed 26-bit int"); emit_int32(insn_I26(bl_op, offs)); }


  void beq(Register rj, Register rd, int offs)      { assert(is_simm(offs, 16), "not a signed 16-bit int"); emit_int32(insn_I16RR(beq_op, offs, (int)rj->encoding(), (int)rd->encoding())); }
  void bne(Register rj, Register rd, int offs)      { assert(is_simm(offs, 16), "not a signed 16-bit int"); emit_int32(insn_I16RR(bne_op, offs, (int)rj->encoding(), (int)rd->encoding())); }
  void blt(Register rj, Register rd, int offs)      { assert(is_simm(offs, 16), "not a signed 16-bit int"); emit_int32(insn_I16RR(blt_op, offs, (int)rj->encoding(), (int)rd->encoding())); }
  void bge(Register rj, Register rd, int offs)      { assert(is_simm(offs, 16), "not a signed 16-bit int"); emit_int32(insn_I16RR(bge_op, offs, (int)rj->encoding(), (int)rd->encoding())); }
  void bltu(Register rj, Register rd, int offs)      { assert(is_simm(offs, 16), "not a signed 16-bit int"); emit_int32(insn_I16RR(bltu_op, offs, (int)rj->encoding(), (int)rd->encoding())); }
  void bgeu(Register rj, Register rd, int offs)      { assert(is_simm(offs, 16), "not a signed 16-bit int"); emit_int32(insn_I16RR(bgeu_op, offs, (int)rj->encoding(), (int)rd->encoding())); }

  void beq   (Register rj, Register rd, address entry) { beq   (rj, rd, offset16(entry)); }
  void bne   (Register rj, Register rd, address entry) { bne   (rj, rd, offset16(entry)); }
  void blt   (Register rj, Register rd, address entry) { blt   (rj, rd, offset16(entry)); }
  void bge   (Register rj, Register rd, address entry) { bge   (rj, rd, offset16(entry)); }
  void bltu  (Register rj, Register rd, address entry) { bltu  (rj, rd, offset16(entry)); }
  void bgeu  (Register rj, Register rd, address entry) { bgeu  (rj, rd, offset16(entry)); }
  void beqz  (Register rj, address entry) { beqz  (rj, offset21(entry)); }
  void bnez  (Register rj, address entry) { bnez  (rj, offset21(entry)); }
  void b(address entry) { b(offset26(entry)); }
  void bl(address entry) { bl(offset26(entry)); }
  void bceqz(ConditionalFlagRegister cj, address entry)     { bceqz(cj, offset21(entry)); }
  void bcnez(ConditionalFlagRegister cj, address entry)     { bcnez(cj, offset21(entry)); }

  void beq   (Register rj, Register rd, Label& L) { beq   (rj, rd, target(L)); }
  void bne   (Register rj, Register rd, Label& L) { bne   (rj, rd, target(L)); }
  void blt   (Register rj, Register rd, Label& L) { blt   (rj, rd, target(L)); }
  void bge   (Register rj, Register rd, Label& L) { bge   (rj, rd, target(L)); }
  void bltu  (Register rj, Register rd, Label& L) { bltu  (rj, rd, target(L)); }
  void bgeu  (Register rj, Register rd, Label& L) { bgeu  (rj, rd, target(L)); }
  void beqz  (Register rj, Label& L) { beqz  (rj, target(L)); }
  void bnez  (Register rj, Label& L) { bnez  (rj, target(L)); }
  void b(Label& L)      { b(target(L)); }
  void bl(Label& L)     { bl(target(L)); }
  void bceqz(ConditionalFlagRegister cj, Label& L)     { bceqz(cj, target(L)); }
  void bcnez(ConditionalFlagRegister cj, Label& L)     { bcnez(cj, target(L)); }

  typedef enum {
    // hint[4]
    Completion = 0,
    Ordering   = (1 << 4),

    // The bitwise-not of the below constants is corresponding to the hint. This is convenient for OR operation.
    // hint[3:2] and hint[1:0]
    LoadLoad   = ((1 << 3) | (1 << 1)),
    LoadStore  = ((1 << 3) | (1 << 0)),
    StoreLoad  = ((1 << 2) | (1 << 1)),
    StoreStore = ((1 << 2) | (1 << 0)),
    AnyAny     = ((3 << 2) | (3 << 0)),
  } Membar_mask_bits;

  // Serializes memory and blows flags
  void membar(Membar_mask_bits hint) {
    assert((hint & (3 << 0)) != 0, "membar mask unsupported!");
    assert((hint & (3 << 2)) != 0, "membar mask unsupported!");
    dbar(Ordering | (~hint & 0xf));
  }

  // LSX and LASX
#define ASSERT_LSX  assert(UseLSX, "");
#define ASSERT_LASX assert(UseLASX, "");

  void  vadd_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vadd_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vadd_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vadd_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vadd_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vadd_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vadd_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vadd_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vadd_q(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vadd_q_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvadd_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvadd_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvadd_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvadd_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvadd_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvadd_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvadd_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvadd_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvadd_q(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvadd_q_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vsub_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsub_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsub_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsub_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsub_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsub_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsub_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsub_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsub_q(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsub_q_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvsub_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsub_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsub_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsub_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsub_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsub_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsub_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsub_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsub_q(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsub_q_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vaddi_bu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vaddi_bu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vaddi_hu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vaddi_hu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vaddi_wu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vaddi_wu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vaddi_du(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vaddi_du_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void xvaddi_bu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvaddi_bu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvaddi_hu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvaddi_hu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvaddi_wu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvaddi_wu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvaddi_du(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvaddi_du_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }

  void  vsubi_bu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vsubi_bu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsubi_hu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vsubi_hu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsubi_wu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vsubi_wu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsubi_du(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vsubi_du_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void xvsubi_bu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvsubi_bu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsubi_hu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvsubi_hu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsubi_wu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvsubi_wu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsubi_du(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvsubi_du_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }

  void  vneg_b(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vneg_b_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vneg_h(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vneg_h_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vneg_w(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vneg_w_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vneg_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vneg_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvneg_b(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvneg_b_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvneg_h(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvneg_h_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvneg_w(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvneg_w_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvneg_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvneg_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vabsd_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vabsd_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vabsd_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vabsd_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vabsd_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vabsd_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vabsd_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vabsd_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvabsd_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvabsd_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvabsd_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvabsd_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvabsd_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvabsd_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvabsd_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvabsd_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmax_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmax_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmax_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmax_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmax_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmax_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmax_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmax_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmax_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmax_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmax_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmax_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmax_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmax_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmax_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmax_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmin_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmin_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmin_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmin_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmin_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmin_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmin_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmin_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmin_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmin_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmin_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmin_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmin_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmin_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmin_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmin_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmul_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmul_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmul_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmul_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmul_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmul_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmul_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmul_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmul_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmul_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmul_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmul_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmul_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmul_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmul_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmul_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmuh_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmuh_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmuh_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmuh_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmuh_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmuh_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmuh_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmuh_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmuh_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmuh_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmuh_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmuh_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmuh_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmuh_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmuh_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmuh_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmuh_bu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmuh_bu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmuh_hu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmuh_hu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmuh_wu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmuh_wu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmuh_du(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmuh_du_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmuh_bu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmuh_bu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmuh_hu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmuh_hu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmuh_wu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmuh_wu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmuh_du(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmuh_du_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmulwev_h_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmulwev_h_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmulwev_w_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmulwev_w_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmulwev_d_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmulwev_d_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmulwev_q_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmulwev_q_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmulwev_h_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmulwev_h_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmulwev_w_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmulwev_w_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmulwev_d_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmulwev_d_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmulwev_q_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmulwev_q_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmulwod_h_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmulwod_h_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmulwod_w_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmulwod_w_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmulwod_d_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmulwod_d_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmulwod_q_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmulwod_q_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmulwod_h_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmulwod_h_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmulwod_w_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmulwod_w_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmulwod_d_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmulwod_d_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmulwod_q_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmulwod_q_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmadd_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmadd_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmadd_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmadd_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmadd_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmadd_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmadd_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmadd_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmadd_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmadd_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmadd_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmadd_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmadd_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmadd_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmadd_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmadd_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vmsub_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmsub_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmsub_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmsub_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmsub_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmsub_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vmsub_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vmsub_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvmsub_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmsub_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmsub_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmsub_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmsub_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmsub_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvmsub_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvmsub_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void vext2xv_h_b(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_h_b_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_w_b(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_w_b_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_d_b(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_d_b_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_w_h(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_w_h_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_d_h(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_d_h_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_d_w(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_d_w_op, (int)xj->encoding(), (int)xd->encoding())); }

  void vext2xv_hu_bu(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_hu_bu_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_wu_bu(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_wu_bu_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_du_bu(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_du_bu_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_wu_hu(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_wu_hu_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_du_hu(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_du_hu_op, (int)xj->encoding(), (int)xd->encoding())); }
  void vext2xv_du_wu(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(vext2xv_du_wu_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vldi(FloatRegister vd, int i13) { ASSERT_LSX  emit_int32(insn_I13R( vldi_op, i13, (int)vd->encoding())); }
  void xvldi(FloatRegister xd, int i13) { ASSERT_LASX emit_int32(insn_I13R(xvldi_op, i13, (int)xd->encoding())); }

  void  vand_v(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vand_v_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvand_v(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvand_v_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vor_v(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vor_v_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvor_v(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvor_v_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vxor_v(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vxor_v_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvxor_v(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvxor_v_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vnor_v(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vnor_v_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvnor_v(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvnor_v_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vandn_v(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vandn_v_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvandn_v(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvandn_v_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vorn_v(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vorn_v_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvorn_v(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvorn_v_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vandi_b(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vandi_b_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void xvandi_b(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvandi_b_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void  vori_b(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vori_b_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void xvori_b(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvori_b_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void  vxori_b(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vxori_b_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void xvxori_b(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvxori_b_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void  vnori_b(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vnori_b_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void xvnori_b(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvnori_b_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void  vsll_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsll_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsll_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsll_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsll_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsll_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsll_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsll_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvsll_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsll_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsll_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsll_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsll_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsll_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsll_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsll_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vslli_b(FloatRegister vd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vslli_b_op, ui3, (int)vj->encoding(), (int)vd->encoding())); }
  void  vslli_h(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vslli_h_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vslli_w(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vslli_w_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vslli_d(FloatRegister vd, FloatRegister vj, int ui6) { ASSERT_LSX  emit_int32(insn_I6RR( vslli_d_op, ui6, (int)vj->encoding(), (int)vd->encoding())); }
  void xvslli_b(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvslli_b_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvslli_h(FloatRegister xd, FloatRegister xj, int ui4) { ASSERT_LASX emit_int32(insn_I4RR(xvslli_h_op, ui4, (int)xj->encoding(), (int)xd->encoding())); }
  void xvslli_w(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvslli_w_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvslli_d(FloatRegister xd, FloatRegister xj, int ui6) { ASSERT_LASX emit_int32(insn_I6RR(xvslli_d_op, ui6, (int)xj->encoding(), (int)xd->encoding())); }

  void  vsrl_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsrl_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrl_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsrl_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrl_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsrl_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrl_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsrl_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvsrl_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsrl_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrl_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsrl_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrl_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsrl_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrl_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsrl_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vsrli_b(FloatRegister vd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vsrli_b_op, ui3, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrli_h(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vsrli_h_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrli_w(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vsrli_w_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrli_d(FloatRegister vd, FloatRegister vj, int ui6) { ASSERT_LSX  emit_int32(insn_I6RR( vsrli_d_op, ui6, (int)vj->encoding(), (int)vd->encoding())); }
  void xvsrli_b(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvsrli_b_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrli_h(FloatRegister xd, FloatRegister xj, int ui4) { ASSERT_LASX emit_int32(insn_I4RR(xvsrli_h_op, ui4, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrli_w(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvsrli_w_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrli_d(FloatRegister xd, FloatRegister xj, int ui6) { ASSERT_LASX emit_int32(insn_I6RR(xvsrli_d_op, ui6, (int)xj->encoding(), (int)xd->encoding())); }

  void  vsra_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsra_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsra_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsra_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsra_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsra_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsra_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsra_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvsra_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsra_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsra_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsra_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsra_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsra_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsra_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsra_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vsrai_b(FloatRegister vd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vsrai_b_op, ui3, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrai_h(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vsrai_h_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrai_w(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vsrai_w_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrai_d(FloatRegister vd, FloatRegister vj, int ui6) { ASSERT_LSX  emit_int32(insn_I6RR( vsrai_d_op, ui6, (int)vj->encoding(), (int)vd->encoding())); }
  void xvsrai_b(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvsrai_b_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrai_h(FloatRegister xd, FloatRegister xj, int ui4) { ASSERT_LASX emit_int32(insn_I4RR(xvsrai_h_op, ui4, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrai_w(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvsrai_w_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvsrai_d(FloatRegister xd, FloatRegister xj, int ui6) { ASSERT_LASX emit_int32(insn_I6RR(xvsrai_d_op, ui6, (int)xj->encoding(), (int)xd->encoding())); }

  void  vrotr_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vrotr_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vrotr_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vrotr_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vrotr_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vrotr_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vrotr_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vrotr_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvrotr_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvrotr_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvrotr_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvrotr_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvrotr_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvrotr_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvrotr_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvrotr_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vrotri_b(FloatRegister vd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vrotri_b_op, ui3, (int)vj->encoding(), (int)vd->encoding())); }
  void  vrotri_h(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vrotri_h_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vrotri_w(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vrotri_w_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vrotri_d(FloatRegister vd, FloatRegister vj, int ui6) { ASSERT_LSX  emit_int32(insn_I6RR( vrotri_d_op, ui6, (int)vj->encoding(), (int)vd->encoding())); }
  void xvrotri_b(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvrotri_b_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvrotri_h(FloatRegister xd, FloatRegister xj, int ui4) { ASSERT_LASX emit_int32(insn_I4RR(xvrotri_h_op, ui4, (int)xj->encoding(), (int)xd->encoding())); }
  void xvrotri_w(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvrotri_w_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvrotri_d(FloatRegister xd, FloatRegister xj, int ui6) { ASSERT_LASX emit_int32(insn_I6RR(xvrotri_d_op, ui6, (int)xj->encoding(), (int)xd->encoding())); }

  void  vsrlni_b_h(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vsrlni_b_h_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrlni_h_w(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vsrlni_h_w_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrlni_w_d(FloatRegister vd, FloatRegister vj, int ui6) { ASSERT_LSX  emit_int32(insn_I6RR( vsrlni_w_d_op, ui6, (int)vj->encoding(), (int)vd->encoding())); }
  void  vsrlni_d_q(FloatRegister vd, FloatRegister vj, int ui7) { ASSERT_LSX  emit_int32(insn_I7RR( vsrlni_d_q_op, ui7, (int)vj->encoding(), (int)vd->encoding())); }

  void  vpcnt_b(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vpcnt_b_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vpcnt_h(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vpcnt_h_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vpcnt_w(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vpcnt_w_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vpcnt_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vpcnt_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvpcnt_b(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvpcnt_b_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvpcnt_h(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvpcnt_h_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvpcnt_w(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvpcnt_w_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvpcnt_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvpcnt_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vbitclr_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitclr_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitclr_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitclr_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitclr_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitclr_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitclr_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitclr_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvbitclr_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitclr_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitclr_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitclr_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitclr_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitclr_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitclr_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitclr_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vbitclri_b(FloatRegister vd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vbitclri_b_op, ui3, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitclri_h(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vbitclri_h_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitclri_w(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vbitclri_w_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitclri_d(FloatRegister vd, FloatRegister vj, int ui6) { ASSERT_LSX  emit_int32(insn_I6RR( vbitclri_d_op, ui6, (int)vj->encoding(), (int)vd->encoding())); }
  void xvbitclri_b(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvbitclri_b_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitclri_h(FloatRegister xd, FloatRegister xj, int ui4) { ASSERT_LASX emit_int32(insn_I4RR(xvbitclri_h_op, ui4, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitclri_w(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvbitclri_w_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitclri_d(FloatRegister xd, FloatRegister xj, int ui6) { ASSERT_LASX emit_int32(insn_I6RR(xvbitclri_d_op, ui6, (int)xj->encoding(), (int)xd->encoding())); }

  void  vbitset_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitset_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitset_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitset_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitset_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitset_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitset_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitset_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvbitset_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitset_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitset_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitset_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitset_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitset_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitset_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitset_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vbitseti_b(FloatRegister vd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vbitseti_b_op, ui3, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitseti_h(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vbitseti_h_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitseti_w(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vbitseti_w_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitseti_d(FloatRegister vd, FloatRegister vj, int ui6) { ASSERT_LSX  emit_int32(insn_I6RR( vbitseti_d_op, ui6, (int)vj->encoding(), (int)vd->encoding())); }
  void xvbitseti_b(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvbitseti_b_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitseti_h(FloatRegister xd, FloatRegister xj, int ui4) { ASSERT_LASX emit_int32(insn_I4RR(xvbitseti_h_op, ui4, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitseti_w(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvbitseti_w_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitseti_d(FloatRegister xd, FloatRegister xj, int ui6) { ASSERT_LASX emit_int32(insn_I6RR(xvbitseti_d_op, ui6, (int)xj->encoding(), (int)xd->encoding())); }

  void  vbitrev_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitrev_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitrev_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitrev_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitrev_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitrev_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitrev_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vbitrev_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvbitrev_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitrev_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitrev_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitrev_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitrev_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitrev_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitrev_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvbitrev_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vbitrevi_b(FloatRegister vd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vbitrevi_b_op, ui3, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitrevi_h(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vbitrevi_h_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitrevi_w(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vbitrevi_w_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vbitrevi_d(FloatRegister vd, FloatRegister vj, int ui6) { ASSERT_LSX  emit_int32(insn_I6RR( vbitrevi_d_op, ui6, (int)vj->encoding(), (int)vd->encoding())); }
  void xvbitrevi_b(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvbitrevi_b_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitrevi_h(FloatRegister xd, FloatRegister xj, int ui4) { ASSERT_LASX emit_int32(insn_I4RR(xvbitrevi_h_op, ui4, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitrevi_w(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvbitrevi_w_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvbitrevi_d(FloatRegister xd, FloatRegister xj, int ui6) { ASSERT_LASX emit_int32(insn_I6RR(xvbitrevi_d_op, ui6, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfadd_s(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfadd_s_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfadd_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfadd_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfadd_s(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfadd_s_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfadd_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfadd_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfsub_s(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfsub_s_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfsub_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfsub_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfsub_s(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfsub_s_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfsub_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfsub_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfmul_s(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfmul_s_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfmul_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfmul_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfmul_s(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfmul_s_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfmul_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfmul_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfdiv_s(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfdiv_s_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfdiv_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfdiv_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfdiv_s(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfdiv_s_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfdiv_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfdiv_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfmadd_s(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vfmadd_s_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfmadd_d(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vfmadd_d_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfmadd_s(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvfmadd_s_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfmadd_d(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvfmadd_d_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfmsub_s(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vfmsub_s_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfmsub_d(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vfmsub_d_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfmsub_s(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvfmsub_s_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfmsub_d(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvfmsub_d_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfnmadd_s(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vfnmadd_s_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfnmadd_d(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vfnmadd_d_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfnmadd_s(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvfnmadd_s_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfnmadd_d(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvfnmadd_d_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfnmsub_s(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vfnmsub_s_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfnmsub_d(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vfnmsub_d_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfnmsub_s(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvfnmsub_s_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfnmsub_d(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvfnmsub_d_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfmax_s(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfmax_s_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfmax_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfmax_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfmax_s(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfmax_s_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfmax_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfmax_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfmin_s(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfmin_s_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfmin_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfmin_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfmin_s(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfmin_s_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfmin_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfmin_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfclass_s(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfclass_s_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vfclass_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfclass_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvfclass_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfclass_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfclass_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfclass_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfsqrt_s(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfsqrt_s_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vfsqrt_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfsqrt_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvfsqrt_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfsqrt_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfsqrt_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfsqrt_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfcvtl_s_h(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vfcvtl_s_h_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vfcvtl_d_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vfcvtl_d_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvfcvtl_s_h(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfcvtl_s_h_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcvtl_d_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfcvtl_d_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfcvth_s_h(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vfcvth_s_h_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vfcvth_d_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vfcvth_d_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvfcvth_s_h(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfcvth_s_h_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcvth_d_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfcvth_d_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfcvt_h_s(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfcvt_h_s_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcvt_s_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vfcvt_s_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvfcvt_h_s(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfcvt_h_s_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcvt_s_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvfcvt_s_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vfrintrne_s(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrintrne_s_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vfrintrne_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrintrne_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvfrintrne_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrintrne_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfrintrne_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrintrne_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfrintrz_s(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrintrz_s_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vfrintrz_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrintrz_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvfrintrz_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrintrz_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfrintrz_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrintrz_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfrintrp_s(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrintrp_s_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vfrintrp_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrintrp_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvfrintrp_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrintrp_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfrintrp_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrintrp_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfrintrm_s(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrintrm_s_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vfrintrm_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrintrm_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvfrintrm_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrintrm_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfrintrm_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrintrm_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfrint_s(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrint_s_op, (int)vj->encoding(), (int)vd->encoding())); }
  void  vfrint_d(FloatRegister vd, FloatRegister vj) { ASSERT_LSX  emit_int32(insn_RR( vfrint_d_op, (int)vj->encoding(), (int)vd->encoding())); }
  void xvfrint_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrint_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvfrint_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvfrint_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrne_w_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrne_w_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vftintrne_l_d(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrne_l_d_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrne_w_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrne_w_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvftintrne_l_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrne_l_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrz_w_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrz_w_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vftintrz_l_d(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrz_l_d_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrz_w_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrz_w_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvftintrz_l_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrz_l_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrp_w_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrp_w_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vftintrp_l_d(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrp_l_d_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrp_w_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrp_w_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvftintrp_l_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrp_l_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrm_w_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrm_w_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vftintrm_l_d(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrm_l_d_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrm_w_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrm_w_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvftintrm_l_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrm_l_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftint_w_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftint_w_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vftint_l_d(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftint_l_d_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftint_w_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftint_w_s_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvftint_l_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftint_l_d_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrne_w_d(FloatRegister vd, FloatRegister vj, FloatRegister vk)  { ASSERT_LSX  emit_int32(insn_RRR( vftintrne_w_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvftintrne_w_d(FloatRegister xd, FloatRegister xj, FloatRegister xk)  { ASSERT_LASX emit_int32(insn_RRR(xvftintrne_w_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrz_w_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vftintrz_w_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvftintrz_w_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvftintrz_w_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrp_w_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vftintrp_w_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvftintrp_w_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvftintrp_w_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrm_w_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vftintrm_w_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvftintrm_w_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvftintrm_w_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vftint_w_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vftint_w_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvftint_w_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvftint_w_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrnel_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrnel_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrnel_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrnel_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrneh_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrneh_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrneh_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrneh_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrzl_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrzl_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrzl_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrzl_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrzh_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrzh_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrzh_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrzh_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrpl_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrpl_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrpl_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrpl_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrph_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrph_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrph_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrph_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrml_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrml_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrml_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrml_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintrmh_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintrmh_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintrmh_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintrmh_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftintl_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftintl_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftintl_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftintl_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vftinth_l_s(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vftinth_l_s_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvftinth_l_s(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvftinth_l_s_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vffint_s_w(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vffint_s_w_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vffint_d_l(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vffint_d_l_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvffint_s_w(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvffint_s_w_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvffint_d_l(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvffint_d_l_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vffint_s_l(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vffint_s_l_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvffint_s_l(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvffint_s_l_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vffintl_d_w(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vffintl_d_w_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvffintl_d_w(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvffintl_d_w_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vffinth_d_w(FloatRegister vd, FloatRegister rj) { ASSERT_LSX  emit_int32(insn_RR( vffinth_d_w_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvffinth_d_w(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvffinth_d_w_op, (int)xj->encoding(), (int)xd->encoding())); }

  void  vseq_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vseq_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vseq_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vseq_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vseq_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vseq_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vseq_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vseq_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvseq_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvseq_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvseq_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvseq_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvseq_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvseq_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvseq_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvseq_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vsle_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsle_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsle_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsle_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsle_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsle_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsle_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsle_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvsle_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsle_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsle_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsle_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsle_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsle_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsle_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsle_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vsle_bu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsle_bu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsle_hu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsle_hu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsle_wu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsle_wu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vsle_du(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vsle_du_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvsle_bu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsle_bu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsle_hu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsle_hu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsle_wu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsle_wu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvsle_du(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvsle_du_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vslt_b(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vslt_b_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vslt_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vslt_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vslt_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vslt_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vslt_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vslt_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvslt_b(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvslt_b_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvslt_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvslt_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvslt_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvslt_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvslt_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvslt_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vslt_bu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vslt_bu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vslt_hu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vslt_hu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vslt_wu(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vslt_wu_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vslt_du(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vslt_du_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvslt_bu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvslt_bu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvslt_hu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvslt_hu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvslt_wu(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvslt_wu_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvslt_du(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvslt_du_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vslti_bu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vslti_bu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vslti_hu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vslti_hu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vslti_wu(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vslti_wu_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void  vslti_du(FloatRegister vd, FloatRegister vj, int ui5) { ASSERT_LSX  emit_int32(insn_I5RR( vslti_du_op, ui5, (int)vj->encoding(), (int)vd->encoding())); }
  void xvslti_bu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvslti_bu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvslti_hu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvslti_hu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvslti_wu(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvslti_wu_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }
  void xvslti_du(FloatRegister xd, FloatRegister xj, int ui5) { ASSERT_LASX emit_int32(insn_I5RR(xvslti_du_op, ui5, (int)xj->encoding(), (int)xd->encoding())); }

  void  vfcmp_caf_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_caf , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cun_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_cun , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_ceq_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_ceq , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cueq_s (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_cueq, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_clt_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_clt , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cult_s (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_cult, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cle_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_cle , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cule_s (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_cule, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cne_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_cne , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cor_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_cor , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cune_s (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_cune, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_saf_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_saf , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sun_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_sun , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_seq_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_seq , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sueq_s (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_sueq, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_slt_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_slt , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sult_s (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_sult, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sle_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_sle , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sule_s (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_sule, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sne_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_sne , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sor_s  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_sor , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sune_s (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_s_op, fcmp_sune, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }

  void  vfcmp_caf_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_caf , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cun_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_cun , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_ceq_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_ceq , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cueq_d (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_cueq, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_clt_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_clt , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cult_d (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_cult, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cle_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_cle , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cule_d (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_cule, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cne_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_cne , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cor_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_cor , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_cune_d (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_cune, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_saf_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_saf , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sun_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_sun , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_seq_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_seq , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sueq_d (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_sueq, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_slt_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_slt , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sult_d (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_sult, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sle_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_sle , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sule_d (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_sule, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sne_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_sne , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sor_d  (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_sor , (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vfcmp_sune_d (FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRRR( vfcmp_cond_d_op, fcmp_sune, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }

  void xvfcmp_caf_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_caf , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cun_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_cun , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_ceq_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_ceq , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cueq_s (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_cueq, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_clt_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_clt , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cult_s (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_cult, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cle_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_cle , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cule_s (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_cule, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cne_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_cne , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cor_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_cor , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cune_s (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_cune, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_saf_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_saf , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sun_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_sun , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_seq_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_seq , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sueq_s (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_sueq, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_slt_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_slt , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sult_s (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_sult, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sle_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_sle , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sule_s (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_sule, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sne_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_sne , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sor_s  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_sor , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sune_s (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_s_op, fcmp_sune, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void xvfcmp_caf_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_caf , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cun_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_cun , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_ceq_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_ceq , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cueq_d (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_cueq, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_clt_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_clt , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cult_d (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_cult, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cle_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_cle , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cule_d (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_cule, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cne_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_cne , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cor_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_cor , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_cune_d (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_cune, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_saf_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_saf , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sun_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_sun , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_seq_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_seq , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sueq_d (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_sueq, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_slt_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_slt , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sult_d (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_sult, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sle_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_sle , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sule_d (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_sule, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sne_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_sne , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sor_d  (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_sor , (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvfcmp_sune_d (FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRRR(xvfcmp_cond_d_op, fcmp_sune, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vbitsel_v(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vbitsel_v_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvbitsel_v(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvbitsel_v_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vinsgr2vr_b(FloatRegister vd, Register rj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vinsgr2vr_b_op, ui4, (int)rj->encoding(), (int)vd->encoding())); }
  void  vinsgr2vr_h(FloatRegister vd, Register rj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vinsgr2vr_h_op, ui3, (int)rj->encoding(), (int)vd->encoding())); }
  void  vinsgr2vr_w(FloatRegister vd, Register rj, int ui2) { ASSERT_LSX  emit_int32(insn_I2RR( vinsgr2vr_w_op, ui2, (int)rj->encoding(), (int)vd->encoding())); }
  void  vinsgr2vr_d(FloatRegister vd, Register rj, int ui1) { ASSERT_LSX  emit_int32(insn_I1RR( vinsgr2vr_d_op, ui1, (int)rj->encoding(), (int)vd->encoding())); }

  void xvinsgr2vr_w(FloatRegister xd, Register rj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvinsgr2vr_w_op, ui3, (int)rj->encoding(), (int)xd->encoding())); }
  void xvinsgr2vr_d(FloatRegister xd, Register rj, int ui2) { ASSERT_LASX emit_int32(insn_I2RR(xvinsgr2vr_d_op, ui2, (int)rj->encoding(), (int)xd->encoding())); }

  void  vpickve2gr_b(Register rd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vpickve2gr_b_op, ui4, (int)vj->encoding(), (int)rd->encoding())); }
  void  vpickve2gr_h(Register rd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vpickve2gr_h_op, ui3, (int)vj->encoding(), (int)rd->encoding())); }
  void  vpickve2gr_w(Register rd, FloatRegister vj, int ui2) { ASSERT_LSX  emit_int32(insn_I2RR( vpickve2gr_w_op, ui2, (int)vj->encoding(), (int)rd->encoding())); }
  void  vpickve2gr_d(Register rd, FloatRegister vj, int ui1) { ASSERT_LSX  emit_int32(insn_I1RR( vpickve2gr_d_op, ui1, (int)vj->encoding(), (int)rd->encoding())); }

  void  vpickve2gr_bu(Register rd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR( vpickve2gr_bu_op, ui4, (int)vj->encoding(), (int)rd->encoding())); }
  void  vpickve2gr_hu(Register rd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR( vpickve2gr_hu_op, ui3, (int)vj->encoding(), (int)rd->encoding())); }
  void  vpickve2gr_wu(Register rd, FloatRegister vj, int ui2) { ASSERT_LSX  emit_int32(insn_I2RR( vpickve2gr_wu_op, ui2, (int)vj->encoding(), (int)rd->encoding())); }
  void  vpickve2gr_du(Register rd, FloatRegister vj, int ui1) { ASSERT_LSX  emit_int32(insn_I1RR( vpickve2gr_du_op, ui1, (int)vj->encoding(), (int)rd->encoding())); }

  void xvpickve2gr_w(Register rd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvpickve2gr_w_op, ui3, (int)xj->encoding(), (int)rd->encoding())); }
  void xvpickve2gr_d(Register rd, FloatRegister xj, int ui2) { ASSERT_LASX emit_int32(insn_I2RR(xvpickve2gr_d_op, ui2, (int)xj->encoding(), (int)rd->encoding())); }

  void xvpickve2gr_wu(Register rd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvpickve2gr_wu_op, ui3, (int)xj->encoding(), (int)rd->encoding())); }
  void xvpickve2gr_du(Register rd, FloatRegister xj, int ui2) { ASSERT_LASX emit_int32(insn_I2RR(xvpickve2gr_du_op, ui2, (int)xj->encoding(), (int)rd->encoding())); }

  void  vreplgr2vr_b(FloatRegister vd, Register rj) { ASSERT_LSX  emit_int32(insn_RR( vreplgr2vr_b_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vreplgr2vr_h(FloatRegister vd, Register rj) { ASSERT_LSX  emit_int32(insn_RR( vreplgr2vr_h_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vreplgr2vr_w(FloatRegister vd, Register rj) { ASSERT_LSX  emit_int32(insn_RR( vreplgr2vr_w_op, (int)rj->encoding(), (int)vd->encoding())); }
  void  vreplgr2vr_d(FloatRegister vd, Register rj) { ASSERT_LSX  emit_int32(insn_RR( vreplgr2vr_d_op, (int)rj->encoding(), (int)vd->encoding())); }
  void xvreplgr2vr_b(FloatRegister xd, Register rj) { ASSERT_LASX emit_int32(insn_RR(xvreplgr2vr_b_op, (int)rj->encoding(), (int)xd->encoding())); }
  void xvreplgr2vr_h(FloatRegister xd, Register rj) { ASSERT_LASX emit_int32(insn_RR(xvreplgr2vr_h_op, (int)rj->encoding(), (int)xd->encoding())); }
  void xvreplgr2vr_w(FloatRegister xd, Register rj) { ASSERT_LASX emit_int32(insn_RR(xvreplgr2vr_w_op, (int)rj->encoding(), (int)xd->encoding())); }
  void xvreplgr2vr_d(FloatRegister xd, Register rj) { ASSERT_LASX emit_int32(insn_RR(xvreplgr2vr_d_op, (int)rj->encoding(), (int)xd->encoding())); }

  void  vreplvei_b(FloatRegister vd, FloatRegister vj, int ui4) { ASSERT_LSX  emit_int32(insn_I4RR(vreplvei_b_op, ui4, (int)vj->encoding(), (int)vd->encoding())); }
  void  vreplvei_h(FloatRegister vd, FloatRegister vj, int ui3) { ASSERT_LSX  emit_int32(insn_I3RR(vreplvei_h_op, ui3, (int)vj->encoding(), (int)vd->encoding())); }
  void  vreplvei_w(FloatRegister vd, FloatRegister vj, int ui2) { ASSERT_LSX  emit_int32(insn_I2RR(vreplvei_w_op, ui2, (int)vj->encoding(), (int)vd->encoding())); }
  void  vreplvei_d(FloatRegister vd, FloatRegister vj, int ui1) { ASSERT_LSX  emit_int32(insn_I1RR(vreplvei_d_op, ui1, (int)vj->encoding(), (int)vd->encoding())); }

  void xvreplve0_b(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvreplve0_b_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvreplve0_h(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvreplve0_h_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvreplve0_w(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvreplve0_w_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvreplve0_d(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvreplve0_d_op, (int)xj->encoding(), (int)xd->encoding())); }
  void xvreplve0_q(FloatRegister xd, FloatRegister xj) { ASSERT_LASX emit_int32(insn_RR(xvreplve0_q_op, (int)xj->encoding(), (int)xd->encoding())); }

  void xvinsve0_w(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvinsve0_w_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvinsve0_d(FloatRegister xd, FloatRegister xj, int ui2) { ASSERT_LASX emit_int32(insn_I2RR(xvinsve0_d_op, ui2, (int)xj->encoding(), (int)xd->encoding())); }

  void xvpickve_w(FloatRegister xd, FloatRegister xj, int ui3) { ASSERT_LASX emit_int32(insn_I3RR(xvpickve_w_op, ui3, (int)xj->encoding(), (int)xd->encoding())); }
  void xvpickve_d(FloatRegister xd, FloatRegister xj, int ui2) { ASSERT_LASX emit_int32(insn_I2RR(xvpickve_d_op, ui2, (int)xj->encoding(), (int)xd->encoding())); }

  void  vshuf_b(FloatRegister vd, FloatRegister vj, FloatRegister vk, FloatRegister va) { ASSERT_LSX  emit_int32(insn_RRRR( vshuf_b_op, (int)va->encoding(), (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void xvshuf_b(FloatRegister xd, FloatRegister xj, FloatRegister xk, FloatRegister xa) { ASSERT_LASX emit_int32(insn_RRRR(xvshuf_b_op, (int)xa->encoding(), (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vshuf_h(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vshuf_h_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vshuf_w(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vshuf_w_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }
  void  vshuf_d(FloatRegister vd, FloatRegister vj, FloatRegister vk) { ASSERT_LSX  emit_int32(insn_RRR( vshuf_d_op, (int)vk->encoding(), (int)vj->encoding(), (int)vd->encoding())); }

  void xvshuf_h(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvshuf_h_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvshuf_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvshuf_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }
  void xvshuf_d(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvshuf_d_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void xvperm_w(FloatRegister xd, FloatRegister xj, FloatRegister xk) { ASSERT_LASX emit_int32(insn_RRR(xvperm_w_op, (int)xk->encoding(), (int)xj->encoding(), (int)xd->encoding())); }

  void  vshuf4i_b(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vshuf4i_b_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void  vshuf4i_h(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vshuf4i_h_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void  vshuf4i_w(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vshuf4i_w_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void xvshuf4i_b(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvshuf4i_b_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }
  void xvshuf4i_h(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvshuf4i_h_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }
  void xvshuf4i_w(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvshuf4i_w_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void  vshuf4i_d(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vshuf4i_d_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void xvshuf4i_d(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvshuf4i_d_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void  vpermi_w(FloatRegister vd, FloatRegister vj, int ui8) { ASSERT_LSX  assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR( vpermi_w_op, ui8, (int)vj->encoding(), (int)vd->encoding())); }
  void xvpermi_w(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvpermi_w_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void xvpermi_d(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvpermi_d_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void xvpermi_q(FloatRegister xd, FloatRegister xj, int ui8) { ASSERT_LASX assert(is_uimm(ui8, 8), "not a unsigned 8-bit int");  emit_int32(insn_I8RR(xvpermi_q_op, ui8, (int)xj->encoding(), (int)xd->encoding())); }

  void  vld(FloatRegister vd, Register rj, int si12) { ASSERT_LSX  assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR( vld_op, si12, (int)rj->encoding(), (int)vd->encoding()));}
  void xvld(FloatRegister xd, Register rj, int si12) { ASSERT_LASX assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(xvld_op, si12, (int)rj->encoding(), (int)xd->encoding()));}

  void  vst(FloatRegister vd, Register rj, int si12) { ASSERT_LSX  assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR( vst_op, si12, (int)rj->encoding(), (int)vd->encoding()));}
  void xvst(FloatRegister xd, Register rj, int si12) { ASSERT_LASX assert(is_simm(si12, 12), "not a signed 12-bit int"); emit_int32(insn_I12RR(xvst_op, si12, (int)rj->encoding(), (int)xd->encoding()));}

  void  vldx(FloatRegister vd, Register rj, Register rk) { ASSERT_LSX  emit_int32(insn_RRR( vldx_op, (int)rk->encoding(), (int)rj->encoding(), (int)vd->encoding())); }
  void xvldx(FloatRegister xd, Register rj, Register rk) { ASSERT_LASX emit_int32(insn_RRR(xvldx_op, (int)rk->encoding(), (int)rj->encoding(), (int)xd->encoding())); }

  void  vstx(FloatRegister vd, Register rj, Register rk) { ASSERT_LSX  emit_int32(insn_RRR( vstx_op, (int)rk->encoding(), (int)rj->encoding(), (int)vd->encoding())); }
  void xvstx(FloatRegister xd, Register rj, Register rk) { ASSERT_LASX emit_int32(insn_RRR(xvstx_op, (int)rk->encoding(), (int)rj->encoding(), (int)xd->encoding())); }

#undef ASSERT_LSX
#undef ASSERT_LASX

public:
  // Creation
  Assembler(CodeBuffer* code) : AbstractAssembler(code) {}

  // Decoding
  static address locate_operand(address inst, WhichOperand which);
  static address locate_next_instruction(address inst);
};

#endif // CPU_LOONGARCH_VM_ASSEMBLER_LOONGARCH_HPP
