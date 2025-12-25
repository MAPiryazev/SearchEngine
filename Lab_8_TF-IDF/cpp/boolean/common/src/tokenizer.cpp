#include "search/tokenizer_stl.hpp"
#include "search/stemmer_data.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include "search/nostl/vec.hpp"
#include "search/nostl/strview.hpp"
#include "search/nostl/strpool.hpp"
#include <stdexcept>


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

static bool has_vowel(const std::u32string& s) {
  for (char32_t cp : s) {
    if (stemmer_data::RU_VOWELS.find(cp) != std::u32string::npos) return true;
  }
  return false;
}

static bool is_cyrillic_token(const std::string& token) {
  std::u32string s = to_u32(token);
  for (char32_t cp : s) {
    if (is_cyrillic(cp)) return true;
  }
  return false;
}

static bool is_latin_token(const std::string& token) {
  for (char c : token) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return true;
  }
  return false;
}

// Улучшенный стемминг для русского языка
static std::string stem_ru_advanced(const std::string& token_utf8) {
  std::u32string s = to_u32(token_utf8);
  if (s.size() < 3) return token_utf8;
  
  // Минимальная длина основы после стемминга
  const std::size_t min_stem_len = 2;
  
  for (const auto& suf : stemmer_data::RU_SUFFIXES) {
    if (s.size() > suf.size() + min_stem_len && ends_with(s, suf)) {
      std::u32string stem = s.substr(0, s.size() - suf.size());
      // Проверяем что в основе есть гласная
      if (has_vowel(stem)) {
        return from_u32(stem);
      }
    }
  }
  
  // Удаление мягкого знака в конце
  if (s.size() > min_stem_len && s.back() == U'ь') {
    std::u32string stem = s.substr(0, s.size() - 1);
    if (has_vowel(stem)) {
      return from_u32(stem);
    }
  }
  
  return token_utf8;
}

// Упрощенный Porter stemmer для английского
static bool is_vowel(char c) {
  return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
}

static bool has_vowel_en(const std::string& s) {
  for (char c : s) {
    if (is_vowel(c)) return true;
  }
  return false;
}

static bool ends_with_en(const std::string& s, const std::string& suf) {
  if (s.size() < suf.size()) return false;
  return s.substr(s.size() - suf.size()) == suf;
}

