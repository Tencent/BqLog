/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#include "bq_common/encryption/chacha20.h"
#include "bq_common/platform/macros.h"
#include "bq_common/platform/simd.h"

namespace bq {

#ifdef BQ_UNIT_TEST
    chacha20::mode chacha20::hardware_acceleration_mode_ = chacha20::mode::auto_detect;

    void chacha20::set_hardware_acceleration_mode(mode m)
    {
        hardware_acceleration_mode_ = m;
    }
#endif

    namespace {
        constexpr uint32_t chacha_sigma[4] = { 0x61707865U, 0x3320646eU, 0x79622d32U, 0x6b206574U };

        bq_forceinline uint32_t rotl32(uint32_t v, uint32_t c)
        {
            return (v << c) | (v >> (32U - c));
        }

        bq_forceinline void quarter_round(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d)
        {
            a += b;
            d ^= a;
            d = rotl32(d, 16);
            c += d;
            b ^= c;
            b = rotl32(b, 12);
            a += b;
            d ^= a;
            d = rotl32(d, 8);
            c += d;
            b ^= c;
            b = rotl32(b, 7);
        }

        bq_forceinline uint32_t load_le32(const uint8_t* p)
        {
            return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
                | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        }

        bq_forceinline void store_le32(uint8_t* p, uint32_t v)
        {
            p[0] = static_cast<uint8_t>(v);
            p[1] = static_cast<uint8_t>(v >> 8);
            p[2] = static_cast<uint8_t>(v >> 16);
            p[3] = static_cast<uint8_t>(v >> 24);
        }

        bq_forceinline void build_initial_state(uint32_t state[16], const uint32_t* key, const uint32_t* nonce, uint32_t counter)
        {
            state[0] = chacha_sigma[0];
            state[1] = chacha_sigma[1];
            state[2] = chacha_sigma[2];
            state[3] = chacha_sigma[3];
            for (size_t i = 0; i < 8; ++i) {
                state[4 + i] = key[i];
            }
            state[12] = counter;
            state[13] = nonce[0];
            state[14] = nonce[1];
            state[15] = nonce[2];
        }

        // Column then diagonal round; the NEON kernel interleaves it with its own.
        bq_forceinline void run_double_round_scalar(uint32_t x[16])
        {
            quarter_round(x[0], x[4], x[8], x[12]);
            quarter_round(x[1], x[5], x[9], x[13]);
            quarter_round(x[2], x[6], x[10], x[14]);
            quarter_round(x[3], x[7], x[11], x[15]);
            quarter_round(x[0], x[5], x[10], x[15]);
            quarter_round(x[1], x[6], x[11], x[12]);
            quarter_round(x[2], x[7], x[8], x[13]);
            quarter_round(x[3], x[4], x[9], x[14]);
        }

        template <uint32_t ROUNDS>
        bq_forceinline void run_rounds_scalar(uint32_t x[16])
        {
            for (uint32_t i = 0; i < ROUNDS; i += 2) {
                run_double_round_scalar(x);
            }
        }

        template <uint32_t ROUNDS>
        void keystream_block_raw(uint8_t* out64, const uint32_t* key, const uint32_t* nonce, uint32_t counter)
        {
            uint32_t initial[16];
            build_initial_state(initial, key, nonce, counter);
            uint32_t x[16];
            for (size_t i = 0; i < 16; ++i) {
                x[i] = initial[i];
            }
            run_rounds_scalar<ROUNDS>(x);
            for (size_t i = 0; i < 16; ++i) {
                store_le32(out64 + i * 4, x[i] + initial[i]);
            }
        }

        // Portable path: also the tail handler for every SIMD path.
        template <uint32_t ROUNDS>
        void xor_blocks_scalar(uint8_t* buf, size_t len, uint64_t block_index, const uint32_t* key, const uint32_t* nonce)
        {
            uint8_t ks[chacha20::block_size];
            size_t done = 0;
            while (done < len) {
                keystream_block_raw<ROUNDS>(ks, key, nonce, static_cast<uint32_t>(block_index));
                const size_t n = (len - done < chacha20::block_size) ? (len - done) : chacha20::block_size;
                for (size_t i = 0; i < n; ++i) {
                    buf[done + i] ^= ks[i];
                }
                done += n;
                ++block_index;
            }
        }

#if defined(BQ_X86)

