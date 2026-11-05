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
#include "bq_log/log/appender/appender_file_binary.h"
#include "bq_log/log/log_imp.h"

namespace bq {
    namespace {
        // The HKDF info string of the v11 format. Its total length is asserted because a silently
        // dropped or added byte here would derive a different key on read than on write, and nothing
        // would report it: the segment would just decode to noise.
        const char segment_key_info_prefix[] = "BqLog v11 segment key";
        constexpr size_t segment_key_info_size = sizeof(segment_key_info_prefix) - 1 // no terminating NUL
            + sizeof(uint32_t) // format version
            + sizeof(uint8_t) // enc_type
            + sizeof(uint8_t) // chacha_rounds
            + sizeof(uint64_t) // rsa_key_fingerprint
            + appender_file_binary::kem_ciphertext_size;
        static_assert(segment_key_info_size == 291, "segment key info layout changed");
    }

    bool appender_file_binary::derive_segment_key(const appender_segment_encryption_head& head,
        const uint8_t* shared_secret, size_t shared_secret_size, segment_key& out)
    {
        assert(shared_secret_size == kem_ciphertext_size && "the KEM shared secret is a fixed 256 bytes");
        if (shared_secret_size != kem_ciphertext_size) {
            return false;
        }
        uint8_t info[segment_key_info_size];
        uint8_t* cursor = info;
        memcpy(cursor, segment_key_info_prefix, sizeof(segment_key_info_prefix) - 1);
        cursor += sizeof(segment_key_info_prefix) - 1;
        const uint32_t version = format_version;
        memcpy(cursor, &version, sizeof(version));
        cursor += sizeof(version);
        *cursor = static_cast<uint8_t>(appender_encryption_type::rsa_kem_chacha);
        ++cursor;
        *cursor = head.chacha_rounds;
        ++cursor;
        memcpy(cursor, &head.rsa_key_fingerprint, sizeof(head.rsa_key_fingerprint));
        cursor += sizeof(head.rsa_key_fingerprint);
        memcpy(cursor, head.kem_ciphertext, kem_ciphertext_size);
        cursor += kem_ciphertext_size;
        assert(static_cast<size_t>(cursor - info) == segment_key_info_size);

        uint8_t okm[chacha20::key_size + chacha20::nonce_size];
        const bool ok = hkdf_sha256::derive(nullptr, 0, shared_secret, shared_secret_size,
            info, sizeof(info), okm, sizeof(okm));
        if (ok) {
            memcpy(out.key, okm, sizeof(out.key));
            memcpy(out.nonce, okm + sizeof(out.key), sizeof(out.nonce));
        }
        // okm is the segment key in the clear. The caller clears the shared secret, which it owns.
        memset(okm, 0, sizeof(okm));
        return ok;
    }

    bool appender_file_binary::derive_segment_key_from_head(const appender_segment_encryption_head& head,
        const rsa::private_key& key, segment_key& out, uint8_t& rounds)
    {
        // Checked before anything is decrypted. It is the only thing that can tell a wrong key from a
        // right one: decapsulation succeeds for any c < n, so without this a wrong key would produce
        // noise that decodes as far as the first record and then looks like a corrupt file.
        if (head.rsa_key_fingerprint != rsa::get_public_key_fingerprint(key.n_, key.e_)) {
            bq::util::log_device_console(bq::log_level::error, "decode log file failed, private key does not match the public key this file was encrypted with");
            return false;
        }
        // A round count outside the set the kernel implements would decode to noise rather than fail,
        // so it is rejected here instead of deeper in the cipher.
        if (!is_supported_chacha_rounds(head.chacha_rounds)) {
            bq::util::log_device_console(bq::log_level::error, "decode log file failed, unsupported chacha round count %" PRIu32,
                static_cast<uint32_t>(head.chacha_rounds));
            return false;
        }
        bq::array<uint8_t> kem_ciphertext;
        kem_ciphertext.insert_batch(kem_ciphertext.end(), head.kem_ciphertext, kem_ciphertext_size);
        bq::array<uint8_t> shared_secret;
        if (!rsa::kem_decapsulate(key, kem_ciphertext, shared_secret)
            || shared_secret.size() != kem_ciphertext_size) {
            bq::util::log_device_console(bq::log_level::error, "decode log file failed, RSA-KEM decapsulation failed");
            return false;
        }
        const bool derived = derive_segment_key(head, shared_secret.begin(), shared_secret.size(), out);
        bq::encryption::secure_zero(shared_secret.begin(), shared_secret.size());
        if (!derived) {
            bq::util::log_device_console(bq::log_level::error, "decode log file failed, segment key derivation failed");
            return false;
        }
        rounds = head.chacha_rounds;
        return true;
    }

