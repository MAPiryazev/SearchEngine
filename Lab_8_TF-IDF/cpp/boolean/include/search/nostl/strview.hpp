#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace nostl {

struct StrView {
    const char* data;
    std::size_t size;

    StrView() : data(nullptr), size(0) {}
    StrView(const char* p, std::size_t n) : data(p), size(n) {}
};

inline bool sv_empty(const StrView& s) { return s.size == 0; }

inline int sv_cmp(const StrView& a, const StrView& b) {
    const std::size_t n = (a.size < b.size) ? a.size : b.size;
    int r = 0;
    if (n) r = std::memcmp(a.data, b.data, n);
    if (r != 0) return r;
    if (a.size < b.size) return -1;
    if (a.size > b.size) return 1;
    return 0;
}

inline bool sv_eq(const StrView& a, const StrView& b) {
    if (a.size != b.size) return false;
    if (a.size == 0) return true;
    return std::memcmp(a.data, b.data, a.size) == 0;
}

inline std::uint64_t sv_hash_fnv1a(const StrView& s) {
    const std::uint64_t FNV_OFFSET = 14695981039346656037ull;
    const std::uint64_t FNV_PRIME  = 1099511628211ull;
    std::uint64_t h = FNV_OFFSET;
    for (std::size_t i = 0; i < s.size; ++i) {
        h ^= static_cast<unsigned char>(s.data[i]);
        h *= FNV_PRIME;
    }
    return h;
}

}  // namespace nostl
