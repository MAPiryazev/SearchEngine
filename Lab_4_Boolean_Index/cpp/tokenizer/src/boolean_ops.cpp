#include "../include/boolean_ops.hpp"

nostl::Vec<std::uint32_t> op_and(const nostl::Vec<std::uint32_t>& a,
                                 const nostl::Vec<std::uint32_t>& b) {
    nostl::Vec<std::uint32_t> out;
    out.reserve((a.size < b.size) ? a.size : b.size);

    std::size_t i = 0, j = 0;
    while (i < a.size && j < b.size) {
        if (a.data[i] == b.data[j]) {
            out.push_back(a.data[i]);
            ++i; ++j;
        } else if (a.data[i] < b.data[j]) {
            ++i;
        } else {
            ++j;
        }
    }
    return out;
}

nostl::Vec<std::uint32_t> op_or(const nostl::Vec<std::uint32_t>& a,
                                const nostl::Vec<std::uint32_t>& b) {
    nostl::Vec<std::uint32_t> out;
    out.reserve(a.size + b.size);

    std::size_t i = 0, j = 0;
    while (i < a.size || j < b.size) {
        if (j >= b.size || (i < a.size && a.data[i] < b.data[j])) {
            out.push_back(a.data[i++]);
        } else if (i >= a.size || (j < b.size && b.data[j] < a.data[i])) {
            out.push_back(b.data[j++]);
        } else {
            out.push_back(a.data[i]);
            ++i; ++j;
        }
    }
    return out;
}

nostl::Vec<std::uint32_t> op_not(const nostl::Vec<std::uint32_t>& a,
                                 std::uint32_t docs_count) {
    nostl::Vec<std::uint32_t> out;
    out.reserve((docs_count > a.size) ? (docs_count - static_cast<std::uint32_t>(a.size)) : 0);

    std::size_t i = 0;
    for (std::uint32_t d = 0; d < docs_count; ++d) {
        if (i < a.size && a.data[i] == d) {
            ++i;
        } else {
            out.push_back(d);
        }
    }
    return out;
}
