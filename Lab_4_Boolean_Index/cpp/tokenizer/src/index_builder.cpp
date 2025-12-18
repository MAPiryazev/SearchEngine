#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
  for (std::size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 2 < s.size()) {
      int hi = hex_val(s[i + 1]);
      int lo = hex_val(s[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    if (s[i] == '_') {
      out.push_back(' ');
      continue;
    }
    out.push_back(s[i]);
  }
  return out;
}

static std::string wiki_title_from_url(const std::string& url) {
  const std::string key = "/wiki/";
  auto pos = url.find(key);
  if (pos == std::string::npos) return "";
  std::string tail = url.substr(pos + key.size());
  auto q = tail.find_first_of("?#");
  if (q != std::string::npos) tail.resize(q);
  return url_decode(tail);
}

struct DocInfo {
  std::string url;
  std::string title;
};

struct TermEntry {
  std::string term;
  std::vector<std::uint32_t> postings;
};

static double avg_term_len_bytes(const std::vector<TermEntry>& dict) {
  if (dict.empty()) return 0.0;
  std::uint64_t sum = 0;
  for (const auto& e : dict) sum += static_cast<std::uint64_t>(e.term.size());
  return static_cast<double>(sum) / static_cast<double>(dict.size());
}

int main(int argc, char** argv) {
  try {
    std::string uri = get_arg(argc, argv, "--uri", "mongodb://localhost:27017/");
    std::string db_name = get_arg(argc, argv, "--db", "search_engine_clean");
    std::string col_name = get_arg(argc, argv, "--col", "documents");
    std::string out_dir = get_arg(argc, argv, "--out", "out");

    std::int64_t limit_docs = get_arg_i64(argc, argv, "--limit-docs", -1);

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
    fopts.projection(make_document(kvp("clean_text", 1), kvp("url", 1), kvp("title", 1)));
    fopts.batch_size(512);

    std::vector<DocInfo> forward;
    forward.reserve(100000);

    std::unordered_map<std::string, std::vector<std::uint32_t>> inv;
    inv.reserve(1 << 20);

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

      auto sv = it_text->get_string().value;
      std::string text(sv.data(), sv.size());

      forward.push_back({url, title});

      std::vector<std::string> terms;
      terms.reserve(256);
      tokenize_doc_terms(text, opt, terms);

      std::sort(terms.begin(), terms.end());
      terms.erase(std::unique(terms.begin(), terms.end()), terms.end());

      for (const auto& term : terms) {
        inv[term].push_back(doc_id);
      }

      doc_id += 1;
      if (limit_docs >= 0 && static_cast<std::int64_t>(doc_id) >= limit_docs) break;
    }

    std::vector<TermEntry> dict;
    dict.reserve(inv.size());
    for (auto& kv : inv) {
      auto& postings = kv.second;
      std::sort(postings.begin(), postings.end());
      postings.erase(std::unique(postings.begin(), postings.end()), postings.end());
      dict.push_back(TermEntry{std::move(kv.first), std::move(postings)});
    }
    inv.clear();

    std::sort(dict.begin(), dict.end(), [](const TermEntry& a, const TermEntry& b) {
      return a.term < b.term;
    });

    std::cout << "docs=" << forward.size() << "\n";
    std::cout << "terms=" << dict.size() << "\n";
    std::cout << "avg_term_len_bytes=" << avg_term_len_bytes(dict) << "\n";

    // -------- index.fwd --------
    {
      std::string fwd_path = out_dir + "/index.fwd";
      std::ofstream out(fwd_path, std::ios::binary);
      if (!out) throw std::runtime_error("cannot open " + fwd_path);

      const std::uint32_t ver = 1;
      const std::uint32_t docs = static_cast<std::uint32_t>(forward.size());

      const std::uint64_t header_size = 4 + 4 + 4 + 8;
      const std::uint64_t rec_size = 8 + 4 + 8 + 4;
      const std::uint64_t pool_off = header_size + static_cast<std::uint64_t>(docs) * rec_size;

      write_bytes(out, "FWD1", 4);
      write_u32(out, ver);
      write_u32(out, docs);
      write_u64(out, pool_off);

      std::uint64_t cur_off = 0;
      for (std::uint32_t i = 0; i < docs; i++) {
        const auto& di = forward[i];

        std::uint64_t url_off = cur_off;
        std::uint32_t url_len = static_cast<std::uint32_t>(di.url.size());
        cur_off += url_len;

        std::uint64_t title_off = cur_off;
        std::uint32_t title_len = static_cast<std::uint32_t>(di.title.size());
        cur_off += title_len;

        write_u64(out, url_off);
        write_u32(out, url_len);
        write_u64(out, title_off);
        write_u32(out, title_len);
      }

      for (std::uint32_t i = 0; i < docs; i++) {
        const auto& di = forward[i];
        if (!di.url.empty()) write_bytes(out, di.url.data(), di.url.size());
        if (!di.title.empty()) write_bytes(out, di.title.data(), di.title.size());
      }

      std::cout << "wrote=" << fwd_path << "\n";
    }

    // -------- index.inv --------
    {
      std::string inv_path = out_dir + "/index.inv";
      std::ofstream out(inv_path, std::ios::binary);
      if (!out) throw std::runtime_error("cannot open " + inv_path);

      const std::uint32_t ver = 1;
      const std::uint32_t docs = static_cast<std::uint32_t>(forward.size());
      const std::uint32_t terms = static_cast<std::uint32_t>(dict.size());

      const std::uint64_t header_size = 4 + 4 + 4 + 4 + 8 + 8 + 8;
      const std::uint64_t entry_size = 8 + 4 + 8 + 4;

      const std::uint64_t dict_off = header_size;
      const std::uint64_t term_pool_off = dict_off + static_cast<std::uint64_t>(terms) * entry_size;

      std::uint64_t term_pool_bytes = 0;
      for (const auto& e : dict) term_pool_bytes += static_cast<std::uint64_t>(e.term.size());

      const std::uint64_t postings_off = term_pool_off + term_pool_bytes;

      write_bytes(out, "INV1", 4);
      write_u32(out, ver);
      write_u32(out, docs);
      write_u32(out, terms);
      write_u64(out, dict_off);
      write_u64(out, term_pool_off);
      write_u64(out, postings_off);

      std::uint64_t cur_term_off = 0;
      std::uint64_t cur_post_off = 0;

      for (const auto& e : dict) {
        std::uint64_t t_off = cur_term_off;
        std::uint32_t t_len = static_cast<std::uint32_t>(e.term.size());
        cur_term_off += t_len;

        std::uint64_t p_off = cur_post_off;
        std::uint32_t p_len = static_cast<std::uint32_t>(e.postings.size());
        cur_post_off += static_cast<std::uint64_t>(p_len) * sizeof(std::uint32_t);

        write_u64(out, t_off);
        write_u32(out, t_len);
        write_u64(out, p_off);
        write_u32(out, p_len);
      }

      for (const auto& e : dict) {
        if (!e.term.empty()) write_bytes(out, e.term.data(), e.term.size());
      }

      for (const auto& e : dict) {
        if (!e.postings.empty()) {
          write_bytes(out, e.postings.data(), e.postings.size() * sizeof(std::uint32_t));
        }
      }

      std::cout << "wrote=" << inv_path << "\n";
    }

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
