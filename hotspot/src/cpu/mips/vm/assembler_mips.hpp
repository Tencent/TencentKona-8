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

#ifndef CPU_MIPS_VM_ASSEMBLER_MIPS_HPP
#define CPU_MIPS_VM_ASSEMBLER_MIPS_HPP

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
 private:
  int _number;
 public:
  enum {
    n_register_parameters = 8,   // 8 integer registers used to pass parameters
    n_float_register_parameters = 8   // 8 float registers used to pass parameters
  };

  Argument(int number):_number(number){ }
  Argument successor() {return Argument(number() + 1);}

  int number()const {return _number;}
  bool is_Register()const {return _number < n_register_parameters;}
  bool is_FloatRegister()const {return _number < n_float_register_parameters;}

  Register as_Register()const {
    assert(is_Register(), "must be a register argument");
    return ::as_Register(RA0->encoding() + _number);
  }
  FloatRegister  as_FloatRegister()const {
    assert(is_FloatRegister(), "must be a float register argument");
    return ::as_FloatRegister(F12->encoding() + _number);
  }

  Address as_caller_address()const {return Address(SP, (number() - n_register_parameters) * wordSize);}
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

const int FPUStateSizeInWords = 512 / wordSize;

// The MIPS LOONGSON Assembler: Pure assembler doing NO optimizations on the instruction
// level ; i.e., what you write is what you get. The Assembler is generating code into
// a CodeBuffer.

class Assembler : public AbstractAssembler  {
  friend class AbstractAssembler; // for the non-virtual hack
  friend class LIR_Assembler; // as_Address()
  friend class StubGenerator;

 public:
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

  // opcode, highest 6 bits: bits[31...26]
  enum ops {
    special_op  = 0x00, // special_ops
    regimm_op   = 0x01, // regimm_ops
    j_op        = 0x02,
    jal_op      = 0x03,
    beq_op      = 0x04,
    bne_op      = 0x05,
    blez_op     = 0x06,
    bgtz_op     = 0x07,
    addiu_op    = 0x09,
    slti_op     = 0x0a,
    sltiu_op    = 0x0b,
    andi_op     = 0x0c,
    ori_op      = 0x0d,
    xori_op     = 0x0e,
    lui_op      = 0x0f,
    cop0_op     = 0x10, // cop0_ops
    cop1_op     = 0x11, // cop1_ops
    gs_cop2_op  = 0x12, // gs_cop2_ops
    cop1x_op    = 0x13, // cop1x_ops
    beql_op     = 0x14,
    bnel_op     = 0x15,
    blezl_op    = 0x16,
    bgtzl_op    = 0x17,
    daddiu_op   = 0x19,
    ldl_op      = 0x1a,
    ldr_op      = 0x1b,
    special2_op = 0x1c, // special2_ops
    msa_op      = 0x1e, // msa_ops
    special3_op = 0x1f, // special3_ops
    lb_op       = 0x20,
    lh_op       = 0x21,
    lwl_op      = 0x22,
    lw_op       = 0x23,
    lbu_op      = 0x24,
    lhu_op      = 0x25,
    lwr_op      = 0x26,
    lwu_op      = 0x27,
    sb_op       = 0x28,
    sh_op       = 0x29,
    swl_op      = 0x2a,
    sw_op       = 0x2b,
    sdl_op      = 0x2c,
    sdr_op      = 0x2d,
    swr_op      = 0x2e,
    cache_op    = 0x2f,
    ll_op       = 0x30,
    lwc1_op     = 0x31,
    gs_lwc2_op  = 0x32, //gs_lwc2_ops
    pref_op     = 0x33,
    lld_op      = 0x34,
    ldc1_op     = 0x35,
    gs_ldc2_op  = 0x36, //gs_ldc2_ops
    ld_op       = 0x37,
    sc_op       = 0x38,
    swc1_op     = 0x39,
    gs_swc2_op  = 0x3a, //gs_swc2_ops
    scd_op      = 0x3c,
    sdc1_op     = 0x3d,
    gs_sdc2_op  = 0x3e, //gs_sdc2_ops
    sd_op       = 0x3f
  };

  static  const char *ops_name[];

  //special family, the opcode is in low 6 bits.
  enum special_ops {
    sll_op       = 0x00,
    movci_op     = 0x01,
    srl_op       = 0x02,
    sra_op       = 0x03,
    sllv_op      = 0x04,
    srlv_op      = 0x06,
    srav_op      = 0x07,
    jr_op        = 0x08,
    jalr_op      = 0x09,
    movz_op      = 0x0a,
    movn_op      = 0x0b,
    syscall_op   = 0x0c,
    break_op     = 0x0d,
    sync_op      = 0x0f,
    mfhi_op      = 0x10,
    mthi_op      = 0x11,
    mflo_op      = 0x12,
    mtlo_op      = 0x13,
    dsllv_op     = 0x14,
    dsrlv_op     = 0x16,
    dsrav_op     = 0x17,
    mult_op      = 0x18,
    multu_op     = 0x19,
    div_op       = 0x1a,
    divu_op      = 0x1b,
    dmult_op     = 0x1c,
    dmultu_op    = 0x1d,
    ddiv_op      = 0x1e,
    ddivu_op     = 0x1f,
    addu_op      = 0x21,
    subu_op      = 0x23,
    and_op       = 0x24,
    or_op        = 0x25,
    xor_op       = 0x26,
    nor_op       = 0x27,
    slt_op       = 0x2a,
    sltu_op      = 0x2b,
    daddu_op     = 0x2d,
    dsubu_op     = 0x2f,
    tge_op       = 0x30,
    tgeu_op      = 0x31,
    tlt_op       = 0x32,
    tltu_op      = 0x33,
    teq_op       = 0x34,
    tne_op       = 0x36,
    dsll_op      = 0x38,
    dsrl_op      = 0x3a,
    dsra_op      = 0x3b,
    dsll32_op    = 0x3c,
    dsrl32_op    = 0x3e,
    dsra32_op    = 0x3f
  };

  static  const char* special_name[];

  //regimm family, the opcode is in rt[16...20], 5 bits
  enum regimm_ops {
    bltz_op      = 0x00,
    bgez_op      = 0x01,
    bltzl_op     = 0x02,
    bgezl_op     = 0x03,
    tgei_op      = 0x08,
    tgeiu_op     = 0x09,
    tlti_op      = 0x0a,
    tltiu_op     = 0x0b,
    teqi_op      = 0x0c,
    tnei_op      = 0x0e,
    bltzal_op    = 0x10,
    bgezal_op    = 0x11,
    bltzall_op   = 0x12,
    bgezall_op   = 0x13,
    bposge32_op  = 0x1c,
    bposge64_op  = 0x1d,
    synci_op     = 0x1f,
  };

  static  const char* regimm_name[];

  //cop0 family, the ops is in bits[25...21], 5 bits
  enum cop0_ops {
    mfc0_op     = 0x00,
    dmfc0_op    = 0x01,
    //
    mxgc0_op    = 0x03, //MFGC0, DMFGC0, MTGC0
    mtc0_op     = 0x04,
    dmtc0_op    = 0x05,
    rdpgpr_op   = 0x0a,
    inter_op    = 0x0b,
    wrpgpr_op   = 0x0c
  };

  //cop1 family, the ops is in bits[25...21], 5 bits
  enum cop1_ops {
    mfc1_op     = 0x00,
    dmfc1_op    = 0x01,
    cfc1_op     = 0x02,
    mfhc1_op    = 0x03,
    mtc1_op     = 0x04,
    dmtc1_op    = 0x05,
    ctc1_op     = 0x06,
    mthc1_op    = 0x07,
    bc1f_op     = 0x08,
    single_fmt  = 0x10,
    double_fmt  = 0x11,
    word_fmt    = 0x14,
    long_fmt    = 0x15,
    ps_fmt      = 0x16
  };


  //2 bist (bits[17...16]) of bc1x instructions (cop1)
  enum bc_ops {
    bcf_op       = 0x0,
    bct_op       = 0x1,
    bcfl_op      = 0x2,
    bctl_op      = 0x3,
  };

  // low 6 bits of c_x_fmt instructions (cop1)
  enum c_conds {
    f_cond       = 0x30,
    un_cond      = 0x31,
    eq_cond      = 0x32,
    ueq_cond     = 0x33,
    olt_cond     = 0x34,
    ult_cond     = 0x35,
    ole_cond     = 0x36,
    ule_cond     = 0x37,
    sf_cond      = 0x38,
    ngle_cond    = 0x39,
    seq_cond     = 0x3a,
    ngl_cond     = 0x3b,
    lt_cond      = 0x3c,
    nge_cond     = 0x3d,
    le_cond      = 0x3e,
    ngt_cond     = 0x3f
  };

  // low 6 bits of cop1 instructions
  enum float_ops {
    fadd_op      = 0x00,
    fsub_op      = 0x01,
    fmul_op      = 0x02,
    fdiv_op      = 0x03,
    fsqrt_op     = 0x04,
    fabs_op      = 0x05,
    fmov_op      = 0x06,
    fneg_op      = 0x07,
    froundl_op   = 0x08,
    ftruncl_op   = 0x09,
    fceill_op    = 0x0a,
    ffloorl_op   = 0x0b,
    froundw_op   = 0x0c,
    ftruncw_op   = 0x0d,
    fceilw_op    = 0x0e,
    ffloorw_op   = 0x0f,
    movf_f_op    = 0x11,
    movt_f_op    = 0x11,
    movz_f_op    = 0x12,
    movn_f_op    = 0x13,
    frecip_op    = 0x15,
    frsqrt_op    = 0x16,
    fcvts_op     = 0x20,
    fcvtd_op     = 0x21,
    fcvtw_op     = 0x24,
    fcvtl_op     = 0x25,
    fcvtps_op    = 0x26,
    fcvtspl_op   = 0x28,
    fpll_op      = 0x2c,
    fplu_op      = 0x2d,
    fpul_op      = 0x2e,
    fpuu_op      = 0x2f
  };

