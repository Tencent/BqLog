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
#include "native_src_bq_log_log_appender_appender_file_compressed.h"
#include "native_src_bq_log_log_log_imp.h"
#include "native_src_bq_log_utils_log_utils.h"
#include "native_src_bq_common_bq_common.h"

namespace bq {
    template <typename T, bool is_aligned>
    struct data_value_helper {
        static T get_data_value(const void* data)
        {
            return *(const T*)data;
        }
    };

    template <typename T>
    struct data_value_helper<T, false> {
        static T get_data_value(const void* data)
        {
            T value;
            memcpy(&value, data, sizeof(T));
            return value;
        }
    };

    static bq_forceinline uint64_t get_format_template_hash(bq::log_level level, uint32_t category_idx, uint64_t fmt_str_hash)
    {
        uint64_t mixer = (static_cast<uint64_t>(category_idx) << 32) | static_cast<uint64_t>(level);
        return fmt_str_hash ^ mixer;
    }

    inline bool is_addr_8_aligned(const void* data)
    {
        constexpr size_t mask_8_bytes_align = (sizeof(uint64_t) - 1);
        return ((size_t)data & mask_8_bytes_align) == 0;
    }

    static bq_forceinline uint64_t mix_cache_key(uint64_t key)
    {
        key ^= key >> 33;
        key *= UINT64_C(0xff51afd7ed558ccd);
        key ^= key >> 33;
        key *= UINT64_C(0xc4ceb9fe1a85ec53);
        key ^= key >> 33;
        return key;
    }

    static uint32_t get_cache_max_entries_config(
        const bq::property_value& config_obj,
        const bq::string& appender_name,
        const char* config_name,
        uint32_t default_value,
        uint32_t min_value,
        uint32_t max_value)
    {
        const auto& configured = config_obj[config_name];
        if (configured.is_null()) {
            return default_value;
        }
        if (!configured.is_integral()) {
            bq::util::log_device_console(
                bq::log_level::warning,
                "compressed appender \"%s\": \"%s\" must be an integer, use default value %" PRIu32,
                appender_name.c_str(),
                config_name,
                default_value);
            return default_value;
        }

        const int64_t configured_value = static_cast<int64_t>(configured);
        if (configured_value < static_cast<int64_t>(min_value)) {
            bq::util::log_device_console(
                bq::log_level::warning,
                "compressed appender \"%s\": \"%s\" value %" PRId64 " is too small, clamp to %" PRIu32,
                appender_name.c_str(),
                config_name,
                configured_value,
                min_value);
            return min_value;
        }
        if (static_cast<uint64_t>(configured_value) > static_cast<uint64_t>(max_value)) {
            bq::util::log_device_console(
                bq::log_level::warning,
                "compressed appender \"%s\": \"%s\" value %" PRId64 " is too large, clamp to %" PRIu32,
                appender_name.c_str(),
                config_name,
                configured_value,
                max_value);
            return max_value;
        }
        return static_cast<uint32_t>(configured_value);
    }

    bool appender_file_compressed::init_impl(const bq::property_value& config_obj)
    {
        format_template_cache_max_entries_ = get_cache_max_entries_config(
            config_obj,
            get_name(),
            "format_template_cache_max_entries",
            DEFAULT_FORMAT_TEMPLATE_CACHE_MAX_ENTRIES,
            CACHE_MIN_ENTRIES,
            FORMAT_L2_MAX_CONFIG_ENTRIES);
        thread_info_cache_max_entries_ = get_cache_max_entries_config(
            config_obj,
            get_name(),
            "thread_info_cache_max_entries",
            DEFAULT_THREAD_INFO_CACHE_MAX_ENTRIES,
            CACHE_MIN_ENTRIES,
            THREAD_L2_MAX_CONFIG_ENTRIES);
        format_l2_.clear();
        thread_l2_.clear();
        const bool format_cache_configured = format_l2_.set_max_size(format_template_cache_max_entries_);
        const bool thread_cache_configured = thread_l2_.set_max_size(thread_info_cache_max_entries_);
        assert(format_cache_configured && thread_cache_configured);
        (void)format_cache_configured;
        (void)thread_cache_configured;
        return appender_file_binary::init_impl(config_obj);
    }

