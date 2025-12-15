#pragma once
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_def.h"
#include "bq_log/log/layout.h"
#include "bq_log/log/appender/appender_file_binary.h"

namespace bq {
    class appender_decoder_base {
    protected:
        struct read_with_cache_handle {
            friend class appender_decoder_base;

        private:
            uint8_t* data_;
            size_t len_;

        public:
            bq_forceinline const uint8_t* data() const { return data_; }
            bq_forceinline size_t len() const { return len_; }
            bq_forceinline read_with_cache_handle offset(size_t offset)
            {
                read_with_cache_handle offset_handle;
                offset_handle.data_ = data_ + offset;
                offset_handle.len_ = len_ - offset;
                return offset_handle;
            }
        };

        struct seg_info {
            uint64_t start_pos;
            uint64_t end_pos;
            appender_file_binary::appender_segment_type seg_type;
            appender_file_binary::appender_encryption_type enc_type;
            bq::array<uint8_t, bq::aligned_allocator<uint8_t, appender_file_base::DEFAULT_BUFFER_ALIGNMENT>> xor_key_blob;
        };

    public:
        virtual ~appender_decoder_base() { }

        appender_decode_result init(const bq::file_handle& file, const bq::string& private_key_str);

        appender_decode_result decode();

        const bq::string& get_decoded_log_text() const
        {
            return decoded_text_;
        }

    protected:
        virtual appender_decode_result init_private() = 0;

        virtual appender_decode_result decode_private() = 0;

        virtual uint32_t get_binary_format_version() const = 0;

        bool seek_read_file_absolute(size_t pos);

        bool seek_read_file_offset(int32_t offset);

        size_t get_current_file_size();

        // data() returned by read_with_cache_handle will be invalid after next calling of "read_with_cache"
        read_with_cache_handle read_with_cache(size_t size);

        void clear_read_cache();

        appender_decode_result do_decode_by_log_entry_handle(const bq::log_entry_handle& item);

    private:
        appender_decode_result read_to_next_segment();

        size_t read_from_file_directly(void* dst, size_t size);
    protected:
        bq::string decoded_text_;
        bq::layout layout_;
        bq::appender_file_binary::appender_file_header file_head_;
        seg_info cur_read_seg_;
        bq::appender_file_binary::appender_payload_metadata payload_metadata_;
        bq::array<bq::string> category_names_;

    private:
        bq::file_handle file_;
        size_t current_file_size_ = 0;
        size_t current_file_cursor_ = SIZE_MAX;
        bq::rsa::private_key private_key_;

        bq::array<uint8_t, bq::aligned_allocator<uint8_t, appender_file_base::DEFAULT_BUFFER_ALIGNMENT>> cache_read_;
        decltype(cache_read_)::size_type cache_read_cursor_ = 0;
    };
}
