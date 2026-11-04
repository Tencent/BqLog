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
#pragma once
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
            uint64_t start_pos = 0;
            uint64_t end_pos = 0;
            appender_file_binary::appender_segment_type seg_type = appender_file_binary::appender_segment_type::normal;
            appender_file_binary::appender_encryption_type enc_type = appender_file_binary::appender_encryption_type::plaintext;
        };

    public:
        virtual ~appender_decoder_base() {}

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

        // The segment layer, and the only part of the decoder that knows an encryption scheme.
        // Everything below it - header parsing, the record stream, the read cache - is
        // version-agnostic, so a decoder for another format version overrides these three and
        // nothing else. The runtime implements exactly one version (v11); the standalone decoder
        // implements the older ones on top of the same base.
        //
        // Positions passed in are absolute file positions, not offsets inside a segment: some
        // schemes address their keystream by file position and some do not, so the base class
        // refuses to guess and hands over the raw position.
        virtual bool is_supported_enc_type(appender_file_binary::appender_encryption_type type) const = 0;

        // Called once per segment, right after its 12-byte header has been read and before any
        // payload byte. Reading the encryption metadata and building the key happen here.
        virtual bool setup_segment(const appender_file_binary::appender_file_segment_head& head, uint64_t seg_start) = 0;

        // Decrypts [data, data + size) in place; file_pos is where its first byte sits in the file.
        // Returns false when the chunk cannot be decoded, which only a corrupt file can ask for: the
        // read is then reported as a broken chain rather than decoded into garbage.
        virtual bool decrypt_segment_chunk(uint8_t* data, size_t size, uint64_t file_pos) = 0;

        // How many bytes of padding the segment layer wants in front of a chunk, given where the
        // chunk starts in the file. A scheme whose SIMD routine needs the buffer and the file offset
        // to agree modulo the alignment vector width asks for `file_pos % alignment` here; the record
        // layer reserves that much and hands over an aligned pointer, so it stays version-agnostic.
        virtual size_t get_read_front_padding(uint64_t file_pos) const
        {
            (void)file_pos;
            return 0;
        }

        static constexpr size_t buffer_alignment = appender_file_base::DEFAULT_BUFFER_ALIGNMENT;

        bool seek_read_file_absolute(uint64_t pos);

        bool seek_read_file_offset(int32_t offset);

        uint64_t get_current_file_size();

        // data() returned by read_with_cache_handle will be invalid after next calling of "read_with_cache"
        read_with_cache_handle read_with_cache(size_t size);

        // A zero length read means the end of the file only when the segment chain ended there.
        bq_forceinline bool is_read_of_file_eof() const
        {
            return read_of_file_eof_;
        }

        void clear_read_cache();

        appender_decode_result do_decode_by_log_entry_handle(const bq::log_entry_handle& item);

    private:
        appender_decode_result read_to_next_segment();

    protected:
        size_t read_from_file_directly(void* dst, size_t size);

    protected:
        bq::string decoded_text_;
        bq::layout layout_;
        bq::appender_file_binary::appender_file_header file_head_;
        seg_info cur_read_seg_;
        bq::appender_file_binary::appender_payload_metadata payload_metadata_;
        bq::array<bq::string> category_names_;
        bq::rsa::private_key private_key_;

    private:
        // built from payload_metadata_ once the header is read, so its time string cache spans entries
        bq::time_zone time_zone_;
        bq::file_handle file_;
        uint64_t current_file_size_ = 0;
        uint64_t current_file_cursor_ = UINT64_MAX;

        bq::array<uint8_t, bq::aligned_allocator<uint8_t, appender_file_base::DEFAULT_BUFFER_ALIGNMENT>> cache_read_;
        decltype(cache_read_)::size_type cache_read_cursor_ = 0;
        bool read_of_file_eof_ = false;

#if defined(BQ_UNIT_TEST)
        size_t peak_cache_read_size_ = 0;

    public:
        size_t get_peak_cache_read_size_for_test() const
        {
            return peak_cache_read_size_;
        }
#endif
    };
}
