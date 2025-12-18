#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace nostl {

template <class T>
struct Vec {
    T* data;
    std::size_t size;
    std::size_t cap;

    Vec() : data(nullptr), size(0), cap(0) {}
    ~Vec() { clear_free(); }

    Vec(const Vec&) = delete;
    Vec& operator=(const Vec&) = delete;

    void clear_free() {
        if (data) std::free(data);
        data = nullptr;
        size = 0;
        cap = 0;
    }

    void clear_keep() { size = 0; }

    T& operator[](std::size_t i) { return data[i]; }
    const T& operator[](std::size_t i) const { return data[i]; }

    T* begin() { return data; }
    T* end() { return data + size; }
    const T* begin() const { return data; }
    const T* end() const { return data + size; }

    bool reserve(std::size_t new_cap) {
        if (new_cap <= cap) return true;
        void* p = std::realloc(data, new_cap * sizeof(T));
        if (!p) return false;
        data = static_cast<T*>(p);
        cap = new_cap;
        return true;
    }

    bool resize(std::size_t new_size) {
        if (new_size > cap) {
            std::size_t nc = cap ? cap : 8;
            while (nc < new_size) nc *= 2;
            if (!reserve(nc)) return false;
        }
        if (new_size > size) {
            std::memset(data + size, 0, (new_size - size) * sizeof(T));
        }
        size = new_size;
        return true;
    }

    bool push_back(const T& v) {
        if (size == cap) {
            std::size_t nc = cap ? cap * 2 : 8;
            if (!reserve(nc)) return false;
        }
        data[size++] = v;
        return true;
    }

    bool push_back_uninit(T** out_ptr) {
        if (size == cap) {
            std::size_t nc = cap ? cap * 2 : 8;
            if (!reserve(nc)) return false;
        }
        *out_ptr = &data[size++];
        return true;
    }

    T pop_back() { return data[--size]; }
};

}  // namespace nostl