        // Every helper needs the target attribute too, or 32-bit x86 cannot inline the intrinsics.
        BQ_HW_SIMD_SSE2_TARGET bq_forceinline __m128i rotl_epi32_16(__m128i v)
        {
            return _mm_shufflehi_epi16(_mm_shufflelo_epi16(v, _MM_SHUFFLE(2, 3, 0, 1)), _MM_SHUFFLE(2, 3, 0, 1));
        }

        // Shifts, not pshufb: this path runs on any x86 without AVX2 and may use SSE2 only.
        BQ_HW_SIMD_SSE2_TARGET bq_forceinline __m128i rotl_epi32_8(__m128i v)
        {
            return _mm_or_si128(_mm_slli_epi32(v, 8), _mm_srli_epi32(v, 24));
        }

        BQ_HW_SIMD_SSE2_TARGET bq_forceinline __m128i rotl_epi32_12(__m128i v)
        {
            return _mm_or_si128(_mm_slli_epi32(v, 12), _mm_srli_epi32(v, 20));
        }

        BQ_HW_SIMD_SSE2_TARGET bq_forceinline __m128i rotl_epi32_7(__m128i v)
        {
            return _mm_or_si128(_mm_slli_epi32(v, 7), _mm_srli_epi32(v, 25));
        }

        BQ_HW_SIMD_SSE2_TARGET bq_forceinline void quarter_round_sse(__m128i& a, __m128i& b, __m128i& c, __m128i& d)
        {
            a = _mm_add_epi32(a, b);
            d = rotl_epi32_16(_mm_xor_si128(d, a));
            c = _mm_add_epi32(c, d);
            b = rotl_epi32_12(_mm_xor_si128(b, c));
            a = _mm_add_epi32(a, b);
            d = rotl_epi32_8(_mm_xor_si128(d, a));
            c = _mm_add_epi32(c, d);
            b = rotl_epi32_7(_mm_xor_si128(b, c));
        }

        BQ_HW_SIMD_SSE2_TARGET bq_forceinline void transpose4_epi32(__m128i& a, __m128i& b, __m128i& c, __m128i& d)
        {
            const __m128i t0 = _mm_unpacklo_epi32(a, b);
            const __m128i t1 = _mm_unpackhi_epi32(a, b);
            const __m128i t2 = _mm_unpacklo_epi32(c, d);
            const __m128i t3 = _mm_unpackhi_epi32(c, d);
            a = _mm_unpacklo_epi64(t0, t2);
            b = _mm_unpackhi_epi64(t0, t2);
            c = _mm_unpacklo_epi64(t1, t3);
            d = _mm_unpackhi_epi64(t1, t3);
        }

