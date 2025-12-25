#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <chrono>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/uri.hpp>

#include "search/tokenizer_api.hpp"
#include "search/index_format.hpp"

#include "search/nostl/fs.hpp"
#include "search/nostl/hashmap.hpp"
#include "search/nostl/sort.hpp"
#include "search/nostl/sort_pair.hpp"
#include "search/nostl/strpool.hpp"
#include "search/nostl/strview.hpp"
#include "search/nostl/vec.hpp"

static bool cstr_starts_with(const char* s, const char* p) {
    while (*p) {
        if (*s != *p) return false;
        ++s;
        ++p;
    }
    return true;
}

static const char* get_arg(int argc, char** argv, const char* name, const char* def) {
    const std::size_t nlen = std::strlen(name);
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, name) == 0) {
            if (i + 1 < argc) return argv[i + 1];
            return def;
        }
        if (cstr_starts_with(a, name) && a[nlen] == '=') return a + nlen + 1;
    }
    return def;
}

static std::int64_t get_arg_i64(int argc, char** argv, const char* name, std::int64_t def) {
    const char* v = get_arg(argc, argv, name, "");
    if (!v || !*v) return def;
    char* endp = nullptr;
    long long x = std::strtoll(v, &endp, 10);
    if (!endp || *endp != 0) return def;
    return static_cast<std::int64_t>(x);
}

static void write_u32(std::ofstream& out, std::uint32_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

static void write_u64(std::ofstream& out, std::uint64_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

static void write_bytes(std::ofstream& out, const void* p, std::size_t n) {
    out.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n));
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = hex_val(s[i + 1]);
            int lo = hex_val(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if (s[i] == '_') { out.push_back(' '); continue; }
        out.push_back(s[i]);
    }
    return out;
}

static std::string wiki_title_from_url(const std::string& url) {
    const std::string key = "/wiki/";
    std::size_t pos = url.find(key);
    if (pos == std::string::npos) return std::string();

    std::string tail = url.substr(pos + key.size());
    std::size_t q = tail.find_first_of("?#");
    if (q != std::string::npos) tail.resize(q);
    return url_decode(tail);
}

struct DocRec {
    std::uint64_t url_off;
    std::uint32_t url_len;
    std::uint64_t title_off;
    std::uint32_t title_len;
};

static bool pool_append(nostl::Vec<char>& pool, const char* p, std::size_t n) {
    std::size_t off = pool.size;
    if (!pool.resize(off + n)) return false;
    if (n) std::memcpy(pool.data + off, p, n);
    return true;
}

struct TermItem {
    nostl::StrView term;
    std::uint32_t old_id;
};

static void swap_term_item(TermItem& a, TermItem& b) {
    TermItem t = a;
    a = b;
    b = t;
}

static bool term_item_less(const TermItem& a, const TermItem& b) {
    return nostl::sv_cmp(a.term, b.term) < 0;
}

static void qsort_term_items(TermItem* v, std::ptrdiff_t l, std::ptrdiff_t r) {
    while (l < r) {
        std::ptrdiff_t i = l, j = r;
        TermItem p = v[l + (r - l) / 2];
        while (i <= j) {
            while (term_item_less(v[i], p)) ++i;
            while (term_item_less(p, v[j])) --j;
            if (i <= j) { swap_term_item(v[i], v[j]); ++i; --j; }
        }
        if (j - l < r - i) {
            if (l < j) qsort_term_items(v, l, j);
            l = i;
        } else {
            if (i < r) qsort_term_items(v, i, r);
            r = j;
        }
    }
}

static void sort_term_items(nostl::Vec<TermItem>& v) {
    if (v.size < 2) return;
    qsort_term_items(v.data, 0, static_cast<std::ptrdiff_t>(v.size - 1));
}

struct Occ {
    std::uint32_t term;
    std::uint32_t doc;
    std::uint32_t pos;
};

static bool occ_less(const Occ& a, const Occ& b) {
    if (a.term != b.term) return a.term < b.term;
    if (a.doc != b.doc) return a.doc < b.doc;
    return a.pos < b.pos;
}

