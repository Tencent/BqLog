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

#if defined(BQ_X86)
    #ifdef BQ_MSVC
        #include <intrin.h>
    #else
        #include <immintrin.h>
    #endif
#elif defined(BQ_ARM)
    #if defined(BQ_MSVC)
        #include <arm64_neon.h>
    #else
        #include <arm_acle.h>
    #endif
#endif

// Target attribute for GCC/Clang to enable specific instruction sets for specific functions.
#if (defined(BQ_CLANG) || defined(BQ_GCC))
    #if defined(BQ_ARM)
        #define BQ_HW_CRC_TARGET __attribute__((target("crc")))
    #elif defined(BQ_X86)
        #define BQ_HW_CRC_TARGET __attribute__((target("sse4.2")))
    #else
        #define BQ_HW_CRC_TARGET
    #endif
    #define BQ_CRC_HW_INLINE inline
#else
    #define BQ_HW_CRC_TARGET
    #define BQ_CRC_HW_INLINE bq_forceinline
#endif

namespace bq {
    
    // Defined in util.cpp
    extern const uint32_t _bq_crc32c_table[256];
    extern bool _bq_crc32_supported_;

    class util {
    private:
        // ---------------- SW Fallbacks ----------------
        static bq_forceinline uint32_t _crc32_sw_u8(uint32_t crc, uint8_t v)
        {
            return (crc >> 8) ^ _bq_crc32c_table[(crc ^ v) & 0xFF];
        }

        static bq_forceinline uint32_t _crc32_sw_u16(uint32_t crc, uint16_t v)
        {
            crc = _crc32_sw_u8(crc, (uint8_t)(v & 0xFF));
            crc = _crc32_sw_u8(crc, (uint8_t)((v >> 8) & 0xFF));
            return crc;
        }

        static bq_forceinline uint32_t _crc32_sw_u32(uint32_t crc, uint32_t v)
        {
            crc = _crc32_sw_u8(crc, (uint8_t)(v & 0xFF));
            crc = _crc32_sw_u8(crc, (uint8_t)((v >> 8) & 0xFF));
            crc = _crc32_sw_u8(crc, (uint8_t)((v >> 16) & 0xFF));
            crc = _crc32_sw_u8(crc, (uint8_t)((v >> 24) & 0xFF));
            return crc;
        }

        static bq_forceinline uint32_t _crc32_sw_u64(uint32_t crc, uint64_t v)
        {
            crc = _crc32_sw_u32(crc, (uint32_t)(v & 0xFFFFFFFF));
            crc = _crc32_sw_u32(crc, (uint32_t)((v >> 32) & 0xFFFFFFFF));
            return crc;
        }

        // ---------------- HW Intrinsics ----------------
        static bq_forceinline BQ_HW_CRC_TARGET uint32_t _bq_crc32_u8_hw(uint32_t crc, uint8_t v)
        {
        #if defined(BQ_X86)
            return _mm_crc32_u8(crc, v);
        #elif defined(BQ_ARM) && (defined(__ARM_FEATURE_CRC32) || defined(BQ_ARM_64))
            return __crc32b(crc, v);
        #else
            return _crc32_sw_u8(crc, v);
        #endif
        }

        static bq_forceinline BQ_HW_CRC_TARGET uint32_t _bq_crc32_u16_hw(uint32_t crc, uint16_t v)
        {
        #if defined(BQ_X86)
            return _mm_crc32_u16(crc, v);
        #elif defined(BQ_ARM) && (defined(__ARM_FEATURE_CRC32) || defined(BQ_ARM_64))
            return __crc32h(crc, v);
        #else
            return _crc32_sw_u16(crc, v);
        #endif
        }

        static bq_forceinline BQ_HW_CRC_TARGET uint32_t _bq_crc32_u32_hw(uint32_t crc, uint32_t v)
        {
        #if defined(BQ_X86)
            return _mm_crc32_u32(crc, v);
        #elif defined(BQ_ARM) && (defined(__ARM_FEATURE_CRC32) || defined(BQ_ARM_64))
            return __crc32w(crc, v);
        #else
            return _crc32_sw_u32(crc, v);
        #endif
        }

        static bq_forceinline BQ_HW_CRC_TARGET uint32_t _bq_crc32_u64_hw(uint32_t crc, uint64_t v)
        {
        #if defined(BQ_X86_64)
            return (uint32_t)_mm_crc32_u64(crc, v);
        #elif defined(BQ_ARM_64) && (defined(__ARM_FEATURE_CRC32) || defined(BQ_ARM_64))
            return __crc32d(crc, v);
        #else
            return _crc32_sw_u64(crc, v);
        #endif
        }

