#pragma once

#include <cstdint>
#include <vector>

std::vector<std::uint32_t> op_and(const std::vector<std::uint32_t>& a,
                                 const std::vector<std::uint32_t>& b);

std::vector<std::uint32_t> op_or(const std::vector<std::uint32_t>& a,
                                const std::vector<std::uint32_t>& b);

std::vector<std::uint32_t> op_not(const std::vector<std::uint32_t>& a,
                                 std::uint32_t docs_count);
