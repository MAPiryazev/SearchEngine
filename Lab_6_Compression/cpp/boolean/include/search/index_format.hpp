#pragma once

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

inline std::uint32_t read_u32(std::ifstream& in) {
  std::uint32_t v = 0;
  in.read(reinterpret_cast<char*>(&v), sizeof(v));
  if (!in) throw std::runtime_error("read_u32 failed");
  return v;
}

inline std::uint64_t read_u64(std::ifstream& in) {
  std::uint64_t v = 0;
  in.read(reinterpret_cast<char*>(&v), sizeof(v));
  if (!in) throw std::runtime_error("read_u64 failed");
  return v;
}

inline void read_exact(std::ifstream& in, void* p, std::size_t n) {
  in.read(reinterpret_cast<char*>(p), static_cast<std::streamsize>(n));
  if (!in) throw std::runtime_error("read_exact failed");
}

struct FwdHeader {
  std::uint32_t ver = 0;
  std::uint32_t docs = 0;
  std::uint64_t pool_off = 0;
};

struct InvHeader {
  std::uint32_t ver = 0;
  std::uint32_t docs = 0;
  std::uint32_t terms = 0;
  std::uint64_t dict_off = 0;
  std::uint64_t term_pool_off = 0;
  std::uint64_t postings_off = 0;
};
