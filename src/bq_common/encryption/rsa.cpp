/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
/*!
 * \file rsa.cpp
 * \date 2025/08/18 01:22
 *
 * \author pippocao
 *
 * \brief
 * RSA raw implementation, compatible with ssh-keygen output.
 */

#include "bq_common/encryption/rsa.h"
#include "bq_common/encryption/random_bytes.h"

namespace bq {
    static uint64_t get_public_key_fingerprint(const bq::array<uint8_t>& n, const bq::array<uint8_t>& e)
    {
        if (n.is_empty() || e.is_empty()) {
            return 0;
        }
        bq::array<uint8_t> data;
        data.fill_uninitialized(n.size() + e.size() + sizeof(uint32_t) * 2);
        uint8_t* data_begin = data.begin();
        if (!data_begin) {
            return 0;
        }
        uint8_t* cursor = data_begin;
        const uint32_t n_size = static_cast<uint32_t>(n.size());
        const uint32_t e_size = static_cast<uint32_t>(e.size());
        cursor[0] = static_cast<uint8_t>(n_size >> 24);
        cursor[1] = static_cast<uint8_t>(n_size >> 16);
        cursor[2] = static_cast<uint8_t>(n_size >> 8);
        cursor[3] = static_cast<uint8_t>(n_size);
        cursor += sizeof(uint32_t);
        memcpy(cursor, n.begin(), n.size());
        cursor += n.size();
        cursor[0] = static_cast<uint8_t>(e_size >> 24);
        cursor[1] = static_cast<uint8_t>(e_size >> 16);
        cursor[2] = static_cast<uint8_t>(e_size >> 8);
        cursor[3] = static_cast<uint8_t>(e_size);
        cursor += sizeof(uint32_t);
        memcpy(cursor, e.begin(), e.size());
        return bq::util::get_hash_64(data_begin, data.size());
    }

    static uint32_t load_u32_be(const uint8_t* p)
    {
        uint32_t v = 0;
        v |= static_cast<uint32_t>(p[0]) << 24;
        v |= static_cast<uint32_t>(p[1]) << 16;
        v |= static_cast<uint32_t>(p[2]) << 8;
        v |= static_cast<uint32_t>(p[3]);
        return v;
    }

    static void trim_be(const uint8_t* in, size_t in_len, bq::array<uint8_t>& out)
    {
        size_t i = 0;
        while (i < in_len && in[i] == 0) {
            ++i;
        }
        size_t n = 0;
        if (i == in_len) {
            n = 1;
        } else {
            n = in_len - i;
        }
        out.clear();
        out.insert_batch(out.end(), in + i, n);
    }

    // base64 decode (RFC 4648, ignores whitespace)
    static bool base64_decode(const uint8_t* in, size_t in_len, bq::array<uint8_t>& out)
    {
        static const uint8_t b64_table[256] = {
            /* 0..15  */ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x81, 0x80, 0x81, 0x81, 0x80, 0x80,
            /* 16..31 */ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            /* 32..47 */ 0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 62, 0x80, 62, 0x80, 63,
            /* 48..63 */ 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0x80, 0x80, 0x80, 0x82, 0x80, 0x80,
            /* 64..79 */ 0x80, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
            /* 80..95 */ 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0x80, 0x80, 0x80, 0x80, 63,
            /* 96..111*/ 0x80, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            /*112..127*/ 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0x80, 0x80, 0x80, 0x80, 0x80,
            /*128..255*/
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
        };
        out.clear();
        uint32_t buf = 0;
        int32_t bits = 0;
        for (size_t i = 0; i < in_len; ++i) {
            uint8_t c = in[i];
            uint8_t v = b64_table[c];
            if (v == 0x81) {
                continue;
            }
            if (v == 0x80) {
                bq::util::log_device_console(bq::log_level::error, "base64_decode: invalid character");
                return false;
            }
            if (v == 0x82) {
                break;
            }
            buf = (buf << 6) | v;
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                uint8_t byte = static_cast<uint8_t>((buf >> bits) & 0xFF);
                out.insert_batch(out.end(), &byte, 1);
            }
        }
        return true;
    }

    static bool parse_ssh_wire_rsa(const uint8_t* data, size_t len, bq::array<uint8_t>& e_be, bq::array<uint8_t>& n_be)
    {
        if (len < 4) {
            bq::util::log_device_console(bq::log_level::error, "ssh-rsa: truncated key");
            return false;
        }
        uint32_t l1 = load_u32_be(data);
        data += 4;
        len -= 4;
        if (len < l1) {
            bq::util::log_device_console(bq::log_level::error, "ssh-rsa: bad type length");
            return false;
        }
        if (l1 != 7) {
            bq::util::log_device_console(bq::log_level::error, "ssh-rsa: bad type length");
            return false;
        }
        if (memcmp(data, "ssh-rsa", 7) != 0) {
            bq::util::log_device_console(bq::log_level::error, "ssh-rsa: wrong type");
            return false;
        }
        data += 7;
        len -= 7;
        if (len < 4) {
            bq::util::log_device_console(bq::log_level::error, "ssh-rsa: missing e length");
            return false;
        }
        uint32_t le = load_u32_be(data);
        data += 4;
        len -= 4;
        if (len < le) {
            bq::util::log_device_console(bq::log_level::error, "ssh-rsa: truncated e");
            return false;
        }
        trim_be(data, le, e_be);
        data += le;
        len -= le;

        if (len < 4) {
            bq::util::log_device_console(bq::log_level::error, "ssh-rsa: missing n length");
            return false;
        }
        uint32_t ln = load_u32_be(data);
        data += 4;
        len -= 4;
        if (len < ln) {
            bq::util::log_device_console(bq::log_level::error, "ssh-rsa: truncated n");
            return false;
        }
        trim_be(data, ln, n_be);
        return true;
    }

    // Minimal DER reader for PKCS#1 RSAPrivateKey
    struct der {
        const uint8_t* p_;
        size_t n_;
        size_t i_;
    };
    static bool der_next_tlv(der& d, uint8_t& tag, const uint8_t*& val, size_t& vlen)
    {
        if (d.i_ >= d.n_) {
            return false;
        }
        tag = d.p_[d.i_];
        d.i_++;
        if (d.i_ >= d.n_) {
            return false;
        }
        uint8_t l = d.p_[d.i_];
        d.i_++;
        size_t len = 0;
        if ((l & 0x80) == 0) {
            len = l;
        } else {
            uint8_t ll = l & 0x7F;
            if (ll == 0 || ll > 4) {
                return false;
            }
            if (d.i_ + ll > d.n_) {
                return false;
            }
            len = 0;
            for (uint8_t k = 0; k < ll; ++k) {
                len = (len << 8) | d.p_[d.i_];
                d.i_++;
            }
        }
        if (d.i_ + len > d.n_) {
            return false;
        }
        val = d.p_ + d.i_;
        vlen = len;
        d.i_ += len;
        return true;
    }

    static bool der_expect_tag(uint8_t tag, uint8_t expected)
    {
        return tag == expected;
    }

    static void der_copy_int_to_be_no_lead(const uint8_t* val, size_t vlen, bq::array<uint8_t>& out)
    {
        trim_be(val, vlen, out);
    }

    // Big number helpers
    static size_t limbs_for_bytes(size_t bytes)
    {
        return (bytes + 3u) / 4u;
    }

    static void bn_zero(uint32_t* x, size_t l)
    {
        memset(x, 0, l * sizeof(uint32_t));
    }

    // Zero every limb, then put v in the lowest one.
    //
    // The null test is for the compiler rather than for a caller: bq::array keeps its pointer null
    // until it has allocated, so GCC cannot prove that fill_uninitialized(l) with l > 0 handed out a
    // valid pointer and reports the write below as a null dereference when this is inlined.
    static void bn_set_u32(uint32_t* x, size_t l, uint32_t v)
    {
        if (!x) {
            return;
        }
        bn_zero(x, l);
        if (l > 0) {
            x[0] = v;
        }
    }

    static void bn_copy(uint32_t* dst, const uint32_t* src, size_t l)
    {
        if (dst != src) {
            memcpy(dst, src, l * sizeof(uint32_t));
        }
    }

    static int32_t bn_ucmp(const uint32_t* a, const uint32_t* b, size_t l)
    {
        for (size_t i = 0; i < l; ++i) {
            size_t j = l - 1 - i;
            if (a[j] < b[j]) {
                return -1;
            }
            if (a[j] > b[j]) {
                return 1;
            }
        }
        return 0;
    }

    static uint32_t bn_add(uint32_t* r, const uint32_t* a, const uint32_t* b, size_t l)
    {
        uint64_t c = 0;
        for (size_t i = 0; i < l; ++i) {
            c = c + (uint64_t)a[i];
            c = c + (uint64_t)b[i];
            r[i] = (uint32_t)c;
            c >>= 32;
        }
        return (uint32_t)c;
    }

    static uint32_t bn_sub(uint32_t* r, const uint32_t* a, const uint32_t* b, size_t l)
    {
        uint64_t c = 0;
        for (size_t i = 0; i < l; ++i) {
            uint64_t ai = a[i];
            uint64_t bi = b[i];
            uint64_t t = ai - bi - c;
            r[i] = (uint32_t)t;
            c = (t >> 63) & 1u;
        }
        return (uint32_t)c;
    }

    static void bn_from_be_bytes(uint32_t* x, size_t l, const uint8_t* be, size_t be_len)
    {
        bn_zero(x, l);
        size_t o = 0;
        for (size_t i = 0; i < be_len; ++i) {
            size_t bi = be_len - 1 - i;
            size_t wi = o >> 2;
            size_t bo = o & 3u;
            if (wi < l) {
                x[wi] |= (uint32_t)be[bi] << (8u * bo);
            }
            o++;
        }
    }

    static void bn_to_be_bytes(const uint32_t* x, size_t l, uint8_t* out, size_t out_len)
    {
        for (size_t i = 0; i < out_len; ++i) {
            out[i] = 0;
        }
        // write i-th least-significant byte to out[out_len-1-i]
        for (size_t i = 0; i < out_len; ++i) {
            size_t wi = i >> 2;
            size_t bo = i & 3u;
            uint8_t v = 0;
            if (wi < l) {
                v = (uint8_t)((x[wi] >> (8u * bo)) & 0xFFu);
            }
            out[out_len - 1 - i] = v;
        }
    }

    static size_t bn_bitlen(const uint32_t* a, size_t l)
    {
        for (size_t i = 0; i < l; ++i) {
            size_t j = l - 1 - i;
            uint32_t w = a[j];
            if (w) {
                uint32_t x = w;
                uint32_t pos = 31;
                while ((x & (1u << 31)) == 0) {
                    x <<= 1;
                    pos--;
                }
                return j * 32 + pos + 1;
            }
        }
        return 0;
    }

    static int32_t bn_getbit(const uint32_t* a, size_t l, size_t bit_index)
    {
        size_t wi = bit_index >> 5;
        size_t bo = bit_index & 31u;
        if (wi >= l) {
            return 0;
        }
        return (int32_t)((a[wi] >> bo) & 1u);
    }

    static void mod_double(uint32_t* r, const uint32_t* a, const uint32_t* m, size_t l, uint32_t* tmp)
    {
        uint32_t carry = bn_add(tmp, a, a, l);
        bool ge = false;
        if (carry != 0) {
            ge = true;
        }
        if (bn_ucmp(tmp, m, l) >= 0) {
            ge = true;
        }
        if (ge) {
            (void)bn_sub(tmp, tmp, m, l);
        }
        bn_copy(r, tmp, l);
    }

