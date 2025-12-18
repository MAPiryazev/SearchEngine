#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include "index_format.hpp"

struct DocInfo {
  std::string url;
  std::string title;
};

class ForwardIndex {
public:
  bool open(const std::string& path);
  std::uint32_t docs() const { return hdr_.docs; }
  DocInfo get(std::uint32_t doc_id);

private:
  std::ifstream in_;
  FwdHeader hdr_{};

  static constexpr std::uint64_t kHeaderSize = 4 + 4 + 4 + 8;
  static constexpr std::uint64_t kRecSize = 8 + 4 + 8 + 4;
};
