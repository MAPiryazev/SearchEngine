#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <chrono>


#include "search/boolean_ops.hpp"
#include "search/fwd_index.hpp"
#include "search/inv_index6.hpp"
#include "search/nostl/hashmap.hpp"
#include "search/nostl/strpool.hpp"
#include "search/nostl/strview.hpp"
#include "search/nostl/vec.hpp"
#include "search/query_parser.hpp"
#include "search/tokenizer_api.hpp"

struct ScoredDoc {
  std::uint32_t docid;
  double score;
};

static bool startswith(const std::string& s, const std::string& p) {
  if (s.size() < p.size()) return false;
  for (std::size_t i = 0; i < p.size(); ++i) if (s[i] != p[i]) return false;
  return true;
}

static std::string getarg(int argc, char** argv, const std::string& name, const std::string& def) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == name) {
      if (i + 1 < argc) return argv[i + 1];
      return def;
    }
    std::string pref = name + "=";
    if (startswith(a, pref)) return a.substr(pref.size());
  }
  return def;
}

static std::int64_t getargi64(int argc, char** argv, const std::string& name, std::int64_t def) {
  std::string v = getarg(argc, argv, name, "");
  if (v.empty()) return def;
  return std::stoll(v);
}

static bool isbooleanquery(const nostl::Vec<QTok>& toks) {
  for (std::size_t i = 0; i < toks.size; ++i) {
    const QTokType tp = toks.data[i].type;
    if (tp == QTokType::AND || tp == QTokType::OR || tp == QTokType::NOT ||
        tp == QTokType::LPAREN || tp == QTokType::RPAREN) {
      return true;
    }
  }
  return false;
}

static bool scored_less(const ScoredDoc& a, const ScoredDoc& b) {
  if (a.score != b.score) return a.score > b.score;
  return a.docid < b.docid;
}

static void sort_scored_desc(nostl::Vec<ScoredDoc>& v) {
  if (v.size < 2) return;

  auto swap = [](ScoredDoc& a, ScoredDoc& b) {
    ScoredDoc t = a;
    a = b;
    b = t;
  };

  auto qsort = [&](auto&& self, std::ptrdiff_t l, std::ptrdiff_t r) -> void {
    while (l < r) {
      std::ptrdiff_t i = l, j = r;
      ScoredDoc p = v.data[l + (r - l) / 2];

      while (i <= j) {
        while (scored_less(v.data[i], p)) ++i;
        while (scored_less(p, v.data[j])) --j;
        if (i <= j) {
          swap(v.data[i], v.data[j]);
          ++i;
          --j;
        }
      }

      if (j - l < r - i) {
        if (l < j) self(self, l, j);
        l = i;
      } else {
        if (i < r) self(self, i, r);
        r = j;
      }
    }
  };

  qsort(qsort, 0, static_cast<std::ptrdiff_t>(v.size - 1));
}

static void tokenize_query_terms(const std::string& q, const TokenizeOptions& opt,
                                 nostl::StrPool& pool, nostl::Vec<nostl::StrView>& terms) {
  pool = nostl::StrPool();
  terms.clear_keep();
  terms.reserve(16);
  tokenize_doc_terms_sv(nostl::StrView(q.data(), q.size()), opt, pool, terms);
}

static nostl::Vec<std::uint32_t> postings_for_raw_term(InvertedIndex6& inv,
                                                       nostl::StrView raw,
                                                       const TokenizeOptions& opt) {
  nostl::StrPool pool;
  nostl::Vec<nostl::StrView> terms;
  terms.reserve(8);

  tokenize_doc_terms_sv(raw, opt, pool, terms);

  nostl::Vec<std::uint32_t> cur;
  if (terms.size == 0) return cur;

  inv.get_postings(terms.data[0], cur);
  for (std::size_t i = 1; i < terms.size; ++i) {
    nostl::Vec<std::uint32_t> nxt;
    inv.get_postings(terms.data[i], nxt);
    cur = op_and(cur, nxt);
    if (cur.size == 0) break;
  }
  return cur;
}

