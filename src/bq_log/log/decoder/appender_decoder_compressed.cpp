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
#include <string.h>
#include "bq_log/log/decoder/appender_decoder_compressed.h"
#include "bq_log/log/appender/appender_file_compressed.h"
#include "bq_log/utils/log_utils.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/log_types.h"

bq::appender_decode_result bq::appender_decoder_compressed::init_private()
{
    log_templates_array_.clear();
    thread_info_templates_map_.set_expand_rate(4);
    last_log_entry_epoch_ = 0;
    return appender_decode_result::success;
}

bq::appender_decode_result bq::appender_decoder_compressed::decode_private()
{
    decoded_text_.clear();
    appender_decode_result result = appender_decode_result::success;
    while (true) {
        auto read_result = read_item_data();
        result = bq::get<0>(read_result);
        if (result != appender_decode_result::success) {
            return result;
        }
        appender_file_compressed::item_type type = bq::get<1>(read_result);
        switch (type) {
        case bq::appender_file_compressed::item_type::log_template:
            switch (*bq::get<2>(read_result).data()) {
            case bq::appender_file_compressed::template_sub_type::format_template:
                result = parse_formate_template(bq::get<2>(read_result).offset(1));
                break;
            case bq::appender_file_compressed::template_sub_type::thread_info_template:
                result = parse_thread_info_template(bq::get<2>(read_result).offset(1));
                break;
            default:
                bq::util::log_device_console_plain_text(bq::log_level::error, "decode compressed log file failed, invalid template sub type");
                return appender_decode_result::failed_decode_error;
                break;
            }
            break;
        case bq::appender_file_compressed::item_type::log_entry:
            result = parse_log_entry(bq::get<2>(read_result));
            break;
        default:
            bq::util::log_device_console(log_level::error, "decode compressed log file failed, unknown data item type %d", (int32_t)type);
            return appender_decode_result::failed_io_error;
            break;
        }
        if (type == bq::appender_file_compressed::item_type::log_entry) {
            break;
        }
    }

    return result;
}

uint32_t bq::appender_decoder_compressed::get_binary_format_version() const
{
    return appender_file_compressed::format_version;
}

bq::tuple<bq::appender_decode_result, bq::appender_file_compressed::item_type, bq::appender_decoder_base::read_with_cache_handle> bq::appender_decoder_compressed::read_item_data()
{
    constexpr size_t VLQ_MAX_SIZE = bq::log_utils::vlq::vlq_max_bytes_count<uint32_t>();
    auto read_handle = read_with_cache(VLQ_MAX_SIZE + 1);
    if (read_handle.len() == 0) {
        return bq::make_tuple(bq::appender_decode_result::eof, appender_file_compressed::item_type::log_template, read_handle);
    }
    if (read_handle.len() < 2) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, read data item head failed");
        return bq::make_tuple(bq::appender_decode_result::failed_decode_error, appender_file_compressed::item_type::log_template, read_handle);
    }
    uint8_t& first_byte = *const_cast<uint8_t*>(read_handle.data());
    int32_t offset = ((first_byte & 0x7F) == 0) ? 1 : 0; // 0b01111111
    auto type = (appender_file_compressed::item_type)(first_byte & 0x80); // 0b10000000
    uint32_t data_size = 0;
    if (offset == 0) {
        first_byte &= 0x7F; // 0b01111111
    }
    size_t size_len = bq::log_utils::vlq::vlq_decode(data_size, read_handle.data() + offset);
    if (bq::log_utils::vlq::invalid_decode_length == size_len) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, decode data size failed");
        return bq::make_tuple(bq::appender_decode_result::failed_decode_error, appender_file_compressed::item_type::log_template, read_handle);
    }
    if (offset == 0) {
        first_byte |= (uint8_t)type;
    }
    seek_read_file_offset(static_cast<int32_t>(size_len) + offset - static_cast<int32_t>(read_handle.len()));
    read_handle = read_with_cache(data_size);
    if (read_handle.len() != (size_t)data_size || data_size < 2) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, read data item head failed");
        return bq::make_tuple(bq::appender_decode_result::failed_decode_error, appender_file_compressed::item_type::log_template, read_handle);
    }
    return bq::make_tuple(bq::appender_decode_result::success, type, read_handle);
}

