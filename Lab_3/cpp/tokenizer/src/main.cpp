#include <algorithm>
#include <chrono>
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

static bool utf8_decode_one(const std::string& s, std::size_t& i, char32_t& cp) {
  if (i >= s.size()) return false;
  unsigned char c0 = static_cast<unsigned char>(s[i]);

  if (c0 < 0x80) {
    cp = c0;
    i += 1;
    return true;
  }

  if ((c0 >> 5) == 0x6) {
    if (i + 1 >= s.size()) return false;
    unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
    if ((c1 & 0xC0) != 0x80) return false;
    cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
    i += 2;
    return true;
  }

  if ((c0 >> 4) == 0xE) {
    if (i + 2 >= s.size()) return false;
    unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
    unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
    if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return false;
    cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
    i += 3;
    return true;
  }

  if ((c0 >> 3) == 0x1E) {
    if (i + 3 >= s.size()) return false;
    unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
    unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
    unsigned char c3 = static_cast<unsigned char>(s[i + 3]);
    if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
    cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    i += 4;
    return true;
  }

  return false;
}

static void utf8_encode_one(std::string& out, char32_t cp) {
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
    return;
  }
  if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    return;
  }
  if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    return;
  }
  out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
  out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
  out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
  out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
}

static bool is_cyrillic(char32_t cp) {
  return (cp >= 0x410 && cp <= 0x44F) || cp == 0x401 || cp == 0x451;
}

static bool is_latin(char32_t cp) {
  return (cp >= U'A' && cp <= U'Z') || (cp >= U'a' && cp <= U'z');
}

static bool is_digit(char32_t cp) {
  return (cp >= U'0' && cp <= U'9');
}

static bool is_letter(char32_t cp) {
  return is_cyrillic(cp) || is_latin(cp);
}

static bool is_hyphen(char32_t cp) {
  return cp == U'-' || cp == 0x2010 || cp == 0x2011 || cp == 0x2012 || cp == 0x2013 || cp == 0x2014;
}

static char32_t to_lower_cp(char32_t cp) {
  if (cp >= U'A' && cp <= U'Z') return cp + 32;
  if (cp >= 0x410 && cp <= 0x42F) return cp + 0x20;
  if (cp == 0x401) return 0x451;
  return cp;
}

static char32_t yo_to_e(char32_t cp) {
  if (cp == 0x451) return 0x435;
  if (cp == 0x401) return 0x415;
  return cp;
}

static std::u32string to_u32(const std::string& s) {
  std::u32string out;
  std::size_t i = 0;
  while (i < s.size()) {
    char32_t cp = 0;
    std::size_t j = i;
    if (!utf8_decode_one(s, j, cp)) {
      i += 1;
      continue;
    }
    out.push_back(cp);
    i = j;
  }
  return out;
}

static std::string from_u32(const std::u32string& s) {
  std::string out;
  out.reserve(s.size() * 2);
  for (char32_t cp : s) utf8_encode_one(out, cp);
  return out;
}

static bool ends_with(const std::u32string& s, const std::u32string& suf) {
  if (s.size() < suf.size()) return false;
  return std::equal(suf.rbegin(), suf.rend(), s.rbegin());
}

static std::string stem_ru_suffix(const std::string& token_utf8) {
  std::u32string s = to_u32(token_utf8);
  if (s.size() < 4) return token_utf8;

  static const std::vector<std::u32string> suffixes = {
      U"иями", U"ями", U"ами", U"ыми", U"ими",
      U"ого", U"ему", U"ому", U"ее", U"ие", U"ые", U"ое",
      U"ей", U"ий", U"ый", U"ая", U"яя", U"ою", U"ею", U"ую", U"юю",
      U"ам", U"ям", U"ах", U"ях", U"ом", U"ем",
      U"а", U"я", U"ы", U"и", U"о", U"е", U"у", U"ю", U"ь"
  };

  for (const auto& suf : suffixes) {
    if (s.size() > suf.size() + 2 && ends_with(s, suf)) {
      s.resize(s.size() - suf.size());
      return from_u32(s);
    }
  }
  return token_utf8;
}

static std::string csv_escape(const std::string& s) {
  bool need = false;
  for (char c : s) {
    if (c == ',' || c == '"' || c == '\n' || c == '\r') {
      need = true;
      break;
    }
  }
  if (!need) return s;
  std::string out = "\"";
  for (char c : s) {
    if (c == '"') out += "\"\"";
    else out.push_back(c);
  }
  out.push_back('"');
  return out;
}

struct TokenizeOptions {
  bool keep_numbers = false;
  bool stem = false;
  std::size_t min_len = 1;
};

static void feed_token(std::string& token, std::size_t token_len,
                       const TokenizeOptions& opt,
                       std::uint64_t& tokens_total,
                       std::uint64_t& token_chars_total,
                       std::unordered_map<std::string, std::uint32_t>& tf) {
  if (token_len == 0) return;
  if (token_len < opt.min_len) {
    token.clear();
    return;
  }

  std::string t = token;
  if (opt.stem) t = stem_ru_suffix(t);

  tf[t] += 1;
  tokens_total += 1;
  token_chars_total += token_len;
  token.clear();
}

enum class Script { NONE, LAT, CYR };

static Script script_of_letter(char32_t cp) {
  if (is_cyrillic(cp)) return Script::CYR;
  if (is_latin(cp)) return Script::LAT;
  return Script::NONE;
}

