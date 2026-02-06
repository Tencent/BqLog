#pragma once
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

#include "bq_common/bq_common_public_include.h"
#include "bq_common/types/basic_types.h"
#include "bq_common/platform/platform_misc.h"

namespace bq {
    class util {
    public:
        /**
         * Ultra-fast copy-and-hash utility optimized for modern CPU pipelines.
         * Approaches hardware limits and can outperform standalone memcpy for small data chunks.
         * Uses 4-way interleaved CRC32C and overlapping block strategy.
         * returns: ((h1 ^ h3) << 32) | (h2 ^ h4)
         */
        static uint64_t bq_memcpy_with_hash(void* BQ_RESTRICT dst, const void* BQ_RESTRICT src, size_t len);

        /**
         * Ultra-fast hash utility optimized for modern CPU pipelines.
         * Approaches hardware limits.
         */
        static uint64_t bq_hash_only(const void* src, size_t len);

        static void bq_assert(bool cond, bq::string msg);
        static void bq_record(bq::string msg, string file_name = "__bq_assert.log");

#if defined(BQ_TOOLS) || defined(BQ_UNIT_TEST)
        static void set_log_device_console_min_level(bq::log_level level);
#endif
        // this function is signal-safety but has limit buffer, the log content exceed buffer size will be truncated
        static void log_device_console(bq::log_level level, const char* format, ...)
#if defined(BQ_GCC) || defined(BQ_CLANG)
            __attribute__((format(printf, 2, 3)))
#endif
            ;

        static void log_device_console_plain_text(bq::log_level level, const char* text);

        typedef void(BQ_STDCALL* type_func_ptr_bq_util_consle_callback)(bq::log_level level, const char* text);
        static void set_console_output_callback(type_func_ptr_bq_util_consle_callback callback);

        // Output to console directly, no console callback will be called
        static void _default_console_output(bq::log_level level, const char* text);

        // Non-inline wrapper for bq_hash_only to reduce binary size in non-critical paths.
        // Returns a 32-bit folded hash.
        static uint32_t get_hash(const void* data, size_t size);

        // Non-inline wrapper for bq_hash_only to reduce binary size in non-critical paths.
        static uint64_t get_hash_64(const void* data, size_t size);

        static bool is_little_endian();

        static void srand(uint32_t seed);

        static uint32_t rand();

        static void srand64(uint64_t seed);

        static uint64_t rand64();

        /// <summary>
        /// `UTF-Mixed` is a custom format in BqLog designed to maximize UTF-16 to UTF-8 conversion performance.
        /// It consists of a UTF-8 prefix followed by a UTF-16 suffix, separated by a 0xFF character
        /// (0xFF is not guaranteed to be present when the whole string is ASCII characters).
        /// </summary>
        /// <param name="src"></param>
        /// <param name="src_character_num"></param>
        /// <param name="dst"></param>
        /// <param name="dst_len"></param>
        /// <returns>Length of final UTF-mixed string in bytes</returns>
        static uint32_t utf16_to_utf_mixed(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num);

        /// <summary>
        /// Decode from `UTF-Mixed` to `UTF8`
        /// Warning: Please ensure that the destination buffer has enough space to hold the converted UTF-8 string.
        /// It's not checked inside this function for performance consideration.
        /// </summary>
        /// <param name="src"></param>
        /// <param name="src_len"></param>
        /// <param name="dst"></param>
        /// <param name="dst_len"></param>
        /// <returns>Length of final UTF-8 string</returns>
        static uint32_t utf_mixed_to_utf8(const char* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num);

        /// <summary>
        /// Computes the hash of a `utf_mixed` string as if it were converted back to UTF-16.
        /// This is useful for recovering the original UTF-16 hash from a compressed `utf_mixed` storage
        /// without fully decoding it to a new string object.
        /// </summary>
        /// <param name="mixed">Pointer to the utf_mixed data</param>
        /// <param name="len">Length of the utf_mixed data in bytes</param>
        /// <returns>The 64-bit hash of the equivalent UTF-16 string</returns>
        static uint64_t hash_utf_mixed_as_utf16(const void* mixed, size_t len);

        /// <summary>
        /// High performance convert utf16 to utf8 (SIMD accelerated)
        /// </summary>
        static uint32_t utf16_to_utf8(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num);

        /// <summary>
        /// High performance convert utf8 to utf16 (SIMD accelerated)
        /// </summary>
        static uint32_t utf8_to_utf16(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num);

        /// <summary>
        /// High performance convert utf16 to utf8 (SIMD accelerated)
        /// </summary>
        static uint32_t utf16_to_utf8_ascii(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num);

        /// <summary>
        /// High performance convert utf8 to utf16 (SIMD accelerated)
        /// </summary>
        static uint32_t utf8_to_utf16_ascii(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num);

#ifdef BQ_UNIT_TEST
        static uint32_t utf16_to_utf8_legacy(const char16_t* BQ_RESTRICT src_utf16_str, uint32_t src_character_num, char* BQ_RESTRICT dst_utf8_str, uint32_t dst_character_num);
        static uint32_t utf8_to_utf16_legacy(const char* BQ_RESTRICT src_utf8_str, uint32_t src_character_num, char16_t* BQ_RESTRICT dst_utf16_str, uint32_t dst_character_num);

        static uint32_t utf16_to_utf8_sw(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num);
        static uint32_t utf8_to_utf16_sw(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num);
        static uint64_t hash_utf_mixed_as_utf16_sw(const void* mixed, size_t len);
#if defined(BQ_X86)
        static uint32_t utf16_to_utf8_sse(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num);
        static uint32_t utf8_to_utf16_sse(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num);
        static uint64_t hash_utf_mixed_as_utf16_sse(const void* mixed, size_t len);
        static uint64_t hash_utf_mixed_as_utf16_avx2(const void* mixed, size_t len);
#elif defined(BQ_ARM_NEON)
        static uint64_t hash_utf_mixed_as_utf16_neon(const void* mixed, size_t len);
#endif
#endif

        /// <summary>
        /// Allocates and constructs an object of type `T` with the specified alignment.
        /// </summary>
        /// <typeparam name="T">The type of the object to allocate and construct.</typeparam>
        /// <typeparam name="V">Variadic template parameters for the constructor arguments of `T`.</typeparam>
        /// <param name="alignment">The alignment requirement for the allocated memory (in bytes).
        ///                        Ignored if `BQ_ALIGNAS_NEW` is defined. Otherwise, it must be a power of 2
        ///                        and should match the `alignas` specifier of `T` (if any).</param>
        /// <param name="args">Arguments forwarded to the constructor of `T`.</param>
        /// <returns>A pointer to the constructed object of type `T`, or `nullptr` if allocation fails.</returns>
        template <typename T, typename... V>
        static T* aligned_new(const size_t alignment, V&&... args)
        {
            void* ptr = bq::platform::aligned_alloc(alignment, sizeof(T));
            if (!ptr) {
                return nullptr;
            }
            new ((void*)ptr, bq::enum_new_dummy::dummy) T(bq::forward<V>(args)...);
            return static_cast<T*>(ptr);
        }
        template <typename T, typename... V>
        static void aligned_delete(T* ptr)
        {
            bq::object_destructor<T>::destruct(static_cast<T*>(ptr));
            bq::platform::aligned_free(ptr);
        }
    };

    template <typename T>
    struct scoped_exist_callback_helper {
    private:
        T callback_;

    public:
        explicit scoped_exist_callback_helper(const T& callback)
            : callback_(callback)
        {
        }
        ~scoped_exist_callback_helper() { callback_(); }
    };
}
