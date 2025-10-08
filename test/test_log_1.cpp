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
#include <limits.h>
#include "test_log.h"
#include "bq_log/log/appender/appender_file_compressed.h"
#include "bq_log/utils/log_utils.h"

namespace bq {
    namespace test {
        bq::string test_log::log_str;
        const char* test_log::log_c_str = nullptr; // friendly to IDE debugger which can not use natvis.

        void test_log::console_callback(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
        {
            (void)log_id;
            (void)category_idx;
            (void)log_level;
            (void)length;
            log_str = content;
            log_c_str = log_str.c_str();
        }

        void test_log::test_1(test_result& result, const test_category_log& log_inst)
        {
            result.add_result(log_inst.get_name() == "test_log", "log name test");

            {
                // test zigzag
                for (int64_t i = INT64_MIN; i < INT64_MAX - 0x123456789AB; i += 0x123456789AB) {
                    uint64_t i_encode = bq::log_utils::zigzag::encode(i);
                    int64_t i_decode = bq::log_utils::zigzag::decode(i_encode);
                    result.add_result(i == i_decode, "zigzag test %d", i);
                }

                // test vlq
                uint8_t target_data[16];
                {
                    {
                        uint64_t i = 0xFFFFFFFFFFFFFFFF;
                        auto len = bq::log_utils::vlq::vlq_encode(i, target_data, 16);
                        decltype(i) i_decode;
                        auto len_decode = bq::log_utils::vlq::vlq_decode(i_decode, target_data);
                        result.add_result(i == i_decode && len == len_decode, "vlq test %d", i);
                    }
                    {
                        uint64_t i = 0xFF326943FFFFFF;
                        auto len = bq::log_utils::vlq::vlq_encode(i, target_data, 16);
                        decltype(i) i_decode;
                        auto len_decode = bq::log_utils::vlq::vlq_decode(i_decode, target_data);
                        result.add_result(i == i_decode && len == len_decode, "vlq test %d", i);
                    }
                    {
                        uint64_t i = 0x326943FFFFFF;
                        auto len = bq::log_utils::vlq::vlq_encode(i, target_data, 16);
                        decltype(i) i_decode;
                        auto len_decode = bq::log_utils::vlq::vlq_decode(i_decode, target_data);
                        result.add_result(i == i_decode && len == len_decode, "vlq test %d", i);
                    }
                    {
                        uint64_t i = 0x6943FFFFFF;
                        auto len = bq::log_utils::vlq::vlq_encode(i, target_data, 16);
                        decltype(i) i_decode;
                        auto len_decode = bq::log_utils::vlq::vlq_decode(i_decode, target_data);
                        result.add_result(i == i_decode && len == len_decode, "vlq test %d", i);
                    }
                }
                for (uint64_t add = 0; add <= UINT8_MAX; ++add) {
                    uint8_t i = (uint8_t)add;
                    auto len = bq::log_utils::vlq::vlq_encode(i, target_data, 16);
                    decltype(i) i_decode;
                    auto len_decode = bq::log_utils::vlq::vlq_decode(i_decode, target_data);
                    result.add_result(i == i_decode && len == len_decode, "vlq test %d", i);
                }
                for (uint64_t add = 0; add <= UINT16_MAX; ++add) {
                    uint16_t i = (uint16_t)add;
                    auto len = bq::log_utils::vlq::vlq_encode(i, target_data, 16);
                    decltype(i) i_decode;
                    auto len_decode = bq::log_utils::vlq::vlq_decode(i_decode, target_data);
                    result.add_result(i == i_decode && len == len_decode, "vlq test %d", i);
                }
                for (uint64_t add = 0; add <= UINT32_MAX; add += 77) {
                    uint32_t i = (uint32_t)add;
                    auto len = bq::log_utils::vlq::vlq_encode(i, target_data, 16);
                    decltype(i) i_decode;
                    auto len_decode = bq::log_utils::vlq::vlq_decode(i_decode, target_data);
                    result.add_result(i == i_decode && len == len_decode, "vlq test %d", i);
                }

                // test vlq + zigzag
                {
                    int32_t i = -1;
                    uint32_t ui = bq::log_utils::zigzag::encode(i);
                    auto len = bq::log_utils::vlq::vlq_encode(ui, target_data, 16);
                    result.add_result(len == 1, "zigzag + vlq test 1");
                    decltype(ui) ui_decode;
                    auto len_decode = bq::log_utils::vlq::vlq_decode(ui_decode, target_data);
                    result.add_result(len == len_decode, "zigzag + vlq test 2");
                    decltype(i) i_decoded = bq::log_utils::zigzag::decode(ui_decode);
                    result.add_result(i == i_decoded, "zigzag + vlq test 3");
                }
            }

            const char* str_type = "utf8_str";
            bq::log::register_console_callback(&test_log::console_callback);

            {
                // test appender hash calculator
                constexpr size_t cache_size = 1024;
                uint8_t cache[cache_size];

                for (size_t size = 0; size < 512; ++size) {
                    for (size_t offset_outter = 0; offset_outter < 128; ++offset_outter) {
                        uint64_t hash_value = 0;
                        // init data;
                        for (size_t i = 0; i < size; ++i) {
                            cache[offset_outter + i] = (uint8_t)(bq::util::rand() & 0xFF);
                        }
                        bool is_proper_aligned = bq::appender_file_compressed::is_addr_8_aligned_test(cache + offset_outter);
                        hash_value = bq::appender_file_compressed::calculate_data_hash_test(is_proper_aligned, cache + offset_outter, size);
                        for (size_t offset_inner = 1; offset_inner < 128; ++offset_inner) {
                            memmove(cache + offset_outter + offset_inner, cache + offset_outter + offset_inner - 1, size);
                            uint64_t new_hash_value = bq::appender_file_compressed::calculate_data_hash_test(false, cache + offset_outter + offset_inner, size);
                            result.add_result(hash_value == new_hash_value, "appender_file_compressed::calculate_data_hash_test, size:%" PRIu64 ", offset_outter: %" PRIu64 ", offset_inner : %" PRIu64, static_cast<uint64_t>(size), static_cast<uint64_t>(offset_outter), static_cast<uint64_t>(offset_inner));
                        }
                    }
                }
            }

            {
                bq::string empty_str;
                bq::string full_str = "123";
                log_inst.fatal(log_inst.cat.ModuleA.SystemA, "测试一下:{}", "的爱丽丝打开房间阿里山的开发机阿里山的开发");
                log_inst.fatal(log_inst.cat.ModuleA.SystemA, "Empty Str Test {}, {}", empty_str, full_str);
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tEmpty Str Test , 123"), "empty str test");
            }

            {
                bq::string ip = "9.134.131.77";
                uint16_t port = 18900;
                log_inst.verbose(log_inst.cat.ModuleA.SystemA, "connect {}:{}", ip, port);
                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\tconnect 9.134.131.77:18900"), "ip port test");
            }

            {
                bq::string empty_str;
                bq::string full_str = "123";
                log_inst.fatal(log_inst.cat.ModuleA.SystemA, "Empty Str Test {}, {}", empty_str.c_str(), full_str.c_str());
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tEmpty Str Test , 123"), "empty str test");
            }

            {
                int32_t* pointer = NULL;
                log_inst.fatal(log_inst.cat.ModuleA.SystemA, "NULL Pointer Str Test {}, {}", pointer, pointer);
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tNULL Pointer Str Test null, null"), "empty str test");
            }

#define TEST_STR(STR) UTF8_STR(STR)
#define TEST_CHAR(CHAR) UTF8_CHAR(CHAR)
            {
                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR("This is a Test Log"));

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log"), "%s, Log Test 1", str_type);

                int32_t* ptr = NULL;

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR("This is a Test Log, Ptr:{0}"), nullptr, ptr);
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, Ptr:null"), "%s, Log Ptr Test 1, Ptr:nullptr", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR("This is a Test Log, Ptr:{0},{1}, {3}"), nullptr, ptr);
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, Ptr:null,null, {3}"), "%s, Log Ptr Test 1, Ptr:nullptr", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR("This is a Test Log, Ptr:{0},{1}"), nullptr, ptr);

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, Ptr:null,null"), "%s, Log Ptr Test 1, Ptr:nullptr", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR("This is a Test Log, param {0}, param {2}"), false, true);

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, param FALSE, param TRUE"), "%s, Log Test 2", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR("This is a Test Log, param {0}, param {2}"), false);

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, param FALSE, param {2}"), "%s, Log Test 3", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR("This is a Test Log, param {CCCAA{0}}, param {2}"), false);

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, param {CCCAAFALSE}, param {2}"), "%s, Log Test 4", str_type);

                log_inst.verbose(log_inst.cat.ModuleA.SystemA, TEST_STR("This is a Test Log, param {0}, param {2}, param \'{3}\', string param:{4}"), 3.5f, 4542232, TEST_CHAR('a'), TEST_STR("real_value"));

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\tThis is a Test Log, param 3.5000000, param 4542232, param \'a\', string param:real_value"), "%s, Log Test 5", str_type);

                log_inst.verbose(log_inst.cat.ModuleA.SystemA, TEST_STR("测试, param {0}, param {2}, param \'{3}\', string param:{4}"), 3.5f, 4542232, TEST_CHAR('a'), TEST_STR("字符串"));

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\t测试, param 3.5000000, param 4542232, param \'a\', string param:字符串"), "%s, Log Test 6", str_type);

                log_inst.verbose(log_inst.cat.ModuleA.SystemA, TEST_STR("测试, param {0}, param {2}, string \"{3}\", string param:{4}"), 3.5f, 4542232, UTF8_STR("utf-8字符串"), UTF16_STR(u"utf-16字符串"));

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\t测试, param 3.5000000, param 4542232, string \"utf-8字符串\", string param:utf-16字符串"), "%s, Log Test 7", str_type);
            }

            {
                bq::string string1 = "test_str1";
                bq::string string11 = "test_str11";
                bq::string string111 = "test_str111";
                bq::string string1111 = "test_str1111";

                bq::string fmt_str = "format:\"{0}\"";

                size_t size = 50;

                log_inst.verbose(log_inst.cat.ModuleA.SystemA, fmt_str, string1);

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\tformat:\"test_str1\""), "Log Test bq::string 1");
                log_inst.verbose(log_inst.cat.ModuleA.SystemA, fmt_str, string11);

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\tformat:\"test_str11\""), "Log Test bq::string 2");
                log_inst.verbose(log_inst.cat.ModuleA.SystemA, fmt_str, string111);

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\tformat:\"test_str111\""), "Log Test bq::string 3");
                log_inst.verbose(log_inst.cat.ModuleA.SystemA, fmt_str, string1111);

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\tformat:\"test_str1111\""), "Log Test bq::string 4");

                log_inst.verbose(log_inst.cat.ModuleA.SystemA, fmt_str, size);

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\tformat:\"50\""), "Log Test bq::string 1");
            }

