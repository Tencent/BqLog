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

// Only works for C++14 and later
#include "bq_common/bq_common.h"
#if defined(BQ_CPP_14)
#include <cstddef>
#include <math.h>
#include <tuple>
#include <utility>
#include <array>
#include <locale>
#include <string>
#include "test_log.h"

namespace bq {
    namespace test {
        static test_result* result_ptr;
        static const test_category_log* log_inst_ptr;
        static bq::string* output_str_ptr;
        static uint32_t snapshot_idx_mode = 1;
        static bq::string pub_key;
        static bq::string priv_key;

        constexpr uint32_t snapshot_buffer_size = 32 * 1024;
        constexpr size_t fmt_text_count = 1024;

        static bq::string log_head;

        static bq::string log_str_templates_standard_utf8[fmt_text_count];
        static bq::string log_str_templates_fmt_utf8[fmt_text_count];
        static constexpr const char* fmt_string_tail_utf8 = "{}ab{c{d{}{{}{efghijk}";

        static bq::u16string log_str_templates_standard_utf16[fmt_text_count];
        static bq::u16string log_str_templates_fmt_utf16[fmt_text_count];
        static constexpr const char16_t* fmt_string_tail_utf16 = u"{}ab{c{d{}{{}{efghijk}"; // must be as same as fmt_string_tail_utf8

        static bq::string snapshot_test_str;

        static bq::array<bq::string> test_log_3_all_console_outputs;

        // Larger parameter (eg. 3) will result in an exponential increase in test time overhead.
        template <size_t BITS>
        struct param_count_helper { };
        template <>
        struct param_count_helper<32> {
            constexpr static int32_t MAX_PARAM = 2;
        };
        template <>
        struct param_count_helper<64> {
            constexpr static int32_t MAX_PARAM = 2;
        };
        static constexpr size_t MAX_PARAM = param_count_helper<sizeof(void*) * 8>::MAX_PARAM;

        template <typename T, typename... Ts>
        constexpr std::array<T, sizeof...(Ts)> test_make_array(Ts... ts)
        {
            return { ts... };
        }

        enum test_log_enum {
            log1,
            log2
        };

        struct custom_type1 {
            const char* bq_log_format_str_chars() const
            {
                return "custom_type1";
            }

            size_t bq_log_format_str_size() const
            {
                return (uint32_t)strlen("custom_type1");
            }
        };

        struct custom_type2 {
        };

        struct custom_type3 {
        private:
            const char16_t* data = u"custom_type3";
        public:
            const char16_t* bq_log_format_str_chars() const
            {
                return data;
            }

            size_t bq_log_format_str_size() const
            {
                return (uint32_t)bq::___string_len(data);
            }
        };

        struct custom_type4 {
        private:
            const char16_t* data = u"custom_type4";

        public:
            size_t bq_log_format_str_size() const
            {
                return (uint32_t)bq::___string_len(data);
            }

            void bq_log_custom_format(char* dest, size_t data_size) const{
                assert(data_size >= bq_log_format_str_size());
                bq::util::utf16_to_utf8(data, static_cast<uint32_t>(bq_log_format_str_size()), dest, static_cast<uint32_t>(data_size));
            }
        };

        struct custom_type5 {
            const char16_t* data = u"custom_type5";

            size_t bq_log_format_str_size() const
            {
                return (uint32_t)bq::___string_len(data);
            }
        };

        const char* bq_log_format_str_chars(const custom_type1& value)
        {
            (void)value;
            return "custom_type1A";
        }

        size_t bq_log_format_str_size(const custom_type1& value)
        {
            (void)value;
            return (size_t)strlen("custom_type1A");
        }

        const char16_t* bq_log_format_str_chars(const custom_type2& value)
        {
            (void)value;
            return u"custom_type2";
        }

        size_t bq_log_format_str_size(const custom_type2& value)
        {
            (void)value;
            return (size_t)strlen("custom_type2");
        }

        void bq_log_custom_format(const custom_type5& obj, char* dest, size_t data_size) {
            assert(data_size >= obj.bq_log_format_str_size());
            bq::util::utf16_to_utf8(obj.data, static_cast<uint32_t>(obj.bq_log_format_str_size()), dest, static_cast<uint32_t>(data_size));
        }

        constexpr auto null_params = test_make_array<std::nullptr_t>(nullptr);
        constexpr auto bool_params = test_make_array<bool>(true, false);
        constexpr auto char_params = test_make_array<char>('a', '\t');
        constexpr auto char16_params = test_make_array<char16_t>(u'A', u'\t');
        constexpr auto char32_params = test_make_array<char32_t>(U'a', U'飞', (char32_t)(0x0001F600));
        constexpr auto int8_params = test_make_array<int8_t>((int8_t)33, (int8_t)-123);
        constexpr auto uint8_params = test_make_array<uint8_t>((uint8_t)1, (uint8_t)255);
        constexpr auto int16_params = test_make_array<int16_t>((int16_t)32532, (int16_t)-22123);
        constexpr auto uint16_params = test_make_array<uint16_t>((uint16_t)1, (uint16_t)65531);
        constexpr auto int32_params = test_make_array<int32_t>((int32_t)-22123, (int32_t)0xffff3223);
        constexpr auto uint32_params = test_make_array<uint32_t>((uint32_t)0x324242fa, (uint32_t)0xffffffff);
        constexpr auto int64_params = test_make_array<int64_t>((int64_t)288888880, (int64_t)-5);
        constexpr auto uint64_params = test_make_array<uint64_t>((uint64_t)1, (uint64_t)0xffaaeeff33333333ULL);
        constexpr auto float_params = test_make_array<float>((float)-484323233.323234f);
        constexpr auto double_params = test_make_array<double>((double)324248284.8, (double)-484323233.323234);
        constexpr auto utf8_params = test_make_array<const char*>(nullptr, "ab", "utf8测试代码看看呢");
        constexpr auto utf16_params = test_make_array<const char16_t*>(nullptr, u"ab", u"utf16测试代码看一看");
        constexpr auto utf32_params = test_make_array<const char32_t*>(nullptr, U"abc", U"utf16测试代码看一看");
        constexpr auto wchar_array_params = test_make_array<const wchar_t*>(nullptr, L"ab", L"utf16测试代码看一看");
        constexpr auto enum_params = test_make_array<test_log_enum>(test_log_enum::log1, test_log_enum::log2);
        constexpr std::array<const char[7], 2> utf8_char_array_params { { { 'a', 'b', 'c', 'd', 'e', 'f', 'g' }, { '1', '2', '3', '4', '5', '6', '7' } } };
        constexpr std::array<const char16_t[7], 2> utf16_char_array_params { { { u'a', u'b', u'c', u'd', u'e', u'f', u'g' }, { u'扁', u'鹊', u'日', u'志', u'测', u'试', u'。' } } };
        constexpr auto data_tuple = std::make_tuple(
            null_params,
            bool_params,
            char_params,
            char16_params,
            char32_params,
            int8_params,
            uint8_params,
            int16_params,
            uint16_params,
            int32_params,
            uint32_params,
            int64_params,
            uint64_params,
            float_params,
            double_params,
            utf8_params,
            utf16_params,
            utf32_params,
            wchar_array_params,
            enum_params,
            utf8_char_array_params,
            utf16_char_array_params);