    void appender_file_binary::transform_segment_payload(uint8_t* data, size_t size, uint64_t payload_offset,
        const segment_key& key, uint8_t rounds)
    {
        assert(is_supported_chacha_rounds(rounds) && "unsupported round count reached the transform");
        chacha20::keystream_xor(data, size, key.key, key.nonce, payload_offset,
            static_cast<chacha20::round_count>(rounds));
    }

    bool appender_file_binary::init_impl(const bq::property_value& config_obj)
    {
        if (config_obj["pub_key"].is_string()) {
            enc_type_ = appender_encryption_type::rsa_kem_chacha;
            bq::string pub_key_str = config_obj["pub_key"];
            if (!rsa::parse_public_key_ssh(pub_key_str, rsa_pub_key_)) {
                return false;
            }
            rsa_key_fingerprint_ = rsa::get_public_key_fingerprint(rsa_pub_key_);
            if (!rsa::validate_public_key(rsa_pub_key_)) {
                bq::util::log_device_console(bq::log_level::error,
                    "encrypted appender rejected the public key; the segment format requires a 2048-bit RSA key with an exponent of at least 65537");
                return false;
            }
            // Encryption goes ahead without a CSPRNG, and the segment keys then come from a mix of
            // clocks, thread ids and addresses that anyone holding the file can search. That is
            // worth one warning and not worth dropping the log for: the records stay encrypted, the
            // threat model narrows. Console platforms inject a source to avoid it altogether.
            if (!bq::encryption::has_secure_random_source()) {
                bq::util::log_device_console(bq::log_level::warning,
                    "encrypted appender has no secure random source; segment keys are derived from a software mix and are not cryptographically random. On console platforms call bq::log::set_secure_random_source() before creating the log");
                warned_weak_key_source_ = true;
            }
        } else {
            enc_type_ = appender_encryption_type::plaintext;
            rsa_pub_key_ = rsa::public_key();
            rsa_key_fingerprint_ = 0;
        }
        // Set before the base runs: init_impl() there may go through recovery, which flushes, and a
        // flush that skipped the transform would put plaintext into a segment header that says
        // otherwise.
        transform_enabled_ = (enc_type_ != appender_encryption_type::plaintext);
        if (!appender_file_base::init_impl(config_obj)) {
            return false;
        }
        return true;
    }

    bool appender_file_binary::reset_impl(const bq::property_value& config_obj)
    {
        if (!appender_file_base::reset_impl(config_obj)) {
            return false;
        }
        rsa::public_key prev_pub_key = rsa_pub_key_;
        auto prev_encryption_type = enc_type_;
        if (config_obj["pub_key"].is_string()) {
            enc_type_ = appender_encryption_type::rsa_kem_chacha;
            bq::string pub_key_str = config_obj["pub_key"];
            if (!rsa::parse_public_key_ssh(pub_key_str, rsa_pub_key_)) {
                return false;
            }
            rsa_key_fingerprint_ = rsa::get_public_key_fingerprint(rsa_pub_key_);
        } else {
            enc_type_ = appender_encryption_type::plaintext;
            rsa_pub_key_ = rsa::public_key();
            rsa_key_fingerprint_ = 0;
        }
        transform_enabled_ = (enc_type_ != appender_encryption_type::plaintext);
        return (enc_type_ == prev_encryption_type)
            && (rsa_pub_key_ == prev_pub_key);
    }