#undef TEST_STR
#define TEST_STR(STR) UTF16_STR(STR)
#undef TEST_CHAR
#define TEST_CHAR(CHAR) UTF16_CHAR(CHAR)
            str_type = "utf16_str";
            {
                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR(u"This is a Test Log"));

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log"), "%s, Log Test 1", str_type);

                int32_t* ptr = NULL;

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR(u"This is a Test Log, Ptr:{0}"), nullptr, ptr);
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, Ptr:null"), "%s, Log Ptr Test 1, Ptr:nullptr", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR(u"This is a Test Log, Ptr:{0},{1}, {3}"), nullptr, ptr);
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, Ptr:null,null, {3}"), "%s, Log Ptr Test 1, Ptr:nullptr", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR(u"This is a Test Log, param {0}, param {2}"), false, true);

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, param FALSE, param TRUE"), "%s, Log Test 2", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR(u"This is a Test Log, param {0}, param {2}"), false);

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, param FALSE, param {2}"), "%s, Log Test 3", str_type);

                log_inst.fatal(log_inst.cat.ModuleA.SystemA, TEST_STR(u"This is a Test Log, param {CCCAA{0}}, param {2}"), false);

                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA]\tThis is a Test Log, param {CCCAAFALSE}, param {2}"), "%s, Log Test 4", str_type);

                log_inst.verbose(log_inst.cat.ModuleA.SystemA, TEST_STR(u"This is a Test Log, param {0}, param {2}, param \'{3}\', string param:{4}"), 3.5f, 4542232, TEST_CHAR('a'), "real_value");

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\tThis is a Test Log, param 3.5000000, param 4542232, param \'a\', string param:real_value"), "%s, Log Test 5", str_type);

                log_inst.verbose(log_inst.cat.ModuleA.SystemA, TEST_STR(u"测试, param {0}, param {2}, param \'{3}\', string param:{4}"), 3.5f, 4542232, 'a', TEST_STR(u"字符串"));

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\t测试, param 3.5000000, param 4542232, param \'a\', string param:字符串"), "%s, Log Test 6", str_type);

                log_inst.verbose(log_inst.cat.ModuleA.SystemA, TEST_STR(u"测试, param {0}, param {2}, string \"{3}\", string param:{4}"), 3.5f, 4542232, UTF8_STR("utf-8字符串"), UTF16_STR(u"utf-16字符串"));

                result.add_result(log_str.end_with("[V]\t[ModuleA.SystemA]\t测试, param 3.5000000, param 4542232, string \"utf-8字符串\", string param:utf-16字符串"), "%s, Log Test 7", str_type);
            }
        }
    }
}