static void swap_occ(Occ& a, Occ& b) {
    Occ t = a;
    a = b;
    b = t;
}

static void qsort_occ(Occ* v, std::ptrdiff_t l, std::ptrdiff_t r) {
    while (l < r) {
        std::ptrdiff_t i = l, j = r;
        Occ p = v[l + (r - l) / 2];
        while (i <= j) {
            while (occ_less(v[i], p)) ++i;
            while (occ_less(p, v[j])) --j;
            if (i <= j) { swap_occ(v[i], v[j]); ++i; --j; }
        }
        if (j - l < r - i) {
            if (l < j) qsort_occ(v, l, j);
            l = i;
        } else {
            if (i < r) qsort_occ(v, i, r);
            r = j;
        }
    }
}

static void sort_occ(nostl::Vec<Occ>& v) {
    if (v.size < 2) return;
    qsort_occ(v.data, 0, static_cast<std::ptrdiff_t>(v.size - 1));
}

int main(int argc, char** argv) {
    try {
        auto t0 = std::chrono::steady_clock::now();

        const char* uri_c = get_arg(argc, argv, "--uri", "mongodb://localhost:27017/");
        const char* db_c  = get_arg(argc, argv, "--db", "search_engine_clean");
        const char* col_c = get_arg(argc, argv, "--col", "documents");
        const char* out_c = get_arg(argc, argv, "--out", "out");

        std::int64_t limit_docs = get_arg_i64(argc, argv, "--limit-docs", -1);

        TokenizeOptions opt;
        opt.keep_numbers = (std::strcmp(get_arg(argc, argv, "--keep-numbers", "0"), "0") != 0);
        opt.stem         = (std::strcmp(get_arg(argc, argv, "--stem", "0"), "0") != 0);

        std::int64_t ml = get_arg_i64(argc, argv, "--min-len", 1);
        if (ml < 1) ml = 1;
        opt.min_len = static_cast<std::size_t>(ml);

        if (nostl::mkdir_p(out_c) != 0) {
            std::fprintf(stderr, "Error: cannot create dir: %s\n", out_c);
            return 1;
        }

        mongocxx::instance inst{};
        mongocxx::client client{mongocxx::uri{std::string(uri_c)}};
        auto col = client[std::string(db_c)][std::string(col_c)];

        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;

        mongocxx::options::find fopts{};
        fopts.projection(make_document(kvp("clean_text", 1), kvp("url", 1), kvp("title", 1)));
        fopts.batch_size(512);

        nostl::Vec<DocRec> fwd;
        nostl::Vec<char> fwd_pool;
        fwd.reserve(100000);
        fwd_pool.reserve(1 << 20);

        nostl::HashMapSV<std::uint32_t> term_id;
        term_id.init(1 << 20);

        nostl::StrPool term_pool;
        term_pool.reserve(1 << 20);

        nostl::Vec<nostl::StrView> terms_raw;
        terms_raw.reserve(1 << 18);

        nostl::Vec<Occ> occ;
        occ.reserve(1 << 20);

        std::uint32_t doc_id = 0;

        for (auto&& doc : col.find({}, fopts)) {
            auto it_text = doc.find("clean_text");
            if (it_text == doc.end() || it_text->type() != bsoncxx::type::k_string) continue;

            std::string url;
            std::string title;

            auto it_url = doc.find("url");
            if (it_url != doc.end() && it_url->type() == bsoncxx::type::k_string) {
                auto sv = it_url->get_string().value;
                url.assign(sv.data(), sv.size());
            }

            auto it_title = doc.find("title");
            if (it_title != doc.end() && it_title->type() == bsoncxx::type::k_string) {
                auto sv = it_title->get_string().value;
                title.assign(sv.data(), sv.size());
            }

            if (title.empty() && !url.empty()) {
                std::string wt = wiki_title_from_url(url);
                if (!wt.empty()) title = wt;
            }
            if (title.empty() && !url.empty()) title = url;

            const std::uint64_t url_off = static_cast<std::uint64_t>(fwd_pool.size);
            const std::uint32_t url_len = static_cast<std::uint32_t>(url.size());
            if (!pool_append(fwd_pool, url.data(), url.size())) throw std::runtime_error("oom");

            const std::uint64_t title_off = static_cast<std::uint64_t>(fwd_pool.size);
            const std::uint32_t title_len = static_cast<std::uint32_t>(title.size());
            if (!pool_append(fwd_pool, title.data(), title.size())) throw std::runtime_error("oom");

            if (!fwd.push_back(DocRec{url_off, url_len, title_off, title_len})) throw std::runtime_error("oom");

            auto sv = it_text->get_string().value;
            nostl::StrView textv(sv.data(), static_cast<std::size_t>(sv.size()));

            nostl::StrPool doc_pool;
            nostl::Vec<TermPos> doc_terms;
            doc_pool.reserve(textv.size + 16);
            doc_terms.reserve(256);

            tokenize_doc_terms_pos_sv(textv, opt, doc_pool, doc_terms);

            for (std::size_t i = 0; i < doc_terms.size; ++i) {
                const TermPos tp = doc_terms.data[i];

                std::uint32_t* pid = term_id.find(tp.term);
                std::uint32_t idv = 0;

                if (pid) {
                    idv = *pid;
                } else {
                    nostl::StrView stored = term_pool.add_copy(tp.term.data, tp.term.size, true);
                    if (!stored.data) throw std::runtime_error("oom");

                    bool inserted = false;
                    std::uint32_t* pv = term_id.get_or_insert(stored, &inserted);
                    if (!pv) throw std::runtime_error("oom");

                    idv = static_cast<std::uint32_t>(terms_raw.size);
                    *pv = idv;

                    if (!terms_raw.push_back(stored)) throw std::runtime_error("oom");
                }

                Occ o;
                o.term = idv;
                o.doc = doc_id;
                o.pos = tp.pos;
                if (!occ.push_back(o)) throw std::runtime_error("oom");
            }

            ++doc_id;
            if (limit_docs >= 0 && static_cast<std::int64_t>(doc_id) >= limit_docs) break;
        }

        const std::uint32_t docs = doc_id;

        nostl::Vec<TermItem> items;
        items.resize(terms_raw.size);
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(terms_raw.size); ++i) {
            items.data[i].term = terms_raw.data[i];
            items.data[i].old_id = i;
        }

        sort_term_items(items);

        nostl::Vec<std::uint32_t> remap;
        if (!remap.resize(terms_raw.size)) throw std::runtime_error("oom");
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(items.size); ++i) {
            remap.data[items.data[i].old_id] = i;
        }

        for (std::size_t i = 0; i < occ.size; ++i) {
            occ.data[i].term = remap.data[occ.data[i].term];
        }

        sort_occ(occ);

        const std::uint32_t terms = static_cast<std::uint32_t>(items.size);

        std::uint64_t term_pool_bytes = 0;
        for (std::uint32_t i = 0; i < terms; ++i) term_pool_bytes += items.data[i].term.size;

        double avg_term_len = terms ? (static_cast<double>(term_pool_bytes) / static_cast<double>(terms)) : 0.0;

        std::printf("docs=%u\n", docs);
        std::printf("terms=%u\n", terms);
        std::printf("avg_term_len_bytes=%.6f\n", avg_term_len);

        {
            std::string fwd_path = std::string(out_c) + "/index.fwd";
            std::ofstream out(fwd_path, std::ios::binary);
            if (!out) throw std::runtime_error("cannot open " + fwd_path);

            const std::uint32_t ver = 1;
            const std::uint64_t header_size = 4 + 4 + 4 + 8;
            const std::uint64_t rec_size = 8 + 4 + 8 + 4;
            const std::uint64_t pool_off = header_size + static_cast<std::uint64_t>(docs) * rec_size;

            write_bytes(out, "FWD1", 4);
            write_u32(out, ver);
            write_u32(out, docs);
            write_u64(out, pool_off);

            for (std::uint32_t i = 0; i < docs; ++i) {
                const DocRec& r = fwd.data[i];
                write_u64(out, r.url_off);
                write_u32(out, r.url_len);
                write_u64(out, r.title_off);
                write_u32(out, r.title_len);
            }

            if (fwd_pool.size) write_bytes(out, fwd_pool.data, fwd_pool.size);
            std::printf("wrote=%s\n", fwd_path.c_str());
        }

        nostl::Vec<std::uint64_t> post_off;
        nostl::Vec<std::uint32_t> post_len;
        if (!post_off.resize(terms)) throw std::runtime_error("oom");
        if (!post_len.resize(terms)) throw std::runtime_error("oom");
        for (std::uint32_t i = 0; i < terms; ++i) { post_off.data[i] = 0; post_len.data[i] = 0; }

        nostl::Vec<std::uint32_t> postings;

        std::uint32_t cur_term = 0;
        std::size_t i = 0;
        while (i < occ.size) {
            const std::uint32_t term = occ.data[i].term;

            while (cur_term < term) {
                post_off.data[cur_term] = static_cast<std::uint64_t>(postings.size);
                post_len.data[cur_term] = 0;
                ++cur_term;
            }

            const std::uint64_t start = static_cast<std::uint64_t>(postings.size);
            post_off.data[term] = start;

            while (i < occ.size && occ.data[i].term == term) {
                const std::uint32_t doc = occ.data[i].doc;
                std::size_t j = i;
                while (j < occ.size && occ.data[j].term == term && occ.data[j].doc == doc) ++j;

                if (!postings.push_back(doc)) throw std::runtime_error("oom");

                const std::uint32_t tf = static_cast<std::uint32_t>(j - i);
                if (!postings.push_back(tf)) throw std::runtime_error("oom");

                for (std::size_t k = i; k < j; ++k) {
                    const std::uint32_t pos = occ.data[k].pos;
                    if (!postings.push_back(pos)) throw std::runtime_error("oom");
                }

                i = j;
            }

            const std::uint64_t end = static_cast<std::uint64_t>(postings.size);
            post_len.data[term] = static_cast<std::uint32_t>(end - start);
            if (cur_term == term) ++cur_term;
        }

        while (cur_term < terms) {
            post_off.data[cur_term] = static_cast<std::uint64_t>(postings.size);
            post_len.data[cur_term] = 0;
            ++cur_term;
        }

        {
            std::string inv_path = std::string(out_c) + "/index.inv";
            std::ofstream out(inv_path, std::ios::binary);
            if (!out) throw std::runtime_error("cannot open " + inv_path);

            const std::uint32_t ver = 1;
            const std::uint64_t header_size = 4 + 4 + 4 + 4 + 8 + 8 + 8;
            const std::uint64_t entry_size = 8 + 4 + 8 + 4;
            const std::uint64_t dict_off = header_size;
            const std::uint64_t term_pool_off = dict_off + static_cast<std::uint64_t>(terms) * entry_size;
            const std::uint64_t postings_off = term_pool_off + term_pool_bytes;

            write_bytes(out, "INV1", 4);
            write_u32(out, ver);
            write_u32(out, docs);
            write_u32(out, terms);
            write_u64(out, dict_off);
            write_u64(out, term_pool_off);
            write_u64(out, postings_off);

            std::uint64_t cur_term_off = 0;
            for (std::uint32_t t = 0; t < terms; ++t) {
                const nostl::StrView tv = items.data[t].term;
                const std::uint64_t t_off = cur_term_off;
                const std::uint32_t t_len = static_cast<std::uint32_t>(tv.size);
                cur_term_off += t_len;

                write_u64(out, t_off);
                write_u32(out, t_len);
                write_u64(out, post_off.data[t]);
                write_u32(out, post_len.data[t]);
            }

            for (std::uint32_t t = 0; t < terms; ++t) {
                const nostl::StrView tv = items.data[t].term;
                if (tv.size) write_bytes(out, tv.data, tv.size);
            }

            if (postings.size) {
                write_bytes(out, postings.data, postings.size * sizeof(std::uint32_t));
            }

            std::printf("wrote=%s\n", inv_path.c_str());
        }

        std::printf("postings_uint32=%zu\n", postings.size);
        std::printf("postings_bytes=%zu\n", postings.size * sizeof(std::uint32_t));

        auto t1 = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::printf("time_ms=%lld\n", (long long)ms);

        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}