    bool appender_file_binary::parse_exist_log_file(parse_file_context& context)
    {
        const uint32_t expected_version = get_binary_format_version();

        segment_topology_info topology;
        if (enc_type_ != appender_encryption_type::plaintext
            || !scan_segment_topology(topology)
            || !topology.has_segment
            || !topology.all_segments_are_plaintext) {
            context.log_parse_fail_reason("binary log segment topology is incompatible with plaintext reuse");
            return false;
        }
        seek_read_file_absolute(0);

        // parse file type
        auto read_handle = appender_file_base::read_with_cache(sizeof(appender_file_header));
        if (read_handle.len() < sizeof(appender_file_header)) {
            context.log_parse_fail_reason("read appender_file_header failed");
            return false;
        }
        appender_file_header file_head;
        memcpy(&file_head, read_handle.data(), sizeof(file_head));
        if (file_head.version != expected_version) {
            context.log_parse_fail_reason("format version incompatible");
            return false;
        }
        if (file_head.format != get_appender_format()) {
            context.log_parse_fail_reason("format incompatible");
            return false;
        }
        // parse first segment information
        cur_read_seg_.end_pos = static_cast<uint64_t>(sizeof(appender_file_header));
        if (!read_to_next_segment()) {
            context.log_parse_fail_reason("parse first appender_file_segment_head failed");
            return false;
        }
        if (cur_read_seg_.enc_type_ != appender_encryption_type::plaintext) {
            return false;
        }
        // parse payload
        read_handle = read_with_cache(sizeof(appender_payload_metadata));
        if (read_handle.len() < sizeof(appender_payload_metadata)) {
            context.log_parse_fail_reason("read appender_payload_metadata failed");
            return false;
        }
        appender_payload_metadata payload_metadata;
        memcpy(&payload_metadata, read_handle.data(), sizeof(payload_metadata));
        if (payload_metadata.use_local_time != time_zone_.is_use_local_time()
            || payload_metadata.gmt_offset_hours != time_zone_.get_gmt_offset_hours()
            || payload_metadata.gmt_offset_minutes != time_zone_.get_gmt_offset_minutes()
            || payload_metadata.time_zone_diff_to_gmt_ms != time_zone_.get_time_zone_diff_to_gmt_ms()
            || payload_metadata.time_zone_str != time_zone_.get_time_zone_str()) {
            context.log_parse_fail_reason("timezone miss match");
            return false;
        }
        if (payload_metadata.category_count != parent_log_->get_categories_count()) {
            context.log_parse_fail_reason("category count miss match");
            return false;
        }
        uint32_t category_count = payload_metadata.category_count;
        for (uint32_t i = 0; i < category_count; ++i) {
            read_handle = read_with_cache(sizeof(uint32_t));
            if (read_handle.len() < sizeof(uint32_t)) {
                context.log_parse_fail_reason("read category name length failed");
                return false;
            }
            uint32_t name_len;
            memcpy(&name_len, read_handle.data(), sizeof(name_len));
            if ((size_t)name_len != parent_log_->get_categories_name()[i].size()) {
                context.log_parse_fail_reason("category name length mismatch");
                return false;
            }
            read_handle = read_with_cache((size_t)name_len);
            if (read_handle.len() < (size_t)name_len) {
                context.log_parse_fail_reason("read category name mismatch");
                return false;
            }
            if (memcmp(read_handle.data(), parent_log_->get_categories_name()[i].c_str(), (size_t)name_len) != 0) {
                context.log_parse_fail_reason("category name mismatch");
                return false;
            }
        }
        return true;
    }

