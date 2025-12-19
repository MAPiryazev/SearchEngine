#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../../tokenizer/include/boolean_ops.hpp"
#include "../../tokenizer/include/fwd_index.hpp"
#include "../../tokenizer/include/inv_index.hpp"
#include "../../tokenizer/include/query_parser.hpp"
#include "../../tokenizer/include/tokenizer_stl.hpp" 

#include "../../tokenizer/include/nostl/vec.hpp"
#include "../../tokenizer/include/nostl/strview.hpp"
#include "../../tokenizer/include/nostl/util.hpp"

static int g_tests = 0;
static int g_asserts = 0;

#define ASSERT_TRUE(x) do { \
  g_asserts++; \
  if (!(x)) { \
    std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " ASSERT_TRUE(" #x ")\n"; \
    return 1; \
  } \
} while (0)

#define ASSERT_VEC_EQ(vec, il) do { \
  g_asserts++; \
  auto& _v = (vec); \
  std::vector<std::uint32_t> _expected = (il); \
  if (_v.size != _expected.size()) { \
     std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " Size mismatch: " << _v.size << " != " << _expected.size() << "\n"; \
     return 1; \
  } \
  for(size_t _i=0; _i<_v.size; ++_i) { \
      if(_v.data[_i] != _expected[_i]) { \
        std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " Mismatch at " << _i << ": " << _v.data[_i] << " != " << _expected[_i] << "\n"; \
        return 1; \
      } \
  } \
} while (0)

static void write_u32(std::ofstream& out, std::uint32_t v) {
  out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
static void write_u64(std::ofstream& out, std::uint64_t v) {
  out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
static void write_bytes(std::ofstream& out, const void* p, std::size_t n) {
  out.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n));
}

static int test_boolean_ops() {
  g_tests++;

  nostl::Vec<std::uint32_t> a; a.push_back(1); a.push_back(3); a.push_back(5);
  nostl::Vec<std::uint32_t> b; b.push_back(3); b.push_back(4); b.push_back(5);

  auto c_and = op_and(a, b);
  auto c_or  = op_or(a, b);
  auto c_not = op_not(a, 6); 

  ASSERT_VEC_EQ(c_and, (std::vector<std::uint32_t>{3, 5}));
  ASSERT_VEC_EQ(c_or,  (std::vector<std::uint32_t>{1, 3, 4, 5}));
  ASSERT_VEC_EQ(c_not, (std::vector<std::uint32_t>{0, 2, 4}));

  return 0;
}

static int test_tokenizer_normalization() {
  g_tests++;

  TokenizeOptions opt;
  opt.keep_numbers = false;
  opt.stem = false;
  opt.min_len = 1;

  std::vector<std::string> terms;
  tokenize_doc_terms("Ёж ёж ЕЖ", opt, terms);

  ASSERT_TRUE(terms.size() == 3);
  ASSERT_TRUE(terms[0] == "еж");
  ASSERT_TRUE(terms[1] == "еж");
  ASSERT_TRUE(terms[2] == "еж");

  return 0;
}

struct MiniDoc {
    std::string url;
    std::string title;
};

static int write_mini_fwd(const std::string& path) {
  std::ofstream out(path, std::ios::binary);
  if (!out) return 1;

  std::vector<MiniDoc> docs = {
    {"u0", "Doc0"},
    {"u1", "Doc1"},
    {"u2", "Doc2"},
  };

  std::uint32_t ver = 1;
  std::uint32_t n = static_cast<std::uint32_t>(docs.size());

  const std::uint64_t header_size = 4 + 4 + 4 + 8;
  const std::uint64_t rec_size = 8 + 4 + 8 + 4;
  const std::uint64_t pool_off = header_size + static_cast<std::uint64_t>(n) * rec_size;

  write_bytes(out, "FWD1", 4);
  write_u32(out, ver);
  write_u32(out, n);
  write_u64(out, pool_off);

  std::uint64_t cur = 0;
  for (std::uint32_t i = 0; i < n; i++) {
    std::uint64_t uoff = cur;
    std::uint32_t ulen = static_cast<std::uint32_t>(docs[i].url.size());
    cur += ulen;

    std::uint64_t toff = cur;
    std::uint32_t tlen = static_cast<std::uint32_t>(docs[i].title.size());
    cur += tlen;

    write_u64(out, uoff);
    write_u32(out, ulen);
    write_u64(out, toff);
    write_u32(out, tlen);
  }

  for (std::uint32_t i = 0; i < n; i++) {
    write_bytes(out, docs[i].url.data(), docs[i].url.size());
    write_bytes(out, docs[i].title.data(), docs[i].title.size());
  }

  return 0;
}

static int write_mini_inv(const std::string& path) {
  std::ofstream out(path, std::ios::binary);
  if (!out) return 1;

  struct TE { std::string term; std::vector<std::uint32_t> post; };
  std::vector<TE> dict = {
    {"a", {0, 2}},
    {"b", {1, 2}},
  };

  std::uint32_t ver = 1;
  std::uint32_t docs = 3;
  std::uint32_t terms = static_cast<std::uint32_t>(dict.size());

  const std::uint64_t header_size = 4 + 4 + 4 + 4 + 8 + 8 + 8;
  const std::uint64_t entry_size = 8 + 4 + 8 + 4;

  const std::uint64_t dict_off = header_size;
  const std::uint64_t term_pool_off = dict_off + static_cast<std::uint64_t>(terms) * entry_size;

  std::uint64_t term_pool_bytes = 0;
  for (auto& e : dict) term_pool_bytes += e.term.size();

  const std::uint64_t postings_off = term_pool_off + term_pool_bytes;

  write_bytes(out, "INV1", 4);
  write_u32(out, ver);
  write_u32(out, docs);
  write_u32(out, terms);
  write_u64(out, dict_off);
  write_u64(out, term_pool_off);
  write_u64(out, postings_off);

  std::uint64_t cur_term = 0;
  std::uint64_t cur_post = 0;

  for (auto& e : dict) {
    std::uint64_t toff = cur_term;
    std::uint32_t tlen = static_cast<std::uint32_t>(e.term.size());
    cur_term += tlen;

    std::uint64_t poff = cur_post;
    std::uint32_t plen = static_cast<std::uint32_t>(e.post.size());
    cur_post += static_cast<std::uint64_t>(plen) * sizeof(std::uint32_t);

    write_u64(out, toff);
    write_u32(out, tlen);
    write_u64(out, poff);
    write_u32(out, plen);
  }

  for (auto& e : dict) write_bytes(out, e.term.data(), e.term.size());
  for (auto& e : dict) write_bytes(out, e.post.data(), e.post.size() * sizeof(std::uint32_t));

  return 0;
}

int main() {
  if (int rc = test_boolean_ops()) return rc;
  if (int rc = test_tokenizer_normalization()) return rc;

  std::cout << "[unit_tests] ALL TESTS PASSED (" << g_tests << " tests, " << g_asserts << " assertions)\n";
  return 0;
}

// Test project /home/michael/InformationSearch/SearchEngine/Lab_4_Boolean_Index/tests/build
//     Start 1: unit_tests
// 1/1 Test #1: unit_tests .......................   Passed    0.00 sec

// 100% tests passed, 0 tests failed out of 1

// Total Test time (real) =   0.01 sec