bq::appender_decode_result bq::appender_decoder_compressed::parse_formate_template(const appender_decoder_base::read_with_cache_handle& read_handle)
{
    if (read_handle.len() < 2) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, format template data item too short");
        return appender_decode_result::failed_decode_error;
    }
    size_t cursor = 0;
    log_templates_array_.push_back(decoder_log_template());
    decoder_log_template& info = log_templates_array_[log_templates_array_.size() - 1];
    info.level = (bq::log_level)read_handle.data()[cursor++];
    size_t size_len = bq::log_utils::vlq::vlq_decode(info.category_idx, read_handle.data() + cursor);
    if (bq::log_utils::vlq::invalid_decode_length == size_len) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, formate template category idx vlq decode failed");
        return appender_decode_result::failed_decode_error;
    }
    cursor += size_len;
    if (cursor > read_handle.len()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, formate template data vlq decode error");
        return appender_decode_result::failed_decode_error;
    }
    if (info.category_idx >= category_names_.size()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, formate template data vlq decode error, invalid category index:%d", info.category_idx);
        return appender_decode_result::failed_decode_error;
    }
    if (cursor < read_handle.len()) {
        info.fmt_string.insert_batch(info.fmt_string.begin(), (const char*)(const uint8_t*)read_handle.data() + cursor, (read_handle.len() - cursor));
    }
    return appender_decode_result::success;
}

bq::appender_decode_result bq::appender_decoder_compressed::parse_thread_info_template(const appender_decoder_base::read_with_cache_handle& read_handle)
{
    uint32_t thread_info_idx = 0;
    const size_t thread_info_idx_len = bq::log_utils::vlq::vlq_decode(thread_info_idx, read_handle.data());
    if (bq::log_utils::vlq::invalid_decode_length == thread_info_idx_len) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, parse_thread_info_template  thread_info_idx vlq decode error");
        return appender_decode_result::failed_decode_error;
    }
    auto read_cursor = static_cast<uint32_t>(thread_info_idx_len);
    if (read_cursor >= read_handle.len()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, parse_thread_info_template vlq decode error, index length exceed data length: index length:%" PRIu32 ", data length:%" PRIu64, read_cursor, static_cast<uint64_t>(read_handle.len()));
        return appender_decode_result::failed_decode_error;
    }
    uint64_t thread_id = 0;
    size_t thread_id_len = bq::log_utils::vlq::vlq_decode(thread_id, read_handle.data() + read_cursor);
    if (bq::log_utils::vlq::invalid_decode_length == thread_id_len) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, thread_id vlq decode error");
        return appender_decode_result::failed_decode_error;
    }
    read_cursor += (uint32_t)thread_id_len;
    if (read_cursor > read_handle.len()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, parse_thread_info_template vlq decode error, thread id length exceed data length: thread id data to:%" PRIu32 ", data length:%" PRIu64, read_cursor, static_cast<uint64_t>(read_handle.len()));
        return appender_decode_result::failed_decode_error;
    }
    decoder_thread_info_template& info = thread_info_templates_map_[thread_info_idx];
    info.thread_id = thread_id;
    info.thread_name.clear();
    info.thread_name.fill_uninitialized(read_handle.len() - read_cursor);
    if (info.thread_name.size() > 0) {
        memcpy(&info.thread_name[0], read_handle.data() + read_cursor, read_handle.len() - read_cursor);
    }
    return appender_decode_result::success;
}

