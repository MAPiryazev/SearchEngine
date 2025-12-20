#pragma once

namespace nostl {
template <class T>
constexpr T&& move(T& x) noexcept { return static_cast<T&&>(x); }
}
