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
#include "legacy/legacy_decoders.h"
#include "legacy/crypto/aes.h"
#include "legacy/crypto/vernam.h"

namespace bq {
    namespace {
        // The v9/v10 encryption head, before the payload: [fingerprint] + RSA-2048 wrapped AES key +
        // AES-CBC IV + the XOR blob encrypted with that key. The blob is 32 KiB because the mask
        // repeats over a power-of-two window; it is the whole reason these formats are gone.
        constexpr size_t legacy_xor_key_blob_size = 32 * 1024;

        size_t legacy_encryption_head_size(size_t fingerprint_size)
        {
            return fingerprint_size + 256 // RSA-2048 ciphertext of the AES key
                + 16 // AES IV, in the clear
                + legacy_xor_key_blob_size; // AES-CBC encrypted XOR key blob
        }
    }

    bool appender_decoder_legacy::is_supported_enc_type(appender_file_binary::appender_encryption_type type) const
    {
        return type == appender_file_binary::appender_encryption_type::plaintext
            || type == appender_file_binary::appender_encryption_type::rsa_aes_xor;
    }

    bool appender_decoder_legacy::setup_segment(const appender_file_binary::appender_file_segment_head& head, uint64_t seg_start)
    {
        masked_ = false;
        payload_base_ = seg_start + sizeof(appender_file_binary::appender_file_segment_head);
        if (head.enc_type == appender_file_binary::appender_encryption_type::plaintext) {
            return true;
        }
        if (!segment_carries_key(head)) {
            // A v9 file writes the wrapped key once, in its first segment, and every later segment
            // reuses it. A file that starts with a keyless encrypted segment has no key at all.
            if (xor_key_blob_.is_empty()) {
                bq::util::log_device_console(bq::log_level::error, "decode log file failed, encrypted segment carries no key and no earlier segment had one");
                return false;
            }
            masked_ = true;
            return true;
        }
        if (private_key_.n_.size() != 256) {
            bq::util::log_device_console(bq::log_level::error, "It's an encrypted log file, private key is not specified");
            return false;
        }
        const size_t head_size = legacy_encryption_head_size(fingerprint_size());
        bq::array<uint8_t> enc_data;
        enc_data.fill_uninitialized(head_size);
        if (read_from_file_directly(enc_data.begin(), head_size) < head_size) {
            bq::util::log_device_console(bq::log_level::error, "decode log file failed, read encryption info failed");
            return false;
        }
        const uint8_t* cursor = enc_data.begin() + static_cast<ptrdiff_t>(fingerprint_size());

        aes::key_type aes_key;
        aes::iv_type aes_iv;
        aes::key_type aes_key_ciphertext;
        aes aes_obj(aes::enum_cipher_mode::AES_CBC, aes::enum_key_bits::AES_256);
        const size_t aes_key_ciphertext_size = private_key_.n_.size();
        const size_t aes_iv_size = aes_obj.get_iv_size();
        aes_key_ciphertext.insert_batch(aes_key_ciphertext.end(), cursor, aes_key_ciphertext_size);
        aes_iv.insert_batch(aes_iv.end(), cursor + static_cast<ptrdiff_t>(aes_key_ciphertext_size), aes_iv_size);
        if (!rsa::decrypt(private_key_, aes_key_ciphertext, aes_key)) {
            bq::util::log_device_console(bq::log_level::error, "decode log file failed, decrypt aes key failed, please check your private key");
            return false;
        }
        xor_key_blob_.clear();
        xor_key_blob_.fill_uninitialized(legacy_xor_key_blob_size);
        if (!aes_obj.decrypt(aes_key, aes_iv,
                cursor + static_cast<ptrdiff_t>(aes_key_ciphertext_size + aes_iv_size),
                legacy_xor_key_blob_size, xor_key_blob_.begin(), legacy_xor_key_blob_size)) {
            bq::util::log_device_console(bq::log_level::error, "decode log file failed, decrypt the XOR key blob failed");
            return false;
        }
        bq::encryption::secure_zero(aes_key.begin(), aes_key.size());
        payload_base_ = seg_start + sizeof(appender_file_binary::appender_file_segment_head) + head_size;
        masked_ = true;
        return true;
    }

    bool appender_decoder_legacy::decrypt_segment_chunk(uint8_t* data, size_t size, uint64_t file_pos)
    {
        if (!masked_) {
            return true;
        }
        // Addressed by absolute file position, and continuity across segments is the point: a
        // segment that carries no key continues the previous one's mask rather than restarting it.
        // The mask is periodic, so every position is valid and there is nothing to reject.
        //
        // The period is the size of the key blob, 32 KiB, so the low bits of the position are the
        // whole address: the truncation a 32-bit build performs here cannot reach a different mask
        // byte, and saying so keeps it from being a silent narrowing.
        vernam::vernam_encrypt_32bytes_aligned(data, size, xor_key_blob_.begin(), xor_key_blob_.size(),
            static_cast<size_t>(file_pos & (legacy_xor_key_blob_size - 1U)));
        return true;
    }

    size_t appender_decoder_legacy::get_read_front_padding(uint64_t file_pos) const
    {
        if (!masked_) {
            return 0;
        }
        return static_cast<size_t>(file_pos % buffer_alignment);
    }

    bool appender_decoder_legacy_v9::segment_carries_key(const appender_file_binary::appender_file_segment_head& head) const
    {
        // v9 wrote a bool at segment +0x0A and two bytes of padding after it. The field is gone from
        // the structure this build compiles against, so it is read at its byte offset - the header is
        // packed and asserted to 12 bytes, which is what makes the offset stable.
        const uint8_t has_key_byte = reinterpret_cast<const uint8_t*>(&head)[offsetof(appender_file_binary::appender_file_segment_head, padding) + 0];
        return head.enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor && has_key_byte != 0;
    }
}
