#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../cpp/boolean/include/search/boolean_ops.hpp"
#include "../cpp/boolean/include/search/tokenizer_stl.hpp"
#include "../cpp/boolean/include/search/nostl/vec.hpp"

static TokenizeOptions optDefault() {
    TokenizeOptions opt;
    opt.keep_numbers = false;
    opt.stem = false;
    opt.min_len = 1;
    return opt;
}

static std::vector<std::string> toks(const std::string& s, TokenizeOptions opt) {
    std::vector<std::string> out;
    tokenize_doc_terms(s, opt, out);
    return out;
}

static nostl::Vec<std::uint32_t> mk(std::initializer_list<std::uint32_t> xs) {
    nostl::Vec<std::uint32_t> v;
    v.reserve(xs.size());
    for (auto x : xs) v.push_back(x);
    return v;
}

TEST_CASE("Tokenizer: basic") {
    auto opt = optDefault();
    auto out = toks("Python И Java", opt);

    REQUIRE(out.size() == 3);
    REQUIRE(out[0] == std::string("python"));
    REQUIRE(out[1] == std::string("и"));
    REQUIRE(out[2] == std::string("java"));
}

TEST_CASE("Tokenizer: keep_numbers") {
    auto opt = optDefault();

    SECTION("keep_numbers = false") {
        opt.keep_numbers = false;
        auto out = toks("win10 123 abc123", opt);

        REQUIRE(out.size() == 2);
        REQUIRE(out[0] == std::string("win"));
        REQUIRE(out[1] == std::string("abc"));
    }

    SECTION("keep_numbers = true") {
        opt.keep_numbers = true;
        auto out = toks("win10 123 abc123", opt);

        REQUIRE(out.size() == 3);
        REQUIRE(out[0] == std::string("win10"));
        REQUIRE(out[1] == std::string("123"));
        REQUIRE(out[2] == std::string("abc123"));
    }
}

TEST_CASE("Boolean ops") {
    SECTION("AND") {
        auto a = mk({1, 2, 4, 10});
        auto b = mk({2, 3, 4, 5});
        auto r = op_and(a, b);

        REQUIRE(r.size == 2);
        REQUIRE(r.data[0] == 2u);
        REQUIRE(r.data[1] == 4u);
    }

    SECTION("OR") {
        auto a = mk({1, 2, 4});
        auto b = mk({2, 3, 5});
        auto r = op_or(a, b);

        REQUIRE(r.size == 5);
        REQUIRE(r.data[0] == 1u);
        REQUIRE(r.data[1] == 2u);
        REQUIRE(r.data[2] == 3u);
        REQUIRE(r.data[3] == 4u);
        REQUIRE(r.data[4] == 5u);
    }

    SECTION("NOT") {
        auto a = mk({1, 3});
        auto r = op_not(a, 5);

        REQUIRE(r.size == 3);
        REQUIRE(r.data[0] == 0u);
        REQUIRE(r.data[1] == 2u);
        REQUIRE(r.data[2] == 4u);
    }
}
