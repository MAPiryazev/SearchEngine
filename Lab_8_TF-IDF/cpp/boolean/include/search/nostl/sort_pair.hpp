#pragma once

#include <cstddef>
#include <cstdint>

namespace nostl {

struct PairU32 {
    std::uint32_t a;
    std::uint32_t b;
};

inline void swap_pair(PairU32& x, PairU32& y) {
    PairU32 t = x;
    x = y;
    y = t;
}

inline bool pair_less(const PairU32& x, const PairU32& y) {
    if (x.a != y.a) return x.a < y.a;
    return x.b < y.b;
}

inline void qsort_pair(PairU32* v, std::ptrdiff_t l, std::ptrdiff_t r) {
    while (l < r) {
        std::ptrdiff_t i = l, j = r;
        PairU32 p = v[l + (r - l) / 2];
        while (i <= j) {
            while (pair_less(v[i], p)) ++i;
            while (pair_less(p, v[j])) --j;
            if (i <= j) { swap_pair(v[i], v[j]); ++i; --j; }
        }
        if (j - l < r - i) {
            if (l < j) qsort_pair(v, l, j);
            l = i;
        } else {
            if (i < r) qsort_pair(v, i, r);
            r = j;
        }
    }
}

inline void sort_pairs(PairU32* v, std::size_t n) {
    if (n < 2) return;
    qsort_pair(v, 0, static_cast<std::ptrdiff_t>(n - 1));
}

}