  static const char* cop1_name[];

  //cop1x family, the opcode is in low 6 bits.
  enum cop1x_ops {
    lwxc1_op    = 0x00,
    ldxc1_op    = 0x01,
    luxc1_op    = 0x05,
    swxc1_op    = 0x08,
    sdxc1_op    = 0x09,
    suxc1_op    = 0x0d,
    prefx_op    = 0x0f,

    alnv_ps_op  = 0x1e,
    madd_s_op   = 0x20,
    madd_d_op   = 0x21,
    madd_ps_op  = 0x26,
    msub_s_op   = 0x28,
    msub_d_op   = 0x29,
    msub_ps_op  = 0x2e,
    nmadd_s_op  = 0x30,
    nmadd_d_op  = 0x31,
    nmadd_ps_op = 0x36,
    nmsub_s_op  = 0x38,
    nmsub_d_op  = 0x39,
    nmsub_ps_op = 0x3e
  };

  static const char* cop1x_name[];

  //special2 family, the opcode is in low 6 bits.
  enum special2_ops {
    madd_op       = 0x00,
    maddu_op      = 0x01,
    mul_op        = 0x02,
    gs0x03_op     = 0x03,
    msub_op       = 0x04,
    msubu_op      = 0x05,
    gs0x06_op     = 0x06,
    gsemul2_op    = 0x07,
    gsemul3_op    = 0x08,
    gsemul4_op    = 0x09,
    gsemul5_op    = 0x0a,
    gsemul6_op    = 0x0b,
    gsemul7_op    = 0x0c,
    gsemul8_op    = 0x0d,
    gsemul9_op    = 0x0e,
    gsemul10_op   = 0x0f,
    gsmult_op     = 0x10,
    gsdmult_op    = 0x11,
    gsmultu_op    = 0x12,
    gsdmultu_op   = 0x13,
    gsdiv_op      = 0x14,
    gsddiv_op     = 0x15,
    gsdivu_op     = 0x16,
    gsddivu_op    = 0x17,
    gsmod_op      = 0x1c,
    gsdmod_op     = 0x1d,
    gsmodu_op     = 0x1e,
    gsdmodu_op    = 0x1f,
    clz_op        = 0x20,
    clo_op        = 0x21,
    xctx_op       = 0x22, //ctz, cto, dctz, dcto, gsX
    gsrxr_x_op    = 0x23, //gsX
    dclz_op       = 0x24,
    dclo_op       = 0x25,
    gsle_op       = 0x26,
    gsgt_op       = 0x27,
    gs86j_op      = 0x28,
    gsloop_op     = 0x29,
    gsaj_op       = 0x2a,
    gsldpc_op     = 0x2b,
    gs86set_op    = 0x30,
    gstm_op       = 0x31,
    gscvt_ld_op   = 0x32,
    gscvt_ud_op   = 0x33,
    gseflag_op    = 0x34,
    gscam_op      = 0x35,
    gstop_op      = 0x36,
    gssettag_op   = 0x37,
    gssdbbp_op    = 0x38
  };

  static  const char* special2_name[];

  // special3 family, the opcode is in low 6 bits.
  enum special3_ops {
    ext_op         = 0x00,
    dextm_op       = 0x01,
    dextu_op       = 0x02,
    dext_op        = 0x03,
    ins_op         = 0x04,
    dinsm_op       = 0x05,
    dinsu_op       = 0x06,
    dins_op        = 0x07,
    lxx_op         = 0x0a, //lwx, lhx, lbux, ldx
    insv_op        = 0x0c,
    dinsv_op       = 0x0d,
    ar1_op         = 0x10, //MIPS DSP
    cmp1_op        = 0x11, //MIPS DSP
    re1_op         = 0x12, //MIPS DSP, re1_ops
    sh1_op         = 0x13, //MIPS DSP
    ar2_op         = 0x14, //MIPS DSP
    cmp2_op        = 0x15, //MIPS DSP
    re2_op         = 0x16, //MIPS DSP, re2_ops
    sh2_op         = 0x17, //MIPS DSP
    ar3_op         = 0x18, //MIPS DSP
    bshfl_op       = 0x20  //seb, seh
  };

  // re1_ops
  enum re1_ops {
    absq_s_qb_op = 0x01,
    repl_qb_op   = 0x02,
    replv_qb_op  = 0x03,
    absq_s_ph_op = 0x09,
    repl_ph_op   = 0x0a,
    replv_ph_op  = 0x0b,
    absq_s_w_op  = 0x11,
    bitrev_op    = 0x1b
  };

  // re2_ops
  enum re2_ops {
    repl_ob_op   = 0x02,
    replv_ob_op  = 0x03,
    absq_s_qh_op = 0x09,
    repl_qh_op   = 0x0a,
    replv_qh_op  = 0x0b,
    absq_s_pw_op = 0x11,
    repl_pw_op   = 0x12,
    replv_pw_op  = 0x13,
  };

  static  const char* special3_name[];

  // lwc2/gs_lwc2 family, the opcode is in low 6 bits.
  enum gs_lwc2_ops {
    gslble_op       = 0x10,
    gslbgt_op       = 0x11,
    gslhle_op       = 0x12,
    gslhgt_op       = 0x13,
    gslwle_op       = 0x14,
    gslwgt_op       = 0x15,
    gsldle_op       = 0x16,
    gsldgt_op       = 0x17,
    gslwlec1_op     = 0x1c,
    gslwgtc1_op     = 0x1d,
    gsldlec1_op     = 0x1e,
    gsldgtc1_op     = 0x1f,
    gslq_op         = 0x20
  };

  static const char* gs_lwc2_name[];

  // ldc2/gs_ldc2 family, the opcode is in low 3 bits.
  enum gs_ldc2_ops {
    gslbx_op        =  0x0,
    gslhx_op        =  0x1,
    gslwx_op        =  0x2,
    gsldx_op        =  0x3,
    gslwxc1_op      =  0x6,
    gsldxc1_op      =  0x7
  };

  static const char* gs_ldc2_name[];

  // swc2/gs_swc2 family, the opcode is in low 6 bits.
  enum gs_swc2_ops {
    gssble_op       = 0x10,
    gssbgt_op       = 0x11,
    gsshle_op       = 0x12,
    gsshgt_op       = 0x13,
    gsswle_op       = 0x14,
    gsswgt_op       = 0x15,
    gssdle_op       = 0x16,
    gssdgt_op       = 0x17,
    gsswlec1_op     = 0x1c,
    gsswgtc1_op     = 0x1d,
    gssdlec1_op     = 0x1e,
    gssdgtc1_op     = 0x1f,
    gssq_op         = 0x20
  };

  static const char* gs_swc2_name[];

  // sdc2/gs_sdc2 family, the opcode is in low 3 bits.
  enum gs_sdc2_ops {
    gssbx_op        =  0x0,
    gsshx_op        =  0x1,
    gsswx_op        =  0x2,
    gssdx_op        =  0x3,
    gsswxc1_op      =  0x6,
    gssdxc1_op      =  0x7
  };

  static const char* gs_sdc2_name[];

  enum WhichOperand {
    // input to locate_operand, and format code for relocations
    imm_operand  = 0,            // embedded 32-bit|64-bit immediate operand
    disp32_operand = 1,          // embedded 32-bit displacement or address
    call32_operand = 2,          // embedded 32-bit self-relative displacement
    narrow_oop_operand = 3,      // embedded 32-bit immediate narrow oop
    _WhichOperand_limit = 4
  };

  static int opcode(int insn) { return (insn>>26)&0x3f; }
  static int rs(int insn) { return (insn>>21)&0x1f; }
  static int rt(int insn) { return (insn>>16)&0x1f; }
  static int rd(int insn) { return (insn>>11)&0x1f; }
  static int sa(int insn) { return (insn>>6)&0x1f; }
  static int special(int insn) { return insn&0x3f; }
  static int imm_off(int insn) { return (short)low16(insn); }

  static int low  (int x, int l) { return bitfield(x, 0, l); }
  static int low16(int x)        { return low(x, 16); }
  static int low26(int x)        { return low(x, 26); }

 protected:
  //help methods for instruction ejection

  // I-Type (Immediate)
  // 31        26 25        21 20      16 15                              0
  //|   opcode   |      rs    |    rt    |            immediat             |
  //|            |            |          |                                 |
  //      6              5          5                     16
  static int insn_ORRI(int op, int rs, int rt, int imm) { assert(is_simm16(imm), "not a signed 16-bit int"); return (op<<26) | (rs<<21) | (rt<<16) | low16(imm); }

  // R-Type (Register)
  // 31         26 25        21 20      16 15      11 10         6 5         0
  //|   special   |      rs    |    rt    |    rd    |     0      |   opcode  |
  //| 0 0 0 0 0 0 |            |          |          | 0 0 0 0 0  |           |
  //      6              5          5           5          5            6
  static int insn_RRRO(int rs, int rt, int rd,   int op) { return (rs<<21) | (rt<<16) | (rd<<11)  | op; }
  static int insn_RRSO(int rt, int rd, int sa,   int op) { return (rt<<16) | (rd<<11) | (sa<<6)   | op; }
  static int insn_RRCO(int rs, int rt, int code, int op) { return (rs<<21) | (rt<<16) | (code<<6) | op; }