bq::appender_decode_result bq::appender_decoder_compressed::parse_log_entry(const appender_decoder_base::read_with_cache_handle& read_handle)
{
    if (read_handle.len() < 2) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, log entry data item too short");
        return appender_decode_result::failed_decode_error;
    }
    size_t cursor = 0;
    uint64_t epoch_offset_zigzag_ms;
    const size_t epoch_diff_ms_len = bq::log_utils::vlq::vlq_decode(epoch_offset_zigzag_ms, read_handle.data() + cursor);
    if (epoch_diff_ms_len == bq::log_utils::vlq::invalid_decode_length) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, decode epoch_diff_ms failed");
        return appender_decode_result::failed_decode_error;
    }
    cursor += epoch_diff_ms_len;
    if (cursor > read_handle.len()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, log entry epoch_diff_ms vlq decode error");
        return appender_decode_result::failed_decode_error;
    }
    int64_t epoch_offset = bq::log_utils::zigzag::decode(epoch_offset_zigzag_ms);
    uint32_t formate_template_idx;
    const size_t formate_template_idx_len = bq::log_utils::vlq::vlq_decode(formate_template_idx, read_handle.data() + cursor);
    if (formate_template_idx_len == bq::log_utils::vlq::invalid_decode_length) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, decode formate_template_idx failed");
        return appender_decode_result::failed_decode_error;
    }
    cursor += formate_template_idx_len;
    if (formate_template_idx >= log_templates_array_.size()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, invalid formate_template_idx: %d", formate_template_idx);
        return appender_decode_result::failed_decode_error;
    }
    if (cursor >= read_handle.len()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, log entry formate_template_idx vlq decode error");
        return appender_decode_result::failed_decode_error;
    }
    uint32_t thread_info_template_index;
    const size_t thread_info_index_len = bq::log_utils::vlq::vlq_decode(thread_info_template_index, read_handle.data() + cursor);
    if (thread_info_index_len == bq::log_utils::vlq::invalid_decode_length) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, decode thread_info_template_index failed");
        return appender_decode_result::failed_decode_error;
    }
    cursor += thread_info_index_len;
    auto thread_info_iter = thread_info_templates_map_.find(thread_info_template_index);
    if (thread_info_iter == thread_info_templates_map_.end()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, log entry thread_info_idx vlq decode error");
        return appender_decode_result::failed_decode_error;
    }
    if (cursor > read_handle.len()) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, log entry thread_info_idx vlq decode error");
        return appender_decode_result::failed_decode_error;
    }
    last_log_entry_epoch_ = static_cast<uint64_t>((static_cast<int64_t>(last_log_entry_epoch_) + epoch_offset));
    auto& formate_template = log_templates_array_[formate_template_idx];

    raw_data_.clear();
    raw_data_.fill_uninitialized(sizeof(bq::_log_entry_head_def));

    ptrdiff_t raw_cursor = 0;
    bq::_log_entry_head_def& head = *((bq::_log_entry_head_def*)&raw_data_[raw_cursor]);
    head.level = (decltype(head.level))formate_template.level;
    head.category_idx = formate_template.category_idx;
    head.timestamp_epoch = last_log_entry_epoch_;
    head.log_format_str_type = (decltype(head.log_format_str_type))log_arg_type_enum::string_utf8_type;
    raw_cursor += static_cast<ptrdiff_t>(sizeof(bq::_log_entry_head_def));

    auto fmt_str_size = sizeof(uint32_t) + formate_template.fmt_string.size();
    auto fmt_str_section_size = bq::align_4(fmt_str_size);
    size_t args_offset = sizeof(bq::_log_entry_head_def) + fmt_str_section_size;
    assert(args_offset <= UINT16_MAX && "log format string too long");
    head.log_args_offset = (uint16_t)args_offset;
    raw_data_.fill_uninitialized(fmt_str_section_size);

    *((uint32_t*)&raw_data_[raw_cursor]) = (uint32_t)formate_template.fmt_string.size();
    if (fmt_str_size > 0) {
        memcpy((uint8_t*)(raw_data_.begin() + raw_cursor + sizeof(uint32_t)), formate_template.fmt_string.c_str(), formate_template.fmt_string.size());
    }
    raw_cursor += static_cast<ptrdiff_t>(fmt_str_section_size);

    while (cursor < read_handle.len()) {
        bq::log_arg_type_enum type_info = (bq::log_arg_type_enum)read_handle.data()[cursor++];
        raw_data_.fill_uninitialized(4);
        raw_data_[raw_cursor] = (uint8_t)type_info;
        size_t vlq_decode_length_tmp = 0;
        switch (type_info) {
        case bq::log_arg_type_enum::unsupported_type:
            bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : non_primitivi_type is not supported yet, type:%d", (int32_t)type_info);
            return appender_decode_result::failed_decode_error;
            break;
        case bq::log_arg_type_enum::null_type:
            raw_cursor += 4;
            break;
        case bq::log_arg_type_enum::pointer_type:
            assert(sizeof(void*) >= 4);
            raw_data_.fill_uninitialized(8);
            memcpy(raw_data_.begin() + raw_cursor + 4, read_handle.data() + cursor, 8);
            cursor += 8;
            raw_cursor += 12;
            break;
        case bq::log_arg_type_enum::bool_type:
        case bq::log_arg_type_enum::char_type:
        case bq::log_arg_type_enum::int8_type:
        case bq::log_arg_type_enum::uint8_type:
            memcpy(raw_data_.begin() + raw_cursor + 2, read_handle.data() + cursor, sizeof(char));
            cursor += sizeof(char);
            raw_cursor += 4;
            break;
        case bq::log_arg_type_enum::char16_type:
        case bq::log_arg_type_enum::uint16_type:
            vlq_decode_length_tmp = bq::log_utils::vlq::vlq_decode(*(uint16_t*)(&raw_data_[raw_cursor + 2]), read_handle.data() + cursor);
            if (bq::log_utils::vlq::invalid_decode_length == vlq_decode_length_tmp) {
                bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param 16 bits integer decode error");
                return bq::appender_decode_result::failed_decode_error;
            }
            cursor += vlq_decode_length_tmp;
            raw_cursor += 4;
            break;
        case bq::log_arg_type_enum::int16_type:
            vlq_decode_length_tmp = bq::log_utils::vlq::vlq_decode(*(uint16_t*)(&raw_data_[raw_cursor + 2]), read_handle.data() + cursor);
            *(int16_t*)(&raw_data_[raw_cursor + 2]) = bq::log_utils::zigzag::decode(*(uint16_t*)(&raw_data_[raw_cursor + 2]));
            if (bq::log_utils::vlq::invalid_decode_length == vlq_decode_length_tmp) {
                bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param 16 bits integer decode error");
                return bq::appender_decode_result::failed_decode_error;
            }
            cursor += vlq_decode_length_tmp;
            raw_cursor += 4;
            break;
        case bq::log_arg_type_enum::char32_type:
        case bq::log_arg_type_enum::uint32_type:
            raw_data_.fill_uninitialized(sizeof(uint32_t));
            vlq_decode_length_tmp = bq::log_utils::vlq::vlq_decode(*(uint32_t*)(&raw_data_[raw_cursor + 4]), read_handle.data() + cursor);
            if (bq::log_utils::vlq::invalid_decode_length == vlq_decode_length_tmp) {
                bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param 32 bits integer decode error");
                return bq::appender_decode_result::failed_decode_error;
            }
            cursor += vlq_decode_length_tmp;
            raw_cursor += 4 + static_cast<ptrdiff_t>(sizeof(uint32_t));
            break;
        case bq::log_arg_type_enum::int32_type:
            raw_data_.fill_uninitialized(sizeof(uint32_t));
            vlq_decode_length_tmp = bq::log_utils::vlq::vlq_decode(*(uint32_t*)(&raw_data_[raw_cursor + 4]), read_handle.data() + cursor);
            *(int32_t*)(&raw_data_[raw_cursor + 4]) = bq::log_utils::zigzag::decode(*(uint32_t*)(&raw_data_[raw_cursor + 4]));
            if (bq::log_utils::vlq::invalid_decode_length == vlq_decode_length_tmp) {
                bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param 32 bits integer decode error");
                return bq::appender_decode_result::failed_decode_error;
            }
            cursor += vlq_decode_length_tmp;
            raw_cursor += 4 + static_cast<ptrdiff_t>(sizeof(uint32_t));
            break;
        case bq::log_arg_type_enum::uint64_type:
            raw_data_.fill_uninitialized(sizeof(uint64_t));
            vlq_decode_length_tmp = bq::log_utils::vlq::vlq_decode(*(uint64_t*)(&raw_data_[raw_cursor + 4]), read_handle.data() + cursor);
            if (bq::log_utils::vlq::invalid_decode_length == vlq_decode_length_tmp) {
                bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param 64 bits integer decode error");
                return bq::appender_decode_result::failed_decode_error;
            }
            cursor += vlq_decode_length_tmp;
            raw_cursor += 4 + static_cast<ptrdiff_t>(sizeof(uint64_t));
            break;
        case bq::log_arg_type_enum::int64_type:
            raw_data_.fill_uninitialized(sizeof(uint64_t));
            vlq_decode_length_tmp = bq::log_utils::vlq::vlq_decode(*(uint64_t*)(&raw_data_[raw_cursor + 4]), read_handle.data() + cursor);
            *(int64_t*)(&raw_data_[raw_cursor + 4]) = bq::log_utils::zigzag::decode(*(uint64_t*)(&raw_data_[raw_cursor + 4]));
            if (bq::log_utils::vlq::invalid_decode_length == vlq_decode_length_tmp) {
                bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param 64 bits integer decode error");
                return bq::appender_decode_result::failed_decode_error;
            }
            cursor += vlq_decode_length_tmp;
            raw_cursor += 4 + static_cast<ptrdiff_t>(sizeof(uint64_t));
            break;
        case bq::log_arg_type_enum::float_type:
            raw_data_.fill_uninitialized(sizeof(float));
            memcpy(raw_data_.begin() + raw_cursor + 4, read_handle.data() + cursor, sizeof(float));
            cursor += sizeof(float);
            raw_cursor += 4 + static_cast<ptrdiff_t>(sizeof(float));
            break;
        case bq::log_arg_type_enum::double_type:
            raw_data_.fill_uninitialized(sizeof(double));
            memcpy(raw_data_.begin() + raw_cursor + 4, read_handle.data() + cursor, sizeof(double));
            cursor += sizeof(double);
            raw_cursor += 4 + static_cast<ptrdiff_t>(sizeof(double));
            break;
        case bq::log_arg_type_enum::string_utf8_type: {
            uint32_t len = 0;
            vlq_decode_length_tmp = bq::log_utils::vlq::vlq_decode(len, read_handle.data() + cursor);
            if (bq::log_utils::vlq::invalid_decode_length == vlq_decode_length_tmp) {
                bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param utf8 string length decode error");
                return bq::appender_decode_result::failed_decode_error;
            }
            cursor += vlq_decode_length_tmp;
            if (cursor + len > read_handle.len()) {
                bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param utf8 string length overflow log entry data item length :%d", len);
                return bq::appender_decode_result::failed_decode_error;
            }
            raw_cursor += 4;
            raw_data_.fill_uninitialized(4);
            *((uint32_t*)(uint8_t*)(raw_data_.begin() + raw_cursor)) = len;
            raw_cursor += 4;

            uint32_t string_section_len = (uint32_t)bq::align_4((size_t)len);
            raw_data_.fill_uninitialized(string_section_len);
            bool has_place_holder = len && !read_handle.data()[cursor];
            const uint8_t* str_begin_pos = has_place_holder ? (const uint8_t*)(read_handle.data() + cursor + 1) : (const uint8_t*)(read_handle.data() + cursor);
            memcpy((uint8_t*)(raw_data_.begin() + raw_cursor), str_begin_pos, (size_t)len);
            cursor += has_place_holder ? (len + 1) : (len);
            raw_cursor += static_cast<ptrdiff_t>(string_section_len);
        } break;
        default:
            break;
        }
        if (cursor > read_handle.len()) {
            bq::util::log_device_console(bq::log_level::error, "decode compressed log file failed : param length overflow log entry data item length");
            return bq::appender_decode_result::failed_decode_error;
        }
        continue;
    }
    size_t ext_info_size = sizeof(ext_log_entry_info_head) + thread_info_iter->value().thread_name.size();
    size_t ext_info_offset = raw_data_.size();
    raw_data_.fill_uninitialized(ext_info_size);
    bq::log_entry_handle entry(raw_data_.begin(), (uint32_t)raw_data_.size());
    // head is invalid now, because raw_data may have expanded it's capacity.
    entry.get_log_head().ext_info_offset = (uint32_t)ext_info_offset;
    auto& ext_info = const_cast<ext_log_entry_info_head&>(entry.get_ext_head());
    BQ_PACK_ACCESS(ext_info.thread_id_) = thread_info_iter->value().thread_id;
    ext_info.thread_name_len_ = (uint8_t)thread_info_iter->value().thread_name.size();
    memcpy((uint8_t*)&ext_info + sizeof(ext_log_entry_info_head), thread_info_iter->value().thread_name.c_str(), ext_info.thread_name_len_);
    return do_decode_by_log_entry_handle(entry);
}