        // Four blocks at a time; consumes whole 256-byte chunks and leaves the rest to the caller.
        template <uint32_t ROUNDS>
        BQ_HW_SIMD_SSE2_TARGET size_t xor_blocks_sse2(uint8_t* buf, size_t len, uint64_t block_index, const uint32_t* key, const uint32_t* nonce)
        {
            constexpr size_t chunk = 4 * chacha20::block_size;
            size_t done = 0;
            while (len - done >= chunk) {
                __m128i x[16];
                x[0] = _mm_set1_epi32(static_cast<int>(chacha_sigma[0]));
                x[1] = _mm_set1_epi32(static_cast<int>(chacha_sigma[1]));
                x[2] = _mm_set1_epi32(static_cast<int>(chacha_sigma[2]));
                x[3] = _mm_set1_epi32(static_cast<int>(chacha_sigma[3]));
                for (size_t i = 0; i < 8; ++i) {
                    x[4 + i] = _mm_set1_epi32(static_cast<int>(key[i]));
                }
                x[12] = _mm_add_epi32(_mm_set1_epi32(static_cast<int>(static_cast<uint32_t>(block_index))), _mm_set_epi32(3, 2, 1, 0));
                x[13] = _mm_set1_epi32(static_cast<int>(nonce[0]));
                x[14] = _mm_set1_epi32(static_cast<int>(nonce[1]));
                x[15] = _mm_set1_epi32(static_cast<int>(nonce[2]));

                __m128i w[16];
                for (size_t i = 0; i < 16; ++i) {
                    w[i] = x[i];
                }
                for (uint32_t r = 0; r < ROUNDS; r += 2) {
                    quarter_round_sse(w[0], w[4], w[8], w[12]);
                    quarter_round_sse(w[1], w[5], w[9], w[13]);
                    quarter_round_sse(w[2], w[6], w[10], w[14]);
                    quarter_round_sse(w[3], w[7], w[11], w[15]);
                    quarter_round_sse(w[0], w[5], w[10], w[15]);
                    quarter_round_sse(w[1], w[6], w[11], w[12]);
                    quarter_round_sse(w[2], w[7], w[8], w[13]);
                    quarter_round_sse(w[3], w[4], w[9], w[14]);
                }
                for (size_t i = 0; i < 16; ++i) {
                    w[i] = _mm_add_epi32(w[i], x[i]);
                }

                // Transpose so each lane holds four consecutive words of one block.
                for (size_t k = 0; k < 16; k += 4) {
                    __m128i a = w[k];
                    __m128i b = w[k + 1];
                    __m128i c = w[k + 2];
                    __m128i d = w[k + 3];
                    transpose4_epi32(a, b, c, d);
                    uint8_t* p0 = buf + done + k * 4;
                    __m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p0));
                    __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p0 + 64));
                    __m128i v2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p0 + 128));
                    __m128i v3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p0 + 192));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(p0), _mm_xor_si128(v0, a));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(p0 + 64), _mm_xor_si128(v1, b));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(p0 + 128), _mm_xor_si128(v2, c));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(p0 + 192), _mm_xor_si128(v3, d));
                }

                done += chunk;
                block_index += 4;
            }
            return done;
        }

        static BQ_HW_SIMD_TARGET __m256i rotl_epi32_16_avx2(__m256i v)
        {
            const __m256i mask = _mm256_setr_epi8(2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13,
                2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13);
            return _mm256_shuffle_epi8(v, mask);
        }

        static BQ_HW_SIMD_TARGET __m256i rotl_epi32_8_avx2(__m256i v)
        {
            const __m256i mask = _mm256_setr_epi8(3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14,
                3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14);
            return _mm256_shuffle_epi8(v, mask);
        }

        static BQ_HW_SIMD_TARGET __m256i rotl_epi32_12_avx2(__m256i v)
        {
            return _mm256_or_si256(_mm256_slli_epi32(v, 12), _mm256_srli_epi32(v, 20));
        }

        static BQ_HW_SIMD_TARGET __m256i rotl_epi32_7_avx2(__m256i v)
        {
            return _mm256_or_si256(_mm256_slli_epi32(v, 7), _mm256_srli_epi32(v, 25));
        }

        static BQ_HW_SIMD_TARGET void quarter_round_avx2(__m256i& a, __m256i& b, __m256i& c, __m256i& d)
        {
            a = _mm256_add_epi32(a, b);
            d = rotl_epi32_16_avx2(_mm256_xor_si256(d, a));
            c = _mm256_add_epi32(c, d);
            b = rotl_epi32_12_avx2(_mm256_xor_si256(b, c));
            a = _mm256_add_epi32(a, b);
            d = rotl_epi32_8_avx2(_mm256_xor_si256(d, a));
            c = _mm256_add_epi32(c, d);
            b = rotl_epi32_7_avx2(_mm256_xor_si256(b, c));
        }

        static BQ_HW_SIMD_TARGET void transpose8_epi32(__m256i r[8])
        {
            const __m256i t0 = _mm256_unpacklo_epi32(r[0], r[1]);
            const __m256i t1 = _mm256_unpackhi_epi32(r[0], r[1]);
            const __m256i t2 = _mm256_unpacklo_epi32(r[2], r[3]);
            const __m256i t3 = _mm256_unpackhi_epi32(r[2], r[3]);
            const __m256i t4 = _mm256_unpacklo_epi32(r[4], r[5]);
            const __m256i t5 = _mm256_unpackhi_epi32(r[4], r[5]);
            const __m256i t6 = _mm256_unpacklo_epi32(r[6], r[7]);
            const __m256i t7 = _mm256_unpackhi_epi32(r[6], r[7]);

            const __m256i s0 = _mm256_unpacklo_epi64(t0, t2);
            const __m256i s1 = _mm256_unpackhi_epi64(t0, t2);
            const __m256i s2 = _mm256_unpacklo_epi64(t1, t3);
            const __m256i s3 = _mm256_unpackhi_epi64(t1, t3);
            const __m256i s4 = _mm256_unpacklo_epi64(t4, t6);
            const __m256i s5 = _mm256_unpackhi_epi64(t4, t6);
            const __m256i s6 = _mm256_unpacklo_epi64(t5, t7);
            const __m256i s7 = _mm256_unpackhi_epi64(t5, t7);

            r[0] = _mm256_permute2x128_si256(s0, s4, 0x20);
            r[1] = _mm256_permute2x128_si256(s1, s5, 0x20);
            r[2] = _mm256_permute2x128_si256(s2, s6, 0x20);
            r[3] = _mm256_permute2x128_si256(s3, s7, 0x20);
            r[4] = _mm256_permute2x128_si256(s0, s4, 0x31);
            r[5] = _mm256_permute2x128_si256(s1, s5, 0x31);
            r[6] = _mm256_permute2x128_si256(s2, s6, 0x31);
            r[7] = _mm256_permute2x128_si256(s3, s7, 0x31);
        }

        // Eight blocks at a time; consumes whole 512-byte chunks and leaves the rest to the caller.
        template <uint32_t ROUNDS>
        BQ_HW_SIMD_TARGET size_t xor_blocks_avx2(uint8_t* buf, size_t len, uint64_t block_index, const uint32_t* key, const uint32_t* nonce)
        {
            constexpr size_t chunk = 8 * chacha20::block_size;
            size_t done = 0;
            while (len - done >= chunk) {
                __m256i w[16];
                w[0] = _mm256_set1_epi32(static_cast<int>(chacha_sigma[0]));
                w[1] = _mm256_set1_epi32(static_cast<int>(chacha_sigma[1]));
                w[2] = _mm256_set1_epi32(static_cast<int>(chacha_sigma[2]));
                w[3] = _mm256_set1_epi32(static_cast<int>(chacha_sigma[3]));
                for (size_t i = 0; i < 8; ++i) {
                    w[4 + i] = _mm256_set1_epi32(static_cast<int>(key[i]));
                }
                w[12] = _mm256_add_epi32(_mm256_set1_epi32(static_cast<int>(static_cast<uint32_t>(block_index))),
                    _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
                w[13] = _mm256_set1_epi32(static_cast<int>(nonce[0]));
                w[14] = _mm256_set1_epi32(static_cast<int>(nonce[1]));
                w[15] = _mm256_set1_epi32(static_cast<int>(nonce[2]));

                __m256i x[16];
                for (size_t i = 0; i < 16; ++i) {
                    x[i] = w[i];
                }
                for (uint32_t r = 0; r < ROUNDS; r += 2) {
                    quarter_round_avx2(x[0], x[4], x[8], x[12]);
                    quarter_round_avx2(x[1], x[5], x[9], x[13]);
                    quarter_round_avx2(x[2], x[6], x[10], x[14]);
                    quarter_round_avx2(x[3], x[7], x[11], x[15]);
                    quarter_round_avx2(x[0], x[5], x[10], x[15]);
                    quarter_round_avx2(x[1], x[6], x[11], x[12]);
                    quarter_round_avx2(x[2], x[7], x[8], x[13]);
                    quarter_round_avx2(x[3], x[4], x[9], x[14]);
                }
                for (size_t i = 0; i < 16; ++i) {
                    x[i] = _mm256_add_epi32(x[i], w[i]);
                }

                // Words 0..7 and 8..15 transpose separately; each yields 32 contiguous bytes per block.
                for (size_t half = 0; half < 2; ++half) {
                    __m256i g[8];
                    for (size_t i = 0; i < 8; ++i) {
                        g[i] = x[half * 8 + i];
                    }
                    transpose8_epi32(g);
                    for (size_t lane = 0; lane < 8; ++lane) {
                        uint8_t* p = buf + done + lane * chacha20::block_size + half * 32;
                        __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
                        _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), _mm256_xor_si256(v, g[lane]));
                    }
                }

                done += chunk;
                block_index += 8;
            }
            return done;
        }
