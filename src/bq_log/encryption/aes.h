#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * \file aes.h
 * \date 2025/08/15 22:05
 *
 * \author pippocao
 *
 * \brief 
 *
 * Simple AES implementation supporting ECB, CBC, CFB, OFB, CTR modes with 128, 192, and 256-bit keys.
 *
*/


#include <stdlib.h>
#include "bq_common/bq_common.h"

namespace bq {
	enum class aes_cipher_mode {
		AES_ECB = 0,  // Electronic Codebook
		AES_CBC = 1,  // Cipher Block Chaining
		AES_CFB = 2,  // Cipher Feedback
		AES_OFB = 3,  // Output Feedback
		AES_CTR = 4   // Counter Mode
	};

	enum class aes_key_bits {
		AES_128 = 128,
		AES_192 = 192,
		AES_256 = 256
	};

	template<size_t SIZE>
	struct aes_array {
	public:
		using data_type = uint8_t[SIZE];
	private:
		data_type data_;
	public:
		aes_array() {
			for (size_t i = 0; i < SIZE; ++i) {
				data_[i] = 0;
			}
		}
		aes_array(const uint8_t* key) {
			for (size_t i = 0; i < SIZE; ++i) {
				data_[i] = key[i];
			}
		}
		aes_array(const aes_array<SIZE>& rhs) {
			for (size_t i = 0; i < SIZE; ++i) {
				data_[i] = rhs.data_[i];
			}
		}
		const data_type& get_data() const {
			return data_;
		}
		data_type& get_data() {
			return data_;
		}
		const uint8_t& operator[](size_t index) const {
			return data_[index];
		}
		uint8_t& operator[](size_t index) {
			return data_[index];
		}
	};

	template<aes_key_bits KeyBits>
	struct aes_key : public aes_array<static_cast<size_t>(KeyBits)/ 8> {
		constexpr static size_t key_size = static_cast<size_t>(KeyBits) / 8;
		using aes_array<key_size>::aes_array;
		using aes_array<key_size>::operator[];
	};

	template<size_t SIZE>
	struct aes_iv : public aes_array<SIZE> {
		using aes_array<SIZE>::aes_array;
		using aes_array<SIZE>::operator[];
	};

    template<aes_cipher_mode Mode, aes_key_bits KeyBits>
	class aes {
		static_assert(Mode == aes_cipher_mode::AES_CBC, "Only CBC mode is supported in this implementation.");
	public:
		using key_type = aes_key<KeyBits>;
		constexpr static size_t key_size = key_type::key_size;
		constexpr static size_t iv_size = bq::condition_value<Mode == aes_cipher_mode::AES_ECB, size_t, 0,
			bq::condition_value<Mode == aes_cipher_mode::AES_CBC, size_t, 16,
			bq::condition_value<Mode == aes_cipher_mode::AES_CFB, size_t, 16,
			bq::condition_value<Mode == aes_cipher_mode::AES_OFB, size_t, 16,
			bq::condition_value<Mode == aes_cipher_mode::AES_CTR, size_t, 16, 0>::value>::value>::value>::value>::value;
		using iv_type = aes_iv<iv_size>;

	public:
		aes();

		key_type generate_key() const;

		iv_type generate_iv() const;

		bool encrypt(const key_type& key, const iv_type& iv, const bq::array<uint8_t>& plaintext, bq::array<uint8_t>& out_ciphertext) const;
		bool decrypt(const key_type& key, const iv_type& iv, const bq::array<uint8_t>& ciphertext, bq::array<uint8_t>& out_plaintext) const;
    
	private:
		struct aes_core {
			// AES parameters derived from KeyBits
			constexpr static size_t nb = 4;
			constexpr static size_t key_size = key_type::key_size;
			constexpr static size_t nk = key_size / 4;
			constexpr static size_t nr = nk + 6;
			constexpr static size_t round_key_words = nb * (nr + 1);
			constexpr static size_t round_key_bytes = round_key_words * 4;

