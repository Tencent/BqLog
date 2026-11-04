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
#include "bq_log/log/decoder/appender_decoder_base.h"
#include "bq_log/utils/time_zone.h"
#include "bq_log/global/log_vars.h"

static constexpr size_t DECODER_CACHE_READ_DEFAULT_SIZE = 32 * 1024;

namespace bq {
    appender_decode_result appender_decoder_base::init(const file_handle& file, const string& private_key_str)
    {
        file_ = file;
        current_file_size_ = file_manager::instance().get_file_size(file_);
        seek_read_file_absolute(0);
        uint32_t format_version = get_binary_format_version();

        if (current_file_size_ <= sizeof(file_head_) + sizeof(appender_file_binary::appender_file_segment_head) + sizeof(payload_metadata_)) {
            util::log_device_console(log_level::error, "decode log file failed, too small size");
            return appender_decode_result::failed_decode_error;
        }
        read_from_file_directly(&file_head_, sizeof(file_head_));
        if (file_head_.version != format_version) {
            // Not "corrupt", not "unsupported by this tool version": the runtime deliberately carries
            // one format version only, so the advice is where to find the other one.
            util::log_device_console(log_level::error,
                "decode failed: log file format version %" PRIu32 " is not supported by this runtime (supports %" PRIu32 ").\n"
                "Use the standalone BqLog_LogDecoder tool, which supports format versions 9, 10 and 11.",
                file_head_.version, format_version);
            return appender_decode_result::failed_decode_error;
        }
        if (!private_key_str.trim().is_empty()) {
            if (!rsa::parse_private_key_pem(private_key_str.trim(), private_key_)) {
                util::log_device_console(log_level::error, "decode log file failed, invalid private key");
                return appender_decode_result::failed_decode_error;
            }
            if (private_key_.n_.size() != 256) {
                util::log_device_console(log_level::error, "decode log file failed, invalid private key size, only rsa 2048 supported");
                return appender_decode_result::failed_decode_error;
            }
        }
        cur_read_seg_.end_pos = sizeof(file_head_);
        appender_decode_result resut = read_to_next_segment();
        if (resut != appender_decode_result::success) {
            util::log_device_console(log_level::error, "decode log file failed, segment parse failed");
            return resut;
        }
        auto read_handle = read_with_cache(sizeof(payload_metadata_));
        if (read_handle.len() != sizeof(payload_metadata_)) {
            util::log_device_console(log_level::error, "decode log file failed, read payload metadata failed");
            return appender_decode_result::failed_io_error;
        }
        // The bool is rebuilt from its raw byte and the time zone name is terminated, since the file may be corrupt.
        const uint8_t raw_use_local_time = read_handle.data()[offsetof(appender_file_binary::appender_payload_metadata, use_local_time)];
        memcpy(&payload_metadata_, read_handle.data(), sizeof(payload_metadata_));
        payload_metadata_.use_local_time = (raw_use_local_time != 0);
        payload_metadata_.time_zone_str[sizeof(payload_metadata_.time_zone_str) - 1] = '\0';
        if (payload_metadata_.magic_number[0] != 2 || payload_metadata_.magic_number[1] != 2 || payload_metadata_.magic_number[2] != 7) {
            if (cur_read_seg_.enc_type == appender_file_binary::appender_encryption_type::plaintext) {
                util::log_device_console(log_level::error, "decode log file failed, magic number mismatch");
            } else {
                util::log_device_console(log_level::error, "decode log file failed, magic number mismatch, please check your private key");
            }
            return appender_decode_result::failed_decode_error;
        }
        time_zone_.restore_by_config(payload_metadata_.use_local_time, payload_metadata_.gmt_offset_hours, payload_metadata_.gmt_offset_minutes, payload_metadata_.time_zone_diff_to_gmt_ms, payload_metadata_.time_zone_str);

        for (uint32_t i = 0; i < payload_metadata_.category_count; ++i) {
            uint32_t name_len = 0;
            string name;
            read_handle = read_with_cache(sizeof(name_len));
            if (read_handle.len() < sizeof(name_len)) {
                util::log_device_console(log_level::error, "read category name length failed");
                return appender_decode_result::failed_io_error;
            }
            memcpy(&name_len, read_handle.data(), sizeof(name_len));
            if (static_cast<size_t>(name_len) > current_file_size_) {
                util::log_device_console(log_level::error, "category name length %" PRIu32 " exceeds file size", name_len);
                return appender_decode_result::failed_decode_error;
            }
            if (name_len > 0) {
                name.fill_uninitialized((size_t)name_len);
                read_handle = read_with_cache((size_t)name_len);
                if (read_handle.len() < (size_t)name_len) {
                    util::log_device_console(log_level::error, "read category name len mismatch");
                    return appender_decode_result::failed_io_error;
                }
                memcpy(&name[0], read_handle.data(), name_len);
            } else if (i != 0) {
                util::log_device_console(log_level::error, "category name length mismatch");
                return appender_decode_result::failed_decode_error;
            }
            category_names_.push_back(move(name));
        }
        return init_private();
    }