static std::string stem_en_porter(const std::string& word) {
  if (word.size() < 3) return word;
  
  std::string s = word;
  
  // Step 1a: множественное число и прошедшее время
  if (ends_with_en(s, "sses") || ends_with_en(s, "ies")) {
    s = s.substr(0, s.size() - 2);
  } else if (ends_with_en(s, "ss")) {
    // ничего не делаем
  } else if (ends_with_en(s, "s") && s.size() > 3) {
    s = s.substr(0, s.size() - 1);
  }
  
  // Step 1b: глагольные формы
  if (ends_with_en(s, "eed")) {
    if (s.size() > 3) s = s.substr(0, s.size() - 1);
  } else if (ends_with_en(s, "ed")) {
    std::string stem = s.substr(0, s.size() - 2);
    if (has_vowel_en(stem)) {
      s = stem;
      if (ends_with_en(s, "at") || ends_with_en(s, "bl") || ends_with_en(s, "iz")) {
        s += "e";
      } else if (s.size() >= 2 && s[s.size()-1] == s[s.size()-2] && 
                 !is_vowel(s[s.size()-1]) && s[s.size()-1] != 'l' && s[s.size()-1] != 's' && s[s.size()-1] != 'z') {
        s = s.substr(0, s.size() - 1);
      } else if (s.size() == 3 && !has_vowel_en(s.substr(0, 1)) && has_vowel_en(s.substr(1, 1)) && 
                 !has_vowel_en(s.substr(2, 1))) {
        s += "e";
      }
    }
  } else if (ends_with_en(s, "ing")) {
    std::string stem = s.substr(0, s.size() - 3);
    if (has_vowel_en(stem)) {
      s = stem;
      if (ends_with_en(s, "at") || ends_with_en(s, "bl") || ends_with_en(s, "iz")) {
        s += "e";
      } else if (s.size() >= 2 && s[s.size()-1] == s[s.size()-2] && 
                 !is_vowel(s[s.size()-1]) && s[s.size()-1] != 'l' && s[s.size()-1] != 's' && s[s.size()-1] != 'z') {
        s = s.substr(0, s.size() - 1);
      } else if (s.size() == 3 && !has_vowel_en(s.substr(0, 1)) && has_vowel_en(s.substr(1, 1)) && 
                 !has_vowel_en(s.substr(2, 1))) {
        s += "e";
      }
    }
  }
  
  // Step 1c: замена y на i
  if (ends_with_en(s, "y") && s.size() > 2) {
    std::string stem = s.substr(0, s.size() - 1);
    if (!has_vowel_en(stem)) {
      s = stem + "i";
    }
  }
  
  // Step 2: суффиксы
  for (const auto& p : stemmer_data::EN_STEP2) {
    if (ends_with_en(s, p.first) && s.size() > p.first.size() + 2) {
      std::string stem = s.substr(0, s.size() - p.first.size());
      if (has_vowel_en(stem)) {
        s = stem + p.second;
        break;
      }
    }
  }
  
  // Step 3: дополнительные суффиксы
  for (const auto& p : stemmer_data::EN_STEP3) {
    if (ends_with_en(s, p.first) && s.size() > p.first.size() + 2) {
      std::string stem = s.substr(0, s.size() - p.first.size());
      if (has_vowel_en(stem)) {
        s = stem + p.second;
        break;
      }
    }
  }
  
  // Step 4: финальные суффиксы
  for (const auto& suf : stemmer_data::EN_STEP4) {
    if (ends_with_en(s, suf) && s.size() > suf.size() + 2) {
      std::string stem = s.substr(0, s.size() - suf.size());
      if (has_vowel_en(stem)) {
        // Особый случай для -ion: должно быть -sion или -tion
        if (suf == "ion" && (ends_with_en(stem, "s") || ends_with_en(stem, "t"))) {
          s = stem;
        } else if (suf != "ion") {
          s = stem;
        }
        break;
      }
    }
  }
  
  // Step 5a и 5b: финальная очистка
  if (ends_with_en(s, "e") && s.size() > 3) {
    std::string stem = s.substr(0, s.size() - 1);
    if (has_vowel_en(stem) && !(stem.size() >= 2 && stem[stem.size()-1] == 'l' && 
                                 !is_vowel(stem[stem.size()-2]))) {
      s = stem;
    }
  }
  
  if (ends_with_en(s, "ll") && s.size() > 3 && has_vowel_en(s.substr(0, s.size()-1))) {
    s = s.substr(0, s.size() - 1);
  }
  
  return s;
}

// Главная функция стемминга - определяет язык и применяет соответствующий алгоритм
static std::string stem_token(const std::string& token) {
  if (token.size() < 3) return token;
  
  bool has_cyr = is_cyrillic_token(token);
  bool has_lat = is_latin_token(token);
  
  // Если есть кириллица - русский стемминг
  if (has_cyr) {
    return stem_ru_advanced(token);
  }
  // Если только латиница - английский стемминг
  else if (has_lat) {
    return stem_en_porter(token);
  }
  
  // Иначе возвращаем как есть
  return token;
}

enum class Script { NONE, LAT, CYR };

static Script script_of_letter(char32_t cp) {
  if (is_cyrillic(cp)) return Script::CYR;
  if (is_latin(cp)) return Script::LAT;
  return Script::NONE;
}

static void feed_token_tf(std::string& token, std::size_t token_len,
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
  if (opt.stem) t = stem_token(t);

  tf[t] += 1;
  tokens_total += 1;
  token_chars_total += token_len;
  token.clear();
}

static void feed_token_terms(std::string& token, std::size_t token_len,
                             const TokenizeOptions& opt,
                             std::vector<std::string>& out_terms) {
  if (token_len == 0) return;
  if (token_len < opt.min_len) {
    token.clear();
    return;
  }

  if (opt.stem) token = stem_token(token);
  out_terms.push_back(token);
  token.clear();
}

