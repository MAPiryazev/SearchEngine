#include "../include/boolean_ops.hpp"

std::vector<std::uint32_t> op_and(const std::vector<std::uint32_t>& a,
                                 const std::vector<std::uint32_t>& b) {
  std::vector<std::uint32_t> out;
  out.reserve((a.size() < b.size()) ? a.size() : b.size());

  std::size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) {
      out.push_back(a[i]);
      i++; j++;
    } else if (a[i] < b[j]) {
      i++;
    } else {
      j++;
    }
  }
  return out;
}

std::vector<std::uint32_t> op_or(const std::vector<std::uint32_t>& a,
                                const std::vector<std::uint32_t>& b) {
  std::vector<std::uint32_t> out;
  out.reserve(a.size() + b.size());

  std::size_t i = 0, j = 0;
  while (i < a.size() || j < b.size()) {
    if (j >= b.size() || (i < a.size() && a[i] < b[j])) {
      out.push_back(a[i++]);
    } else if (i >= a.size() || (j < b.size() && b[j] < a[i])) {
      out.push_back(b[j++]);
    } else {
      out.push_back(a[i]);
      i++; j++;
    }
  }
  return out;
}

std::vector<std::uint32_t> op_not(const std::vector<std::uint32_t>& a,
                                 std::uint32_t docs_count) {
  std::vector<std::uint32_t> out;
  out.reserve(docs_count > a.size() ? (docs_count - a.size()) : 0);

  std::size_t i = 0;
  for (std::uint32_t d = 0; d < docs_count; d++) {
    if (i < a.size() && a[i] == d) {
      i++;
    } else {
      out.push_back(d);
    }
  }
  return out;
}
