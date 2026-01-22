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

#include "test_layout.h"
#include "bq_log/log/layout.h"
#include "bq_common/bq_common.h"
#include <vector>
#include <string>
#include <cstring>
#include <random>

namespace bq {
    namespace test {

        test_result test_layout::test() {
            test_result result;
            
            result = result + test_find_brace_and_copy();
            result = result + test_find_brace_and_convert_u16();
            result = result + test_throughput();

            return result;
        }

        test_result test_layout::test_find_brace_and_copy() {
            test_result result;
            
            struct TestCase {
                std::string input;
                std::string desc;
            };

            std::vector<TestCase> cases = {
                {"", "Empty"},
                {"abc", "Short no brace"},
                {"{", "Short brace start"},
                {"}", "Short brace end"},
                {"a{b", "Short brace middle"},
                {"abcdefghijklmnopqrstuvwxyz", "Medium no brace"},
                {"abcdefghijklmnop{qrstuvwxyz", "Medium brace middle"},
                {"abcdefghijklmnopqrstuvwxyz1234567890", "Long no brace"},
                {std::string(100, 'a'), "Very long no brace"},
                {std::string(100, 'a') + "{" + std::string(100, 'b'), "Very long brace middle"},
                {"{", "Brace at start"},
                {"}", "Brace at end"},
            };

            // Add boundary cases
            std::vector<size_t> lengths = {15, 16, 17, 31, 32, 33, 47, 48, 49, 63, 64, 65};
            for (size_t len : lengths) {
                cases.push_back({std::string(len, 'a'), "Boundary " + std::to_string(len)});
                
                std::string s(len, 'a');
                s[len-1] = '{';
                cases.push_back({s, "Boundary Brace End " + std::to_string(len)});

                s = std::string(len, 'a');
                s[0] = '{';
                cases.push_back({s, "Boundary Brace Start " + std::to_string(len)});

                if (len > 1) {
                    s = std::string(len, 'a');
                    s[len/2] = '{';
                    cases.push_back({s, "Boundary Brace Middle " + std::to_string(len)});
                }
            }

            // Add random cases
            std::mt19937 rng(12345);
            for (int32_t i = 0; i < 1000; ++i) {
                int32_t len = rng() % 512;
                std::string s;
                s.reserve((size_t)len);
                for (int32_t j = 0; j < len; ++j) {
                    char c = (char)('a' + (rng() % 26));
                    if (rng() % 20 == 0) c = (rng() % 2) ? '{' : '}';
                    s += c;
                }
                cases.push_back({s, "Random " + std::to_string(i)});
            }

            // Use heap for buffers to avoid stack overflow with large inputs if we increase max len
            std::vector<char> dst_sw_vec(2048);
            std::vector<char> dst_simd_vec(2048);
            char* dst_sw = dst_sw_vec.data();
            char* dst_simd = dst_simd_vec.data();

            for (const auto& tc : cases) {
                bool brace_sw = false;
                bool brace_simd = false;
                
                memset(dst_sw, 0, dst_sw_vec.size());
                memset(dst_simd, 0, dst_simd_vec.size());

                uint32_t ret_sw = bq::layout::test_find_brace_and_copy_sw(tc.input.c_str(), (uint32_t)tc.input.size(), dst_sw, brace_sw);

#if defined(BQ_X86)
                if (bq::common_global_vars::get().avx2_support_) {
                        uint32_t ret_avx2 = bq::layout::test_find_brace_and_copy_avx2(tc.input.c_str(), (uint32_t)tc.input.size(), dst_simd, brace_simd);
                        bool match = (ret_sw == ret_avx2) && (brace_sw == brace_simd) && (memcmp(dst_sw, dst_simd, ret_sw) == 0);
                        result.add_result(match, "AVX2 vs SW: %s (Len: %zu)", tc.desc.c_str(), tc.input.size());
                        if (!match) {
                            // printf("FAIL: AVX2 Input: '%s'\n", tc.input.c_str());
                        }
                }
                
                // SSE
                memset(dst_simd, 0, dst_simd_vec.size());
                brace_simd = false;
                uint32_t ret_sse = bq::layout::test_find_brace_and_copy_sse(tc.input.c_str(), (uint32_t)tc.input.size(), dst_simd, brace_simd);
                bool match_sse = (ret_sw == ret_sse) && (brace_sw == brace_simd) && (memcmp(dst_sw, dst_simd, ret_sw) == 0);
                result.add_result(match_sse, "SSE vs SW: %s (Len: %zu)", tc.desc.c_str(), tc.input.size());
                if (!match_sse) {
                        // printf("FAIL: SSE Input: '%s'\n", tc.input.c_str());
                }

#elif defined(BQ_ARM_NEON)
                uint32_t ret_neon = bq::layout::test_find_brace_and_copy_neon(tc.input.c_str(), (uint32_t)tc.input.size(), dst_simd, brace_simd);
                bool match_neon = (ret_sw == ret_neon) && (brace_sw == brace_simd) && (memcmp(dst_sw, dst_simd, ret_sw) == 0);
                result.add_result(match_neon, "NEON vs SW: %s (Len: %zu)", tc.desc.c_str(), tc.input.size());
#else
                (void)brace_sw;
                (void)brace_simd;
                (void)ret_sw;
#endif
            }
            return result;
        }

