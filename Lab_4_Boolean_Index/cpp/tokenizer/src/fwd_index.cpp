#include "../include/fwd_index.hpp"

#include <stdexcept>

bool ForwardIndex::open(const std::string& path) {
    in.open(path, std::ios::binary);
    if (!in) return false;

    char magic[4];
    read_exact(in, magic, 4);
    if (std::string(magic, magic + 4) != "FWD1") throw std::runtime_error("bad FWD magic");

    hdr.ver = read_u32(in);
    hdr.docs = read_u32(in);
    hdr.pool_off = read_u64(in);

    const std::uint64_t endpos = static_cast<std::uint64_t>(in.seekg(0, std::ios::end).tellg());
    if (endpos < hdr.pool_off) throw std::runtime_error("bad FWD: pool_off out of range");

    const std::uint64_t pool_size = endpos - hdr.pool_off;
    pool.clear_free();
    if (!pool.resize(static_cast<std::size_t>(pool_size))) throw std::runtime_error("oom");

    if (pool_size) {
        in.seekg(static_cast<std::streamoff>(hdr.pool_off), std::ios::beg);
        read_exact(in, pool.data, static_cast<std::size_t>(pool_size));
    }
    return true;
}

DocInfoView ForwardIndex::get(std::uint32_t docid) {
    if (!in) throw std::runtime_error("fwd not opened");
    if (docid >= hdr.docs) throw std::runtime_error("docid out of range");

    const std::uint64_t recoff = kHeaderSize + static_cast<std::uint64_t>(docid) * kRecSize;
    in.seekg(static_cast<std::streamoff>(recoff), std::ios::beg);

    const std::uint64_t url_off   = read_u64(in);
    const std::uint32_t url_len   = read_u32(in);
    const std::uint64_t title_off = read_u64(in);
    const std::uint32_t title_len = read_u32(in);

    if (url_off + url_len > pool.size) throw std::runtime_error("bad fwd url range");
    if (title_off + title_len > pool.size) throw std::runtime_error("bad fwd title range");

    DocInfoView d;
    d.url = nostl::StrView(pool.data + static_cast<std::size_t>(url_off), url_len);
    d.title = nostl::StrView(pool.data + static_cast<std::size_t>(title_off), title_len);
    return d;
}