static nostl::Vec<std::uint32_t> eval_boolean_rpn(InvertedIndex6& inv,
                                                  const nostl::Vec<QTok>& rpn,
                                                  const TokenizeOptions& opt) {
  nostl::Vec<nostl::Vec<std::uint32_t>> st;
  st.reserve(rpn.size);

  for (std::size_t i = 0; i < rpn.size; ++i) {
    const QTok t = rpn.data[i];

    if (t.type == QTokType::TERM) {
      auto v = postings_for_raw_term(inv, t.text, opt);
      st.push_back(std::move(v));
      continue;
    }

    if (t.type == QTokType::NOT) {
      if (st.size == 0) throw std::runtime_error("NOT empty stack");
      auto a = std::move(st.data[st.size - 1]);
      --st.size;
      auto r = op_not(a, inv.docs());
      st.push_back(std::move(r));
      continue;
    }

    if (t.type == QTokType::AND || t.type == QTokType::OR) {
      if (st.size < 2) throw std::runtime_error("binary op: stack size < 2");
      auto b = std::move(st.data[st.size - 1]);
      --st.size;
      auto a = std::move(st.data[st.size - 1]);
      --st.size;
      auto r = (t.type == QTokType::AND) ? op_and(a, b) : op_or(a, b);
      st.push_back(std::move(r));
      continue;
    }

    throw std::runtime_error("unexpected token in RPN");
  }

  if (st.size != 1) throw std::runtime_error("bad query stack size != 1");
  return std::move(st.data[0]);
}

static void union_candidates(InvertedIndex6& inv,
                             const nostl::Vec<nostl::StrView>& qterms,
                             nostl::Vec<std::uint32_t>& out_docs) {
  out_docs.clear_keep();
  nostl::Vec<std::uint32_t> tmp;
  for (std::size_t i = 0; i < qterms.size; ++i) {
    tmp.clear_keep();
    inv.get_postings(qterms.data[i], tmp);
    out_docs = op_or(out_docs, tmp);
  }
}

static void score_docs_tfidf(InvertedIndex6& inv,
                             const nostl::Vec<nostl::StrView>& qterms,
                             const nostl::Vec<std::uint32_t>& candidates,
                             nostl::Vec<ScoredDoc>& out_scored) {
  out_scored.clear_keep();
  if (qterms.size == 0 || candidates.size == 0) return;

  nostl::HashMapSV<std::uint32_t> doc2idx;
  if (!doc2idx.init(candidates.size * 2 + 32)) throw std::runtime_error("oom");

  nostl::StrPool keypool;
  nostl::Vec<double> scores;
  if (!scores.resize(candidates.size)) throw std::runtime_error("oom");
  for (std::size_t i = 0; i < scores.size; ++i) scores.data[i] = 0.0;

  for (std::size_t i = 0; i < candidates.size; ++i) {
    const std::uint32_t docid = candidates.data[i];
    const char* p = reinterpret_cast<const char*>(&docid);
    nostl::StrView k = keypool.add_copy(p, sizeof(docid), false);
    bool inserted = false;
    std::uint32_t* pv = doc2idx.get_or_insert(k, &inserted);
    if (!pv) throw std::runtime_error("oom");
    *pv = static_cast<std::uint32_t>(i);
  }

  const double N = static_cast<double>(inv.docs());

  for (std::size_t ti = 0; ti < qterms.size; ++ti) {
    nostl::Vec<PostingTF> plist;
    inv.get_postings_tf(qterms.data[ti], plist);

    const double df = static_cast<double>(plist.size);
    const double idf = std::log((N + 1.0) / (df + 1.0));

    for (std::size_t j = 0; j < plist.size; ++j) {
      const std::uint32_t docid = plist.data[j].docid;
      const std::uint32_t tf = plist.data[j].tf;

      const char* p = reinterpret_cast<const char*>(&docid);
      nostl::StrView probe(p, sizeof(docid));
      std::uint32_t* idxp = doc2idx.find(probe);
      if (!idxp) continue;

      scores.data[*idxp] += static_cast<double>(tf) * idf;
    }
  }

  out_scored.reserve(candidates.size);
  for (std::size_t i = 0; i < candidates.size; ++i) {
    if (scores.data[i] <= 0.0) continue;
    ScoredDoc sd;
    sd.docid = candidates.data[i];
    sd.score = scores.data[i];
    out_scored.push_back(sd);
  }
}