template <class FeedFn>
static void tokenize_impl(const std::string& text, const TokenizeOptions& opt, FeedFn feed) {
  std::string token;
  token.reserve(32);
  std::size_t token_len = 0;
  Script cur_script = Script::NONE;

  auto flush = [&]() {
    feed(token, token_len);
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

void tokenize_text_tf(const std::string& text, const TokenizeOptions& opt,
                      std::uint64_t& tokens_total,
                      std::uint64_t& token_chars_total,
                      std::unordered_map<std::string, std::uint32_t>& tf) {
  auto feed = [&](std::string& t, std::size_t len) {
    feed_token_tf(t, len, opt, tokens_total, token_chars_total, tf);
  };
  tokenize_impl(text, opt, feed);
}

void tokenize_doc_terms(const std::string& text, const TokenizeOptions& opt,
                        std::vector<std::string>& out_terms) {
  auto feed = [&](std::string& t, std::size_t len) {
    feed_token_terms(t, len, opt, out_terms);
  };
  tokenize_impl(text, opt, feed);
}

void tokenize_doc_terms_pos_sv(nostl::StrView text,
                               const TokenizeOptions& opt,
                               nostl::StrPool& pool,
                               nostl::Vec<TermPos>& out_terms) {
    std::string s;
    if (text.data && text.size) s.assign(text.data, text.size);

    std::vector<std::string> tmp;
    tmp.reserve(256);
    tokenize_doc_terms(s, opt, tmp);

    out_terms.clear_keep();
    if (tmp.size()) out_terms.reserve(tmp.size());

    for (std::size_t i = 0; i < tmp.size(); ++i) {
        const std::string& t = tmp[i];
        nostl::StrView v = pool.add_copy(t.data(), t.size(), true);
        if (!v.data) throw std::runtime_error("oom");

        TermPos tp;
        tp.term = v;
        tp.pos = static_cast<std::uint32_t>(i);

        if (!out_terms.push_back(tp)) throw std::runtime_error("oom");
    }
}



void tokenize_doc_terms_sv(nostl::StrView text,
                           const TokenizeOptions& opt,
                           nostl::StrPool& pool,
                           nostl::Vec<nostl::StrView>& out_terms) {
    std::string s;
    if (text.data && text.size) s.assign(text.data, text.size);

    std::vector<std::string> tmp;
    tmp.reserve(64);
    tokenize_doc_terms(s, opt, tmp);

    out_terms.clear_keep();
    if (tmp.size()) out_terms.reserve(tmp.size());

    for (std::size_t i = 0; i < tmp.size(); ++i) {
        const std::string& t = tmp[i];
        nostl::StrView v = pool.add_copy(t.data(), t.size(), true);
        if (!v.data) throw std::runtime_error("oom");
        if (!out_terms.push_back(v)) throw std::runtime_error("oom");
    }
}

void tokenize_text_tf_nostl(nostl::StrView text, const TokenizeOptions& opt,
                            std::uint64_t& tokens_total,
                            std::uint64_t& token_chars_total,
                            nostl::StrPool& pool,
                            nostl::HashMapSV<std::uint32_t>& tf) {
    std::string s;
    if (text.data && text.size) s.assign(text.data, text.size);

    std::unordered_map<std::string, std::uint32_t> tmp;
    tmp.reserve(1 << 16);

    std::uint64_t dummy_tokens = 0;
    std::uint64_t dummy_chars = 0;
    tokenize_text_tf(s, opt, dummy_tokens, dummy_chars, tmp);

    tokens_total += dummy_tokens;
    token_chars_total += dummy_chars;

    for (const auto& kv : tmp) {
        const std::string& k = kv.first;
        const std::uint32_t add = kv.second;

        nostl::StrView probe(k.data(), k.size());
        std::uint32_t* pv = tf.find(probe);
        if (pv) {
            *pv += add;
        } else {
            nostl::StrView stored = pool.add_copy(k.data(), k.size(), true);
            if (!stored.data) throw std::runtime_error("oom");
            bool inserted = false;
            pv = tf.get_or_insert(stored, &inserted);
            if (!pv) throw std::runtime_error("oom");
            *pv = add;
        }
    }
}