#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "tokenizer_api.hpp"

void tokenize_text_tf(const std::string& text, const TokenizeOptions& opt,
                      std::uint64_t& tokens_total,
                      std::uint64_t& token_chars_total,
                      std::unordered_map<std::string, std::uint32_t>& tf);

void tokenize_doc_terms(const std::string& text, const TokenizeOptions& opt,
                        std::vector<std::string>& out_terms);