#endif

#if defined(BQ_ARM_NEON)
        bq_forceinline uint32x4_t rotl_neon_16(uint32x4_t v)
        {
            return vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(v)));
        }

        bq_forceinline uint32x4_t rotl_neon_8(uint32x4_t v)
        {
#if defined(BQ_ARM_64)
            static const uint8_t rotate8_table[16] = { 3, 0, 1, 2, 7, 4, 5, 6, 11, 8, 9, 10, 15, 12, 13, 14 };
            const uint8x16_t table = vld1q_u8(rotate8_table);
            return vreinterpretq_u32_u8(vqtbl1q_u8(vreinterpretq_u8_u32(v), table));
#else
            return vorrq_u32(vshlq_n_u32(v, 8), vshrq_n_u32(v, 24));
#endif
        }

        bq_forceinline uint32x4_t rotl_neon_12(uint32x4_t v)
        {
            return vsriq_n_u32(vshlq_n_u32(v, 12), v, 20);
        }

        bq_forceinline uint32x4_t rotl_neon_7(uint32x4_t v)
        {
            return vsriq_n_u32(vshlq_n_u32(v, 7), v, 25);
        }

        bq_forceinline void quarter_round_neon(uint32x4_t& a, uint32x4_t& b, uint32x4_t& c, uint32x4_t& d)
        {
            a = vaddq_u32(a, b);
            d = rotl_neon_16(veorq_u32(d, a));
            c = vaddq_u32(c, d);
            b = rotl_neon_12(veorq_u32(b, c));
            a = vaddq_u32(a, b);
            d = rotl_neon_8(veorq_u32(d, a));
            c = vaddq_u32(c, d);
            b = rotl_neon_7(veorq_u32(b, c));
        }

        bq_forceinline void transpose4_u32(uint32x4_t& r0, uint32x4_t& r1, uint32x4_t& r2, uint32x4_t& r3)
        {
            const uint32x4x2_t t0 = vtrnq_u32(r0, r1);
            const uint32x4x2_t t1 = vtrnq_u32(r2, r3);
            r0 = vcombine_u32(vget_low_u32(t0.val[0]), vget_low_u32(t1.val[0]));
            r1 = vcombine_u32(vget_low_u32(t0.val[1]), vget_low_u32(t1.val[1]));
            r2 = vcombine_u32(vget_high_u32(t0.val[0]), vget_high_u32(t1.val[0]));
            r3 = vcombine_u32(vget_high_u32(t0.val[1]), vget_high_u32(t1.val[1]));
        }

        // State word i for four blocks; only the counter differs.
        bq_forceinline uint32x4_t neon_initial_word(size_t word_index, uint32_t block_index,
            const uint32_t* key, const uint32_t* nonce)
        {
            if (word_index < 4) {
                return vdupq_n_u32(chacha_sigma[word_index]);
            }
            if (word_index < 12) {
                return vdupq_n_u32(key[word_index - 4]);
            }
            if (word_index == 12) {
                const uint32_t lanes[4] = { 0, 1, 2, 3 };
                return vaddq_u32(vdupq_n_u32(block_index), vld1q_u32(lanes));
            }
            // The explicit bound is for the static analyser.
            const size_t nonce_word = word_index - 13;
            if (nonce_word >= 3) {
                return vdupq_n_u32(0);
            }
            return vdupq_n_u32(nonce[nonce_word]);
        }

        bq_forceinline void neon_init_state(uint32x4_t x[16], uint32_t block_index,
            const uint32_t* key, const uint32_t* nonce)
        {
            for (size_t i = 0; i < 16; ++i) {
                x[i] = neon_initial_word(i, block_index, key, nonce);
            }
        }

        bq_forceinline void neon_double_round(uint32x4_t w[16])
        {
            quarter_round_neon(w[0], w[4], w[8], w[12]);
            quarter_round_neon(w[1], w[5], w[9], w[13]);
            quarter_round_neon(w[2], w[6], w[10], w[14]);
            quarter_round_neon(w[3], w[7], w[11], w[15]);
            quarter_round_neon(w[0], w[5], w[10], w[15]);
            quarter_round_neon(w[1], w[6], w[11], w[12]);
            quarter_round_neon(w[2], w[7], w[8], w[13]);
            quarter_round_neon(w[3], w[4], w[9], w[14]);
        }

        // The initial state is rebuilt here rather than kept live, which would spill registers.
        bq_forceinline void neon_add_and_store4(uint8_t* p, uint32x4_t w[16], size_t word_base,
            uint32_t block_index, const uint32_t* key, const uint32_t* nonce)
        {
            uint32x4_t a = vaddq_u32(w[word_base], neon_initial_word(word_base, block_index, key, nonce));
            uint32x4_t b = vaddq_u32(w[word_base + 1], neon_initial_word(word_base + 1, block_index, key, nonce));
            uint32x4_t c = vaddq_u32(w[word_base + 2], neon_initial_word(word_base + 2, block_index, key, nonce));
            uint32x4_t d = vaddq_u32(w[word_base + 3], neon_initial_word(word_base + 3, block_index, key, nonce));
            transpose4_u32(a, b, c, d);
            uint8_t* q = p + word_base * 4;
            vst1q_u8(q, veorq_u8(vld1q_u8(q), vreinterpretq_u8_u32(a)));
            vst1q_u8(q + 64, veorq_u8(vld1q_u8(q + 64), vreinterpretq_u8_u32(b)));
            vst1q_u8(q + 128, veorq_u8(vld1q_u8(q + 128), vreinterpretq_u8_u32(c)));
            vst1q_u8(q + 192, veorq_u8(vld1q_u8(q + 192), vreinterpretq_u8_u32(d)));
        }

        bq_forceinline void neon_emit4(uint8_t* p, uint32x4_t w[16], uint32_t block_index,
            const uint32_t* key, const uint32_t* nonce)
        {
            for (size_t k = 0; k < 16; k += 4) {
                neon_add_and_store4(p, w, k, block_index, key, nonce);
            }
        }

        // Eight NEON blocks plus one scalar block per iteration: the scalar block fills pipeline
        // gaps for about 10% (M4); a second one spills registers.
        template <uint32_t ROUNDS>
        size_t xor_blocks_neon(uint8_t* buf, size_t len, uint64_t block_index, const uint32_t* key, const uint32_t* nonce)
        {
            constexpr size_t vector_chunk = 8 * chacha20::block_size;
            constexpr size_t chunk = vector_chunk + chacha20::block_size;
            size_t done = 0;
            while (len - done >= chunk) {
                const uint32_t block32 = static_cast<uint32_t>(block_index);
                uint32x4_t w[16];
                uint32x4_t v[16];
                uint32_t sx[16];
                neon_init_state(w, block32, key, nonce);
                neon_init_state(v, block32 + 4U, key, nonce);
                build_initial_state(sx, key, nonce, block32 + 8U);

                for (uint32_t r = 0; r < ROUNDS; r += 2) {
                    neon_double_round(w);
                    run_double_round_scalar(sx);
                    neon_double_round(v);
                }

                neon_emit4(buf + done, w, block32, key, nonce);
                neon_emit4(buf + done + 256, v, block32 + 4U, key, nonce);

                uint32_t initial[16];
                build_initial_state(initial, key, nonce, block32 + 8U);
                uint8_t* tail = buf + done + vector_chunk;
                for (size_t i = 0; i < 16; ++i) {
                    uint32_t word;
                    memcpy(&word, tail + i * 4, sizeof(word));
                    word ^= sx[i] + initial[i];
                    memcpy(tail + i * 4, &word, sizeof(word));
                }

                done += chunk;
                block_index += 9;
            }
            return done;
        }