        template <typename... Arrays>
        struct array_size_helper {
        };

        template <typename ArrayType>
        struct array_size_helper<ArrayType> {
            static constexpr size_t size = std::tuple_size<ArrayType>::value;
        };

        template <typename First, typename... Rest>
        struct array_size_helper<First, Rest...> {
            static constexpr size_t size = std::tuple_size<First>::value + array_size_helper<Rest...>::size;
        };

        template <typename... Arrays>
        constexpr size_t add_of_sizes(const std::tuple<Arrays...>& t)
        {
            (void)t;
            return array_size_helper<Arrays...>::size;
        }

        static size_t total_test_num = 0;

        static size_t current_tested_num = 0;
        static size_t current_tested_percent = 0;

        enum test_log_3_phase {
            calculate_count,
            do_test
        };
        static test_log_3_phase test_3_phase = test_log_3_phase::calculate_count;

        template <typename CHAR_TYPE>
        bq::string get_utf8_from_utf16(const CHAR_TYPE* input_str)
        {
            const char16_t* str = (const char16_t*)input_str;
            static_assert(sizeof(CHAR_TYPE) == 2, "invalid utf16 char type");
            bq::string result;
            bq::u16string from = str;
            uint32_t wchar_len = (uint32_t)from.size();
            uint32_t codepoint = 0;
            uint32_t surrogate = 0;

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
                        result.push_back((char)c);

                        while ((uint32_t)(p - s) < wchar_len && *p && *p < 0x80) {
                            result.push_back(*((const char*)p));
                            p++;
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
                    result.push_back((char)codepoint);
                } else if (codepoint < 0x0800) {
                    result.push_back((char)(0xC0 | (codepoint >> 6)));
                    result.push_back((char)(0x80 | (codepoint & 0x3F)));
                } else if (codepoint < 0x10000) {
                    result.push_back((char)(0xE0 | (codepoint >> 12)));
                    result.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
                    result.push_back((char)(0x80 | (codepoint & 0x3F)));
                } else {
                    result.push_back((char)(0xF0 | (codepoint >> 18)));
                    result.push_back((char)(0x80 | ((codepoint >> 12) & 0x3F)));
                    result.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
                    result.push_back((char)(0x80 | (codepoint & 0x3F)));
                }
            }
            return result;
        }

        template <typename CHAR_TYPE>
        bq::string get_utf8_from_utf32(const CHAR_TYPE* input_str)
        {
            const char32_t* str = (const char32_t*)input_str;
            static_assert(sizeof(CHAR_TYPE) == 4, "invalid utf32 char type");
            bq::u16string result_u16;
            bq::u32string from = str;
            for (bq::u32string::char_type c : from) {
                if (c <= 0xFFFF) {
                    result_u16.push_back(static_cast<char16_t>(c));
                } else {
                    c -= 0x10000;
                    result_u16.push_back(static_cast<char16_t>((c >> 10) + 0xD800));
                    result_u16.push_back(static_cast<char16_t>((c & 0x3FF) + 0xDC00));
                }
            }
            return get_utf8_from_utf16(result_u16.c_str());
        }

