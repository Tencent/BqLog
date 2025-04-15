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
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_wrapper_tools.h"
#include "bq_log/log/layout.h"

#include "bq_log/global/vars.h"
#include "bq_log/utils/log_utils.h"

namespace bq {
    layout::layout()
        : is_gmt_time_(false)
        , time_cache_ { 0 }
        , time_cache_len_(0)
        , last_time_epoch_cache_(0)
        , categories_name_array_ptr_(nullptr)
        , format_content_cursor(0)
    {
        thread_names_cache_.set_expand_rate(4);
    }

    layout::enum_layout_result layout::do_layout(const bq::log_entry_handle& log_entry, bool gmt_time, const bq::array<bq::string>* categories_name_array_ptr)
    {
        is_gmt_time_ = gmt_time;
        categories_name_array_ptr_ = categories_name_array_ptr;
        format_content_cursor = 0;
        expand_format_content_buff_size(1024);
        auto result = layout_prefix(log_entry);
        if (result != enum_layout_result::finished) {
            return result;
        }
        python_style_format_content(log_entry);
        return enum_layout_result::finished;
    }

    layout::enum_layout_result layout::layout_prefix(const bq::log_entry_handle& log_entry)
    {
        const auto& head = log_entry.get_log_head();
        auto result = insert_time(log_entry);
        if (result != enum_layout_result::finished) {
            bq::util::log_device_console(log_level::error, "layout_prefix error, insert_time", "");
            return result;
        }

        result = insert_thread_info(log_entry);
        if (result != enum_layout_result::finished) {
            bq::util::log_device_console(log_level::error, "layout_prefix error, insert_thread_info", "");
            return result;
        }

        auto level = log_entry.get_level();
        if (level < log_level::verbose || level > log_level::fatal) {
            bq::util::log_device_console(log_level::error, "layout_prefix error, log_level %d, maybe header file or struct mismatch in include", level);
            return enum_layout_result::parse_error;
        }
        const auto& level_str = log_global_vars::get().log_level_str_[(int32_t)log_entry.get_level()];
        insert_str_utf8(level_str, sizeof(level_str));
        insert_char('\t');

        if (head.category_idx >= categories_name_array_ptr_->size()) {
            bq::util::log_device_console(log_level::error, "layout_prefix error, category %d, maybe header file or struct mismatch in include", head.category_idx);
            return enum_layout_result::parse_error;
        }
        const bq::string* category_str_ptr = &(*categories_name_array_ptr_)[head.category_idx];
        if (!category_str_ptr->is_empty()) {
            insert_char('[');
            insert_str_utf8(category_str_ptr->c_str(), static_cast<uint32_t>(category_str_ptr->size()));
            insert_str_utf8("]\t", (uint32_t)2);
        }
        return enum_layout_result::finished;
    }

    layout::enum_layout_result layout::insert_time(const bq::log_entry_handle& log_entry)
    {
        expand_format_content_buff_size(format_content_cursor + MAX_TIME_STR_LEN + 3);
        uint64_t epoch = log_entry.get_log_head().timestamp_epoch;

        if (epoch != last_time_epoch_cache_) {
            struct tm time_st;
            if (is_gmt_time_) {
                bq::util::get_gmt_time_by_epoch(epoch, time_st);
            } else {
                bq::util::get_local_time_by_epoch(epoch, time_st);
            }
            time_cache_len_ = snprintf(time_cache_, sizeof(time_cache_),
                "%s %d-%02d-%02d %02d:%02d:%02d.", is_gmt_time_ ? log_global_vars::get().utc_time_zone_str_ : log_global_vars::get().time_zone_str_, time_st.tm_year + 1900, time_st.tm_mon + 1, time_st.tm_mday, time_st.tm_hour, time_st.tm_min, time_st.tm_sec);
            last_time_epoch_cache_ = epoch;
        }
        memcpy(&format_content[format_content_cursor], time_cache_, time_cache_len_);
        format_content_cursor += (uint32_t)time_cache_len_;

        int32_t millsec = static_cast<int32_t>(epoch % 1000);
        const char* ms_src = &log_global_vars::get().digit3_array[millsec * 3];
        char* ms_dest = &format_content[format_content_cursor];
        ms_dest[0] = ms_src[0];
        ms_dest[1] = ms_src[1];
        ms_dest[2] = ms_src[2];
        format_content_cursor += 3;
        return enum_layout_result::finished;
    }

    bq::layout::enum_layout_result layout::insert_thread_info(const bq::log_entry_handle& log_entry)
    {
        const auto& ext_info = log_entry.get_ext_head();
        auto iter = thread_names_cache_.find(BQ_PACK_ACCESS(ext_info.thread_id_));
        if (iter == thread_names_cache_.end()) {
            char tmp[256];
            uint32_t cursor = snprintf(tmp, sizeof(tmp), "[tid-%" PRIu64 " ", BQ_PACK_ACCESS(ext_info.thread_id_));
            memcpy(tmp + cursor, (uint8_t*)&ext_info + sizeof(ext_log_entry_info_head), ext_info.thread_name_len_);
            cursor += ext_info.thread_name_len_;
            tmp[cursor++] = ']';
            tmp[cursor++] = '\t';
            assert(cursor < 256);
            tmp[cursor] = '\0';
            bq::string thread_name = tmp;
            iter = thread_names_cache_.add(BQ_PACK_ACCESS(ext_info.thread_id_), bq::move(thread_name));
        }
        if (iter->value().size() > 0) {
            expand_format_content_buff_size(format_content_cursor + (uint32_t)iter->value().size());
            memcpy(&format_content[format_content_cursor], iter->value().c_str(), iter->value().size());
            format_content_cursor += (uint32_t)iter->value().size();
        }
        return enum_layout_result::finished;
    }