			aes_array<round_key_bytes> round_key_;

			static constexpr uint8_t xtime(uint8_t x) {
				return static_cast<uint8_t>((x << 1) ^ ((x & 0x80) ? 0x1b : 0x00));
			}

			static uint8_t gf_mul(uint8_t a, uint8_t b) {
				uint8_t p = 0;
				for (int32_t i = 0; i < 8; ++i) {
					if (b & 1) p ^= a;
					uint8_t hi = a & 0x80;
					a <<= 1;
					if (hi) a ^= 0x1b;
					b >>= 1;
				}
				return p;
			}

			static inline void xor_block(uint8_t* dst, const uint8_t* a, const uint8_t* b) {
				for (int32_t i = 0; i < 16; ++i) dst[i] = static_cast<uint8_t>(a[i] ^ b[i]);
			}

			static inline void add_round_key(uint8_t* state, const uint8_t* round_key) {
				for (int32_t i = 0; i < 16; ++i) state[i] ^= round_key[i];
			}

			static inline uint8_t rcon(uint8_t i) {
				uint8_t c = 1;
				if (i == 0) return 0;
				while (i > 1) { c = xtime(c); --i; }
				return c;
			}

			static constexpr uint8_t sbox_[256] = {
				0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
				0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
				0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
				0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
				0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
				0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
				0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
				0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
				0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
				0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
				0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
				0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
				0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
				0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
				0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
				0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
			};
			static constexpr uint8_t rsbox_[256] = {
					0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
					0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
					0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
					0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
					0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
					0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
					0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
					0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
					0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
					0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
					0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
					0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
					0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
					0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
					0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
					0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
			};

			template<size_t NK_SIZE>
			bq::enable_if_t<(NK_SIZE > 6), void> aes_256_ext_subword(uint8_t (&temp)[4]) {
				temp[0] = sbox_[temp[0]];
				temp[1] = sbox_[temp[1]];
				temp[2] = sbox_[temp[2]];
				temp[3] = sbox_[temp[3]];
			}

			template<size_t NK_SIZE>
			bq::enable_if_t<(NK_SIZE <= 6), void> aes_256_ext_subword(uint8_t (&temp)[4]) {
				(void)temp;
			}

			void key_expansion(const key_type& key) {
				// first nk words (key)
				for (size_t i = 0; i < nk; ++i) {
					round_key_[4 * i + 0] = key[4 * i + 0];
					round_key_[4 * i + 1] = key[4 * i + 1];
					round_key_[4 * i + 2] = key[4 * i + 2];
					round_key_[4 * i + 3] = key[4 * i + 3];
				}
				uint8_t temp[4];
				for (size_t i = nk; i < round_key_words; ++i) {
					temp[0] = round_key_[4 * (i - 1) + 0];
					temp[1] = round_key_[4 * (i - 1) + 1];
					temp[2] = round_key_[4 * (i - 1) + 2];
					temp[3] = round_key_[4 * (i - 1) + 3];

					if (i % nk == 0) {
						// RotWord
						uint8_t t = temp[0];
						temp[0] = temp[1]; temp[1] = temp[2]; temp[2] = temp[3]; temp[3] = t;
						// SubWord
						temp[0] = sbox_[temp[0]];
						temp[1] = sbox_[temp[1]];
						temp[2] = sbox_[temp[2]];
						temp[3] = sbox_[temp[3]];
						// Rcon
						temp[0] ^= rcon(static_cast<uint8_t>(i / nk));
					}
					else if (i % nk == 4) {
						aes_256_ext_subword<nk>(temp);
					}

					round_key_[4 * i + 0] = static_cast<uint8_t>(round_key_[4 * (i - nk) + 0] ^ temp[0]);
					round_key_[4 * i + 1] = static_cast<uint8_t>(round_key_[4 * (i - nk) + 1] ^ temp[1]);
					round_key_[4 * i + 2] = static_cast<uint8_t>(round_key_[4 * (i - nk) + 2] ^ temp[2]);
					round_key_[4 * i + 3] = static_cast<uint8_t>(round_key_[4 * (i - nk) + 3] ^ temp[3]);
				}
			}