static void tokenize_text(const std::string& text, const TokenizeOptions& opt,
                          std::uint64_t& tokens_total,
                          std::uint64_t& token_chars_total,
                          std::unordered_map<std::string, std::uint32_t>& tf) {
  std::string token;
  token.reserve(32);
  std::size_t token_len = 0;
  Script cur_script = Script::NONE;

  auto flush = [&]() {
    feed_token(token, token_len, opt, tokens_total, token_chars_total, tf);
    token_len = 0;
    cur_script = Script::NONE;
  };

  std::size_t i = 0;
  while (i < text.size()) {
    char32_t cp = 0;
    std::size_t j = i;

    if (!utf8_decode_one(text, j, cp)) {
      flush();
      i += 1;
      continue;
    }

    cp = yo_to_e(to_lower_cp(cp));

    bool is_num = opt.keep_numbers && is_digit(cp);
    bool is_let = is_letter(cp);
    bool word = is_let || is_num;

    if (word) {
      if (is_let) {
        Script s = script_of_letter(cp);

        if (!token.empty() && cur_script != Script::NONE && s != Script::NONE && s != cur_script) {
          flush();
        }

        if (token.empty()) cur_script = s;
      }

      utf8_encode_one(token, cp);
      token_len += 1;
      i = j;
      continue;
    }

    if (is_hyphen(cp) && token_len > 0) {
      std::size_t k = j;
      char32_t next = 0;

      if (utf8_decode_one(text, k, next)) {
        next = yo_to_e(to_lower_cp(next));
        bool next_num = opt.keep_numbers && is_digit(next);
        bool next_let = is_letter(next);
        bool next_word = next_let || next_num;

        if (next_word) {
          if (next_let) {
            Script ns = script_of_letter(next);
            if (cur_script != Script::NONE && ns != Script::NONE && ns != cur_script) {
              flush();
              i = j;
              continue;
            }
          }

          token.push_back('-');
          token_len += 1;
          i = j;
          continue;
        }
      }
    }

    flush();
    i = j;
  }

  flush();
}

#ifndef TOKENIZER_NO_MAIN
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

    std::unordered_map<std::string, std::uint32_t> tf;
    tf.reserve(1 << 20);

    std::uint64_t docs = 0;
    std::uint64_t bytes_in = 0;
    std::uint64_t tokens_total = 0;
    std::uint64_t token_chars_total = 0;

    auto t0 = std::chrono::steady_clock::now();

    for (auto&& doc : col.find({}, fopts)) {
      auto it = doc.find("clean_text");
      if (it == doc.end() || it->type() != bsoncxx::type::k_string) continue;

      auto sv = it->get_string().value;
      std::string text(sv.data(), sv.size());

      docs += 1;
      bytes_in += text.size();

      tokenize_text(text, opt, tokens_total, token_chars_total, tf);

      if (limit_docs >= 0 && static_cast<std::int64_t>(docs) >= limit_docs) break;
      if (limit_bytes >= 0 && static_cast<std::int64_t>(bytes_in) >= limit_bytes) break;
    }

    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    double avg_len = tokens_total ? (static_cast<double>(token_chars_total) / static_cast<double>(tokens_total)) : 0.0;
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

    std::string tf_path = out_dir + "/term_freq.csv";
    std::ofstream tf_out(tf_path, std::ios::binary);
    tf_out << "term,freq\n";
    for (const auto& kv : tf) {
      tf_out << csv_escape(kv.first) << "," << kv.second << "\n";
    }
    tf_out.close();

    std::vector<std::pair<std::string, std::uint32_t>> vec;
    vec.reserve(tf.size());
    for (const auto& kv : tf) vec.push_back(kv);

    std::sort(vec.begin(), vec.end(),
              [](const auto& a, const auto& b) {
                if (a.second != b.second) return a.second > b.second;
                return a.first < b.first;
              });

    std::uint32_t f1 = vec.empty() ? 0u : vec[0].second;

    std::size_t top_n = static_cast<std::size_t>(
    std::max<std::int64_t>(1, get_arg_i64(argc, argv, "--top-n", 50))
    );
    if (top_n > vec.size()) top_n = vec.size();

    std::string top_path = out_dir + "/top_terms.csv";
    std::ofstream top_out(top_path, std::ios::binary);
    top_out << "rank,term,freq\n";
    for (std::size_t r = 0; r < top_n; r++) {
      top_out << (r + 1) << "," << csv_escape(vec[r].first) << "," << vec[r].second << "\n";
    }
    top_out.close();

    std::cout << "top_terms_file=" << top_path << "\n";
    std::cout << "top_" << top_n << ":\n";
    for (std::size_t r = 0; r < std::min<std::size_t>(top_n, 10); r++) {
      std::cout << (r + 1) << ") " << vec[r].first << " : " << vec[r].second << "\n";
    }

    std::string zipf_path = out_dir + "/zipf.csv";
    std::ofstream z_out(zipf_path, std::ios::binary);
    z_out << "rank,freq,zipf_pred\n";
    for (std::size_t r = 0; r < vec.size(); r++) {
      std::size_t rank = r + 1;
      double pred = (rank > 0) ? (static_cast<double>(f1) / static_cast<double>(rank)) : 0.0;
      z_out << rank << "," << vec[r].second << "," << pred << "\n";
    }
    z_out.close();

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
#endif