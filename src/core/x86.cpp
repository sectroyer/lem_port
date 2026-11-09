// SPDX-License-Identifier: GPL-3.0-or-later
#include "x86.h"

#include <cstdio>

namespace lem {

namespace {

enum Flag { CF = 0, PF = 2, AF = 4, ZF = 6, SF = 7, TF = 8, IF = 9, DF = 10, OF = 11 };

constexpr uint16_t kReturnSeg = 0xF000;   // HLT placed here ends a call

bool parity(uint32_t v) {
    v &= 0xFF;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return !(v & 1);
}

} // namespace

Cpu8086::Cpu8086() : mem(1 << 20, 0) {
    s[SS] = 0x9000;
    r[SP] = 0xFFFE;
    mem[uint32_t(kReturnSeg) << 4] = 0xF4;   // HLT
}

uint16_t Cpu8086::read16(uint16_t seg, uint16_t off) {
    return uint16_t(at(seg, off) | at(seg, uint16_t(off + 1)) << 8);
}

void Cpu8086::write16(uint16_t seg, uint16_t off, uint16_t v) {
    at(seg, off) = uint8_t(v);
    at(seg, uint16_t(off + 1)) = uint8_t(v >> 8);
}

void Cpu8086::push(uint16_t v) {
    r[SP] -= 2;
    write16(s[SS], r[SP], v);
}

uint16_t Cpu8086::pop() {
    uint16_t v = read16(s[SS], r[SP]);
    r[SP] += 2;
    return v;
}

bool Cpu8086::callInterrupt(uint16_t seg, uint16_t off, long maxSteps) {
    push(flags);
    push(kReturnSeg);
    push(0);
    s[CS] = seg;
    ip = off;
    halted_ = false;
    error_.clear();
    for (long n = 0; n < maxSteps; ++n) {
        if (!step()) return false;
        if (halted_) return s[CS] == kReturnSeg;
    }
    error_ = "step limit reached";
    return false;
}

// ---------------------------------------------------------------------------
// Operand decoding

Cpu8086::ModRM Cpu8086::decode() {
    uint8_t b = fetch8();
    ModRM m{b >> 6, (b >> 3) & 7, b & 7, 0, 0};
    if (m.mod == 3) return m;
    uint16_t off = 0;
    bool bpBased = false;
    switch (m.rm) {
    case 0: off = r[BX] + r[SI]; break;
    case 1: off = r[BX] + r[DI]; break;
    case 2: off = r[BP] + r[SI]; bpBased = true; break;
    case 3: off = r[BP] + r[DI]; bpBased = true; break;
    case 4: off = r[SI]; break;
    case 5: off = r[DI]; break;
    case 6:
        if (m.mod == 0) off = fetch16();
        else { off = r[BP]; bpBased = true; }
        break;
    case 7: off = r[BX]; break;
    }
    if (m.mod == 1) off += uint16_t(int8_t(fetch8()));
    else if (m.mod == 2) off += fetch16();
    m.off = off;
    m.seg = segOverride_ >= 0 ? s[segOverride_] : s[bpBased ? SS : DS];
    return m;
}

uint8_t Cpu8086::getReg8(int i) const { return i < 4 ? uint8_t(r[i]) : uint8_t(r[i - 4] >> 8); }

void Cpu8086::setReg8(int i, uint8_t v) {
    if (i < 4) r[i] = uint16_t((r[i] & 0xFF00) | v);
    else r[i - 4] = uint16_t((r[i - 4] & 0x00FF) | v << 8);
}

uint8_t Cpu8086::getRM8(const ModRM& m) { return m.mod == 3 ? getReg8(m.rm) : at(m.seg, m.off); }
void Cpu8086::setRM8(const ModRM& m, uint8_t v) {
    if (m.mod == 3) setReg8(m.rm, v);
    else at(m.seg, m.off) = v;
}
uint16_t Cpu8086::getRM16(const ModRM& m) { return m.mod == 3 ? r[m.rm] : read16(m.seg, m.off); }
void Cpu8086::setRM16(const ModRM& m, uint16_t v) {
    if (m.mod == 3) r[m.rm] = v;
    else write16(m.seg, m.off, v);
}

// ---------------------------------------------------------------------------
// Arithmetic

void Cpu8086::setSZP(uint32_t v, bool word) {
    uint32_t mask = word ? 0xFFFF : 0xFF;
    v &= mask;
    setFlag(ZF, v == 0);
    setFlag(SF, v & (word ? 0x8000 : 0x80));
    setFlag(PF, parity(v));
}

uint32_t Cpu8086::alu(int op, uint32_t a, uint32_t b, bool word) {
    uint32_t mask = word ? 0xFFFF : 0xFF, sign = word ? 0x8000 : 0x80, res = 0;
    a &= mask;
    b &= mask;
    switch (op) {
    case 0: case 2: {   // ADD, ADC
        uint32_t c = op == 2 && flag(CF) ? 1 : 0;
        res = a + b + c;
        setFlag(CF, res > mask);
        setFlag(OF, ((a ^ res) & (b ^ res) & sign) != 0);
        setFlag(AF, ((a ^ b ^ res) & 0x10) != 0);
        break;
    }
    case 3: case 5: case 7: {   // SBB, SUB, CMP
        uint32_t c = op == 3 && flag(CF) ? 1 : 0;
        res = a - b - c;
        setFlag(CF, b + c > a);
        setFlag(OF, ((a ^ b) & (a ^ res) & sign) != 0);
        setFlag(AF, ((a ^ b ^ res) & 0x10) != 0);
        break;
    }
    case 1: res = a | b; setFlag(CF, false); setFlag(OF, false); break;
    case 4: res = a & b; setFlag(CF, false); setFlag(OF, false); break;
    case 6: res = a ^ b; setFlag(CF, false); setFlag(OF, false); break;
    }
    res &= mask;
    setSZP(res, word);
    return res;
}

uint32_t Cpu8086::shift(int op, uint32_t v, int count, bool word) {
    uint32_t mask = word ? 0xFFFF : 0xFF, top = word ? 0x8000 : 0x80;
    int bits = word ? 16 : 8;
    count &= 0x1F;
    v &= mask;
    if (count == 0) return v;
    for (int i = 0; i < count; ++i) {
        bool c;
        switch (op) {
        case 0: c = v & top; v = ((v << 1) | (c ? 1 : 0)) & mask; setFlag(CF, c); break;          // ROL
        case 1: c = v & 1; v = (v >> 1) | (c ? top : 0); setFlag(CF, c); break;                   // ROR
        case 2: c = v & top; v = ((v << 1) | (flag(CF) ? 1 : 0)) & mask; setFlag(CF, c); break;   // RCL
        case 3: c = v & 1; v = (v >> 1) | (flag(CF) ? top : 0); setFlag(CF, c); break;            // RCR
        case 4: case 6: setFlag(CF, v & top); v = (v << 1) & mask; break;                       // SHL
        case 5: setFlag(CF, v & 1); v >>= 1; break;                                             // SHR
        case 7: setFlag(CF, v & 1); v = (v >> 1) | (v & top); break;                            // SAR
        }
    }
    (void)bits;
    if (op >= 4) setSZP(v, word);
    // OF is only defined for single-bit shifts; use the usual definition.
    switch (op) {
    case 0: case 2: case 4: case 6: setFlag(OF, ((v & top) != 0) != flag(CF)); break;
    case 1: case 3: setFlag(OF, ((v ^ (v << 1)) & top) != 0); break;
    case 5: setFlag(OF, false); break;
    case 7: setFlag(OF, false); break;
    }
    return v;
}

bool Cpu8086::cond(int cc) const {
    bool res;
    switch (cc >> 1) {
    case 0: res = flag(OF); break;
    case 1: res = flag(CF); break;
    case 2: res = flag(ZF); break;
    case 3: res = flag(CF) || flag(ZF); break;
    case 4: res = flag(SF); break;
    case 5: res = flag(PF); break;
    case 6: res = flag(SF) != flag(OF); break;
    default: res = flag(ZF) || flag(SF) != flag(OF); break;
    }
    return (cc & 1) ? !res : res;
}

// ---------------------------------------------------------------------------
// Execution

bool Cpu8086::step() {
    segOverride_ = -1;
    int rep = 0;   // 0 none, 1 REPNE, 2 REP/REPE
    uint16_t startIp = ip;
    uint8_t op;
    for (;;) {
        op = fetch8();
        if (op == 0x26) segOverride_ = ES;
        else if (op == 0x2E) segOverride_ = CS;
        else if (op == 0x36) segOverride_ = SS;
        else if (op == 0x3E) segOverride_ = DS;
        else if (op == 0xF2) rep = 1;
        else if (op == 0xF3) rep = 2;
        else if (op == 0xF0) {}
        else break;
    }
    uint16_t dataSeg = segOverride_ >= 0 ? s[segOverride_] : s[DS];

    // ALU block 00..3F.
    if (op < 0x40 && (op & 7) < 6) {
        int aop = op >> 3, form = op & 7;
        switch (form) {
        case 0: { ModRM m = decode(); uint32_t v = alu(aop, getRM8(m), getReg8(m.reg), false); if (aop != 7) setRM8(m, uint8_t(v)); break; }
        case 1: { ModRM m = decode(); uint32_t v = alu(aop, getRM16(m), r[m.reg], true); if (aop != 7) setRM16(m, uint16_t(v)); break; }
        case 2: { ModRM m = decode(); uint32_t v = alu(aop, getReg8(m.reg), getRM8(m), false); if (aop != 7) setReg8(m.reg, uint8_t(v)); break; }
        case 3: { ModRM m = decode(); uint32_t v = alu(aop, r[m.reg], getRM16(m), true); if (aop != 7) r[m.reg] = uint16_t(v); break; }
        case 4: { uint32_t v = alu(aop, r[AX] & 0xFF, fetch8(), false); if (aop != 7) setReg8(0, uint8_t(v)); break; }
        case 5: { uint32_t v = alu(aop, r[AX], fetch16(), true); if (aop != 7) r[AX] = uint16_t(v); break; }
        }
        return true;
    }

    auto stringOp = [&](auto body, bool usesZf) {
        if (!rep) { body(); return; }
        while (r[CX] != 0) {
            body();
            --r[CX];
            if (usesZf && ((rep == 2) != flag(ZF))) break;
        }
    };
    int delta = flag(DF) ? -1 : 1;

    switch (op) {
    case 0x06: push(s[ES]); break;
    case 0x07: s[ES] = pop(); break;
    case 0x0E: push(s[CS]); break;
    case 0x16: push(s[SS]); break;
    case 0x17: s[SS] = pop(); break;
    case 0x1E: push(s[DS]); break;
    case 0x1F: s[DS] = pop(); break;
    case 0x27: case 0x2F: case 0x37: case 0x3F:
        error_ = "BCD adjust instruction not supported";
        return false;

    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: {
        bool c = flag(CF);
        r[op & 7] = uint16_t(alu(0, r[op & 7], 1, true));
        setFlag(CF, c);
        break;
    }
    case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F: {
        bool c = flag(CF);
        r[op & 7] = uint16_t(alu(5, r[op & 7], 1, true));
        setFlag(CF, c);
        break;
    }
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        push(r[op & 7]);
        break;
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        r[op & 7] = pop();
        break;
    case 0x60: {
        uint16_t sp = r[SP];
        push(r[AX]); push(r[CX]); push(r[DX]); push(r[BX]); push(sp); push(r[BP]); push(r[SI]); push(r[DI]);
        break;
    }
    case 0x61:
        r[DI] = pop(); r[SI] = pop(); r[BP] = pop(); pop(); r[BX] = pop(); r[DX] = pop(); r[CX] = pop(); r[AX] = pop();
        break;
    case 0x68: push(fetch16()); break;
    case 0x6A: push(uint16_t(int8_t(fetch8()))); break;
    case 0x69: case 0x6B: {
        ModRM m = decode();
        int32_t a = int16_t(getRM16(m));
        int32_t b = op == 0x69 ? int16_t(fetch16()) : int8_t(fetch8());
        int32_t p = a * b;
        r[m.reg] = uint16_t(p);
        bool o = p != int16_t(p);
        setFlag(CF, o);
        setFlag(OF, o);
        break;
    }

    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
        int8_t d = int8_t(fetch8());
        if (cond(op & 0xF)) ip = uint16_t(ip + d);
        break;
    }

    case 0x80: case 0x82: { ModRM m = decode(); uint8_t imm = fetch8(); uint32_t v = alu(m.reg, getRM8(m), imm, false); if (m.reg != 7) setRM8(m, uint8_t(v)); break; }
    case 0x81: { ModRM m = decode(); uint16_t imm = fetch16(); uint32_t v = alu(m.reg, getRM16(m), imm, true); if (m.reg != 7) setRM16(m, uint16_t(v)); break; }
    case 0x83: { ModRM m = decode(); uint16_t imm = uint16_t(int8_t(fetch8())); uint32_t v = alu(m.reg, getRM16(m), imm, true); if (m.reg != 7) setRM16(m, uint16_t(v)); break; }
    case 0x84: { ModRM m = decode(); alu(4, getRM8(m), getReg8(m.reg), false); break; }
    case 0x85: { ModRM m = decode(); alu(4, getRM16(m), r[m.reg], true); break; }
    case 0x86: { ModRM m = decode(); uint8_t a = getRM8(m); setRM8(m, getReg8(m.reg)); setReg8(m.reg, a); break; }
    case 0x87: { ModRM m = decode(); uint16_t a = getRM16(m); setRM16(m, r[m.reg]); r[m.reg] = a; break; }
    case 0x88: { ModRM m = decode(); setRM8(m, getReg8(m.reg)); break; }
    case 0x89: { ModRM m = decode(); setRM16(m, r[m.reg]); break; }
    case 0x8A: { ModRM m = decode(); setReg8(m.reg, getRM8(m)); break; }
    case 0x8B: { ModRM m = decode(); r[m.reg] = getRM16(m); break; }
    case 0x8C: { ModRM m = decode(); setRM16(m, s[m.reg & 3]); break; }
    case 0x8D: { ModRM m = decode(); r[m.reg] = m.off; break; }
    case 0x8E: { ModRM m = decode(); s[m.reg & 3] = getRM16(m); break; }
    case 0x8F: { ModRM m = decode(); setRM16(m, pop()); break; }

    case 0x90: break;
    case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: {
        uint16_t t = r[AX]; r[AX] = r[op & 7]; r[op & 7] = t;
        break;
    }
    case 0x98: r[AX] = uint16_t(int8_t(r[AX] & 0xFF)); break;
    case 0x99: r[DX] = (r[AX] & 0x8000) ? 0xFFFF : 0; break;
    case 0x9A: { uint16_t o = fetch16(), sg = fetch16(); push(s[CS]); push(ip); s[CS] = sg; ip = o; break; }
    case 0x9C: push(flags); break;
    case 0x9D: flags = uint16_t((pop() & 0x0FD5) | 0x0002); break;
    case 0x9E: flags = uint16_t((flags & 0xFF00) | ((r[AX] >> 8) & 0xD5) | 0x02); break;
    case 0x9F: setReg8(4, uint8_t(flags)); break;

    case 0xA0: setReg8(0, at(dataSeg, fetch16())); break;
    case 0xA1: r[AX] = read16(dataSeg, fetch16()); break;
    case 0xA2: at(dataSeg, fetch16()) = uint8_t(r[AX]); break;
    case 0xA3: write16(dataSeg, fetch16(), r[AX]); break;
    case 0xA4: stringOp([&] { at(s[ES], r[DI]) = at(dataSeg, r[SI]); r[SI] += delta; r[DI] += delta; }, false); break;
    case 0xA5: stringOp([&] { write16(s[ES], r[DI], read16(dataSeg, r[SI])); r[SI] += 2 * delta; r[DI] += 2 * delta; }, false); break;
    case 0xA6: stringOp([&] { alu(7, at(dataSeg, r[SI]), at(s[ES], r[DI]), false); r[SI] += delta; r[DI] += delta; }, true); break;
    case 0xA7: stringOp([&] { alu(7, read16(dataSeg, r[SI]), read16(s[ES], r[DI]), true); r[SI] += 2 * delta; r[DI] += 2 * delta; }, true); break;
    case 0xA8: alu(4, r[AX] & 0xFF, fetch8(), false); break;
    case 0xA9: alu(4, r[AX], fetch16(), true); break;
    case 0xAA: stringOp([&] { at(s[ES], r[DI]) = uint8_t(r[AX]); r[DI] += delta; }, false); break;
    case 0xAB: stringOp([&] { write16(s[ES], r[DI], r[AX]); r[DI] += 2 * delta; }, false); break;
    case 0xAC: stringOp([&] { setReg8(0, at(dataSeg, r[SI])); r[SI] += delta; }, false); break;
    case 0xAD: stringOp([&] { r[AX] = read16(dataSeg, r[SI]); r[SI] += 2 * delta; }, false); break;
    case 0xAE: stringOp([&] { alu(7, r[AX] & 0xFF, at(s[ES], r[DI]), false); r[DI] += delta; }, true); break;
    case 0xAF: stringOp([&] { alu(7, r[AX], read16(s[ES], r[DI]), true); r[DI] += 2 * delta; }, true); break;

    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        setReg8(op & 7, fetch8());
        break;
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
        r[op & 7] = fetch16();
        break;

    case 0xC0: { ModRM m = decode(); uint8_t c = fetch8(); setRM8(m, uint8_t(shift(m.reg, getRM8(m), c, false))); break; }
    case 0xC1: { ModRM m = decode(); uint8_t c = fetch8(); setRM16(m, uint16_t(shift(m.reg, getRM16(m), c, true))); break; }
    case 0xC2: { uint16_t n = fetch16(); ip = pop(); r[SP] += n; break; }
    case 0xC3: ip = pop(); break;
    case 0xC4: { ModRM m = decode(); r[m.reg] = read16(m.seg, m.off); s[ES] = read16(m.seg, uint16_t(m.off + 2)); break; }
    case 0xC5: { ModRM m = decode(); r[m.reg] = read16(m.seg, m.off); s[DS] = read16(m.seg, uint16_t(m.off + 2)); break; }
    case 0xC6: { ModRM m = decode(); setRM8(m, fetch8()); break; }
    case 0xC7: { ModRM m = decode(); setRM16(m, fetch16()); break; }
    case 0xCA: { uint16_t n = fetch16(); ip = pop(); s[CS] = pop(); r[SP] += n; break; }
    case 0xCB: ip = pop(); s[CS] = pop(); break;
    case 0xCC: case 0xCD: case 0xCE: {
        int n = op == 0xCC ? 3 : op == 0xCD ? fetch8() : 4;
        if (op == 0xCE && !flag(OF)) break;
        uint16_t vo = read16(0, uint16_t(n * 4)), vs = read16(0, uint16_t(n * 4 + 2));
        if (vo == 0 && vs == 0) break;   // no handler installed: ignore
        push(flags); push(s[CS]); push(ip);
        setFlag(IF, false); setFlag(TF, false);
        s[CS] = vs; ip = vo;
        break;
    }
    case 0xCF: ip = pop(); s[CS] = pop(); flags = uint16_t((pop() & 0x0FD5) | 0x0002); break;

    case 0xD0: { ModRM m = decode(); setRM8(m, uint8_t(shift(m.reg, getRM8(m), 1, false))); break; }
    case 0xD1: { ModRM m = decode(); setRM16(m, uint16_t(shift(m.reg, getRM16(m), 1, true))); break; }
    case 0xD2: { ModRM m = decode(); setRM8(m, uint8_t(shift(m.reg, getRM8(m), r[CX] & 0xFF, false))); break; }
    case 0xD3: { ModRM m = decode(); setRM16(m, uint16_t(shift(m.reg, getRM16(m), r[CX] & 0xFF, true))); break; }
    case 0xD4: {
        uint8_t base = fetch8();
        if (!base) { error_ = "AAM by zero"; return false; }
        uint8_t al = uint8_t(r[AX]);
        r[AX] = uint16_t((al / base) << 8 | (al % base));
        setSZP(r[AX] & 0xFF, false);
        break;
    }
    case 0xD5: {
        uint8_t base = fetch8();
        r[AX] = uint16_t((uint8_t(r[AX]) + uint8_t(r[AX] >> 8) * base) & 0xFF);
        setSZP(r[AX], false);
        break;
    }
    case 0xD7: setReg8(0, at(dataSeg, uint16_t(r[BX] + (r[AX] & 0xFF)))); break;

    case 0xE0: case 0xE1: case 0xE2: {
        int8_t d = int8_t(fetch8());
        --r[CX];
        bool take = r[CX] != 0 && (op == 0xE2 || (op == 0xE1 ? flag(ZF) : !flag(ZF)));
        if (take) ip = uint16_t(ip + d);
        break;
    }
    case 0xE3: { int8_t d = int8_t(fetch8()); if (r[CX] == 0) ip = uint16_t(ip + d); break; }
    case 0xE4: setReg8(0, in ? in(fetch8()) : 0xFF); break;
    case 0xE5: { uint8_t p = fetch8(); r[AX] = uint16_t(in ? in(p) | in(uint16_t(p + 1)) << 8 : 0xFFFF); break; }
    case 0xE6: { uint8_t p = fetch8(); if (out) out(p, uint8_t(r[AX])); break; }
    case 0xE7: { uint8_t p = fetch8(); if (out) { out(p, uint8_t(r[AX])); out(uint16_t(p + 1), uint8_t(r[AX] >> 8)); } break; }
    case 0xE8: { int16_t d = int16_t(fetch16()); push(ip); ip = uint16_t(ip + d); break; }
    case 0xE9: { int16_t d = int16_t(fetch16()); ip = uint16_t(ip + d); break; }
    case 0xEA: { uint16_t o = fetch16(), sg = fetch16(); s[CS] = sg; ip = o; break; }
    case 0xEB: { int8_t d = int8_t(fetch8()); ip = uint16_t(ip + d); break; }
    case 0xEC: setReg8(0, in ? in(r[DX]) : 0xFF); break;
    case 0xED: r[AX] = uint16_t(in ? in(r[DX]) | in(uint16_t(r[DX] + 1)) << 8 : 0xFFFF); break;
    case 0xEE: if (out) out(r[DX], uint8_t(r[AX])); break;
    case 0xEF: if (out) { out(r[DX], uint8_t(r[AX])); out(uint16_t(r[DX] + 1), uint8_t(r[AX] >> 8)); } break;

    case 0xF4: halted_ = true; break;
    case 0xF5: setFlag(CF, !flag(CF)); break;
    case 0xF6: case 0xF7: {
        bool w = op == 0xF7;
        ModRM m = decode();
        uint32_t v = w ? getRM16(m) : getRM8(m);
        switch (m.reg) {
        case 0: case 1: alu(4, v, w ? fetch16() : fetch8(), w); break;
        case 2: if (w) setRM16(m, uint16_t(~v)); else setRM8(m, uint8_t(~v)); break;
        case 3: {
            uint32_t res = alu(5, 0, v, w);
            setFlag(CF, (v & (w ? 0xFFFF : 0xFF)) != 0);
            if (w) setRM16(m, uint16_t(res)); else setRM8(m, uint8_t(res));
            break;
        }
        case 4:
            if (w) { uint32_t p = uint32_t(r[AX]) * v; r[AX] = uint16_t(p); r[DX] = uint16_t(p >> 16); setFlag(CF, r[DX] != 0); setFlag(OF, r[DX] != 0); }
            else { uint16_t p = uint16_t((r[AX] & 0xFF) * v); r[AX] = p; setFlag(CF, (p >> 8) != 0); setFlag(OF, (p >> 8) != 0); }
            break;
        case 5:
            if (w) { int32_t p = int32_t(int16_t(r[AX])) * int16_t(v); r[AX] = uint16_t(p); r[DX] = uint16_t(uint32_t(p) >> 16); bool o = p != int16_t(p); setFlag(CF, o); setFlag(OF, o); }
            else { int16_t p = int16_t(int8_t(r[AX] & 0xFF) * int8_t(v)); r[AX] = uint16_t(p); bool o = p != int8_t(p); setFlag(CF, o); setFlag(OF, o); }
            break;
        case 6:
            if (v == 0) { error_ = "division by zero"; return false; }
            if (w) {
                uint32_t n = uint32_t(r[DX]) << 16 | r[AX], q = n / v;
                if (q > 0xFFFF) { error_ = "division overflow"; return false; }
                r[AX] = uint16_t(q); r[DX] = uint16_t(n % v);
            } else {
                uint16_t n = r[AX], q = uint16_t(n / v);
                if (q > 0xFF) { error_ = "division overflow"; return false; }
                r[AX] = uint16_t((n % v) << 8 | q);
            }
            break;
        case 7:
            if (v == 0) { error_ = "division by zero"; return false; }
            if (w) {
                int32_t n = int32_t(uint32_t(r[DX]) << 16 | r[AX]), d = int16_t(v), q = n / d;
                if (q != int16_t(q)) { error_ = "division overflow"; return false; }
                r[AX] = uint16_t(q); r[DX] = uint16_t(n % d);
            } else {
                int16_t n = int16_t(r[AX]), d = int8_t(v), q = int16_t(n / d);
                if (q != int8_t(q)) { error_ = "division overflow"; return false; }
                r[AX] = uint16_t(uint8_t(n % d) << 8 | uint8_t(q));
            }
            break;
        }
        break;
    }
    case 0xF8: setFlag(CF, false); break;
    case 0xF9: setFlag(CF, true); break;
    case 0xFA: setFlag(IF, false); break;
    case 0xFB: setFlag(IF, true); break;
    case 0xFC: setFlag(DF, false); break;
    case 0xFD: setFlag(DF, true); break;
    case 0xFE: {
        ModRM m = decode();
        bool c = flag(CF);
        if (m.reg == 0) setRM8(m, uint8_t(alu(0, getRM8(m), 1, false)));
        else if (m.reg == 1) setRM8(m, uint8_t(alu(5, getRM8(m), 1, false)));
        else { error_ = "bad FE opcode"; return false; }
        setFlag(CF, c);
        break;
    }
    case 0xFF: {
        ModRM m = decode();
        switch (m.reg) {
        case 0: { bool c = flag(CF); setRM16(m, uint16_t(alu(0, getRM16(m), 1, true))); setFlag(CF, c); break; }
        case 1: { bool c = flag(CF); setRM16(m, uint16_t(alu(5, getRM16(m), 1, true))); setFlag(CF, c); break; }
        case 2: { uint16_t t = getRM16(m); push(ip); ip = t; break; }
        case 3: { uint16_t o = read16(m.seg, m.off), sg = read16(m.seg, uint16_t(m.off + 2)); push(s[CS]); push(ip); s[CS] = sg; ip = o; break; }
        case 4: ip = getRM16(m); break;
        case 5: { uint16_t o = read16(m.seg, m.off), sg = read16(m.seg, uint16_t(m.off + 2)); s[CS] = sg; ip = o; break; }
        case 6: push(getRM16(m)); break;
        default: error_ = "bad FF opcode"; return false;
        }
        break;
    }
    default: {
        char buf[64];
        std::snprintf(buf, sizeof buf, "unsupported opcode %02X at %04X:%04X", op, s[CS], startIp);
        error_ = buf;
        return false;
    }
    }
    return true;
}

} // namespace lem
