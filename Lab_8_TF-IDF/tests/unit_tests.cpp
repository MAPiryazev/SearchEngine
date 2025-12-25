#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "search/boolean_ops.hpp"
#include "search/query_parser.hpp"
#include "search/tokenizer_stl.hpp"
#include "search/inv_index6.hpp"
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

TEST_CASE("Tokenizer: ё to e") {
  auto opt = opt_default();
  auto out = toks_stl("Ёжик и ёлка", opt);

  REQUIRE(out.size() == 3);
  REQUIRE(out[0] == "ежик");
  REQUIRE(out[1] == "и");
  REQUIRE(out[2] == "елка");
}

TEST_CASE("Tokenizer: punctuation separators") {
  auto opt = opt_default();
  auto out = toks_stl("hello,world! (test)... ok?", opt);

  REQUIRE(out.size() == 4);
  REQUIRE(out[0] == "hello");
  REQUIRE(out[1] == "world");
  REQUIRE(out[2] == "test");
  REQUIRE(out[3] == "ok");
}

TEST_CASE("Tokenizer: min_len drops short tokens") {
  auto opt = opt_default();
  opt.min_len = 2;

  auto out = toks_stl("я и ты в it", opt);

  REQUIRE(out.size() == 2);
  REQUIRE(out[0] == "ты");
  REQUIRE(out[1] == "it");
}

TEST_CASE("Tokenizer: hyphen inside word") {
  auto opt = opt_default();
  auto out = toks_stl("санкт-петербург e-mail", opt);

  REQUIRE(out.size() == 2);
  REQUIRE(out[0] == "санкт-петербург");
  REQUIRE(out[1] == "e-mail");
}

// ========== Расширенные тесты на токенизацию ==========

TEST_CASE("Tokenizer: mixed scripts separation") {
  auto opt = opt_default();
  auto out = toks_stl("Python программирование", opt);

  REQUIRE(out.size() == 2);
  REQUIRE(out[0] == "python");
  REQUIRE(out[1] == "программирование");
}

TEST_CASE("Tokenizer: numbers with letters") {
  auto opt = opt_default();
  
  SECTION("keep_numbers = false") {
    opt.keep_numbers = false;
    auto out = toks_stl("test123 456abc", opt);
    REQUIRE(out.size() == 2);
    REQUIRE(out[0] == "test");
    REQUIRE(out[1] == "abc");
  }
  
  SECTION("keep_numbers = true") {
    opt.keep_numbers = true;
    auto out = toks_stl("test123 456abc", opt);
    REQUIRE(out.size() == 2);
    REQUIRE(out[0] == "test123");
    REQUIRE(out[1] == "456abc");
  }
}

TEST_CASE("Tokenizer: empty and whitespace") {
  auto opt = opt_default();
  
  auto out1 = toks_stl("", opt);
  REQUIRE(out1.size() == 0);
  
  auto out2 = toks_stl("   ", opt);
  REQUIRE(out2.size() == 0);
  
  auto out3 = toks_stl("  word  ", opt);
  REQUIRE(out3.size() == 1);
  REQUIRE(out3[0] == "word");
}

TEST_CASE("Tokenizer: special characters") {
  auto opt = opt_default();
  auto out = toks_stl("test@example.com file_name.txt", opt);
  
  REQUIRE(out.size() >= 4);
  REQUIRE(out[0] == "test");
  REQUIRE(out[1] == "example");
  REQUIRE(out[2] == "com");
  REQUIRE(out[3] == "file");
}

TEST_CASE("Tokenizer: cyrillic case handling") {
  auto opt = opt_default();
  auto out = toks_stl("ПРОГРАММИРОВАНИЕ Программирование", opt);
  
  REQUIRE(out.size() == 2);
  REQUIRE(out[0] == "программирование");
  REQUIRE(out[1] == "программирование");
}

// ========== Тесты на стемминг русского языка ==========