#if defined(BQ_UNIT_TEST)
    // The schoolbook double-and-add modexp the Montgomery code replaced. Only the tests compile it:
    // they compare the two on random inputs instead of trusting a hand-written vector.
    static void mod_add(uint32_t* r, const uint32_t* a, const uint32_t* b, const uint32_t* m, size_t l, uint32_t* tmp)
    {
        uint32_t carry = bn_add(tmp, a, b, l);
        bool ge = false;
        if (carry != 0) {
            ge = true;
        }
        if (bn_ucmp(tmp, m, l) >= 0) {
            ge = true;
        }
        if (ge) {
            (void)bn_sub(tmp, tmp, m, l);
        }
        bn_copy(r, tmp, l);
    }

    static void mod_mul_bin(uint32_t* r, const uint32_t* a, const uint32_t* b, const uint32_t* m, size_t l,
        uint32_t* acc, uint32_t* cur, uint32_t* tmp)
    {
        bn_zero(acc, l);
        bn_copy(cur, a, l);
        size_t bits = bn_bitlen(b, l);
        for (size_t i = 0; i < bits; ++i) {
            if (bn_getbit(b, l, i)) {
                mod_add(acc, acc, cur, m, l, tmp);
            }
            mod_double(cur, cur, m, l, tmp);
        }
        bn_copy(r, acc, l);
    }

    static void mod_pow(uint32_t* r, const uint32_t* a, const uint32_t* e, const uint32_t* m, size_t l,
        uint32_t* t0, uint32_t* t1, uint32_t* t2, uint32_t* t3)
    {
        bn_set_u32(r, l, 1u);
        bn_copy(t0, a, l);
        size_t bits = bn_bitlen(e, l);
        for (size_t i = 0; i < bits; ++i) {
            if (bn_getbit(e, l, i)) {
                mod_mul_bin(r, r, t0, m, l, t1, t2, t3);
            }
            mod_mul_bin(t0, t0, t0, m, l, t1, t2, t3);
        }
    }