    bool appender_file_compressed::reset_impl(const bq::property_value& config_obj)
    {
        const uint32_t new_format_template_cache_max_entries = get_cache_max_entries_config(
            config_obj,
            get_name(),
            "format_template_cache_max_entries",
            DEFAULT_FORMAT_TEMPLATE_CACHE_MAX_ENTRIES,
            CACHE_MIN_ENTRIES,
            FORMAT_L2_MAX_CONFIG_ENTRIES);
        const uint32_t new_thread_info_cache_max_entries = get_cache_max_entries_config(
            config_obj,
            get_name(),
            "thread_info_cache_max_entries",
            DEFAULT_THREAD_INFO_CACHE_MAX_ENTRIES,
            CACHE_MIN_ENTRIES,
            THREAD_L2_MAX_CONFIG_ENTRIES);
        if (!appender_file_binary::reset_impl(config_obj)) {
            return false;
        }
        if (new_format_template_cache_max_entries != format_template_cache_max_entries_) {
            if (!format_l2_.set_max_size(new_format_template_cache_max_entries)) {
                format_l2_.clear();
                const bool configured =
                    format_l2_.set_max_size(new_format_template_cache_max_entries);
                assert(configured);
                (void)configured;
            }
            format_template_cache_max_entries_ =
                new_format_template_cache_max_entries;
        }
        if (new_thread_info_cache_max_entries != thread_info_cache_max_entries_) {
            if (!thread_l2_.set_max_size(new_thread_info_cache_max_entries)) {
                thread_l2_.clear();
                const bool configured =
                    thread_l2_.set_max_size(new_thread_info_cache_max_entries);
                assert(configured);
                (void)configured;
            }
            thread_info_cache_max_entries_ =
                new_thread_info_cache_max_entries;
        }
        return true;
    }

    void appender_file_compressed::on_file_open(bool is_new_created)
    {
        appender_file_binary::on_file_open(is_new_created);
        if (is_new_created) {
            reset();
        }
    }

    bq::string appender_file_compressed::get_file_ext_name()
    {
        return ".logcompr";
    }

    uint32_t appender_file_compressed::get_binary_format_version() const
    {
        return format_version;
    }

    bool appender_file_compressed::parse_exist_log_file(parse_file_context& context)
    {
        if (!appender_file_binary::parse_exist_log_file(context)) {
            return false;
        }
        reset();
        while (true) {
            auto read_result = read_item_data(context);
            if (is_read_of_cache_eof()) {
                // parse finished
                return true;
            }
            if (!bq::get<0>(read_result)) {
                return false;
            }
            switch (bq::get<1>(read_result)) {
            case bq::appender_file_compressed::log_template:
                switch (*bq::get<2>(read_result).data()) {
                case template_sub_type::format_template_utf8:
                case template_sub_type::format_template_utf16:
                    if (!parse_formate_template(context, bq::get<2>(read_result).offset(1), (template_sub_type)*bq::get<2>(read_result).data())) {
                        return false;
                    }
                    break;
                case template_sub_type::thread_info_template:
                    if (!parse_thread_info_template(context, bq::get<2>(read_result).offset(1))) {
                        return false;
                    }
                    break;
                default:
                    context.log_parse_fail_reason("decode compressed log file failed, invalid log template sub type");
                    return false;
                    break;
                }
                break;
            case bq::appender_file_compressed::log_entry:
                if (!parse_log_entry(context, bq::get<2>(read_result))) {
                    return false;
                }
                break;
            default:
                return false;
                break;
            }
        }
        return false;
    }

