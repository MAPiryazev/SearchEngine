#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/boolean_ops.hpp"
#include "../include/fwd_index.hpp"
#include "../include/inv_index.hpp"
#include "../include/query_parser.hpp"
#include "../include/tokenizer.hpp"

static bool starts_with(const std::string& s, const std::string& p) {
  return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

static std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def) {
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == name && i + 1 < argc) return argv[i + 1];
    if (starts_with(a, name + "=")) return a.substr(name.size() + 1);
  }
  return def;
}

static std::int64_t get_arg_i64(int argc, char** argv, const std::string& name, std::int64_t def) {
  std::string v = get_arg(argc, argv, name, "");
  if (v.empty()) return def;
  return std::stoll(v);
}

static std::vector<std::uint32_t> postings_for_raw_term(InvertedIndex& inv,
                                                        const std::string& raw,
                                                        const TokenizeOptions& opt) {
  std::vector<std::string> terms;
  terms.reserve(4);
  tokenize_doc_terms(raw, opt, terms);
  if (terms.empty()) return {};

  std::vector<std::uint32_t> cur;
  inv.get_postings(terms[0], cur);

  for (std::size_t i = 1; i < terms.size(); i++) {
    std::vector<std::uint32_t> nxt;
    inv.get_postings(terms[i], nxt);
    cur = op_and(cur, nxt);
    if (cur.empty()) break;
  }
  return cur;
}

int main(int argc, char** argv) {
  try {
    std::string inv_path = get_arg(argc, argv, "--inv", "out/index.inv");
    std::string fwd_path = get_arg(argc, argv, "--fwd", "out/index.fwd");
    std::string q = get_arg(argc, argv, "--q", "");

    if (q.empty()) {
      std::cerr << "Usage: ./bool_search --q \"term1 AND term2\" [--inv out/index.inv --fwd out/index.fwd --top 20]\n";
      return 2;
    }

    std::int64_t top_n = get_arg_i64(argc, argv, "--top", 20);
    if (top_n < 1) top_n = 1;

    TokenizeOptions opt;
    opt.keep_numbers = (get_arg(argc, argv, "--keep-numbers", "0") != "0");
    opt.stem = (get_arg(argc, argv, "--stem", "0") != "0");
    opt.min_len = static_cast<std::size_t>(std::max<std::int64_t>(1, get_arg_i64(argc, argv, "--min-len", 1)));

    InvertedIndex inv;
    ForwardIndex fwd;
    if (!inv.open(inv_path)) throw std::runtime_error("cannot open inv: " + inv_path);
    if (!fwd.open(fwd_path)) throw std::runtime_error("cannot open fwd: " + fwd_path);
    if (inv.docs() != fwd.docs()) throw std::runtime_error("docs mismatch between inv and fwd");

    auto toks = lex_query(q);
    auto rpn = to_rpn(toks);

    std::vector<std::vector<std::uint32_t>> st;
    st.reserve(rpn.size());

    for (const auto& t : rpn) {
      if (t.type == QTokType::TERM) {
        st.push_back(postings_for_raw_term(inv, t.text, opt));
        continue;
      }

      if (t.type == QTokType::NOT) {
        if (st.empty()) throw std::runtime_error("NOT: empty stack");
        auto a = std::move(st.back());
        st.pop_back();
        st.push_back(op_not(a, inv.docs()));
        continue;
      }

      if (t.type == QTokType::AND || t.type == QTokType::OR) {
        if (st.size() < 2) throw std::runtime_error("binary op: stack size < 2");
        auto b = std::move(st.back()); st.pop_back();
        auto a = std::move(st.back()); st.pop_back();
        if (t.type == QTokType::AND) st.push_back(op_and(a, b));
        else st.push_back(op_or(a, b));
        continue;
      }

      throw std::runtime_error("unexpected token in RPN");
    }

    if (st.size() != 1) throw std::runtime_error("bad query: stack size != 1");
    const auto& res = st.back();

    std::cout << "hits=" << res.size() << "\n";
    std::size_t show = static_cast<std::size_t>(std::min<std::int64_t>(top_n, static_cast<std::int64_t>(res.size())));

    for (std::size_t i = 0; i < show; i++) {
      std::uint32_t doc_id = res[i];
      DocInfo di = fwd.get(doc_id);
      std::cout << doc_id << "\t" << di.title << "\t" << di.url << "\n";
    }

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