static int run_one_query(InvertedIndex6& inv, ForwardIndex& fwd,
                         const std::string& q, const TokenizeOptions& opt,
                         std::int64_t topn) {
  auto t0 = std::chrono::steady_clock::now();
  QueryLexResult lex = lex_query(nostl::StrView(q.data(), q.size()));
  const bool is_bool = isbooleanquery(lex.toks);

  nostl::Vec<std::uint32_t> candidates;

  if (is_bool) {
    nostl::Vec<QTok> rpn = to_rpn(lex.toks);
    candidates = eval_boolean_rpn(inv, rpn, opt);
  } else {
    nostl::StrPool qpool;
    nostl::Vec<nostl::StrView> qterms;
    tokenize_query_terms(q, opt, qpool, qterms);
    union_candidates(inv, qterms, candidates);
  }

  nostl::StrPool qpool2;
  nostl::Vec<nostl::StrView> qterms2;
  tokenize_query_terms(q, opt, qpool2, qterms2);

  nostl::Vec<ScoredDoc> scored;
  score_docs_tfidf(inv, qterms2, candidates, scored);
  sort_scored_desc(scored);

  std::cout << "hits " << scored.size << "\n";

  std::size_t show = static_cast<std::size_t>(topn);
  if (show > scored.size) show = scored.size;

  for (std::size_t i = 0; i < show; ++i) {
    const std::uint32_t docid = scored.data[i].docid;
    DocInfoView di = fwd.get(docid);
    std::cout << docid << " " << scored.data[i].score << " ";
    std::cout.write(di.title.data, static_cast<std::streamsize>(di.title.size));
    std::cout << " ";
    std::cout.write(di.url.data, static_cast<std::streamsize>(di.url.size));
    std::cout << "\n";
  }
  auto t1 = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
  std::cout << "time_ms " << ms << "\n";
  return 0;
}

int main(int argc, char** argv) {
  try {
    std::string inv_path = getarg(argc, argv, "--inv", "out6/index6.inv");
    std::string fwd_path = getarg(argc, argv, "--fwd", "out6/index6.fwd");
    std::string q = getarg(argc, argv, "--q", "");
    std::string qfile = getarg(argc, argv, "--qfile", "");
    std::int64_t topn = getargi64(argc, argv, "--top", 20);
    if (topn < 1) topn = 1;

    if (q.empty() && qfile.empty()) {
      std::cerr << "Usage:\n";
      std::cerr << "  ./ranksearch8 --q \"r1 r2\" --inv out6/index6.inv --fwd out6/index6.fwd --top 20\n";
      std::cerr << "  ./ranksearch8 --q \"r1 && r2\" --inv out6/index6.inv --fwd out6/index6.fwd --top 20\n";
      std::cerr << "  ./ranksearch8 --qfile queries.txt --inv out6/index6.inv --fwd out6/index6.fwd --top 20\n";
      return 2;
    }

    TokenizeOptions opt;
    opt.keep_numbers = (getarg(argc, argv, "--keep-numbers", "0") != "0");
    opt.stem = (getarg(argc, argv, "--stem", "0") != "0");
    std::int64_t ml = getargi64(argc, argv, "--min-len", 1);
    if (ml < 1) ml = 1;
    opt.min_len = static_cast<std::size_t>(ml);

    InvertedIndex6 inv;
    ForwardIndex fwd;
    if (!inv.open(inv_path)) throw std::runtime_error("cannot open inv " + inv_path);
    if (!fwd.open(fwd_path)) throw std::runtime_error("cannot open fwd " + fwd_path);
    if (inv.docs() != fwd.docs()) throw std::runtime_error("docs mismatch between inv and fwd");

    if (!qfile.empty()) {
      std::ifstream fin(qfile);
      if (!fin) throw std::runtime_error("cannot open qfile " + qfile);
      std::string line;
      while (std::getline(fin, line)) {
        if (line.empty()) continue;
        run_one_query(inv, fwd, line, opt, topn);
      }
      return 0;
    }

    return run_one_query(inv, fwd, q, opt, topn);
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
