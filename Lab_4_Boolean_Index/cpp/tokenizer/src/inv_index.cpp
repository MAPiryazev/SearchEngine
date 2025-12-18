#include "../include/inv_index.hpp"

#include <algorithm>
#include <stdexcept>

bool InvertedIndex::open(const std::string& path) {
  in_.open(path, std::ios::binary);
  if (!in_) return false;

  char magic[4];
  read_exact(in_, magic, 4);
  if (std::string(magic, magic + 4) != "INV1") throw std::runtime_error("bad INV magic");

  hdr_.ver = read_u32(in_);
  hdr_.docs = read_u32(in_);
  hdr_.terms = read_u32(in_);
  hdr_.dict_off = read_u64(in_);
  hdr_.term_pool_off = read_u64(in_);
  hdr_.postings_off = read_u64(in_);

  dict_.clear();
  dict_.resize(hdr_.terms);

  in_.seekg(static_cast<std::streamoff>(hdr_.dict_off), std::ios::beg);
  for (std::uint32_t i = 0; i < hdr_.terms; i++) {
    dict_[i].term_off = read_u64(in_);
    dict_[i].term_len = read_u32(in_);
    dict_[i].post_off = read_u64(in_);
    dict_[i].post_len = read_u32(in_);
  }

  std::uint64_t term_pool_size = hdr_.postings_off - hdr_.term_pool_off;
  term_pool_.assign(static_cast<std::size_t>(term_pool_size), '\0');

  in_.seekg(static_cast<std::streamoff>(hdr_.term_pool_off), std::ios::beg);
  if (term_pool_size) read_exact(in_, term_pool_.data(), static_cast<std::size_t>(term_pool_size));

  return true;
}

std::string_view InvertedIndex::term_view(std::size_t i) const {
  const auto& e = dict_[i];
  return std::string_view(term_pool_.data() + e.term_off, e.term_len);
}

bool InvertedIndex::find_term(const std::string& term, std::size_t& idx) const {
  std::size_t lo = 0, hi = dict_.size();
  while (lo < hi) {
    std::size_t mid = lo + (hi - lo) / 2;
    std::string_view tv = term_view(mid);
    if (tv < term) lo = mid + 1;
    else hi = mid;
  }
  if (lo < dict_.size() && term_view(lo) == term) {
    idx = lo;
    return true;
  }
  return false;
}

bool InvertedIndex::get_postings(const std::string& term, std::vector<std::uint32_t>& out) {
  if (!in_) throw std::runtime_error("inv not opened");

  std::size_t idx = 0;
  if (!find_term(term, idx)) {
    out.clear();
    return false;
  }

  const auto& e = dict_[idx];
  out.assign(e.post_len, 0);

  if (e.post_len == 0) return true;

  std::uint64_t byte_off = hdr_.postings_off + e.post_off;
  in_.seekg(static_cast<std::streamoff>(byte_off), std::ios::beg);
  read_exact(in_, out.data(), static_cast<std::size_t>(e.post_len) * sizeof(std::uint32_t));
  return true;
}