    void appender_file_binary::on_file_open(bool is_new_created)
    {
        appender_file_base::on_file_open(is_new_created);
        current_file_is_new_created_ = is_new_created;
        if (enc_type_ != appender_encryption_type::plaintext) {
            if (!is_new_created) {
                bq::util::log_device_console(bq::log_level::error, "encrypted binary appender attempted to reuse an existing file: %s", get_file_handle().abs_file_path().c_str());
                get_file_handle().invalid();
                return;
            }
        }
        cur_read_seg_.start_pos = static_cast<uint64_t>(sizeof(appender_file_header));
        cur_read_seg_.end_pos = get_current_file_size();
        if (is_new_created) {
            // write file header and initialize encryption information
            appender_file_header file_head {};
            file_head.format = get_appender_format();
            file_head.version = get_binary_format_version();
            if (direct_write(&file_head, sizeof(file_head), bq::file_manager::seek_option::end, 0) != sizeof(file_head)) {
                bq::util::log_device_console(bq::log_level::error, "write binary appender file header failed: %s", get_file_handle().abs_file_path().c_str());
                get_file_handle().invalid();
                return;
            }

            // add first appender segment
            if (!append_new_segment(appender_segment_type::normal)) {
                bq::util::log_device_console(bq::log_level::error, "write first binary appender segment failed: %s", get_file_handle().abs_file_path().c_str());
                get_file_handle().invalid();
                return;
            }
            appender_payload_metadata payload_matadata;
            payload_matadata.magic_number[0] = 2;
            payload_matadata.magic_number[1] = 2;
            payload_matadata.magic_number[2] = 7;
            payload_matadata.use_local_time = time_zone_.is_use_local_time();
            payload_matadata.gmt_offset_hours = time_zone_.get_gmt_offset_hours();
            payload_matadata.gmt_offset_minutes = time_zone_.get_gmt_offset_minutes();
            payload_matadata.time_zone_diff_to_gmt_ms = time_zone_.get_time_zone_diff_to_gmt_ms();
            snprintf(payload_matadata.time_zone_str, sizeof(payload_matadata.time_zone_str), "%s", time_zone_.get_time_zone_str().c_str());
            payload_matadata.category_count = parent_log_->get_categories_count();
            auto handle = alloc_write_cache(sizeof(payload_matadata));
            memcpy(handle.data(), &payload_matadata, handle.allcoated_len());
            return_write_cache(handle);
            for (uint32_t i = 0; i < payload_matadata.category_count; ++i) {
                const bq::string& category_name = parent_log_->get_categories_name()[i];
                uint32_t name_len = (uint32_t)category_name.size();
                handle = alloc_write_cache(sizeof(name_len) + static_cast<size_t>(name_len));
                memcpy(handle.data(), &name_len, sizeof(name_len));
                memcpy(handle.data() + static_cast<ptrdiff_t>(sizeof(name_len)), category_name.c_str(), static_cast<size_t>(name_len));
                return_write_cache(handle);
            }
            mark_write_finished();
            flush_write_cache();
        }
    }

    appender_file_binary::~appender_file_binary()
    {
        // The base destructor flushes too, but it drives the window through on_flush_transform(),
        // and by the time it runs that call lands on the base class's empty version: this class is
        // already gone. Whatever is still in the write cache would then go to disk exactly as it
        // sits in memory - compressed, but not encrypted. Flush here, while the override is
        // still reachable and the segment key still exists, and only if the caller wants it: turning
        // the destructor flush off is how a recovery test leaves data behind in the cache, and it
        // has to keep working.
        if (get_flush_when_destruct() && get_pendding_flush_written_size() > 0) {
            flush_write_cache();
        }
        bq::encryption::secure_zero(&cur_seg_key_, sizeof(cur_seg_key_));
        set_flush_when_destruct(false);
    }

