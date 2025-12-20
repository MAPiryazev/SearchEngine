#pragma once

#include <cstddef>
#include <cstdint>

#include "vec.hpp"
#include "strview.hpp"

namespace nostl {

inline void swap_u32(std::uint32_t& a, std::uint32_t& b) {
    std::uint32_t t = a; a = b; b = t;
}

inline void qsort_u32(std::uint32_t* a, std::ptrdiff_t l, std::ptrdiff_t r) {
    while (l < r) {
        std::ptrdiff_t i = l, j = r;
        std::uint32_t p = a[l + (r - l) / 2];
        while (i <= j) {
            while (a[i] < p) ++i;
            while (a[j] > p) --j;
            if (i <= j) { swap_u32(a[i], a[j]); ++i; --j; }
        }
        if (j - l < r - i) {
            if (l < j) qsort_u32(a, l, j);
            l = i;
        } else {
            if (i < r) qsort_u32(a, i, r);
            r = j;
        }
    }
}

inline void sort_u32(Vec<std::uint32_t>& v) {
    if (v.size < 2) return;
    qsort_u32(v.data, 0, static_cast<std::ptrdiff_t>(v.size - 1));
}

inline std::size_t unique_u32_inplace(Vec<std::uint32_t>& v) {
    if (v.size == 0) return 0;
    std::size_t w = 1;
    for (std::size_t i = 1; i < v.size; ++i) {
        if (v.data[i] != v.data[w - 1]) v.data[w++] = v.data[i];
    }
    v.size = w;
    return w;
}

inline void swap_sv(StrView& a, StrView& b) { StrView t = a; a = b; b = t; }

inline void qsort_sv(StrView* a, std::ptrdiff_t l, std::ptrdiff_t r) {
    while (l < r) {
        std::ptrdiff_t i = l, j = r;
        StrView p = a[l + (r - l) / 2];
        while (i <= j) {
            while (sv_cmp(a[i], p) < 0) ++i;
            while (sv_cmp(a[j], p) > 0) --j;
            if (i <= j) { swap_sv(a[i], a[j]); ++i; --j; }
        }
        if (j - l < r - i) {
            if (l < j) qsort_sv(a, l, j);
            l = i;
        } else {
            if (i < r) qsort_sv(a, i, r);
            r = j;
        }
    }
}

inline void sort_sv(Vec<StrView>& v) {
    if (v.size < 2) return;
    qsort_sv(v.data, 0, static_cast<std::ptrdiff_t>(v.size - 1));
}

inline std::size_t unique_sv_inplace(Vec<StrView>& v) {
    if (v.size == 0) return 0;
    std::size_t w = 1;
    for (std::size_t i = 1; i < v.size; ++i) {
        if (!sv_eq(v.data[i], v.data[w - 1])) v.data[w++] = v.data[i];
    }
    v.size = w;
    return w;
}

}  // namespace nostl
