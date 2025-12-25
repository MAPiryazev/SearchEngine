#include "search/vbyte.hpp"

namespace lab6 {

bool vbyte_put_u32(nostl::Vec<std::uint8_t>& out, std::uint32_t v) {
    while (v >= 0x80u) {
        std::uint8_t b = static_cast<std::uint8_t>(v & 0x7Fu);
        if (!out.push_back(b)) return false;
        v >>= 7;
    }
    std::uint8_t last = static_cast<std::uint8_t>((v & 0x7Fu) | 0x80u);
    return out.push_back(last);
}

const std::uint8_t* vbyte_get_u32(
    const std::uint8_t* p,
    const std::uint8_t* end,
    std::uint32_t& out) {

    std::uint32_t v = 0;
    int shift = 0;

    while (true) {
        if (p >= end) return nullptr;
        std::uint8_t b = *p++;
        v |= (static_cast<std::uint32_t>(b & 0x7Fu) << shift);

        if (b & 0x80u) {
            out = v;
            return p;
        }

        shift += 7;
        if (shift > 28) return nullptr;
    }
}

} // namespace lab6
