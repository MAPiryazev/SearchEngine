#pragma once

#include <cstddef>
#include <cstdint>

#include "nostl/vec.hpp"
#include "nostl/strview.hpp"
#include "nostl/strpool.hpp"

enum class QTokType { TERM, AND, OR, NOT, LPAREN, RPAREN };

struct QTok {
  QTokType type;
  nostl::StrView text;
};

struct QueryLexResult {
  nostl::Vec<QTok> toks;
  nostl::StrPool pool;
};

QueryLexResult lex_query(nostl::StrView q);
nostl::Vec<QTok> to_rpn(const nostl::Vec<QTok>& toks);