    // Start entering C++20 style formatting
    //[[fill]align][sign][#][0][width][.precision][type]
    // author: eggdai
    template <typename T>
    layout::format_info layout::c20_format(const T* style, int32_t len)
    {
        format_info fi;
        fi.used = true;
        if (style[0] != ':')
            return fi;

        int index = 0;
        bool precision = false;
        bool align = false;
        bool fill = false;
        // Only supports maximum 99 bit length alignment
        char width[3] = { '0', 0, 0 };
        while (true) {
            index++;
            if (index >= len || index > 10) {
                fi.offset = 0;
                fi.width = 0;
                break;
            }
            char c = (char)style[index];
            if (c == '{') {
                fi.offset = 0;
                fi.width = 0;
                break;
            } else if (c == '}') {
                fi.offset = index;
                break;
            }
            // Dynamic width not currently supported
            switch (c) {
            case '#':
                fi.prefix = c;
                break;
            case '>':
            case '<':
            case '^':
                if (!align) {
                    fi.align = c;
                    align = true;
                } else {
                    fi.fill = c;
                    if (fi.align == '^' || fi.align == '<')
                        fi.fill = ' ';
                }
                break;
            case '+':
            case '-':
                fi.sign = c;
                break;
            case 'b':
            case 'B':
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'x':
            case 'X':
            case 'o':
            case 'd':
                // only one type
                if (fi.type == ' ') {
                    fi.type = c;
                    // check use uppercase
                    if (fi.type == 'E') {
                        fi.type = 'e';
                        fi.upper = true;
                    } else if (fi.type == 'X') {
                        fi.type = 'x';
                        fi.upper = true;
                    } else if (fi.type == 'B') {
                        fi.type = 'b';
                        fi.upper = true;
                    } else
                        fi.upper = false;
                }
                // Fill space when use x-style and right aligned
                if ((c == 'x' || c == 'X') && fi.align != '>') {
                    fi.fill = ' ';
                }
                break;
            case '.':
                precision = true;
                fi.width = atoi(width);
                width[0] = '0';
                width[1] = 0;
                break;
            default:
                if (!fill && (c < '1' || c > '9')) {
                    fi.fill = c;
                    if (fi.align == '^' || fi.align == '<')
                        fi.fill = ' ';
                } else {
                    if (isdigit((int32_t)(uint8_t)c)) {
                        fill = true;
                        if (width[0] == '0')
                            width[0] = c;
                        else if (width[1] == 0)
                            width[1] = c;
                    }
                }
                break;
            }
        }
        if (precision) {
            fi.precision = atoi(width);
        } else {
            fi.width = atoi(width);
        }
        if (fi.type == ' ')
            fi.type = 'd';
        return fi;
    }

    //'format_content' may be more suitable as a parameter
    //@param format_content
    //@paramwirte_begin_pos
    void layout::fill_and_alignment(uint32_t wirte_begin_pos)
    {
        // fill and alignment
        uint32_t dis = format_content_cursor - wirte_begin_pos;
        if (dis < format_info_.width) {
            uint32_t fill_count = format_info_.width - dis;
            // alignment right
            if (format_info_.align == '>') {
                int32_t ignore = 0;
                uint32_t ignore_index = 0;
                // move
                for (uint32_t i = 1; i <= dis; i++) {
                    uint32_t opt_index = wirte_begin_pos + format_info_.width - i;
                    uint32_t move_index = wirte_begin_pos + (dis - i);

                    // keep the sign in front
                    if ((format_content[move_index] == '+' || format_content[move_index] == '-') && format_info_.fill != ' ') {
                        format_content[opt_index] = format_info_.fill;
                        ignore = 1;
                        continue;
                    }
                    if (format_info_.fill == '0' && format_info_.prefix == '#' && (format_info_.type == 'b' || format_info_.type == 'x')) {
                        if (format_content[move_index] == 'b' || format_content[move_index] == 'B' || format_content[move_index] == 'x' || format_content[move_index] == 'X') {
                            format_content[opt_index] = format_info_.fill;
                            ignore_index = move_index - 1;
                            ignore = 2;
                            continue;
                        }
                    }
                    if (ignore == 2 && ignore_index == move_index) {
                        format_content[opt_index] = format_info_.fill;
                        continue;
                    }
                    format_content[opt_index] = format_content[move_index];
                }
                // fill
                for (uint32_t i = ignore; i < fill_count; i++) {
                    uint32_t opt_index = wirte_begin_pos + i;
                    format_content[opt_index] = format_info_.fill;
                }
            }
            // alignment left
            else if (format_info_.align == '<') {
                // fill
                for (uint32_t i = 0; i < fill_count; i++) {
                    uint32_t opt_index = format_content_cursor + i;
                    format_content[opt_index] = format_info_.fill;
                }
            }
            // alignment middle
            else if (format_info_.align == '^') {
                uint32_t end = fill_count / 2;
                uint32_t front = fill_count - end;
                // move
                for (uint32_t i = 1; i <= dis; i++) {
                    uint32_t opt_index = wirte_begin_pos + format_info_.width - i - end;
                    uint32_t move_index = wirte_begin_pos + (dis - i);
                    format_content[opt_index] = format_content[move_index];
                }
                // fill front
                for (uint32_t i = 0; i < front; i++) {
                    uint32_t opt_index = wirte_begin_pos + i;
                    format_content[opt_index] = format_info_.fill;
                }
                // fill end
                for (uint32_t i = 0; i < end; i++) {
                    uint32_t opt_index = front + wirte_begin_pos + dis + i;
                    format_content[opt_index] = format_info_.fill;
                }
            }
            format_content_cursor += fill_count;
            memset((void*)&format_info_, 0, sizeof(format_info));
        }
    }

