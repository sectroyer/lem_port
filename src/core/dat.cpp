// SPDX-License-Identifier: GPL-3.0-or-later
#include "dat.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace lem {

namespace {

// Reads the bitstream backwards: starting at the last byte, least significant
// bit first. Only `bitsInFirst` bits are taken from the last byte.
class BackwardBitReader {
public:
    BackwardBitReader(const uint8_t* src, size_t len, int bitsInFirst)
        : src_(src), pos_(len) {
        if (len == 0) throw std::runtime_error("dat: empty section");
        --pos_;
        cur_ = src_[pos_];
        left_ = bitsInFirst;
    }

    unsigned bit() {
        while (left_ == 0) {
            if (pos_ == 0) throw std::runtime_error("dat: bitstream underrun");
            --pos_;
            cur_ = src_[pos_];
            left_ = 8;
        }
        unsigned b = cur_ & 1u;
        cur_ >>= 1;
        --left_;
        return b;
    }

    unsigned bits(int n) {
        unsigned v = 0;
        for (int i = 0; i < n; ++i) v = (v << 1) | bit();
        return v;
    }

private:
    const uint8_t* src_;
    size_t pos_;
    unsigned cur_ = 0;
    int left_ = 0;
};

uint16_t be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }

} // namespace

Bytes decompressSection(const uint8_t* src, size_t srcLen, int bitsInFirst,
                        size_t outLen) {
    Bytes out(outLen);
    BackwardBitReader br(src, srcLen, bitsInFirst);
    size_t pos = outLen; // next byte is written at pos-1

    auto raw = [&](unsigned n) {
        if (n > pos) throw std::runtime_error("dat: raw run overflows output");
        for (unsigned i = 0; i < n; ++i) out[--pos] = uint8_t(br.bits(8));
    };
    auto ref = [&](unsigned len, unsigned offset) {
        if (len > pos) throw std::runtime_error("dat: reference overflows output");
        for (unsigned i = 0; i < len; ++i) {
            --pos;
            if (pos + offset >= outLen) throw std::runtime_error("dat: bad reference");
            out[pos] = out[pos + offset];
        }
    };

    while (pos > 0) {
        if (br.bit() == 0) {
            if (br.bit() == 0) raw(br.bits(3) + 1);       // 00 nnn
            else ref(2, br.bits(8) + 1);                   // 01 m8
        } else {
            switch (br.bits(2)) {
            case 0: ref(3, br.bits(9) + 1); break;         // 100 m9
            case 1: ref(4, br.bits(10) + 1); break;        // 101 m10
            case 2: {                                      // 110 n8 m12
                unsigned len = br.bits(8) + 1;
                ref(len, br.bits(12) + 1);
                break;
            }
            default: raw(br.bits(8) + 9); break;           // 111 n8
            }
        }
    }
    return out;
}

std::vector<DatSection> decompressDat(const Bytes& file) {
    std::vector<DatSection> sections;
    size_t off = 0;
    while (off + 10 <= file.size()) {
        const uint8_t* h = &file[off];
        int bitsInFirst = h[0];
        uint8_t checksum = h[1];
        size_t outLen = be16(h + 4);
        size_t total = be16(h + 8);
        if (total < 10 || off + total > file.size())
            throw std::runtime_error("dat: section size out of range");
        const uint8_t* body = h + 10;
        size_t bodyLen = total - 10;
        uint8_t x = 0;
        for (size_t i = 0; i < bodyLen; ++i) x ^= body[i];
        if (x != checksum) throw std::runtime_error("dat: checksum mismatch");
        sections.push_back({decompressSection(body, bodyLen, bitsInFirst, outLen)});
        off += total;
    }
    if (off != file.size()) throw std::runtime_error("dat: trailing bytes");
    return sections;
}

namespace {

// Writes bits in stream order; the stream is later laid out backwards.
struct BitWriter {
    std::vector<uint8_t> bits;
    void put(unsigned v, int n) {
        for (int i = n - 1; i >= 0; --i) bits.push_back(uint8_t((v >> i) & 1));
    }
};

} // namespace

Bytes compressSectionStored(const Bytes& data) {
    if (data.empty() || data.size() > 0xFFFF)
        throw std::runtime_error("dat: unsupported section size");
    BitWriter bw;
    // Output is reconstructed from the end, so emit chunks from the back.
    size_t pos = data.size();
    while (pos > 0) {
        size_t n = pos >= 9 ? std::min<size_t>(pos, 264) : pos;
        if (n >= 9) bw.put(0b111, 3), bw.put(unsigned(n - 9), 8);
        else bw.put(0b00, 2), bw.put(unsigned(n - 1), 3);
        for (size_t i = 0; i < n; ++i) bw.put(data[pos - 1 - i], 8);
        pos -= n;
    }
    size_t nbits = bw.bits.size();
    size_t nbytes = (nbits + 7) / 8;
    int bitsInFirst = int(nbits - (nbytes - 1) * 8);
    Bytes body(nbytes, 0);
    // Stream bit k goes into byte (nbytes-1 - byteIndex), LSB first.
    for (size_t k = 0; k < nbits; ++k) {
        size_t byteFromEnd, bitPos;
        if (k < size_t(bitsInFirst)) byteFromEnd = 0, bitPos = k;
        else byteFromEnd = 1 + (k - bitsInFirst) / 8, bitPos = (k - bitsInFirst) % 8;
        if (bw.bits[k]) body[nbytes - 1 - byteFromEnd] |= uint8_t(1u << bitPos);
    }
    size_t total = body.size() + 10;
    if (total > 0xFFFF) throw std::runtime_error("dat: section too large");
    uint8_t x = 0;
    for (uint8_t b : body) x ^= b;
    Bytes out = {uint8_t(bitsInFirst), x, 0, 0,
                 uint8_t(data.size() >> 8), uint8_t(data.size()), 0, 0,
                 uint8_t(total >> 8), uint8_t(total)};
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

Bytes readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    return Bytes(std::istreambuf_iterator<char>(f), {});
}

void writeFile(const std::string& path, const Bytes& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + path);
    f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
}

} // namespace lem