        #define BQ_GEN_HASH_CORE(DO_COPY, CRC_U8, CRC_U16, CRC_U32, CRC_U64) \
            const uint8_t* s = (const uint8_t*)src; \
            uint8_t* d = (uint8_t*)dst; \
            uint32_t h1 = 0, h2 = 0, h3 = 0, h4 = 0; \
            BQ_LIKELY_IF(len >= 32) { \
                const uint8_t* const src_end = s + len; \
                uint8_t* const dst_end = d + len; \
                while (s <= src_end - 32) { \
                    uint64_t v1, v2, v3, v4; \
                    memcpy(&v1, s, 8); memcpy(&v2, s + 8, 8); memcpy(&v3, s + 16, 8); memcpy(&v4, s + 24, 8); \
                    BQ_CONSTEXPR_IF (DO_COPY) { \
                        memcpy(d, &v1, 8); memcpy(d + 8, &v2, 8); memcpy(d + 16, &v3, 8); memcpy(d + 24, &v4, 8); \
                        d += 32; \
                    } \
                    h1 = CRC_U64(h1, v1); h2 = CRC_U64(h2, v2); h3 = CRC_U64(h3, v3); h4 = CRC_U64(h4, v4); \
                    s += 32; \
                } \
                if (s < src_end) { \
                    s = src_end - 32; \
                    uint64_t v1, v2, v3, v4; \
                    memcpy(&v1, s, 8); memcpy(&v2, s + 8, 8); memcpy(&v3, s + 16, 8); memcpy(&v4, s + 24, 8); \
                    BQ_CONSTEXPR_IF (DO_COPY) { \
                        d = dst_end - 32; \
                        memcpy(d, &v1, 8); memcpy(d + 8, &v2, 8); memcpy(d + 16, &v3, 8); memcpy(d + 24, &v4, 8); \
                    } \
                    h1 = CRC_U64(h1, v1); h2 = CRC_U64(h2, v2); h3 = CRC_U64(h3, v3); h4 = CRC_U64(h4, v4); \
                } \
            } else { \
                if (len >= 16) { \
                    uint64_t v1, v2; \
                    memcpy(&v1, s, 8); memcpy(&v2, s + 8, 8); \
                    BQ_CONSTEXPR_IF (DO_COPY) { memcpy(d, &v1, 8); memcpy(d + 8, &v2, 8); } \
                    h1 = CRC_U64(h1, v1); h2 = CRC_U64(h2, v2); \
                    const uint8_t* s_last = s + len - 16; \
                    memcpy(&v1, s_last, 8); memcpy(&v2, s_last + 8, 8); \
                    BQ_CONSTEXPR_IF (DO_COPY) { \
                        uint8_t* d_last = d + len - 16; \
                        memcpy(d_last, &v1, 8); memcpy(d_last + 8, &v2, 8); \
                    } \
                    h3 = CRC_U64(h3, v1); h4 = CRC_U64(h4, v2); \
                } else if (len >= 8) { \
                    uint64_t v; \
                    memcpy(&v, s, 8); \
                    BQ_CONSTEXPR_IF (DO_COPY) memcpy(d, &v, 8); \
                    h1 = CRC_U64(h1, v); \
                    const uint8_t* s_last = s + len - 8; \
                    memcpy(&v, s_last, 8); \
                    BQ_CONSTEXPR_IF (DO_COPY) { uint8_t* d_last = d + len - 8; memcpy(d_last, &v, 8); } \
                    h2 = CRC_U64(h2, v); \
                } else if (len >= 4) { \
                    uint32_t v; \
                    memcpy(&v, s, 4); \
                    BQ_CONSTEXPR_IF (DO_COPY) memcpy(d, &v, 4); \
                    h1 = CRC_U32(h1, v); \
                    const uint8_t* s_last = s + len - 4; \
                    memcpy(&v, s_last, 4); \
                    BQ_CONSTEXPR_IF (DO_COPY) { uint8_t* d_last = d + len - 4; memcpy(d_last, &v, 4); } \
                    h2 = CRC_U32(h2, v); \
                } else if (len > 0) { \
                    if (len & 2) { \
                        uint16_t v; \
                        memcpy(&v, s, 2); \
                        BQ_CONSTEXPR_IF (DO_COPY) { memcpy(d, &v, 2); d += 2; } \
                        h1 = CRC_U16(h1, v); \
                        s += 2; \
                    } \
                    if (len & 1) { \
                        uint8_t v = *s; \
                        BQ_CONSTEXPR_IF (DO_COPY) *d = v; \
                        h2 = CRC_U8(h2, v); \
                    } \
                } \
            } \
            uint64_t low = (uint64_t)(h1 ^ h3); \
            uint64_t high = (uint64_t)(h2 ^ h4); \
            return (high << 32) | low;


