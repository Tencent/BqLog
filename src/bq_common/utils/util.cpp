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
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "bq_common/bq_common.h"
#if BQ_ANDROID
#include <android/log.h>
#endif
#include <time.h>

namespace bq {
    static uint32_t rand_seed = 0;

    static bq::array<char>& get_device_console_buffer()
    {
        static bq::array<char> device_console_buffer_({ '\0' });
        return device_console_buffer_;
    }

    void util::bq_assert(bool cond, bq::string msg)
    {
        if (!cond)
            bq_record(msg);
        assert(cond && msg.c_str());
    }

    void util::bq_record(bq::string msg, string file_name)
    {
        static bq::platform::mutex mutex_;
        bq::platform::scoped_mutex lock(mutex_);
        string path = TO_ABSOLUTE_PATH(file_name, true);
        bq::file_manager::instance().append_all_text(path, msg);
    }

    bq::string util::format(const char* fmt, ...)
    {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        return buffer;
    }

    bq::string util::get_current_gmt_time_string()
    {
        char error_text[256] = { 0 };
        auto epoch = bq::platform::high_performance_epoch_ms();
        struct tm result;
        get_gmt_time_by_epoch(epoch, result);
        snprintf(error_text, sizeof(error_text), "%04d-%02d-%02d %02d:%02d:%02d",
            result.tm_year + 1900, result.tm_mon + 1, result.tm_mday, result.tm_hour, result.tm_min, result.tm_sec);
        return error_text;
    }

    bq::string util::get_current_local_time_string()
    {
        char error_text[256] = { 0 };
        auto epoch = bq::platform::high_performance_epoch_ms();
        struct tm result;
        get_local_time_by_epoch(epoch, result);
        snprintf(error_text, sizeof(error_text), "%04d-%02d-%02d %02d:%02d:%02d",
            result.tm_year + 1900, result.tm_mon + 1, result.tm_mday, result.tm_hour, result.tm_min, result.tm_sec);
        return error_text;
    }

    bool util::get_local_time_by_epoch(uint64_t epoch, struct tm& result)
    {
        time_t epoch_sec = (time_t)(epoch / 1000);
#if defined(_MSC_VER)
        return localtime_s(&result, &epoch_sec) == 0;
#else
        return localtime_r(&epoch_sec, &result) != NULL;
#endif
    }

    bool util::get_gmt_time_by_epoch(uint64_t epoch, struct tm& result)
    {
        time_t epoch_sec = (time_t)(epoch / 1000);
#if defined(_MSC_VER)
        return gmtime_s(&result, &epoch_sec) == 0;
#else
        return gmtime_r(&epoch_sec, &result) != NULL;
#endif
    }

#if BQ_TOOLS || BQ_UNIT_TEST
    static bq::log_level log_device_min_level = bq::log_level::error;
    void util::set_log_device_console_min_level(bq::log_level level)
    {
        log_device_min_level = level;
    }
#endif

    void util::log_device_console(bq::log_level level, const char* format, ...)
    {
#if BQ_TOOLS || BQ_UNIT_TEST
        if (level < log_device_min_level) {
            return;
        }
#endif
        auto& device_console_buffer = get_device_console_buffer();
        static bq::platform::mutex mutex_;
        bq::platform::scoped_mutex lock(mutex_);
        va_list args;
        va_start(args, format);
        if (format) {
            while (true) {
                va_start(args, format);
                bool failed = ((bq::array<char>::size_type)vsnprintf(&device_console_buffer[0], device_console_buffer.size(), format, args) + 1) >= device_console_buffer.size();
                va_end(args);
                if (failed) {
                    device_console_buffer.fill_uninitialized(device_console_buffer.size());
                } else {
                    break;
                }
            }
        } else {
            device_console_buffer[0] = '\0';
        }
        va_end(args);
        log_device_console_plain_text(level, device_console_buffer.begin().operator->());
    }

    void util::log_device_console_plain_text(bq::log_level level, const char* text)
    {
#if BQ_TOOLS || BQ_UNIT_TEST
        if (level < log_device_min_level) {
            return;
        }
#endif
#if BQ_ANDROID
        __android_log_write(ANDROID_LOG_VERBOSE + ((int32_t)level - (int32_t)bq::log_level::verbose), "Bq", text);
#elif BQ_IOS
        (void)level;
        bq::platform::ios_print(text);
#else
        decltype(stdout) output_target = stdout;
        static bq::platform::mutex mutex;
        bq::platform::scoped_mutex lock(mutex);
        switch (level) {
        case bq::log_level::verbose:
            fputs("\033[3m", output_target);
            break;
        case bq::log_level::debug:
            fputs("\033[92m", output_target);
            break;
        case bq::log_level::info:
            fputs("\033[94m", output_target);
            break;
        case bq::log_level::warning:
            fputs("\033[1;40;93m", output_target);
            break;
        case bq::log_level::error:
            output_target = stderr;
            fputs("\033[1;40;91m", output_target);
            break;
        case bq::log_level::fatal:
            output_target = stderr;
            fputs("\033[1;30;101m", output_target);
            break;
        default:
            fputs("\033[37m", output_target);
            break;
        }
        fputs(text, output_target);
        fputs("\033[0m", output_target);
        fputs("\n", output_target);
        fflush(output_target);
#if _MSC_VER
        OutputDebugStringA(text);
        OutputDebugStringA("\n");
#endif

#endif
    }