TEST_CASE("Stemming Russian: noun cases") {
  auto opt = opt_default();
  opt.stem = true;
  
  // Именительный падеж (основа)
  auto out1 = toks_stl("программирование", opt);
  REQUIRE(out1[0] == "программирован");
  
  // Родительный падеж
  auto out2 = toks_stl("программирования", opt);
  REQUIRE(out2[0] == "программировани");
  
  // Дательный падеж
  auto out3 = toks_stl("программированию", opt);
  REQUIRE(out3[0] == "программировани");
  
  // Винительный падеж
  auto out4 = toks_stl("программирование", opt);
  REQUIRE(out4[0] == "программирован");
  
  // Творительный падеж
  auto out5 = toks_stl("программированием", opt);
  REQUIRE(out5[0] == "программировани");
  
  // Предложный падеж
  auto out6 = toks_stl("программировании", opt);
  REQUIRE(out6[0] == "программировани");
}

TEST_CASE("Stemming Russian: adjective cases") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("красивый", opt);
  REQUIRE(out1[0] == "красив");
  
  auto out2 = toks_stl("красивая", opt);
  REQUIRE(out2[0] == "красив");
  
  auto out3 = toks_stl("красивое", opt);
  REQUIRE(out3[0] == "красив");
  
  auto out4 = toks_stl("красивые", opt);
  REQUIRE(out4[0] == "красив");
  
  auto out5 = toks_stl("красивым", opt);
  REQUIRE(out5[0] == "красив");
}

TEST_CASE("Stemming Russian: verb forms") {
  auto opt = opt_default();
  opt.stem = true;
  
  // Инфинитив
  auto out1 = toks_stl("программировать", opt);
  REQUIRE(out1[0] == "программиров");
  
  // Настоящее время
  auto out2 = toks_stl("программирую", opt);
  REQUIRE(out2[0] == "программир");
  
  auto out3 = toks_stl("программирует", opt);
  REQUIRE(out3[0] == "программиру");
  
  // Прошедшее время
  auto out4 = toks_stl("программировал", opt);
  REQUIRE(out4[0] == "программиров");
  
  auto out5 = toks_stl("программировала", opt);
  REQUIRE(out5[0] == "программиров");
  
  auto out6 = toks_stl("программировали", opt);
  REQUIRE(out6[0] == "программиров");
}

TEST_CASE("Stemming Russian: plural forms") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("программисты", opt);
  REQUIRE(out1[0] == "программист");
  
  auto out2 = toks_stl("программистами", opt);
  REQUIRE(out2[0] == "программист");
  
  auto out3 = toks_stl("программистах", opt);
  REQUIRE(out3[0] == "программист");
}

TEST_CASE("Stemming Russian: soft sign") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("мать", opt);
  REQUIRE(out1[0] == "мат");
  
  auto out2 = toks_stl("день", opt);
  REQUIRE(out2[0] == "ден");
}

TEST_CASE("Stemming Russian: no change for short words") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("я", opt);
  REQUIRE(out1[0] == "я");
  
  auto out2 = toks_stl("он", opt);
  REQUIRE(out2[0] == "он");
  
  auto out3 = toks_stl("мы", opt);
  REQUIRE(out3[0] == "мы");
}

TEST_CASE("Stemming Russian: complex words") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out = toks_stl("программирования алгоритмов", opt);
  REQUIRE(out.size() == 2);
  REQUIRE(out[0] == "программировани");
  // Алгоритмов может не стеммиться полностью из-за короткой основы
  REQUIRE(out[1].size() >= 7);
}

// ========== Тесты на стемминг английского языка ==========

TEST_CASE("Stemming English: plural forms") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("computers", opt);
  REQUIRE(out1[0] == "comput");
  
  auto out2 = toks_stl("programs", opt);
  REQUIRE(out2[0] == "program");
  
  auto out3 = toks_stl("classes", opt);
  REQUIRE(out3[0] == "class");
}

TEST_CASE("Stemming English: verb forms") {
  auto opt = opt_default();
  opt.stem = true;
  
  // -ed forms
  auto out1 = toks_stl("programmed", opt);
  REQUIRE(out1[0] == "program");
  
  auto out2 = toks_stl("computed", opt);
  REQUIRE(out2[0] == "comput");
  
  // -ing forms
  auto out3 = toks_stl("programming", opt);
  REQUIRE(out3[0] == "program");
  
  auto out4 = toks_stl("computing", opt);
  REQUIRE(out4[0] == "comput");
  
  // Special cases
  auto out5 = toks_stl("running", opt);
  REQUIRE(out5[0] == "run");
  
  auto out6 = toks_stl("stopping", opt);
  REQUIRE(out6[0] == "stop");
}