  static int insn_COP0(int op, int rt, int rd) { return (cop0_op<<26) | (op<<21) | (rt<<16) | (rd<<11); }
  static int insn_COP1(int op, int rt, int fs) { return (cop1_op<<26) | (op<<21) | (rt<<16) | (fs<<11); }

  static int insn_F3RO(int fmt, int ft, int fs, int fd, int func) {
    return (cop1_op<<26) | (fmt<<21) | (ft<<16) | (fs<<11) | (fd<<6) | func;
  }
  static int insn_F3ROX(int fmt, int ft, int fs, int fd, int func) {
    return (cop1x_op<<26) | (fmt<<21) | (ft<<16) | (fs<<11) | (fd<<6) | func;
  }

  static int high  (int x, int l) { return bitfield(x, 32-l, l); }
  static int high16(int x)        { return high(x, 16); }
  static int high6 (int x)        { return high(x, 6); }

  //get the offset field of jump/branch instruction
  int offset(address entry) {
    assert(is_simm16((entry - pc() - 4) / 4), "change this code");
    if (!is_simm16((entry - pc() - 4) / 4)) {
      tty->print_cr("!!! is_simm16: %lx", (entry - pc() - 4) / 4);
    }
    return (entry - pc() - 4) / 4;
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

  // MIPS lui/addiu is both sign extended, so if you wan't to use off32/imm32, you have to use the follow three
  static int split_low(int x) {
    return (x & 0xffff);
  }

  // Convert 16-bit x to a sign-extended 16-bit integer
  static int simm16(int x) {
    assert(x == (x & 0xFFFF), "must be 16-bit only");
    return (x << 16) >> 16;
  }

  static int split_high(int x) {
    return ( (x >> 16) + ((x & 0x8000) != 0) ) & 0xffff;
  }

  static int merge(int low, int high) {
    return expand(low, 15) + (high<<16);
  }

  static intptr_t merge(intptr_t x0, intptr_t x16, intptr_t x32, intptr_t x48) {
    return (x48 << 48) | (x32 << 32) | (x16 << 16) | x0;
  }

  // Test if x is within signed immediate range for nbits.
  static bool is_simm  (int x, int nbits) {
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

  static bool is_simm16(int x)            { return is_simm(x, 16); }
  static bool is_simm16(long x)           { return is_simm((jlong)x, (unsigned int)16); }

  static bool fit_in_jal(address target, address pc) {
    intptr_t mask = 0xfffffffff0000000;
    return ((intptr_t)(pc + 4) & mask) == ((intptr_t)target & mask);
  }

  bool fit_int_branch(address entry) {
    return is_simm16(offset(entry));
  }

protected:
#ifdef ASSERT
    #define CHECK_DELAY
#endif
#ifdef CHECK_DELAY
  enum Delay_state { no_delay, at_delay_slot, filling_delay_slot } delay_state;
#endif

public:
  void assert_not_delayed() {
#ifdef CHECK_DELAY
    assert_not_delayed("next instruction should not be a delay slot");
#endif
  }

  void assert_not_delayed(const char* msg) {
#ifdef CHECK_DELAY
    assert(delay_state == no_delay, msg);
#endif
  }

protected:
  // Delay slot helpers
  // cti is called when emitting control-transfer instruction,
  // BEFORE doing the emitting.
  // Only effective when assertion-checking is enabled.

  // called when emitting cti with a delay slot, AFTER emitting
  void has_delay_slot() {
#ifdef CHECK_DELAY
    assert_not_delayed("just checking");
    delay_state = at_delay_slot;
#endif
  }

public:
  Assembler* delayed() {
#ifdef CHECK_DELAY
    guarantee( delay_state == at_delay_slot, "delayed instructition is not in delay slot");
    delay_state = filling_delay_slot;
#endif
    return this;
  }

  void flush() {
#ifdef CHECK_DELAY
    guarantee( delay_state == no_delay, "ending code with a delay slot");
#endif
    AbstractAssembler::flush();
  }

  inline void emit_long(int);  // shadows AbstractAssembler::emit_long
  inline void emit_data(int x) { emit_long(x); }
  inline void emit_data(int, RelocationHolder const&);
  inline void emit_data(int, relocInfo::relocType rtype);
  inline void check_delay();


  // Generic instructions
  // Does 32bit or 64bit as needed for the platform. In some sense these
  // belong in macro assembler but there is no need for both varieties to exist

  void addu32(Register rd, Register rs, Register rt){ emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), addu_op)); }
  void addiu32(Register rt, Register rs, int imm)   { emit_long(insn_ORRI(addiu_op, (int)rs->encoding(), (int)rt->encoding(), imm)); }
  void addiu(Register rt, Register rs, int imm)     { daddiu (rt, rs, imm);}
  void addu(Register rd, Register rs, Register rt)  { daddu  (rd, rs, rt);  }

  void andr(Register rd, Register rs, Register rt) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), and_op)); }
  void andi(Register rt, Register rs, int imm)     { emit_long(insn_ORRI(andi_op, (int)rs->encoding(), (int)rt->encoding(), simm16(imm))); }

  void beq    (Register rs, Register rt, int off)  { emit_long(insn_ORRI(beq_op, (int)rs->encoding(), (int)rt->encoding(), off)); has_delay_slot(); }
  void beql   (Register rs, Register rt, int off)  { emit_long(insn_ORRI(beql_op, (int)rs->encoding(), (int)rt->encoding(), off)); has_delay_slot(); }
  void bgez   (Register rs, int off) { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), bgez_op, off)); has_delay_slot(); }
  void bgezal (Register rs, int off) { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), bgezal_op, off)); has_delay_slot(); }
  void bgezall(Register rs, int off) { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), bgezall_op, off)); has_delay_slot(); }
  void bgezl  (Register rs, int off) { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), bgezl_op, off)); has_delay_slot(); }
  void bgtz   (Register rs, int off) { emit_long(insn_ORRI(bgtz_op,   (int)rs->encoding(), 0, off)); has_delay_slot(); }
  void bgtzl  (Register rs, int off) { emit_long(insn_ORRI(bgtzl_op,  (int)rs->encoding(), 0, off)); has_delay_slot(); }
  void blez   (Register rs, int off) { emit_long(insn_ORRI(blez_op,   (int)rs->encoding(), 0, off)); has_delay_slot(); }
  void blezl  (Register rs, int off) { emit_long(insn_ORRI(blezl_op,  (int)rs->encoding(), 0, off)); has_delay_slot(); }
  void bltz   (Register rs, int off) { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), bltz_op, off)); has_delay_slot(); }
  void bltzal (Register rs, int off) { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), bltzal_op, off)); has_delay_slot(); }
  void bltzall(Register rs, int off) { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), bltzall_op, off)); has_delay_slot(); }
  void bltzl  (Register rs, int off) { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), bltzl_op, off)); has_delay_slot(); }
  void bne    (Register rs, Register rt, int off) { emit_long(insn_ORRI(bne_op,  (int)rs->encoding(), (int)rt->encoding(), off)); has_delay_slot(); }
  void bnel   (Register rs, Register rt, int off) { emit_long(insn_ORRI(bnel_op, (int)rs->encoding(), (int)rt->encoding(), off)); has_delay_slot(); }
  // two versions of brk:
  // the brk(code) version is according to MIPS64 Architecture For Programmers Volume II: The MIPS64 Instruction Set
  // the brk(code1, code2) is according to disassembler of hsdis (binutils-2.27)
  // both versions work
  void brk    (int code) { assert(is_uimm(code, 20), "code is 20 bits"); emit_long( (low(code, 20)<<6) | break_op ); }
  void brk    (int code1, int code2) { assert(is_uimm(code1, 10) && is_uimm(code2, 10), "code is 20 bits"); emit_long( (low(code1, 10)<<16) | (low(code2, 10)<<6) | break_op ); }

  void beq    (Register rs, Register rt, address entry) { beq(rs, rt, offset(entry)); }
  void beql   (Register rs, Register rt, address entry) { beql(rs, rt, offset(entry));}
  void bgez   (Register rs, address entry) { bgez   (rs, offset(entry)); }
  void bgezal (Register rs, address entry) { bgezal (rs, offset(entry)); }
  void bgezall(Register rs, address entry) { bgezall(rs, offset(entry)); }
  void bgezl  (Register rs, address entry) { bgezl  (rs, offset(entry)); }
  void bgtz   (Register rs, address entry) { bgtz   (rs, offset(entry)); }
  void bgtzl  (Register rs, address entry) { bgtzl  (rs, offset(entry)); }
  void blez   (Register rs, address entry) { blez   (rs, offset(entry)); }
  void blezl  (Register rs, address entry) { blezl  (rs, offset(entry)); }
  void bltz   (Register rs, address entry) { bltz   (rs, offset(entry)); }
  void bltzal (Register rs, address entry) { bltzal (rs, offset(entry)); }
  void bltzall(Register rs, address entry) { bltzall(rs, offset(entry)); }
  void bltzl  (Register rs, address entry) { bltzl  (rs, offset(entry)); }
  void bne    (Register rs, Register rt, address entry) { bne(rs, rt, offset(entry)); }
  void bnel   (Register rs, Register rt, address entry) { bnel(rs, rt, offset(entry)); }

  void beq    (Register rs, Register rt, Label& L) { beq(rs, rt, target(L)); }
  void beql   (Register rs, Register rt, Label& L) { beql(rs, rt, target(L)); }
  void bgez   (Register rs, Label& L){ bgez   (rs, target(L)); }
  void bgezal (Register rs, Label& L){ bgezal (rs, target(L)); }
  void bgezall(Register rs, Label& L){ bgezall(rs, target(L)); }
  void bgezl  (Register rs, Label& L){ bgezl  (rs, target(L)); }
  void bgtz   (Register rs, Label& L){ bgtz   (rs, target(L)); }
  void bgtzl  (Register rs, Label& L){ bgtzl  (rs, target(L)); }
  void blez   (Register rs, Label& L){ blez   (rs, target(L)); }
  void blezl  (Register rs, Label& L){ blezl  (rs, target(L)); }
  void bltz   (Register rs, Label& L){ bltz   (rs, target(L)); }
  void bltzal (Register rs, Label& L){ bltzal (rs, target(L)); }
  void bltzall(Register rs, Label& L){ bltzall(rs, target(L)); }
  void bltzl  (Register rs, Label& L){ bltzl  (rs, target(L)); }
  void bne    (Register rs, Register rt, Label& L){ bne(rs, rt, target(L)); }
  void bnel   (Register rs, Register rt, Label& L){ bnel(rs, rt, target(L)); }

  void daddiu(Register rt, Register rs, int imm)     { emit_long(insn_ORRI(daddiu_op, (int)rs->encoding(), (int)rt->encoding(), imm)); }
  void daddu (Register rd, Register rs, Register rt) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), daddu_op)); }
  void ddiv  (Register rs, Register rt)              { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), 0, ddiv_op));  }
  void ddivu (Register rs, Register rt)              { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), 0, ddivu_op)); }

  void movz  (Register rd, Register rs,   Register rt) { emit_long(insn_RRRO((int)rs->encoding(),  (int)rt->encoding(),   (int)rd->encoding(), movz_op)); }
  void movn  (Register rd, Register rs,   Register rt) { emit_long(insn_RRRO((int)rs->encoding(),  (int)rt->encoding(),   (int)rd->encoding(), movn_op)); }

  void movt  (Register rd, Register rs) { emit_long(((int)rs->encoding() << 21) | (1 << 16) | ((int)rd->encoding() << 11) | movci_op); }
  void movf  (Register rd, Register rs) { emit_long(((int)rs->encoding() << 21) | ((int)rd->encoding() << 11) | movci_op); }

  enum bshfl_ops {
     seb_op = 0x10,
     seh_op = 0x18
  };
  void seb  (Register rd, Register rt) { emit_long((special3_op << 26) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | (seb_op << 6) | bshfl_op); }
  void seh  (Register rd, Register rt) { emit_long((special3_op << 26) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | (seh_op << 6) | bshfl_op); }

  void ext  (Register rt, Register rs, int pos, int size) {
     guarantee((0 <= pos) && (pos < 32), "pos must be in [0, 32)");
     guarantee((0 < size) && (size <= 32), "size must be in (0, 32]");
     guarantee((0 < pos + size) && (pos + size <= 32), "pos + size must be in (0, 32]");

     int lsb  = pos;
     int msbd = size - 1;

     emit_long((special3_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | (msbd << 11) | (lsb << 6) | ext_op);
  }

  void dext  (Register rt, Register rs, int pos, int size) {
     guarantee((0 <= pos) && (pos < 32), "pos must be in [0, 32)");
     guarantee((0 < size) && (size <= 32), "size must be in (0, 32]");
     guarantee((0 < pos + size) && (pos + size <= 63), "pos + size must be in (0, 63]");

     int lsb  = pos;
     int msbd = size - 1;

     emit_long((special3_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | (msbd << 11) | (lsb << 6) | dext_op);
  }

  void dextm (Register rt, Register rs, int pos, int size) {
     guarantee((0 <= pos) && (pos < 32), "pos must be in [0, 32)");
     guarantee((32 < size) && (size <= 64), "size must be in (32, 64]");
     guarantee((32 < pos + size) && (pos + size <= 64), "pos + size must be in (32, 64]");

     int lsb  = pos;
     int msbd = size - 1 - 32;

     emit_long((special3_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | (msbd << 11) | (lsb << 6) | dextm_op);
  }

  void rotr (Register rd, Register rt, int sa) {
     emit_long((special_op << 26) | (1 << 21) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | (low(sa, 5) << 6) | srl_op);
  }

  void drotr (Register rd, Register rt, int sa) {
     emit_long((special_op << 26) | (1 << 21) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | (low(sa, 5) << 6) | dsrl_op);
  }

  void drotr32 (Register rd, Register rt, int sa) {
     emit_long((special_op << 26) | (1 << 21) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | (low(sa, 5) << 6) | dsrl32_op);
  }

  void rotrv (Register rd, Register rt, Register rs) {
     emit_long((special_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | (1 << 6) | srlv_op);
  }

  void drotrv (Register rd, Register rt, Register rs) {
     emit_long((special_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | (1 << 6) | dsrlv_op);
  }

  void div   (Register rs, Register rt)              { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), 0, div_op)); }
  void divu  (Register rs, Register rt)              { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), 0, divu_op)); }
  void dmult (Register rs, Register rt)              { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), 0, dmult_op)); }
  void dmultu(Register rs, Register rt)              { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), 0, dmultu_op)); }
  void dsll  (Register rd, Register rt , int sa)     { emit_long(insn_RRSO((int)rt->encoding(), (int)rd->encoding(), low(sa, 5), dsll_op)); }
  void dsllv (Register rd, Register rt, Register rs) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), dsllv_op)); }
  void dsll32(Register rd, Register rt , int sa)     { emit_long(insn_RRSO((int)rt->encoding(), (int)rd->encoding(), low(sa, 5), dsll32_op)); }
  void dsra  (Register rd, Register rt , int sa)     { emit_long(insn_RRSO((int)rt->encoding(), (int)rd->encoding(), low(sa, 5), dsra_op)); }
  void dsrav (Register rd, Register rt, Register rs) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), dsrav_op)); }
  void dsra32(Register rd, Register rt , int sa)     { emit_long(insn_RRSO((int)rt->encoding(), (int)rd->encoding(), low(sa, 5), dsra32_op)); }
  void dsrl  (Register rd, Register rt , int sa)     { emit_long(insn_RRSO((int)rt->encoding(), (int)rd->encoding(), low(sa, 5), dsrl_op)); }
  void dsrlv (Register rd, Register rt, Register rs)  { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), dsrlv_op)); }
  void dsrl32(Register rd, Register rt , int sa)     { emit_long(insn_RRSO((int)rt->encoding(), (int)rd->encoding(), low(sa, 5), dsrl32_op)); }
  void dsubu (Register rd, Register rs, Register rt) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), dsubu_op)); }

  void b(int off)       { beq(R0, R0, off); }
  void b(address entry) { b(offset(entry)); }
  void b(Label& L)      { b(target(L)); }

  void j(address entry);
  void jal(address entry);

  void jalr(Register rd, Register rs) { emit_long( ((int)rs->encoding()<<21) | ((int)rd->encoding()<<11) | jalr_op); has_delay_slot(); }
  void jalr(Register rs)              { jalr(RA, rs); }
  void jalr()                         { jalr(RT9); }

  void jr(Register rs) { emit_long(((int)rs->encoding()<<21) | jr_op); has_delay_slot(); }
  void jr_hb(Register rs) { emit_long(((int)rs->encoding()<<21) | (1 << 10) | jr_op); has_delay_slot(); }

  void lb (Register rt, Register base, int off) { emit_long(insn_ORRI(lb_op,  (int)base->encoding(), (int)rt->encoding(), off)); }
  void lbu(Register rt, Register base, int off) { emit_long(insn_ORRI(lbu_op, (int)base->encoding(), (int)rt->encoding(), off)); }
  void ld (Register rt, Register base, int off) { emit_long(insn_ORRI(ld_op,  (int)base->encoding(), (int)rt->encoding(), off)); }
  void ldl(Register rt, Register base, int off) { emit_long(insn_ORRI(ldl_op, (int)base->encoding(), (int)rt->encoding(), off)); }
  void ldr(Register rt, Register base, int off) { emit_long(insn_ORRI(ldr_op, (int)base->encoding(), (int)rt->encoding(), off)); }
  void lh (Register rt, Register base, int off) { emit_long(insn_ORRI(lh_op,  (int)base->encoding(), (int)rt->encoding(), off)); }
  void lhu(Register rt, Register base, int off) { emit_long(insn_ORRI(lhu_op, (int)base->encoding(), (int)rt->encoding(), off)); }
  void ll (Register rt, Register base, int off) { emit_long(insn_ORRI(ll_op,  (int)base->encoding(), (int)rt->encoding(), off)); }
  void lld(Register rt, Register base, int off) { emit_long(insn_ORRI(lld_op, (int)base->encoding(), (int)rt->encoding(), off)); }
  void lui(Register rt, int imm)                { emit_long(insn_ORRI(lui_op, 0, (int)rt->encoding(), simm16(imm))); }
  void lw (Register rt, Register base, int off) { emit_long(insn_ORRI(lw_op,  (int)base->encoding(), (int)rt->encoding(), off)); }
  void lwl(Register rt, Register base, int off) { emit_long(insn_ORRI(lwl_op, (int)base->encoding(), (int)rt->encoding(), off)); }
  void lwr(Register rt, Register base, int off) { emit_long(insn_ORRI(lwr_op, (int)base->encoding(), (int)rt->encoding(), off)); }
  void lwu(Register rt, Register base, int off) { emit_long(insn_ORRI(lwu_op, (int)base->encoding(), (int)rt->encoding(), off)); }

  void lb (Register rt, Address src);
  void lbu(Register rt, Address src);
  void ld (Register rt, Address src);
  void ldl(Register rt, Address src);
  void ldr(Register rt, Address src);
  void lh (Register rt, Address src);
  void lhu(Register rt, Address src);
  void ll (Register rt, Address src);
  void lld(Register rt, Address src);
  void lw (Register rt, Address src);
  void lwl(Register rt, Address src);
  void lwr(Register rt, Address src);
  void lwu(Register rt, Address src);
  void lea(Register rt, Address src);
  void pref(int hint, Register base, int off) { emit_long(insn_ORRI(pref_op, (int)base->encoding(), low(hint, 5), low(off, 16))); }

  void mfhi (Register rd)              { emit_long( ((int)rd->encoding()<<11) | mfhi_op ); }
  void mflo (Register rd)              { emit_long( ((int)rd->encoding()<<11) | mflo_op ); }
  void mthi (Register rs)              { emit_long( ((int)rs->encoding()<<21) | mthi_op ); }
  void mtlo (Register rs)              { emit_long( ((int)rs->encoding()<<21) | mtlo_op ); }

  void mult (Register rs, Register rt) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), 0, mult_op)); }
  void multu(Register rs, Register rt) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), 0, multu_op)); }

  void nor(Register rd, Register rs, Register rt) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), nor_op)); }

  void orr(Register rd, Register rs, Register rt) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), or_op)); }
  void ori(Register rt, Register rs, int imm)     { emit_long(insn_ORRI(ori_op, (int)rs->encoding(), (int)rt->encoding(), simm16(imm))); }

  void sb   (Register rt, Register base, int off)     { emit_long(insn_ORRI(sb_op,    (int)base->encoding(), (int)rt->encoding(), off)); }
  void sc   (Register rt, Register base, int off)     { emit_long(insn_ORRI(sc_op,    (int)base->encoding(), (int)rt->encoding(), off)); }
  void scd  (Register rt, Register base, int off)     { emit_long(insn_ORRI(scd_op,   (int)base->encoding(), (int)rt->encoding(), off)); }
  void sd   (Register rt, Register base, int off)     { emit_long(insn_ORRI(sd_op,    (int)base->encoding(), (int)rt->encoding(), off)); }
  void sdl  (Register rt, Register base, int off)     { emit_long(insn_ORRI(sdl_op,   (int)base->encoding(), (int)rt->encoding(), off)); }
  void sdr  (Register rt, Register base, int off)     { emit_long(insn_ORRI(sdr_op,   (int)base->encoding(), (int)rt->encoding(), off)); }
  void sh   (Register rt, Register base, int off)     { emit_long(insn_ORRI(sh_op,    (int)base->encoding(), (int)rt->encoding(), off)); }
  void sll  (Register rd, Register rt ,  int sa)      { emit_long(insn_RRSO((int)rt->encoding(),  (int)rd->encoding(),   low(sa, 5),      sll_op)); }
  void sllv (Register rd, Register rt,   Register rs) { emit_long(insn_RRRO((int)rs->encoding(),  (int)rt->encoding(),   (int)rd->encoding(), sllv_op)); }
  void slt  (Register rd, Register rs,   Register rt) { emit_long(insn_RRRO((int)rs->encoding(),  (int)rt->encoding(),   (int)rd->encoding(), slt_op)); }
  void slti (Register rt, Register rs,   int imm)     { emit_long(insn_ORRI(slti_op,  (int)rs->encoding(),   (int)rt->encoding(), imm)); }
  void sltiu(Register rt, Register rs,   int imm)     { emit_long(insn_ORRI(sltiu_op, (int)rs->encoding(),   (int)rt->encoding(), imm)); }
  void sltu (Register rd, Register rs,   Register rt) { emit_long(insn_RRRO((int)rs->encoding(),  (int)rt->encoding(),   (int)rd->encoding(), sltu_op)); }
  void sra  (Register rd, Register rt ,  int sa)      { emit_long(insn_RRSO((int)rt->encoding(),  (int)rd->encoding(),   low(sa, 5),      sra_op)); }
  void srav (Register rd, Register rt,   Register rs) { emit_long(insn_RRRO((int)rs->encoding(),  (int)rt->encoding(),   (int)rd->encoding(), srav_op)); }
  void srl  (Register rd, Register rt ,  int sa)      { emit_long(insn_RRSO((int)rt->encoding(),  (int)rd->encoding(),   low(sa, 5),      srl_op)); }
  void srlv (Register rd, Register rt,   Register rs) { emit_long(insn_RRRO((int)rs->encoding(),  (int)rt->encoding(),   (int)rd->encoding(), srlv_op)); }

  void subu (Register rd, Register rs,   Register rt) { dsubu (rd, rs, rt); }
  void subu32 (Register rd, Register rs,   Register rt) { emit_long(insn_RRRO((int)rs->encoding(),  (int)rt->encoding(),   (int)rd->encoding(), subu_op)); }
  void sw   (Register rt, Register base, int off)     { emit_long(insn_ORRI(sw_op,    (int)base->encoding(), (int)rt->encoding(), off)); }
  void swl  (Register rt, Register base, int off)     { emit_long(insn_ORRI(swl_op,   (int)base->encoding(), (int)rt->encoding(), off)); }
  void swr  (Register rt, Register base, int off)     { emit_long(insn_ORRI(swr_op,   (int)base->encoding(), (int)rt->encoding(), off)); }
  void synci(Register base, int off)                  { emit_long(insn_ORRI(regimm_op, (int)base->encoding(), synci_op, off)); }
  void sync ()                                        {
    if (os::is_ActiveCoresMP())
      emit_long(0);
    else
      emit_long(sync_op);
  }
  void syscall(int code)                              { emit_long( (code<<6) | syscall_op ); }

  void sb(Register rt, Address dst);
  void sc(Register rt, Address dst);
  void scd(Register rt, Address dst);
  void sd(Register rt, Address dst);
  void sdl(Register rt, Address dst);
  void sdr(Register rt, Address dst);
  void sh(Register rt, Address dst);
  void sw(Register rt, Address dst);
  void swl(Register rt, Address dst);
  void swr(Register rt, Address dst);

  void teq  (Register rs, Register rt, int code) { emit_long(insn_RRCO((int)rs->encoding(),   (int)rt->encoding(), code, teq_op)); }
  void teqi (Register rs, int imm)               { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), teqi_op, imm)); }
  void tge  (Register rs, Register rt, int code) { emit_long(insn_RRCO((int)rs->encoding(),   (int)rt->encoding(), code, tge_op)); }
  void tgei (Register rs, int imm)               { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), tgei_op, imm)); }
  void tgeiu(Register rs, int imm)               { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), tgeiu_op, imm)); }
  void tgeu (Register rs, Register rt, int code) { emit_long(insn_RRCO((int)rs->encoding(),   (int)rt->encoding(), code, tgeu_op)); }
  void tlt  (Register rs, Register rt, int code) { emit_long(insn_RRCO((int)rs->encoding(),   (int)rt->encoding(), code, tlt_op)); }
  void tlti (Register rs, int imm)               { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), tlti_op, imm)); }
  void tltiu(Register rs, int imm)               { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), tltiu_op, imm)); }
  void tltu (Register rs, Register rt, int code) { emit_long(insn_RRCO((int)rs->encoding(),   (int)rt->encoding(), code, tltu_op)); }
  void tne  (Register rs, Register rt, int code) { emit_long(insn_RRCO((int)rs->encoding(),   (int)rt->encoding(), code, tne_op)); }
  void tnei (Register rs, int imm)               { emit_long(insn_ORRI(regimm_op, (int)rs->encoding(), tnei_op, imm)); }

  void xorr(Register rd, Register rs, Register rt) { emit_long(insn_RRRO((int)rs->encoding(), (int)rt->encoding(), (int)rd->encoding(), xor_op)); }
  void xori(Register rt, Register rs, int imm) { emit_long(insn_ORRI(xori_op, (int)rs->encoding(), (int)rt->encoding(), simm16(imm))); }

  void nop()               { emit_long(0); }



  void ldc1(FloatRegister ft, Register base, int off) { emit_long(insn_ORRI(ldc1_op, (int)base->encoding(), (int)ft->encoding(), off)); }
  void lwc1(FloatRegister ft, Register base, int off) { emit_long(insn_ORRI(lwc1_op, (int)base->encoding(), (int)ft->encoding(), off)); }
  void ldc1(FloatRegister ft, Address src);
  void lwc1(FloatRegister ft, Address src);

  //COP0
  void mfc0  (Register rt, Register rd)       { emit_long(insn_COP0( mfc0_op, (int)rt->encoding(), (int)rd->encoding())); }
  void dmfc0 (Register rt, FloatRegister rd)  { emit_long(insn_COP0(dmfc0_op, (int)rt->encoding(), (int)rd->encoding())); }
  // MFGC0, DMFGC0, MTGC0, DMTGC0 not implemented yet
  void mtc0  (Register rt, Register rd)       { emit_long(insn_COP0( mtc0_op, (int)rt->encoding(), (int)rd->encoding())); }
  void dmtc0 (Register rt, FloatRegister rd)  { emit_long(insn_COP0(dmtc0_op, (int)rt->encoding(), (int)rd->encoding())); }
  //COP0 end


  //COP1
  void mfc1 (Register rt, FloatRegister fs) { emit_long(insn_COP1 (mfc1_op, (int)rt->encoding(), (int)fs->encoding())); }
  void dmfc1(Register rt, FloatRegister fs) { emit_long(insn_COP1(dmfc1_op, (int)rt->encoding(), (int)fs->encoding())); }
  void cfc1 (Register rt, int fs)           { emit_long(insn_COP1( cfc1_op, (int)rt->encoding(), fs)); }
  void mfhc1(Register rt, int fs)           { emit_long(insn_COP1(mfhc1_op, (int)rt->encoding(), fs)); }
  void mtc1 (Register rt, FloatRegister fs) { emit_long(insn_COP1( mtc1_op, (int)rt->encoding(), (int)fs->encoding())); }
  void dmtc1(Register rt, FloatRegister fs) { emit_long(insn_COP1(dmtc1_op, (int)rt->encoding(), (int)fs->encoding())); }
  void ctc1 (Register rt, FloatRegister fs) { emit_long(insn_COP1( ctc1_op, (int)rt->encoding(), (int)fs->encoding())); }
  void ctc1 (Register rt, int fs)           { emit_long(insn_COP1(ctc1_op,  (int)rt->encoding(), fs)); }
  void mthc1(Register rt, int fs)           { emit_long(insn_COP1(mthc1_op, (int)rt->encoding(), fs)); }

  void bc1f (int off) { emit_long(insn_ORRI(cop1_op, bc1f_op, bcf_op, off)); has_delay_slot(); }
  void bc1fl(int off) { emit_long(insn_ORRI(cop1_op, bc1f_op, bcfl_op, off)); has_delay_slot(); }
  void bc1t (int off) { emit_long(insn_ORRI(cop1_op, bc1f_op, bct_op, off)); has_delay_slot(); }
  void bc1tl(int off) { emit_long(insn_ORRI(cop1_op, bc1f_op, bctl_op, off));  has_delay_slot(); }

  void bc1f (address entry) { bc1f(offset(entry)); }
  void bc1fl(address entry) { bc1fl(offset(entry)); }
  void bc1t (address entry) { bc1t(offset(entry)); }
  void bc1tl(address entry) { bc1tl(offset(entry)); }

  void bc1f (Label& L) { bc1f(target(L)); }
  void bc1fl(Label& L) { bc1fl(target(L)); }
  void bc1t (Label& L) { bc1t(target(L)); }
  void bc1tl(Label& L) { bc1tl(target(L)); }