    void layout::fill_e_style(uint32_t eCount, uint32_t begin_cursor)
    {
        int32_t bitcount = 0;
        auto temp = eCount;
        do {
            temp /= 10;
            bitcount++;
        } while (temp != 0);
        expand_format_content_buff_size(format_content_cursor + bitcount + 3); // 3->e+0
        // 1.000000 -> 1.0000
        int32_t jump = format_content_cursor - (begin_cursor + format_info_.width - bitcount - 2); // 2->e+
        if (jump > 0) {
            format_content_cursor -= jump;
            if (format_content_cursor == begin_cursor)
                format_content_cursor++;
        }
        // 1.0000 -> 1.0000e+
        if (format_info_.upper)
            format_content[format_content_cursor++] = 'E';
        else
            format_content[format_content_cursor++] = 'e';
        format_content[format_content_cursor++] = '+';
        begin_cursor = format_content_cursor;
        temp = eCount;
        // 1.0000e+ -> 1.0000e+60
        do {
            int32_t digit = eCount % 10;
            format_content[format_content_cursor++] = '0' + (char)digit;
            eCount /= 10;
        } while (eCount != 0);
        if (temp <= 9)
            format_content[format_content_cursor++] = '0';
        // 1.0000e+60 -> 1.0000e+06
        reverse(begin_cursor, format_content_cursor - 1);
    }

    void layout::python_style_format_content(const bq::log_entry_handle& log_entry)
    {
        if (log_entry.get_log_head().log_format_str_type == (uint16_t)log_arg_type_enum::string_utf8_type) {
            python_style_format_content_utf8(log_entry);
        } else {
            python_style_format_content_utf16(log_entry);
        }
        format_content[format_content_cursor] = '\0';
        assert(format_content_cursor < format_content.size());
    }

