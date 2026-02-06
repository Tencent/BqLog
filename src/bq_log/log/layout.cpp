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
#include "bq_log/log/layout.h"
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_wrapper_tools.h"

#include "bq_log/global/log_vars.h"
#include "bq_log/utils/log_utils.h"

namespace bq {

    // =================================================================================================
    // Internal SIMD Helpers for Layout
    // =================================================================================================

    // -------------------------------------------------------------------------------------------------
    // find_brace_and_copy (UTF-8 / ASCII)
    // Scans the source buffer for braces '{' or '}' and copies content to the destination buffer.
    // Stops upon encountering a brace or reaching the specified length.
    // Returns the number of bytes processed.
    // The 'found_brace' output parameter is set to true if the scan stopped due to a brace.
    // -------------------------------------------------------------------------------------------------

    bq_forceinline uint32_t _impl_find_brace_and_copy_sw(const char* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace)
    {
        const char* start = src;
        const char* end = src + len;
        while (src < end) {
            char c = *src;
            if (c == '{' || c == '}') {
                found_brace = true;
                return static_cast<uint32_t>(src - start);
            }
            *dst++ = c;
            src++;
        }
        found_brace = false;
        return len;
    }

#if defined(BQ_X86)
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_find_brace_and_copy_avx2(const char* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace)
    {
        const char* start = src;
        const char* end = src + len;

        const __m256i brace_l = _mm256_set1_epi8('{'); // 0x7B
        const __m256i brace_r = _mm256_set1_epi8('}'); // 0x7D

        while (src + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
            __m256i cmp_l = _mm256_cmpeq_epi8(chunk, brace_l);
            __m256i cmp_r = _mm256_cmpeq_epi8(chunk, brace_r);
            __m256i mask_v = _mm256_or_si256(cmp_l, cmp_r);

            int32_t mask = _mm256_movemask_epi8(mask_v);
            if (mask != 0) {
                // Found brace
                uint32_t idx = 0;
#if defined(BQ_MSVC)
                unsigned long index;
                _BitScanForward(&index, static_cast<unsigned long>(mask));
                idx = static_cast<uint32_t>(index);
#else
                idx = static_cast<uint32_t>(__builtin_ctz(static_cast<uint32_t>(mask)));
#endif
                // Copy up to idx
                memcpy(dst, src, idx);
                found_brace = true;
                return static_cast<uint32_t>(src - start) + idx;
            }

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), chunk);
            src += 32;
            dst += 32;
        }

        return static_cast<uint32_t>(src - start) + _impl_find_brace_and_copy_sw(src, static_cast<uint32_t>(end - src), dst, found_brace);
    }

    BQ_SIMD_HW_INLINE BQ_HW_SIMD_SSE_TARGET uint32_t _impl_find_brace_and_copy_sse(const char* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace)
    {
        const char* start = src;
        const char* end = src + len;

        const __m128i brace_l = _mm_set1_epi8('{');
        const __m128i brace_r = _mm_set1_epi8('}');

        while (src + 16 <= end) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
            __m128i cmp_l = _mm_cmpeq_epi8(chunk, brace_l);
            __m128i cmp_r = _mm_cmpeq_epi8(chunk, brace_r);
            __m128i mask_v = _mm_or_si128(cmp_l, cmp_r);

            int32_t mask = _mm_movemask_epi8(mask_v);
            if (mask != 0) {
                uint32_t idx = 0;
#if defined(BQ_MSVC)
                unsigned long index;
                _BitScanForward(&index, static_cast<unsigned long>(mask));
                idx = static_cast<uint32_t>(index);
#else
                idx = static_cast<uint32_t>(__builtin_ctz(static_cast<uint32_t>(mask)));
#endif
                memcpy(dst, src, idx);
                found_brace = true;
                return static_cast<uint32_t>(src - start) + idx;
            }

            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), chunk);
            src += 16;
            dst += 16;
        }
        return static_cast<uint32_t>(src - start) + _impl_find_brace_and_copy_sw(src, static_cast<uint32_t>(end - src), dst, found_brace);
    }
#elif defined(BQ_ARM_NEON)
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_find_brace_and_copy_neon(const char* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace)
    {
        const char* start = src;
        const char* end = src + len;

        const uint8x16_t brace_l = vdupq_n_u8('{');
        const uint8x16_t brace_r = vdupq_n_u8('}');

        while (src + 16 <= end) {
            uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(src));
            uint8x16_t cmp_l = vceqq_u8(chunk, brace_l);
            uint8x16_t cmp_r = vceqq_u8(chunk, brace_r);
            uint8x16_t mask_v = vorrq_u8(cmp_l, cmp_r);

            if (bq_vmaxvq_u8(mask_v) != 0) {
                break;
            }

            vst1q_u8(reinterpret_cast<uint8_t*>(dst), chunk);
            src += 16;
            dst += 16;
        }
        return static_cast<uint32_t>(src - start) + _impl_find_brace_and_copy_sw(src, static_cast<uint32_t>(end - src), dst, found_brace);
    }
#endif

    bq_forceinline uint32_t find_brace_and_copy(const char* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace)
    {
#if defined(BQ_X86)
        if (_bq_avx2_supported_) {
            return _impl_find_brace_and_copy_avx2(src, len, dst, found_brace);
        } else {
            return _impl_find_brace_and_copy_sse(src, len, dst, found_brace);
        }
#elif defined(BQ_ARM_NEON)
        return _impl_find_brace_and_copy_neon(src, len, dst, found_brace);
#else
        return _impl_find_brace_and_copy_sw(src, len, dst, found_brace);
#endif
    }

    // -------------------------------------------------------------------------------------------------
    // find_brace_and_convert_utf16_to_utf8
    // Converts from UTF-16 to UTF-8 until '{', '}', end, OR non-ASCII char.
    // NOTE: This implementation is "Optimistic ASCII". It stops at ANY non-ASCII char or brace.
    // The caller is expected to loop and handle the non-ASCII fallback or brace handling.
    // -------------------------------------------------------------------------------------------------

    bq_forceinline uint32_t _impl_find_brace_and_convert_u16_sw(const char16_t* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace, bool& non_ascii)
    {
        const char16_t* start = src;
        const char16_t* end = src + len;

        while (src < end) {
            char16_t c = *src;
            if (c >= 0x80) {
                non_ascii = true;
                found_brace = false;
                return static_cast<uint32_t>(src - start);
            }
            if (c == u'{' || c == u'}') {
                found_brace = true;
                non_ascii = false;
                return static_cast<uint32_t>(src - start);
            }
            *dst++ = static_cast<char>(c);
            src++;
        }
        found_brace = false;
        non_ascii = false;
        return len;
    }

