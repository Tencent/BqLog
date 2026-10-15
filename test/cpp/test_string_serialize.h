#pragma once
/*
 * Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "test_base.h"
#if defined(BQ_WIN)
#include <windows.h>
#elif defined(BQ_POSIX)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace bq {
    namespace test {
        // The storage size and the copy of the log string arguments: every start alignment, supplementary
        // characters at the tail, and strings that end right before an inaccessible page.
        class test_string_serialize : public test_base {
        private:
            // The longest string is 40 characters, at most two UTF-16 units each, plus the length and a guard.
            static constexpr size_t out_capacity = 40 * 2 * sizeof(char16_t) + sizeof(uint32_t) + 8;

            template <typename CHAR>
            static std::vector<char16_t> expected_utf16(const CHAR* str, size_t count)
            {
                std::vector<char16_t> units;
                for (size_t i = 0; i < count; ++i) {
                    const uint32_t c = static_cast<uint32_t>(str[i]);
                    if (sizeof(CHAR) == 4 && c > 0xFFFF) {
                        units.push_back(static_cast<char16_t>(((c - 0x10000) >> 10) + 0xD800));
                        units.push_back(static_cast<char16_t>(((c - 0x10000) & 0x3FF) + 0xDC00));
                    } else {
                        units.push_back(static_cast<char16_t>(c));
                    }
                }
                return units;
            }

            template <typename CHAR>
            static void fill(CHAR* str, size_t count, int32_t pattern)
            {
                for (size_t i = 0; i < count; ++i) {
                    uint32_t c;
                    switch (pattern) {
                    case 0:
                        c = 'a' + static_cast<uint32_t>(i % 26);
                        break;
                    case 1:
                        c = 0x4E00 + static_cast<uint32_t>(i % 3000);
                        break;
                    case 2:
                        c = (i % 3 == 2) ? 0x1F600 + static_cast<uint32_t>(i % 64) : 'x';
                        break;
                    default:
                        c = (i + 2 >= count) ? 0x1F601 + static_cast<uint32_t>(i % 2) : 'y';
                        break;
                    }
                    if (sizeof(CHAR) == 1) {
                        c = 'a' + static_cast<uint32_t>((i + static_cast<size_t>(pattern)) % 26);
                    } else if (sizeof(CHAR) == 2 && c > 0xFFFF) {
                        c = 0x4E01;
                    }
                    str[i] = static_cast<CHAR>(c);
                }
            }

            // The c-style string: length from the terminator, then the copy that writes length + utf16 units.
            template <typename CHAR>
            static bool check_c_string(const CHAR* str, size_t count)
            {
                using helper = bq::tools::_serialize_str_helper_by_type<const CHAR*>;
                const std::vector<char16_t> units = expected_utf16(str, count);
                const size_t expected_size = (sizeof(CHAR) == 1 ? count : units.size() * sizeof(char16_t));
                const size_t size = helper::get_storage_data_size(str);
                if (size != expected_size || size + sizeof(uint32_t) >= out_capacity) {
                    return false;
                }
                uint8_t out[out_capacity];
                memset(out, 0xCD, sizeof(out));
                helper::type_copy(str, out, size + sizeof(uint32_t));
                uint32_t written = 0;
                memcpy(&written, out, sizeof(written));
                if (written != size || out[size + sizeof(uint32_t)] != 0xCD) {
                    return false;
                }
                return sizeof(CHAR) == 1 ? memcmp(out + sizeof(uint32_t), str, size) == 0
                                         : memcmp(out + sizeof(uint32_t), units.data(), size) == 0;
            }

            // A sized string: no terminator to stop at, so only `count` characters may be read.
            static bool check_u32_view(const char32_t* str, size_t count)
            {
                using helper = bq::tools::_serialize_str_helper_by_type<bq::u32string>;
                bq::u32string value;
                value.insert_batch(value.end(), str, count);
                const std::vector<char16_t> units = expected_utf16(str, count);
                const size_t size = helper::get_storage_data_size(value);
                if (size != units.size() * sizeof(char16_t) || size + sizeof(uint32_t) >= out_capacity) {
                    return false;
                }
                uint8_t out[out_capacity];
                memset(out, 0xCD, sizeof(out));
                helper::type_copy(value, out, size + sizeof(uint32_t));
                return out[size + sizeof(uint32_t)] == 0xCD && memcmp(out + sizeof(uint32_t), units.data(), size) == 0;
            }

            // Heap copies of exactly the right size, so a sanitizer build reports any read past the end.
            template <typename CHAR>
            static void test_heap(test_result& result, const char* name)
            {
                bool all_pass = true;
                for (int32_t pattern = 0; pattern < 4; ++pattern) {
                    for (size_t offset = 0; offset < 8; ++offset) {
                        for (size_t count = 0; count <= 40; ++count) {
                            CHAR* block = static_cast<CHAR*>(malloc((offset + count + 1) * sizeof(CHAR)));
                            CHAR* str = block + offset;
                            fill(str, count, pattern);
                            str[count] = 0;
                            all_pass &= check_c_string<CHAR>(str, count);
                            free(block);
                        }
                    }
                }
                result.add_result(all_pass, "%s c-style string size and copy", name);
            }

            static void test_heap_u32_view(test_result& result)
            {
                bool all_pass = true;
                for (int32_t pattern = 0; pattern < 4; ++pattern) {
                    for (size_t count = 1; count <= 40; ++count) {
                        char32_t* str = static_cast<char32_t*>(malloc(count * sizeof(char32_t)));
                        fill(str, count, pattern);
                        all_pass &= check_u32_view(str, count);
                        free(str);
                    }
                }
                result.add_result(all_pass, "u32string size and copy");
            }

#if defined(BQ_WIN) || defined(BQ_POSIX)
            struct guarded_page {
                uint8_t* base = nullptr;
                size_t size = 0;
                guarded_page()
                {
#if defined(BQ_WIN)
                    SYSTEM_INFO info;
                    GetSystemInfo(&info);
                    size = static_cast<size_t>(info.dwPageSize);
                    base = static_cast<uint8_t*>(VirtualAlloc(nullptr, size * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
                    DWORD old_protect = 0;
                    if (base && !VirtualProtect(base + size, size, PAGE_NOACCESS, &old_protect)) {
                        VirtualFree(base, 0, MEM_RELEASE);
                        base = nullptr;
                    }
#else
                    size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
                    void* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    base = (mapping == MAP_FAILED) ? nullptr : static_cast<uint8_t*>(mapping);
                    if (base && mprotect(base + size, size, PROT_NONE) != 0) {
                        munmap(base, size * 2);
                        base = nullptr;
                    }
#endif
                }
                ~guarded_page()
                {
                    if (!base) {
                        return;
                    }
#if defined(BQ_WIN)
                    VirtualFree(base, 0, MEM_RELEASE);
#else
                    munmap(base, size * 2);
#endif
                }
            };

            // The terminator, or the last character of a sized string, is the last element before the
            // inaccessible page: a read that strays past the string faults.
            template <typename CHAR>
            static void test_page_end(test_result& result, const char* name)
            {
                guarded_page page;
                if (!page.base) {
                    result.add_result(true, "%s page end: no guard page on this platform", name);
                    return;
                }
                bool all_pass = true;
                for (int32_t pattern = 0; pattern < 4; ++pattern) {
                    for (size_t offset = 0; offset < 8; ++offset) {
                        for (size_t count = 0; count <= 40; ++count) {
                            CHAR* str = reinterpret_cast<CHAR*>(page.base + page.size) - (count + 1) - offset;
                            fill(str, count, pattern);
                            str[count] = 0;
                            all_pass &= check_c_string<CHAR>(str, count);
                        }
                    }
                }
                result.add_result(all_pass, "%s c-style string ending at a guard page", name);
            }

            static void test_page_end_u32_view(test_result& result)
            {
                guarded_page page;
                if (!page.base) {
                    result.add_result(true, "u32 view page end: no guard page on this platform");
                    return;
                }
                bool all_pass = true;
                for (int32_t pattern = 0; pattern < 4; ++pattern) {
                    for (size_t count = 1; count <= 40; ++count) {
                        char32_t* str = reinterpret_cast<char32_t*>(page.base + page.size) - count;
                        fill(str, count, pattern);
                        using helper = bq::tools::_serialize_str_helper_by_type<const char32_t*>;
                        const std::vector<char16_t> units = expected_utf16(str, count);
                        const size_t size = units.size() * sizeof(char16_t);
                        uint8_t out[out_capacity] = {};
                        helper::type_copy(str, out, size + sizeof(uint32_t));
                        all_pass &= memcmp(out + sizeof(uint32_t), units.data(), size) == 0;
                    }
                }
                result.add_result(all_pass, "utf32 copy of a sized string ending at a guard page");
            }
#endif

        public:
            virtual test_result test() override
            {
                test_result result;
                test_heap<char>(result, "utf8");
                test_heap<char16_t>(result, "utf16");
                test_heap<char32_t>(result, "utf32");
                test_heap<wchar_t>(result, "wchar_t");
                test_heap_u32_view(result);
#if defined(BQ_WIN) || defined(BQ_POSIX)
                test_page_end<char>(result, "utf8");
                test_page_end<char16_t>(result, "utf16");
                test_page_end<char32_t>(result, "utf32");
                test_page_end<wchar_t>(result, "wchar_t");
                test_page_end_u32_view(result);
#endif
                return result;
            }
        };
    }
}
