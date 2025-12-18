#include "../include/fwd_index.hpp"

#include <stdexcept>

bool ForwardIndex::open(const std::string& path) {
  in_.open(path, std::ios::binary);
  if (!in_) return false;

  char magic[4];
  read_exact(in_, magic, 4);
  if (std::string(magic, magic + 4) != "FWD1") throw std::runtime_error("bad FWD magic");

  hdr_.ver = read_u32(in_);
  hdr_.docs = read_u32(in_);
  hdr_.pool_off = read_u64(in_);
  return true;
}

DocInfo ForwardIndex::get(std::uint32_t doc_id) {
  if (!in_) throw std::runtime_error("fwd not opened");
  if (doc_id >= hdr_.docs) throw std::runtime_error("doc_id out of range");

  std::uint64_t rec_off = kHeaderSize + static_cast<std::uint64_t>(doc_id) * kRecSize;
  in_.seekg(static_cast<std::streamoff>(rec_off), std::ios::beg);

  std::uint64_t url_off = read_u64(in_);
  std::uint32_t url_len = read_u32(in_);
  std::uint64_t title_off = read_u64(in_);
  std::uint32_t title_len = read_u32(in_);

  DocInfo d;
  d.url.resize(url_len);
  d.title.resize(title_len);

  if (url_len) {
    in_.seekg(static_cast<std::streamoff>(hdr_.pool_off + url_off), std::ios::beg);
    read_exact(in_, d.url.data(), url_len);
  }
  if (title_len) {
    in_.seekg(static_cast<std::streamoff>(hdr_.pool_off + title_off), std::ios::beg);
    read_exact(in_, d.title.data(), title_len);
  }

  return d;
}