#endif

    static void be_to_fixed_limbs(uint32_t* x, size_t l, const bq::array<uint8_t>& be)
    {
        bn_zero(x, l);
        bn_from_be_bytes(x, l, be.begin(), be.size());
    }

    static void fixed_limbs_to_be_bytes(const uint32_t* x, size_t l, size_t k_bytes, bq::array<uint8_t>& out)
    {
        out.clear();
        out.fill_uninitialized(k_bytes);
        bn_to_be_bytes(x, l, out.begin(), k_bytes);
    }

    static void fixed_limbs_to_min_be(const uint32_t* x, size_t l, bq::array<uint8_t>& out)
    {
        size_t k = l * 4u;
        bq::array<uint8_t> tmp;
        tmp.fill_uninitialized(k);
        bn_to_be_bytes(x, l, tmp.begin(), k);
        size_t i = 0;
        while (i < k && tmp[i] == 0) {
            i++;
        }
        out.clear();
        if (i == k) {
            // Zero, whose minimal encoding is one zero byte. Copying from tmp.begin() + k would read
            // one byte past the buffer: the count and the position both have to be handled here, not
            // left to the general path below.
            out.push_back(static_cast<uint8_t>(0));
            return;
        }
        out.insert_batch(out.end(), tmp.begin() + static_cast<ptrdiff_t>(i), k - i);
    }

    // ---------------------------------------------------------------------------------------
    // Montgomery arithmetic
    //
    // Limbs are little-endian: limb 0 is the least significant, and bit i of the value lives in
    // limb i / 32. Everything uses 32-bit words with a 64-bit accumulator, so MSVC compiles the
    // same code without __int128.
    // ---------------------------------------------------------------------------------------

    static void bn_sub_from_zero(uint32_t* r, const uint32_t* n, size_t l)
    {
        // 0 - n == ~n + 1, which is 2^(32l) - n: the value R - n with R = 2^(32l).
        uint64_t borrow = 1U;
        for (size_t i = 0; i < l; ++i) {
            // The complement is kept at 32 bits before it enters the accumulator. MSVC's C4319
            // fires on the zero extension of a ~ result and reads like a sign-extension bug there.
            const uint64_t v = static_cast<uint64_t>(n[i] ^ 0xFFFFFFFFU) + borrow;
            r[i] = static_cast<uint32_t>(v);
            borrow = v >> 32;
        }
    }

    static void bn_rshift1(uint32_t* x, size_t l)
    {
        for (size_t i = 0; i < l; ++i) {
            // The high limb is read before it is overwritten, so walking upwards is safe.
            const uint32_t hi = (i + 1U < l) ? x[i + 1U] : 0U;
            x[i] = (x[i] >> 1) | (hi << 31);
        }
    }

    static bool bn_is_one(const uint32_t* a, size_t l)
    {
        if (a[0] != 1U) {
            return false;
        }
        for (size_t i = 1; i < l; ++i) {
            if (a[i] != 0U) {
                return false;
            }
        }
        return true;
    }

    // -n0^-1 mod 2^32, the constant CIOS reduction needs. Newton's iteration doubles the number of
    // correct low bits every round, and x0 = n0 is already correct for three bits when n0 is odd, so
    // five rounds cover all 32.
    static uint32_t bn_n0_inverse(uint32_t n0)
    {
        uint32_t x = n0;
        for (int32_t i = 0; i < 5; ++i) {
            x = static_cast<uint32_t>(x * (2U - n0 * x));
        }
        return static_cast<uint32_t>(0U - x);
    }

    static void bn_mul_school(uint32_t* r, const uint32_t* a, size_t la, const uint32_t* b, size_t lb, size_t lr)
    {
        // r must hold la + lb words; callers size it that way instead of truncating here.
        bn_zero(r, lr);
        for (size_t i = 0; i < la; ++i) {
            uint64_t carry = 0;
            for (size_t j = 0; j < lb; ++j) {
                const uint64_t cur = static_cast<uint64_t>(r[i + j]) + static_cast<uint64_t>(a[i]) * b[j] + carry;
                r[i + j] = static_cast<uint32_t>(cur);
                carry = cur >> 32;
            }
            // Nothing has written r[i + lb] yet, so the carry lands on a zero word.
            r[i + lb] = static_cast<uint32_t>(carry);
        }
    }

    // r = a mod m, scanning a from its top bit down. A decode-side operation that runs once per
    // file, so the short auditable algorithm is worth more than the speed of a real division.
    static void bn_mod_binary(uint32_t* r, const uint32_t* a, size_t a_limbs, const uint32_t* m, size_t l, uint32_t* work)
    {
        uint32_t* cur = work; // l + 1 words: the doubling can carry out of the top limb
        bn_zero(cur, l + 1U);
        for (size_t i = bn_bitlen(a, a_limbs); i > 0; --i) {
            for (size_t j = l; j > 0; --j) {
                cur[j] = (cur[j] << 1) | (cur[j - 1U] >> 31);
            }
            cur[0] = (cur[0] << 1) | static_cast<uint32_t>(bn_getbit(a, a_limbs, i - 1));
            bool ge = cur[l] != 0U;
            if (!ge) {
                ge = bn_ucmp(cur, m, l) >= 0;
            }
            if (ge) {
                uint64_t borrow = 0;
                for (size_t j = 0; j < l; ++j) {
                    const uint64_t v = static_cast<uint64_t>(cur[j]) - m[j] - borrow;
                    cur[j] = static_cast<uint32_t>(v);
                    borrow = (v >> 63) & 1U;
                }
                cur[l] -= static_cast<uint32_t>(borrow);
            }
        }
        bn_copy(r, cur, l);
    }

    static bool bn_mod_inverse(uint32_t* x, const uint32_t* a, const uint32_t* m, size_t l);

    // Montgomery domain modulo an odd n, with R = 2^(32 * limbs).
    //
    // An instance owns the modulus, n0inv and R^2, so a whole modexp costs one allocation instead of
    // one per multiplication. R^2 is not cached anywhere outside the instance on purpose: the keys
    // are shared read-only across threads, and a lazily filled cache inside them would be a race.
    class mont_ctx {
    public:
        mont_ctx() : l_(0), n0inv_(0) {}

        bool init(const uint32_t* n, size_t l)
        {
            if (l == 0 || (n[0] & 1U) == 0U) {
                return false;
            }
            l_ = l;
            n0inv_ = bn_n0_inverse(n[0]);
            n_.fill_uninitialized(l);
            memcpy(n_.begin(), n, l * sizeof(uint32_t));
            one_.fill_uninitialized(l);
            bn_zero(one_.begin(), l);
            one_.begin()[0] = 1U;
            r2_.fill_uninitialized(l);
            t_.fill_uninitialized(l + 2U);
            d_.fill_uninitialized(l);
            compute_r2();
            return true;
        }

        size_t limbs() const { return l_; }

        // r = a * b * R^-1 mod n. r may alias a or b: they are only read while the accumulator is
        // built, and r is written once at the end.
        void mul(uint32_t* r, const uint32_t* a, const uint32_t* b)
        {
            const size_t l = l_;
            const uint32_t* n = n_.begin();
            uint32_t* t = t_.begin();
            for (size_t i = 0; i <= l; ++i) {
                t[i] = 0U;
            }
            for (size_t i = 0; i < l; ++i) {
                const uint32_t bi = b[i];
                uint64_t c = 0U;
                for (size_t j = 0; j < l; ++j) {
                    c = c + t[j] + static_cast<uint64_t>(a[j]) * bi;
                    t[j] = static_cast<uint32_t>(c);
                    c >>= 32;
                }
                c += t[l];
                t[l] = static_cast<uint32_t>(c);
                t[l + 1U] = static_cast<uint32_t>(c >> 32);

                const uint32_t q = t[0] * n0inv_;
                c = static_cast<uint64_t>(t[0]) + static_cast<uint64_t>(q) * n[0];
                for (size_t j = 1; j < l; ++j) {
                    c = (c >> 32) + t[j] + static_cast<uint64_t>(q) * n[j];
                    t[j - 1U] = static_cast<uint32_t>(c);
                }
                c = (c >> 32) + t[l];
                t[l - 1U] = static_cast<uint32_t>(c);
                t[l] = t[l + 1U] + static_cast<uint32_t>(c >> 32);
            }
            // The result is below 2n, so at most one subtraction; a mask picks between the two
            // candidates so the instruction trace does not depend on the values.
            uint32_t* d = d_.begin();
            const uint32_t borrow = bn_sub(d, t, n, l);
            const uint32_t top = t[l] - borrow;
            const uint32_t take_d = 0U - ((top >> 31) ^ 1U);
            for (size_t j = 0; j < l; ++j) {
                r[j] = (d[j] & take_d) | (t[j] & ~take_d);
            }
        }

        void to_mont(uint32_t* r, const uint32_t* x)
        {
            mul(r, x, r2_.begin());
        }

        void from_mont(uint32_t* r, const uint32_t* x)
        {
            mul(r, x, one_.begin());
        }

        // r = a * b mod n, for callers staying outside the Montgomery domain.
        void mod_mul(uint32_t* r, const uint32_t* a, const uint32_t* b)
        {
            mul(r, a, b);
            mul(r, r, r2_.begin());
        }

        // r = x^e mod n, e given as e_bits bits of little-endian limbs.
        void pow(uint32_t* r, const uint32_t* x, const uint32_t* e, size_t e_bits)
        {
            bq::array<uint32_t> acc_buf;
            bq::array<uint32_t> base_buf;
            acc_buf.fill_uninitialized(l_);
            base_buf.fill_uninitialized(l_);
            uint32_t* acc = acc_buf.begin();
            uint32_t* base = base_buf.begin();
            to_mont(base, x);
            to_mont(acc, one_.begin());
            for (size_t i = e_bits; i > 0; --i) {
                mul(acc, acc, acc);
                if (bn_getbit(e, l_, i - 1) != 0) {
                    mul(acc, acc, base);
                }
            }
            from_mont(r, acc);
        }

        // r = c^d mod n with base blinding: the exponentiation runs on c * s^e and the result is
        // multiplied by s^-1, so neither the exponent loop nor the intermediate values correlate
        // with c. Returns false when the random s turns out not to be invertible, which for a real
        // RSA modulus means the call would have factored the key.
        bool pow_blinded(uint32_t* r, const uint32_t* c, const uint32_t* pub_e, size_t pub_e_bits,
            const uint32_t* d, size_t d_bits)
        {
            bq::array<uint32_t> s_buf;
            bq::array<uint32_t> se_buf;
            bq::array<uint32_t> blinded_buf;
            bq::array<uint32_t> inv_buf;
            s_buf.fill_uninitialized(l_);
            se_buf.fill_uninitialized(l_);
            blinded_buf.fill_uninitialized(l_);
            inv_buf.fill_uninitialized(l_);
            uint32_t* s = s_buf.begin();
            uint32_t* se = se_buf.begin();
            uint32_t* blinded = blinded_buf.begin();
            uint32_t* inv = inv_buf.begin();

            // 256 bits of blinding factor: far shorter than any modulus this code accepts, so the
            // reduction below never actually removes anything, and it cannot overflow the limbs.
            uint8_t seed[32];
            bool got_seed = false;
            for (int32_t attempt = 0; attempt < 4 && !got_seed; ++attempt) {
                bq::encryption::fill_random_bytes(seed, sizeof(seed));
                bn_zero(s, l_);
                for (size_t i = 0; i < sizeof(seed) && i / 4U < l_; ++i) {
                    s[i / 4U] |= static_cast<uint32_t>(seed[i]) << (8U * (i % 4U));
                }
                got_seed = !bn_is_zero(s, l_);
            }
            if (!got_seed) {
                // No usable randomness: a partially blinded result is worse than a clear failure.
                return false;
            }
            clear_bytes(seed, sizeof(seed));

            pow(se, s, pub_e, pub_e_bits);
            mod_mul(blinded, c, se);
            pow(r, blinded, d, d_bits);
            if (!bn_mod_inverse(inv, s, n_.begin(), l_)) {
                return false;
            }
            mod_mul(r, r, inv);
            return true;
        }

    private:
        static bool bn_is_zero(const uint32_t* a, size_t l)
        {
            for (size_t i = 0; i < l; ++i) {
                if (a[i] != 0U) {
                    return false;
                }
            }
            return true;
        }

        static void clear_bytes(void* p, size_t n)
        {
            volatile uint8_t* v = static_cast<volatile uint8_t*>(p);
            for (size_t i = 0; i < n; ++i) {
                v[i] = 0U;
            }
        }

        void compute_r2()
        {
            const size_t l = l_;
            const uint32_t* n = n_.begin();
            uint32_t* r2 = r2_.begin();
            uint32_t* tmp = d_.begin();
            // Start from R mod n. A normalized modulus (top bit set) satisfies R < 2n, so R mod n is
            // the two's complement of n; otherwise double up to it from one.
            if ((n[l - 1U] & 0x80000000U) != 0U) {
                bn_sub_from_zero(r2, n, l);
            } else {
                bn_zero(r2, l);
                r2[0] = 1U;
                for (size_t i = 0; i < 32U * l; ++i) {
                    mod_double(r2, r2, n, l, tmp);
                }
            }
            // R^2 = R * 2^(32l) mod n.
            for (size_t i = 0; i < 32U * l; ++i) {
                mod_double(r2, r2, n, l, tmp);
            }
        }

        size_t l_;
        uint32_t n0inv_;
        bq::array<uint32_t> n_;
        bq::array<uint32_t> one_;
        bq::array<uint32_t> r2_;
        bq::array<uint32_t> t_;
        bq::array<uint32_t> d_;
    };

    // x/2 mod m for odd m.
    static void bn_mod_half(uint32_t* x, const uint32_t* m, size_t l)
    {
        if ((x[0] & 1U) == 0U) {
            bn_rshift1(x, l);
            return;
        }
        // x and m are both odd, so (x + m)/2 == x/2 + m/2 + 1 and neither sum can overflow: each
        // half term is at most 2^(32l - 1) - 1.
        uint32_t carry = 1U;
        for (size_t j = 0; j < l; ++j) {
            const uint32_t hi_x = (j + 1U < l) ? (x[j + 1U] << 31) : 0U;
            const uint32_t hi_m = (j + 1U < l) ? (m[j + 1U] << 31) : 0U;
            const uint32_t hx = (x[j] >> 1) | hi_x;
            const uint32_t hm = (m[j] >> 1) | hi_m;
            const uint32_t sum = hx + hm;
            const uint32_t c1 = (sum < hx) ? 1U : 0U;
            const uint32_t total = sum + carry;
            const uint32_t c2 = (total < sum) ? 1U : 0U;
            x[j] = total;
            carry = c1 | c2;
        }
    }

    // r = a - b mod m, for a, b < m.
    static void bn_sub_mod(uint32_t* r, const uint32_t* a, const uint32_t* b, const uint32_t* m, size_t l, uint32_t* tmp)
    {
        if (bn_ucmp(a, b, l) >= 0) {
            (void)bn_sub(r, a, b, l);
            return;
        }
        // b < m so m - b does not borrow, and a < b makes a + (m - b) < m so it does not carry.
        (void)bn_sub(tmp, m, b, l);
        (void)bn_add(r, a, tmp, l);
    }

    // x = a^-1 mod m for odd m and a < m. Binary extended gcd: u and v lose a bit every round, so
    // the loop count is bounded by the bit length of m.
    static bool bn_mod_inverse(uint32_t* x, const uint32_t* a, const uint32_t* m, size_t l)
    {
        if (bn_ucmp(a, m, l) >= 0) {
            return false;
        }
        bool a_is_zero = true;
        for (size_t i = 0; i < l; ++i) {
            if (a[i] != 0U) {
                a_is_zero = false;
                break;
            }
        }
        if (a_is_zero) {
            return false;
        }
        bq::array<uint32_t> u_buf;
        bq::array<uint32_t> v_buf;
        bq::array<uint32_t> k1_buf;
        bq::array<uint32_t> k2_buf;
        bq::array<uint32_t> tmp_buf;
        u_buf.fill_uninitialized(l);
        v_buf.fill_uninitialized(l);
        k1_buf.fill_uninitialized(l);
        k2_buf.fill_uninitialized(l);
        tmp_buf.fill_uninitialized(l);
        uint32_t* u = u_buf.begin();
        uint32_t* v = v_buf.begin();
        uint32_t* k1 = k1_buf.begin();
        uint32_t* k2 = k2_buf.begin();
        uint32_t* tmp = tmp_buf.begin();
        bn_copy(u, a, l);
        bn_copy(v, m, l);
        bn_zero(k1, l);
        k1[0] = 1U;
        bn_zero(k2, l);
        for (;;) {
            while ((u[0] & 1U) == 0U) {
                bn_rshift1(u, l);
                bn_mod_half(k1, m, l);
            }
            while ((v[0] & 1U) == 0U) {
                bn_rshift1(v, l);
                bn_mod_half(k2, m, l);
            }
            if (bn_is_one(u, l)) {
                bn_copy(x, k1, l);
                return true;
            }
            if (bn_is_one(v, l)) {
                bn_copy(x, k2, l);
                return true;
            }
            if (bn_ucmp(u, v, l) >= 0) {
                (void)bn_sub(u, u, v, l);
                bn_sub_mod(k1, k1, k2, m, l, tmp);
            } else {
                (void)bn_sub(v, v, u, l);
                bn_sub_mod(k2, k2, k1, m, l, tmp);
            }
        }
    }

    static bool parse_two_tokens(const uint8_t* p, size_t n, const uint8_t*& t1, size_t& l1, const uint8_t*& t2, size_t& l2)
    {
        size_t i = 0;
        while (i < n && (p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n')) {
            ++i;
        }
        size_t s1 = i;
        while (i < n && !(p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n')) {
            ++i;
        }
        size_t e1 = i;
        while (i < n && (p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n')) {
            ++i;
        }
        size_t s2 = i;
        while (i < n && !(p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n')) {
            ++i;
        }
        size_t e2 = i;
        if (e1 <= s1 || e2 <= s2) {
            return false;
        }
        t1 = p + s1;
        l1 = e1 - s1;
        t2 = p + s2;
        l2 = e2 - s2;
        return true;
    }

    static bool bn_check_nonzero(const bq::array<uint8_t>& x)
    {
        for (size_t i = 0; i < x.size(); ++i) {
            if (x[i] != 0) {
                return true;
            }
        }
        return false;
    }

    bool rsa::parse_public_key_ssh(const bq::string& ssh_pub_text, public_key& out)
    {
        bq::array<uint8_t> str_bytes;
        str_bytes.clear();
        str_bytes.insert_batch(str_bytes.end(), (const uint8_t*)ssh_pub_text.c_str(), ssh_pub_text.size());
        const uint8_t* t1 = nullptr;
        size_t l1 = 0;
        const uint8_t* t2 = nullptr;
        size_t l2 = 0;
        if (!parse_two_tokens(str_bytes.begin(), str_bytes.size(), t1, l1, t2, l2)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_public_key_ssh: cannot tokenize");
            return false;
        }
        bq::string type_str = "ssh-rsa";
        if (l1 != type_str.size()) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_public_key_ssh: not ssh-rsa");
            return false;
        }
        if (memcmp(t1, type_str.c_str(), l1) != 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_public_key_ssh: not ssh-rsa");
            return false;
        }
        bq::array<uint8_t> blob;
        if (!base64_decode(t2, l2, blob)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_public_key_ssh: base64 decode failed");
            return false;
        }
        bq::array<uint8_t> e_be;
        bq::array<uint8_t> n_be;
        if (!parse_ssh_wire_rsa(blob.begin(), blob.size(), e_be, n_be)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_public_key_ssh: wire parse failed");
            return false;
        }
        if (!bn_check_nonzero(n_be)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_public_key_ssh: empty n");
            return false;
        }
        if (!bn_check_nonzero(e_be)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_public_key_ssh: empty e");
            return false;
        }
        out.n_.clear();
        out.n_.insert_batch(out.n_.end(), n_be.begin(), n_be.size());
        out.e_.clear();
        out.e_.insert_batch(out.e_.end(), e_be.begin(), e_be.size());
        return true;
    }

    bool rsa::validate_public_key(const public_key& key)
    {
        // 2048 bits, exactly: the segment head reserves a fixed 256-byte field for the wrapped key.
        constexpr size_t required_modulus_size = 256;
        if (key.n_.size() != required_modulus_size) {
            bq::util::log_device_console(bq::log_level::error,
                "rsa::validate_public_key: modulus is %d bytes, exactly %d (2048 bits) is required",
                static_cast<int32_t>(key.n_.size()), static_cast<int32_t>(required_modulus_size));
            return false;
        }
        if ((key.n_[0] & 0x80) == 0) {
            bq::util::log_device_console(bq::log_level::error,
                "rsa::validate_public_key: modulus is shorter than 2048 bits");
            return false;
        }
        // Montgomery arithmetic requires an odd modulus, and a real RSA modulus always is one. An
        // even one here means the input is not an RSA key at all.
        if ((key.n_[key.n_.size() - 1] & 0x01) == 0) {
            bq::util::log_device_console(bq::log_level::error,
                "rsa::validate_public_key: modulus is even");
            return false;
        }
        if (key.e_.size() == 0) {
            bq::util::log_device_console(bq::log_level::error,
                "rsa::validate_public_key: exponent is empty");
            return false;
        }
        if ((key.e_[key.e_.size() - 1] & 0x01) == 0) {
            bq::util::log_device_console(bq::log_level::error,
                "rsa::validate_public_key: exponent must be odd");
            return false;
        }
        // The exponent has to be at least 65537. Compared as big-endian minimal-length bytes: a
        // longer value is larger, a shorter one is smaller, and equal lengths compare byte by byte.
        static const uint8_t minimum_exponent[] = { 0x01, 0x00, 0x01 };
        const size_t minimum_size = sizeof(minimum_exponent);
        bool exponent_large_enough = key.e_.size() > minimum_size;
        if (key.e_.size() == minimum_size) {
            exponent_large_enough = memcmp(key.e_.begin(), minimum_exponent, minimum_size) >= 0;
        }
        if (!exponent_large_enough) {
            bq::util::log_device_console(bq::log_level::error,
                "rsa::validate_public_key: exponent must be at least 65537");
            return false;
        }
        return true;
    }

    bool rsa::parse_private_key_pem(const bq::string& pem, private_key& out)
    {
        bq::string h1 = "-----BEGIN RSA PRIVATE KEY-----";
        bq::string h2 = "-----END RSA PRIVATE KEY-----";
        const uint8_t* p = (const uint8_t*)pem.c_str();
        size_t n = pem.size();

        size_t i = 0;
        bool found = false;
        for (; i + h1.size() <= n; ++i) {
            if (memcmp(p + i, h1.c_str(), h1.size()) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: header not found");
            return false;
        }
        while (i < n && p[i] != '\n') {
            ++i;
        }
        if (i < n) {
            ++i;
        }

        size_t j = i;
        found = false;
        for (; j + h2.size() <= n; ++j) {
            if (memcmp(p + j, h2.c_str(), h2.size()) == 0) {
                found = true;
                break;
            }
        }
        if (!found || j <= i) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: footer not found");
            return false;
        }

        bq::array<uint8_t> der_bytes;
        if (!base64_decode(p + i, j - i, der_bytes)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: base64 decode failed");
            return false;
        }

        der d;
        d.p_ = der_bytes.begin();
        d.n_ = der_bytes.size();
        d.i_ = 0;
        uint8_t tag = 0;
        const uint8_t* val = nullptr;
        size_t vlen = 0;
        if (!der_next_tlv(d, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: not a SEQUENCE");
            return false;
        }
        if (!der_expect_tag(tag, 0x30)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: not a SEQUENCE");
            return false;
        }
        der s;
        s.p_ = val;
        s.n_ = vlen;
        s.i_ = 0;
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: version missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: version missing");
            return false;
        }
        bq::array<uint8_t> n_be;
        bq::array<uint8_t> e_be;
        bq::array<uint8_t> d_be;
        bq::array<uint8_t> p_be;
        bq::array<uint8_t> q_be;
        bq::array<uint8_t> dp_be;
        bq::array<uint8_t> dq_be;
        bq::array<uint8_t> qi_be;
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: n missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: n missing");
            return false;
        }
        der_copy_int_to_be_no_lead(val, vlen, n_be);
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: e missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: e missing");
            return false;
        }
        der_copy_int_to_be_no_lead(val, vlen, e_be);
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: d missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: d missing");
            return false;
        }
        der_copy_int_to_be_no_lead(val, vlen, d_be);
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: p missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: p missing");
            return false;
        }
        der_copy_int_to_be_no_lead(val, vlen, p_be);
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: q missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: q missing");
            return false;
        }
        der_copy_int_to_be_no_lead(val, vlen, q_be);
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: dp missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: dp missing");
            return false;
        }
        der_copy_int_to_be_no_lead(val, vlen, dp_be);
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: dq missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: dq missing");
            return false;
        }
        der_copy_int_to_be_no_lead(val, vlen, dq_be);
        if (!der_next_tlv(s, tag, val, vlen)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: qInv missing");
            return false;
        }
        if (!der_expect_tag(tag, 0x02)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: qInv missing");
            return false;
        }
        der_copy_int_to_be_no_lead(val, vlen, qi_be);

        if (!bn_check_nonzero(n_be)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: empty n");
            return false;
        }
        if (!bn_check_nonzero(e_be)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: empty e");
            return false;
        }
        if (!bn_check_nonzero(d_be)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::parse_private_key_pem: empty d");
            return false;
        }

        out.n_.clear();
        out.n_.insert_batch(out.n_.end(), n_be.begin(), n_be.size());
        out.e_.clear();
        out.e_.insert_batch(out.e_.end(), e_be.begin(), e_be.size());
        out.d_.clear();
        out.d_.insert_batch(out.d_.end(), d_be.begin(), d_be.size());
        out.p_.clear();
        out.p_.insert_batch(out.p_.end(), p_be.begin(), p_be.size());
        out.q_.clear();
        out.q_.insert_batch(out.q_.end(), q_be.begin(), q_be.size());
        out.dp_.clear();
        out.dp_.insert_batch(out.dp_.end(), dp_be.begin(), dp_be.size());
        out.dq_.clear();
        out.dq_.insert_batch(out.dq_.end(), dq_be.begin(), dq_be.size());
        out.qinv_.clear();
        out.qinv_.insert_batch(out.qinv_.end(), qi_be.begin(), qi_be.size());

        return true;
    }

    uint64_t rsa::get_public_key_fingerprint(const public_key& key)
    {
        return bq::get_public_key_fingerprint(key.n_, key.e_);
    }

    uint64_t rsa::get_public_key_fingerprint(const bq::array<uint8_t>& n, const bq::array<uint8_t>& e)
    {
        return bq::get_public_key_fingerprint(n, e);
    }

    bool rsa::encrypt(const public_key& pub, const bq::array<uint8_t>& plaintext, bq::array<uint8_t>& out_ciphertext)
    {
        if (plaintext[0] == 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::encrypt (raw): plaintext starts with 0x00; unpadded RSA returns minimal big-endian on decrypt and drops leading zeros. ");
            return false;
        }
        if (pub.n_.size() == 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::encrypt: empty public key (n)");
            return false;
        }
        if (pub.e_.size() == 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::encrypt: empty public key (e)");
            return false;
        }
        size_t k_bytes = pub.n_.size();
        if (plaintext.size() > k_bytes) {
            bq::util::log_device_console(bq::log_level::error, "rsa::encrypt: plaintext too large");
            return false;
        }
        size_t l = limbs_for_bytes(k_bytes);
        bq::array<uint32_t> n_l;
        bq::array<uint32_t> e_l;
        bq::array<uint32_t> m_l;
        bq::array<uint32_t> c_l;
        n_l.fill_uninitialized(l);
        e_l.fill_uninitialized(l);
        m_l.fill_uninitialized(l);
        c_l.fill_uninitialized(l);

        be_to_fixed_limbs(n_l.begin(), l, pub.n_);
        be_to_fixed_limbs(e_l.begin(), l, pub.e_);
        be_to_fixed_limbs(m_l.begin(), l, plaintext);

        if (bn_ucmp(m_l.begin(), n_l.begin(), l) >= 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::encrypt: plaintext integer not less than modulus");
            return false;
        }

        mont_ctx mont;
        if (!mont.init(n_l.begin(), l)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::encrypt: modulus is not usable for Montgomery arithmetic");
            return false;
        }
        mont.pow(c_l.begin(), m_l.begin(), e_l.begin(), bn_bitlen(e_l.begin(), l));

        fixed_limbs_to_be_bytes(c_l.begin(), l, k_bytes, out_ciphertext);
        return true;
    }

    // RSA-CRT: two exponentiations with a half-size modulus, recombined through qinv. Returns false
    // when the CRT parameters are missing or inconsistent, and the caller then falls back to d.
    static bool rsa_decrypt_crt(uint32_t* out, size_t out_limbs, const rsa::private_key& pri,
        const uint32_t* c_l, size_t c_limbs, const uint32_t* e_l)
    {
        const size_t lp = limbs_for_bytes(pri.p_.size());
        const size_t lq = limbs_for_bytes(pri.q_.size());
        if (lp == 0 || lq == 0 || lp + lq < out_limbs) {
            return false;
        }
        const size_t lh = (lp > lq) ? lp : lq;
        bq::array<uint32_t> p_buf, q_buf, dp_buf, dq_buf, qi_buf, ep_buf, eq_buf;
        bq::array<uint32_t> cp_buf, cq_buf, m1_buf, m2_buf, h_buf, diff_buf, work_buf;
        p_buf.fill_uninitialized(lp);
        q_buf.fill_uninitialized(lq);
        dp_buf.fill_uninitialized(lp);
        dq_buf.fill_uninitialized(lq);
        qi_buf.fill_uninitialized(lp);
        ep_buf.fill_uninitialized(lp);
        eq_buf.fill_uninitialized(lq);
        cp_buf.fill_uninitialized(lp);
        cq_buf.fill_uninitialized(lq);
        m1_buf.fill_uninitialized(lh);
        m2_buf.fill_uninitialized(lh);
        h_buf.fill_uninitialized(lp);
        diff_buf.fill_uninitialized(lp);
        // The reduction works one word above the modulus, see bn_mod_binary.
        work_buf.fill_uninitialized(lh + 2U);
        uint32_t* work = work_buf.begin();
        uint32_t* p = p_buf.begin();
        uint32_t* q = q_buf.begin();
        uint32_t* dp = dp_buf.begin();
        uint32_t* dq = dq_buf.begin();
        uint32_t* qi = qi_buf.begin();
        uint32_t* ep = ep_buf.begin();
        uint32_t* eq = eq_buf.begin();
        uint32_t* m1 = m1_buf.begin();
        uint32_t* m2 = m2_buf.begin();

        be_to_fixed_limbs(p, lp, pri.p_);
        be_to_fixed_limbs(q, lq, pri.q_);
        be_to_fixed_limbs(dp, lp, pri.dp_);
        be_to_fixed_limbs(dq, lq, pri.dq_);
        be_to_fixed_limbs(qi, lp, pri.qinv_);
        bn_zero(m1, lh);
        bn_zero(m2, lh);

        mont_ctx mont_p;
        mont_ctx mont_q;
        if (!mont_p.init(p, lp) || !mont_q.init(q, lq)) {
            return false;
        }
        // The public exponent is small in every real key, but reducing it costs nothing and keeps
        // the modexp correct for a key that carries a large one.
        bn_mod_binary(ep, e_l, c_limbs, p, lp, work);
        bn_mod_binary(eq, e_l, c_limbs, q, lq, work);
        bn_mod_binary(cp_buf.begin(), c_l, c_limbs, p, lp, work);
        bn_mod_binary(cq_buf.begin(), c_l, c_limbs, q, lq, work);

        if (!mont_p.pow_blinded(m1, cp_buf.begin(), ep, bn_bitlen(ep, lp), dp, bn_bitlen(dp, lp))) {
            return false;
        }
        if (!mont_q.pow_blinded(m2, cq_buf.begin(), eq, bn_bitlen(eq, lq), dq, bn_bitlen(dq, lq))) {
            return false;
        }

        // h = (m1 - m2) * qinv mod p, then m = m2 + h * q.
        bn_sub_mod(diff_buf.begin(), m1, m2, p, lp, work);
        mont_p.mod_mul(h_buf.begin(), qi, diff_buf.begin());

        bq::array<uint32_t> prod_buf;
        prod_buf.fill_uninitialized(lp + lq);
        bn_mul_school(prod_buf.begin(), h_buf.begin(), lp, q, lq, lp + lq);
        uint64_t carry = 0;
        for (size_t i = 0; i < out_limbs; ++i) {
            const uint64_t sum = static_cast<uint64_t>(prod_buf[i]) + ((i < lq) ? m2[i] : 0U) + carry;
            out[i] = static_cast<uint32_t>(sum);
            carry = sum >> 32;
        }
        return true;
    }

    // r = c^d mod n: CRT when the key carries the parameters, base-blinded either way.
    static bool rsa_private_op(uint32_t* out, size_t out_limbs, const rsa::private_key& pri,
        const uint32_t* c_l, const uint32_t* e_l, const uint32_t* n_l, const uint32_t* d_l)
    {
        const bool have_crt = pri.p_.size() != 0 && pri.q_.size() != 0 && pri.dp_.size() != 0
            && pri.dq_.size() != 0 && pri.qinv_.size() != 0;
        if (have_crt && rsa_decrypt_crt(out, out_limbs, pri, c_l, out_limbs, e_l)) {
            return true;
        }
        mont_ctx mont;
        if (!mont.init(n_l, out_limbs)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: modulus is not usable for Montgomery arithmetic");
            return false;
        }
        const size_t e_bits = bn_bitlen(e_l, out_limbs);
        const size_t d_bits = bn_bitlen(d_l, out_limbs);
        if (!mont.pow_blinded(out, c_l, e_l, e_bits, d_l, d_bits)) {
            bq::util::log_device_console(bq::log_level::error, "rsa: blinding failed, no usable random source");
            return false;
        }
        return true;
    }

    bool rsa::decrypt(const private_key& pri, const bq::array<uint8_t>& ciphertext, bq::array<uint8_t>& out_plaintext)
    {
        if (pri.n_.size() == 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::decrypt: empty private key (n)");
            return false;
        }
        if (pri.d_.size() == 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::decrypt: empty private key (d)");
            return false;
        }
        if (pri.e_.size() == 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::decrypt: empty private key (e), blinding needs it");
            return false;
        }
        size_t k_bytes = pri.n_.size();
        if (ciphertext.size() != k_bytes) {
            bq::util::log_device_console(bq::log_level::error, "rsa::decrypt: ciphertext length must equal modulus length");
            return false;
        }
        size_t l = limbs_for_bytes(k_bytes);
        bq::array<uint32_t> n_l;
        bq::array<uint32_t> d_l;
        bq::array<uint32_t> e_l;
        bq::array<uint32_t> c_l;
        bq::array<uint32_t> m_l;
        n_l.fill_uninitialized(l);
        d_l.fill_uninitialized(l);
        e_l.fill_uninitialized(l);
        c_l.fill_uninitialized(l);
        m_l.fill_uninitialized(l);

        be_to_fixed_limbs(n_l.begin(), l, pri.n_);
        be_to_fixed_limbs(d_l.begin(), l, pri.d_);
        be_to_fixed_limbs(e_l.begin(), l, pri.e_);
        be_to_fixed_limbs(c_l.begin(), l, ciphertext);

        if (bn_ucmp(c_l.begin(), n_l.begin(), l) >= 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::decrypt: ciphertext integer not less than modulus");
            return false;
        }

        if (!rsa_private_op(m_l.begin(), l, pri, c_l.begin(), e_l.begin(), n_l.begin(), d_l.begin())) {
            return false;
        }

        fixed_limbs_to_min_be(m_l.begin(), l, out_plaintext);
        return true;
    }

    bool rsa::kem_encapsulate(const public_key& pub, bq::array<uint8_t>& out_ciphertext, bq::array<uint8_t>& out_shared_secret)
    {
        if (pub.n_.size() == 0 || pub.e_.size() == 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::kem_encapsulate: incomplete public key");
            return false;
        }
        const size_t k_bytes = pub.n_.size();
        const size_t l = limbs_for_bytes(k_bytes);
        bq::array<uint32_t> n_l;
        bq::array<uint32_t> e_l;
        bq::array<uint32_t> r_l;
        bq::array<uint32_t> c_l;
        bq::array<uint32_t> two_l;
        bq::array<uint32_t> upper_l;
        n_l.fill_uninitialized(l);
        e_l.fill_uninitialized(l);
        r_l.fill_uninitialized(l);
        c_l.fill_uninitialized(l);
        two_l.fill_uninitialized(l);
        upper_l.fill_uninitialized(l);
        be_to_fixed_limbs(n_l.begin(), l, pub.n_);
        be_to_fixed_limbs(e_l.begin(), l, pub.e_);
        // The sampling range is [2, n - 2]: zero and one carry no entropy, and n - 1 is its own
        // inverse of nothing useful, so a value that extreme is rejected rather than used.
        bn_set_u32(two_l.begin(), l, 2U);
        bn_copy(upper_l.begin(), n_l.begin(), l);
        (void)bn_sub(upper_l.begin(), upper_l.begin(), two_l.begin(), l);

        mont_ctx mont;
        if (!mont.init(n_l.begin(), l)) {
            bq::util::log_device_console(bq::log_level::error, "rsa::kem_encapsulate: modulus is not usable for Montgomery arithmetic");
            return false;
        }

        bq::array<uint8_t> random_bytes;
        random_bytes.fill_uninitialized(k_bytes);
        bool accepted = false;
        // The policy on weak key material belongs to the caller: an appender with no CSPRNG warns
        // once at creation and encrypts anyway, so refusing here would only contradict it.
        // The modulus has its top bit set, so a uniform 256-byte value lands in range about half the
        // time; 64 attempts failing means the random source is not random.
        for (int32_t attempt = 0; attempt < 64 && !accepted; ++attempt) {
            bq::encryption::fill_random_bytes(random_bytes.begin(), k_bytes);
            be_to_fixed_limbs(r_l.begin(), l, random_bytes);
            accepted = bn_ucmp(r_l.begin(), two_l.begin(), l) >= 0
                && bn_ucmp(r_l.begin(), upper_l.begin(), l) <= 0;
        }
        if (!accepted) {
            bq::util::log_device_console(bq::log_level::error, "rsa::kem_encapsulate: no value in [2, n-2] came out of the random source");
            return false;
        }

        mont.pow(c_l.begin(), r_l.begin(), e_l.begin(), bn_bitlen(e_l.begin(), l));

        // Both halves are fixed width by construction: the shared secret is the sampled integer in
        // its 256-byte form, which is exactly the buffer it was sampled from.
        fixed_limbs_to_be_bytes(c_l.begin(), l, k_bytes, out_ciphertext);
        out_shared_secret.clear();
        out_shared_secret.insert_batch(out_shared_secret.end(), random_bytes.begin(), random_bytes.size());
        return true;
    }

    bool rsa::kem_decapsulate(const private_key& pri, const bq::array<uint8_t>& ciphertext, bq::array<uint8_t>& out_shared_secret)
    {
        if (pri.n_.size() == 0 || pri.d_.size() == 0 || pri.e_.size() == 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::kem_decapsulate: incomplete private key");
            return false;
        }
        const size_t k_bytes = pri.n_.size();
        if (ciphertext.size() != k_bytes) {
            bq::util::log_device_console(bq::log_level::error, "rsa::kem_decapsulate: ciphertext must be exactly %d bytes", static_cast<int32_t>(k_bytes));
            return false;
        }
        const size_t l = limbs_for_bytes(k_bytes);
        bq::array<uint32_t> n_l;
        bq::array<uint32_t> e_l;
        bq::array<uint32_t> d_l;
        bq::array<uint32_t> c_l;
        bq::array<uint32_t> r_l;
        n_l.fill_uninitialized(l);
        e_l.fill_uninitialized(l);
        d_l.fill_uninitialized(l);
        c_l.fill_uninitialized(l);
        r_l.fill_uninitialized(l);
        be_to_fixed_limbs(n_l.begin(), l, pri.n_);
        be_to_fixed_limbs(e_l.begin(), l, pri.e_);
        be_to_fixed_limbs(d_l.begin(), l, pri.d_);
        be_to_fixed_limbs(c_l.begin(), l, ciphertext);
        if (bn_ucmp(c_l.begin(), n_l.begin(), l) >= 0) {
            bq::util::log_device_console(bq::log_level::error, "rsa::kem_decapsulate: ciphertext integer is not below the modulus");
            return false;
        }
        if (!rsa_private_op(r_l.begin(), l, pri, c_l.begin(), e_l.begin(), n_l.begin(), d_l.begin())) {
            return false;
        }
        // Fixed width, not minimal: a leading zero byte is part of the secret.
        fixed_limbs_to_be_bytes(r_l.begin(), l, k_bytes, out_shared_secret);
        return true;
    }

