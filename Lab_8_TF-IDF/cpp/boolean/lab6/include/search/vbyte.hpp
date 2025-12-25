#pragma once

#include <cstddef>
#include <cstdint>

#include "search/nostl/vec.hpp"

namespace lab6 {

bool vbyte_put_u32(nostl::Vec<std::uint8_t>& out, std::uint32_t v);

const std::uint8_t* vbyte_get_u32(
    const std::uint8_t* p,
    const std::uint8_t* end,
    std::uint32_t& out);

} // namespace lab6
