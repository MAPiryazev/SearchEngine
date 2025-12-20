#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/uri.hpp>

#include "search/tokenizer_stl.hpp"
#include "search/nostl/hashmap.hpp"
#include "search/nostl/strpool.hpp"
#include "search/nostl/strview.hpp"

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

static int mkdir_p(const char* path) {
    if (!path || !*path) return 0;

    char buf[4096];
    std::size_t len = std::strlen(path);
    if (len >= sizeof(buf)) return -1;

    std::memcpy(buf, path, len + 1);

    if (buf[len - 1] == '/') buf[len - 1] = 0;

    for (char* p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = 0;
            if (::mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (::mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static std::uint64_t now_ms() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000ull +
           static_cast<std::uint64_t>(ts.tv_nsec) / 1000000ull;
}

int main(int argc, char** argv) {
    try {
        const char* uri_c = get_arg(argc, argv, "--uri", "mongodb://localhost:27017/");
        const char* db_c  = get_arg(argc, argv, "--db", "search_engine_clean");
        const char* col_c = get_arg(argc, argv, "--col", "documents");
        const char* out_c = get_arg(argc, argv, "--out", "out");

        std::int64_t limit_docs  = get_arg_i64(argc, argv, "--limit-docs", -1);
        std::int64_t limit_bytes = get_arg_i64(argc, argv, "--limit-bytes", -1);

        TokenizeOptions opt;
        opt.keep_numbers = (std::strcmp(get_arg(argc, argv, "--keep-numbers", "0"), "0") != 0);
        opt.stem         = (std::strcmp(get_arg(argc, argv, "--stem", "0"), "0") != 0);

        std::int64_t ml = get_arg_i64(argc, argv, "--min-len", 1);
        if (ml < 1) ml = 1;
        opt.min_len = static_cast<std::size_t>(ml);

        if (mkdir_p(out_c) != 0) {
            std::fprintf(stderr, "Error: cannot create dir: %s\n", out_c);
            return 1;
        }

        mongocxx::instance inst{};
        mongocxx::client client{mongocxx::uri{std::string(uri_c)}};
        auto col = client[std::string(db_c)][std::string(col_c)];

        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;

        mongocxx::options::find fopts{};
        fopts.projection(make_document(kvp("clean_text", 1)));
        fopts.batch_size(512);

        std::uint64_t docs = 0;
        std::uint64_t bytes_in = 0;
        std::uint64_t tokens_total = 0;
        std::uint64_t token_chars_total = 0;

        nostl::HashMapSV<std::uint32_t> tf;
        tf.init(1 << 20);

        nostl::StrPool pool;
        pool.reserve(1 << 20);

        const std::uint64_t t0 = now_ms();

        for (auto&& doc : col.find({}, fopts)) {
            auto it = doc.find("clean_text");
            if (it == doc.end() || it->type() != bsoncxx::type::k_string) continue;

            auto sv = it->get_string().value;
            nostl::StrView textv(sv.data(), static_cast<std::size_t>(sv.size()));

            docs += 1;
            bytes_in += static_cast<std::uint64_t>(textv.size);

            tokenize_text_tf_nostl(textv, opt, tokens_total, token_chars_total, pool, tf);

            if (limit_docs >= 0 && static_cast<std::int64_t>(docs) >= limit_docs) break;
            if (limit_bytes >= 0 && static_cast<std::int64_t>(bytes_in) >= limit_bytes) break;
        }

        const std::uint64_t t1 = now_ms();
        const std::uint64_t ms = (t1 >= t0) ? (t1 - t0) : 0;

        const double avg_len = tokens_total
            ? (static_cast<double>(token_chars_total) / static_cast<double>(tokens_total))
            : 0.0;

        const double kb = static_cast<double>(bytes_in) / 1024.0;
        const double sec = static_cast<double>(ms) / 1000.0;
        const double kbps = (sec > 0.0) ? (kb / sec) : 0.0;

        std::printf("docs=%llu\n", (unsigned long long)docs);
        std::printf("bytes_in=%llu\n", (unsigned long long)bytes_in);
        std::printf("tokens=%llu\n", (unsigned long long)tokens_total);
        std::printf("avg_token_len=%.6f\n", avg_len);
        std::printf("time_ms=%llu\n", (unsigned long long)ms);
        std::printf("KB_per_s=%.6f\n", kbps);
        std::printf("unique_terms=%llu\n", (unsigned long long)tf.size);

        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