    void layout::python_style_format_content_utf8(const bq::log_entry_handle& log_entry)
    {
        const uint8_t* args_data_ptr = log_entry.get_log_args_data();
        uint32_t args_data_len = log_entry.get_log_args_data_size();
        const char* format_data_ptr = log_entry.get_format_string_data();
        uint32_t format_data_len = *((uint32_t*)format_data_ptr);
        format_data_ptr += sizeof(uint32_t);

        if (args_data_len == 0) {
            expand_format_content_buff_size(format_content_cursor + format_data_len);
            memcpy((char*)&format_content[format_content_cursor], format_data_ptr, format_data_len);
            format_content_cursor += format_data_len;
            return;
        }

        uint32_t i = 0;
        bool is_arg = false;
        uint32_t last_left_brace_index = 0;
        uint32_t args_data_cursor = 0;

        // maybe waste, but safe
        uint32_t safe_buff_size = format_content_cursor + format_data_len;
        expand_format_content_buff_size(safe_buff_size);
        while (++i <= format_data_len) {
            uint32_t cursor = i - 1;
            char c = format_data_ptr[cursor];
            if (c == '{') {
                last_left_brace_index = format_content_cursor;
                is_arg = true;
                if (format_info_.used)
                    format_info_.reset();
            } else if (c == '}') {
                bool temp_arg = is_arg;
                is_arg = false;
                if (temp_arg && args_data_cursor < args_data_len) {
                    auto write_begin_pos = format_content_cursor - 1;
                    format_content_cursor = last_left_brace_index;
                    uint8_t type_info_i = *(args_data_ptr + args_data_cursor);
                    bq::log_arg_type_enum type_info = (bq::log_arg_type_enum)(type_info_i);
                    switch (type_info) {
                    case bq::log_arg_type_enum::unsupported_type:
                        bq::util::log_device_console(bq::log_level::warning, "non_primitivi_type is not supported yet");
                        break;
                    case bq::log_arg_type_enum::null_type:
                        assert(sizeof(void*) >= 4);
                        insert_str_utf8("null", (uint32_t)(sizeof("null") - 1));
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::pointer_type:
                        assert(sizeof(void*) >= 4);
                        {
                            const void* arg_data_ptr = *((const void**)(args_data_ptr + args_data_cursor + 4));
                            insert_pointer(arg_data_ptr);
                            args_data_cursor += 4 + sizeof(uint64_t); // use 64bit pointer for serialize
                        }
                        break;
                    case bq::log_arg_type_enum::bool_type:
                        insert_bool(*(const bool*)(args_data_ptr + args_data_cursor + 2));
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::char_type:
                        insert_char(*(const char*)(args_data_ptr + args_data_cursor + 2));
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::char16_type:
                        insert_char16(*(const char16_t*)(args_data_ptr + args_data_cursor + 2));
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::char32_type:
                        insert_char32(*(const char32_t*)(args_data_ptr + args_data_cursor + 4));
                        args_data_cursor += 8;
                        break;
                    case bq::log_arg_type_enum::int8_type:
                        insert_integral_signed(*(const int8_t*)(args_data_ptr + args_data_cursor + 2));
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::uint8_type:
                        insert_integral_unsigned(*(const uint8_t*)(args_data_ptr + args_data_cursor + 2));
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::int16_type:
                        insert_integral_signed(*(const int16_t*)(args_data_ptr + args_data_cursor + 2));
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::uint16_type:
                        insert_integral_unsigned(*(const uint16_t*)(args_data_ptr + args_data_cursor + 2));
                        args_data_cursor += 4;
                        break;
                    case bq::log_arg_type_enum::int32_type:
                        insert_integral_signed(*(const int32_t*)(args_data_ptr + args_data_cursor + 4));
                        args_data_cursor += 8;
                        break;
                    case bq::log_arg_type_enum::uint32_type:
                        insert_integral_unsigned(*(const uint32_t*)(args_data_ptr + args_data_cursor + 4));
                        args_data_cursor += 8;
                        break;
                    case bq::log_arg_type_enum::int64_type:
                        insert_integral_signed(*(const int64_t*)(args_data_ptr + args_data_cursor + 4));
                        args_data_cursor += 12;
                        break;
                    case bq::log_arg_type_enum::uint64_type:
                        insert_integral_unsigned(*(const uint64_t*)(args_data_ptr + args_data_cursor + 4));
                        args_data_cursor += 12;
                        break;
                    case bq::log_arg_type_enum::float_type:
                        insert_decimal(*(const float*)(args_data_ptr + args_data_cursor + 4));
                        args_data_cursor += (4 + sizeof(float));
                        break;
                    case bq::log_arg_type_enum::double_type:
                        insert_decimal(*(const double*)(args_data_ptr + args_data_cursor + 4));
                        args_data_cursor += (4 + sizeof(double));
                        break;
                    case bq::log_arg_type_enum::string_utf8_type: {
                        const uint32_t* len_ptr = (const uint32_t*)(args_data_ptr + args_data_cursor + 4);
                        const char* str = (const char*)args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t);
                        uint32_t str_len = *len_ptr;
                        insert_str_utf8(str, str_len);
                        args_data_cursor += 4 + sizeof(uint32_t) + (uint32_t)bq::align_4(str_len);
                    } break;
                    case bq::log_arg_type_enum::string_utf16_type: {
                        const uint32_t* len_ptr = (const uint32_t*)(args_data_ptr + args_data_cursor + 4);
                        const char* str = (const char*)args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t);
                        uint32_t str_len = *len_ptr;
                        insert_str_utf16(str, str_len);
                        args_data_cursor += 4 + sizeof(uint32_t) + (uint32_t)bq::align_4(str_len);

                    } break;
                    default:
                        break;
                    }

                    assert(args_data_cursor <= args_data_len);
                    fill_and_alignment(write_begin_pos);
                    uint32_t written_len = format_content_cursor - last_left_brace_index;
                    safe_buff_size += written_len;
                    expand_format_content_buff_size(safe_buff_size);

                    if (args_data_cursor == args_data_len) {
                        if (format_data_len > i) {
                            expand_format_content_buff_size(format_content_cursor + format_data_len - i);
                            memcpy((char*)&format_content[format_content_cursor], format_data_ptr + i, format_data_len - i);
                            format_content_cursor += (format_data_len - i);
                        }
                        return;
                    }
                    continue;
                }
            } else if (is_arg) {
                if (args_data_cursor < args_data_len) {
                    format_info_ = c20_format(&format_data_ptr[cursor], format_data_len - cursor);
                    if (format_info_.offset == 0) {
                        if (!isdigit((int32_t)(uint8_t)c) && !isspace((int32_t)(uint8_t)c)) {
                            is_arg = false;
                        }
                    } else {
                        i += (format_info_.offset - 1);
                        continue;
                    }
                }
            }
            format_content[format_content_cursor++] = c;
        }
    }

    void layout::python_style_format_content_utf16(const bq::log_entry_handle& log_entry)
    {
        const uint8_t* args_data_ptr = log_entry.get_log_args_data();
        uint32_t args_data_len = log_entry.get_log_args_data_size();
        const char16_t* format_data_ptr = (const char16_t*)log_entry.get_format_string_data();
        uint32_t format_data_len = *((uint32_t*)format_data_ptr);
        format_data_ptr += sizeof(uint32_t) / sizeof(char16_t);

        // (* 3 / 2 + 1) make sure enough space, maybe waste
        uint32_t safe_buff_size = format_content_cursor + (uint32_t)(((size_t)format_data_len * 3) >> 1);
        expand_format_content_buff_size(safe_buff_size);
        uint32_t wchar_len = format_data_len >> 1;

        uint32_t i = 0;
        bool is_arg = false;
        uint32_t last_left_brace_index = 0;
        uint32_t args_data_cursor = 0;

        uint32_t codepoint = 0;
        uint32_t surrogate = 0;

        while (++i <= wchar_len) {
            uint32_t cursor = i - 1;
            char16_t c = format_data_ptr[cursor];

            if (surrogate) {
                if (c >= 0xDC00 && c <= 0xDFFF) {
                    codepoint = 0x10000 + (c - 0xDC00) + ((surrogate - 0xD800) << 10);
                    surrogate = 0;
                } else {
                    surrogate = 0;
                    continue;
                }
            } else {
                if (c < 0x80) {
                    codepoint = c;
                } else if (c >= 0xD800 && c <= 0xDBFF) {
                    surrogate = c;
                } else if (c >= 0xDC00 && c <= 0xDFFF) {
                    continue;
                } else {
                    codepoint = c;
                }
            }

            if (surrogate != 0)
                continue;

            if (codepoint < 0x80) {
                if (args_data_cursor >= args_data_len) {
                } else if (c == ('{')) {
                    last_left_brace_index = format_content_cursor;
                    is_arg = true;
                    if (format_info_.used)
                        format_info_.reset();
                } else if (c == '}') {
                    bool temp_arg = is_arg;
                    is_arg = false;
                    if (temp_arg && args_data_cursor < args_data_len) {
                        auto write_begin_pos = format_content_cursor - 1;
                        format_content_cursor = last_left_brace_index;
                        uint8_t type_info_i = *(args_data_ptr + args_data_cursor);
                        bq::log_arg_type_enum type_info = (bq::log_arg_type_enum)(type_info_i);
                        switch (type_info) {
                        case bq::log_arg_type_enum::unsupported_type:
                            bq::util::log_device_console(bq::log_level::warning, "non_primitivi_type is not supported yet");
                            break;
                        case bq::log_arg_type_enum::null_type:
                            assert(sizeof(void*) >= 4);
                            insert_str_utf8("null", (uint32_t)(sizeof("null") - 1));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::pointer_type:
                            assert(sizeof(void*) >= 4);
                            {
                                const void* ptr_data = *((const void**)(args_data_ptr + args_data_cursor + 4));
                                insert_pointer(ptr_data);
                                args_data_cursor += 4 + sizeof(uint64_t); // use 64bit pointer for serialize
                            }
                            break;
                        case bq::log_arg_type_enum::bool_type:
                            insert_bool(*(const bool*)(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char_type:
                            insert_char(*(const char*)(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char16_type:
                            insert_char16(*(const char16_t*)(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char32_type:
                            insert_char32(*(const char32_t*)(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::int8_type:
                            insert_integral_signed(*(const int8_t*)(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint8_type:
                            insert_integral_unsigned(*(const uint8_t*)(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int16_type:
                            insert_integral_signed(*(const int16_t*)(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint16_type:
                            insert_integral_unsigned(*(const uint16_t*)(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int32_type:
                            insert_integral_signed(*(const int32_t*)(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += (4 + sizeof(int32_t));
                            break;
                        case bq::log_arg_type_enum::uint32_type:
                            insert_integral_unsigned(*(const uint32_t*)(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += (4 + sizeof(uint32_t));
                            break;
                        case bq::log_arg_type_enum::int64_type:
                            insert_integral_signed(*(const int64_t*)(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += (4 + sizeof(int64_t));
                            break;
                        case bq::log_arg_type_enum::uint64_type:
                            insert_integral_unsigned(*(const uint64_t*)(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += (4 + sizeof(uint64_t));
                            break;
                        case bq::log_arg_type_enum::float_type:
                            insert_decimal(*(const float*)(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += (4 + sizeof(float));
                            break;
                        case bq::log_arg_type_enum::double_type:
                            insert_decimal(*(const double*)(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += (4 + sizeof(double));
                            break;
                        case bq::log_arg_type_enum::string_utf8_type: {
                            const uint32_t* len_ptr = (const uint32_t*)(args_data_ptr + args_data_cursor + 4);
                            const char* str = (const char*)args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t);
                            uint32_t str_len = *len_ptr;
                            insert_str_utf8(str, str_len);
                            args_data_cursor += 4 + sizeof(uint32_t) + (uint32_t)bq::align_4(str_len);
                        } break;
                        case bq::log_arg_type_enum::string_utf16_type: {
                            const uint32_t* len_ptr = (const uint32_t*)(args_data_ptr + args_data_cursor + 4);
                            const char* str = (const char*)args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t);
                            uint32_t str_len = *len_ptr;
                            insert_str_utf16(str, str_len);
                            args_data_cursor += 4 + sizeof(uint32_t) + (uint32_t)bq::align_4(str_len);
                        } break;
                        default:
                            break;
                        }

                        assert(args_data_cursor <= args_data_len);
                        fill_and_alignment(write_begin_pos);
                        uint32_t written_len = format_content_cursor - last_left_brace_index;
                        safe_buff_size += written_len;
                        expand_format_content_buff_size(safe_buff_size);
                        continue;
                    }
                } else if (is_arg) {
                    if (args_data_cursor < args_data_len) {
                        format_info_ = c20_format(&format_data_ptr[cursor], format_data_len - cursor);
                        if (format_info_.offset == 0) {
                            if (!isdigit((int32_t)(uint8_t)c) && !isspace((int32_t)(uint8_t)c)) {
                                is_arg = false;
                            }
                        } else {
                            i += (format_info_.offset - 1);
                            continue;
                        }
                    }
                }
                format_content[format_content_cursor++] = (char)codepoint;
            } else if (codepoint < 0x0800) {
                format_content[format_content_cursor++] = (char)(0xC0 | (codepoint >> 6));
                format_content[format_content_cursor++] = (char)(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                format_content[format_content_cursor++] = (char)(0xE0 | (codepoint >> 12));
                format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                format_content[format_content_cursor++] = (char)(0x80 | (codepoint & 0x3F));
            } else {
                format_content[format_content_cursor++] = (char)(0xF0 | (codepoint >> 18));
                format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                format_content[format_content_cursor++] = (char)(0x80 | (codepoint & 0x3F));
            }
        }
    }

    void layout::clear_format_content()
    {
        format_content_cursor = 0;
    }

    void layout::expand_format_content_buff_size(uint32_t new_size)
    {
        if (format_info_.offset != 0) {
            new_size += (format_info_.width + format_info_.precision + 2); // #will add 2 byte
        }
        if (new_size <= format_content.size()) {
            return;
        }
        format_content.fill_uninitialized(bq::roundup_pow_of_two(new_size) - format_content.size());
    }

    uint32_t layout::insert_str_utf8(const char* str, const uint32_t len)
    {
        expand_format_content_buff_size(format_content_cursor + len);
        memcpy(&format_content[format_content_cursor], str, len);
        format_content_cursor += len;
        assert(format_content_cursor <= format_content.size());
        return len;
    }

    uint32_t layout::insert_str_utf16(const char* str, const uint32_t len)
    {
        // (* 3 / 2 + 1) make sure enough space, maybe waste
        expand_format_content_buff_size(format_content_cursor + (len * 3 / 2 + 1));
        uint32_t codepoint = 0;
        uint32_t surrogate = 0;

        uint32_t wchar_len = len >> 1;

        const char16_t* s = (const char16_t*)(str);
        const char16_t* p = s;

        char16_t c;
        while ((uint32_t)(p - s) < wchar_len && (c = *p++) != 0) {
            if (surrogate) {
                if (c >= 0xDC00 && c <= 0xDFFF) {
                    codepoint = 0x10000 + (c - 0xDC00) + ((surrogate - 0xD800) << 10);
                    surrogate = 0;
                } else {
                    surrogate = 0;
                    continue;
                }
            } else {
                if (c < 0x80) {
                    format_content[format_content_cursor++] = (char)c;

                    while ((uint32_t)(p - s) < wchar_len && *p && *p < 0x80) {
                        format_content[format_content_cursor++] = (char)*p++;
                    }

                    continue;
                } else if (c >= 0xD800 && c <= 0xDBFF) {
                    surrogate = c;
                } else if (c >= 0xDC00 && c <= 0xDFFF) {
                    continue;
                } else {
                    codepoint = c;
                }
            }

            if (surrogate != 0)
                continue;

            if (codepoint < 0x80) {
                format_content[format_content_cursor++] = (char)codepoint;
            } else if (codepoint < 0x0800) {
                format_content[format_content_cursor++] = (char)(0xC0 | (codepoint >> 6));
                format_content[format_content_cursor++] = (char)(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                format_content[format_content_cursor++] = (char)(0xE0 | (codepoint >> 12));
                format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                format_content[format_content_cursor++] = (char)(0x80 | (codepoint & 0x3F));
            } else {
                format_content[format_content_cursor++] = (char)(0xF0 | (codepoint >> 18));
                format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                format_content[format_content_cursor++] = (char)(0x80 | (codepoint & 0x3F));
            }
        }
        assert(format_content_cursor <= format_content.size());
        return len;
    }

    void layout::insert_pointer(const void* ptr)
    {
        if (ptr) {
            uint32_t len = (uint32_t)sizeof("0x") - 1;
            expand_format_content_buff_size(format_content_cursor + len);
            memcpy(&format_content[format_content_cursor], "0x", len);
            format_content_cursor += len;
            insert_integral_unsigned((uint64_t)ptr, 16);
        } else {
            uint32_t len = (uint32_t)sizeof("null") - 1;
            expand_format_content_buff_size(format_content_cursor + len);
            memcpy(&format_content[format_content_cursor], "null", len);
            format_content_cursor += len;
        }
    }

    void layout::insert_bool(bool value)
    {
        if (value) {
            uint32_t len = (uint32_t)sizeof("TRUE") - 1;
            expand_format_content_buff_size(format_content_cursor + len);
            memcpy(&format_content[format_content_cursor], "TRUE", len);
            format_content_cursor += len;
        } else {
            uint32_t len = (uint32_t)sizeof("FALSE") - 1;
            expand_format_content_buff_size(format_content_cursor + len);
            memcpy(&format_content[format_content_cursor], "FALSE", len);
            format_content_cursor += len;
        }
    }

    void layout::insert_char(char value)
    {
        expand_format_content_buff_size(format_content_cursor + sizeof(char));
        format_content[format_content_cursor] = value;
        ++format_content_cursor;
    }

    void layout::insert_char16(char16_t value)
    {
        insert_str_utf16((const char*)(&value), sizeof(decltype(value)));
    }

    void layout::insert_char32(char32_t value)
    {
        if (value <= 0xFFFF) {
            insert_str_utf16((const char*)(&value), sizeof(char16_t));
        } else {
            value -= 0x10000;
            char16_t tmp[2];
            tmp[0] = static_cast<char16_t>((value >> 10) + 0xD800);
            tmp[1] = static_cast<char16_t>((value & 0x3FF) + 0xDC00);
            insert_str_utf16((const char*)tmp, sizeof(char16_t) * 2);
        }
    }

    uint32_t layout::insert_integral_unsigned(uint64_t value, uint32_t base /* = 10 */)
    {
        uint32_t width = format_content_cursor;
        assert(base <= 32 && "base is a number belongs to [2, 32]");
        if (format_info_.type == 'b') {
            base = 2;
        } else if (format_info_.type == 'x') {
            base = 16;
        } else if (format_info_.type == 'o') {
            base = 8;
        }
        // uint64_t max = 18,446,744,073,709,551,615
        if (base >= 16) {
            expand_format_content_buff_size(format_content_cursor + 16);
        } else if (base >= 10) {
            expand_format_content_buff_size(format_content_cursor + 20);
        } else {
            expand_format_content_buff_size(format_content_cursor + 64);
        }
        if (value > 0) {
            if (format_info_.sign == '+') {
                format_content[format_content_cursor++] = '+';
            }
        }

        if (format_info_.prefix == '#') {
            if (format_info_.type == 'b') {
                format_content[format_content_cursor++] = '0';
                if (format_info_.upper)
                    format_content[format_content_cursor++] = 'B';
                else
                    format_content[format_content_cursor++] = 'b';
            } else if (format_info_.type == 'x') {
                format_content[format_content_cursor++] = '0';
                if (format_info_.upper)
                    format_content[format_content_cursor++] = 'X';
                else
                    format_content[format_content_cursor++] = 'x';
            }
        }

        auto begin_cursor = format_content_cursor;
        int32_t eCount = 0;
        do {
            int32_t digit = value % base;
            if (digit < 0xA) {
                format_content[format_content_cursor] = '0' + (char)digit;
            } else {
                if (format_info_.upper)
                    format_content[format_content_cursor] = 'A' + (char)digit - (char)0xA;
                else
                    format_content[format_content_cursor] = 'a' + (char)digit - (char)0xA;
            }
            value /= base;
            ++format_content_cursor;
            if (value > base)
                eCount++;
        } while (value != 0);

        if (format_info_.type == 'e') {
            // 0000001 -> 000000.1
            format_content[format_content_cursor] = format_content[format_content_cursor - 1];
            format_content[format_content_cursor - 1] = '.';
            ++format_content_cursor;

            // 000000.1 -> 1.000000
            reverse(begin_cursor, format_content_cursor - 1);

            fill_e_style(eCount, begin_cursor);
        } else
            reverse(begin_cursor, format_content_cursor - 1);
        return format_content_cursor - width;
    }

    uint32_t layout::insert_integral_signed(int64_t value, uint32_t base /* = 10 */)
    {
        uint32_t width = format_content_cursor;
        assert(base <= 32 && "base is a number belongs to [2, 32]");
        // Hex check
        if (format_info_.type == 'b') {
            base = 2;
        } else if (format_info_.type == 'x') {
            base = 16;
        } else if (format_info_.type == 'o') {
            base = 8;
        }

        // Length expansion
        // uint64_t max = 18,446,744,073,709,551,615
        if (base >= 16) {
            expand_format_content_buff_size(format_content_cursor + 16);
        } else if (base >= 10) {
            expand_format_content_buff_size(format_content_cursor + 20);
        } else {
            expand_format_content_buff_size(format_content_cursor + 64);
        }
        //+- fill
        if (value < 0) {
            format_content[format_content_cursor++] = '-';
        } else {
            if (format_info_.sign == '+') {
                format_content[format_content_cursor++] = '+';
            }
        }

        // Default character padding
        if (format_info_.prefix == '#') {
            if (format_info_.type == 'b') {
                format_content[format_content_cursor++] = '0';
                if (format_info_.upper)
                    format_content[format_content_cursor++] = 'B';
                else
                    format_content[format_content_cursor++] = 'b';
            } else if (format_info_.type == 'x') {
                format_content[format_content_cursor++] = '0';
                if (format_info_.upper)
                    format_content[format_content_cursor++] = 'X';
                else
                    format_content[format_content_cursor++] = 'x';
            }
        }

        auto begin_cursor = format_content_cursor;
        // Scientific Counting Check
        int32_t eCount = 0;
        do {
            char digit = static_cast<char>(abs((int32_t)(value % base)));
            if (digit < 0xA) {
                format_content[format_content_cursor] = '0' + digit;
            } else {
                if (format_info_.upper)
                    format_content[format_content_cursor] = 'A' + digit - 0xA;
                else
                    format_content[format_content_cursor] = 'a' + digit - 0xA;
            }
            value /= base;
            if (value > base)
                eCount++; // Counting
            ++format_content_cursor;
        } while (value != 0);
        if (format_info_.type == 'e') {
            // 0000001 -> 000000.1
            format_content[format_content_cursor] = format_content[format_content_cursor - 1];
            format_content[format_content_cursor - 1] = '.';
            ++format_content_cursor;

            // 000000.1 -> 1.000000
            reverse(begin_cursor, format_content_cursor - 1);

            fill_e_style(eCount, begin_cursor);
        } else
            reverse(begin_cursor, format_content_cursor - 1);
        return format_content_cursor - width;
    }

    void layout::insert_decimal(float value)
    {
        if (format_info_.type == 'e')
            format_info_.type = 'Z';
        int int_width = 0;
        auto precision = (int32_t)((format_info_.precision != 0xFFFFFFFF) ? format_info_.precision : 7);
        auto begin_cursor = format_content_cursor;
        if (value >= 0) {
            uint64_t i_part = static_cast<uint64_t>(value);
            int_width = insert_integral_unsigned(i_part, 10);
            value -= static_cast<float>(i_part);
        } else {
            int64_t i_part = static_cast<int64_t>(value);
            int_width = insert_integral_signed(i_part, 10);
            value -= static_cast<float>(i_part);
        }
        if (format_info_.width > 0 && format_info_.width < (uint32_t)precision + 1 + int_width) {
            precision = (format_info_.width - int_width - 1);
        }

        value = fabsf(value);
        expand_format_content_buff_size(format_content_cursor + precision + 5); // more 5 maybe use in e style
        uint32_t point_index = 0;
        if (precision > 0 || (format_info_.type == 'Z' && int_width > 1)) {
            point_index = format_content_cursor;
            format_content[format_content_cursor++] = '.';
        }
        while (precision > 0) {
            value *= 10;
            int32_t digit = (int32_t)value;
            value -= (float)(digit);
            --precision;
            format_content[format_content_cursor++] = '0' + (char)digit;
        }

        if (format_info_.type == 'Z') {
            format_info_.type = 'e';
            uint32_t moves = int_width - 1;
            if (point_index != 0) {
                while (moves > 0) {
                    auto temp = format_content[point_index];
                    format_content[point_index] = format_content[point_index - 1];
                    format_content[point_index - 1] = temp;
                    --point_index;
                    --moves;
                }
            }

            moves = int_width - 1;
            if (format_info_.precision > 0 && format_info_.precision < format_content_cursor - begin_cursor)
                format_content_cursor = point_index + format_info_.precision + 1;
            fill_e_style(moves, begin_cursor);
        }
    }

    void layout::insert_decimal(double value)
    {
        if (format_info_.type == 'e')
            format_info_.type = 'Z';
        int int_width = 0;
        auto precision = (int32_t)((format_info_.precision != 0xFFFFFFFF) ? format_info_.precision : 15);
        auto begin_cursor = format_content_cursor;
        if (value >= 0) {
            uint64_t i_part = static_cast<uint64_t>(value);
            int_width = insert_integral_unsigned(i_part, 10);
            value -= static_cast<double>(i_part);
        } else {
            int64_t i_part = static_cast<int64_t>(value);
            int_width = insert_integral_signed(i_part, 10);
            value -= static_cast<double>(i_part);
        }
        if (format_info_.width > 0 && format_info_.width < (uint32_t)precision + 1 + int_width) {
            precision = (format_info_.width - int_width - 1);
            if (precision < 0)
                precision = 0;
        }

        value = fabs(value);
        expand_format_content_buff_size(format_content_cursor + precision + 5); // more 5 maybe use for e-style
        uint32_t point_index = 0;
        if (precision > 0 || (format_info_.type == 'Z' && int_width > 1)) {
            point_index = format_content_cursor;
            format_content[format_content_cursor++] = '.';
        }
        auto temp_precision = precision;
        while (temp_precision > 0) {
            value *= 10;
            int32_t digit = (int32_t)value;
            value -= (double)(digit);
            --temp_precision;
            format_content[format_content_cursor++] = '0' + (char)digit;
        }
        if (format_info_.type == 'Z') {
            format_info_.type = 'e';
            uint32_t moves = int_width - 1;
            if (point_index != 0) {
                while (moves > 0) {
                    auto temp = format_content[point_index];
                    format_content[point_index] = format_content[point_index - 1];
                    format_content[point_index - 1] = temp;
                    --point_index;
                    --moves;
                }
            }
            moves = int_width - 1;
            if (format_info_.precision > 0 && format_info_.precision < format_content_cursor - begin_cursor)
                format_content_cursor = point_index + format_info_.precision + 1;
            fill_e_style(moves, begin_cursor);
        }
    }

    void layout::reverse(uint32_t begin_cursor, uint32_t end_cursor)
    {
        while (begin_cursor < end_cursor) {
            char tmp = format_content[begin_cursor];
            format_content[begin_cursor] = format_content[end_cursor];
            format_content[end_cursor] = tmp;
            ++begin_cursor;
            --end_cursor;
        }
    }
}