    bq::tuple<bool, appender_file_compressed::item_type, appender_file_base::read_with_cache_handle> appender_file_compressed::read_item_data(parse_file_context& context)
    {
        constexpr size_t VLQ_MAX_SIZE = bq::log_utils::vlq::vlq_max_bytes_count<uint32_t>();
        auto read_handle = read_with_cache(VLQ_MAX_SIZE + 1);
        if (read_handle.len() == 0 && is_read_of_cache_eof()) {
            return bq::make_tuple(false, appender_file_compressed::item_type::log_template, read_handle);
        }
        if (read_handle.len() < 2) {
            context.log_parse_fail_reason("decode compressed log file failed, read item head failed");
            return bq::make_tuple(false, appender_file_compressed::item_type::log_template, read_handle);
        }
        uint8_t& first_byte = const_cast<uint8_t&>(read_handle.data()[0]);
        int32_t offset = ((first_byte & 0x7F) == 0) ? 1 : 0; // 0b01111111
        auto type = (appender_file_compressed::item_type)(first_byte & 0x80); // 0b10000000
        uint32_t data_size = 0;
        if (offset == 0) {
            first_byte &= 0x7F; // 0b01111111
        }
        size_t size_len = bq::log_utils::vlq::vlq_decode(data_size, read_handle.data() + offset);
        if (size_len == bq::log_utils::vlq::invalid_decode_length) {
            return bq::make_tuple(false, type, read_handle);
        }
        if (offset == 0) {
            first_byte |= (uint8_t)type;
        }
        seek_read_file_offset(static_cast<int32_t>(size_len) + offset - static_cast<int32_t>(read_handle.len()));
        read_handle = read_with_cache(data_size);
        if (read_handle.len() != (size_t)data_size || data_size < 2) {
            context.log_parse_fail_reason("decode compressed log file failed, read item head failed");
            return bq::make_tuple(false, appender_file_compressed::item_type::log_template, read_handle);
        }
        return bq::make_tuple(true, type, read_handle);
    }

    bool appender_file_compressed::parse_log_entry(parse_file_context& context, const appender_file_base::read_with_cache_handle& data_handle)
    {
        if (data_handle.len() < 2) {
            context.log_parse_fail_reason("log entry content size should be equal or larger than 2");
            return false;
        }
        uint64_t epoch_offset_zigzag;
        if (bq::log_utils::vlq::vlq_decode(epoch_offset_zigzag, data_handle.data()) == bq::log_utils::vlq::invalid_decode_length) {
            context.log_parse_fail_reason("log entry epoch_offset decode failed");
            return false;
        }
        const int64_t epoch_offset = bq::log_utils::zigzag::decode(epoch_offset_zigzag);
        last_log_entry_epoch_ = static_cast<uint64_t>(static_cast<int64_t>(last_log_entry_epoch_) + epoch_offset);
        return true;
    }

    bool appender_file_compressed::parse_formate_template(parse_file_context& context, const appender_file_base::read_with_cache_handle& data_handle, template_sub_type sub_type)
    {
        if (data_handle.len() < 2) {
            context.log_parse_fail_reason("format template data size should be equal or larger than 2");
            return false;
        }
        uint8_t level_byte = data_handle.data()[0];
        if (level_byte > (uint8_t)bq::log_level::fatal) {
            context.log_parse_fail_reason("parse format template failed, invalid log level");
            return false;
        }
        // bq::log_level log_level = (bq::log_level)level_byte;
        uint32_t category_idx = 0;
        size_t category_idx_size = bq::log_utils::vlq::vlq_decode(category_idx, data_handle.data() + 1);
        if (category_idx_size == bq::log_utils::vlq::invalid_decode_length) {
            context.log_parse_fail_reason("parse format template failed, category index vlq decode failed");
            return false;
        }
        size_t current_data_cursor = sizeof(level_byte) + category_idx_size;
        if (category_idx >= parent_log_->get_categories_count()) {
            context.log_parse_fail_reason("parse format template failed, invalid category index");
            return false;
        }
        if (current_data_cursor > data_handle.len()) {
            context.log_parse_fail_reason("parse format template failed, invalid category_idx encode size");
            return false;
        }

        // Recover Hash from Content
        uint64_t fmt_str_hash = 0;
        const uint8_t* fmt_data_ptr = data_handle.data() + current_data_cursor;
        size_t fmt_data_len = data_handle.len() - current_data_cursor;

        if (sub_type == template_sub_type::format_template_utf8) {
            // Case A: UTF-8 source. File content IS original content.
            fmt_str_hash = bq::util::get_hash_64(fmt_data_ptr, fmt_data_len);
        } else {
            // Case B: UTF-16 source. File content is UTF-Mixed. Recover original UTF-16 hash.
            fmt_str_hash = bq::util::hash_utf_mixed_as_utf16(fmt_data_ptr, fmt_data_len);
        }

        uint64_t format_template_hash = get_format_template_hash((bq::log_level)level_byte, category_idx, fmt_str_hash);
        format_l2_.insert(format_template_hash, current_format_template_max_index_);
        ++current_format_template_max_index_;
        return true;
    }