//R0->encoding() is 0; INSN_SINGLE is enclosed by {} for ctags.
#define INSN_SINGLE(r1, r2, r3, op)   \
  { emit_long(insn_F3RO(single_fmt, (int)r1->encoding(), (int)r2->encoding(), (int)r3->encoding(), op));}
  void add_s    (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, fd, fadd_op)}
  void sub_s    (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, fd, fsub_op)}
  void mul_s    (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, fd, fmul_op)}
  void div_s    (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, fd, fdiv_op)}
  void sqrt_s   (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fsqrt_op)}
  void abs_s    (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fabs_op)}
  void mov_s    (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fmov_op)}
  void neg_s    (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fneg_op)}
  void round_l_s(FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, froundl_op)}
  void trunc_l_s(FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, ftruncl_op)}
  void ceil_l_s (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fceill_op)}
  void floor_l_s(FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, ffloorl_op)}
  void round_w_s(FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, froundw_op)}
  void trunc_w_s(FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, ftruncw_op)}
  void ceil_w_s (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fceilw_op)}
  void floor_w_s(FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, ffloorw_op)}
  //null
  void movf_s(FloatRegister fs, FloatRegister fd, int cc = 0) {
    assert(cc >= 0 && cc <= 7, "cc is 3 bits");
    emit_long((cop1_op<<26) | (single_fmt<<21) | (cc<<18) | ((int)fs->encoding()<<11) | ((int)fd->encoding()<<6) | movf_f_op );}
  void movt_s(FloatRegister fs, FloatRegister fd, int cc = 0) {
    assert(cc >= 0 && cc <= 7, "cc is 3 bits");
    emit_long((cop1_op<<26) | (single_fmt<<21) | (cc<<18) | 1<<16 | ((int)fs->encoding()<<11) | ((int)fd->encoding()<<6) | movf_f_op );}
  void movz_s  (FloatRegister fd, FloatRegister fs, Register rt) {INSN_SINGLE(rt, fs, fd, movz_f_op)}
  void movn_s  (FloatRegister fd, FloatRegister fs, Register rt) {INSN_SINGLE(rt, fs, fd, movn_f_op)}
  //null
  void recip_s (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, frecip_op)}
  void rsqrt_s (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, frsqrt_op)}
  //null
  void cvt_d_s (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fcvtd_op)}
  //null
  void cvt_w_s (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fcvtw_op)}
  void cvt_l_s (FloatRegister fd, FloatRegister fs) {INSN_SINGLE(R0, fs, fd, fcvtl_op)}
  void cvt_ps_s(FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, fd, fcvtps_op)}
  //null
  void c_f_s   (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, f_cond)}
  void c_un_s  (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, un_cond)}
  void c_eq_s  (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, eq_cond)}
  void c_ueq_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, ueq_cond)}
  void c_olt_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, olt_cond)}
  void c_ult_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, ult_cond)}
  void c_ole_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, ole_cond)}
  void c_ule_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, ule_cond)}
  void c_sf_s  (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, sf_cond)}
  void c_ngle_s(FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, ngle_cond)}
  void c_seq_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, seq_cond)}
  void c_ngl_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, ngl_cond)}
  void c_lt_s  (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, lt_cond)}
  void c_nge_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, nge_cond)}
  void c_le_s  (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, le_cond)}
  void c_ngt_s (FloatRegister fs, FloatRegister ft) {INSN_SINGLE(ft, fs, R0, ngt_cond)}