        // Implementations
        static BQ_CRC_HW_INLINE BQ_HW_CRC_TARGET uint64_t _bq_memcpy_with_hash_hw(void* dst, const void* src, size_t len) {
            BQ_GEN_HASH_CORE(true, _bq_crc32_u8_hw, _bq_crc32_u16_hw, _bq_crc32_u32_hw, _bq_crc32_u64_hw)
        }
        static BQ_CRC_HW_INLINE uint64_t _bq_memcpy_with_hash_sw(void* dst, const void* src, size_t len) {
            BQ_GEN_HASH_CORE(true, _crc32_sw_u8, _crc32_sw_u16, _crc32_sw_u32, _crc32_sw_u64)
        }
        static BQ_CRC_HW_INLINE BQ_HW_CRC_TARGET uint64_t _bq_hash_only_hw(const void* src, size_t len) {
            void* dst = nullptr; // Dummy
            BQ_GEN_HASH_CORE(false, _bq_crc32_u8_hw, _bq_crc32_u16_hw, _bq_crc32_u32_hw, _bq_crc32_u64_hw)
        }
        static BQ_CRC_HW_INLINE uint64_t _bq_hash_only_sw(const void* src, size_t len) {
            void* dst = nullptr; // Dummy
            BQ_GEN_HASH_CORE(false, _crc32_sw_u8, _crc32_sw_u16, _crc32_sw_u32, _crc32_sw_u64)
        }

    public:
        /**
         * Ultra-fast copy-and-hash utility optimized for modern CPU pipelines.
         * Approaches hardware limits and can outperform standalone memcpy for small data chunks.
         * Uses 4-way interleaved CRC32C and overlapping block strategy.
         * returns: ((h1 ^ h3) << 32) | (h2 ^ h4)
         */
        static bq_forceinline uint64_t bq_memcpy_with_hash(void* BQ_RESTRICT dst, const void* BQ_RESTRICT src, size_t len)
        {
            BQ_LIKELY_IF(_bq_crc32_supported_) {
                return _bq_memcpy_with_hash_hw(dst, src, len);
            } else {
                return _bq_memcpy_with_hash_sw(dst, src, len);
            }
        }

        /**
         * Ultra-fast hash utility optimized for modern CPU pipelines.
         * Approaches hardware limits.
         */
        static bq_forceinline uint64_t bq_hash_only(const void* src, size_t len)
        {
            BQ_LIKELY_IF(_bq_crc32_supported_) {
                return _bq_hash_only_hw(src, len);
            } else {
                return _bq_hash_only_sw(src, len);
            }
        }

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
        /// convert utf16 to utf8
        /// </summary>
        /// <param name="src_utf16_str"></param>
        /// <param name="src_character_num"></param>
        /// <param name="target_utf8_str"></param>
        /// <param name="target_utf8_character_num"></param>
        /// <returns>length of final utf8 str</returns>
        static uint32_t utf16_to_utf8(const char16_t* src_utf16_str, uint32_t src_character_num, char* target_utf8_str, uint32_t target_utf8_character_num);

        /// <summary>
        /// convert utf8 to utf16
        /// </summary>
        /// <param name="src_utf8_str"></param>
        /// <param name="src_character_num"></param>
        /// <param name="target_utf16_str"></param>
        /// <param name="target_utf16_character_num"></param>
        /// <returns>length of final utf16 str, it's len of char16_t*, not char*</returns>
        static uint32_t utf8_to_utf16(const char* src_utf8_str, uint32_t src_character_num, char16_t* target_utf16_str, uint32_t target_utf16_character_num);

        /// <summary>
        /// Allocates and constructs an object of type `T` with the specified alignment.
        ///
        /// If the compiler supports C++17 aligned `new` (indicated by `BQ_ALIGNAS_NEW`),
        /// the function uses the standard `new` operator, and the `alignment` parameter is ignored.
        /// In this case, the alignment is determined by the `alignas` specifier in the class definition of `T`.
        ///
        /// If the compiler does not support C++17 aligned `new`, the function uses
        /// `bq::platform::aligned_alloc` to allocate memory with the specified `alignment`,
        /// and constructs the object using placement new. The caller must ensure that the
        /// provided `alignment` matches the `alignas` specifier (if any) in the class definition
        /// of `T` to avoid undefined behavior.
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
#if defined(BQ_ALIGNAS_NEW)
            (void)alignment;
            return new T(bq::forward<V>(args)...);
#else
            void* ptr = bq::platform::aligned_alloc(alignment, sizeof(T));
            if (!ptr) {
                return nullptr;
            }
            new ((void*)ptr, bq::enum_new_dummy::dummy) T(bq::forward<V>(args)...);
            return static_cast<T*>(ptr);
#endif
        }
        template <typename T, typename... V>
        static void aligned_delete(T* ptr)
        {
#if defined(BQ_ALIGNAS_NEW)
            delete ptr;
#else
            bq::object_destructor<T>::destruct(static_cast<T*>(ptr));
            bq::platform::aligned_free(ptr);
#endif
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