    uint32_t util::get_hash(const void* data, size_t size)
    {
        uint32_t hash = 0;
        for (size_t i = 0; i < size; i++) {
            hash *= 16777619;
            hash ^= static_cast<uint32_t>(*((const uint8_t*)data + i));
        }
        return hash;
    }

    uint64_t util::get_hash_64(const void* data, size_t size)
    {
        uint64_t hash = 0;
        for (size_t i = 0; i < size; i++) {
            hash *= 1099511628211ull;
            hash ^= static_cast<uint64_t>(*((const uint8_t*)data + i));
        }
        return hash;
    }

    bool util::is_little_endian()
    {
        int32_t test = 1;
        return *(char*)(&test) == 1;
    }

    void util::srand(uint32_t seek)
    {
        rand_seed = seek;
    }
    uint32_t util::rand()
    {
        if (rand_seed == 0)
            rand_seed = (uint32_t)time(0);
        rand_seed = rand_seed * 1103515245 + 12345;
        uint32_t ret = (uint32_t)(rand_seed / 65536) % 32768;
        return ret;
    }

    uint32_t util::utf16_to_utf8(const char16_t* src_utf16_str, uint32_t src_character_num, char* target_utf8_str, uint32_t target_utf8_character_num)
    {
        uint32_t result_len = 0;
        uint32_t codepoint = 0;
        uint32_t surrogate = 0;

        uint32_t wchar_len = src_character_num;

        const char16_t* s = src_utf16_str;
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
                    target_utf8_str[result_len++] = (char)c;

                    while ((uint32_t)(p - s) < wchar_len && *p && *p < 0x80) {
                        target_utf8_str[result_len++] = (char)*p++;
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
                target_utf8_str[result_len++] = (char)codepoint;
            } else if (codepoint < 0x0800) {
                target_utf8_str[result_len++] = (char)(0xC0 | (codepoint >> 6));
                target_utf8_str[result_len++] = (char)(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                target_utf8_str[result_len++] = (char)(0xE0 | (codepoint >> 12));
                target_utf8_str[result_len++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                target_utf8_str[result_len++] = (char)(0x80 | (codepoint & 0x3F));
            } else {
                target_utf8_str[result_len++] = (char)(0xF0 | (codepoint >> 18));
                target_utf8_str[result_len++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                target_utf8_str[result_len++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                target_utf8_str[result_len++] = (char)(0x80 | (codepoint & 0x3F));
            }
        }
        assert(result_len <= target_utf8_character_num && "target buffer of utf16_to_utf8 is not enough!");
        return result_len;
    }

    uint32_t util::utf8_to_utf16(const char* src_utf8_str, uint32_t src_character_num, char16_t* target_utf16_str, uint32_t target_utf16_character_num)
    {
        uint32_t result_len = 0;
        uint32_t mb_size = 0;
        uint32_t mb_remain = 0;

        char16_t c;
        uint32_t codepoint;

        const char* p = (const char*)src_utf8_str;
        while (((uint32_t)(p - src_utf8_str) < src_character_num) && (c = *p++) != 0) {
            if (mb_size == 0) {
                if (c < 0x80) {
                    target_utf16_str[result_len++] = c;
                } else if ((c & 0xE0) == 0xC0) {
                    codepoint = c & 0x1F;
                    mb_size = 2;
                } else if ((c & 0xF0) == 0xE0) {
                    codepoint = c & 0x0F;
                    mb_size = 3;
                } else if ((c & 0xF8) == 0xF0) {
                    codepoint = c & 7;
                    mb_size = 4;
                } else if ((c & 0xFC) == 0xF8) {
                    codepoint = c & 3;
                    mb_size = 5;
                } else if ((c & 0xFE) == 0xFC) {
                    codepoint = c & 3;
                    mb_size = 6;
                } else {
                    codepoint = mb_remain = mb_size = 0;
                }

                if (mb_size > 1)
                    mb_remain = mb_size - 1;

            } else {
                if ((c & 0xC0) == 0x80) {
                    codepoint = (codepoint << 6) | (c & 0x3F);

                    if (--mb_remain == 0) {
                        if (codepoint < 0x10000) {
                            target_utf16_str[result_len++] = (char16_t)(codepoint % 0x10000);
                        } else if (codepoint < 0x110000) {
                            codepoint -= 0x10000;
                            target_utf16_str[result_len++] = (char16_t)((codepoint >> 10) + 0xD800);
                            target_utf16_str[result_len++] = (char16_t)((codepoint & 0x3FF) + 0xDC00);
                        } else {
                            codepoint = mb_remain = 0;
                        }
                        mb_size = 0;
                    }
                } else {
                    codepoint = mb_remain = mb_size = 0;
                }
            }
        }
        assert(result_len <= target_utf16_character_num && "target buffer of utf8_to_utf16 is not enough!");
        return result_len;
    }
}