#undef INSN_SINGLE


//R0->encoding() is 0; INSN_DOUBLE is enclosed by {} for ctags.
#define INSN_DOUBLE(r1, r2, r3, op)   \
  { emit_long(insn_F3RO(double_fmt, (int)r1->encoding(), (int)r2->encoding(), (int)r3->encoding(), op));}

  void add_d    (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, fd, fadd_op)}
  void sub_d    (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, fd, fsub_op)}
  void mul_d    (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, fd, fmul_op)}
  void div_d    (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, fd, fdiv_op)}
  void sqrt_d   (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fsqrt_op)}
  void abs_d    (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fabs_op)}
  void mov_d    (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fmov_op)}
  void neg_d    (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fneg_op)}
  void round_l_d(FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, froundl_op)}
  void trunc_l_d(FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, ftruncl_op)}
  void ceil_l_d (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fceill_op)}
  void floor_l_d(FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, ffloorl_op)}
  void round_w_d(FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, froundw_op)}
  void trunc_w_d(FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, ftruncw_op)}
  void ceil_w_d (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fceilw_op)}
  void floor_w_d(FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, ffloorw_op)}
  //null
  void movf_d(FloatRegister fs, FloatRegister fd, int cc = 0) {
    assert(cc >= 0 && cc <= 7, "cc is 3 bits");
    emit_long((cop1_op<<26) | (double_fmt<<21) | (cc<<18) | ((int)fs->encoding()<<11) | ((int)fd->encoding()<<6) | movf_f_op );}
  void movt_d(FloatRegister fs, FloatRegister fd, int cc = 0) {
    assert(cc >= 0 && cc <= 7, "cc is 3 bits");
    emit_long((cop1_op<<26) | (double_fmt<<21) | (cc<<18) | 1<<16 | ((int)fs->encoding()<<11) | ((int)fd->encoding()<<6) | movf_f_op );}
  void movz_d  (FloatRegister fd, FloatRegister fs, Register rt) {INSN_DOUBLE(rt, fs, fd, movz_f_op)}
  void movn_d  (FloatRegister fd, FloatRegister fs, Register rt) {INSN_DOUBLE(rt, fs, fd, movn_f_op)}
  //null
  void recip_d (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, frecip_op)}
  void rsqrt_d (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, frsqrt_op)}
  //null
  void cvt_s_d (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fcvts_op)}
  void cvt_l_d (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fcvtl_op)}
  //null
  void cvt_w_d (FloatRegister fd, FloatRegister fs) {INSN_DOUBLE(R0, fs, fd, fcvtw_op)}
  //null
  void c_f_d   (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, f_cond)}
  void c_un_d  (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, un_cond)}
  void c_eq_d  (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, eq_cond)}
  void c_ueq_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, ueq_cond)}
  void c_olt_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, olt_cond)}
  void c_ult_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, ult_cond)}
  void c_ole_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, ole_cond)}
  void c_ule_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, ule_cond)}
  void c_sf_d  (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, sf_cond)}
  void c_ngle_d(FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, ngle_cond)}
  void c_seq_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, seq_cond)}
  void c_ngl_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, ngl_cond)}
  void c_lt_d  (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, lt_cond)}
  void c_nge_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, nge_cond)}
  void c_le_d  (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, le_cond)}
  void c_ngt_d (FloatRegister fs, FloatRegister ft) {INSN_DOUBLE(ft, fs, R0, ngt_cond)}