        test_result test_layout::test_find_brace_and_convert_u16() {
            test_result result;
            
            struct TestCase {
                std::u16string input;
                std::string desc;
            };

            std::vector<TestCase> cases = {
                {u"", "Empty"},
                {u"abc", "Short ASCII no brace"},
                {u"{", "Short brace start"},
                {u"}", "Short brace end"},
                {u"a{b", "Short brace middle"},
                {u"abcdefghijklmnopqrstuvwxyz", "Medium ASCII no brace"},
                {u"abcdefghijklmnop{qrstuvwxyz", "Medium brace middle"},
                {u"你好", "Short Non-ASCII"},
                {u"abc你好def", "Mixed Non-ASCII"},
                {u"abc{def", "Mixed Brace ASCII"},
                {u"abc{你好", "Brace then Non-ASCII"}, // Should stop at brace
                {u"abc你好{def", "Non-ASCII then Brace"}, // Should stop at Non-ASCII
                {std::u16string(100, u'a'), "Very long ASCII"},
            };

             // Add boundary cases
            std::vector<size_t> lengths = {7, 8, 9, 15, 16, 17, 31, 32, 33};
            for (size_t len : lengths) {
                cases.push_back({std::u16string(len, u'a'), "Boundary " + std::to_string(len)});
                
                std::u16string s(len, u'a');
                s[len-1] = u'{';
                cases.push_back({s, "Boundary Brace End " + std::to_string(len)});

                s = std::u16string(len, u'a');
                s[0] = u'{';
                cases.push_back({s, "Boundary Brace Start " + std::to_string(len)});
                
                 s = std::u16string(len, u'a');
                 s[len-1] = 0x4E00; // CJK
                 cases.push_back({s, "Boundary Non-ASCII End " + std::to_string(len)});
            }

            // Random cases
            std::mt19937 rng(54321);
            for (int32_t i = 0; i < 1000; ++i) {
                int32_t len = rng() % 256;
                std::u16string s;
                s.reserve((size_t)len);
                for (int32_t j = 0; j < len; ++j) {
                    int32_t r = static_cast<int32_t>(rng() % 100);
                    if (r < 80) s += (char16_t)('a' + (rng() % 26));
                    else if (r < 90) s += (rng() % 2) ? u'{' : u'}';
                    else s += static_cast<char16_t>(0x4E00 + (rng() % 100)); // CJK char
                }
                cases.push_back({s, "Random " + std::to_string(i)});
            }

            std::vector<char> dst_sw_vec(2048);
            std::vector<char> dst_simd_vec(2048);
            char* dst_sw = dst_sw_vec.data();
            char* dst_simd = dst_simd_vec.data();

            for (const auto& tc : cases) {
                bool brace_sw = false;
                bool non_ascii_sw = false;
                bool brace_simd = false;
                bool non_ascii_simd = false;
                
                memset(dst_sw, 0, dst_sw_vec.size());
                memset(dst_simd, 0, dst_simd_vec.size());

                uint32_t ret_sw = bq::layout::test_find_brace_and_convert_u16_sw(tc.input.c_str(), (uint32_t)tc.input.size(), dst_sw, brace_sw, non_ascii_sw);

#if defined(BQ_X86)
                if (bq::common_global_vars::get().avx2_support_) {
                        uint32_t ret_avx2 = bq::layout::test_find_brace_and_convert_u16_avx2(tc.input.c_str(), (uint32_t)tc.input.size(), dst_simd, brace_simd, non_ascii_simd);
                        bool match = (ret_sw == ret_avx2) && (brace_sw == brace_simd) && (non_ascii_sw == non_ascii_simd) && (memcmp(dst_sw, dst_simd, ret_sw) == 0);
                        result.add_result(match, "AVX2 U16 vs SW: %s (Len: %zu)", tc.desc.c_str(), tc.input.size());
                        if (!match) {
                        // printf("FAIL AVX2 U16 Input len %zu\n", tc.input.size());
                        }
                }

                // SSE
                memset(dst_simd, 0, dst_simd_vec.size());
                brace_simd = false;
                non_ascii_simd = false;
                uint32_t ret_sse = bq::layout::test_find_brace_and_convert_u16_sse(tc.input.c_str(), (uint32_t)tc.input.size(), dst_simd, brace_simd, non_ascii_simd);
                bool match_sse = (ret_sw == ret_sse) && (brace_sw == brace_simd) && (non_ascii_sw == non_ascii_simd) && (memcmp(dst_sw, dst_simd, ret_sw) == 0);
                result.add_result(match_sse, "SSE U16 vs SW: %s (Len: %zu)", tc.desc.c_str(), tc.input.size());
                    if (!match_sse) {
                        // printf("FAIL SSE U16 Input len %zu\n", tc.input.size());
                    }

#elif defined(BQ_ARM_NEON)
                uint32_t ret_neon = bq::layout::test_find_brace_and_convert_u16_neon(tc.input.c_str(), (uint32_t)tc.input.size(), dst_simd, brace_simd, non_ascii_simd);
                bool match_neon = (ret_sw == ret_neon) && (brace_sw == brace_simd) && (non_ascii_sw == non_ascii_simd) && (memcmp(dst_sw, dst_simd, ret_sw) == 0);
                result.add_result(match_neon, "NEON U16 vs SW: %s (Len: %zu)", tc.desc.c_str(), tc.input.size());
#else
                (void)brace_sw;
                (void)non_ascii_sw;
                (void)brace_simd;
                (void)non_ascii_simd;
                (void)ret_sw;
#endif
            }
            return result;
        }

