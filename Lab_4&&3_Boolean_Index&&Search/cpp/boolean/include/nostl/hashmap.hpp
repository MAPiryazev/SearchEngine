#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "strview.hpp"

namespace nostl {

template <class V>
struct HashMapSV {
    struct Entry {
        std::uint64_t h;   // 0 empty
        StrView key;
        V val;
    };

    Entry* tab;
    std::size_t cap;
    std::size_t size;

    HashMapSV() : tab(nullptr), cap(0), size(0) {}
    ~HashMapSV() { free(); }

    HashMapSV(const HashMapSV&) = delete;
    HashMapSV& operator=(const HashMapSV&) = delete;

    void free() {
        if (tab) std::free(tab);
        tab = nullptr;
        cap = 0;
        size = 0;
    }

    static std::size_t pow2_ge(std::size_t x) {
        std::size_t p = 1;
        while (p < x) p <<= 1;
        return p;
    }

    bool init(std::size_t initial_cap = 1024) {
        free();
        cap = pow2_ge(initial_cap);
        tab = static_cast<Entry*>(std::calloc(cap, sizeof(Entry)));
        if (!tab) { cap = 0; return false; }
        size = 0;
        return true;
    }

    static std::uint64_t pack_hash(std::uint64_t h) { return h ? h : 14695981039346656037ull; }

    bool rehash(std::size_t new_cap) {
        Entry* old = tab;
        std::size_t old_cap = cap;

        cap = pow2_ge(new_cap);
        tab = static_cast<Entry*>(std::calloc(cap, sizeof(Entry)));
        if (!tab) { tab = old; cap = old_cap; return false; }

        size = 0;
        for (std::size_t i = 0; i < old_cap; ++i) {
            if (old[i].h == 0) continue;
            put_existing(old[i].h, old[i].key, old[i].val);
        }
        std::free(old);
        return true;
    }

    bool ensure_load() {
        if (cap == 0) return init(1024);
        if ((size + 1) * 10 <= cap * 7) return true;
        return rehash(cap * 2);
    }

    void put_existing(std::uint64_t hh, StrView k, const V& v) {
        std::size_t mask = cap - 1;
        std::size_t i = static_cast<std::size_t>(hh) & mask;
        while (tab[i].h != 0) i = (i + 1) & mask;
        tab[i].h = hh;
        tab[i].key = k;
        tab[i].val = v;
        ++size;
    }

    V* find(StrView k) {
        if (!tab || cap == 0) return nullptr;
        std::uint64_t hh = pack_hash(sv_hash_fnv1a(k));
        std::size_t mask = cap - 1;
        std::size_t i = static_cast<std::size_t>(hh) & mask;
        while (true) {
            if (tab[i].h == 0) return nullptr;
            if (tab[i].h == hh && sv_eq(tab[i].key, k)) return &tab[i].val;
            i = (i + 1) & mask;
        }
    }

    V* get_or_insert(StrView k, bool* inserted) {
        if (!ensure_load()) return nullptr;
        std::uint64_t hh = pack_hash(sv_hash_fnv1a(k));

        std::size_t mask = cap - 1;
        std::size_t i = static_cast<std::size_t>(hh) & mask;
        while (true) {
            if (tab[i].h == 0) {
                tab[i].h = hh;
                tab[i].key = k;
                std::memset(&tab[i].val, 0, sizeof(V));
                ++size;
                if (inserted) *inserted = true;
                return &tab[i].val;
            }
            if (tab[i].h == hh && sv_eq(tab[i].key, k)) {
                if (inserted) *inserted = false;
                return &tab[i].val;
            }
            i = (i + 1) & mask;
        }
    }
};

}  // namespace nostl