#undef INSN_DOUBLE


  //null
  void cvt_s_w(FloatRegister fd, FloatRegister fs) { emit_long(insn_F3RO(word_fmt, 0, (int)fs->encoding(), (int)fd->encoding(), fcvts_op)); }
  void cvt_d_w(FloatRegister fd, FloatRegister fs) { emit_long(insn_F3RO(word_fmt, 0, (int)fs->encoding(), (int)fd->encoding(), fcvtd_op)); }
  //null
  void cvt_s_l(FloatRegister fd, FloatRegister fs) { emit_long(insn_F3RO(long_fmt, 0, (int)fs->encoding(), (int)fd->encoding(), fcvts_op)); }
  void cvt_d_l(FloatRegister fd, FloatRegister fs) { emit_long(insn_F3RO(long_fmt, 0, (int)fs->encoding(), (int)fd->encoding(), fcvtd_op)); }
  //null


//R0->encoding() is 0; INSN_PS is enclosed by {} for ctags.
#define INSN_PS(r1, r2, r3, op)   \
  { emit_long(insn_F3RO(ps_fmt, (int)r1->encoding(), (int)r2->encoding(), (int)r3->encoding(), op));}

  void add_ps (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, fd, fadd_op)}
  void sub_ps (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, fd, fsub_op)}
  void mul_ps (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, fd, fmul_op)}
  //null
  void abs_ps (FloatRegister fd, FloatRegister fs) {INSN_PS(R0, fs, fd, fabs_op)}
  void mov_ps (FloatRegister fd, FloatRegister fs) {INSN_PS(R0, fs, fd, fmov_op)}
  void neg_ps (FloatRegister fd, FloatRegister fs) {INSN_PS(R0, fs, fd, fneg_op)}
  //null
  //void movf_ps(FloatRegister rd, FloatRegister rs, FPConditionCode cc) { unimplemented(" movf_ps")}
  //void movt_ps(FloatRegister rd, FloatRegister rs, FPConditionCode cc) { unimplemented(" movt_ps") }
  void movz_ps  (FloatRegister fd, FloatRegister fs, Register rt) {INSN_PS(rt, fs, fd, movz_f_op)}
  void movn_ps  (FloatRegister fd, FloatRegister fs, Register rt) {INSN_PS(rt, fs, fd, movn_f_op)}
  //null
  void cvt_s_pu (FloatRegister fd, FloatRegister fs) {INSN_PS(R0, fs, fd, fcvts_op)}
  //null
  void cvt_s_pl (FloatRegister fd, FloatRegister fs) {INSN_PS(R0, fs, fd, fcvtspl_op)}
  //null
  void pll_ps   (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, fd, fpll_op)}
  void plu_ps   (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, fd, fplu_op)}
  void pul_ps   (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, fd, fpul_op)}
  void puu_ps   (FloatRegister fd, FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, fd, fpuu_op)}
  void c_f_ps   (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, f_cond)}
  void c_un_ps  (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, un_cond)}
  void c_eq_ps  (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, eq_cond)}
  void c_ueq_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, ueq_cond)}
  void c_olt_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, olt_cond)}
  void c_ult_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, ult_cond)}
  void c_ole_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, ole_cond)}
  void c_ule_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, ule_cond)}
  void c_sf_ps  (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, sf_cond)}
  void c_ngle_ps(FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, ngle_cond)}
  void c_seq_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, seq_cond)}
  void c_ngl_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, ngl_cond)}
  void c_lt_ps  (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, lt_cond)}
  void c_nge_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, nge_cond)}
  void c_le_ps  (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, le_cond)}
  void c_ngt_ps (FloatRegister fs, FloatRegister ft) {INSN_PS(ft, fs, R0, ngt_cond)}
  //null