    bool appender_file_binary::on_flush_before_transform(size_t pending_size, uint64_t file_pos)
    {
        if (file_pos < cur_seg_payload_base_) {
            // The file position precedes the segment's payload, so the keystream offset would wrap.
            // Nothing writes before the payload base, so this is a corrupted-state assertion rather
            // than an expected outcome.
            assert(false && "flush position precedes the payload base of the current segment");
            return false;
        }
        const uint64_t payload_offset = static_cast<uint64_t>(file_pos - cur_seg_payload_base_);
        if (payload_offset + static_cast<uint64_t>(pending_size) <= max_segment_payload_size()) {
            return true;
        }
        // The ChaCha counter is 32 bits of 64-byte blocks, so one (key, nonce) pair covers 2^38
        // bytes of payload and no more. Crossing it would reuse keystream, which is worse than
        // losing a window: refuse the flush and drop the file handle, so the next entry opens a
        // fresh file with a fresh segment and a fresh key.
        bq::util::log_device_console(bq::log_level::error,
            "segment payload of %s would pass the 2^38 byte keystream range, starting a new file",
            get_file_handle().abs_file_path().c_str());
        get_file_handle().invalid();
        return false;
    }

    void appender_file_binary::on_flush_transform(uint8_t* data, size_t size, uint64_t file_pos)
    {
        assert(transform_enabled_ && "on_flush_transform reached a plaintext appender");
        if (size == 0) {
            return;
        }
        const uint64_t payload_offset = static_cast<uint64_t>(file_pos - cur_seg_payload_base_);
        transform_segment_payload(data, size, payload_offset, cur_seg_key_, cur_seg_rounds_);
    }

    bool appender_file_binary::seek_read_file_absolute(uint64_t pos)
    {
        if (appender_file_base::seek_read_file_absolute(pos)) {

            return true;
        }
        return false;
    }

    void appender_file_binary::seek_read_file_offset(int32_t offset)
    {
        appender_file_base::seek_read_file_offset(offset);
        if (cur_read_seg_.start_pos > get_read_file_pos() || cur_read_seg_.end_pos < get_read_file_pos()) {
            read_to_correct_segment();
        }
    }

    bool appender_file_binary::on_appender_file_recovery_begin()
    {
        if (!appender_file_base::on_appender_file_recovery_begin()) {
            return false;
        }
        return append_new_segment(appender_segment_type::recovery_by_appender);
    }

    void appender_file_binary::on_appender_file_recovery_end()
    {
        appender_file_base::on_appender_file_recovery_end();
    }

    bool appender_file_binary::on_log_item_recovery_begin(bq::log_entry_handle& read_handle)
    {
        if (!appender_file_base::on_log_item_recovery_begin(read_handle)) {
            return false;
        }

        segment_topology_info topology;
        const bool can_append_to_original = !current_file_is_new_created_
            && enc_type_ == appender_encryption_type::plaintext
            && scan_segment_topology(topology)
            && topology.has_segment
            && topology.all_segments_are_plaintext;

        if ((current_file_is_new_created_ || can_append_to_original)
            && append_new_segment(appender_segment_type::recovery_by_log_buffer)) {
            return true;
        }

        if (!current_file_is_new_created_) {
            open_new_indexed_file_by_name();
            if (get_file_handle() && append_new_segment(appender_segment_type::recovery_by_log_buffer)) {
                return true;
            }
        }
        get_file_handle().invalid();
        return false;
    }
    void appender_file_binary::on_log_item_recovery_end()
    {
        appender_file_base::on_log_item_recovery_end();
    }

    bool appender_file_binary::on_log_item_new_begin(bq::log_entry_handle& read_handle)
    {
        if (!appender_file_base::on_log_item_new_begin(read_handle)) {
            return false;
        }
        if (append_new_segment(appender_segment_type::normal)) {
            return true;
        }
        get_file_handle().invalid();
        return false;
    }

