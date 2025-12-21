#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "search/boolean_ops.hpp"
#include "search/query_parser.hpp"
#include "search/tokenizer_stl.hpp"
#include "search/lab6/inv_index6.hpp"
#include "search/nostl/vec.hpp"

static TokenizeOptions opt_default() {
  TokenizeOptions opt;
  opt.keep_numbers = false;
  opt.stem = false;
  opt.min_len = 1;
  return opt;
}

static std::vector<std::string> toks_stl(const std::string& s, TokenizeOptions opt) {
  std::vector<std::string> out;
  tokenize_doc_terms(s, opt, out);
  return out;
}

static nostl::Vec<std::uint32_t> mk_u32(std::initializer_list<std::uint32_t> xs) {
  nostl::Vec<std::uint32_t> v;
  v.reserve(xs.size());
  for (auto x : xs) v.push_back(x);
  return v;
}

TEST_CASE("Tokenizer: basic lowercase") {
  auto opt = opt_default();
  auto out = toks_stl("Python И Java", opt);

  REQUIRE(out.size() == 3);
  REQUIRE(out[0] == "python");
  REQUIRE(out[1] == "и");
  REQUIRE(out[2] == "java");
}

TEST_CASE("Tokenizer: keep_numbers") {
  auto opt = opt_default();

  SECTION("keep_numbers = false") {
    opt.keep_numbers = false;
    auto out = toks_stl("win10 123 abc123", opt);

    REQUIRE(out.size() == 2);
    REQUIRE(out[0] == "win");
    REQUIRE(out[1] == "abc");
  }

  SECTION("keep_numbers = true") {
    opt.keep_numbers = true;
    auto out = toks_stl("win10 123 abc123", opt);

    REQUIRE(out.size() == 3);
    REQUIRE(out[0] == "win10");
    REQUIRE(out[1] == "123");
    REQUIRE(out[2] == "abc123");
  }
}

TEST_CASE("Boolean ops: AND/OR/NOT") {
  SECTION("AND") {
    auto a = mk_u32({1, 2, 4, 10});
    auto b = mk_u32({2, 3, 4, 5});
    auto r = op_and(a, b);

    REQUIRE(r.size == 2);
    REQUIRE(r.data[0] == 2u);
    REQUIRE(r.data[1] == 4u);
  }

  SECTION("OR") {
    auto a = mk_u32({1, 2, 4});
    auto b = mk_u32({2, 3, 5});
    auto r = op_or(a, b);

    REQUIRE(r.size == 5);
    REQUIRE(r.data[0] == 1u);
    REQUIRE(r.data[1] == 2u);
    REQUIRE(r.data[2] == 3u);
    REQUIRE(r.data[3] == 4u);
    REQUIRE(r.data[4] == 5u);
  }

  SECTION("NOT") {
    auto a = mk_u32({1, 3});
    auto r = op_not(a, 5);

    REQUIRE(r.size == 3);
    REQUIRE(r.data[0] == 0u);
    REQUIRE(r.data[1] == 2u);
    REQUIRE(r.data[2] == 4u);
  }
}

TEST_CASE("Query parser: RPN for simple AND") {
  QueryLexResult lex = lex_query(nostl::StrView("python && java", 13));
  auto rpn = to_rpn(lex.toks);

  REQUIRE(rpn.size == 3);
  REQUIRE(rpn.data[0].type == QTokType::TERM);
  REQUIRE(rpn.data[1].type == QTokType::TERM);
  REQUIRE(rpn.data[2].type == QTokType::AND);
}

TEST_CASE("INV2: open missing file returns false") {
  InvertedIndex6 inv;
  REQUIRE(inv.open("definitely_not_exists_12345.inv") == false);
}