TEST_CASE("Stemming English: comparative and superlative") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("faster", opt);
  REQUIRE(out1[0] == "fast");
  
  auto out2 = toks_stl("fastest", opt);
  REQUIRE(out2[0] == "fastest");
  
  auto out3 = toks_stl("bigger", opt);
  REQUIRE(out3[0] == "bigg");
}

TEST_CASE("Stemming English: -tion, -sion") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("computation", opt);
  REQUIRE(out1[0] == "comput");
  
  auto out2 = toks_stl("programming", opt);
  REQUIRE(out2[0] == "program");
  
  auto out3 = toks_stl("decision", opt);
  REQUIRE(out3[0] == "decis");
}

TEST_CASE("Stemming English: -ly, -er, -est") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("quickly", opt);
  REQUIRE(out1[0] == "quickly");
  
  auto out2 = toks_stl("faster", opt);
  REQUIRE(out2[0] == "fast");
  
  auto out3 = toks_stl("programmer", opt);
  REQUIRE(out3[0] == "programm");
}

TEST_CASE("Stemming English: -ize, -ization") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("organize", opt);
  REQUIRE(out1[0] == "organ");
  
  auto out2 = toks_stl("organization", opt);
  REQUIRE(out2[0] == "organ");
  
  auto out3 = toks_stl("optimization", opt);
  REQUIRE(out3[0] == "optim");
}

TEST_CASE("Stemming English: -y to -i") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("happily", opt);
  REQUIRE(out1[0] == "happily");
  
  auto out2 = toks_stl("studies", opt);
  REQUIRE(out2[0] == "studi");
  
  auto out3 = toks_stl("happy", opt);
  REQUIRE(out3[0] == "happy");
}

TEST_CASE("Stemming English: final -e removal") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("compute", opt);
  REQUIRE(out1[0] == "comput");
  
  auto out2 = toks_stl("programme", opt);
  REQUIRE(out2[0] == "programm");
  
  // But not if it would make word too short
  auto out3 = toks_stl("be", opt);
  REQUIRE(out3[0] == "be");
}

TEST_CASE("Stemming English: double consonant") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out1 = toks_stl("running", opt);
  REQUIRE(out1[0] == "run");
  
  auto out2 = toks_stl("stopping", opt);
  REQUIRE(out2[0] == "stop");
}

// ========== Тесты на смешанный стемминг ==========

TEST_CASE("Stemming: mixed Russian and English") {
  auto opt = opt_default();
  opt.stem = true;
  
  auto out = toks_stl("программирование programming", opt);
  REQUIRE(out.size() == 2);
  REQUIRE(out[0] == "программирован");
  REQUIRE(out[1] == "program");
}

TEST_CASE("Stemming: disabled") {
  auto opt = opt_default();
  opt.stem = false;
  
  auto out = toks_stl("программирования computers", opt);
  REQUIRE(out.size() == 2);
  REQUIRE(out[0] == "программирования");
  REQUIRE(out[1] == "computers");
}

// ========== Дополнительные тесты на токенизацию ==========

TEST_CASE("Tokenizer: unicode handling") {
  auto opt = opt_default();
  auto out = toks_stl("Привет Hello 你好", opt);
  
  REQUIRE(out.size() == 2);
  REQUIRE(out[0] == "привет");
  REQUIRE(out[1] == "hello");
}

TEST_CASE("Tokenizer: mixed case") {
  auto opt = opt_default();
  auto out = toks_stl("Python JAVA c++", opt);
  
  REQUIRE(out.size() == 3);
  REQUIRE(out[0] == "python");
  REQUIRE(out[1] == "java");
  REQUIRE(out[2] == "c");
}

TEST_CASE("Tokenizer: multiple spaces") {
  auto opt = opt_default();
  auto out = toks_stl("word1    word2\t\tword3\n\nword4", opt);
  
  REQUIRE(out.size() == 4);
  // При keep_numbers=false числа удаляются
  REQUIRE(out[0] == "word");
  REQUIRE(out[1] == "word");
  REQUIRE(out[2] == "word");
  REQUIRE(out[3] == "word");
}