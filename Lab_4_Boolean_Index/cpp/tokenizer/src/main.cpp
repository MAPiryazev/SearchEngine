#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/uri.hpp>

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

static void ensure_dir(const std::string& path) {
  std::filesystem::create_directories(path);
}

int main(int argc, char** argv) {
  try {
    std::string uri = get_arg(argc, argv, "--uri", "mongodb://localhost:27017/");
    std::string db_name = get_arg(argc, argv, "--db", "search_engine_clean");
    std::string col_name = get_arg(argc, argv, "--col", "documents");
    std::string out_dir = get_arg(argc, argv, "--out", "out");

    std::int64_t limit_docs = get_arg_i64(argc, argv, "--limit-docs", -1);
    std::int64_t limit_bytes = get_arg_i64(argc, argv, "--limit-bytes", -1);

    TokenizeOptions opt;
    opt.keep_numbers = (get_arg(argc, argv, "--keep-numbers", "0") != "0");
    opt.stem = (get_arg(argc, argv, "--stem", "0") != "0");
    opt.min_len = static_cast<std::size_t>(std::max<std::int64_t>(1, get_arg_i64(argc, argv, "--min-len", 1)));

    ensure_dir(out_dir);

    mongocxx::instance inst{};
    mongocxx::client client{mongocxx::uri{uri}};
    auto col = client[db_name][col_name];

    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;

    mongocxx::options::find fopts{};
    fopts.projection(make_document(kvp("clean_text", 1)));
    fopts.batch_size(512);

    std::uint64_t docs = 0;
    std::uint64_t bytes_in = 0;
    std::uint64_t tokens_total = 0;
    std::uint64_t token_chars_total = 0;

    std::unordered_map<std::string, std::uint32_t> tf;
    tf.reserve(1 << 20);

    auto t0 = std::chrono::steady_clock::now();

    for (auto&& doc : col.find({}, fopts)) {
      auto it = doc.find("clean_text");
      if (it == doc.end() || it->type() != bsoncxx::type::k_string) continue;

      auto sv = it->get_string().value;
      std::string text(sv.data(), sv.size());

      docs += 1;
      bytes_in += text.size();

      tokenize_text_tf(text, opt, tokens_total, token_chars_total, tf);

      if (limit_docs >= 0 && static_cast<std::int64_t>(docs) >= limit_docs) break;
      if (limit_bytes >= 0 && static_cast<std::int64_t>(bytes_in) >= limit_bytes) break;
    }

    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    double avg_len = tokens_total
      ? (static_cast<double>(token_chars_total) / static_cast<double>(tokens_total))
      : 0.0;

    double kb = static_cast<double>(bytes_in) / 1024.0;
    double sec = static_cast<double>(ms) / 1000.0;
    double kbps = (sec > 0.0) ? (kb / sec) : 0.0;

    std::cout << "docs=" << docs << "\n";
    std::cout << "bytes_in=" << bytes_in << "\n";
    std::cout << "tokens=" << tokens_total << "\n";
    std::cout << "avg_token_len=" << avg_len << "\n";
    std::cout << "time_ms=" << ms << "\n";
    std::cout << "KB_per_s=" << kbps << "\n";
    std::cout << "unique_terms=" << tf.size() << "\n";

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
