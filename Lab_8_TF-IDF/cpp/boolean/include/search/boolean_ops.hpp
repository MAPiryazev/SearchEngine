#pragma once

#include <cstdint>
#include "nostl/vec.hpp"

nostl::Vec<std::uint32_t> op_and(const nostl::Vec<std::uint32_t>& a,
                                 const nostl::Vec<std::uint32_t>& b);

nostl::Vec<std::uint32_t> op_or(const nostl::Vec<std::uint32_t>& a,
                                const nostl::Vec<std::uint32_t>& b);

nostl::Vec<std::uint32_t> op_not(const nostl::Vec<std::uint32_t>& a,
                                 std::uint32_t docs_count);