        struct test_format_helper {
        public:
            static bq::string trans(bool value)
            {
                return value ? "TRUE" : "FALSE";
            }
            static bq::string trans(std::nullptr_t value)
            {
                (void)value;
                return "null";
            }
            static bq::string trans(void* value)
            {
                if (!value) {
                    return "null";
                }
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "0x%" PRIX64, (uint64_t)value);
                return tmp;
            }
            static bq::string trans(char c)
            {
                bq::string result;
                result.insert(result.begin(), c);
                return result;
            }
            static bq::string trans(char16_t c)
            {
                char16_t str[] = { c, u'\0' };
                return get_utf8_from_utf16(str);
            }
            static bq::string trans(char32_t c)
            {
                char32_t str[] = { c, U'\0' };
                return get_utf8_from_utf32(str);
            }
            static bq::enable_if_t<!bq::is_same<int8_t, char>::value, bq::string> trans(int8_t value)
            {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%" PRId8, value);
                return tmp;
            }
            static bq::enable_if_t<!bq::is_same<uint8_t, char>::value, bq::string> trans(uint8_t value)
            {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%" PRIu8, value);
                return tmp;
            }
            static bq::string trans(int16_t value)
            {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%" PRId16, value);
                return tmp;
            }
            static bq::string trans(uint16_t value)
            {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%" PRIu16, value);
                return tmp;
            }
            static bq::string trans(int32_t value)
            {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%" PRId32, value);
                return tmp;
            }
            static bq::string trans(uint32_t value)
            {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%" PRIu32, value);
                return tmp;
            }
            static bq::string trans(int64_t value)
            {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%" PRId64, value);
                return tmp;
            }
            static bq::string trans(uint64_t value)
            {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%" PRIu64, value);
                return tmp;
            }
            static bq::string trans(const custom_type1& value)
            {
                (void)value;
                return "custom_type1";
            }
            static bq::string trans(const custom_type2& value)
            {
                (void)value;
                return "custom_type2";
            }
            static bq::string trans(const custom_type3& value)
            {
                (void)value;
                return "custom_type3";
            }
            static bq::string trans(const custom_type4& value)
            {
                (void)value;
                return "custom_type4";
            }
            static bq::string trans(const custom_type5& value)
            {
                (void)value;
                return "custom_type5";
            }
            static bq::string trans(float value)
            {
                bq::string result;
                int32_t precision = 7;
                if (value >= 0) {
                    uint64_t i_part = static_cast<uint64_t>(value);
                    result = trans(i_part);
                    value -= static_cast<float>(i_part);
                } else {
                    int64_t i_part = static_cast<int64_t>(value);
                    result = trans(i_part);
                    value -= static_cast<float>(i_part);
                }
                value = fabsf(value);
                result += ".";
                while (precision > 0) {
                    value *= 10;
                    int32_t digit = (int32_t)value;
                    value -= (float)(digit);
                    --precision;
                    result.push_back((char)('0' + (char)digit));
                }
                return result;
            }
            static bq::string trans(double value)
            {
                bq::string result;
                int32_t precision = 15;
                if (value >= 0) {
                    uint64_t i_part = static_cast<uint64_t>(value);
                    result = trans(i_part);
                    value -= static_cast<double>(i_part);
                } else {
                    int64_t i_part = static_cast<int64_t>(value);
                    result = trans(i_part);
                    value -= static_cast<double>(i_part);
                }
                value = fabs(value);
                result += ".";
                while (precision > 0) {
                    value *= 10;
                    int32_t digit = (int32_t)value;
                    value -= (double)(digit);
                    --precision;
                    result.push_back((char)('0' + (char)digit));
                }
                return result;
            }
            template <size_t N>
            static bq::string trans_utf8_char_array_impl(const char (&value)[N])
            {
                bq::string result;
                result.insert_batch(result.end(), value, N);
                return result;
            }
            template <typename T>
            static typename bq::enable_if<(bq::tools::_is_utf8_c_style_string<T>::value && bq::is_array<T>::value), bq::string>::type trans(const T& value)
            {
                return trans_utf8_char_array_impl(value);
            }
            template <typename T>
            static typename bq::enable_if<(bq::tools::_is_utf8_c_style_string<T>::value && bq::is_pointer<T>::value), bq::string>::type trans(const T& value)
            {
                if (!value) {
                    return "null";
                }
                return value;
            }

            template <typename CHAR_TYPE, size_t N>
            static bq::string trans_utf16_char_array_impl(const CHAR_TYPE (&value)[N])
            {
                bq::u16string result;
                result.insert_batch(result.end(), (const char16_t*)value, N);
                return get_utf8_from_utf16(result.c_str());
            }
            template <typename T>
            static typename bq::enable_if<bq::tools::_is_utf16_c_style_string<T>::value && bq::is_array<T>::value, bq::string>::type trans(const T& value)
            {
                return trans_utf16_char_array_impl(value);
            }
            template <typename T>
            static typename bq::enable_if<bq::tools::_is_utf16_c_style_string<T>::value && bq::is_pointer<T>::value, bq::string>::type trans(const T& value)
            {
                if (!value) {
                    return "null";
                }
                return get_utf8_from_utf16(value);
            }

            template <typename CHAR_TYPE, size_t N>
            static bq::string trans_utf32_char_array_impl(const CHAR_TYPE (&value)[N])
            {
                bq::u32string result;
                result.insert_batch(result.end(), (const char32_t*)value, N);
                return get_utf8_from_utf16(result.c_str());
            }
            template <typename T>
            static typename bq::enable_if<bq::tools::_is_utf32_c_style_string<T>::value && bq::is_array<T>::value, bq::string>::type trans(const T& value)
            {
                return trans_utf32_char_array_impl(value);
            }
            template <typename T>
            static typename bq::enable_if<bq::tools::_is_utf32_c_style_string<T>::value && bq::is_pointer<T>::value, bq::string>::type trans(const T& value)
            {
                if (!value) {
                    return "null";
                }
                return get_utf8_from_utf32(value);
            }

            template <typename T>
            static typename bq::enable_if<bq::tools::_is_std_string_view_like<T>::value, bq::string>::type trans(const T& value)
            {
                return trans(value.data());
            }

            template <typename T>
            static typename bq::enable_if<bq::tools::_is_bq_string_like<T>::value, bq::string>::type trans(const T& value)
            {
                return trans(value.c_str());
            }

