#include "search/lab6/inv_index6.hpp"

#include <stdexcept>

#include "search/lab6/vbyte.hpp"


nostl::StrView InvertedIndex6::termview(std::size_t i) const {
    const auto& e = dict.data[i];
    return nostl::StrView(
        termpool.data + static_cast<std::size_t>(e.termoff),
        static_cast<std::size_t>(e.termlen));
}

bool InvertedIndex6::find_term(nostl::StrView term, std::size_t& idx) const {
    std::size_t lo = 0, hi = dict.size;
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;
        nostl::StrView tv = termview(mid);
        int c = nostl::sv_cmp(tv, term);
        if (c < 0) lo = mid + 1;
        else hi = mid;
    }
    if (lo < dict.size && nostl::sv_eq(termview(lo), term)) {
        idx = lo;
        return true;
    }
    return false;
}

bool InvertedIndex6::open(const std::string& path) {
    in.open(path, std::ios::binary);
    if (!in) return false;

    char magic[4];
    read_exact(in, magic, 4);
    if (std::string(magic, magic + 4) != "INV2")
        throw std::runtime_error("bad INV magic");

    hdr.ver = read_u32(in);
    hdr.docs = read_u32(in);
    hdr.terms = read_u32(in);
    hdr.dict_off = read_u64(in);
    hdr.term_pool_off = read_u64(in);
    hdr.postings_off = read_u64(in);

    if (hdr.ver != 2) throw std::runtime_error("INV2: bad ver");

    dict.clear_free();
    termpool.clear_free();

    if (!dict.resize(hdr.terms)) throw std::runtime_error("oom");

    in.seekg(static_cast<std::streamoff>(hdr.dict_off), std::ios::beg);
    for (std::uint32_t i = 0; i < hdr.terms; ++i) {
        dict.data[i].termoff = read_u64(in);
        dict.data[i].termlen = read_u32(in);
        dict.data[i].postoff = read_u64(in);
        dict.data[i].postlen = read_u32(in);
    }

    const std::uint64_t termpoolsize = hdr.postings_off - hdr.term_pool_off;
    if (!termpool.resize(static_cast<std::size_t>(termpoolsize)))
        throw std::runtime_error("oom");

    in.seekg(static_cast<std::streamoff>(hdr.term_pool_off), std::ios::beg);
    if (termpoolsize) read_exact(in, termpool.data, static_cast<std::size_t>(termpoolsize));

    return true;
}

bool InvertedIndex6::get_postings(nostl::StrView term, nostl::Vec<std::uint32_t>& out_docs) {
    if (!in) throw std::runtime_error("inv not opened");

    std::size_t idx = 0;
    if (!find_term(term, idx)) {
        out_docs.clear_keep();
        return false;
    }

    const auto& e = dict.data[idx];
    out_docs.clear_keep();
    if (e.postlen == 0) return true;

    nostl::Vec<std::uint8_t> buf;
    if (!buf.resize(static_cast<std::size_t>(e.postlen))) throw std::runtime_error("oom");

    const std::uint64_t byteoff = hdr.postings_off + e.postoff;
    in.seekg(static_cast<std::streamoff>(byteoff), std::ios::beg);
    read_exact(in, buf.data, static_cast<std::size_t>(e.postlen));

    const std::uint8_t* p = buf.data;
    const std::uint8_t* end = buf.data + buf.size;

    std::uint32_t doc = 0;
    while (p < end) {
        std::uint32_t doc_gap = 0;
        p = lab6::vbyte_get_u32(p, end, doc_gap);
        if (!p) throw std::runtime_error("bad vbyte in doc_gap");
        doc += doc_gap;

        if (!out_docs.push_back(doc)) throw std::runtime_error("oom");

        std::uint32_t tf = 0;
        p = lab6::vbyte_get_u32(p, end, tf);
        if (!p) throw std::runtime_error("bad vbyte in tf");

        for (std::uint32_t i = 0; i < tf; ++i) {
            std::uint32_t pos_gap = 0;
            p = lab6::vbyte_get_u32(p, end, pos_gap);
            if (!p) throw std::runtime_error("bad vbyte in pos_gap");
        }
    }

    return true;
}

bool InvertedIndex6::get_postings_tf(nostl::StrView term, nostl::Vec<PostingTF>& out) {
  if (!in) throw std::runtime_error("inv not opened");

  std::size_t idx = 0;
  if (!find_term(term, idx)) {
    out.clear_keep();
    return false;
  }

  const auto e = dict.data[idx];
  out.clear_keep();
  if (e.postlen == 0) return true;

  nostl::Vec<std::uint8_t> buf;
  if (!buf.resize(static_cast<std::size_t>(e.postlen))) throw std::runtime_error("oom");

  const std::uint64_t byteoff = hdr.postings_off + e.postoff;
  in.seekg(static_cast<std::streamoff>(byteoff), std::ios::beg);
  read_exact(in, buf.data, static_cast<std::size_t>(e.postlen));

  const std::uint8_t* p = buf.data;
  const std::uint8_t* end = buf.data + buf.size;

  std::uint32_t doc = 0;
  while (p < end) {
    std::uint32_t docgap = 0;
    p = lab6::vbyte_get_u32(p, end, docgap);
    if (!p) throw std::runtime_error("bad vbyte in docgap");

    doc += docgap;

    std::uint32_t tf = 0;
    p = lab6::vbyte_get_u32(p, end, tf);
    if (!p) throw std::runtime_error("bad vbyte in tf");

    PostingTF pt;
    pt.docid = doc;
    pt.tf = tf;
    if (!out.push_back(pt)) throw std::runtime_error("oom");

    for (std::uint32_t i = 0; i < tf; i++) {
      std::uint32_t posgap = 0;
      p = lab6::vbyte_get_u32(p, end, posgap);
      if (!p) throw std::runtime_error("bad vbyte in posgap");
    }
  }
  return true;
}