    bool appender_file_compressed::parse_thread_info_template(parse_file_context& context, const appender_file_base::read_with_cache_handle& data_handle)
    {
        if (data_handle.len() < 1) {
            context.log_parse_fail_reason("thread info template content size should be equal or larger than 1");
            return false;
        }
        return true;
    }

    void appender_file_compressed::reset()
    {
        format_l2_.clear();
        thread_l2_.clear();
        last_thread_id_ = UINT64_MAX;
        last_thread_info_idx_ = CACHE_EMPTY;
        current_format_template_max_index_ = 0;
        current_thread_info_max_index_ = 0;
        last_log_entry_epoch_ = 0;
        for (uint32_t i = 0; i < FORMAT_L1_SIZE; ++i) {
            format_l1_[i].value = CACHE_EMPTY;
        }
        for (uint32_t i = 0; i < THREAD_L1_SIZE; ++i) {
            thread_l1_[i].value = CACHE_EMPTY;
        }
    }

    // Due to the use of VLQ and character encoding conversions,
    // the actual storage size may be up to 5 times smaller than the initially calculated maximum size.
    // In such cases, it's possible that the final storage space required will be one bit less.
    bq_forceinline uint32_t get_vlq_min_bytes_length_of_item_header(uint64_t value)
    {
        uint32_t bytes_len = bq::log_utils::vlq::get_vlq_encode_length(value);
        return bytes_len > 1 ? (bytes_len - 1) : bytes_len;
    }