            static bq::string trans(test_log_enum value)
            {
                char str[2];
                str[0] = '0' + (char)value;
                str[1] = '\0';
                return str;
            }
        };

        static void clear_test_output_folder()
        {
            bq::string path = bq::file_manager::trans_process_relative_path_to_absolute_path("bqLog/UnitTestLog", 1);
            bq::file_manager::instance().remove_file_or_dir(path);
        }

        template <size_t PARAM_COUNT>
        static void init_fmt_strings()
        {
            {
                // utf8
                char buffer[fmt_text_count];
                char buffer_idx[fmt_text_count];
                for (size_t i = 0; i < fmt_text_count; ++i) {
                    auto idx_char_count = (size_t)snprintf(buffer_idx, fmt_text_count, "%d_", (int32_t)i);
                    buffer[i] = 'a' + (char)(i % 26);
                    log_str_templates_standard_utf8[i] = "test log:";
                    log_str_templates_standard_utf8[i].insert_batch(log_str_templates_standard_utf8[i].begin(), buffer, i);
                    log_str_templates_standard_utf8[i].insert_batch(log_str_templates_standard_utf8[i].begin(), buffer_idx, idx_char_count);

                    log_str_templates_fmt_utf8[i] = log_str_templates_standard_utf8[i];
                    for (size_t j = 0; j < PARAM_COUNT; ++j) {
                        log_str_templates_fmt_utf8[i] += " {},";
                    }
                    log_str_templates_fmt_utf8[i] += fmt_string_tail_utf8;
                }
            }
            {
                // utf16
                char16_t buffer[fmt_text_count];
                char buffer_idx[fmt_text_count];
                char16_t buffer_idx_utf16[fmt_text_count];
                for (size_t i = 0; i < fmt_text_count; ++i) {
                    auto idx_char_count = (size_t)snprintf(buffer_idx, fmt_text_count, "%d_", (int32_t)i);
                    idx_char_count = (size_t)bq::util::utf8_to_utf16(buffer_idx, (uint32_t)idx_char_count, buffer_idx_utf16, (uint32_t)fmt_text_count);
                    buffer[i] = u'唉' + (char16_t)i;
                    log_str_templates_standard_utf16[i] = u"test log:";
                    log_str_templates_standard_utf16[i].insert_batch(log_str_templates_standard_utf16[i].begin(), buffer, i);
                    log_str_templates_standard_utf16[i].insert_batch(log_str_templates_standard_utf16[i].begin(), buffer_idx_utf16, idx_char_count);

                    log_str_templates_fmt_utf16[i] = log_str_templates_standard_utf16[i];
                    for (size_t j = 0; j < PARAM_COUNT; ++j) {
                        log_str_templates_fmt_utf16[i] += u"(#`O′)测试{},";
                    }
                    log_str_templates_fmt_utf16[i] += fmt_string_tail_utf16;
                }
            }
        }

        struct decoder_test {
        private:
            bq::tools::log_decoder* decoder = nullptr;
            bq::string file_path;
            bq::string ext_name;

        public:
            decoder_test(const bq::string& extname)
            {
                ext_name = extname;
                pick_new_log_file();
            }

            ~decoder_test()
            {
                delete decoder;
            }

            bq::tools::log_decoder& get()
            {
                return *decoder;
            }

            void pick_new_log_file()
            {
                if (decoder) {
                    delete decoder;
                    bq::file_manager::instance().remove_file_or_dir(file_path);
                }
                bq::array<bq::string> file_list;
                bq::string path = TO_ABSOLUTE_PATH("bqLog/UnitTestLog", 1);
                bq::file_manager::get_all_files(path, file_list);
                for (auto iter = file_list.begin(); iter != file_list.end(); ++iter) {
                    if (iter->end_with(ext_name)) {
                        file_path = *iter;
                    }
                }
                assert(!file_path.is_empty() && "failed to read test output log file");
                decoder = new bq::tools::log_decoder(file_path, priv_key);
            }
        };

        static const bq::string& decode_raw_item()
        {
            static decoder_test decoder(".lograw");
            auto result = decoder.get().decode();
            if (result == bq::appender_decode_result::eof) {
                decoder.pick_new_log_file();
                result = decoder.get().decode();
            }
            result_ptr->add_result(result == bq::appender_decode_result::success, "decoder failed, error code:%d", (int32_t)result);
            return decoder.get().get_last_decoded_log_entry();
        }

        static const bq::string& decode_compressed_item()
        {
            static decoder_test decoder(".logcompr");
            auto result = decoder.get().decode();
            if (result == bq::appender_decode_result::eof) {
                decoder.pick_new_log_file();
                result = decoder.get().decode();
            }
            result_ptr->add_result(result == bq::appender_decode_result::success, "decoder failed, error code:%d", (int32_t)result);
            return decoder.get().get_last_decoded_log_entry();
        }

        template <typename First>
        void parameter_expand_tools(First first)
        {
            (void)first;
            // use (EXPAND ,...) in C++ 14 directly may causes compiling issue.
        }

        template <typename First, typename... Args>
        void parameter_expand_tools(First first, Args... args)
        {
            (void)first;
            parameter_expand_tools(args...);
            // use (EXPAND ,...) in C++ 14 directly may causes compiling issue.
        }

        template <size_t LEFT_PARAM_COUNT, size_t TYPE_INDEX, size_t OBJ_INDEX, typename PARAM_TUPLE>
        int32_t log_param_test_level3(const PARAM_TUPLE& param_tuple);

        template <size_t LEFT_PARAM_COUNT, size_t TYPE_INDEX, typename PARAM_TUPLE, size_t... OBJ_INDICES>
        void log_param_test_level2_unpack(const PARAM_TUPLE& param_tuple, const std::index_sequence<OBJ_INDICES...>& seq);

        template <size_t LEFT_PARAM_COUNT, size_t TYPE_INDEX, typename PARAM_TUPLE>
        int32_t log_param_test_level2(const PARAM_TUPLE& param_tuple);

        template <size_t LEFT_PARAM_COUNT, typename PARAM_TUPLE, size_t... TYPE_INDICES>
        void log_param_test_level1_unpack(const PARAM_TUPLE& param_tuple, const std::index_sequence<TYPE_INDICES...>& seq);

        template <size_t LEFT_PARAM_COUNT>
        struct log_param_test_level1;

        template <>
        struct log_param_test_level1<0>;

        char tmp[128] = { 0 };
        char16_t tmp16[128] = { 0 };
        char32_t tmp32[128] = { 0 };
        wchar_t tmpwchar[128] = { 0 };
#define BQ_STR_ADDITIONAL_TEST(param)                                                                \
    {                                                                                                \
        bq::string test_str = param;                                                                 \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }
#define BQ_U16_STR_ADDITIONAL_TEST(param)                                                            \
    {                                                                                                \
        bq::u16string test_str = param;                                                              \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }
#define BQ_U32_STR_ADDITIONAL_TEST(param)                                                            \
    {                                                                                                \
        bq::u32string test_str = param;                                                              \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }

#define STD_STR_ADDITIONAL_TEST(param)                                                               \
    {                                                                                                \
        std::string test_str = param;                                                                \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }
#define STD_U16_STR_ADDITIONAL_TEST(param)                                                           \
    {                                                                                                \
        std::u16string test_str = param;                                                             \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }
#define STD_U32_STR_ADDITIONAL_TEST(param)                                                           \
    {                                                                                                \
        std::u32string test_str = param;                                                             \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }
#define STD_WCHAR_STR_ADDITIONAL_TEST(param)                                                         \
    {                                                                                                \
        std::wstring test_str = param;                                                               \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }
#if defined(BQ_CPP_17)
#define STD_STR_VIEW_ADDITIONAL_TEST(param)                                                          \
    {                                                                                                \
        std::string_view test_str = param;                                                           \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }
#define STD_U16_STR_VIEW_ADDITIONAL_TEST(param)                                                      \
    {                                                                                                \
        std::u16string_view test_str = param;                                                        \
        auto new_param_tuple_with_test_str = std::tuple_cat(param_tuple, std::make_tuple(test_str)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str);                \
    }
#endif
#define CUSTOM_TYPE_ADDITIONAL_TEST()                                                               \
    {                                                                                                \
        custom_type1 test_var1;                                                                       \
        auto new_param_tuple_with_test_str1 = std::tuple_cat(param_tuple, std::make_tuple(test_var1)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str1);                \
        custom_type2 test_var2;                                                                       \
        auto new_param_tuple_with_test_str2 = std::tuple_cat(param_tuple, std::make_tuple(test_var2)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str2);                \
        custom_type3 test_var3;                                                                       \
        auto new_param_tuple_with_test_str3 = std::tuple_cat(param_tuple, std::make_tuple(test_var3)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str3);                \
        custom_type4 test_var4;                                                                       \
        auto new_param_tuple_with_test_str4 = std::tuple_cat(param_tuple, std::make_tuple(test_var4)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str4);                \
        custom_type5 test_var5;                                                                       \
        auto new_param_tuple_with_test_str5 = std::tuple_cat(param_tuple, std::make_tuple(test_var5)); \
        log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_test_str5);                \
    }

#define CUSTOM_TYPE2_ADDITIONAL_TEST()                                                               \
    {                                                                                                \
    }

        template <size_t LEFT_PARAM_COUNT, size_t TYPE_INDEX, size_t OBJ_INDEX, typename PARAM_TUPLE>
        int32_t log_param_test_level3(const PARAM_TUPLE& param_tuple)
        {
            auto& new_param_group = std::get<TYPE_INDEX>(data_tuple);
            auto& new_param_obj = new_param_group[OBJ_INDEX];
            auto new_tuple_element = std::tuple<decltype(new_param_obj)&>(new_param_obj);
            auto new_param_tuple = std::tuple_cat(param_tuple, new_tuple_element);
            log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple);
            return 0;
        }

        template <size_t LEFT_PARAM_COUNT, size_t TYPE_INDEX, typename PARAM_TUPLE, size_t... OBJ_INDICES>
        void log_param_test_level2_unpack(const PARAM_TUPLE& param_tuple, const std::index_sequence<OBJ_INDICES...>& seq)
        {
            (void)seq;
            parameter_expand_tools(log_param_test_level3<LEFT_PARAM_COUNT, TYPE_INDEX, OBJ_INDICES>(param_tuple)...);
        }

        template <size_t LEFT_PARAM_COUNT, size_t TYPE_INDEX, typename PARAM_TUPLE>
        int32_t log_param_test_level2(const PARAM_TUPLE& param_tuple)
        {
            auto seq = std::make_index_sequence<std::get<TYPE_INDEX>(data_tuple).size()>();
            log_param_test_level2_unpack<LEFT_PARAM_COUNT, TYPE_INDEX>(param_tuple, seq);
            return 0;
        }

        template <size_t LEFT_PARAM_COUNT, typename PARAM_TUPLE, size_t... TYPE_INDICES>
        void log_param_test_level1_unpack(const PARAM_TUPLE& param_tuple, const std::index_sequence<TYPE_INDICES...>& seq)
        {
            (void)seq;
            parameter_expand_tools(log_param_test_level2<LEFT_PARAM_COUNT, TYPE_INDICES>(param_tuple)...);

            // left params which can not list in constexpr

            // void* can not add to constexpr array, so what I only can do is that add it to param tuple mannualy
            auto new_param_tuple_with_ptr1 = std::tuple_cat(param_tuple, std::make_tuple((void*)nullptr));
            log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_ptr1);
            auto new_param_tuple_with_ptr2 = std::tuple_cat(param_tuple, std::make_tuple((void*)result_ptr));
            log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_ptr2);
            auto new_param_tuple_with_ptr3 = std::tuple_cat(param_tuple, std::make_tuple((void*)&result_ptr));
            log_param_test_level1<LEFT_PARAM_COUNT - 1>()(new_param_tuple_with_ptr3);

            BQ_STR_ADDITIONAL_TEST(tmp);
            BQ_STR_ADDITIONAL_TEST("");
            BQ_STR_ADDITIONAL_TEST(nullptr);
            BQ_STR_ADDITIONAL_TEST("1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg");

            BQ_U16_STR_ADDITIONAL_TEST(tmp16);
            BQ_U16_STR_ADDITIONAL_TEST(u"1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg");

            BQ_U32_STR_ADDITIONAL_TEST(tmp32);
            BQ_U32_STR_ADDITIONAL_TEST(U"1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg1234.323rgsdfg");

            STD_STR_ADDITIONAL_TEST("1234.323");
            STD_U16_STR_ADDITIONAL_TEST(u"1234.323");
            STD_U32_STR_ADDITIONAL_TEST(U"1234.323");
            STD_WCHAR_STR_ADDITIONAL_TEST(L"1234.323");
#if defined(BQ_CPP_17)
            STD_STR_VIEW_ADDITIONAL_TEST("StringVewTest");
            STD_U16_STR_VIEW_ADDITIONAL_TEST(u"StringVewTest");
