#pragma once

#include <string>
#include <vector>

enum class QTokType { TERM, AND, OR, NOT, LPAREN, RPAREN };

struct QTok {
  QTokType type;
  std::string text; // только для TERM
};

std::vector<QTok> lex_query(const std::string& q);
std::vector<QTok> to_rpn(const std::vector<QTok>& toks);