        test_result test_layout::test_throughput() {
            test_result result;
            
            // Construct a fake log entry
            // Format: "Benchmark throughput test with some args: int32_t={}, str={}"
            // Args: 12345678, "simple_string"
            std::string fmt = "Benchmark throughput test with some args: int32_t={}, str={}";
            uint32_t fmt_len = (uint32_t)fmt.size(); // No null terminator needed if len is correct
            
            // Layout logic expects format string at data + sizeof(head)
            // Args at data + sizeof(head) + align4(fmt_len)
            
            size_t head_size = sizeof(_log_entry_head_def);
            size_t fmt_padding = bq::align_4(fmt_len) - fmt_len;
            size_t args_offset = head_size + fmt_len + fmt_padding;
            
            // Args construction
            // Arg 1: int32 (12345678)
            // Layout: [Type(1)] [Pad(3)] [Value(4)]
            // Arg 2: string ("simple_string")
            // Layout: [Type(1)] [Pad(3)] [Len(4)] [Data(aligned)]
            std::string arg_str = "simple_string";
            uint32_t arg_str_len = (uint32_t)arg_str.size();
            size_t arg_str_padding = bq::align_4(arg_str_len) - arg_str_len;
            
            size_t arg1_size = 1 + 3 + 4; // 8
            size_t arg2_size = 1 + 3 + 4 + arg_str_len + arg_str_padding;
            
            size_t total_size = args_offset + arg1_size + arg2_size + 100; // Extra buffer
            std::vector<uint8_t> buffer(total_size, 0);
            
            // Fill Head
            _log_entry_head_def* head = (_log_entry_head_def*)buffer.data();
            head->log_format_str_type = (uint8_t)log_arg_type_enum::string_utf8_type;
            head->log_format_data_len = fmt_len;
            
            // Fill Format
            memcpy(buffer.data() + head_size, fmt.c_str(), fmt_len);
            
            // Fill Args
            uint8_t* args_ptr = buffer.data() + args_offset;
            
            // Arg 1
            args_ptr[0] = (uint8_t)log_arg_type_enum::int32_type;
            int32_t val = 12345678;
            memcpy(args_ptr + 4, &val, 4);
            args_ptr += arg1_size;
            
            // Arg 2
            args_ptr[0] = (uint8_t)log_arg_type_enum::string_utf8_type;
            memcpy(args_ptr + 4, &arg_str_len, 4);
            memcpy(args_ptr + 8, arg_str.c_str(), arg_str_len);
            
            log_entry_handle handle(buffer.data(), (uint32_t)buffer.size());
            
            bq::layout l;
            // Warm up
            l.test_python_style_format_content_simd(handle);
            
            const int32_t iterations = 1000000;
            
            // Test Legacy
            uint64_t t1 = bq::platform::high_performance_epoch_ms();
            for(int32_t i=0; i<iterations; ++i) {
                l.test_python_style_format_content_legacy(handle);
            }
            uint64_t t2 = bq::platform::high_performance_epoch_ms();
            
            // Test SW
            uint64_t t3 = bq::platform::high_performance_epoch_ms();
            for(int32_t i=0; i<iterations; ++i) {
                l.test_python_style_format_content_sw(handle);
            }
            uint64_t t4 = bq::platform::high_performance_epoch_ms();
            
            // Test SIMD
            uint64_t t5 = bq::platform::high_performance_epoch_ms();
            for(int32_t i=0; i<iterations; ++i) {
                l.test_python_style_format_content_simd(handle);
            }
            uint64_t t6 = bq::platform::high_performance_epoch_ms();
            
            bq::util::set_log_device_console_min_level(bq::log_level::debug);
            bq::util::log_device_console(bq::log_level::debug, "Layout Throughput (1M ops): Legacy=%" PRIu64 " ms, SW=%" PRIu64 " ms, SIMD=%" PRIu64 " ms", (t2-t1), (t4-t3), (t6-t5));
            bq::util::set_log_device_console_min_level(bq::log_level::warning);

            // Verify outputs match
            l.test_python_style_format_content_legacy(handle);
            std::string res_legacy = l.get_formated_str();
            
            l.test_python_style_format_content_sw(handle);
            std::string res_sw = l.get_formated_str();
            
            l.test_python_style_format_content_simd(handle);
            std::string res_simd = l.get_formated_str();
            
            result.add_result(res_legacy == res_sw, "Legacy vs SW output match");
            result.add_result(res_sw == res_simd, "SW vs SIMD output match");
            
            return result;
        }
    }
}
