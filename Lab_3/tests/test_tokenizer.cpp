#include <string>
#include <unordered_map>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#define TOKENIZER_NO_MAIN
#include "../cpp/tokenizer/src"

static std::vector<std::string> tokenize_vec(const std::string& text, const TokenizeOptions& opt) {
  std::uint64_t tokens_total = 0;
  std::uint64_t token_chars_total = 0;
  std::unordered_map<std::string, std::uint32_t> tf;
  tf.reserve(256);

  tokenize_text(text, opt, tokens_total, token_chars_total, tf);

  std::vector<std::string> out;
  out.reserve(tf.size());
  for (const auto& kv : tf) out.push_back(kv.first);
  return out;
}

static std::unordered_map<std::string, std::uint32_t> tf_map(const std::string& text, const TokenizeOptions& opt) {
  std::uint64_t tokens_total = 0;
  std::uint64_t token_chars_total = 0;
  std::unordered_map<std::string, std::uint32_t> tf;
  tf.reserve(256);

  tokenize_text(text, opt, tokens_total, token_chars_total, tf);
  return tf;
}

TEST_CASE("Lowercase + yo->e") {
  TokenizeOptions opt;
  auto tf = tf_map("Ёжик и ёлка", opt);
  REQUIRE(tf["ежик"] == 1);
  REQUIRE(tf["и"] == 1);
  REQUIRE(tf["елка"] == 1);
}

TEST_CASE("Punctuation splits") {
  TokenizeOptions opt;
  auto tf = tf_map("Привет, мир!", opt);
  REQUIRE(tf["привет"] == 1);
  REQUIRE(tf["мир"] == 1);
}

TEST_CASE("Hyphen inside word") {
  TokenizeOptions opt;
  auto tf = tf_map("Санкт-Петербург", opt);
  REQUIRE(tf["санкт-петербург"] == 1);
}

TEST_CASE("Hyphen not inside word") {
  TokenizeOptions opt;
  auto tf = tf_map("-тест-", opt);
  REQUIRE(tf["тест"] == 1);
  REQUIRE(tf.size() == 1);
}

TEST_CASE("Alphabet switch split") {
  TokenizeOptions opt;
  auto tf = tf_map("frameworkи", opt);
  REQUIRE(tf["framework"] == 1);
  REQUIRE(tf["и"] == 1);
}

TEST_CASE("Digits are separators when keep_numbers=0") {
  TokenizeOptions opt;
  opt.keep_numbers = false;
  auto tf = tf_map("x86 test123 done", opt);
  REQUIRE(tf["x"] == 1);
  REQUIRE(tf["test"] == 1);
  REQUIRE(tf["done"] == 1);
  REQUIRE(tf.find("86") == tf.end());
  REQUIRE(tf.find("123") == tf.end());
}

TEST_CASE("Digits kept when keep_numbers=1") {
  TokenizeOptions opt;
  opt.keep_numbers = true;
  auto tf = tf_map("x86 test123 done", opt);
  REQUIRE(tf["x86"] == 1);
  REQUIRE(tf["test123"] == 1);
  REQUIRE(tf["done"] == 1);
}

TEST_CASE("min_len filters short tokens") {
  TokenizeOptions opt;
  opt.min_len = 2;
  auto tf = tf_map("я в дом", opt);
  REQUIRE(tf.find("я") == tf.end());
  REQUIRE(tf.find("в") == tf.end());
  REQUIRE(tf["дом"] == 1);
}

TEST_CASE("Stem reduces simple russian suffixes when enabled") {
  TokenizeOptions opt;
  opt.stem = true;
  auto tf = tf_map("машины машина", opt);
  REQUIRE(tf["машин"] == 2);
}