#undef INSN_PS
  //COP1 end


  //COP1X
//R0->encoding() is 0; INSN_SINGLE is enclosed by {} for ctags.
#define INSN_COP1X(r0, r1, r2, r3, op)   \
  { emit_long(insn_F3ROX((int)r0->encoding(), (int)r1->encoding(), (int)r2->encoding(), (int)r3->encoding(), op));}
  void madd_s(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft) {INSN_COP1X(fr, ft, fs, fd, madd_s_op) }
  void madd_d(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft) {INSN_COP1X(fr, ft, fs, fd, madd_d_op) }
  void madd_ps(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft){INSN_COP1X(fr, ft, fs, fd, madd_ps_op) }
  void msub_s(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft) {INSN_COP1X(fr, ft, fs, fd, msub_s_op) }
  void msub_d(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft) {INSN_COP1X(fr, ft, fs, fd, msub_d_op) }
  void msub_ps(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft){INSN_COP1X(fr, ft, fs, fd, msub_ps_op) }
  void nmadd_s(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft) {INSN_COP1X(fr, ft, fs, fd, nmadd_s_op) }
  void nmadd_d(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft) {INSN_COP1X(fr, ft, fs, fd, nmadd_d_op) }
  void nmadd_ps(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft){INSN_COP1X(fr, ft, fs, fd, nmadd_ps_op) }
  void nmsub_s(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft) {INSN_COP1X(fr, ft, fs, fd, nmsub_s_op) }
  void nmsub_d(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft) {INSN_COP1X(fr, ft, fs, fd, nmsub_d_op) }
  void nmsub_ps(FloatRegister fd, FloatRegister fr, FloatRegister fs, FloatRegister ft){INSN_COP1X(fr, ft, fs, fd, nmsub_ps_op) }
#undef INSN_COP1X
  //COP1X end

  //SPECIAL2
//R0->encoding() is 0; INSN_PS is enclosed by {} for ctags.
#define INSN_S2(op)   \
  { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | op);}

  void madd    (Register rs, Register rt) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | madd_op); }
  void maddu   (Register rs, Register rt) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | maddu_op); }
  void mul     (Register rd, Register rs, Register rt) { INSN_S2(mul_op)     }
  void gsandn  (Register rd, Register rs, Register rt) { INSN_S2((0x12 << 6) | gs0x03_op) }
  void msub    (Register rs, Register rt) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | msub_op); }
  void msubu   (Register rs, Register rt) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | msubu_op); }
  void gsorn   (Register rd, Register rs, Register rt) { INSN_S2((0x12 << 6) | gs0x06_op) }

  void gsmult  (Register rd, Register rs, Register rt) { INSN_S2(gsmult_op)  }
  void gsdmult (Register rd, Register rs, Register rt) { INSN_S2(gsdmult_op) }
  void gsmultu (Register rd, Register rs, Register rt) { INSN_S2(gsmultu_op) }
  void gsdmultu(Register rd, Register rs, Register rt) { INSN_S2(gsdmultu_op)}
  void gsdiv   (Register rd, Register rs, Register rt) { INSN_S2(gsdiv_op)   }
  void gsddiv  (Register rd, Register rs, Register rt) { INSN_S2(gsddiv_op)  }
  void gsdivu  (Register rd, Register rs, Register rt) { INSN_S2(gsdivu_op)  }
  void gsddivu (Register rd, Register rs, Register rt) { INSN_S2(gsddivu_op) }
  void gsmod   (Register rd, Register rs, Register rt) { INSN_S2(gsmod_op)   }
  void gsdmod  (Register rd, Register rs, Register rt) { INSN_S2(gsdmod_op)  }
  void gsmodu  (Register rd, Register rs, Register rt) { INSN_S2(gsmodu_op)  }
  void gsdmodu (Register rd, Register rs, Register rt) { INSN_S2(gsdmodu_op) }
  void clz (Register rd, Register rs) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rd->encoding() << 16) | ((int)rd->encoding() << 11) | clz_op); }
  void clo (Register rd, Register rs) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rd->encoding() << 16) | ((int)rd->encoding() << 11) | clo_op); }
  void ctz (Register rd, Register rs) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rd->encoding() << 16) | ((int)rd->encoding() << 11) | 0 << 6| xctx_op); }
  void cto (Register rd, Register rs) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rd->encoding() << 16) | ((int)rd->encoding() << 11) | 1 << 6| xctx_op); }
  void dctz(Register rd, Register rs) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rd->encoding() << 16) | ((int)rd->encoding() << 11) | 2 << 6| xctx_op); }
  void dcto(Register rd, Register rs) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rd->encoding() << 16) | ((int)rd->encoding() << 11) | 3 << 6| xctx_op); }
  void dclz(Register rd, Register rs) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rd->encoding() << 16) | ((int)rd->encoding() << 11) | dclz_op); }
  void dclo(Register rd, Register rs) { emit_long((special2_op << 26) | ((int)rs->encoding() << 21) | ((int)rd->encoding() << 16) | ((int)rd->encoding() << 11) | dclo_op); }

#undef INSN_S2

  //SPECIAL3
/*
// FIXME
#define is_0_to_32(a, b) \
  assert (a >= 0, " just a check"); \
  assert (a <= 0, " just a check"); \
  assert (b >= 0, " just a check"); \
  assert (b <= 0, " just a check"); \
  assert (a+b >= 0, " just a check"); \
  assert (a+b <= 0, " just a check");
  */