#endif

        template <uint32_t ROUNDS>
        void xor_keystream(uint8_t* buf, size_t len, uint64_t block_index, const uint32_t* key, const uint32_t* nonce)
        {
#if defined(BQ_X86) || defined(BQ_ARM_NEON)
            size_t done = 0;
#endif
#if defined(BQ_X86)
#ifdef BQ_UNIT_TEST
            if (chacha20::hardware_acceleration_mode_ == chacha20::mode::scalar) {
                xor_blocks_scalar<ROUNDS>(buf, len, block_index, key, nonce);
                return;
            }
            if (chacha20::hardware_acceleration_mode_ == chacha20::mode::sse2) {
                done = xor_blocks_sse2<ROUNDS>(buf, len, block_index, key, nonce);
                xor_blocks_scalar<ROUNDS>(buf + done, len - done, block_index + done / chacha20::block_size, key, nonce);
                return;
            }
            if (chacha20::hardware_acceleration_mode_ == chacha20::mode::avx2) {
                done = xor_blocks_avx2<ROUNDS>(buf, len, block_index, key, nonce);
                xor_blocks_scalar<ROUNDS>(buf + done, len - done, block_index + done / chacha20::block_size, key, nonce);
                return;
            }
#endif
            if (common_global_vars::get().avx2_support_) {
                done = xor_blocks_avx2<ROUNDS>(buf, len, block_index, key, nonce);
                xor_blocks_scalar<ROUNDS>(buf + done, len - done, block_index + done / chacha20::block_size, key, nonce);
                return;
            }
            done = xor_blocks_sse2<ROUNDS>(buf, len, block_index, key, nonce);
            xor_blocks_scalar<ROUNDS>(buf + done, len - done, block_index + done / chacha20::block_size, key, nonce);
#elif defined(BQ_ARM_NEON)
#ifdef BQ_UNIT_TEST
            if (chacha20::hardware_acceleration_mode_ == chacha20::mode::scalar) {
                xor_blocks_scalar<ROUNDS>(buf, len, block_index, key, nonce);
                return;
            }
#endif
            done = xor_blocks_neon<ROUNDS>(buf, len, block_index, key, nonce);
            xor_blocks_scalar<ROUNDS>(buf + done, len - done, block_index + done / chacha20::block_size, key, nonce);
#else
            xor_blocks_scalar<ROUNDS>(buf, len, block_index, key, nonce);
#endif
        }
    }

    void chacha20::keystream_xor(uint8_t* buf, size_t len, const uint8_t* key, const uint8_t* nonce,
        uint64_t byte_offset, round_count rounds)
    {
        if (len == 0) {
            return;
        }

        uint32_t key_words[8];
        uint32_t nonce_words[3];
        for (size_t i = 0; i < 8; ++i) {
            key_words[i] = load_le32(key + i * 4);
        }
        for (size_t i = 0; i < 3; ++i) {
            nonce_words[i] = load_le32(nonce + i * 4);
        }

        uint64_t block_index = byte_offset / block_size;
        size_t pos = static_cast<size_t>(byte_offset - block_index * block_size);

        // A misaligned start touches one block partially; everything after it is block aligned.
        if (pos != 0) {
            uint8_t ks[block_size];
            switch (rounds) {
            case round_count::rounds_8:
                keystream_block_raw<8>(ks, key_words, nonce_words, static_cast<uint32_t>(block_index));
                break;
            case round_count::rounds_12:
                keystream_block_raw<12>(ks, key_words, nonce_words, static_cast<uint32_t>(block_index));
                break;
            default:
                keystream_block_raw<20>(ks, key_words, nonce_words, static_cast<uint32_t>(block_index));
                break;
            }
            size_t n = block_size - pos;
            if (n > len) {
                n = len;
            }
            for (size_t i = 0; i < n; ++i) {
                buf[i] ^= ks[pos + i];
            }
            buf += n;
            len -= n;
            ++block_index;
        }

        if (len == 0) {
            return;
        }

        switch (rounds) {
        case round_count::rounds_8:
            xor_keystream<8>(buf, len, block_index, key_words, nonce_words);
            break;
        case round_count::rounds_12:
            xor_keystream<12>(buf, len, block_index, key_words, nonce_words);
            break;
        default:
            xor_keystream<20>(buf, len, block_index, key_words, nonce_words);
            break;
        }
    }

    void chacha20::keystream_block(uint8_t* out64, const uint8_t* key, const uint8_t* nonce,
        uint64_t block_index, round_count rounds)
    {
        uint32_t key_words[8];
        uint32_t nonce_words[3];
        for (size_t i = 0; i < 8; ++i) {
            key_words[i] = load_le32(key + i * 4);
        }
        for (size_t i = 0; i < 3; ++i) {
            nonce_words[i] = load_le32(nonce + i * 4);
        }
        switch (rounds) {
        case round_count::rounds_8:
            keystream_block_raw<8>(out64, key_words, nonce_words, static_cast<uint32_t>(block_index));
            break;
        case round_count::rounds_12:
            keystream_block_raw<12>(out64, key_words, nonce_words, static_cast<uint32_t>(block_index));
            break;
        default:
            keystream_block_raw<20>(out64, key_words, nonce_words, static_cast<uint32_t>(block_index));
            break;
        }
    }
}
