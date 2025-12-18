#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "index_format.hpp"

class InvertedIndex {
public:
  struct Entry {
    std::uint64_t term_off = 0;
    std::uint32_t term_len = 0;
    std::uint64_t post_off = 0;
    std::uint32_t post_len = 0; // количество docID (не байты)
  };

  bool open(const std::string& path);
  std::uint32_t docs() const { return hdr_.docs; }
  std::uint32_t terms() const { return hdr_.terms; }

  bool get_postings(const std::string& term, std::vector<std::uint32_t>& out);

private:
  std::ifstream in_;
  InvHeader hdr_{};
  std::vector<Entry> dict_;
  std::string term_pool_;

  std::string_view term_view(std::size_t i) const;
  bool find_term(const std::string& term, std::size_t& idx) const;
};
