#include "search/inv_index.hpp"
#include <stdexcept>

nostl::StrView InvertedIndex::termview(std::size_t i) const {
    const auto& e = dict.data[i];
    return nostl::StrView(termpool.data + static_cast<std::size_t>(e.termoff),
                          static_cast<std::size_t>(e.termlen));
}

bool InvertedIndex::find_term(nostl::StrView term, std::size_t& idx) const {
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

bool InvertedIndex::open(const std::string& path) {
    in.open(path, std::ios::binary);
    if (!in) return false;

    char magic[4];
    read_exact(in, magic, 4);
    if (std::string(magic, magic + 4) != "INV1") throw std::runtime_error("bad INV magic");

    hdr.ver = read_u32(in);
    hdr.docs = read_u32(in);
    hdr.terms = read_u32(in);
    hdr.dict_off = read_u64(in);
    hdr.term_pool_off = read_u64(in);
    hdr.postings_off = read_u64(in);

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
    if (!termpool.resize(static_cast<std::size_t>(termpoolsize))) throw std::runtime_error("oom");

    in.seekg(static_cast<std::streamoff>(hdr.term_pool_off), std::ios::beg);
    if (termpoolsize) read_exact(in, termpool.data, static_cast<std::size_t>(termpoolsize));

    return true;
}

bool InvertedIndex::get_postings(nostl::StrView term, nostl::Vec<std::uint32_t>& out) {
    if (!in) throw std::runtime_error("inv not opened");

    std::size_t idx = 0;
    if (!find_term(term, idx)) {
        out.clear_keep();
        return false;
    }

    const auto& e = dict.data[idx];
    out.clear_keep();

    if (e.postlen == 0) return true;

    nostl::Vec<std::uint32_t> buf;
    if (!buf.resize(static_cast<std::size_t>(e.postlen))) throw std::runtime_error("oom");

    const std::uint64_t byteoff = hdr.postings_off + e.postoff * sizeof(std::uint32_t);
    in.seekg(static_cast<std::streamoff>(byteoff), std::ios::beg);
    read_exact(in, buf.data, static_cast<std::size_t>(e.postlen) * sizeof(std::uint32_t));

    std::size_t i = 0;
    while (i < buf.size) {
        std::uint32_t docid = buf.data[i++];
        if (i >= buf.size) throw std::runtime_error("bad posting format: missing tf");

        std::uint32_t tf = buf.data[i++];

        if (!out.push_back(docid)) throw std::runtime_error("oom");

        i += tf;
        if (i > buf.size) throw std::runtime_error("bad posting format: tf exceeds data");
    }

    return true;
}
