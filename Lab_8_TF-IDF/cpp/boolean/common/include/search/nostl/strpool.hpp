#pragma once

#include <cstring>
#include <cstdlib>
#include "strview.hpp"
#include "vec.hpp"

namespace nostl {

struct StrPool {
    Vec<char*> blocks;
    std::size_t block_cap;
    std::size_t current_used;

    StrPool(std::size_t initial_block_cap = 16 * 1024) 
        : block_cap(initial_block_cap), current_used(0) {
        add_block(block_cap);
    }

    ~StrPool() {
        for (std::size_t i = 0; i < blocks.size; ++i) {
            if (blocks[i]) std::free(blocks[i]);
        }
    }

    StrPool(const StrPool&) = delete;
    StrPool& operator=(const StrPool&) = delete;

    StrPool(StrPool&& o) noexcept 
        : blocks(static_cast<Vec<char*>&&>(o.blocks)),
          block_cap(o.block_cap), 
          current_used(o.current_used) {
        o.block_cap = 0;
        o.current_used = 0;
    }

    StrPool& operator=(StrPool&& o) noexcept {
        if (this == &o) return *this;
        for (std::size_t i = 0; i < blocks.size; ++i) {
            if (blocks[i]) std::free(blocks[i]);
        }
        blocks = static_cast<Vec<char*>&&>(o.blocks);
        block_cap = o.block_cap;
        current_used = o.current_used;
        
        o.block_cap = 0;
        o.current_used = 0;
        return *this;
    }

    void add_block(std::size_t cap) {
        char* ptr = static_cast<char*>(std::malloc(cap));
        if (ptr) {
            blocks.push_back(ptr);
            current_used = 0;
        }
    }

    StrView add_copy(const char* s, std::size_t n, bool add_null = true) {
        std::size_t needed = n + (add_null ? 1 : 0);
        
        if (blocks.size == 0 || current_used + needed > block_cap) {
            std::size_t new_cap = (needed > block_cap) ? needed : block_cap;
            add_block(new_cap);
            if (blocks.size == 0) return StrView(nullptr, 0);
        }

        char* current_block = blocks[blocks.size - 1];
        char* dest = current_block + current_used;
        
        if (n) std::memcpy(dest, s, n);
        if (add_null) dest[n] = 0;
        
        current_used += needed;
        return StrView(dest, n);
    }

    StrView add_view(StrView v, bool add_null = true) {
        return add_copy(v.data, v.size, add_null);
    }
    
    bool reserve(std::size_t n) {
        return true; 
    }
};

}
