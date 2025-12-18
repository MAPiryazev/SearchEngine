#pragma once

#include <cstdlib>
#include <cstring>
#include <utility>

namespace nostl {

template <typename T>
struct Vec {
    T* data;
    std::size_t size;
    std::size_t cap;

    Vec() : data(nullptr), size(0), cap(0) {}

    ~Vec() { clear_free(); }

    Vec(const Vec&) = delete;
    Vec& operator=(const Vec&) = delete;

    Vec(Vec&& o) noexcept : data(o.data), size(o.size), cap(o.cap) {
        o.data = nullptr;
        o.size = 0;
        o.cap = 0;
    }

    Vec& operator=(Vec&& o) noexcept {
        if (this == &o) return *this;
        clear_free();
        data = o.data;
        size = o.size;
        cap = o.cap;
        o.data = nullptr;
        o.size = 0;
        o.cap = 0;
        return *this;
    }

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
        
        T* new_data = static_cast<T*>(std::malloc(new_cap * sizeof(T)));
        if (!new_data) return false;

        if (data) {
            std::memcpy(new_data, data, size * sizeof(T));
            std::free(data);
        }
        
        data = new_data;
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
        std::memcpy(data + size, &v, sizeof(T));
        size++;
        return true;
    }

    bool push_back(T&& v) {
        if (size == cap) {
            std::size_t nc = cap ? cap * 2 : 8;
            if (!reserve(nc)) return false;
        }
        std::memcpy(data + size, &v, sizeof(T));
        std::memset(&v, 0, sizeof(T));
        size++;
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

    T pop_back() { 
        return static_cast<T&&>(data[--size]); 
    }
};

}
