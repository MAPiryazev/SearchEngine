#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include "index_format.hpp"
#include "nostl/strview.hpp"
#include "nostl/vec.hpp"

struct DocInfoView {
    nostl::StrView url;
    nostl::StrView title;
};

class ForwardIndex {
public:
    bool open(const std::string& path);
    std::uint32_t docs() const { return hdr.docs; }

    DocInfoView get(std::uint32_t docid);

private:
    std::ifstream in;
    FwdHeader hdr{};

    nostl::Vec<char> pool;

    static constexpr std::uint64_t kHeaderSize = 4 + 4 + 4 + 8;
    static constexpr std::uint64_t kRecSize    = 8 + 4 + 8 + 4;
};