    appender_decode_result appender_decoder_base::decode()
    {
        bool lose_data = false;
        appender_decode_result result;
        do {
            result = decode_private();
            if (appender_decode_result::success == result
                || appender_decode_result::eof == result) {
                break;
            }
            lose_data = true;
            if (appender_decode_result::success != read_to_next_segment()) {
                break;
            }
        } while (true);
        if (lose_data) {
            bq::string error_tips;
            error_tips += "/*********************************************************************/\n";
            error_tips += "/*       WARNING: Some log data may be lost or corrupted here        */\n";
            error_tips += "/*********************************************************************/\n";
            decoded_text_.insert_batch(decoded_text_.begin(), error_tips.begin(), error_tips.size());
        }
        return result;
    }

    bool appender_decoder_base::seek_read_file_absolute(uint64_t pos)
    {
        if (current_file_cursor_ == pos) {
            return true;
        }
        // A segment start above 2 GiB is a normal position, so the offset keeps its width all the
        // way down to lseek: narrowing it here made the seek fail - or land somewhere else - and
        // the failure reads as the end of the file, which silently drops every later segment.
        bool result = file_manager::instance().seek(file_, file_manager::seek_option::begin, static_cast<int64_t>(pos));
        if (result) {
            clear_read_cache();
            current_file_cursor_ = pos;
        }
        return result;
    }

    bool appender_decoder_base::seek_read_file_offset(int32_t offset)
    {
        int64_t final_cache_cursor = static_cast<int64_t>(cache_read_cursor_) + offset;
        if (final_cache_cursor >= 0 && final_cache_cursor <= static_cast<int64_t>(cache_read_.size())) {
            cache_read_cursor_ = static_cast<size_t>(final_cache_cursor);
            return true;
        } else {
            return seek_read_file_absolute(static_cast<uint64_t>(static_cast<int64_t>(current_file_cursor_) + offset));
        }
    }

    uint64_t appender_decoder_base::get_current_file_size()
    {
        return current_file_size_;
    }