			static inline void sub_bytes(uint8_t* s) {
				for (int32_t i = 0; i < 16; ++i) {
					s[i] = sbox_[s[i]];
				}
			}
			static inline void inv_sub_bytes(uint8_t* s) {
				for (int32_t i = 0; i < 16; ++i) {
					s[i] = rsbox_[s[i]];
				}
			}

			static inline void shift_rows(uint8_t* s) {
				uint8_t t;
				// row 1
				t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
				// row 2
				t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
				// row 3
				t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
			}
			static inline void inv_shift_rows(uint8_t* s) {
				uint8_t t;
				// row 1
				t = s[13]; s[13] = s[9]; s[9] = s[5]; s[5] = s[1]; s[1] = t;
				// row 2
				t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
				// row 3
				t = s[3]; s[3] = s[7]; s[7] = s[11]; s[11] = s[15]; s[15] = t;
			}

			static inline void mix_columns(uint8_t* s) {
				for (int32_t c = 0; c < 4; ++c) {
					int32_t i = 4 * c;
					uint8_t a0 = s[i + 0], a1 = s[i + 1], a2 = s[i + 2], a3 = s[i + 3];
					uint8_t t = static_cast<uint8_t>(a0 ^ a1 ^ a2 ^ a3);
					uint8_t u = a0;
					s[i + 0] ^= static_cast<uint8_t>(t ^ xtime(static_cast<uint8_t>(a0 ^ a1)));
					s[i + 1] ^= static_cast<uint8_t>(t ^ xtime(static_cast<uint8_t>(a1 ^ a2)));
					s[i + 2] ^= static_cast<uint8_t>(t ^ xtime(static_cast<uint8_t>(a2 ^ a3)));
					s[i + 3] ^= static_cast<uint8_t>(t ^ xtime(static_cast<uint8_t>(a3 ^ u)));
				}
			}
			static inline void inv_mix_columns(uint8_t* s) {
				for (int32_t c = 0; c < 4; ++c) {
					int32_t i = 4 * c;
					uint8_t a0 = s[i + 0], a1 = s[i + 1], a2 = s[i + 2], a3 = s[i + 3];
					s[i + 0] = static_cast<uint8_t>(gf_mul(a0, 0x0e) ^ gf_mul(a1, 0x0b) ^ gf_mul(a2, 0x0d) ^ gf_mul(a3, 0x09));
					s[i + 1] = static_cast<uint8_t>(gf_mul(a0, 0x09) ^ gf_mul(a1, 0x0e) ^ gf_mul(a2, 0x0b) ^ gf_mul(a3, 0x0d));
					s[i + 2] = static_cast<uint8_t>(gf_mul(a0, 0x0d) ^ gf_mul(a1, 0x09) ^ gf_mul(a2, 0x0e) ^ gf_mul(a3, 0x0b));
					s[i + 3] = static_cast<uint8_t>(gf_mul(a0, 0x0b) ^ gf_mul(a1, 0x0d) ^ gf_mul(a2, 0x09) ^ gf_mul(a3, 0x0e));
				}
			}

			void encrypt_block(uint8_t* block) const {
				uint8_t state[16];
				memcpy(state, block, 16);

				add_round_key(state, round_key_.get_data());

				for (size_t round = 1; round < nr; ++round) {
					sub_bytes(state);
					shift_rows(state);
					mix_columns(state);
					add_round_key(state, round_key_.get_data() + (16 * round));
				}

				sub_bytes(state);
				shift_rows(state);
				add_round_key(state, round_key_.get_data() + (16 * nr));

				memcpy(block, state, 16);
			}