#define is_0_to_32(a, b)

  void ins  (Register rt, Register rs, int pos, int size) { is_0_to_32(pos, size); emit_long((special3_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | (low(pos+size-1, 5) << 11) | (low(pos, 5) << 6) | ins_op); }
  void dinsm(Register rt, Register rs, int pos, int size) { is_0_to_32(pos, size); emit_long((special3_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | (low(pos+size-33, 5) << 11) | (low(pos, 5) << 6) | dinsm_op); }
  void dinsu(Register rt, Register rs, int pos, int size) { is_0_to_32(pos, size); emit_long((special3_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | (low(pos+size-33, 5) << 11) | (low(pos-32, 5) << 6) | dinsu_op); }
  void dins (Register rt, Register rs, int pos, int size) {
     guarantee((0 <= pos) && (pos < 32), "pos must be in [0, 32)");
     guarantee((0 < size) && (size <= 32), "size must be in (0, 32]");
     guarantee((0 < pos + size) && (pos + size <= 32), "pos + size must be in (0, 32]");

     emit_long((special3_op << 26) | ((int)rs->encoding() << 21) | ((int)rt->encoding() << 16) | (low(pos+size-1, 5) << 11) | (low(pos, 5) << 6) | dins_op);
  }

  void repl_qb (Register rd, int const8)  { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | (low(const8, 8) << 16)      | ((int)rd->encoding() << 11) | repl_qb_op  << 6 | re1_op); }
  void replv_qb(Register rd, Register rt) { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | replv_qb_op << 6 | re1_op ); }
  void repl_ph (Register rd, int const10) { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | (low(const10, 10) << 16)    | ((int)rd->encoding() << 11) | repl_ph_op  << 6 | re1_op); }
  void replv_ph(Register rd, Register rt) { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | replv_ph_op << 6 | re1_op ); }

  void repl_ob (Register rd, int const8)  { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | (low(const8, 8) << 16)      | ((int)rd->encoding() << 11) | repl_ob_op  << 6 | re2_op); }
  void replv_ob(Register rd, Register rt) { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | replv_ob_op << 6 | re2_op ); }
  void repl_qh (Register rd, int const10) { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | (low(const10, 10) << 16)    | ((int)rd->encoding() << 11) | repl_qh_op  << 6 | re2_op); }
  void replv_qh(Register rd, Register rt) { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | replv_qh_op << 6 | re2_op ); }
  void repl_pw (Register rd, int const10) { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | (low(const10, 10) << 16)    | ((int)rd->encoding() << 11) | repl_pw_op  << 6 | re2_op); }
  void replv_pw(Register rd, Register rt) { assert(VM_Version::supports_dsp(), ""); emit_long((special3_op << 26) | ((int)rt->encoding() << 16) | ((int)rd->encoding() << 11) | replv_pw_op << 6 | re2_op ); }

  void sdc1(FloatRegister ft, Register base, int off) { emit_long(insn_ORRI(sdc1_op, (int)base->encoding(), (int)ft->encoding(), off)); }
  void sdc1(FloatRegister ft, Address dst);
  void swc1(FloatRegister ft, Register base, int off) { emit_long(insn_ORRI(swc1_op, (int)base->encoding(), (int)ft->encoding(), off)); }
  void swc1(FloatRegister ft, Address dst);


  static void print_instruction(int);
  int patched_branch(int dest_pos, int inst, int inst_pos);
  int branch_destination(int inst, int pos);

  // Loongson extension

  // gssq/gslq/gssqc1/gslqc1: vAddr = sign_extend(offset << 4 ) + GPR[base]. Therefore, the off should be ">> 4".
  void gslble(Register rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gslble_op);
  }

  void gslbgt(Register rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gslbgt_op);
  }

  void gslhle(Register rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gslhle_op);
  }

  void gslhgt(Register rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gslhgt_op);
  }

  void gslwle(Register rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gslwle_op);
  }

  void gslwgt(Register rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gslwgt_op);
  }

  void gsldle(Register rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsldle_op);
  }

  void gsldgt(Register rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsldgt_op);
  }

  void gslwlec1(FloatRegister rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gslwlec1_op);
  }

  void gslwgtc1(FloatRegister rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gslwgtc1_op);
  }

  void gsldlec1(FloatRegister rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsldlec1_op);
  }

  void gsldgtc1(FloatRegister rt, Register base, Register bound) {
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsldgtc1_op);
  }

  void gslq(Register rq, Register rt, Register base, int off) {
    assert(!(off & 0xF), "gslq: the low 4 bits of off must be 0");
    off = off >> 4;
    assert(is_simm(off, 9),"gslq: off exceeds 9 bits");
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | 0 << 15 | (low(off, 9) << 6) | gslq_op | (int)rq->encoding() );
  }

  void gslqc1(FloatRegister rq, FloatRegister rt, Register base, int off) {
    assert(!(off & 0xF), "gslqc1: the low 4 bits of off must be 0");
    off = off >> 4;
    assert(is_simm(off, 9),"gslqc1: off exceeds 9 bits");
    emit_long((gs_lwc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | 1 << 15 | (low(off, 9) << 6) | gslq_op | (int)rq->encoding() );
  }

  void gssble(Register rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gssble_op);
  }

  void gssbgt(Register rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gssbgt_op);
  }

  void gsshle(Register rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsshle_op);
  }

  void gsshgt(Register rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsshgt_op);
  }

  void gsswle(Register rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsswle_op);
  }

  void gsswgt(Register rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsswgt_op);
  }

  void gssdle(Register rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gssdle_op);
  }

  void gssdgt(Register rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gssdgt_op);
  }

  void gsswlec1(FloatRegister rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsswlec1_op);
  }

  void gsswgtc1(FloatRegister rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gsswgtc1_op);
  }

  void gssdlec1(FloatRegister rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gssdlec1_op);
  }

  void gssdgtc1(FloatRegister rt, Register base, Register bound) {
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | ((int)bound->encoding() << 11) | 0 << 6 | gssdgtc1_op);
  }

  void gssq(Register rq, Register rt, Register base, int off) {
    assert(!(off & 0xF), "gssq: the low 4 bits of off must be 0");
    off = off >> 4;
    assert(is_simm(off, 9),"gssq: off exceeds 9 bits");
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | 0 << 15 | (low(off, 9) << 6) | gssq_op | (int)rq->encoding() );
  }

  void gssqc1(FloatRegister rq, FloatRegister rt, Register base, int off) {
    assert(!(off & 0xF), "gssqc1: the low 4 bits of off must be 0");
    off = off >> 4;
    assert(is_simm(off, 9),"gssqc1: off exceeds 9 bits");
    emit_long((gs_swc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) | 1 << 15 | (low(off, 9) << 6) | gssq_op | (int)rq->encoding() );
  }

  //LDC2 & SDC2
#define INSN(OPS, OP) \
    assert(is_simm(off, 8), "NAME: off exceeds 8 bits");                                           \
    assert(UseLEXT1, "check UseLEXT1");                                                      \
    emit_long( (OPS << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) |         \
               ((int)index->encoding() << 11) | (low(off, 8) << 3) | OP);

#define INSN_LDC2(NAME, op)  \
  void NAME(Register rt, Register base, Register index, int off) {                                 \
    INSN(gs_ldc2_op, op)                                                                           \
  }

#define INSN_LDC2_F(NAME, op)  \
  void NAME(FloatRegister rt, Register base, Register index, int off) {                            \
    INSN(gs_ldc2_op, op)                                                                           \
  }

#define INSN_SDC2(NAME, op)  \
  void NAME(Register rt, Register base, Register index, int off) {                                 \
    INSN(gs_sdc2_op, op)                                                                           \
  }

#define INSN_SDC2_F(NAME, op)  \
  void NAME(FloatRegister rt, Register base, Register index, int off) {                            \
    INSN(gs_sdc2_op, op)                                                                           \
  }

/*
 void gslbx(Register rt, Register base, Register index, int off) {
    assert(is_simm(off, 8), "gslbx: off exceeds 8 bits");
    assert(UseLEXT1, "check UseLEXT1");
    emit_long( (gs_ldc2_op << 26) | ((int)base->encoding() << 21) | ((int)rt->encoding() << 16) |
               ((int)index->encoding() << 11) | (low(off, 8) << 3) | gslbx_op);
 void gslbx(Register rt, Register base, Register index, int off) {INSN(gs_ldc2_op, gslbx_op);}

  INSN_LDC2(gslbx, gslbx_op)
  INSN_LDC2(gslhx, gslhx_op)
  INSN_LDC2(gslwx, gslwx_op)
  INSN_LDC2(gsldx, gsldx_op)
  INSN_LDC2_F(gslwxc1, gslwxc1_op)
  INSN_LDC2_F(gsldxc1, gsldxc1_op)

  INSN_SDC2(gssbx, gssbx_op)
  INSN_SDC2(gsshx, gsshx_op)
  INSN_SDC2(gsswx, gsswx_op)
  INSN_SDC2(gssdx, gssdx_op)
  INSN_SDC2_F(gsswxc1, gsswxc1_op)
  INSN_SDC2_F(gssdxc1, gssdxc1_op)
*/
  void gslbx(Register rt, Register base, Register index, int off) {INSN(gs_ldc2_op, gslbx_op) }
  void gslhx(Register rt, Register base, Register index, int off) {INSN(gs_ldc2_op, gslhx_op) }
  void gslwx(Register rt, Register base, Register index, int off) {INSN(gs_ldc2_op, gslwx_op) }
  void gsldx(Register rt, Register base, Register index, int off) {INSN(gs_ldc2_op, gsldx_op) }
  void gslwxc1(FloatRegister rt, Register base, Register index, int off) {INSN(gs_ldc2_op, gslwxc1_op) }
  void gsldxc1(FloatRegister rt, Register base, Register index, int off) {INSN(gs_ldc2_op, gsldxc1_op) }

  void gssbx(Register rt, Register base, Register index, int off) {INSN(gs_sdc2_op, gssbx_op) }
  void gsshx(Register rt, Register base, Register index, int off) {INSN(gs_sdc2_op, gsshx_op) }
  void gsswx(Register rt, Register base, Register index, int off) {INSN(gs_sdc2_op, gsswx_op) }
  void gssdx(Register rt, Register base, Register index, int off) {INSN(gs_sdc2_op, gssdx_op) }
  void gsswxc1(FloatRegister rt, Register base, Register index, int off) {INSN(gs_sdc2_op, gsswxc1_op) }
  void gssdxc1(FloatRegister rt, Register base, Register index, int off) {INSN(gs_sdc2_op, gssdxc1_op) }

#undef INSN
#undef INSN_LDC2
#undef INSN_LDC2_F
#undef INSN_SDC2
#undef INSN_SDC2_F

  // cpucfg on Loongson CPUs above 3A4000
  void cpucfg(Register rd, Register rs) { emit_long((gs_lwc2_op << 26) | ((int)rs->encoding() << 21) | (0b01000 << 16) | ((int)rd->encoding() << 11) | ( 0b00100 << 6) | 0b011000);}


public:
  // Creation
  Assembler(CodeBuffer* code) : AbstractAssembler(code) {
#ifdef CHECK_DELAY
    delay_state = no_delay;
#endif
  }

  // Decoding
  static address locate_operand(address inst, WhichOperand which);
  static address locate_next_instruction(address inst);
};



#endif // CPU_MIPS_VM_ASSEMBLER_MIPS_HPP
