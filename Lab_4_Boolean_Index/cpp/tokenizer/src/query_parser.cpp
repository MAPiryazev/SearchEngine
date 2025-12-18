#include "../include/query_parser.hpp"

#include <cctype>
#include <stdexcept>

static char lower_ascii(char c) {
  if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
  return c;
}

static bool ieq_kw(nostl::StrView s, const char* kw, std::size_t kwlen) {
  if (s.size != kwlen) return false;
  for (std::size_t i = 0; i < kwlen; ++i) {
    if (lower_ascii(s.data[i]) != kw[i]) return false;
  }
  return true;
}

static void must_push(nostl::Vec<QTok>& v, const QTok& t) {
  if (!v.push_back(t)) throw std::runtime_error("oom");
}

QueryLexResult lex_query(nostl::StrView q) {
  QueryLexResult res;
  res.toks.reserve(64);
  res.pool.reserve(q.size + 1);

  std::size_t i = 0;
  while (i < q.size) {
    while (i < q.size && std::isspace(static_cast<unsigned char>(q.data[i]))) ++i;
    if (i >= q.size) break;

    const char c = q.data[i];
    if (c == '(') {
      must_push(res.toks, QTok{QTokType::LPAREN, nostl::StrView(nullptr, 0)});
      ++i;
      continue;
    }
    if (c == ')') {
      must_push(res.toks, QTok{QTokType::RPAREN, nostl::StrView(nullptr, 0)});
      ++i;
      continue;
    }

    std::size_t j = i;
    while (j < q.size &&
           !std::isspace(static_cast<unsigned char>(q.data[j])) &&
           q.data[j] != '(' && q.data[j] != ')') {
      ++j;
    }

    nostl::StrView tok(q.data + i, j - i);

    if (ieq_kw(tok, "and", 3)) {
      must_push(res.toks, QTok{QTokType::AND, nostl::StrView(nullptr, 0)});
    } else if (ieq_kw(tok, "or", 2)) {
      must_push(res.toks, QTok{QTokType::OR, nostl::StrView(nullptr, 0)});
    } else if (ieq_kw(tok, "not", 3)) {
      must_push(res.toks, QTok{QTokType::NOT, nostl::StrView(nullptr, 0)});
    } else {
      nostl::StrView saved = res.pool.add_copy(tok.data, tok.size, true);
      if (!saved.data) throw std::runtime_error("oom");
      must_push(res.toks, QTok{QTokType::TERM, saved});
    }

    i = j;
  }

  return res;
}

static int prec(QTokType t) {
  if (t == QTokType::NOT) return 3;
  if (t == QTokType::AND) return 2;
  if (t == QTokType::OR)  return 1;
  return 0;
}

static bool is_op(QTokType t) {
  return t == QTokType::NOT || t == QTokType::AND || t == QTokType::OR;
}

nostl::Vec<QTok> to_rpn(const nostl::Vec<QTok>& toks) {
  nostl::Vec<QTok> out;
  nostl::Vec<QTok> st;

  out.reserve(toks.size);
  st.reserve(toks.size);

  for (std::size_t k = 0; k < toks.size; ++k) {
    const QTok t = toks.data[k];

    if (t.type == QTokType::TERM) {
      must_push(out, t);
      continue;
    }

    if (is_op(t.type)) {
      while (st.size && is_op(st.data[st.size - 1].type)) {
        QTokType top = st.data[st.size - 1].type;
        const bool right_assoc = (t.type == QTokType::NOT);

        if ((right_assoc && prec(top) > prec(t.type)) ||
            (!right_assoc && prec(top) >= prec(t.type))) {
          must_push(out, st.data[st.size - 1]);
          --st.size;
        } else {
          break;
        }
      }
      must_push(st, t);
      continue;
    }

    if (t.type == QTokType::LPAREN) {
      must_push(st, t);
      continue;
    }

    if (t.type == QTokType::RPAREN) {
      while (st.size && st.data[st.size - 1].type != QTokType::LPAREN) {
        must_push(out, st.data[st.size - 1]);
        --st.size;
      }
      if (!st.size || st.data[st.size - 1].type != QTokType::LPAREN) {
        throw std::runtime_error("mismatched parentheses");
      }
      --st.size;
      continue;
    }
  }

  while (st.size) {
    if (st.data[st.size - 1].type == QTokType::LPAREN ||
        st.data[st.size - 1].type == QTokType::RPAREN) {
      throw std::runtime_error("mismatched parentheses");
    }
    must_push(out, st.data[st.size - 1]);
    --st.size;
  }

  return out;
}
