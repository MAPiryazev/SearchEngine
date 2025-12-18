#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

#include "index_format.hpp"
#include "nostl/vec.hpp"
#include "nostl/strview.hpp"

class InvertedIndex {
public:
    bool open(const std::string& path);
    std::uint32_t docs() const { return hdr.docs; }

    bool get_postings(nostl::StrView term, nostl::Vec<std::uint32_t>& out);

private:
    std::ifstream in;
    InvHeader hdr{};

    struct DictEntry {
        std::uint64_t termoff;
        std::uint32_t termlen;
        std::uint64_t postoff;
        std::uint32_t postlen;
    };

    nostl::Vec<DictEntry> dict;
    nostl::Vec<char> termpool;

    nostl::StrView termview(std::size_t i) const;
    bool find_term(nostl::StrView term, std::size_t& idx) const;
};
