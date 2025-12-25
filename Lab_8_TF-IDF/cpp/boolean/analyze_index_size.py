#!/usr/bin/env python3
import struct
import os
import sys

def read_u32(f):
    return struct.unpack('<I', f.read(4))[0]

def read_u64(f):
    return struct.unpack('<Q', f.read(8))[0]

def vbyte_decode(data, pos):
    v = 0
    shift = 0
    while pos < len(data):
        b = data[pos]
        pos += 1
        v |= ((b & 0x7F) << shift)
        if b & 0x80:
            return v, pos
        shift += 7
    return None, pos

def analyze_inv_base(path):
    if not os.path.exists(path):
        return None

    size = os.path.getsize(path)
    with open(path, 'rb') as f:
        magic = f.read(4).decode('ascii', errors='ignore')
        ver = read_u32(f)
        docs = read_u32(f)
        terms = read_u32(f)
        dict_off = read_u64(f)
        term_pool_off = read_u64(f)
        postings_off = read_u64(f)

        postings_size = size - postings_off
        dict_size = term_pool_off - dict_off
        term_pool_size = postings_off - term_pool_off

        return {
            'magic': magic,
            'version': ver,
            'docs': docs,
            'terms': terms,
            'total_size': size,
            'dict_size': dict_size,
            'term_pool_size': term_pool_size,
            'postings_size': postings_size,
            'dict_off': dict_off,
            'term_pool_off': term_pool_off,
            'postings_off': postings_off
        }

def analyze_inv6_compressed(path):
    if not os.path.exists(path):
        return None

    info = analyze_inv_base(path)
    if not info:
        return None

    with open(path, 'rb') as f:
        f.seek(info['postings_off'])
        postings_data = f.read()

        f.seek(info['dict_off'])
        total_docs_in_postings = 0
        total_positions = 0
        total_vbyte_numbers = 0

        for t in range(info['terms']):
            term_off = read_u64(f)
            term_len = read_u32(f)
            post_off = read_u64(f)
            post_len = read_u32(f)

            if post_len > 0:
                pos = post_off
                end = post_off + post_len

                while pos < end:
                    doc_gap, pos = vbyte_decode(postings_data, pos)
                    total_vbyte_numbers += 1
                    if doc_gap is None:
                        break

                    tf, pos = vbyte_decode(postings_data, pos)
                    total_vbyte_numbers += 1
                    if tf is None:
                        break

                    total_docs_in_postings += 1
                    total_positions += tf

                    for _ in range(tf):
                        pos_gap, pos = vbyte_decode(postings_data, pos)
                        total_vbyte_numbers += 1
                        if pos_gap is None:
                            break

    info['total_docs_in_postings'] = total_docs_in_postings
    info['total_positions'] = total_positions
    info['total_vbyte_numbers'] = total_vbyte_numbers
    info['avg_bytes_per_vbyte'] = info['postings_size'] / total_vbyte_numbers if total_vbyte_numbers > 0 else 0

    return info

def analyze_fwd(path):
    if not os.path.exists(path):
        return None

    size = os.path.getsize(path)
    with open(path, 'rb') as f:
        magic = f.read(4).decode('ascii', errors='ignore')
        ver = read_u32(f)
        docs = read_u32(f)
        pool_off = read_u64(f)

        pool_size = size - pool_off

        return {
            'magic': magic,
            'version': ver,
            'docs': docs,
            'total_size': size,
            'pool_size': pool_size
        }

print("=" * 80)
print("INDEX SIZE ANALYSIS")
print("=" * 80)

fwd_base = analyze_fwd('out/index.fwd')
inv_base = analyze_inv_base('out/index.inv')
fwd6 = analyze_fwd('out6/index6.fwd')
inv6 = analyze_inv6_compressed('out6/index6.inv')

print("\n[FORWARD INDEX]")
if fwd_base:
    print(f"  Base (out/index.fwd):     {fwd_base['total_size']:>12,} bytes")
if fwd6:
    print(f"  Lab6 (out6/index6.fwd):   {fwd6['total_size']:>12,} bytes")
if fwd_base and fwd6:
    diff = fwd6['total_size'] - fwd_base['total_size']
    pct = (diff / fwd_base['total_size'] * 100) if fwd_base['total_size'] > 0 else 0
    print(f"  Difference:               {diff:>12,} bytes ({pct:+.1f}%)")

print("\n[INVERTED INDEX]")
if inv_base:
    print(f"  Base (out/index.inv):     {inv_base['total_size']:>12,} bytes")
    print(f"    - Dictionary:           {inv_base['dict_size']:>12,} bytes")
    print(f"    - Term pool:            {inv_base['term_pool_size']:>12,} bytes")
    print(f"    - Postings:             {inv_base['postings_size']:>12,} bytes")
    print(f"    - Terms:                {inv_base['terms']:>12,}")
    print(f"    - Docs:                 {inv_base['docs']:>12,}")

if inv6:
    print(f"\n  Lab6 (out6/index6.inv):   {inv6['total_size']:>12,} bytes")
    print(f"    - Dictionary:           {inv6['dict_size']:>12,} bytes")
    print(f"    - Term pool:            {inv6['term_pool_size']:>12,} bytes")
    print(f"    - Postings (VByte):     {inv6['postings_size']:>12,} bytes")
    print(f"    - Terms:                {inv6['terms']:>12,}")
    print(f"    - Docs:                 {inv6['docs']:>12,}")
    print(f"    - VByte numbers:        {inv6['total_vbyte_numbers']:>12,}")
    print(f"    - Avg bytes/VByte:      {inv6['avg_bytes_per_vbyte']:>12.2f}")

if inv_base and inv6:
    diff = inv6['total_size'] - inv_base['total_size']
    pct = (diff / inv_base['total_size'] * 100) if inv_base['total_size'] > 0 else 0
    print(f"\n  Difference:               {diff:>12,} bytes ({pct:+.1f}%)")

    if inv_base['postings_size'] > 0:
        post_diff = inv6['postings_size'] - inv_base['postings_size']
        post_pct = (post_diff / inv_base['postings_size'] * 100)
        print(f"  Postings difference:      {post_diff:>12,} bytes ({post_pct:+.1f}%)")

print("\n" + "=" * 80)
print("DIAGNOSIS")
print("=" * 80)

if inv_base and inv6:
    if inv6['postings_size'] > inv_base['postings_size']:
        print("\n❌ PROBLEM: Compressed postings are LARGER than base!")
        print("\nPossible causes:")
        print("  1. Base format might already use compression")
        print("  2. VByte overhead for small numbers (each number needs ≥1 byte)")
        print("  3. Base format might store data more efficiently")
        print("\nNeed to check: What format does base index use?")
        print("Run this to inspect base index structure:")
        print("  hexdump -C out/index.inv | head -n 50")
    else:
        print("✓ Postings compression works!")
        ratio = (1 - inv6['postings_size'] / inv_base['postings_size']) * 100
        print(f"  Compression ratio: {ratio:.1f}% reduction")
