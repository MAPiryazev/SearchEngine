#pragma once

#include <cstddef>
#include <cstdint>

#include "nostl/hashmap.hpp"
#include "nostl/strpool.hpp"
#include "nostl/strview.hpp"
#include "nostl/vec.hpp"

struct TokenizeOptions {
    bool keep_numbers = false;
    bool stem = false;
    std::size_t min_len = 1;
};

void tokenize_doc_terms_sv(nostl::StrView text,
                           const TokenizeOptions& opt,
                           nostl::StrPool& pool,
                           nostl::Vec<nostl::StrView>& out_terms);

void tokenize_text_tf_nostl(nostl::StrView text,
                            const TokenizeOptions& opt,
                            std::uint64_t& tokens_total,
                            std::uint64_t& token_chars_total,
                            nostl::StrPool& pool,
                            nostl::HashMapSV<std::uint32_t>& tf);