    appender_file_base::read_with_cache_handle appender_file_binary::read_with_cache(size_t size)
    {
        uint64_t current_read_pos = get_read_file_pos();
        assert(((current_read_pos >= cur_read_seg_.start_pos) && (current_read_pos <= cur_read_seg_.end_pos)) && "read beyond file size");
        while (current_read_pos == cur_read_seg_.end_pos) {
            if (!read_to_next_segment()) {
                return appender_file_base::read_with_cache(0);
            }
            current_read_pos = get_read_file_pos();
        }
        if (current_read_pos + static_cast<uint64_t>(size) > cur_read_seg_.end_pos) {
            size = static_cast<size_t>(cur_read_seg_.end_pos - current_read_pos);
        }
        return appender_file_base::read_with_cache(size);
    }

    bool appender_file_binary::read_to_correct_segment()
    {
        auto current_file_read_pos_backup = get_read_file_pos();
        cur_read_seg_.end_pos = static_cast<uint64_t>(sizeof(appender_file_header));
        bool found_segment = false;
        while (read_to_next_segment()) {
            if (current_file_read_pos_backup >= cur_read_seg_.start_pos && current_file_read_pos_backup <= cur_read_seg_.end_pos) {
                seek_read_file_absolute(current_file_read_pos_backup);
                found_segment = true;
                break;
            }
        }
        return found_segment;
    }

    bool appender_file_binary::read_to_next_segment()
    {
        auto new_seg_start_pos = cur_read_seg_.end_pos;
        if (new_seg_start_pos == UINT64_MAX) {
            return false;
        }
        if (!seek_read_file_absolute(new_seg_start_pos)) {
            return false;
        }
        auto read_handle = appender_file_base::read_with_cache(sizeof(appender_file_segment_head));
        if (read_handle.len() < sizeof(appender_file_segment_head)) {
            return false;
        }
        appender_file_segment_head seg_head;
        memcpy(&seg_head, read_handle.data(), sizeof(seg_head));
        if ((seg_head.enc_type != appender_encryption_type::plaintext && seg_head.enc_type != appender_encryption_type::rsa_kem_chacha)
            || (seg_head.seg_type != appender_segment_type::normal
                && seg_head.seg_type != appender_segment_type::recovery_by_appender
                && seg_head.seg_type != appender_segment_type::recovery_by_log_buffer)) {
            return false;
        }
        if (enc_type_ != seg_head.enc_type) {
            return false;
        }
        if (seg_head.next_seg_pos < new_seg_start_pos + sizeof(appender_file_segment_head)) {
            bq::util::log_device_console(bq::log_level::error, "file format of segment start pos: %" PRIu64 ", invalid segment end pos:%" PRIu64 ", file path:%s", new_seg_start_pos, seg_head.next_seg_pos, get_file_handle().abs_file_path().c_str());
            return false;
        }
        cur_read_seg_.start_pos = new_seg_start_pos;
        cur_read_seg_.end_pos = seg_head.next_seg_pos;
        cur_read_seg_.enc_type_ = seg_head.enc_type;
        return true;
    }