			void decrypt_block(uint8_t* block) const {
				uint8_t state[16];
				memcpy(state, block, 16);

				add_round_key(state, round_key_.get_data() + (16 * nr));

				for (size_t round = nr; round-- > 1; ) {
					inv_shift_rows(state);
					inv_sub_bytes(state);
					add_round_key(state, round_key_.get_data() + (16 * round));
					inv_mix_columns(state);
				}

				inv_shift_rows(state);
				inv_sub_bytes(state);
				add_round_key(state, round_key_.get_data());

				memcpy(block, state, 16);
			}
		};

	private:
		// helper to fill random bytes
		template <size_t N>
		static void fill_random(uint8_t(&out)[N]) {
			uint32_t seed = static_cast<uint32_t>(bq::platform::high_performance_epoch_ms() % UINT32_MAX);
			bq::util::srand(seed);
			for (size_t i = 0; i < N; ++i) {
				out[i] = static_cast<uint8_t>(bq::util::rand() % UINT8_MAX);
			}
		}

		static bool is_block_aligned(size_t n) {
			return (n % 16u) == 0u;
		}
	};

	// -------------------- Implementation --------------------
	template <aes_cipher_mode Mode, aes_key_bits KeyBits>
	inline aes<Mode, KeyBits>::aes() {}

	template <aes_cipher_mode Mode, aes_key_bits KeyBits>
	inline typename aes<Mode, KeyBits>::key_type aes<Mode, KeyBits>::generate_key() const {
		uint8_t tmp[key_size];
		fill_random(tmp);
		return key_type(tmp);
	}

	template <aes_cipher_mode Mode, aes_key_bits KeyBits>
	inline typename aes<Mode, KeyBits>::iv_type aes<Mode, KeyBits>::generate_iv() const {
		uint8_t tmp[iv_size];
		fill_random(tmp);
		return iv_type(tmp);
	}

	template <aes_cipher_mode Mode, aes_key_bits KeyBits>
	inline bool aes<Mode, KeyBits>::encrypt(const key_type& key, const iv_type& iv, const bq::array<uint8_t>& plaintext, bq::array<uint8_t>& out_ciphertext) const {
		const size_t n = plaintext.size();
		if (!is_block_aligned(n)) {
			return false; // no padding; caller must align length to 16
		}

		out_ciphertext.clear();
		if (n == 0) return true;
		// in-place over output
		out_ciphertext.insert_batch(out_ciphertext.end(), plaintext.begin(), plaintext.size());

		// prepare key schedule
		aes_core core;
		core.key_expansion(key);

		// working buffers
		uint8_t chain[iv_size];
		memcpy(chain, iv.get_data(), iv_size);

		uint8_t* buf = (uint8_t*)out_ciphertext.begin();

		for (size_t i = 0; i < n; i += 16) {
			aes_core::xor_block(buf + i, buf + i, chain);
			core.encrypt_block(buf + i);
			memcpy(chain, buf + i, 16);
		}
		return true;
	}

	template <aes_cipher_mode Mode, aes_key_bits KeyBits>
	inline bool aes<Mode, KeyBits>::decrypt(const key_type& key, const iv_type& iv, const bq::array<uint8_t>& ciphertext, bq::array<uint8_t>& out_plaintext) const {
		const std::size_t n = ciphertext.size();
		if (!is_block_aligned(n)) {
			return false; // no padding; caller must align length to 16
		}

		out_plaintext.clear();
		if (n == 0) return true;
		// in-place over output
		out_plaintext.insert_batch(out_plaintext.end(), ciphertext.begin(), ciphertext.size());

		// prepare key schedule
		aes_core core{};
		core.key_expansion(key);

		// working buffers
		uint8_t prev[16];
		uint8_t cur[16];
		memcpy(prev, iv.get_data(), 16);

		uint8_t* buf = (uint8_t*)out_plaintext.begin();

		for (std::size_t i = 0; i < n; i += 16) {
			memcpy(cur, buf + i, 16);
			core.decrypt_block(buf + i);
			aes_core::xor_block(buf + i, buf + i, prev);
			memcpy(prev, cur, 16);
		}
		return true;
	}
}
