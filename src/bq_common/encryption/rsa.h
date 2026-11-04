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
 * \file rsa.h
 * \date 2025/08/18 01:22
 *
 * \author pippocao
 *
 * \brief
 * Simple RSA implementation (raw, no padding) with key parsing compatible with ssh-keygen:
 * - Public key: OpenSSH "ssh-rsa AAAA..." format
 * - Private key: PKCS#1 PEM "-----BEGIN RSA PRIVATE KEY-----"
 *
 */
#pragma once

#include "bq_common/bq_common.h"

namespace bq {

    class rsa {
    public:
        struct public_key {
            bq::array<uint8_t> n_;
            bq::array<uint8_t> e_;
            bool operator==(const public_key& other) const
            {
                return n_ == other.n_ && e_ == other.e_;
            }
            bool operator!=(const public_key& other) const
            {
                return !(*this == other);
            }
        };
        struct private_key {
            bq::array<uint8_t> n_;
            bq::array<uint8_t> e_;
            bq::array<uint8_t> d_;
            bq::array<uint8_t> p_;
            bq::array<uint8_t> q_;
            bq::array<uint8_t> dp_;
            bq::array<uint8_t> dq_;
            bq::array<uint8_t> qinv_;
            bool operator==(const private_key& other) const
            {
                return n_ == other.n_ && e_ == other.e_ && d_ == other.d_
                    && p_ == other.p_ && q_ == other.q_
                    && dp_ == other.dp_ && dq_ == other.dq_ && qinv_ == other.qinv_;
            }
            bool operator!=(const private_key& other) const
            {
                return !(*this == other);
            }
        };

    public:
        // Parse OpenSSH public key text (bq::string)
        static bool parse_public_key_ssh(const bq::string& ssh_pub_text, public_key& out);

        // The segment format writes the wrapped key into a fixed 256-byte field, so the key has to be
        // exactly 2048 bits: a smaller modulus leaves the field misaligned and the file cannot be
        // opened, and a larger one does not fit at all. The exponent is checked for the same reason
        // every other RSA implementation does - a small or even one is not a usable public exponent.
        // Rejecting here means the appender never comes into existence instead of producing files
        // nobody can read.
        static bool validate_public_key(const public_key& key);

        // Parse PKCS#1 PEM private key text (bq::string)
        static bool parse_private_key_pem(const bq::string& pem, private_key& out);

        static uint64_t get_public_key_fingerprint(const public_key& key);

        // The same value, from the two numbers directly. A decoder holds a private_key and must reach
        // the public fingerprint without building a public_key first.
        static uint64_t get_public_key_fingerprint(const bq::array<uint8_t>& n, const bq::array<uint8_t>& e);

        // Encrypt: output ciphertext is k bytes, plaintext must be integer < modulus
        static bool encrypt(const public_key& pub,
            const bq::array<uint8_t>& plaintext,
            bq::array<uint8_t>& out_ciphertext);

        // Decrypt: output minimal big-endian plaintext
        static bool decrypt(const private_key& pri,
            const bq::array<uint8_t>& ciphertext,
            bq::array<uint8_t>& out_plaintext);

        // RSA-KEM (ISO/IEC 18033-2, the HKDF instance): the raw RSA transform of a random integer,
        // with the KDF applied outside. Both outputs are exactly exponent_of_modulus bytes.
        //
        // Deliberately not built on encrypt/decrypt: those hand back the plaintext in minimal
        // big-endian form, and a shared secret whose leading byte happens to be zero would come back
        // one byte short, changing every key derived from it. Here the width is part of the format.
        static bool kem_encapsulate(const public_key& pub,
            bq::array<uint8_t>& out_ciphertext,
            bq::array<uint8_t>& out_shared_secret);
        static bool kem_decapsulate(const private_key& pri,
            const bq::array<uint8_t>& ciphertext,
            bq::array<uint8_t>& out_shared_secret);

#if defined(BQ_UNIT_TEST)
        // Test-only. The double-and-add modexp the Montgomery code replaced, kept so the tests can
        // compare the two on random inputs and boundary values rather than on fixed vectors.
        static bool test_mod_pow_reference(const bq::array<uint8_t>& a,
            const bq::array<uint8_t>& e,
            const bq::array<uint8_t>& n,
            bq::array<uint8_t>& out);

        // Test-only. The Montgomery path on its own: rsa::encrypt cannot express a == 0 or a == 1,
        // which are exactly the boundary values the comparison needs.
        static bool test_mod_pow_mont(const bq::array<uint8_t>& a,
            const bq::array<uint8_t>& e,
            const bq::array<uint8_t>& n,
            bq::array<uint8_t>& out);
#endif
    };

}