    bool appender_file_binary::scan_segment_topology(segment_topology_info& result)
    {
        result = segment_topology_info();
        if (!get_file_handle()) {
            return false;
        }

        const uint64_t file_size = get_current_file_size();
        if (file_size < sizeof(appender_file_header) || !seek_read_file_absolute(0)) {
            return false;
        }
        auto read_handle = appender_file_base::read_with_cache(sizeof(appender_file_header));
        if (read_handle.len() != sizeof(appender_file_header)) {
            return false;
        }
        appender_file_header file_head;
        memcpy(&file_head, read_handle.data(), sizeof(file_head));
        if (file_head.version != get_binary_format_version() || file_head.format != get_appender_format()) {
            return false;
        }

        uint64_t current_segment_start_pos = static_cast<uint64_t>(sizeof(appender_file_header));
        if (file_size == current_segment_start_pos) {
            return true;
        }

        while (true) {
            if (current_segment_start_pos > file_size
                || file_size - current_segment_start_pos < sizeof(appender_file_segment_head)
                || !seek_read_file_absolute(current_segment_start_pos)) {
                return false;
            }
            read_handle = appender_file_base::read_with_cache(sizeof(appender_file_segment_head));
            if (read_handle.len() != sizeof(appender_file_segment_head)) {
                return false;
            }

            appender_file_segment_head seg_head;
            memcpy(&seg_head, read_handle.data(), sizeof(seg_head));
            if ((seg_head.enc_type != appender_encryption_type::plaintext
                    && seg_head.enc_type != appender_encryption_type::rsa_kem_chacha)
                || (seg_head.seg_type != appender_segment_type::normal
                    && seg_head.seg_type != appender_segment_type::recovery_by_appender
                    && seg_head.seg_type != appender_segment_type::recovery_by_log_buffer)) {
                return false;
            }

            uint64_t min_next_segment_pos = current_segment_start_pos + sizeof(appender_file_segment_head);
            if (seg_head.enc_type == appender_encryption_type::rsa_kem_chacha) {
                if (file_size - min_next_segment_pos < sizeof(appender_segment_encryption_head)) {
                    return false;
                }
                appender_segment_encryption_head enc_head;
                read_handle = appender_file_base::read_with_cache(sizeof(enc_head));
                if (read_handle.len() != sizeof(enc_head)) {
                    return false;
                }
                memcpy(&enc_head, read_handle.data(), sizeof(enc_head));
                // A round count outside the set the kernel implements would silently decode to
                // noise, so it is a corrupt file rather than an unsupported one.
                if (!is_supported_chacha_rounds(enc_head.chacha_rounds)) {
                    return false;
                }
                if (!result.has_encrypted_segment) {
                    result.has_encrypted_segment = true;
                    result.rsa_key_fingerprint = enc_head.rsa_key_fingerprint;
                } else if (result.rsa_key_fingerprint != enc_head.rsa_key_fingerprint) {
                    return false;
                }
                min_next_segment_pos += sizeof(appender_segment_encryption_head);
            }

            result.has_segment = true;
            result.last_segment_start_pos = current_segment_start_pos;
            result.all_segments_are_plaintext &= (seg_head.enc_type == appender_encryption_type::plaintext);

            if (seg_head.next_seg_pos == UINT64_MAX) {
                return file_size >= min_next_segment_pos;
            }
            if (seg_head.next_seg_pos < min_next_segment_pos
                || seg_head.next_seg_pos > file_size
                || file_size - seg_head.next_seg_pos < sizeof(appender_file_segment_head)) {
                return false;
            }
            current_segment_start_pos = seg_head.next_seg_pos;
        }
    }

