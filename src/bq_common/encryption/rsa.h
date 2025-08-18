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

#include "bq_common/bq_common.h"

namespace bq {

    class rsa {
    public:
        struct public_key {
            bq::array<uint8_t> n_;
            bq::array<uint8_t> e_;
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
        };

    public:
        // Parse OpenSSH public key text (bq::string)
        static bool parse_public_key_ssh(const bq::string& ssh_pub_text, public_key& out);

        // Parse PKCS#1 PEM private key text (bq::string)
        static bool parse_private_key_pem(const bq::string& pem, private_key& out);

        // Encrypt: output ciphertext is k bytes, plaintext must be integer < modulus
        static bool encrypt(const public_key& pub,
            const bq::array<uint8_t>& plaintext,
            bq::array<uint8_t>& out_ciphertext);

        // Decrypt: output minimal big-endian plaintext
        static bool decrypt(const private_key& pri,
            const bq::array<uint8_t>& ciphertext,
            bq::array<uint8_t>& out_plaintext);
    };

}