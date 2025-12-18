#include "../include/query_parser.hpp"

#include <cctype>
#include <stdexcept>

static std::string to_lower_ascii(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

std::vector<QTok> lex_query(const std::string& q) {
  std::vector<QTok> out;
  std::size_t i = 0;

  while (i < q.size()) {
    while (i < q.size() && std::isspace(static_cast<unsigned char>(q[i]))) i++;
    if (i >= q.size()) break;

    char c = q[i];
    if (c == '(') { out.push_back({QTokType::LPAREN, ""}); i++; continue; }
    if (c == ')') { out.push_back({QTokType::RPAREN, ""}); i++; continue; }

    std::size_t j = i;
    while (j < q.size() && !std::isspace(static_cast<unsigned char>(q[j])) && q[j] != '(' && q[j] != ')') j++;
    std::string tok = q.substr(i, j - i);
    std::string low = to_lower_ascii(tok);

    if (low == "and") out.push_back({QTokType::AND, ""});
    else if (low == "or") out.push_back({QTokType::OR, ""});
    else if (low == "not") out.push_back({QTokType::NOT, ""});
    else out.push_back({QTokType::TERM, tok});

    i = j;
  }

  return out;
}

static int prec(QTokType t) {
  if (t == QTokType::NOT) return 3;
  if (t == QTokType::AND) return 2;
  if (t == QTokType::OR) return 1;
  return 0;
}

static bool is_op(QTokType t) {
  return t == QTokType::NOT || t == QTokType::AND || t == QTokType::OR;
}

std::vector<QTok> to_rpn(const std::vector<QTok>& toks) {
  std::vector<QTok> out;
  std::vector<QTok> st;

  for (const auto& t : toks) {
    if (t.type == QTokType::TERM) {
      out.push_back(t);
      continue;
    }

    if (is_op(t.type)) {
      while (!st.empty() && is_op(st.back().type)) {
        QTokType top = st.back().type;
        bool right_assoc = (t.type == QTokType::NOT);
        if ((right_assoc && prec(top) > prec(t.type)) ||
            (!right_assoc && prec(top) >= prec(t.type))) {
          out.push_back(st.back());
          st.pop_back();
        } else break;
      }
      st.push_back(t);
      continue;
    }

    if (t.type == QTokType::LPAREN) {
      st.push_back(t);
      continue;
    }

    if (t.type == QTokType::RPAREN) {
      while (!st.empty() && st.back().type != QTokType::LPAREN) {
        out.push_back(st.back());
        st.pop_back();
      }
      if (st.empty() || st.back().type != QTokType::LPAREN) throw std::runtime_error("mismatched parentheses");
      st.pop_back();
      continue;
    }
  }

  while (!st.empty()) {
    if (st.back().type == QTokType::LPAREN || st.back().type == QTokType::RPAREN) {
      throw std::runtime_error("mismatched parentheses");
    }
    out.push_back(st.back());
    st.pop_back();
  }

  return out;
}
