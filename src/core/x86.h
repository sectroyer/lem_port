// SPDX-License-Identifier: GPL-3.0-or-later
// Small real-mode 8086 interpreter, used to run the original sound driver.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace lem {

class Cpu8086 {
public:
    enum Reg16 { AX, CX, DX, BX, SP, BP, SI, DI };
    enum SegReg { ES, CS, SS, DS };

    Cpu8086();

    std::vector<uint8_t> mem;            // 1 MiB address space
    uint16_t r[8]{};
    uint16_t s[4]{};
    uint16_t ip = 0;
    uint16_t flags = 0x0002;

    std::function<void(uint16_t port, uint8_t value)> out;
    std::function<uint8_t(uint16_t port)> in;

    uint8_t& at(uint16_t seg, uint16_t off) { return mem[(uint32_t(seg) << 4) + off & 0xFFFFF]; }
    uint16_t read16(uint16_t seg, uint16_t off);
    void write16(uint16_t seg, uint16_t off, uint16_t v);

    // Calls an interrupt-style routine at seg:off (it must end with IRET).
    // Returns false if execution failed (unknown opcode or step limit).
    bool callInterrupt(uint16_t seg, uint16_t off, long maxSteps = 2000000);

    const std::string& error() const { return error_; }

private:
    bool step();
    uint8_t fetch8() { return at(s[CS], ip++); }
    uint16_t fetch16() { uint16_t v = read16(s[CS], ip); ip += 2; return v; }
    void push(uint16_t v);
    uint16_t pop();

    // ModR/M handling.
    struct ModRM { int mod, reg, rm; uint16_t seg, off; };
    ModRM decode();
    uint8_t getRM8(const ModRM& m);
    void setRM8(const ModRM& m, uint8_t v);
    uint16_t getRM16(const ModRM& m);
    void setRM16(const ModRM& m, uint16_t v);
    uint8_t getReg8(int i) const;
    void setReg8(int i, uint8_t v);

    // Arithmetic with flags.
    uint32_t alu(int op, uint32_t a, uint32_t b, bool word);
    uint32_t shift(int op, uint32_t v, int count, bool word);
    void setSZP(uint32_t v, bool word);
    bool cond(int cc) const;
    bool flag(int bit) const { return flags >> bit & 1; }
    void setFlag(int bit, bool on) { flags = on ? flags | (1u << bit) : flags & ~(1u << bit); }

    int segOverride_ = -1;
    std::string error_;
    bool halted_ = false;
};

} // namespace lem