    bool appender_file_binary::append_new_segment(appender_file_binary::appender_segment_type type)
    {
        clear_read_cache();
        segment_topology_info topology;
        if (!scan_segment_topology(topology)
            || (!topology.has_segment && !current_file_is_new_created_)) {
            bq::util::log_device_console(bq::log_level::error, "binary appender refused to append a segment to invalid topology: %s", get_file_handle().abs_file_path().c_str());
            return false;
        }
        if (enc_type_ != appender_encryption_type::plaintext
            && topology.has_encrypted_segment
            && topology.rsa_key_fingerprint != rsa_key_fingerprint_) {
            bq::util::log_device_console(bq::log_level::warning, "binary appender refused to append a segment with a different RSA key: %s", get_file_handle().abs_file_path().c_str());
            return false;
        }

        // The key of the segment being replaced is about to become unusable; wipe it before the new
        // one lands on top of it.
        bq::encryption::secure_zero(&cur_seg_key_, sizeof(cur_seg_key_));
        appender_segment_encryption_head enc_head {};
        if (enc_type_ != appender_encryption_type::plaintext) {
            // Checked again here, not only at init: the application can remove its injected source
            // at any time, and the keys derived after that are weaker. Said once, not per segment.
            if (!bq::encryption::has_secure_random_source() && !warned_weak_key_source_) {
                bq::util::log_device_console(bq::log_level::warning,
                    "no secure random source left for segment key material; segment keys are derived from a software mix: %s", get_file_handle().abs_file_path().c_str());
                warned_weak_key_source_ = true;
            }
            enc_head.rsa_key_fingerprint = rsa_key_fingerprint_;
            enc_head.chacha_rounds = default_chacha_rounds;
            memset(enc_head.reserved, 0, sizeof(enc_head.reserved));
            // Fixed width on both halves: kem_ciphertext_ is I2OSP(c, 256) and the shared secret is
            // I2OSP(r, 256). The seed is never encoded at its minimal length, because a seed with a
            // leading zero byte would then derive a different key than the decoder does for the same
            // c - one segment in 256, silently.
            bq::array<uint8_t> kem_ciphertext;
            bq::array<uint8_t> shared_secret;
            if (!bq::rsa::kem_encapsulate(rsa_pub_key_, kem_ciphertext, shared_secret)
                || kem_ciphertext.size() != kem_ciphertext_size
                || shared_secret.size() != kem_ciphertext_size) {
                bq::util::log_device_console(bq::log_level::error, "RSA-KEM encapsulation failed while appending binary segment");
                return false;
            }
            memcpy(enc_head.kem_ciphertext, kem_ciphertext.begin(), kem_ciphertext_size);
            const bool derived = derive_segment_key(enc_head, shared_secret.begin(), shared_secret.size(), cur_seg_key_);
            // The shared secret is the seed the whole segment rests on; nothing else needs it.
            bq::encryption::secure_zero(shared_secret.begin(), shared_secret.size());
            if (!derived) {
                bq::util::log_device_console(bq::log_level::error, "segment key derivation failed while appending binary segment");
                return false;
            }
            cur_seg_rounds_ = enc_head.chacha_rounds;
        }

        const uint64_t new_seg_start_pos = get_current_file_size();
        appender_file_segment_head new_segment {};
        new_segment.enc_type = enc_type_;
        new_segment.next_seg_pos = UINT64_MAX;
        new_segment.seg_type = type;
        if (direct_write(&new_segment, sizeof(new_segment), bq::file_manager::seek_option::end, 0) != sizeof(new_segment)) {
            bq::util::log_device_console(bq::log_level::error, "write binary segment header failed: %s", get_file_handle().abs_file_path().c_str());
            bq::file_manager::instance().truncate_file(get_file_handle(), new_seg_start_pos);
            return false;
        }
        if (enc_type_ != appender_encryption_type::plaintext) {
            if (direct_write(&enc_head, sizeof(enc_head), bq::file_manager::seek_option::current, 0) != sizeof(enc_head)) {
                bq::util::log_device_console(bq::log_level::error, "write binary segment encryption data failed: %s", get_file_handle().abs_file_path().c_str());
                bq::file_manager::instance().truncate_file(get_file_handle(), new_seg_start_pos);
                return false;
            }
        }
        cur_seg_payload_base_ = new_seg_start_pos
            + (enc_type_ == appender_encryption_type::plaintext
                    ? plaintext_payload_base_offset
                    : encrypted_payload_base_offset);
        flush_write_io();
        if (topology.has_segment
            && direct_write(&new_seg_start_pos, sizeof(new_seg_start_pos), bq::file_manager::seek_option::begin,
                   static_cast<int64_t>(topology.last_segment_start_pos) + static_cast<int64_t>(BQ_POD_RUNTIME_OFFSET_OF(appender_file_segment_head, next_seg_pos)))
                != sizeof(new_seg_start_pos)) {
            bq::file_manager::instance().truncate_file(get_file_handle(), static_cast<size_t>(new_seg_start_pos));
            bq::util::log_device_console(bq::log_level::error, "link previous binary segment failed: %s", get_file_handle().abs_file_path().c_str());
            return false;
        }
        flush_write_io();
        return true;
    }
}