    bool appender_file_compressed::log_impl(const log_entry_handle& handle)
    {
        if (!appender_file_base::log_impl(handle)) {
            return false;
        }

        uint32_t format_data_len = handle.get_log_head().log_format_data_len;
        const char* format_data_ptr = handle.get_format_string_data();
        if ((const uint8_t*)format_data_ptr + format_data_len > handle.get_log_args_data()) {
            bq::util::log_device_console(bq::log_level::error, "appender_file_compressed::log_impl invalid format data length:%" PRIu32, format_data_len);
            return false;
        }
        uint64_t fmt_hash = handle.get_log_head().format_hash;
        if (!fmt_hash) {
            fmt_hash = bq::util::get_hash_64(format_data_ptr, (size_t)format_data_len);
        }
        uint64_t format_template_hash = get_format_template_hash(handle.get_level(), handle.get_log_head().category_idx, fmt_hash);

        uint32_t format_template_idx = (uint32_t)-1;
        const uint32_t format_l1_index = static_cast<uint32_t>(mix_cache_key(format_template_hash) >> 32) & (FORMAT_L1_SIZE - 1);
        if (format_l1_[format_l1_index].value != CACHE_EMPTY
            && format_l1_[format_l1_index].key == format_template_hash) {
            format_template_idx = format_l1_[format_l1_index].value;
        } else {
            decltype(format_l2_)::insert_token format_insert_token;
            const bool format_template_found = format_l2_.find(format_template_hash, format_template_idx, format_insert_token);
            // write format template
            if (!format_template_found) {
                constexpr size_t VLQ_MAX_SIZE = bq::log_utils::vlq::vlq_max_bytes_count<uint32_t>();
                uint32_t fmt_size_calculated = 0;
                bool success = true;
                do {
                    size_t fmt_max_size = (fmt_size_calculated) ? (size_t)fmt_size_calculated : ((handle.get_log_head().log_format_str_type == (uint16_t)log_arg_type_enum::string_utf8_type ? format_data_len : ((size_t)(format_data_len * 3) >> 1) + 1)); // 1 additional byte for utf-mixed mark;
                    auto max_format_template_data_size = (uint32_t)(sizeof(uint8_t) + sizeof(uint8_t) + VLQ_MAX_SIZE + fmt_max_size); // level(1 byte), category_idx(VLQ), fmt

                    auto data_len_min_size = get_vlq_min_bytes_length_of_item_header(max_format_template_data_size);
                    auto prealloc_head_size = 1 + data_len_min_size;
                    auto write_handle = alloc_write_cache(max_format_template_data_size + prealloc_head_size);

                    // write format template body first to get the real length, then write header back.
                    uint32_t format_template_data_cursor = prealloc_head_size;

                    if (handle.get_log_head().log_format_str_type == (uint16_t)log_arg_type_enum::string_utf8_type) {
                        write_handle.data()[format_template_data_cursor++] = (uint8_t)template_sub_type::format_template_utf8;
                    } else {
                        write_handle.data()[format_template_data_cursor++] = (uint8_t)template_sub_type::format_template_utf16;
                    }
                    write_handle.data()[format_template_data_cursor++] = (uint8_t)handle.get_level();
                    format_template_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(handle.get_log_head().category_idx, write_handle.data() + format_template_data_cursor, VLQ_MAX_SIZE);
                    if (handle.get_log_head().log_format_str_type == (uint16_t)log_arg_type_enum::string_utf8_type) {
                        fmt_size_calculated = format_data_len;
                        memcpy(write_handle.data() + format_template_data_cursor, format_data_ptr, (size_t)format_data_len);
                        format_template_data_cursor += format_data_len;
                    } else {
                        fmt_size_calculated = bq::util::utf16_to_utf_mixed((const char16_t*)format_data_ptr, format_data_len >> 1, (char*)(uint8_t*)(write_handle.data() + format_template_data_cursor), ((format_data_len * 3) >> 1) + 1);
                        format_template_data_cursor += fmt_size_calculated;
                    }

                    uint32_t real_total_len = format_template_data_cursor;
                    write_handle.reset_used_len(real_total_len);

                    // write back head
                    uint32_t real_body_len = real_total_len - prealloc_head_size;
                    uint32_t data_len_real_size = bq::log_utils::vlq::get_vlq_encode_length((uint64_t)real_body_len);
                    if (data_len_real_size != data_len_min_size) {
                        if (data_len_real_size != 1 + data_len_min_size) {
                            assert(success == true && "utf16 compress error");
                            success = false;
                            write_handle.reset_used_len(0);
                            return_write_cache(write_handle);
                            continue;
                        }
                        bq::log_utils::vlq::vlq_encode(real_body_len, write_handle.data(), data_len_real_size);
                        *write_handle.data() |= (uint8_t)item_type::log_template;
                    } else {
                        bq::log_utils::vlq::vlq_encode(real_body_len, write_handle.data() + 1, data_len_real_size);
                        *write_handle.data() = (uint8_t)item_type::log_template;
                    }
                    return_write_cache(write_handle);
                    success = true;
                } while (!success);

                format_template_idx = current_format_template_max_index_;
                format_l2_.insert(format_template_hash, format_template_idx, format_insert_token);
                ++current_format_template_max_index_;
            }
            format_l1_[format_l1_index].key = format_template_hash;
            format_l1_[format_l1_index].value = format_template_idx;
        }

        uint32_t thread_info_idx = (uint32_t)-1;
        const uint64_t current_thread_id = handle.get_log_head().log_thread_id;
        if (current_thread_id == last_thread_id_) {
            thread_info_idx = last_thread_info_idx_;
        } else {
            const uint32_t thread_l1_index = static_cast<uint32_t>(mix_cache_key(current_thread_id) >> 32) & (THREAD_L1_SIZE - 1);
            if (thread_l1_[thread_l1_index].value != CACHE_EMPTY
                && thread_l1_[thread_l1_index].key == current_thread_id) {
                thread_info_idx = thread_l1_[thread_l1_index].value;
            } else {
                decltype(thread_l2_)::insert_token thread_insert_token;
                const bool thread_info_found = thread_l2_.find(current_thread_id, thread_info_idx, thread_insert_token);
                // write thread_info template
                if (!thread_info_found) {
                    const auto& ext_info = handle.get_ext_head();
                    constexpr size_t VLQ_MAX_SIZE = bq::log_utils::vlq::vlq_max_bytes_count<decltype(current_thread_info_max_index_)>();
                    constexpr size_t VLQ_MAX_SIZE_64 = bq::log_utils::vlq::vlq_max_bytes_count<decltype(handle.get_log_head().log_thread_id)>();
                    auto max_thread_info_data_size = sizeof(uint8_t) + VLQ_MAX_SIZE + VLQ_MAX_SIZE_64 + ext_info.thread_name_len_;
#ifndef NDEBUG
                    assert(max_thread_info_data_size < 64);
#endif // !NDEBUG
                    auto data_len_min_size = bq::log_utils::vlq::get_vlq_encode_length((uint64_t)max_thread_info_data_size);
                    auto prealloc_head_size = 1 + data_len_min_size;
                    auto write_handle = alloc_write_cache(max_thread_info_data_size + prealloc_head_size);
                    assert(data_len_min_size == 1 && "thread info template size error");

                    uint32_t thread_info_data_cursor = prealloc_head_size;
                    write_handle.data()[thread_info_data_cursor++] = (uint32_t)template_sub_type::thread_info_template;
                    thread_info_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(current_thread_info_max_index_, write_handle.data() + thread_info_data_cursor, VLQ_MAX_SIZE);
                    thread_info_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(current_thread_id, write_handle.data() + thread_info_data_cursor, VLQ_MAX_SIZE_64);
                    memcpy(write_handle.data() + thread_info_data_cursor, (const uint8_t*)&ext_info + sizeof(_log_entry_ext_head_def), (size_t)ext_info.thread_name_len_);
                    thread_info_data_cursor += ext_info.thread_name_len_;
                    write_handle.reset_used_len(thread_info_data_cursor);
                    *(uint8_t*)write_handle.data() = (uint8_t)item_type::log_template;
                    uint32_t real_body_len = thread_info_data_cursor - prealloc_head_size;
                    auto real_body_len_size = bq::log_utils::vlq::vlq_encode(real_body_len, write_handle.data() + 1, 1);
                    assert(real_body_len_size == 1 && "thread info template size encoding error");
                    return_write_cache(write_handle);
                    thread_info_idx = current_thread_info_max_index_;
                    thread_l2_.insert(current_thread_id, thread_info_idx, thread_insert_token);
                    ++current_thread_info_max_index_;
                }
                thread_l1_[thread_l1_index].key = current_thread_id;
                thread_l1_[thread_l1_index].value = thread_info_idx;
            }
            last_thread_id_ = current_thread_id;
            last_thread_info_idx_ = thread_info_idx;
        }

        // write log entry
        {
            constexpr size_t VLQ_MAX_SIZE = bq::log_utils::vlq::vlq_max_bytes_count<uint32_t>();
            constexpr size_t VLQ_MAX_SIZE_64 = bq::log_utils::vlq::vlq_max_bytes_count<uint64_t>();

            uint32_t raw_log_args_data_len = handle.get_log_args_data_size();
            auto max_log_data_size = VLQ_MAX_SIZE + VLQ_MAX_SIZE + VLQ_MAX_SIZE + ((size_t)raw_log_args_data_len << 1); // format template idx(VLQ), epoch offset milliseconds(VLQ), args(*2, mabe wast, but can ensure utf16 can properly trans to utf-mixed, consider vlq size my increate 1 bytes to, so use *2 instead of *3/2 + 1)

            auto data_len_min_size = get_vlq_min_bytes_length_of_item_header(max_log_data_size);
            auto prealloc_head_size = 1 + data_len_min_size;
            auto write_handle = alloc_write_cache(max_log_data_size + prealloc_head_size);

            // write log entry body first to get the real length, then write header back.
            uint32_t log_data_cursor = prealloc_head_size;

            auto log_epoch = handle.get_log_head().timestamp_epoch;
            // in particular case, log epoch may less than base epoch time.
            int64_t epoch_offset = (int64_t)(log_epoch - last_log_entry_epoch_);
            last_log_entry_epoch_ = log_epoch;
            uint64_t zigzag_epoch_offset = bq::log_utils::zigzag::encode(epoch_offset);
            log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(zigzag_epoch_offset, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE_64);
            log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(format_template_idx, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
            log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(thread_info_idx, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);

            // write log params
            {
                const uint8_t* const args_data_ptr = handle.get_log_args_data();
                uint32_t args_data_cursor = 0;
                while (args_data_cursor < raw_log_args_data_len) {
                    uint8_t type_info_i = *(args_data_ptr + args_data_cursor);
                    bq::log_arg_type_enum type_info = (bq::log_arg_type_enum)(type_info_i);
                    write_handle.data()[log_data_cursor++] = type_info_i;
                    switch (type_info) {
                    case bq::log_arg_type_enum::unsupported_type:
                        bq::util::log_device_console(bq::log_level::warning, "appender_file_compressed : non_primitivi_type is not supported yet, type:%" PRId32, (int32_t)type_info);
                        args_data_cursor = raw_log_args_data_len;
                        break;
                    case bq::log_arg_type_enum::null_type:
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::pointer_type:
                        assert(sizeof(void*) >= 4);
                        memcpy(write_handle.data() + log_data_cursor, args_data_ptr + args_data_cursor + 4, sizeof(uint64_t));
                        log_data_cursor += static_cast<uint32_t>(sizeof(uint64_t));
                        args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint64_t)); // use 64bit pointer for serialize
                        break;
                    case bq::log_arg_type_enum::bool_type:
                    case bq::log_arg_type_enum::char_type:
                    case bq::log_arg_type_enum::int8_type:
                    case bq::log_arg_type_enum::uint8_type:
                        write_handle.data()[log_data_cursor] = *(args_data_ptr + args_data_cursor + 2);
                        log_data_cursor++;
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::char16_type:
                    case bq::log_arg_type_enum::uint16_type:
                        log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(*(const uint16_t*)(args_data_ptr + args_data_cursor + 2), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::int16_type:
                        log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(bq::log_utils::zigzag::encode(*(const int16_t*)(args_data_ptr + args_data_cursor + 2)), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::char32_type:
                    case bq::log_arg_type_enum::uint32_type:
                        log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(*(const uint32_t*)(args_data_ptr + args_data_cursor + 4), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
                        args_data_cursor += static_cast<uint32_t>(4U + sizeof(int32_t));
                        break;
                    case bq::log_arg_type_enum::int32_type:
                        log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(bq::log_utils::zigzag::encode(*(const int32_t*)(args_data_ptr + args_data_cursor + 4)), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
                        args_data_cursor += static_cast<uint32_t>(4U + sizeof(int32_t));
                        break;
                    case bq::log_arg_type_enum::float_type:
                        memcpy(write_handle.data() + log_data_cursor, args_data_ptr + args_data_cursor + 4, sizeof(int32_t));
                        log_data_cursor += static_cast<uint32_t>(sizeof(int32_t));
                        args_data_cursor += static_cast<uint32_t>(4U + sizeof(int32_t));
                        break;
                    case bq::log_arg_type_enum::uint64_type:
                        log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(*(const uint64_t*)(args_data_ptr + args_data_cursor + 4), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE_64);
                        args_data_cursor += static_cast<uint32_t>(4U + sizeof(int64_t));
                        break;
                    case bq::log_arg_type_enum::int64_type:
                        log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(bq::log_utils::zigzag::encode(*(const int64_t*)(args_data_ptr + args_data_cursor + 4)), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE_64);
                        args_data_cursor += static_cast<uint32_t>(4U + sizeof(int64_t));
                        break;
                    case bq::log_arg_type_enum::double_type:
                        memcpy(write_handle.data() + log_data_cursor, args_data_ptr + args_data_cursor + 4, sizeof(int64_t));
                        log_data_cursor += static_cast<uint32_t>(sizeof(int64_t));
                        args_data_cursor += static_cast<uint32_t>(4 + sizeof(int64_t));
                        break;
                    case bq::log_arg_type_enum::string_utf8_type: {
                        const uint32_t* len_ptr = (const uint32_t*)(args_data_ptr + args_data_cursor + 4);
                        uint32_t str_len = *len_ptr;
                        log_data_cursor += (uint32_t)bq::log_utils::vlq::vlq_encode(str_len, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
                        memcpy(write_handle.data() + log_data_cursor, args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t), str_len);
                        log_data_cursor += str_len;
                        args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                    } break;
                    case bq::log_arg_type_enum::string_utf16_type: {
                        // trans to utf-mixed to get best balance of size and performance
                        write_handle.data()[log_data_cursor - 1] = (uint8_t)bq::log_arg_type_enum::string_utf_mixed_type;
                        const uint32_t* len_ptr = (const uint32_t*)(args_data_ptr + args_data_cursor + 4);
                        uint32_t str_len = *len_ptr;

                        uint32_t max_utf8_str_len = ((str_len * 3) >> 1) + 1;
                        auto pre_len_size = bq::log_utils::vlq::get_vlq_encode_length((uint32_t)max_utf8_str_len);

                        uint32_t utf_mixed_len = bq::util::utf16_to_utf_mixed((const char16_t*)(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t)), str_len >> 1, (char*)(write_handle.data() + log_data_cursor + pre_len_size), max_utf8_str_len);

                        uint32_t real_len_size = (uint32_t)bq::log_utils::vlq::vlq_encode(utf_mixed_len, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);

                        assert((real_len_size == pre_len_size || (real_len_size + 1 == pre_len_size)) && "compressed log, utf16 arguments write error");
                        if (real_len_size + 1 == pre_len_size) {
                            write_handle.data()[log_data_cursor + real_len_size] = 0; // 0 placeholder, if the pre-estimated size is not accurate
                        }
                        log_data_cursor += (pre_len_size + utf_mixed_len);
                        args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                    } break;
                    default:
                        break;
                    }
                    continue;
                }
            }
            // write back head
            uint32_t real_total_len = log_data_cursor;
            write_handle.reset_used_len(real_total_len);
            uint32_t real_body_len = real_total_len - prealloc_head_size;
            uint32_t data_len_real_size = bq::log_utils::vlq::get_vlq_encode_length((uint64_t)real_body_len);
            if (data_len_real_size != data_len_min_size) {
                if (data_len_real_size != 1 + data_len_min_size) {
                    assert(false && "error while encoding compressed file log item data header");
                }
                bq::log_utils::vlq::vlq_encode(real_body_len, write_handle.data(), data_len_real_size);
                *write_handle.data() |= (uint8_t)item_type::log_entry;
            } else {
                bq::log_utils::vlq::vlq_encode(real_body_len, write_handle.data() + 1, data_len_real_size);
                *write_handle.data() = (uint8_t)item_type::log_entry;
            }
            return_write_cache(write_handle);
        }
        mark_write_finished();
        return true;
    }
}