    // data() returned by read_with_cache_handle will be invalid after next calling of "read_with_cache"
    appender_decoder_base::read_with_cache_handle appender_decoder_base::read_with_cache(size_t size)
    {
        auto left_size = cache_read_.size() - cache_read_cursor_;
        if (left_size < size) {
            // A zero length read is only the end of the file when the segment chain ended there, so a
            // broken chain or a failed read is reported to the caller instead of a silent stop.
            read_of_file_eof_ = false;
            if (left_size == 0) {
                while (static_cast<uint64_t>(current_file_cursor_) == cur_read_seg_.end_pos) {
                    auto result = read_to_next_segment();
                    if (result != appender_decode_result::success) {
                        read_of_file_eof_ = (result == appender_decode_result::eof);
                        read_with_cache_handle empty_handle;
                        empty_handle.data_ = nullptr;
                        empty_handle.len_ = 0;
                        return empty_handle;
                    }
                }
            }
            clear_read_cache();
            if (left_size != 0) {
                uint64_t adjusted_file_cursor = static_cast<uint64_t>(static_cast<int64_t>(current_file_cursor_) - static_cast<int64_t>(left_size));
                seek_read_file_absolute(adjusted_file_cursor);
            }
            const uint64_t chunk_file_pos = static_cast<uint64_t>(current_file_cursor_);
            const size_t read_offset = get_read_front_padding(chunk_file_pos);
            uint64_t seg_left_size = cur_read_seg_.end_pos - static_cast<uint64_t>(current_file_cursor_);
            // Never allocate more than the segment can supply; a short read is reported to the caller.
            size_t alloc_size = (static_cast<uint64_t>(size) > seg_left_size) ? static_cast<size_t>(seg_left_size) : size;
            auto total_size = bq::max_value(alloc_size + read_offset, DECODER_CACHE_READ_DEFAULT_SIZE);
            cache_read_.clear();
            cache_read_.fill_uninitialized(total_size);
#if defined(BQ_UNIT_TEST)
            peak_cache_read_size_ = bq::max_value(peak_cache_read_size_, total_size);
#endif
            auto expected_read_size = total_size - read_offset;
            if (static_cast<uint64_t>(expected_read_size) > seg_left_size) {
                expected_read_size = static_cast<size_t>(seg_left_size);
            }
            auto read_size = file_manager::instance().read_file(file_, cache_read_.begin() + static_cast<ptrdiff_t>(read_offset), expected_read_size);
            current_file_cursor_ += read_size;
            cache_read_cursor_ = read_offset;

            if (read_size < total_size - read_offset) {
                cache_read_.erase(cache_read_.begin() + static_cast<ptrdiff_t>(read_offset + read_size), total_size - read_offset - read_size);
            }

            // The chunk is decoded on its own, from its file position: no state carries over from the
            // previous chunk, which is what makes a seek land on the same bytes as a sequential read.
            if (read_size > 0) {
                // A chunk that cannot be decoded is reported like any other failed read: the caller
                // ends up at "the segment chain is broken" instead of decoding bytes against a
                // keystream offset that does not exist.
                if (!decrypt_segment_chunk(cache_read_.begin() + static_cast<ptrdiff_t>(read_offset), read_size, chunk_file_pos)) {
                    clear_read_cache();
                    read_of_file_eof_ = false;
                    read_with_cache_handle undecodable_handle;
                    undecodable_handle.data_ = nullptr;
                    undecodable_handle.len_ = 0;
                    return undecodable_handle;
                }
            }
        }
        read_with_cache_handle result;
        result.data_ = cache_read_.begin() + static_cast<ptrdiff_t>(cache_read_cursor_);
        result.len_ = bq::min_value(size, cache_read_.size() - cache_read_cursor_);
        cache_read_cursor_ += result.len_;
        return result;
    }

    size_t appender_decoder_base::read_from_file_directly(void* dst, size_t size)
    {
        auto read_size = file_manager::instance().read_file(file_, dst, size);
        current_file_cursor_ += read_size;
        return read_size;
    }

    void appender_decoder_base::clear_read_cache()
    {
        cache_read_cursor_ = 0;
        cache_read_.clear();
        if (cache_read_.capacity() > DECODER_CACHE_READ_DEFAULT_SIZE) {
            cache_read_.shrink();
        }
    }

    appender_decode_result appender_decoder_base::do_decode_by_log_entry_handle(const log_entry_handle& item)
    {
        auto layout_result = layout_.do_layout(item, time_zone_, &category_names_);
        if (layout_result != layout::enum_layout_result::finished) {
            util::log_device_console(log_level::error, "decode compressed log file failed, layout error code:%" PRId32, (int32_t)layout_result);
            return appender_decode_result::failed_decode_error;
        }
        if (layout_.get_formated_str_len() > 0) {
            decoded_text_.insert_batch(decoded_text_.end(), layout_.get_formated_str(), layout_.get_formated_str_len());
            layout_.tidy_memory();
        }
        return appender_decode_result::success;
    }

