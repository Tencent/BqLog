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
 */
#include "bq_log/log/log_imp.h"
#include "bq_log/log/appender/appender_file_compressed.h"
#include "bq_log/utils/log_utils.h"
#include "bq_common/bq_common.h"

namespace bq {
    template <typename T, bool is_aligned>
    struct data_value_helper {
        static T get_data_value(const void* data)
        {
            return *(T*)data;
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

    inline bool is_addr_8_aligned(const void* data)
    {
        constexpr size_t mask_8_bytes_align = (sizeof(uint64_t) - 1);
        return ((size_t)data & mask_8_bytes_align) == 0;
    }

    template <bool is_aligned>
    inline uint64_t calculate_hash_64_for_compressed_appender(const void* data, size_t size)
    {
#ifndef NDEBUG
        if (is_aligned) {
            assert(is_addr_8_aligned(data)
                && "data of calculate_hash_64_when_logging should be 8 bytes aligned");
        }
#endif
        if (!is_aligned) {
            if (is_addr_8_aligned(data)) {
                return calculate_hash_64_for_compressed_appender<true>(data, size);
            }
        }

        constexpr uint64_t fnv_offset_basis = 14695981039346656037ull;
        constexpr uint64_t fnv_prime = 1099511628211ull;

        const uint8_t* end = (const uint8_t*)data + size;
        const uint8_t* cursor = (const uint8_t*)data;
        uint64_t hash = fnv_offset_basis;
        while (cursor + sizeof(uint64_t) <= end) {
            hash ^= static_cast<uint64_t>(data_value_helper<uint64_t, is_aligned>::get_data_value(cursor));
            hash *= fnv_prime;
            cursor += sizeof(uint64_t);
        }
        while (cursor + sizeof(uint32_t) <= end) {
            hash ^= static_cast<uint64_t>(data_value_helper<uint32_t, is_aligned>::get_data_value(cursor));
            hash *= fnv_prime;
            cursor += sizeof(uint32_t);
        }
        for (; cursor < end; ++cursor) {
            hash ^= static_cast<uint64_t>(*cursor);
            hash *= fnv_prime;
            cursor += sizeof(uint8_t);
        }
        return hash;
    }

#if BQ_UNIT_TEST
    bool appender_file_compressed::is_addr_8_aligned_test(const void* data)
    {
        return is_addr_8_aligned(data);
    }

    uint64_t appender_file_compressed::calculate_data_hash_test(bool is_8_aligned, const void* data, size_t size)
    {
        if (is_8_aligned) {
            return calculate_hash_64_for_compressed_appender<true>(data, size);
        } else {
            return calculate_hash_64_for_compressed_appender<false>(data, size);
        }
    }
#endif

    bool appender_file_compressed::init_impl(const bq::property_value& config_obj)
    {
        appender_file_binary::init_impl(config_obj);
        thread_info_hash_cache_.set_expand_rate(4);
        format_templates_hash_cache_.set_expand_rate(4);
        return true;
    }

    void appender_file_compressed::on_file_open(bool is_new_created)
    {
        appender_file_binary::on_file_open(is_new_created);
        if (is_new_created) {
            reset();
        }
    }

    static const bq::string log_file_ext_name_compressed = ".logcompr";
    bq::string appender_file_compressed::get_file_ext_name()
    {
        return log_file_ext_name_compressed;
    }

    uint32_t appender_file_compressed::get_binary_format_version() const
    {
        return format_version;
    }

    template <typename T>
    struct _st_appender_file_exist_callback {
    private:
        T callback_;

    public:
        _st_appender_file_exist_callback(const T& callback)
            : callback_(callback)
        {
        }
        ~_st_appender_file_exist_callback() { callback_(); }
    };

    bool appender_file_compressed::parse_exist_log_file(parse_file_context& context)
    {
        if (!appender_file_binary::parse_exist_log_file(context)) {
            return false;
        }
        reset();
        auto release_cache_lamda = [this]() {
            utf16_trans_cache_.reset();
        };
        _st_appender_file_exist_callback<decltype(release_cache_lamda)> cache_reset_obj(release_cache_lamda);
        while (true) {
            if (context.parsed_size == get_current_file_size()) {
                // parse finished
                return true;
            }
            auto read_result = read_item_data(context);
            if (!bq::get<0>(read_result)) {
                return false;
            }
            switch (bq::get<1>(read_result)) {
            case bq::appender_file_compressed::log_template:
                switch (*bq::get<2>(read_result).data()) {
                case template_sub_type::format_template:
                    if (!parse_formate_template(context, bq::get<2>(read_result).offset(1))) {
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
        constexpr size_t VLQ_MAX_SIZE = bq::vlq::vlq_max_bytes_count<uint32_t>();
        auto read_handle = read_with_cache(VLQ_MAX_SIZE + 1);
        if (read_handle.len() < 2) {
            context.parsed_size += read_handle.len();
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
        size_t size_len = bq::vlq::vlq_decode(data_size, read_handle.data() + offset);
        if (size_len == bq::vlq::invalid_decode_length) {
            return bq::make_tuple(false, type, read_handle);
        }
        if (offset == 0) {
            first_byte |= (uint8_t)type;
        }
        context.parsed_size += size_len + offset;
        seek_read_file_offset((int32_t)(size_len + offset) - (int32_t)read_handle.len());
        read_handle = read_with_cache(data_size);
        context.parsed_size += read_handle.len();
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
        uint64_t epoch_offset;
        if (bq::vlq::vlq_decode(epoch_offset, data_handle.data()) == bq::vlq::invalid_decode_length) {
            context.log_parse_fail_reason("log entry epoch_offset decode failed");
            return false;
        }
        last_log_entry_epoch_ += epoch_offset;
        return true;
    }

    bool appender_file_compressed::parse_formate_template(parse_file_context& context, const appender_file_base::read_with_cache_handle& data_handle)
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
        bq::log_level log_level = (bq::log_level)level_byte;
        uint32_t category_idx = 0;
        size_t category_idx_size = bq::vlq::vlq_decode(category_idx, data_handle.data() + 1);
        if (category_idx_size == bq::vlq::invalid_decode_length) {
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
        size_t fmt_str_len = data_handle.len() - current_data_cursor;
        uint64_t utf8_hash = calculate_hash_64_for_compressed_appender<false>(data_handle.data() + current_data_cursor, fmt_str_len);
        uint64_t format_template_hash_utf8 = get_format_template_hash(log_level, category_idx, utf8_hash);
        if (utf16_trans_cache_.size() < fmt_str_len) {
            // that's enough
            utf16_trans_cache_.fill_uninitialized(fmt_str_len - utf16_trans_cache_.size());
        }
        auto utf16_len = (fmt_str_len > 0) ? bq::util::utf8_to_utf16((const char*)(const uint8_t*)(data_handle.data() + current_data_cursor), (uint32_t)fmt_str_len, &utf16_trans_cache_[0], (uint32_t)utf16_trans_cache_.size())
                                           : 0;
        uint64_t utf16_hash = calculate_hash_64_for_compressed_appender<false>(utf16_trans_cache_.begin(), (size_t)utf16_len * sizeof(decltype(utf16_trans_cache_)::value_type));
        uint64_t format_template_hash_utf16 = get_format_template_hash(log_level, category_idx, utf16_hash);
        format_templates_hash_cache_[format_template_hash_utf8] = current_format_template_max_index_;
        format_templates_hash_cache_[format_template_hash_utf16] = current_format_template_max_index_;
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

    uint64_t appender_file_compressed::get_format_template_hash(bq::log_level level, uint32_t category_idx, uint64_t fmt_str_hash)
    {
        const uint64_t fnv_prime = 1099511628211ull;

        uint64_t hash = fmt_str_hash;
        hash ^= static_cast<uint64_t>(level);
        hash *= fnv_prime;
        hash ^= static_cast<uint64_t>(category_idx);
        hash *= fnv_prime;
        return hash;
    }

    void appender_file_compressed::reset()
    {
        format_templates_hash_cache_.clear();
        thread_info_hash_cache_.clear();
        current_format_template_max_index_ = 0;
        current_thread_info_max_index_ = 0;
        last_log_entry_epoch_ = 0;
    }

    // Due to the use of VLQ and character encoding conversions,
    // the actual storage size may be up to 5 times smaller than the initially calculated maximum size.
    // In such cases, it's possible that the final storage space required will be one bit less.
    bq_forceinline uint32_t get_vlq_min_bytes_length_of_item_header(uint64_t value)
    {
        uint32_t bytes_len = bq::vlq::get_vlq_encode_length(value);
        return bytes_len > 1 ? (bytes_len - 1) : bytes_len;
    }

    void appender_file_compressed::log_impl(const log_entry_handle& handle)
    {
        appender_file_binary::log_impl(handle);

        const char* format_data_ptr = handle.get_format_string_data();
        uint32_t format_data_len = *((uint32_t*)format_data_ptr);
        format_data_ptr += sizeof(uint32_t);
        uint64_t fmt_hash = calculate_hash_64_for_compressed_appender<true>(format_data_ptr, (size_t)format_data_len);
        uint64_t format_template_hash = get_format_template_hash(handle.get_level(), handle.get_log_head().category_idx, fmt_hash);

        auto format_template_iter = format_templates_hash_cache_.find(format_template_hash);
        uint32_t format_template_idx = (uint32_t)-1;
        // write format template
        if (format_template_iter == format_templates_hash_cache_.end()) {
            constexpr size_t VLQ_MAX_SIZE = bq::vlq::vlq_max_bytes_count<uint32_t>();
            size_t fmt_max_size = (handle.get_log_head().log_format_str_type == (uint16_t)log_arg_type_enum::string_utf8_type ? format_data_len : ((size_t)(format_data_len * 3) >> 1) + 1);
            auto max_format_template_data_size = (uint32_t)(sizeof(uint8_t) + sizeof(uint8_t) + VLQ_MAX_SIZE + fmt_max_size); // level(1 byte), category_idx(VLQ), fmt

            auto data_len_min_size = get_vlq_min_bytes_length_of_item_header(max_format_template_data_size);
            auto prealloc_head_size = 1 + data_len_min_size;
            auto write_handle = write_with_cache_alloc(max_format_template_data_size + prealloc_head_size);

            // write format template body first to get the real length, then write header back.
            uint32_t format_template_data_cursor = prealloc_head_size;
            write_handle.data()[format_template_data_cursor++] = (uint8_t)template_sub_type::format_template;
            write_handle.data()[format_template_data_cursor++] = (uint8_t)handle.get_level();
            format_template_data_cursor += (uint32_t)bq::vlq::vlq_encode(handle.get_log_head().category_idx, write_handle.data() + format_template_data_cursor, VLQ_MAX_SIZE);
            if (handle.get_log_head().log_format_str_type == (uint16_t)log_arg_type_enum::string_utf8_type) {
                memcpy(write_handle.data() + format_template_data_cursor, format_data_ptr, (size_t)format_data_len);
                format_template_data_cursor += format_data_len;
            } else {
                format_template_data_cursor += bq::util::utf16_to_utf8((const char16_t*)format_data_ptr, format_data_len >> 1, (char*)(uint8_t*)(write_handle.data() + format_template_data_cursor), ((format_data_len * 3) >> 1) + 1);
            }
            uint32_t real_total_len = format_template_data_cursor;
            write_handle.reset_used_len(real_total_len);

            // write back head
            uint32_t real_body_len = real_total_len - prealloc_head_size;
            uint32_t data_len_real_size = bq::vlq::get_vlq_encode_length((uint64_t)real_body_len);
            if (data_len_real_size != data_len_min_size) {
                if (data_len_real_size != 1 + data_len_min_size) {
                    assert(false && "error while encoding compressed file format template header");
                }
                bq::vlq::vlq_encode(real_body_len, write_handle.data(), data_len_real_size);
                *write_handle.data() |= (uint8_t)item_type::log_template;
            } else {
                bq::vlq::vlq_encode(real_body_len, write_handle.data() + 1, data_len_real_size);
                *write_handle.data() = (uint8_t)item_type::log_template;
            }

            write_with_cache_commit(write_handle);
            format_templates_hash_cache_[format_template_hash] = current_format_template_max_index_;
            format_template_idx = current_format_template_max_index_;
            ++current_format_template_max_index_;
        } else {
            format_template_idx = format_template_iter->value();
        }

        const auto& ext_info = handle.get_ext_head();
        auto thread_info__iter = thread_info_hash_cache_.find(ext_info.thread_id_);
        uint32_t thread_info_idx = (uint32_t)-1;
        // write thread_info template
        if (thread_info__iter == thread_info_hash_cache_.end()) {
            constexpr size_t VLQ_MAX_SIZE = bq::vlq::vlq_max_bytes_count<decltype(current_thread_info_max_index_)>();
            constexpr size_t VLQ_MAX_SIZE_64 = bq::vlq::vlq_max_bytes_count<decltype(ext_info.thread_id_)>();
            auto max_thread_info_data_size = sizeof(uint8_t) + VLQ_MAX_SIZE + VLQ_MAX_SIZE_64 + ext_info.thread_name_len_;
#ifndef NDEBUG
            assert(max_thread_info_data_size < 64);
#endif // !NDEBUG
            auto data_len_min_size = bq::vlq::get_vlq_encode_length((uint64_t)max_thread_info_data_size);
            auto prealloc_head_size = 1 + data_len_min_size;
            auto write_handle = write_with_cache_alloc(max_thread_info_data_size + prealloc_head_size);
            assert(data_len_min_size == 1 && "thread info template size error");

            uint32_t thread_info_data_cursor = prealloc_head_size;
            write_handle.data()[thread_info_data_cursor++] = (uint32_t)template_sub_type::thread_info_template;
            thread_info_data_cursor += (uint32_t)bq::vlq::vlq_encode(current_thread_info_max_index_, write_handle.data() + thread_info_data_cursor, VLQ_MAX_SIZE);
            thread_info_data_cursor += (uint32_t)bq::vlq::vlq_encode(ext_info.thread_id_, write_handle.data() + thread_info_data_cursor, VLQ_MAX_SIZE_64);
            memcpy(write_handle.data() + thread_info_data_cursor, (uint8_t*)&ext_info + sizeof(ext_log_entry_info_head), (size_t)ext_info.thread_name_len_);
            thread_info_data_cursor += ext_info.thread_name_len_;
            write_handle.reset_used_len(thread_info_data_cursor);
            *(uint8_t*)write_handle.data() = (uint8_t)item_type::log_template;
            uint32_t real_body_len = thread_info_data_cursor - prealloc_head_size;
            auto real_body_len_size = bq::vlq::vlq_encode(real_body_len, write_handle.data() + 1, 1);
            assert(real_body_len_size == 1 && "thread info template size encoding error");
            write_with_cache_commit(write_handle);
            thread_info_hash_cache_[ext_info.thread_id_] = current_thread_info_max_index_;
            thread_info_idx = current_thread_info_max_index_;
            ++current_thread_info_max_index_;
        } else {
            thread_info_idx = thread_info__iter->value();
        }

        // write log entry
        {
            constexpr size_t VLQ_MAX_SIZE = bq::vlq::vlq_max_bytes_count<uint32_t>();
            constexpr size_t VLQ_MAX_SIZE_64 = bq::vlq::vlq_max_bytes_count<uint64_t>();

            uint32_t raw_log_args_data_len = handle.get_log_args_data_size();
            auto max_log_data_size = VLQ_MAX_SIZE + VLQ_MAX_SIZE + VLQ_MAX_SIZE + ((size_t)raw_log_args_data_len << 1); // format template idx(VLQ), epoch offset milliseconds(VLQ), args(*2, mabe wast, but can ensure utf16 can properly trans to utf8, consider vlq size my increate 1 bytes to, so use *2 instead of *3/2 + 1)

            auto data_len_min_size = get_vlq_min_bytes_length_of_item_header(max_log_data_size);
            auto prealloc_head_size = 1 + data_len_min_size;
            auto write_handle = write_with_cache_alloc(max_log_data_size + prealloc_head_size);

            // write log entry body first to get the real length, then write header back.
            uint32_t log_data_cursor = prealloc_head_size;

            auto log_epoch = handle.get_log_head().timestamp_epoch;
            // in particular case, log epoch may less than base epoch time.
            uint64_t epoch_offset = (log_epoch > last_log_entry_epoch_) ? (log_epoch - last_log_entry_epoch_) : 0;
            last_log_entry_epoch_ += epoch_offset;
            log_data_cursor += (uint32_t)bq::vlq::vlq_encode(epoch_offset, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE_64);
            log_data_cursor += (uint32_t)bq::vlq::vlq_encode(format_template_idx, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
            log_data_cursor += (uint32_t)bq::vlq::vlq_encode(thread_info_idx, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);

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
                        bq::util::log_device_console(bq::log_level::warning, "appender_file_compressed : non_primitivi_type is not supported yet, type:%d", (int32_t)type_info);
                        break;
                    case bq::log_arg_type_enum::null_type:
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::pointer_type:
                        assert(sizeof(void*) >= 4);
                        memcpy(write_handle.data() + log_data_cursor, args_data_ptr + args_data_cursor + 4, sizeof(uint64_t));
                        log_data_cursor += sizeof(uint64_t);
                        args_data_cursor += 4 + sizeof(uint64_t); // use 64bit pointer for serialize
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
                    case bq::log_arg_type_enum::int16_type:
                    case bq::log_arg_type_enum::uint16_type:
                        log_data_cursor += (uint32_t)bq::vlq::vlq_encode(*(uint16_t*)(args_data_ptr + args_data_cursor + 2), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::char32_type:
                    case bq::log_arg_type_enum::int32_type:
                    case bq::log_arg_type_enum::uint32_type:
                        log_data_cursor += (uint32_t)bq::vlq::vlq_encode(*(uint32_t*)(args_data_ptr + args_data_cursor + 4), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
                        args_data_cursor += (4 + sizeof(int32_t));
                        break;
                    case bq::log_arg_type_enum::float_type:
                        memcpy(write_handle.data() + log_data_cursor, args_data_ptr + args_data_cursor + 4, sizeof(int32_t));
                        log_data_cursor += sizeof(int32_t);
                        args_data_cursor += (4 + sizeof(int32_t));
                        break;
                    case bq::log_arg_type_enum::int64_type:
                    case bq::log_arg_type_enum::uint64_type:
                        log_data_cursor += (uint32_t)bq::vlq::vlq_encode(*(uint64_t*)(args_data_ptr + args_data_cursor + 4), write_handle.data() + log_data_cursor, VLQ_MAX_SIZE_64);
                        args_data_cursor += (4 + sizeof(int64_t));
                        break;
                    case bq::log_arg_type_enum::double_type:
                        memcpy(write_handle.data() + log_data_cursor, args_data_ptr + args_data_cursor + 4, sizeof(int64_t));
                        log_data_cursor += sizeof(int64_t);
                        args_data_cursor += (4 + sizeof(int64_t));
                        break;
                    case bq::log_arg_type_enum::string_utf8_type: {
                        const uint32_t* len_ptr = (const uint32_t*)(args_data_ptr + args_data_cursor + 4);
                        uint32_t str_len = *len_ptr;
                        log_data_cursor += (uint32_t)bq::vlq::vlq_encode(str_len, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);
                        memcpy(write_handle.data() + log_data_cursor, args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t), str_len);
                        log_data_cursor += str_len;
                        args_data_cursor += 4 + sizeof(uint32_t) + (uint32_t)bq::align_4(str_len);
                    } break;
                    case bq::log_arg_type_enum::string_utf16_type: {
                        // trans to utf-8 to save storage space
                        write_handle.data()[log_data_cursor - 1] = (uint8_t)bq::log_arg_type_enum::string_utf8_type;
                        const uint32_t* len_ptr = (const uint32_t*)(args_data_ptr + args_data_cursor + 4);
                        uint32_t str_len = *len_ptr;

                        uint32_t max_utf8_str_len = ((str_len * 3) >> 1) + 1;
                        auto pre_len_size = bq::vlq::get_vlq_encode_length((uint32_t)max_utf8_str_len);

                        uint32_t utf8_len = bq::util::utf16_to_utf8((const char16_t*)(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t)), str_len >> 1, (char*)(write_handle.data() + log_data_cursor + pre_len_size), max_utf8_str_len);
                        if (utf8_len < (str_len >> 1)) {
                            // This must be an invalid UTF-16 string, so just directly copy the UTF-16 in. Implement a protection.
                            utf8_len = str_len;
                            memcpy(write_handle.data() + log_data_cursor + pre_len_size, args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t), str_len);
                        }
                        uint32_t real_len_size = (uint32_t)bq::vlq::vlq_encode(utf8_len, write_handle.data() + log_data_cursor, VLQ_MAX_SIZE);

                        assert((real_len_size == pre_len_size || (real_len_size + 1 == pre_len_size)) && "compressed log, utf16 arguments write error");
                        if (real_len_size + 1 == pre_len_size) {
                            write_handle.data()[log_data_cursor + real_len_size] = 0; // 0 placeholder, if the pre-estimated size is not accurate
                        }
                        log_data_cursor += (pre_len_size + utf8_len);
                        args_data_cursor += 4 + sizeof(uint32_t) + (uint32_t)bq::align_4(str_len);
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
            uint32_t data_len_real_size = bq::vlq::get_vlq_encode_length((uint64_t)real_body_len);
            if (data_len_real_size != data_len_min_size) {
                if (data_len_real_size != 1 + data_len_min_size) {
                    assert(false && "error while encoding compressed file log item data header");
                }
                bq::vlq::vlq_encode(real_body_len, write_handle.data(), data_len_real_size);
                *write_handle.data() |= (uint8_t)item_type::log_entry;
            } else {
                bq::vlq::vlq_encode(real_body_len, write_handle.data() + 1, data_len_real_size);
                *write_handle.data() = (uint8_t)item_type::log_entry;
            }
            write_with_cache_commit(write_handle);
        }
    }
}
