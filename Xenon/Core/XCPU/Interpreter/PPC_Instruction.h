// Copyright 2025 Xenon Emulator Project. All rights reserved.

/*
* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#pragma once

#include <array>
#include <string>

#include "Base/Hash.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/XCPU/Interpreter/PPCOpcodes.h"
#include "Core/XCPU/PPU/PPU_JIT.h"

constexpr u64 PPCRotateMask(u32 mb, u32 me) {
  const u64 mask = ~0ULL << (~(me - mb) & 63);
  return (mask >> (mb & 63)) | (mask << ((64 - mb) & 63));
}

constexpr u32 PPCDecode(u32 instr) {
  return ((instr >> 26) | (instr << 6)) & 0x1FFFF; // Rotate + mask
}

namespace PPCInterpreter {
  // Define a type alias for function pointers
  using instructionHandler = fptr<void(PPU_STATE *ppuState)>;
  using instructionHandlerJIT = fptr<void(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr)>;
  extern void PPCInterpreter_nop(PPU_STATE *ppuState);
  extern void PPCInterpreter_invalid(PPU_STATE *ppuState);
  extern void PPCInterpreter_known_unimplemented(const char *name, PPU_STATE *ppuState);
  extern const std::string PPCInterpreter_getFullName(u32 instr);
  extern void PPCInterpreterJIT_invalid(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr);
  class PPCDecoder {
    template <typename T>
    class InstrInfo {
    public:
      constexpr InstrInfo(u32 v, T p, T pRc, u32 m = 0) :
        value(v), ptr0(p), ptrRc(pRc), magn(m)
      {}

      constexpr InstrInfo(u32 v, const T* p, const T* pRc, u32 m = 0) :
        value(v), ptr0(*p), ptrRc(*pRc), magn(m)
      {}

      u32 value;
      T ptr0;
      T ptrRc;
      u32 magn; // Non-zero for "columns" (effectively, number of most significant bits "eaten")
    };
  public:
    void fillTables();
    void fillJITTables();
    void fillNameTables() {
      #define GET_(name) std::string(#name)
      #define GET(name) GET_(name), GET_(name)
      #define GETRC(name) GET_(name##x), GET_(name##x)
      for (auto& x : nameTable) {
        x = GET(invalid);
      }
      // Main opcodes (field 0..5)
      fillTable<std::string>(nameTable, 0x00, 6, -1, {
        { 0x02, GET(tdi) },
        { 0x03, GET(twi) },
        { 0x07, GET(mulli) },
        { 0x08, GET(subfic) },
        { 0x0A, GET(cmpli) },
        { 0x0B, GET(cmpi) },
        { 0x0C, GET(addic) },
        { 0x0D, GET(addic) },
        { 0x0E, GET(addi) },
        { 0x0F, GET(addis) },
        { 0x10, GET(bc) },
        { 0x11, GET(sc) },
        { 0x12, GET(b) },
        { 0x14, GETRC(rlwimi) },
        { 0x15, GETRC(rlwinm) },
        { 0x17, GETRC(rlwnm) },
        { 0x18, GET(ori) },
        { 0x19, GET(oris) },
        { 0x1A, GET(xori) },
        { 0x1B, GET(xoris) },
        { 0x1C, GET(andi) },
        { 0x1D, GET(andis) },
        { 0x20, GET(lwz) },
        { 0x21, GET(lwzu) },
        { 0x22, GET(lbz) },
        { 0x23, GET(lbzu) },
        { 0x24, GET(stw) },
        { 0x25, GET(stwu) },
        { 0x26, GET(stb) },
        { 0x27, GET(stbu) },
        { 0x28, GET(lhz) },
        { 0x29, GET(lhzu) },
        { 0x2A, GET(lha) },
        { 0x2B, GET(lhau) },
        { 0x2C, GET(sth) },
        { 0x2D, GET(sthu) },
        { 0x2E, GET(lmw) },
        { 0x2F, GET(stmw) },
        { 0x30, GET(lfs) },
        { 0x31, GET(lfsu) },
        { 0x32, GET(lfd) },
        { 0x33, GET(lfdu) },
        { 0x34, GET(stfs) },
        { 0x35, GET(stfsu) },
        { 0x36, GET(stfd) },
        { 0x37, GET(stfdu) },
      });
      // Special case opcodes
      fillTable<std::string>(nameTable, 0x04, 11, 11, {
        { 0x945, GET(vperm128) },
        { 0x7193, GET(stvewx128) },
        { 0x7393, GET(stvrx128) },
        { 0x73C4, GET(stvx128) },
        { 0xB705, GET(vor128) },
        { 0x30C4, GET(lvx128) },
        { 0x2405, GET(vmulfp128) },
        { 0x141C4, GET(stvlx128) },
        { 0x151C4, GET(stvrx128) },
        { 0x1D8C6, GET(vmrglw128) },
        { 0x1C8C6, GET(vmrghw128) },        
        { 0x1C1C4, GET(stvlxl128) },
        { 0x1DD06, GET(vspltisw128) },
      });
      // Group 0x04 opcodes (field 21..31)
      fillTable<std::string>(nameTable, 0x04, 11, 0, {
        { 0x83, GET(lvewx128) },
        { 0x403, GET(lvlx128) },
        { 0x603, GET(lvlxl128) },
        { 0x443, GET(lvrx128) },
        { 0x643, GET(lvrxl128) },
        { 0x3, GET(lvsl128) },
        { 0x43, GET(lvsr128) },
        { 0x2C3, GET(lvxl128) },
        { 0x604, GET(mfvscr) },
        { 0x644, GET(mtvscr) },
        { 0x180, GET(vaddcuw) },
        { 0xA, GET(vaddfp) },
        { 0x300, GET(vaddsbs) },
        { 0x340, GET(vaddshs) },
        { 0x380, GET(vaddsws) },
        { 0x0, GET(vaddubm) },
        { 0x200, GET(vaddubs) },
        { 0x40, GET(vadduhm) },
        { 0x240, GET(vadduhs) },
        { 0x80, GET(vadduwm) },
        { 0x280, GET(vadduws) },
        { 0x404, GET(vand) },
        { 0x444, GET(vandc) },
        { 0x502, GET(vavgsb) },
        { 0x542, GET(vavgsh) },
        { 0x582, GET(vavgsw) },
        { 0x402, GET(vavgub) },
        { 0x442, GET(vavguh) },
        { 0x482, GET(vavguw) },
        { 0x34A, GET(vcfsx) },
        { 0x30A, GET(vcfux) },
        { 0x3C6, GET(vcmpbfp) },
        { 0xC6, GET(vcmpeqfp) },
        { 0x6, GET(vcmpequb) },
        { 0x46, GET(vcmpequh) },
        { 0x86, GET(vcmpequw) },
        { 0x1C6, GET(vcmpgefp) },
        { 0x2C6, GET(vcmpgtfp) },
        { 0x3C6, GET(vcmpgtsb) },
        { 0x346, GET(vcmpgtsh) },
        { 0x386, GET(vcmpgtsw) },
        { 0x206, GET(vcmpgtub) },
        { 0x246, GET(vcmpgtuh) },
        { 0x286, GET(vcmpgtuw) },
        { 0x3CA, GET(vctsxs) },
        { 0x38A, GET(vctuxs) },
        { 0x18A, GET(vexptefp) },
        { 0x1CA, GET(vlogefp) },
        { 0x2E, GET(vmaddfp) },
        { 0x40A, GET(vmaxfp) },
        { 0x102, GET(vmaxsb) },
        { 0x142, GET(vmaxsh) },
        { 0x182, GET(vmaxsw) },
        { 0x2, GET(vmaxub) },
        { 0x42, GET(vmaxuh) },
        { 0x82, GET(vmaxuw) },
        { 0x20, GET(vmhaddshs) },
        { 0x21, GET(vmhraddshs) },
        { 0x44A, GET(vminfp) },
        { 0x302, GET(vminsb) },
        { 0x342, GET(vminsh) },
        { 0x382, GET(vminsw) },
        { 0x202, GET(vminub) },
        { 0x242, GET(vminuh) },
        { 0x282, GET(vminuw) },
        { 0x222, GET(vmladduhm) },
        { 0xC, GET(vmrghb) },
        { 0x4C, GET(vmrghh) },
        { 0x8C, GET(vmrghw) },
        { 0x10C, GET(vmrglb) },
        { 0x14C, GET(vmrglh) },
        { 0x18C, GET(vmrglw) },
        { 0x25, GET(vmsummbm) },
        { 0x28, GET(vmsumshm) },
        { 0x29, GET(vmsumshs) },
        { 0x24, GET(vmsumubm) },
        { 0x26, GET(vmsumuhm) },
        { 0x27, GET(vmsumuhs) },
        { 0x308, GET(vmulesb) },
        { 0x348, GET(vmulesh) },
        { 0x208, GET(vmuleub) },
        { 0x248, GET(vmuleuh) },
        { 0x108, GET(vmulosb) },
        { 0x148, GET(vmulosh) },
        { 0x8, GET(vmuloub) },
        { 0x48, GET(vmulouh) },
        { 0x2F, GET(vnmsubfp) },
        { 0x504, GET(vnor) },
        { 0x484, GET(vor) },
        { 0x2B, GET(vperm) },
        { 0x30E, GET(vpkpx) },
        { 0x18E, GET(vpkshss) },
        { 0x10E, GET(vpkshus) },
        { 0x1CE, GET(vpkswss) },
        { 0x14E, GET(vpkswus) },
        { 0xE, GET(vpkuhum) },
        { 0x8E, GET(vpkuhus) },
        { 0x4E, GET(vpkuwum) },
        { 0xCE, GET(vpkuwus) },
        { 0x10A, GET(vrefp) },
        { 0x2CA, GET(vrfim) },
        { 0x20A, GET(vrfin) },
        { 0x28A, GET(vrfip) },
        { 0x24A, GET(vrfiz) },
        { 0x4, GET(vrlb) },
        { 0x44, GET(vrlh) },
        { 0x84, GET(vrlw) },
        { 0x14A, GET(vrsqrtefp) },
        { 0x2A, GET(vsel) },
        { 0x1C4, GET(vsl) },
        { 0x104, GET(vslb) },
        { 0x2C, GET(vsldoi) },
        { 0x10, GET(vsldoi128) },
        { 0x144, GET(vslh) },
        { 0x40C, GET(vslo) },
        { 0x184, GET(vslw) },
        { 0x20C, GET(vspltb) },
        { 0x24C, GET(vsplth) },
        { 0x30C, GET(vspltisb) },
        { 0x34C, GET(vspltish) },
        { 0x38C, GET(vspltisw) },
        { 0x28C, GET(vspltw) },
        { 0x24C, GET(vsr) },
        { 0x304, GET(vsrab) },
        { 0x344, GET(vsrah) },
        { 0x384, GET(vsraw) },
        { 0x204, GET(vsrb) },
        { 0x244, GET(vsrh) },
        { 0x44C, GET(vsro) },
        { 0x284, GET(vsrw) },
        { 0x580, GET(vsubcuw) },
        { 0x4A, GET(vsubfp) },
        { 0x700, GET(vsubsbs) },
        { 0x740, GET(vsubshs) },
        { 0x780, GET(vsubsws) },
        { 0x400, GET(vsububm) },
        { 0x600, GET(vsububs) },
        { 0x440, GET(vsubuhm) },
        { 0x640, GET(vsubuhs) },
        { 0x480, GET(vsubuwm) },
        { 0x680, GET(vsubuws) },
        { 0x688, GET(vsum2sws) },
        { 0x708, GET(vsum4sbs) },
        { 0x648, GET(vsum4shs) },
        { 0x608, GET(vsum4ubs) },
        { 0x788, GET(vsumsws) },
        { 0x34E, GET(vupkhpx) },
        { 0x20E, GET(vupkhsb) },
        { 0x24E, GET(vupkhsh) },
        { 0x3CE, GET(vupklpx) },
        { 0x28E, GET(vupklsb) },
        { 0x28E, GET(vupklsb) },
        { 0x2CE, GET(vupklsh) },
        { 0x4C4, GET(vxor) },
      });
      // Group 0x13 opcodes (field 21..30)
      fillTable<std::string>(nameTable, 0x13, 10, 1, {
        { 0x000, GET(mcrf) },
        { 0x010, GET(bclr) },
        { 0x012, GET(rfid) },
        { 0x021, GET(crnor) },
        { 0x081, GET(crandc) },
        { 0x096, GET(isync) },
        { 0x0C1, GET(crxor) },
        { 0x0E1, GET(crnand) },
        { 0x101, GET(crand) },
        { 0x121, GET(creqv) },
        { 0x1A1, GET(crorc) },
        { 0x1C1, GET(cror) },
        { 0x210, GET(bcctr) },
      });
      // Group 0x1E opcodes (field 27..30)
      fillTable<std::string>(nameTable, 0x1E, 4, 1, {
        { 0x0, GETRC(rldicl) },
        { 0x1, GETRC(rldicl) },
        { 0x2, GETRC(rldicr) },
        { 0x3, GETRC(rldicr) },
        { 0x4, GETRC(rldic) },
        { 0x5, GETRC(rldic) },
        { 0x6, GETRC(rldimi) },
        { 0x7, GETRC(rldimi) },
        { 0x8, GETRC(rldcl) },
        { 0x9, GETRC(rldcr) },
      });
      // Group 0x1F opcodes (field 21..30)
      fillTable<std::string>(nameTable, 0x1F, 10, 1, {
        { 0x000, GET(cmp) },
        { 0x004, GET(tw) },
        { 0x006, GET(lvsl) },
        { 0x007, GET(lvebx) },
        { 0x008, GETRC(subfc) },
        { 0x208, GETRC(subfco) },
        { 0x009, GETRC(mulhdu) },
        { 0x00A, GETRC(addc) },
        { 0x20A, GETRC(addco) },
        { 0x00B, GETRC(mulhwu) },
        { 0x013, GET(mfocrf) },
        { 0x014, GET(lwarx) },
        { 0x015, GET(ldx) },
        { 0x017, GET(lwzx) },
        { 0x018, GETRC(slw) },
        { 0x01A, GETRC(cntlzw) },
        { 0x01B, GETRC(sld) },
        { 0x01C, GETRC(and) },
        { 0x020, GET(cmpl) },
        { 0x026, GET(lvsr) },
        { 0x027, GET(lvehx) },
        { 0x028, GETRC(subf) },
        { 0x228, GETRC(subfo) },
        { 0x035, GET(ldux) },
        { 0x036, GET(dcbst) },
        { 0x037, GET(lwzux) },
        { 0x03A, GETRC(cntlzd) },
        { 0x03C, GETRC(andc) },
        { 0x044, GET(td) },
        { 0x047, GET(lvewx) },
        { 0x049, GETRC(mulhd) },
        { 0x04B, GETRC(mulhw) },
        { 0x053, GET(mfmsr) },
        { 0x054, GET(ldarx) },
        { 0x056, GET(dcbf) },
        { 0x057, GET(lbzx) },
        { 0x067, GET(lvx) },
        { 0x068, GETRC(neg) },
        { 0x268, GETRC(nego) },
        { 0x077, GET(lbzux) },
        { 0x07C, GETRC(nor) },
        { 0x087, GET(stvebx) },
        { 0x088, GETRC(subfe) },
        { 0x288, GETRC(subfeo) },
        { 0x08A, GETRC(adde) },
        { 0x28A, GETRC(addeo) },
        { 0x090, GET(mtocrf) },
        { 0x092, GET(mtmsr) },
        { 0x095, GET(stdx) },
        { 0x096, GET(stwcx) },
        { 0x097, GET(stwx) },
        { 0x0A7, GET(stvehx) },
        { 0x0B2, GET(mtmsrd) },
        { 0x0B5, GET(stdux) },
        { 0x0B7, GET(stwux) },
        { 0x0C7, GET(stvewx) },
        { 0x0C8, GETRC(subfze) },
        { 0x2C8, GETRC(subfzeo) },
        { 0x0CA, GETRC(addze) },
        { 0x2CA, GETRC(addzeo) },
        { 0x0D6, GET(stdcx) },
        { 0x0D7, GET(stbx) },
        { 0x0E7, GET(stvx) },
        { 0x0E8, GETRC(subfme) },
        { 0x2E8, GETRC(subfmeo) },
        { 0x0E9, GETRC(mulld) },
        { 0x2E9, GETRC(mulldo) },
        { 0x0EA, GETRC(addme) },
        { 0x2EA, GETRC(addmeo) },
        { 0x0EB, GETRC(mullw) },
        { 0x2EB, GETRC(mullwo) },
        { 0x0F6, GET(dcbtst) },
        { 0x0F7, GET(stbux) },
        { 0x10A, GETRC(add) },
        { 0x30A, GETRC(addo) },
        { 0x116, GET(dcbt) },
        { 0x117, GET(lhzx) },
        { 0x11C, GETRC(eqv) },
        { 0x112, GET(tlbiel) },
        { 0x132, GET(tlbie) },
        { 0x136, GET(eciwx) },
        { 0x137, GET(lhzux) },
        { 0x13C, GETRC(xor) },
        { 0x153, GET(mfspr) },
        { 0x155, GET(lwax) },
        { 0x156, GET(dst) },
        { 0x157, GET(lhax) },
        { 0x167, GET(lvxl) },
        { 0x173, GET(mftb) },
        { 0x175, GET(lwaux) },
        { 0x176, GET(dstst) },
        { 0x177, GET(lhaux) },
        { 0x192, GET(slbmte) },
        { 0x197, GET(sthx) },
        { 0x19C, GET(orcx) },
        { 0x1B2, GET(slbie) },
        { 0x1B6, GET(ecowx) },
        { 0x1B7, GET(sthux) },
        { 0x1BC, GETRC(or) },
        { 0x1C9, GETRC(divdu) },
        { 0x3C9, GETRC(divduo) },
        { 0x1CB, GETRC(divwu) },
        { 0x3CB, GETRC(divwuo) },
        { 0x1D3, GET(mtspr) },
        { 0x1D6, GET(dcbi) },
        { 0x1DC, GETRC(nand) },
        { 0x1F2, GET(slbia) },
        { 0x1E7, GET(stvxl) },
        { 0x1E9, GETRC(divd) },
        { 0x3E9, GETRC(divdo) },
        { 0x1EB, GETRC(divw) },
        { 0x3EB, GETRC(divwo) },
        { 0x207, GET(lvlx) },
        { 0x214, GET(ldbrx) },
        { 0x215, GET(lswx) },
        { 0x216, GET(lwbrx) },
        { 0x217, GET(lfsx) },
        { 0x218, GETRC(srw) },
        { 0x21B, GETRC(srd) },
        { 0x227, GET(lvrx) },
        { 0x236, GET(tlbsync) },
        { 0x237, GET(lfsux) },
        { 0x239, GET(mfsrin) },
        { 0x253, GET(mfsr) },
        { 0x255, GET(lswi) },
        { 0x256, GET(sync) },
        { 0x257, GET(lfdx) },
        { 0x277, GET(lfdux) },
        { 0x287, GET(stvlx) },
        { 0x294, GET(stdbrx) },
        { 0x295, GET(stswx) },
        { 0x296, GET(stwbrx) },
        { 0x297, GET(stfsx) },
        { 0x2A7, GET(stvrx) },
        { 0x2B7, GET(stfsux) },
        { 0x2D5, GET(stswi) },
        { 0x2D7, GET(stfdx) },
        { 0x2F7, GET(stfdux) },
        { 0x307, GET(lvlxl) },
        { 0x316, GET(lhbrx) },
        { 0x318, GETRC(sraw) },
        { 0x31A, GETRC(srad) },
        { 0x327, GET(lvrxl) },
        { 0x336, GET(dss) },
        { 0x338, GETRC(srawi) },
        { 0x33A, GETRC(sradi) },
        { 0x33B, GETRC(sradi) },
        { 0x353, GET(slbmfev) },
        { 0x356, GET(eieio) },
        { 0x387, GET(stvlxl) },
        { 0x393, GET(slbmfee) },
        { 0x396, GET(sthbrx) },
        { 0x39A, GETRC(extsh) },
        { 0x3A7, GET(stvrxl) },
        { 0x3BA, GETRC(extsb) },
        { 0x3D7, GET(stfiwx) },
        { 0x3DA, GETRC(extsw) },
        { 0x3D6, GET(icbi) },
        { 0x3F6, GET(dcbz) },
      });
      // Group 0x3A opcodes (field 30..31)
      fillTable<std::string>(nameTable, 0x3A, 2, 0, {
        { 0x0, GET(ld) },
        { 0x1, GET(ldu) },
        { 0x2, GET(lwa) },
      });
      // Group 0x3B opcodes (field 21..30)
      fillTable<std::string>(nameTable, 0x3B, 10, 1, {
        { 0x12, GETRC(fdivs), 5 },
        { 0x14, GETRC(fsubs), 5 },
        { 0x15, GETRC(fadds), 5 },
        { 0x16, GETRC(fsqrts), 5 },
        { 0x18, GETRC(fres), 5 },
        { 0x19, GETRC(fmuls), 5 },
        { 0x1C, GETRC(fmsubs), 5 },
        { 0x1D, GETRC(fmadds), 5 },
        { 0x1E, GETRC(fnmsubs), 5 },
        { 0x1F, GETRC(fnmadds), 5 },
      });
      // Group 0x3E opcodes (field 30..31)
      fillTable<std::string>(nameTable, 0x3E, 2, 0, {
        { 0x0, GET(std) },
        { 0x1, GET(stdu) },
      });
      // Group 0x3F opcodes (field 21..30)
      fillTable<std::string>(nameTable, 0x3F, 10, 1, {
        { 0x026, GETRC(mtfsb1) },
        { 0x040, GET(mcrfs) },
        { 0x046, GETRC(mtfsb0) },
        { 0x086, GETRC(mtfsfi) },
        { 0x247, GETRC(mffs) },
        { 0x2C7, GETRC(mtfsf) },

        { 0x000, GET(fcmpu) },
        { 0x00C, GETRC(frsp) },
        { 0x00E, GETRC(fctiw) },
        { 0x00F, GETRC(fctiwz) },

        { 0x012, GETRC(fdiv), 5 },
        { 0x014, GETRC(fsub), 5 },
        { 0x015, GETRC(fadd), 5 },
        { 0x016, GETRC(fsqrt), 5 },
        { 0x017, GETRC(fsel), 5 },
        { 0x019, GETRC(fmul), 5 },
        { 0x01A, GETRC(frsqrte), 5 },
        { 0x01C, GETRC(fmsub), 5 },
        { 0x01D, GETRC(fmadd), 5 },
        { 0x01E, GETRC(fnmsub), 5 },
        { 0x01F, GETRC(fnmadd), 5 },

        { 0x020, GET(fcmpo) },
        { 0x028, GETRC(fneg) },
        { 0x048, GETRC(fmr) },
        { 0x088, GETRC(fnabs) },
        { 0x108, GETRC(fabs) },
        { 0x32E, GETRC(fctid) },
        { 0x32F, GETRC(fctidz) },
        { 0x34E, GETRC(fcfid) },
      });
    }
    PPCDecoder();
    ~PPCDecoder() = default;
    const std::array<instructionHandler, 0x20000> &getTable() const noexcept {
      return table;
    }
    const std::array<instructionHandlerJIT, 0x20000>& getJITTable() const noexcept {
      return jitTable;
    }
    instructionHandler decode(u32 instr) const noexcept {
      if (instr == 0x60000000) {
        return &PPCInterpreter_nop;
      }
      return getTable()[PPCDecode(instr)];
    }
    instructionHandlerJIT decodeJIT(u32 instr) const noexcept {
      return getJITTable()[PPCDecode(instr)];
    }
    const std::array<std::string, 0x20000> &getNameTable() const noexcept {
      return nameTable;
    }
    std::string decodeName(u32 instr) const noexcept {
      return getNameTable()[PPCDecode(instr)];
    }
    bool isBranch(u32 decoded) const noexcept {
      return decoded == 0x0012 // b
          || decoded == 0x0010 // bc
          || decoded == 0x6490 // bclr (0x13 << 11 | 0x010)
          || decoded == 0x7290 // bcctr (0x13 << 11 | 0x210)
          || decoded == 0x6590;// rfid  (0x13 << 11 | 0x012)
    }
  private:
    // Fast lookup table
    std::array<instructionHandler, 0x20000> table;
    std::array<instructionHandlerJIT, 0x20000> jitTable;
    std::array<std::string, 0x20000> nameTable;

    template <typename T>
    void fillTable(std::array<T, 0x20000> &t, u32 mainOp, u32 count, u32 sh, std::initializer_list<InstrInfo<T>> entries) noexcept {
      if (sh < 11) {
        for (const auto& v : entries) {
          for (u32 i = 0; i < 1u << (v.magn + (11 - sh - count)); i++) {
            for (u32 j = 0; j < 1u << sh; j++) {
              const u32 k = (((i << (count - v.magn)) | v.value) << sh) | j;
              c_at(t, (k << 6) | mainOp) = k & 1 ? v.ptrRc : v.ptr0;
            }
          }
        }
      }
      else {
        // Special case opcodes
        for (const auto& v : entries) {
          for (u32 i = 0; i < 1u << 11; i++) {
            c_at(t, i << 6 | v.value) = i & 1 ? v.ptrRc : v.ptr0;
          }
        }
      }
    }
  };
} // namespace PPCInterpreter