    appender_decode_result appender_decoder_base::read_to_next_segment()
    {
        auto new_seg_start_pos = cur_read_seg_.end_pos;
        // The cached bytes belong to the segment being left, so they cannot outlive the switch. The
        // reload path in read_with_cache clears the cache anyway, but decode() also comes here to
        // resynchronise after a failed item, and it previously left the old segment's cache behind:
        // the next fill then rewound the file cursor into the new segment's head, before its payload
        // base, and the keystream offset was computed from a position that segment does not have.
        clear_read_cache();
        current_file_size_ = file_manager::instance().get_file_size(file_);
        if (!seek_read_file_absolute(new_seg_start_pos)) {
            return appender_decode_result::eof;
        }
        bq::appender_file_binary::appender_file_segment_head seg_head;
        auto read_size = read_from_file_directly(&seg_head, sizeof(seg_head));
        if (read_size < sizeof(seg_head)) {
            return appender_decode_result::eof;
        }
        if (!is_supported_enc_type(seg_head.enc_type)
            || (seg_head.seg_type != appender_file_binary::appender_segment_type::normal
                && seg_head.seg_type != appender_file_binary::appender_segment_type::recovery_by_appender
                && seg_head.seg_type != appender_file_binary::appender_segment_type::recovery_by_log_buffer)) {
            bq::util::log_device_console(bq::log_level::error, "decode log file failed, invalid segment header");
            return appender_decode_result::failed_decode_error;
        }
        // Reads whatever encryption metadata follows the header and builds the key for this
        // segment. Everything version-specific lives behind this call.
        if (!setup_segment(seg_head, new_seg_start_pos)) {
            return appender_decode_result::failed_decode_error;
        }
        const uint64_t current_file_size = static_cast<uint64_t>(current_file_size_);
        if (seg_head.next_seg_pos != UINT64_MAX
            && (seg_head.next_seg_pos < current_file_cursor_
                || seg_head.next_seg_pos > current_file_size
                || current_file_size - seg_head.next_seg_pos < sizeof(seg_head))) {
            bq::util::log_device_console(bq::log_level::error, "file format of segment start pos: %" PRIu64 ", invalid segment end pos:%" PRIu64, new_seg_start_pos, seg_head.next_seg_pos);
            return appender_decode_result::failed_decode_error;
        }
        auto last_seg_type = (new_seg_start_pos == sizeof(file_head_) ? appender_file_binary::appender_segment_type::normal : cur_read_seg_.seg_type);
        cur_read_seg_.enc_type = seg_head.enc_type;
        cur_read_seg_.start_pos = new_seg_start_pos;
        cur_read_seg_.end_pos = seg_head.next_seg_pos == UINT64_MAX ? current_file_size : seg_head.next_seg_pos;
        cur_read_seg_.seg_type = seg_head.seg_type;

        bool is_last_seg_recover = (last_seg_type == appender_file_binary::appender_segment_type::recovery_by_appender
            || last_seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer);
        bool is_cur_seg_recover = (cur_read_seg_.seg_type == appender_file_binary::appender_segment_type::recovery_by_appender
            || cur_read_seg_.seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer);
        if (is_last_seg_recover != is_cur_seg_recover) {
            if (is_cur_seg_recover) {
                decoded_text_.insert_batch(decoded_text_.end(), log_global_vars::get().log_recover_start_str_, strlen(log_global_vars::get().log_recover_start_str_));
                decoded_text_.push_back('\n');
            } else if (is_last_seg_recover) {
                decoded_text_.insert_batch(decoded_text_.end(), log_global_vars::get().log_recover_end_str_, strlen(log_global_vars::get().log_recover_end_str_));
                decoded_text_.push_back('\n');
            }
        }
        return appender_decode_result::success;
    }

}