#if defined(BQ_UNIT_TEST)
    bool rsa::test_mod_pow_reference(const bq::array<uint8_t>& a, const bq::array<uint8_t>& e,
        const bq::array<uint8_t>& n, bq::array<uint8_t>& out)
    {
        if (a.size() == 0 || e.size() == 0 || n.size() == 0) {
            return false;
        }
        size_t l = limbs_for_bytes(n.size());
        bq::array<uint32_t> n_l, a_l, e_l, r_l, t0, t1, t2, t3;
        n_l.fill_uninitialized(l);
        a_l.fill_uninitialized(l);
        e_l.fill_uninitialized(l);
        r_l.fill_uninitialized(l);
        t0.fill_uninitialized(l);
        t1.fill_uninitialized(l);
        t2.fill_uninitialized(l);
        t3.fill_uninitialized(l);
        be_to_fixed_limbs(n_l.begin(), l, n);
        be_to_fixed_limbs(a_l.begin(), l, a);
        be_to_fixed_limbs(e_l.begin(), l, e);
        mod_pow(r_l.begin(), a_l.begin(), e_l.begin(), n_l.begin(), l,
            t0.begin(), t1.begin(), t2.begin(), t3.begin());
        fixed_limbs_to_min_be(r_l.begin(), l, out);
        return true;
    }

    bool rsa::test_mod_pow_mont(const bq::array<uint8_t>& a, const bq::array<uint8_t>& e,
        const bq::array<uint8_t>& n, bq::array<uint8_t>& out)
    {
        if (a.size() == 0 || e.size() == 0 || n.size() == 0) {
            return false;
        }
        size_t l = limbs_for_bytes(n.size());
        bq::array<uint32_t> n_l, a_l, e_l, r_l;
        n_l.fill_uninitialized(l);
        a_l.fill_uninitialized(l);
        e_l.fill_uninitialized(l);
        r_l.fill_uninitialized(l);
        be_to_fixed_limbs(n_l.begin(), l, n);
        be_to_fixed_limbs(a_l.begin(), l, a);
        be_to_fixed_limbs(e_l.begin(), l, e);
        mont_ctx mont;
        if (!mont.init(n_l.begin(), l)) {
            return false;
        }
        mont.pow(r_l.begin(), a_l.begin(), e_l.begin(), bn_bitlen(e_l.begin(), l));
        fixed_limbs_to_min_be(r_l.begin(), l, out);
        return true;
    }
#endif

}