#if defined(BQ_X86)
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_find_brace_and_convert_u16_avx2(const char16_t* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace, bool& non_ascii)
    {
        const char16_t* start = src;
        const char16_t* end = src + len;

        const __m256i brace_l = _mm256_set1_epi16(u'{');
        const __m256i brace_r = _mm256_set1_epi16(u'}');
        const __m256i ascii_mask = _mm256_set1_epi16(static_cast<int16_t>(0xFF80u)); // Check for high byte or bit 7

        // Process 16 chars (32 bytes) at a time
        while (src + 16 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));

            // Check non-ASCII
            if (!_mm256_testz_si256(chunk, ascii_mask)) {
                // Found non-ASCII (or potentially garbage high bytes if bad input, but we assume valid U16 or stop)
                non_ascii = true; // Potentially, but let SW verify exact position
                break;
            }

            // Check braces
            __m256i cmp_l = _mm256_cmpeq_epi16(chunk, brace_l);
            __m256i cmp_r = _mm256_cmpeq_epi16(chunk, brace_r);
            if (!_mm256_testz_si256(_mm256_or_si256(cmp_l, cmp_r), _mm256_set1_epi16(-1))) {
                // Found brace
                break;
            }

            // Safe to pack and store
            __m256i compressed = _mm256_packus_epi16(chunk, chunk);
            __m256i permuted = _mm256_permute4x64_epi64(compressed, 0xD8);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), _mm256_castsi256_si128(permuted));

            src += 16;
            dst += 16;
        }

        return static_cast<uint32_t>(src - start) + _impl_find_brace_and_convert_u16_sw(src, static_cast<uint32_t>(end - src), dst, found_brace, non_ascii);
    }

    BQ_SIMD_HW_INLINE BQ_HW_SIMD_SSE_TARGET uint32_t _impl_find_brace_and_convert_u16_sse(const char16_t* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace, bool& non_ascii)
    {
        const char16_t* start = src;
        const char16_t* end = src + len;

        const __m128i brace_l = _mm_set1_epi16(u'{');
        const __m128i brace_r = _mm_set1_epi16(u'}');
        const __m128i ascii_mask = _mm_set1_epi16(static_cast<int16_t>(0xFF80u));

        while (src + 8 <= end) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));

            if (!_mm_testz_si128(chunk, ascii_mask)) {
                non_ascii = true;
                break;
            }

            __m128i cmp_l = _mm_cmpeq_epi16(chunk, brace_l);
            __m128i cmp_r = _mm_cmpeq_epi16(chunk, brace_r);
            if (!_mm_testz_si128(_mm_or_si128(cmp_l, cmp_r), _mm_set1_epi16(-1))) {
                break;
            }

            __m128i compressed = _mm_packus_epi16(chunk, chunk);
            _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), compressed);

            src += 8;
            dst += 8;
        }
        return static_cast<uint32_t>(src - start) + _impl_find_brace_and_convert_u16_sw(src, static_cast<uint32_t>(end - src), dst, found_brace, non_ascii);
    }

#elif defined(BQ_ARM_NEON)
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_find_brace_and_convert_u16_neon(const char16_t* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace, bool& non_ascii)
    {
        const char16_t* start = src;
        const char16_t* end = src + len;

        const uint16x8_t brace_l = vdupq_n_u16(u'{');
        const uint16x8_t brace_r = vdupq_n_u16(u'}');

        while (src + 8 <= end) {
            uint16x8_t chunk = vld1q_u16(reinterpret_cast<const uint16_t*>(src));

            // Check non-ASCII (any >= 0x80)
            if (bq_vmaxvq_u16(chunk) >= 0x80) {
                non_ascii = true;
                break;
            }

            uint16x8_t cmp_l = vceqq_u16(chunk, brace_l);
            uint16x8_t cmp_r = vceqq_u16(chunk, brace_r);
            if (bq_vmaxvq_u16(vorrq_u16(cmp_l, cmp_r)) != 0) {
                break;
            }

            vst1_u8(reinterpret_cast<uint8_t*>(dst), vmovn_u16(chunk));
            src += 8;
            dst += 8;
        }
        return static_cast<uint32_t>(src - start) + _impl_find_brace_and_convert_u16_sw(src, static_cast<uint32_t>(end - src), dst, found_brace, non_ascii);
    }