#endif
            CUSTOM_TYPE_ADDITIONAL_TEST();
        }

        template <size_t LEFT_PARAM_COUNT>
        struct log_param_test_level1 {
            template <typename PARAM_TUPLE>
            void operator()(const PARAM_TUPLE& param_tuple)
            {
                auto seq = std::make_index_sequence<std::tuple_size<decltype(data_tuple)>::value>();
                log_param_test_level1_unpack<LEFT_PARAM_COUNT>(param_tuple, seq);
            }
        };

        static bq::string log_str_standard;
        static size_t fmt_idx = 0;
        template <>
        struct log_param_test_level1<0> {
            template <typename First>
            void generate_log_str_standard_utf8(First& first)
            {
                log_str_standard += " ";
                log_str_standard += test_format_helper::trans(first);
                log_str_standard += ",";
                log_str_standard += fmt_string_tail_utf8;
            }

            template <typename First, typename... Args>
            void generate_log_str_standard_utf8(First& first, Args&... args)
            {
                log_str_standard += " ";
                log_str_standard += test_format_helper::trans(first);
                log_str_standard += ",";
                generate_log_str_standard_utf8(args...);
            }

            template <typename First>
            void generate_log_str_standard_utf16(First& first)
            {
                log_str_standard += "(#`O′)测试";
                log_str_standard += test_format_helper::trans(first);
                log_str_standard += ",";
                log_str_standard += fmt_string_tail_utf8;
            }

            template <typename First, typename... Args>
            void generate_log_str_standard_utf16(First& first, Args&... args)
            {
                log_str_standard += "(#`O′)测试";
                log_str_standard += test_format_helper::trans(first);
                log_str_standard += ",";
                generate_log_str_standard_utf16(args...);
            }

            template <typename PARAM_TUPLE, size_t... INDICES>
            void final_test(const PARAM_TUPLE& param_tuple, const std::index_sequence<INDICES...>& seq)
            {
                if (test_3_phase == test_log_3_phase::calculate_count) {
                    ++total_test_num;
                    return;
                }
                (void)seq;
                {
                    // utf8 format test
                }
                fmt_idx = (fmt_idx + 1) % fmt_text_count;
                if (fmt_idx % 2 == 0) {
                    log_str_standard = log_str_templates_standard_utf8[fmt_idx];
                    generate_log_str_standard_utf8(std::get<INDICES>(param_tuple)...);
                    log_inst_ptr->error(log_inst_ptr->cat.ModuleA.SystemA.ClassA, log_str_templates_fmt_utf8[fmt_idx], std::get<INDICES>(param_tuple)...);
                } else {
                    log_str_standard = test_format_helper::trans(log_str_templates_standard_utf16[fmt_idx]);
                    generate_log_str_standard_utf16(std::get<INDICES>(param_tuple)...);
                    log_inst_ptr->error(log_inst_ptr->cat.ModuleA.SystemA.ClassA, log_str_templates_fmt_utf16[fmt_idx], std::get<INDICES>(param_tuple)...);
                }
                *output_str_ptr = output_str_ptr->substr(log_head.size(), output_str_ptr->size() - log_head.size());
                test_log_3_all_console_outputs.push_back(*output_str_ptr);
                result_ptr->add_result(output_str_ptr->end_with(log_str_standard), "test idx:%" PRIu64 ", %s \n != %s", static_cast<uint64_t>(current_tested_num), output_str_ptr->c_str(), log_str_standard.c_str());
                snapshot_test_str += *output_str_ptr;
                snapshot_test_str += "\n";

                current_tested_num++;
                if (snapshot_test_str.size() > (snapshot_buffer_size << 1)) {
                    size_t erase_count = snapshot_test_str.size() - (snapshot_buffer_size << 1);
                    snapshot_test_str.erase(snapshot_test_str.begin(), erase_count);
                }

                if ((current_tested_num % snapshot_idx_mode == 0) || true) {
                    bq::string snapshot = log_inst_ptr->take_snapshot("localtime");
                    if (!snapshot.is_empty()) {
                        result_ptr->add_result(snapshot.size() >= output_str_ptr->size(), "snapshot size test failed, index:%" PRIu64 ", \nstandard: %s\nstandard size:%" PRIu64 ", snapshot size:%" PRIu64 "\nsnapshot: %s", static_cast<uint64_t>(current_tested_num), output_str_ptr->c_str(), static_cast<uint64_t>(output_str_ptr->size()), static_cast<uint64_t>(snapshot.size()), snapshot.c_str());
                        if (snapshot.size() >= output_str_ptr->size()) {
                            bq::string last_snapshot = snapshot.substr(snapshot.size() - output_str_ptr->size(), output_str_ptr->size());
                            result_ptr->add_result(snapshot.end_with(snapshot_test_str) || snapshot_test_str.end_with(snapshot), "snapshot test failed, index:%" PRIu64 ", \nstandard: %s\nstandard size : %" PRIu64 ", snapshot size : %" PRIu64 " \nsnapshot : %s", static_cast<uint64_t>(current_tested_num), output_str_ptr->c_str(), static_cast<uint64_t>(output_str_ptr->size()), static_cast<uint64_t>(snapshot.size()), last_snapshot.c_str());
                        }
                    }
                    snapshot_idx_mode = (snapshot_idx_mode % 1024) + 1;
                }
                size_t new_percent = (size_t)(current_tested_num * 100 / total_test_num);
                if (new_percent != current_tested_percent) {
                    current_tested_percent = new_percent;
                    test_output_dynamic_param(bq::log_level::info, "full log test percent %d/100              \r", (int32_t)current_tested_percent);
                }
            }

            template <typename PARAM_TUPLE>
            void operator()(const PARAM_TUPLE& param_tuple)
            {
                auto seq = std::make_index_sequence<std::tuple_size<PARAM_TUPLE>::value>();
                final_test(param_tuple, seq);
            }
        };

        template <size_t PARAM_COUNT>
        void log_param_test()
        {
            std::tuple<> empty_tuple = std::make_tuple();
            log_param_test_level1<PARAM_COUNT>()(empty_tuple);
        }

        static void invalid_utf16_test(test_result& result, const test_category_log& log_inst)
        {
            bq::u16string invalid_utf16_str = u"#";
            for (size_t i = 0; i < 17 * 1024; ++i) {
                invalid_utf16_str.push_back(u'1');
            }
            invalid_utf16_str[1] = u'\0';
            test_output(bq::log_level::info, "invalid_utf16_test1                           \n");
            bq::log::force_flush_all_logs();
            test_output(bq::log_level::info, "invalid_utf16_test2                           \n");
            log_inst.error(log_inst.cat.ModuleA.SystemA.ClassA, invalid_utf16_str);
            test_output(bq::log_level::info, "invalid_utf16_test3                           \n");
            bq::log::force_flush_all_logs();
            test_output(bq::log_level::info, "invalid_utf16_test4                           \n");
            log_inst.error(log_inst.cat.ModuleA.SystemA.ClassA, "{}", invalid_utf16_str);
            test_output(bq::log_level::info, "invalid_utf16_test5                           \n");
            bq::log::force_flush_all_logs();
            test_output(bq::log_level::info, "invalid_utf16_test6                           \n");
            const bq::string raw_item1 = decode_raw_item();
            test_output(bq::log_level::info, "invalid_utf16_test7                           \n");
            const bq::string compressed_item1 = decode_compressed_item();
            test_output(bq::log_level::info, "invalid_utf16_test8                           \n");
            const bq::string raw_item2 = decode_raw_item();
            test_output(bq::log_level::info, "invalid_utf16_test9                           \n");
            const bq::string compressed_item2 = decode_compressed_item();
            test_output(bq::log_level::info, "invalid_utf16_test10                           \n");
            result.add_result(raw_item1.end_with("#"), "invalid utf16 raw test:%s", raw_item1.c_str());
            result.add_result(compressed_item1.end_with("#"), "invalid utf16 compressed test:%s", compressed_item1.c_str());
            result.add_result(raw_item2.end_with("#"), "invalid utf16 raw test:%s", raw_item2.c_str());
        }

        static bq::platform::mutex encript_config_mutex_;
        static bq::string encript_config_;
        
        static bq::string get_encript_config() {
            bq::platform::scoped_mutex lock(encript_config_mutex_);
            return encript_config_;
        }
        static void set_encript_config(const bq::string& config)
        {
            bq::platform::scoped_mutex lock(encript_config_mutex_);
            encript_config_ = config;
        }

        static void create_test_log_3_file_appender(bq::string extra_config_ = "")
        {
            test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=localtime
						appenders_config.ConsoleAppender.levels=[all]
						
						appenders_config.CompressedAppender.type=compressed_file
						appenders_config.CompressedAppender.time_zone=localtime
						appenders_config.CompressedAppender.levels=[error,fatal]
						appenders_config.CompressedAppender.file_name=bqLog/UnitTestLog/test3
						appenders_config.CompressedAppender.base_dir_type=0
						appenders_config.CompressedAppender.max_file_size=100000000
						appenders_config.CompressedAppender.expire_time_days=2
						
						appenders_config.RawAppender.type=raw_file
						appenders_config.RawAppender.time_zone=localtime
						appenders_config.RawAppender.levels=[error,fatal]
						appenders_config.RawAppender.file_name=bqLog/UnitTestLog/test3
						appenders_config.RawAppender.base_dir_type=0
						appenders_config.RawAppender.max_file_size=100000000
						appenders_config.RawAppender.expire_time_days=2
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA,ModuleB]
			        )"
                + extra_config_);
        }

        class snapshot_thread : public bq::platform::thread {
        private:
            const bq::log* log_ptr;

        public:
            snapshot_thread(const bq::log& log)
            {
                log_ptr = &log;
            }

        protected:
            virtual void run()
            {
                bq::array<bq::string> snapshot_config = { "",
                    "snapshot.buffer_size=65536",
                    "snapshot.buffer_size=120000",
                    "snapshot.buffer_size=200000" };
                auto begin_epoch_ms = bq::platform::high_performance_epoch_ms();
                while (!is_cancelled()) {
                    bq::test::create_test_log_3_file_appender(snapshot_config[(int32_t)(begin_epoch_ms % 4)]
                        + "\n" 
                        + get_encript_config());
                    log_ptr->take_snapshot("localtime");
                    //sleep(begin_epoch_ms % 10);
                }
            }
        };

        static void full_log_test()
        {
            total_test_num = 0;
            current_tested_num = 0;
            current_tested_percent = 0;
            test_log_3_all_console_outputs.clear();
            test_3_phase = test_log_3_phase::calculate_count;
            log_param_test<MAX_PARAM>();
            test_3_phase = test_log_3_phase::do_test;
            log_param_test<MAX_PARAM>();
            bq::log::force_flush_all_logs();

            // decode test
            for (size_t i = 0; i < test_log_3_all_console_outputs.size(); ++i) {
                const bq::string& raw_item = decode_raw_item();
                result_ptr->add_result(test_log_3_all_console_outputs[i] == (raw_item), "encrypted test idx:%" PRIu64 ", raw test, \ndecoded: %s, \nconsole: %s", static_cast<uint64_t>(i), raw_item.c_str(), test_log_3_all_console_outputs[i].c_str());
                const bq::string& compressed_item = decode_compressed_item();
                result_ptr->add_result(test_log_3_all_console_outputs[i] == (compressed_item), "encrypted idx:%" PRIu64 ", compressed test, \ndecoded: %s, \nconsole: %s", static_cast<uint64_t>(i), compressed_item.c_str(), test_log_3_all_console_outputs[i].c_str());
            }
        }


        void test_log::test_3(test_result& result, const test_category_log& log_inst)
        {
            snapshot_thread snapeshot1(log_inst);
            snapeshot1.start();
            snapshot_thread snapeshot2(log_inst);
            snapeshot2.start();

            create_test_log_3_file_appender("snapshot.buffer_size=65536");

            test_output(bq::log_level::info, "full log test begin, this will take minutes, and need about 50M free disk space.\n");
            clear_test_output_folder();
            init_fmt_strings<MAX_PARAM>();
            log_head = "[" + log_inst.get_name() + "]\t";
            result_ptr = &result;
            log_inst_ptr = &log_inst;
            output_str_ptr = &test_log::log_str;
            invalid_utf16_test(result, log_inst);


            test_output(bq::log_level::info, "plaintext log test begin                           \n");
            full_log_test();
            test_output(bq::log_level::info, "plaintext log test end                           \n");


            pub_key = bq::string("ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+FonyOHuS2uC6IZc16bfd6qQk4ykBOt3nTfBFc")
                + "Nr8ZWvvcf4H0hFkrpMtQ0AJO057GhVTQCCfnvfStSq2Yra+O5VGpI5Q6NLrUuVERimjNgwtxbXt3P8Nw87jEIJiY/8m2FUXhZE"
                + "PwoA7t+2/953cNE1itJskJtojwaUlMN0dXBJxs4NP8MfBPPZQ5vNV8xgEf1SCQzQBAJsofy1kPHHqJNBXUBsNA44SP5H95JOz+"
                + "r0oaNkYxT88Zk4tbk5N3hk5aXyZVp49OqhrXCPf5owDa4Lqk4UzVTk9EimxvtSuiUTzr7IJhHYy7jsGnSgq6dH0xlUfxKeX pippocao@PIPPOCAO-PC6";
            priv_key = bq::string("-----BEGIN RSA PRIVATE KEY-----\n")
                + "MIIEpAIBAAKCAQEAsL90LQ1wf30DfhaJ8jh7ktrguiGXNem33eqkJOMpATrd503w\n"
                + "RXDa/GVr73H+B9IRZK6TLUNACTtOexoVU0Agn5730rUqtmK2vjuVRqSOUOjS61Ll\n"
                + "REYpozYMLcW17dz/DcPO4xCCYmP/JthVF4WRD8KAO7ftv/ed3DRNYrSbJCbaI8Gl\n"
                + "JTDdHVwScbODT/DHwTz2UObzVfMYBH9UgkM0AQCbKH8tZDxx6iTQV1AbDQOOEj+R\n"
                + "/eSTs/q9KGjZGMU/PGZOLW5OTd4ZOWl8mVaePTqoa1wj3+aMA2uC6pOFM1U5PRIp\n"
                + "sb7UrolE86+yCYR2Mu47Bp0oKunR9MZVH8SnlwIDAQABAoIBACtaWpmuYTi0JkYo\n"
                + "Kx/hoNXtoA+nq5pKwJHLOwXdPjKSCNnycQvnWZ9tFSN/V2r9qMyEUY9ZnnxlMqPZ\n"
                + "Sv/Hi/j7Ghhx3Y8s+VwB62SPemT4JrwX8ipj91SULjqP80br3Re4PqfNZd3SX0Rc\n"
                + "7co+Nc2izKdZPxTGHM9leNHMMP2VrVbZeSlQBnqlFVMqi2g9ukMGZG10vPdIJV7z\n"
                + "5dqWaKuW/2F8dp/o6i/uUDWAH4fITLD1PLqx5/kP8ohOXup8wxYaY8jhKlvswuCh\n"
                + "qlY773SamnrIGctQWe63Fe9q7hzs3vcCOfciFYsVX2qfHPvGORzd8DgMwe3TFbrM\n"
                + "nmUijcECgYEA5zyarIPQfbEsmMtKgY7OSz+M1SaOE9r/I0Hl2dz7f0r7JocZ6KUj\n"
                + "NinbX/zvJ7AMzDUefbptjf9F4vNa0eBZmSZDAiWm3byvMKS5uLboFNrKnYptIc3c\n"
                + "0CzDzC+nMi4NrPoAZZrUyJw1Emr7gWVVG2FW2NatOVqfPq3XBbk9dncCgYEAw60H\n"
                + "FqcvrSTAVSk9L+TB8Fn92p5YQtaV7CSZj9GQdvfs+pIkPlq+jYvSH7nYjK2Qlq+h\n"
                + "sn+3YcVVczbGuhSLuq4bHPd46HOjya3rbAq39RxlpFRFjZ4hici4XIFKnB7Kylta\n"
                + "Ph6nNq9m5tdFZ1SurgLlOaxg2AwLAia2V5L5/+ECgYAzmhmmP/Ap7HzYSB2DVfwB\n"
                + "XNgvxN/V3HwtQQprGN5i5LexPFryyM9XyfVzsT0pbScd9wir5AuIsZvF7qqoxVkZ\n"
                + "TSmM9BwNxYqO32O2rdKSvNSUXYzHC2qoZiT3jvbPwuk4Xb3y7p9neTx6tLcVhCh+\n"
                + "6LT5xMZ5UxjQYvjmBRWLNwKBgQC5Ta9rzXHB5w7Y1w/hviHHKoHTOabdzPb3RQXD\n"
                + "g7LqZwkdla4K+sZ/pwybDNU9C9TkTnizYG1agpTUYeg6KeDVLbHxcY4nm/Nct349\n"
                + "t7zTu0uqHkArx7d9Ev88Yxgz1pk2nuJL951klSC+tNg97Zzqn0VSo6KmlmkKZXzC\n"
                + "XCayIQKBgQC+hPlGE7T6WLKvsmaH3IJI/TAjbuSu25o+aar7ecPoeax6YnSgyF/8\n"
                + "Xx7f0BFCYEzprftfqDBcfa6D8GKkcsqAMDeJRz8meD+55o3qdL6LKFkjCKvmRxuv\n"
                + "OmO+42HqCD4mqxMU1rgcWOn+LLW3HSqbE5kYA9XDwEtxCnMiKqP7sA==\n"
                + "-----END RSA PRIVATE KEY-----\n";
            set_encript_config("appenders_config.CompressedAppender.pub_key="
                + pub_key + "\n"
                + "appenders_config.RawAppender.pub_key="
                + pub_key + "\n");
            create_test_log_3_file_appender("snapshot.buffer_size=65536\n" + get_encript_config());


            test_output(bq::log_level::info, "encrypted log test begin\n");
            full_log_test();
            test_output(bq::log_level::info, "encrypted log test end                      \n");

            while (!snapeshot1.cancel()) {
                bq::platform::thread::yield();
            }
            while (!snapeshot2.cancel()) {
                bq::platform::thread::yield();
            }
            snapeshot1.join();
            snapeshot2.join();
            test_output(bq::log_level::info, "full log test finished              \n");
        }
    }
}
#else
#include "test_log.h"
#include <string_view>
namespace bq {
    namespace test {
        void test_log::test_3(test_result& result, const test_category_log& log_inst)
        {
            test_output(bq::log_level::info, "full log test only works for c++ 14 or later version.\n");
            (void)result;
            (void)log_inst;
        }
    }
}
#endif
