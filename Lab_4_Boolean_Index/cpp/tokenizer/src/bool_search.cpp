#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../include/boolean_ops.hpp"
#include "../include/fwd_index.hpp"
#include "../include/inv_index.hpp"
#include "../include/query_parser.hpp"
#include "../include/tokenizer_api.hpp"

#include "../include/nostl/util.hpp"
#include "../include/nostl/vec.hpp"
#include "../include/nostl/strpool.hpp"
#include "../include/nostl/strview.hpp"

static bool starts_with(const std::string& s, const std::string& p) {
    if (s.size() < p.size()) return false;
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (s[i] != p[i]) return false;
    }
    return true;
}

static std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == name && i + 1 < argc) return argv[i + 1];
        std::string pref = name + "=";
        if (starts_with(a, pref)) return a.substr(name.size() + 1);
    }
    return def;
}

static std::int64_t get_arg_i64(int argc, char** argv, const std::string& name, std::int64_t def) {
    std::string v = get_arg(argc, argv, name, "");
    if (v.empty()) return def;
    return std::stoll(v);
}

static nostl::Vec<std::uint32_t> postings_for_raw_term(InvertedIndex& inv,
                                                       nostl::StrView raw,
                                                       const TokenizeOptions& opt) {
    nostl::StrPool pool;
    nostl::Vec<nostl::StrView> terms;
    pool.reserve(raw.size + 16);
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
        std::int64_t ml = get_arg_i64(argc, argv, "--min-len", 1);
        if (ml < 1) ml = 1;
        opt.min_len = static_cast<std::size_t>(ml);

        InvertedIndex inv;
        ForwardIndex fwd;
        if (!inv.open(inv_path)) throw std::runtime_error("cannot open inv: " + inv_path);
        if (!fwd.open(fwd_path)) throw std::runtime_error("cannot open fwd: " + fwd_path);
        if (inv.docs() != fwd.docs()) throw std::runtime_error("docs mismatch between inv and fwd");

        QueryLexResult lex = lex_query(nostl::StrView(q.c_str(), q.size()));
        nostl::Vec<QTok> rpn = to_rpn(lex.toks);

        nostl::Vec< nostl::Vec<std::uint32_t> > st;
        st.reserve(rpn.size);

        for (std::size_t i = 0; i < rpn.size; ++i) {
            const QTok t = rpn.data[i];

            if (t.type == QTokType::TERM) {
                nostl::Vec<std::uint32_t> v = postings_for_raw_term(inv, t.text, opt);
                st.push_back(nostl::move(v));
                continue;
            }

            if (t.type == QTokType::NOT) {
                if (st.size == 0) throw std::runtime_error("NOT: empty stack");
                nostl::Vec<std::uint32_t> a = nostl::move(st.data[st.size - 1]);
                --st.size;

                nostl::Vec<std::uint32_t> r = op_not(a, inv.docs());
                st.push_back(nostl::move(r));
                continue;
            }

            if (t.type == QTokType::AND || t.type == QTokType::OR) {
                if (st.size < 2) throw std::runtime_error("binary op: stack size < 2");
                nostl::Vec<std::uint32_t> b = nostl::move(st.data[st.size - 1]);
                --st.size;
                nostl::Vec<std::uint32_t> a = nostl::move(st.data[st.size - 1]);
                --st.size;

                nostl::Vec<std::uint32_t> r = (t.type == QTokType::AND) ? op_and(a, b) : op_or(a, b);
                st.push_back(nostl::move(r));
                continue;
            }

            throw std::runtime_error("unexpected token in RPN");
        }

        if (st.size != 1) throw std::runtime_error("bad query: stack size != 1");
        const nostl::Vec<std::uint32_t>& res = st.data[0];

        std::cout << "hits=" << res.size << "\n";
        std::size_t show = static_cast<std::size_t>(top_n);
        if (show > res.size) show = res.size;

        for (std::size_t i = 0; i < show; ++i) {
            std::uint32_t doc_id = res.data[i];
            DocInfoView di = fwd.get(doc_id);

            std::cout << doc_id << "\t";
            std::cout.write(di.title.data, static_cast<std::streamsize>(di.title.size));
            std::cout << "\t";
            std::cout.write(di.url.data, static_cast<std::streamsize>(di.url.size));
            std::cout << "\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