#endif

    bq_forceinline uint32_t find_brace_and_convert_u16(const char16_t* BQ_RESTRICT src, uint32_t len, char* BQ_RESTRICT dst, bool& found_brace, bool& non_ascii)
    {
#if defined(BQ_X86)
        if (_bq_avx2_supported_) {
            return _impl_find_brace_and_convert_u16_avx2(src, len, dst, found_brace, non_ascii);
        } else {
            return _impl_find_brace_and_convert_u16_sse(src, len, dst, found_brace, non_ascii);
        }
#elif defined(BQ_ARM_NEON)
        return _impl_find_brace_and_convert_u16_neon(src, len, dst, found_brace, non_ascii);
#else
        return _impl_find_brace_and_convert_u16_sw(src, len, dst, found_brace, non_ascii);
#endif
    }

    layout::layout()
        : time_zone_ptr_(nullptr)
        , categories_name_array_ptr_(nullptr)
        , format_content_cursor(0)
    {
        thread_names_cache_.set_expand_rate(4);
    }

    layout::enum_layout_result layout::do_layout(const bq::log_entry_handle& log_entry, time_zone& input_time_zone, const bq::array<bq::string>* categories_name_array_ptr)
    {
        time_zone_ptr_ = &input_time_zone;
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
            bq::util::log_device_console(log_level::error, "layout_prefix error, insert_time, result:%" PRId32, static_cast<int32_t>(result));
            return result;
        }

        result = insert_thread_info(log_entry);
        if (result != enum_layout_result::finished) {
            bq::util::log_device_console(log_level::error, "layout_prefix error, insert_thread_info, result:%" PRId32, static_cast<int32_t>(result));
            return result;
        }

        auto level = log_entry.get_level();
        if (level < log_level::verbose || level > log_level::fatal) {
            bq::util::log_device_console(log_level::error, "layout_prefix error, log_level %" PRId32 ", maybe header file or struct mismatch in include", static_cast<int32_t>(level));
            return enum_layout_result::parse_error;
        }
        const auto& level_str = log_global_vars::get().log_level_str_[static_cast<int32_t>(log_entry.get_level())];
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
            insert_str_utf8("]\t", static_cast<uint32_t>(2));
        }
        return enum_layout_result::finished;
    }

    layout::enum_layout_result layout::insert_time(const bq::log_entry_handle& log_entry)
    {
        expand_format_content_buff_size(format_content_cursor + time_zone::MAX_TIME_STR_LEN + 3);
        uint64_t epoch_ms = log_entry.get_log_head().timestamp_epoch;
        time_zone_ptr_->refresh_time_string_cache(epoch_ms);
        memcpy(&format_content[format_content_cursor], time_zone_ptr_->get_time_string_cache(), time_zone_ptr_->get_time_string_cache_len());
        format_content_cursor += static_cast<uint32_t>(time_zone_ptr_->get_time_string_cache_len());

        auto m_sec = static_cast<int32_t>(epoch_ms % 1000);
        const char* ms_src = &log_global_vars::get().digit3_array[m_sec * 3];
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
        auto iter = thread_names_cache_.find(log_entry.get_log_head().log_thread_id);
        if (iter == thread_names_cache_.end()) {
            char tmp[256];
            uint32_t cursor = static_cast<uint32_t>(snprintf(tmp, sizeof(tmp), "[tid-%" PRIu64 " ", log_entry.get_log_head().log_thread_id));
            memcpy(tmp + cursor, (const uint8_t*)&ext_info + sizeof(_log_entry_ext_head_def), ext_info.thread_name_len_);
            cursor += ext_info.thread_name_len_;
            tmp[cursor++] = ']';
            tmp[cursor++] = '\t';
            assert(cursor < 256);
            tmp[cursor] = '\0';
            bq::string thread_name = tmp;
            iter = thread_names_cache_.add(log_entry.get_log_head().log_thread_id, bq::move(thread_name));
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

        int32_t index = 0;
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
            char c = static_cast<char>(style[index]);
            if (c == '{') {
                fi.offset = 0;
                fi.width = 0;
                break;
            } else if (c == '}') {
                fi.offset = static_cast<uint32_t>(index);
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
                fi.width = static_cast<uint32_t>(atoi(width));
                width[0] = '0';
                width[1] = 0;
                break;
            default:
                if (!fill && (c < '1' || c > '9')) {
                    fi.fill = c;
                    if (fi.align == '^' || fi.align == '<')
                        fi.fill = ' ';
                } else {
                    if (isdigit(static_cast<int32_t>(static_cast<uint8_t>(c)))) {
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
            fi.precision = static_cast<uint32_t>(atoi(width));
        } else {
            fi.width = static_cast<uint32_t>(atoi(width));
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
                uint32_t ignore = 0;
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

    void layout::fill_e_style(uint32_t e_count, uint32_t begin_cursor)
    {
        uint32_t bitcount = 0;
        auto temp = e_count;
        do {
            temp /= 10;
            bitcount++;
        } while (temp != 0);
        expand_format_content_buff_size(format_content_cursor + bitcount + 3); // 3->e+0
        // 1.000000 -> 1.0000
        int32_t jump = static_cast<int32_t>(format_content_cursor) - (static_cast<int32_t>(begin_cursor + format_info_.width - bitcount) - 2); // 2->e+
        if (jump > 0) {
            format_content_cursor -= static_cast<uint32_t>(jump);
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
        temp = e_count;
        // 1.0000e+ -> 1.0000e+60
        do {
            int32_t digit = static_cast<int32_t>(e_count % 10);
            format_content[format_content_cursor++] = static_cast<char>('0' + digit);
            e_count /= 10;
        } while (e_count != 0);
        if (temp <= 9)
            format_content[format_content_cursor++] = '0';
        // 1.0000e+60 -> 1.0000e+06
        reverse(begin_cursor, format_content_cursor - 1);
    }

    void layout::python_style_format_content(const bq::log_entry_handle& log_entry)
    {
        if (log_entry.get_log_head().log_format_str_type == static_cast<uint16_t>(log_arg_type_enum::string_utf8_type)) {
            python_style_format_content_utf8(log_entry);
        } else {
            python_style_format_content_utf16(log_entry);
        }
        expand_format_content_buff_size(format_content_cursor + 1);
        format_content[format_content_cursor] = '\0';
        assert(format_content_cursor < format_content.size());
    }

    void layout::python_style_format_content_utf8(const bq::log_entry_handle& log_entry)
    {
        const uint8_t* args_data_ptr = log_entry.get_log_args_data();
        uint32_t args_data_len = log_entry.get_log_args_data_size();
        const char* format_data_ptr = log_entry.get_format_string_data();
        uint32_t format_data_len = log_entry.get_log_head().log_format_data_len;

        if (args_data_len == 0) {
            expand_format_content_buff_size(format_content_cursor + format_data_len);
            memcpy((char*)&format_content[format_content_cursor], format_data_ptr, format_data_len);
            format_content_cursor += format_data_len;
            return;
        }

        // Pre-allocate buffer to ensure sufficient space.
        // Although this might allocate slightly more than necessary, it ensures safety and avoids frequent reallocations.
        expand_format_content_buff_size(format_content_cursor + format_data_len);

        uint32_t i = 0;
        uint32_t args_data_cursor = 0;

        while (i < format_data_len) {
            bool found_brace = false;
            // Step 1: Utilize SIMD-accelerated search to copy content until a brace is encountered.
            uint32_t remaining = format_data_len - i;
            uint32_t copied = find_brace_and_copy(format_data_ptr + i, remaining, &format_content[format_content_cursor], found_brace);

            i += copied;
            format_content_cursor += copied;

            if (found_brace) {
                // Ensure space for potential next characters.

                char c = format_data_ptr[i]; // Expecting '{' or '}'
                i++; // Advance past the brace

                if (i < format_data_len) {
                    char next_c = format_data_ptr[i];
                    // Only handle "}}" as escape. "{{" is handled by nested brace detection logic.
                    if (c == '}' && next_c == '}') {
                        expand_format_content_buff_size(format_content_cursor + 1);
                        format_content[format_content_cursor++] = '}';
                        i++;
                        continue;
                    }
                }

                if (c == '}') { // Handle unbalanced '}', treating it as a literal character.
                    expand_format_content_buff_size(format_content_cursor + 1);
                    format_content[format_content_cursor++] = '}';
                    continue;
                }

                if (format_info_.used)
                    format_info_.reset();

                // Parse format string (e.g. "{:.2f}")
                if (args_data_cursor < args_data_len) {
                    // Look ahead for the closing brace '}' to determine the length of the format specifier.
                    int32_t format_spec_len = 0;
                    bool format_spec_closed = false;
                    for (uint32_t k = i; k < format_data_len && k < i + 20; ++k) { // Limit lookahead to avoid excessive scanning.
                        char c_scan = format_data_ptr[k];
                        if (c_scan == '}') {
                            format_spec_closed = true;
                            break;
                        }
                        if (c_scan == '{') {
                            // Nested brace found (including {{), treat the starting brace as literal
                            format_spec_closed = false;
                            break;
                        }
                        format_spec_len++;
                    }

                    if (format_spec_closed) {
                        format_info_ = c20_format(&format_data_ptr[i], format_spec_len + 1);
                        if (format_info_.offset != 0) {
                            i += (format_info_.offset + 1); // advance past format spec and '}'
                        } else {
                            i += static_cast<uint32_t>(format_spec_len + 1); // advance past content and '}'
                        }

                        auto write_begin_pos = format_content_cursor;
                        uint8_t type_info_i = *(args_data_ptr + args_data_cursor);
                        bq::log_arg_type_enum type_info = static_cast<bq::log_arg_type_enum>(type_info_i);

                        switch (type_info) {
                        case bq::log_arg_type_enum::unsupported_type:
                            bq::util::log_device_console(bq::log_level::warning, "non_primitivi_type is not supported yet");
                            break;
                        case bq::log_arg_type_enum::null_type:
                            assert(sizeof(void*) >= 4);
                            insert_str_utf8("null", static_cast<uint32_t>(sizeof("null") - 1));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::pointer_type:
                            assert(sizeof(void*) >= 4);
                            {
                                const void* arg_data_ptr = *reinterpret_cast<const void* const*>(args_data_ptr + args_data_cursor + 4);
                                insert_pointer(arg_data_ptr);
                                args_data_cursor += static_cast<uint32_t>(4 + sizeof(uint64_t)); // use 64bit pointer for serialize
                            }
                            break;
                        case bq::log_arg_type_enum::bool_type:
                            insert_bool(*reinterpret_cast<const bool*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char_type:
                            insert_char(*reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char16_type:
                            insert_char16(*reinterpret_cast<const char16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char32_type:
                            insert_char32(*reinterpret_cast<const char32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::int8_type:
                            insert_integral_signed(*reinterpret_cast<const int8_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint8_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint8_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int16_type:
                            insert_integral_signed(*reinterpret_cast<const int16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint16_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int32_type:
                            insert_integral_signed(*reinterpret_cast<const int32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::uint32_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::int64_type:
                            insert_integral_signed(*reinterpret_cast<const int64_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 12;
                            break;
                        case bq::log_arg_type_enum::uint64_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint64_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 12;
                            break;
                        case bq::log_arg_type_enum::float_type:
                            insert_decimal(*reinterpret_cast<const float*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(float));
                            break;
                        case bq::log_arg_type_enum::double_type:
                            insert_decimal(*reinterpret_cast<const double*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(double));
                            break;
                        case bq::log_arg_type_enum::string_utf8_type: {
                            const uint32_t* len_ptr = reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4);
                            const char* str = reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t));
                            uint32_t str_len = *len_ptr;
                            insert_str_utf8(str, str_len);
                            args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                        } break;
                        case bq::log_arg_type_enum::string_utf16_type: {
                            const uint32_t* len_ptr = reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4);
                            const char* str = reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t));
                            uint32_t str_len = *len_ptr;
                            insert_str_utf16(str, str_len);
                            args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                        } break;
                        default:
                            break;
                        }

                        assert(args_data_cursor <= args_data_len);
                        fill_and_alignment(write_begin_pos);

                        // Check buffer size after insertion (insert functions handle it, but good to be safe)
                        uint32_t safe_buff_size = format_content_cursor + (format_data_len - i); // Approx remaining
                        expand_format_content_buff_size(safe_buff_size);

                    } else {
                        // Unclosed brace or complex parsing failure, treat as literal '{'
                        expand_format_content_buff_size(format_content_cursor + 1);
                        format_content[format_content_cursor++] = '{';
                        // i was already incremented past '{'
                    }
                } else {
                    // No more args, treat as literal '{'
                    expand_format_content_buff_size(format_content_cursor + 1);
                    format_content[format_content_cursor++] = '{';
                }
            }
        }
    }

    void layout::python_style_format_content_utf16(const bq::log_entry_handle& log_entry)
    {
        const uint8_t* args_data_ptr = log_entry.get_log_args_data();
        uint32_t args_data_len = log_entry.get_log_args_data_size();
        const char16_t* format_data_ptr = (const char16_t*)log_entry.get_format_string_data();
        uint32_t format_data_len = log_entry.get_log_head().log_format_data_len;

        uint32_t safe_buff_size = format_content_cursor + (uint32_t)(((size_t)format_data_len * 3) >> 1);
        expand_format_content_buff_size(safe_buff_size);
        uint32_t wchar_len = format_data_len >> 1;

        uint32_t i = 0;
        uint32_t args_data_cursor = 0;

        while (i < wchar_len) {
            bool found_brace = false;
            bool non_ascii = false;

            // Step 1: Attempt Optimistic Conversion and Copy utilizing SIMD.
            // This process stops at the first non-ASCII character or brace.
            uint32_t remaining = wchar_len - i;
            uint32_t processed = find_brace_and_convert_u16(format_data_ptr + i, remaining, &format_content[format_content_cursor], found_brace, non_ascii);

            i += processed;
            format_content_cursor += processed;

            if (non_ascii) {
                // Fallback to scalar char conversion for the current character.
                if (i < wchar_len) {
                    char16_t c = format_data_ptr[i];
                    i++;

                    if (c < 0x80) {
                        format_content[format_content_cursor++] = (char)c;
                    } else if (c < 0x800) {
                        expand_format_content_buff_size(format_content_cursor + 2);
                        format_content[format_content_cursor++] = (char)(0xC0 | (c >> 6));
                        format_content[format_content_cursor++] = (char)(0x80 | (c & 0x3F));
                    } else if (c >= 0xD800 && c <= 0xDBFF) {
                        // Handle High Surrogate
                        if (i < wchar_len) {
                            char16_t c2 = format_data_ptr[i];
                            if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
                                i++;
                                uint32_t codepoint = static_cast<uint32_t>(0x10000) + (static_cast<uint32_t>(c) - 0xD800) * 0x400 + (static_cast<uint32_t>(c2) - 0xDC00);
                                expand_format_content_buff_size(format_content_cursor + 4);
                                format_content[format_content_cursor++] = (char)(0xF0 | (codepoint >> 18));
                                format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                                format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                                format_content[format_content_cursor++] = (char)(0x80 | (codepoint & 0x3F));
                            }
                        }
                    } else if (c >= 0xDC00 && c <= 0xDFFF) {
                        // Orphan low surrogate, ignore.
                    } else {
                        // Basic Multilingual Plane (BMP) character
                        expand_format_content_buff_size(format_content_cursor + 3);
                        format_content[format_content_cursor++] = (char)(0xE0 | (c >> 12));
                        format_content[format_content_cursor++] = (char)(0x80 | ((c >> 6) & 0x3F));
                        format_content[format_content_cursor++] = (char)(0x80 | (c & 0x3F));
                    }
                }
                continue;
            }

            if (found_brace) {
                char16_t c = format_data_ptr[i];
                i++;

                if (i < wchar_len) {
                    char16_t next_c = format_data_ptr[i];
                    // Only handle "}}" as escape. "{{" is handled by nested brace detection logic.
                    if (c == '}' && next_c == '}') {
                        expand_format_content_buff_size(format_content_cursor + 1);
                        format_content[format_content_cursor++] = static_cast<char>('}');
                        i++;
                        continue;
                    }
                }

                if (c == '}') {
                    expand_format_content_buff_size(format_content_cursor + 1);
                    format_content[format_content_cursor++] = '}';
                    continue;
                }

                if (format_info_.used)
                    format_info_.reset();

                // Parse format string (simple scan in U16)
                if (args_data_cursor < args_data_len) {

                    int32_t format_spec_len = 0;
                    bool format_spec_closed = false;
                    for (uint32_t k = i; k < wchar_len && k < i + 20; ++k) {
                        char16_t c_scan = format_data_ptr[k];
                        if (c_scan == '}') {
                            format_spec_closed = true;
                            break;
                        }
                        if (c_scan == '{') {
                            bool is_escaped = false;
                            if (k + 1 < wchar_len && format_data_ptr[k + 1] == '{') {
                                is_escaped = true;
                            }

                            if (!is_escaped) {
                                format_spec_closed = false;
                                break;
                            } else {
                                k++;
                                format_spec_len++;
                            }
                        }
                        format_spec_len++;
                    }

                    if (format_spec_closed) {
                        format_info_ = c20_format(&format_data_ptr[i], format_spec_len + 1);
                        if (format_info_.offset != 0) {
                            i += (format_info_.offset + 1);
                        } else {
                            i += static_cast<uint32_t>(format_spec_len + 1);
                        }

                        auto write_begin_pos = format_content_cursor;
                        uint8_t type_info_i = *(args_data_ptr + args_data_cursor);
                        bq::log_arg_type_enum type_info = static_cast<bq::log_arg_type_enum>(type_info_i);

                        switch (type_info) {
                        case bq::log_arg_type_enum::unsupported_type:
                            bq::util::log_device_console(bq::log_level::warning, "non_primitivi_type is not supported yet");
                            break;
                        case bq::log_arg_type_enum::null_type:
                            assert(sizeof(void*) >= 4);
                            insert_str_utf8("null", static_cast<uint32_t>(sizeof("null") - 1));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::pointer_type:
                            assert(sizeof(void*) >= 4);
                            {
                                const void* ptr_data = *reinterpret_cast<const void* const*>(args_data_ptr + args_data_cursor + 4);
                                insert_pointer(ptr_data);
                                args_data_cursor += static_cast<uint32_t>(4 + sizeof(uint64_t));
                            }
                            break;
                        case bq::log_arg_type_enum::bool_type:
                            insert_bool(*reinterpret_cast<const bool*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char_type:
                            insert_char(*reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char16_type:
                            insert_char16(*reinterpret_cast<const char16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char32_type:
                            insert_char32(*reinterpret_cast<const char32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::int8_type:
                            insert_integral_signed(*reinterpret_cast<const int8_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint8_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint8_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int16_type:
                            insert_integral_signed(*reinterpret_cast<const int16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint16_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int32_type:
                            insert_integral_signed(*reinterpret_cast<const int32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(int32_t));
                            break;
                        case bq::log_arg_type_enum::uint32_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(uint32_t));
                            break;
                        case bq::log_arg_type_enum::int64_type:
                            insert_integral_signed(*reinterpret_cast<const int64_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(int64_t));
                            break;
                        case bq::log_arg_type_enum::uint64_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint64_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(uint64_t));
                            break;
                        case bq::log_arg_type_enum::float_type:
                            insert_decimal(*reinterpret_cast<const float*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(float));
                            break;
                        case bq::log_arg_type_enum::double_type:
                            insert_decimal(*reinterpret_cast<const double*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(double));
                            break;
                        case bq::log_arg_type_enum::string_utf8_type: {
                            const uint32_t* len_ptr = reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4);
                            const char* str = reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t));
                            uint32_t str_len = *len_ptr;
                            insert_str_utf8(str, str_len);
                            args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                        } break;
                        case bq::log_arg_type_enum::string_utf16_type: {
                            const uint32_t* len_ptr = reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4);
                            const char* str = reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t));
                            uint32_t str_len = *len_ptr;
                            insert_str_utf16(str, str_len);
                            args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                        } break;
                        default:
                            break;
                        }

                        assert(args_data_cursor <= args_data_len);
                        fill_and_alignment(write_begin_pos);

                        safe_buff_size = format_content_cursor + (uint32_t)((wchar_len - i) * 3 / 2);
                        expand_format_content_buff_size(safe_buff_size);
                    } else {
                        expand_format_content_buff_size(format_content_cursor + 1);
                        format_content[format_content_cursor++] = '{';
                    }
                } else {
                    expand_format_content_buff_size(format_content_cursor + 1);
                    format_content[format_content_cursor++] = '{';
                }
            }
        }
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
        uint32_t max_bytes_len = (len * 3 / 2 + 1);
        expand_format_content_buff_size(format_content_cursor + max_bytes_len);
        auto write_size = bq::util::utf16_to_utf8(reinterpret_cast<const char16_t*>(str), len >> 1, &format_content[format_content_cursor], max_bytes_len);
        format_content_cursor += write_size;
        assert(format_content_cursor <= format_content.size());
        return write_size;
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
        expand_format_content_buff_size(format_content_cursor + static_cast<uint32_t>(sizeof(char)));
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
        uint32_t e_count = 0;
        do {
            int32_t digit = static_cast<int32_t>(value % base);
            if (digit < 0xA) {
                format_content[format_content_cursor] = static_cast<char>('0' + digit);
            } else {
                if (format_info_.upper)
                    format_content[format_content_cursor] = static_cast<char>('A' + digit - 0xA);
                else
                    format_content[format_content_cursor] = static_cast<char>('a' + digit - 0xA);
            }
            value /= base;
            ++format_content_cursor;
            if (value > base)
                e_count++;
        } while (value != 0);

        if (format_info_.type == 'e') {
            // 0000001 -> 000000.1
            format_content[format_content_cursor] = format_content[format_content_cursor - 1];
            format_content[format_content_cursor - 1] = '.';
            ++format_content_cursor;

            // 000000.1 -> 1.000000
            reverse(begin_cursor, format_content_cursor - 1);

            fill_e_style(e_count, begin_cursor);
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
        //+-
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
        uint32_t e_count = 0;
        do {
            char digit = static_cast<char>(abs(static_cast<int32_t>(value % static_cast<int32_t>(base))));
            if (digit < 0xA) {
                format_content[format_content_cursor] = static_cast<char>('0' + digit);
            } else {
                if (format_info_.upper)
                    format_content[format_content_cursor] = static_cast<char>('A' + digit - 0xA);
                else
                    format_content[format_content_cursor] = static_cast<char>('a' + digit - 0xA);
            }
            value /= base;
            if (value > base)
                e_count++; // Counting
            ++format_content_cursor;
        } while (value != 0);
        if (format_info_.type == 'e') {
            // 0000001 -> 000000.1
            format_content[format_content_cursor] = format_content[format_content_cursor - 1];
            format_content[format_content_cursor - 1] = '.';
            ++format_content_cursor;

            // 000000.1 -> 1.000000
            reverse(begin_cursor, format_content_cursor - 1);

            fill_e_style(e_count, begin_cursor);
        } else
            reverse(begin_cursor, format_content_cursor - 1);
        return format_content_cursor - width;
    }

    void layout::insert_decimal(float value)
    {
        if (format_info_.type == 'e')
            format_info_.type = 'Z';
        uint32_t int_width = 0;
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
        if (format_info_.width > 0 && format_info_.width < static_cast<uint32_t>(precision) + 1 + int_width) {
            precision = static_cast<int32_t>(format_info_.width - int_width - 1);
        }

        value = fabsf(value);
        expand_format_content_buff_size(format_content_cursor + static_cast<uint32_t>(precision) + 5); // more 5 maybe use in e style
        uint32_t point_index = 0;
        if (precision > 0 || (format_info_.type == 'Z' && int_width > 1)) {
            point_index = format_content_cursor;
            format_content[format_content_cursor++] = '.';
        }
        while (precision > 0) {
            value *= 10;
            int32_t digit = static_cast<int32_t>(value);
            value -= static_cast<float>(digit);
            --precision;
            format_content[format_content_cursor++] = static_cast<char>('0' + digit);
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
        uint32_t int_width = 0;
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
        if (format_info_.width > 0 && format_info_.width < static_cast<uint32_t>(precision) + 1 + int_width) {
            precision = (static_cast<int32_t>(format_info_.width) - static_cast<int32_t>(int_width) - 1);
            if (precision < 0)
                precision = 0;
        }

        value = fabs(value);
        expand_format_content_buff_size(format_content_cursor + static_cast<uint32_t>(precision) + 5); // more 5 maybe use for e-style
        uint32_t point_index = 0;
        if (precision > 0 || (format_info_.type == 'Z' && int_width > 1)) {
            point_index = format_content_cursor;
            format_content[format_content_cursor++] = '.';
        }
        auto temp_precision = precision;
        while (temp_precision > 0) {
            value *= 10;
            int32_t digit = static_cast<int32_t>(value);
            value -= static_cast<double>(digit);
            --temp_precision;
            format_content[format_content_cursor++] = static_cast<char>('0' + digit);
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

#ifdef BQ_UNIT_TEST
    uint32_t layout::test_find_brace_and_copy_sw(const char* src, uint32_t len, char* dst, bool& found_brace)
    {
        return _impl_find_brace_and_copy_sw(src, len, dst, found_brace);
    }
    uint32_t layout::test_find_brace_and_convert_u16_sw(const char16_t* src, uint32_t len, char* dst, bool& found_brace, bool& non_ascii)
    {
        return _impl_find_brace_and_convert_u16_sw(src, len, dst, found_brace, non_ascii);
    }
#if defined(BQ_X86)
    uint32_t layout::test_find_brace_and_copy_avx2(const char* src, uint32_t len, char* dst, bool& found_brace)
    {
        return _impl_find_brace_and_copy_avx2(src, len, dst, found_brace);
    }
    uint32_t layout::test_find_brace_and_copy_sse(const char* src, uint32_t len, char* dst, bool& found_brace)
    {
        return _impl_find_brace_and_copy_sse(src, len, dst, found_brace);
    }
    uint32_t layout::test_find_brace_and_convert_u16_avx2(const char16_t* src, uint32_t len, char* dst, bool& found_brace, bool& non_ascii)
    {
        return _impl_find_brace_and_convert_u16_avx2(src, len, dst, found_brace, non_ascii);
    }
    uint32_t layout::test_find_brace_and_convert_u16_sse(const char16_t* src, uint32_t len, char* dst, bool& found_brace, bool& non_ascii)
    {
        return _impl_find_brace_and_convert_u16_sse(src, len, dst, found_brace, non_ascii);
    }
#elif defined(BQ_ARM_NEON)
    uint32_t layout::test_find_brace_and_copy_neon(const char* src, uint32_t len, char* dst, bool& found_brace)
    {
        return _impl_find_brace_and_copy_neon(src, len, dst, found_brace);
    }
    uint32_t layout::test_find_brace_and_convert_u16_neon(const char16_t* src, uint32_t len, char* dst, bool& found_brace, bool& non_ascii)
    {
        return _impl_find_brace_and_convert_u16_neon(src, len, dst, found_brace, non_ascii);
    }
#endif

    void layout::python_style_format_content_legacy(const bq::log_entry_handle& log_entry)
    {
        if (log_entry.get_log_head().log_format_str_type == static_cast<uint16_t>(log_arg_type_enum::string_utf8_type)) {
            python_style_format_content_utf8_legacy(log_entry);
        } else {
            python_style_format_content_utf16_legacy(log_entry);
        }
        expand_format_content_buff_size(format_content_cursor + 1);
        format_content[format_content_cursor] = '\0';
        assert(format_content_cursor < format_content.size());
    }

    void layout::python_style_format_content_utf8_legacy(const bq::log_entry_handle& log_entry)
    {
        const uint8_t* args_data_ptr = log_entry.get_log_args_data();
        uint32_t args_data_len = log_entry.get_log_args_data_size();
        const char* format_data_ptr = log_entry.get_format_string_data();
        uint32_t format_data_len = log_entry.get_log_head().log_format_data_len;

        expand_format_content_buff_size(format_content_cursor + format_data_len);

        uint32_t i = 0;
        uint32_t args_data_cursor = 0;

        while (i < format_data_len) {
            bool found_brace = false;

            // Legacy slow copy
            while (i < format_data_len) {
                char c = format_data_ptr[i];
                if (c == '{' || c == '}') {
                    found_brace = true;
                    break;
                }
                expand_format_content_buff_size(format_content_cursor + 1);
                format_content[format_content_cursor++] = c;
                i++;
            }

            if (found_brace) {
                char c = format_data_ptr[i];
                i++;

                if (i < format_data_len) {
                    char next_c = format_data_ptr[i];
                    if (c == '}' && next_c == '}') {
                        expand_format_content_buff_size(format_content_cursor + 1);
                        format_content[format_content_cursor++] = '}';
                        i++;
                        continue;
                    }
                }

                if (c == '}') {
                    expand_format_content_buff_size(format_content_cursor + 1);
                    format_content[format_content_cursor++] = '}';
                    continue;
                }

                if (format_info_.used)
                    format_info_.reset();

                if (args_data_cursor < args_data_len) {
                    int32_t format_spec_len = 0;
                    bool format_spec_closed = false;
                    for (uint32_t k = i; k < format_data_len && k < i + 20; ++k) {
                        char c_scan = format_data_ptr[k];
                        if (c_scan == '}') {
                            format_spec_closed = true;
                            break;
                        }
                        if (c_scan == '{') {
                            format_spec_closed = false;
                            break;
                        }
                        format_spec_len++;
                    }

                    if (format_spec_closed) {
                        format_info_ = c20_format(&format_data_ptr[i], format_spec_len + 1);
                        if (format_info_.offset != 0) {
                            i += (format_info_.offset + 1);
                        } else {
                            i += static_cast<uint32_t>(format_spec_len + 1);
                        }

                        auto write_begin_pos = format_content_cursor;
                        uint8_t type_info_i = *(args_data_ptr + args_data_cursor);
                        bq::log_arg_type_enum type_info = static_cast<bq::log_arg_type_enum>(type_info_i);

                        switch (type_info) {
                        case bq::log_arg_type_enum::unsupported_type:
                            bq::util::log_device_console(bq::log_level::warning, "non_primitivi_type is not supported yet");
                            break;
                        case bq::log_arg_type_enum::null_type:
                            assert(sizeof(void*) >= 4);
                            insert_str_utf8("null", static_cast<uint32_t>(sizeof("null") - 1));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::pointer_type:
                            assert(sizeof(void*) >= 4);
                            {
                                const void* arg_data_ptr = *reinterpret_cast<const void* const*>(args_data_ptr + args_data_cursor + 4);
                                insert_pointer(arg_data_ptr);
                                args_data_cursor += static_cast<uint32_t>(4 + sizeof(uint64_t));
                            }
                            break;
                        case bq::log_arg_type_enum::bool_type:
                            insert_bool(*reinterpret_cast<const bool*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char_type:
                            insert_char(*reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char16_type:
                            insert_char16(*reinterpret_cast<const char16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char32_type:
                            insert_char32(*reinterpret_cast<const char32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::int8_type:
                            insert_integral_signed(*reinterpret_cast<const int8_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint8_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint8_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int16_type:
                            insert_integral_signed(*reinterpret_cast<const int16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint16_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int32_type:
                            insert_integral_signed(*reinterpret_cast<const int32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::uint32_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::int64_type:
                            insert_integral_signed(*reinterpret_cast<const int64_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 12;
                            break;
                        case bq::log_arg_type_enum::uint64_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint64_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 12;
                            break;
                        case bq::log_arg_type_enum::float_type:
                            insert_decimal(*reinterpret_cast<const float*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(float));
                            break;
                        case bq::log_arg_type_enum::double_type:
                            insert_decimal(*reinterpret_cast<const double*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(double));
                            break;
                        case bq::log_arg_type_enum::string_utf8_type: {
                            const uint32_t* len_ptr = reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4);
                            const char* str = reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t));
                            uint32_t str_len = *len_ptr;
                            insert_str_utf8(str, str_len);
                            args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                        } break;
                        case bq::log_arg_type_enum::string_utf16_type: {
                            const uint32_t* len_ptr = reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4);
                            const char* str = reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t));
                            uint32_t str_len = *len_ptr;
                            insert_str_utf16(str, str_len);
                            args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                        } break;
                        default:
                            break;
                        }

                        assert(args_data_cursor <= args_data_len);
                        fill_and_alignment(write_begin_pos);

                        expand_format_content_buff_size(format_content_cursor + (format_data_len - i));

                    } else {
                        expand_format_content_buff_size(format_content_cursor + 1);
                        format_content[format_content_cursor++] = '{';
                    }
                } else {
                    expand_format_content_buff_size(format_content_cursor + 1);
                    format_content[format_content_cursor++] = '{';
                }
            }
        }
    }

    void layout::python_style_format_content_utf16_legacy(const bq::log_entry_handle& log_entry)
    {
        const uint8_t* args_data_ptr = log_entry.get_log_args_data();
        uint32_t args_data_len = log_entry.get_log_args_data_size();
        const char16_t* format_data_ptr = (const char16_t*)log_entry.get_format_string_data();
        uint32_t format_data_len = log_entry.get_log_head().log_format_data_len;

        uint32_t safe_buff_size = format_content_cursor + (uint32_t)(((size_t)format_data_len * 3) >> 1);
        expand_format_content_buff_size(safe_buff_size);
        uint32_t wchar_len = format_data_len >> 1;

        uint32_t i = 0;
        uint32_t args_data_cursor = 0;

        while (i < wchar_len) {
            bool found_brace = false;

            // Legacy slow copy
            while (i < wchar_len) {
                char16_t c = format_data_ptr[i];
                if (c == u'{' || c == u'}') {
                    found_brace = true;
                    break;
                }

                // Legacy scalar utf16->utf8 conversion
                if (c < 0x80) {
                    format_content[format_content_cursor++] = (char)c;
                } else if (c < 0x800) {
                    expand_format_content_buff_size(format_content_cursor + 2);
                    format_content[format_content_cursor++] = (char)(0xC0 | (c >> 6));
                    format_content[format_content_cursor++] = (char)(0x80 | (c & 0x3F));
                } else if (c >= 0xD800 && c <= 0xDBFF) {
                    if (i + 1 < wchar_len) {
                        char16_t c2 = format_data_ptr[i + 1];
                        if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
                            i++; // consume extra char
                            uint32_t codepoint = static_cast<uint32_t>(0x10000) + (static_cast<uint32_t>(c) - 0xD800) * 0x400 + (static_cast<uint32_t>(c2) - 0xDC00);
                            expand_format_content_buff_size(format_content_cursor + 4);
                            format_content[format_content_cursor++] = (char)(0xF0 | (codepoint >> 18));
                            format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                            format_content[format_content_cursor++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                            format_content[format_content_cursor++] = (char)(0x80 | (codepoint & 0x3F));
                        }
                    }
                } else if (c >= 0xDC00 && c <= 0xDFFF) {
                    // ignore
                } else {
                    expand_format_content_buff_size(format_content_cursor + 3);
                    format_content[format_content_cursor++] = (char)(0xE0 | (c >> 12));
                    format_content[format_content_cursor++] = (char)(0x80 | ((c >> 6) & 0x3F));
                    format_content[format_content_cursor++] = (char)(0x80 | (c & 0x3F));
                }

                i++;
            }

            if (found_brace) {
                char16_t c = format_data_ptr[i];
                i++;

                if (i < wchar_len) {
                    char16_t next_c = format_data_ptr[i];
                    if (c == '}' && next_c == '}') {
                        expand_format_content_buff_size(format_content_cursor + 1);
                        format_content[format_content_cursor++] = '}';
                        i++;
                        continue;
                    }
                }

                if (c == '}') {
                    expand_format_content_buff_size(format_content_cursor + 1);
                    format_content[format_content_cursor++] = '}';
                    continue;
                }

                if (format_info_.used)
                    format_info_.reset();

                if (args_data_cursor < args_data_len) {
                    int32_t format_spec_len = 0;
                    bool format_spec_closed = false;
                    for (uint32_t k = i; k < wchar_len && k < i + 20; ++k) {
                        char16_t c_scan = format_data_ptr[k];
                        if (c_scan == '}') {
                            format_spec_closed = true;
                            break;
                        }
                        if (c_scan == '{') {
                            format_spec_closed = false;
                            break;
                        }
                        format_spec_len++;
                    }

                    if (format_spec_closed) {
                        format_info_ = c20_format(&format_data_ptr[i], format_spec_len + 1);
                        if (format_info_.offset != 0) {
                            i += (format_info_.offset + 1);
                        } else {
                            i += (uint32_t)(format_spec_len + 1);
                        }

                        auto write_begin_pos = format_content_cursor;
                        uint8_t type_info_i = *(args_data_ptr + args_data_cursor);
                        bq::log_arg_type_enum type_info = static_cast<bq::log_arg_type_enum>(type_info_i);

                        switch (type_info) {
                        case bq::log_arg_type_enum::unsupported_type:
                            bq::util::log_device_console(bq::log_level::warning, "non_primitivi_type is not supported yet");
                            break;
                        case bq::log_arg_type_enum::null_type:
                            assert(sizeof(void*) >= 4);
                            insert_str_utf8("null", static_cast<uint32_t>(sizeof("null") - 1));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::pointer_type:
                            assert(sizeof(void*) >= 4);
                            {
                                const void* ptr_data = *reinterpret_cast<const void* const*>(args_data_ptr + args_data_cursor + 4);
                                insert_pointer(ptr_data);
                                args_data_cursor += static_cast<uint32_t>(4 + sizeof(uint64_t));
                            }
                            break;
                        case bq::log_arg_type_enum::bool_type:
                            insert_bool(*reinterpret_cast<const bool*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char_type:
                            insert_char(*reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char16_type:
                            insert_char16(*reinterpret_cast<const char16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::char32_type:
                            insert_char32(*reinterpret_cast<const char32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += 8;
                            break;
                        case bq::log_arg_type_enum::int8_type:
                            insert_integral_signed(*reinterpret_cast<const int8_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint8_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint8_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int16_type:
                            insert_integral_signed(*reinterpret_cast<const int16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::uint16_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint16_t*>(args_data_ptr + args_data_cursor + 2));
                            args_data_cursor += 4;
                            break;
                        case bq::log_arg_type_enum::int32_type:
                            insert_integral_signed(*reinterpret_cast<const int32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(int32_t));
                            break;
                        case bq::log_arg_type_enum::uint32_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(uint32_t));
                            break;
                        case bq::log_arg_type_enum::int64_type:
                            insert_integral_signed(*reinterpret_cast<const int64_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(int64_t));
                            break;
                        case bq::log_arg_type_enum::uint64_type:
                            insert_integral_unsigned(*reinterpret_cast<const uint64_t*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(uint64_t));
                            break;
                        case bq::log_arg_type_enum::float_type:
                            insert_decimal(*reinterpret_cast<const float*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(float));
                            break;
                        case bq::log_arg_type_enum::double_type:
                            insert_decimal(*reinterpret_cast<const double*>(args_data_ptr + args_data_cursor + 4));
                            args_data_cursor += static_cast<uint32_t>(4 + sizeof(double));
                            break;
                        case bq::log_arg_type_enum::string_utf8_type: {
                            const uint32_t* len_ptr = reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4);
                            const char* str = reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t));
                            uint32_t str_len = *len_ptr;
                            insert_str_utf8(str, str_len);
                            args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                        } break;
                        case bq::log_arg_type_enum::string_utf16_type: {
                            const uint32_t* len_ptr = reinterpret_cast<const uint32_t*>(args_data_ptr + args_data_cursor + 4);
                            const char* str = reinterpret_cast<const char*>(args_data_ptr + args_data_cursor + 4 + sizeof(uint32_t));
                            uint32_t str_len = *len_ptr;
                            insert_str_utf16(str, str_len);
                            args_data_cursor += static_cast<uint32_t>(4U + sizeof(uint32_t) + bq::align_4(str_len));
                        } break;
                        default:
                            break;
                        }

                        assert(args_data_cursor <= args_data_len);
                        fill_and_alignment(write_begin_pos);

                        safe_buff_size = format_content_cursor + (uint32_t)((wchar_len - i) * 3 / 2);
                        expand_format_content_buff_size(safe_buff_size);
                    } else {
                        expand_format_content_buff_size(format_content_cursor + 1);
                        format_content[format_content_cursor++] = '{';
                    }
                } else {
                    expand_format_content_buff_size(format_content_cursor + 1);
                    format_content[format_content_cursor++] = '{';
                }
            }
        }
    }

    void layout::test_python_style_format_content_legacy(const bq::log_entry_handle& log_entry)
    {
        format_content_cursor = 0;
        python_style_format_content_legacy(log_entry);
    }

    void layout::test_python_style_format_content_sw(const bq::log_entry_handle& log_entry)
    {
        format_content_cursor = 0;
#if defined(BQ_X86)
        bool old_avx = _bq_avx2_supported_;
        _bq_avx2_supported_ = false;
#endif
        python_style_format_content(log_entry);
#if defined(BQ_X86)
        _bq_avx2_supported_ = old_avx;
#endif
    }

    void layout::test_python_style_format_content_simd(const bq::log_entry_handle& log_entry)
    {
        format_content_cursor = 0;
        python_style_format_content(log_entry);
    }
#endif
}