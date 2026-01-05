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

#include "bq_common/utils/util.h"
#include <sys/stat.h>
#include "bq_common/bq_common.h"
#if defined(BQ_ANDROID)
#include <android/log.h>
#elif defined(BQ_OHOS)
#include <hilog/log.h>
#elif defined(BQ_WIN)
#include "bq_common/platform/win64_includes_begin.h"
#elif defined(BQ_APPLE)
#include <sys/sysctl.h>
#endif
#if defined(BQ_POSIX)
#include <unistd.h>
#endif

#if defined(BQ_ARM_NEON)

bq_forceinline uint16_t bq_vmaxvq_u16(uint16x8_t v) {
#if defined(BQ_ARM_64)
    return vmaxvq_u16(v);
#else
    uint16x4_t hi = vget_high_u16(v);
    uint16x4_t lo = vget_low_u16(v);
    uint16x4_t m = vmax_u16(hi, lo);
    m = vpmax_u16(m, m);
    m = vpmax_u16(m, m);
    return vget_lane_u16(m, 0);
#endif
}

bq_forceinline uint8_t bq_vmaxvq_u8(uint8x16_t v) {
#if defined(BQ_ARM_64)
    return vmaxvq_u8(v);
#else
    uint8x8_t hi = vget_high_u8(v);
    uint8x8_t lo = vget_low_u8(v);
    uint8x8_t m = vmax_u8(hi, lo);
    m = vpmax_u8(m, m);
    m = vpmax_u8(m, m);
    m = vpmax_u8(m, m);
    return vget_lane_u8(m, 0);
#endif
}
#endif

namespace bq {
    // Castagnoli CRC32C Polynomial: 0x1EDC6F41 (Reversed: 0x82F63B78)
    const static uint32_t _bq_crc32c_table[256] = {
        0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4, 0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
        0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B, 0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
        0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B, 0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
        0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54, 0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
        0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A, 0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
        0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5, 0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
        0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45, 0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
        0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A, 0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
        0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48, 0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
        0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687, 0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
        0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927, 0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
        0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8, 0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
        0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096, 0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
        0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859, 0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
        0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9, 0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
        0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36, 0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
        0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C, 0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
        0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043, 0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
        0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3, 0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
        0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C, 0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
        0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652, 0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
        0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D, 0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
        0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D, 0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
        0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2, 0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
        0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530, 0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
        0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF, 0x8ECEE914, 0x7CA56A17, 0x6FD599E3, 0x9DBE1AE0,
        0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F, 0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
        0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90, 0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
        0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE, 0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
        0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321, 0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
        0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81, 0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
        0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E, 0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351
    };
    static bool _bq_crc32_supported_ = common_global_vars::get().crc32_supported_;

#if defined(BQ_X86)
    static bool _bq_avx2_supported_ = common_global_vars::get().avx2_support_;
#endif

    // Internal flag to check if SIMD UTF is supported on current platform
    static bool _bq_utf_simd_supported_ = []() {
#if defined(BQ_X86)
        // Assume basic SSE is available on modern x86 (including Android x86/Atom)
        return true; 
#elif defined(BQ_ARM_NEON)
        return true; // Assume NEON on ARMv8/M1
#else
        return false;
#endif
    }();

    // ---------------- SW Fallbacks ----------------
    bq_forceinline uint32_t _crc32_sw_u8(uint32_t crc, uint8_t v)
    {
        return (crc >> 8) ^ _bq_crc32c_table[(crc ^ v) & 0xFF];
    }

    bq_forceinline uint32_t _crc32_sw_u16(uint32_t crc, uint16_t v)
    {
        crc = _crc32_sw_u8(crc, static_cast<uint8_t>(v & 0xFF));
        crc = _crc32_sw_u8(crc, static_cast<uint8_t>((v >> 8) & 0xFF));
        return crc;
    }

    bq_forceinline uint32_t _crc32_sw_u32(uint32_t crc, uint32_t v)
    {
        crc = _crc32_sw_u8(crc, static_cast<uint8_t>(v & 0xFF));
        crc = _crc32_sw_u8(crc, static_cast<uint8_t>((v >> 8) & 0xFF));
        crc = _crc32_sw_u8(crc, static_cast<uint8_t>((v >> 16) & 0xFF));
        crc = _crc32_sw_u8(crc, static_cast<uint8_t>((v >> 24) & 0xFF));
        return crc;
    }

    bq_forceinline uint32_t _crc32_sw_u64(uint32_t crc, uint64_t v)
    {
        crc = _crc32_sw_u32(crc, static_cast<uint32_t>(v & 0xFFFFFFFF));
        crc = _crc32_sw_u32(crc, static_cast<uint32_t>((v >> 32) & 0xFFFFFFFF));
        return crc;
    }

    // ---------------- HW Intrinsics ----------------
    bq_forceinline BQ_HW_CRC_TARGET uint32_t _bq_crc32_u8_hw(uint32_t crc, uint8_t v)
    {
#if defined(BQ_X86)
        return _mm_crc32_u8(crc, v);
#elif defined(BQ_ARM) && defined(__ARM_FEATURE_CRC32)
        return __crc32b(crc, v);
#else
        assert(false && "impossible path, only for compile");
        return 0;
#endif
    }

    bq_forceinline BQ_HW_CRC_TARGET uint32_t _bq_crc32_u16_hw(uint32_t crc, uint16_t v)
    {
#if defined(BQ_X86)
        return _mm_crc32_u16(crc, v);
#elif defined(BQ_ARM) && defined(__ARM_FEATURE_CRC32)
        return __crc32h(crc, v);
#else
        assert(false && "impossible path, only for compile");
        return 0;
#endif
    }

    bq_forceinline BQ_HW_CRC_TARGET uint32_t _bq_crc32_u32_hw(uint32_t crc, uint32_t v)
    {
#if defined(BQ_X86)
        return _mm_crc32_u32(crc, v);
#elif defined(BQ_ARM) && defined(__ARM_FEATURE_CRC32)
        return __crc32w(crc, v);
#else
        assert(false && "impossible path, only for compile");
        return 0;
#endif
    }

    bq_forceinline BQ_HW_CRC_TARGET uint32_t _bq_crc32_u64_hw(uint32_t crc, uint64_t v)
    {
#if defined(BQ_X86_64)
        return (uint32_t)_mm_crc32_u64(crc, v);
#elif defined(BQ_ARM_64) && defined(__ARM_FEATURE_CRC32)
        return __crc32d(crc, v);
#else
        assert(false && "impossible path, only for compile");
        return 0;
#endif
    }

#define BQ_GEN_HASH_CORE(DO_COPY, CRC_U8, CRC_U16, CRC_U32, CRC_U64) \
            auto* s = reinterpret_cast<const uint8_t*>(src); \
            auto* d = reinterpret_cast<uint8_t*>(dst); \
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
            return (high << 32) | low; \


    // Implementations
    BQ_CRC_HW_INLINE BQ_HW_CRC_TARGET uint64_t _bq_memcpy_with_hash_hw(void* BQ_RESTRICT dst, const void* BQ_RESTRICT src, size_t len) {
        BQ_GEN_HASH_CORE(true, _bq_crc32_u8_hw, _bq_crc32_u16_hw, _bq_crc32_u32_hw, _bq_crc32_u64_hw)
    }
    BQ_CRC_HW_INLINE uint64_t _bq_memcpy_with_hash_sw(void* BQ_RESTRICT dst, const void* BQ_RESTRICT src, size_t len) {
        BQ_GEN_HASH_CORE(true, _crc32_sw_u8, _crc32_sw_u16, _crc32_sw_u32, _crc32_sw_u64)
    }
    BQ_CRC_HW_INLINE BQ_HW_CRC_TARGET uint64_t _bq_hash_only_hw(const void* BQ_RESTRICT src, size_t len) {
        void* dst = nullptr; // Dummy
        BQ_GEN_HASH_CORE(false, _bq_crc32_u8_hw, _bq_crc32_u16_hw, _bq_crc32_u32_hw, _bq_crc32_u64_hw)
    }
    BQ_CRC_HW_INLINE uint64_t _bq_hash_only_sw(const void* BQ_RESTRICT src, size_t len) {
        void* dst = nullptr; // Dummy
        BQ_GEN_HASH_CORE(false, _crc32_sw_u8, _crc32_sw_u16, _crc32_sw_u32, _crc32_sw_u64)
    }


    static BQ_TLS uint32_t rand_seed = 0;
    static BQ_TLS uint64_t rand_seed_64 = 0;
    static util::type_func_ptr_bq_util_consle_callback consle_callback_ = nullptr;


    uint64_t util::bq_memcpy_with_hash(void* BQ_RESTRICT dst, const void* BQ_RESTRICT src, size_t len)
    {
        BQ_LIKELY_IF(_bq_crc32_supported_) {
            return _bq_memcpy_with_hash_hw(dst, src, len);
        }
        else {
            return _bq_memcpy_with_hash_sw(dst, src, len);
        }
    }


    uint64_t util::bq_hash_only(const void* src, size_t len)
    {
        BQ_LIKELY_IF(_bq_crc32_supported_) {
            return _bq_hash_only_hw(src, len);
        }
        else {
            return _bq_hash_only_sw(src, len);
        }
    }

    void util::bq_assert(bool cond, bq::string msg)
    {
        if (!cond)
            bq_record(msg);
        assert(cond && msg.c_str());
    }

    void util::bq_record(bq::string msg, string file_name)
    {
        bq::platform::scoped_mutex lock(common_global_vars::get().console_mutex_);
        string path = TO_ABSOLUTE_PATH(file_name, true);
        bq::file_manager::append_all_text(path, msg);
    }

#if defined(BQ_TOOLS) || defined(BQ_UNIT_TEST)
    static bq::log_level log_device_min_level = bq::log_level::warning;
    void util::set_log_device_console_min_level(bq::log_level level)
    {
        log_device_min_level = level;
    }
#endif

    void util::log_device_console(bq::log_level level, const char* format, ...)
    {
#if defined(BQ_TOOLS) || defined(BQ_UNIT_TEST)
        if (level < log_device_min_level) {
            return;
        }
#endif
        auto& device_console_buffer = common_global_vars::get().device_console_buffer_;
        bq::platform::scoped_mutex lock(common_global_vars::get().console_mutex_);
        va_list args;
        va_start(args, format);
        if (format) {
            while (true) {
                va_start(args, format);
                bool failed = (static_cast<bq::array<char>::size_type>(vsnprintf(&device_console_buffer[0], device_console_buffer.size(), format, args)) + 1) >= device_console_buffer.size();
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
        auto callback = consle_callback_;
        if (callback) {
            callback(level, text);
        } else {
            _default_console_output(level, text);
        }
    }

    void util::set_console_output_callback(type_func_ptr_bq_util_consle_callback callback)
    {
        consle_callback_ = callback;
    }

    void util::_default_console_output(bq::log_level level, const char* text)
    {
#if defined(BQ_TOOLS) || defined(BQ_UNIT_TEST)
        if (level < log_device_min_level) {
            return;
        }
#endif

#if defined(BQ_IN_GITHUB_ACTIONS)
        switch (level) {
        case bq::log_level::verbose:
            printf("[Bq][V] %s\n", text ? text : "");
            break;
        case bq::log_level::debug:
            printf("[Bq][D] %s\n", text ? text : "");
            break;
        case bq::log_level::info:
            printf("[Bq][I] %s\n", text ? text : "");
            break;
        case bq::log_level::warning:
            printf("[Bq][W] %s\n", text ? text : "");
            break;
        case bq::log_level::error:
            printf("[Bq][E] %s\n", text ? text : "");
            break;
        case bq::log_level::fatal:
            printf("[Bq][F] %s\n", text ? text : "");
            break;
        default:
            break;
        }
#else

#if defined(BQ_ANDROID) && !defined(BQ_UNIT_TEST)
        __android_log_write(ANDROID_LOG_VERBOSE + (static_cast<int32_t>(level) - static_cast<int32_t>(bq::log_level::verbose)),
            "Bq", text ? text : "");
#elif defined(BQ_OHOS)
        {
            auto oh_level = level < bq::log_level::debug ? bq::log_level::debug : level; // There is no verbose level in OHOS
            OH_LOG_PrintMsg(LOG_APP,
                static_cast<LogLevel>(static_cast<int32_t>(LOG_DEBUG) + static_cast<int32_t>(oh_level) - static_cast<int32_t>(bq::log_level::debug)), 0x8527, "Bq", text ? text : "");
        }
#elif defined(BQ_IOS)
        (void)level;
        bq::platform::ios_print(text ? text : "");
#else
        bq::platform::scoped_mutex lock(common_global_vars::get().console_mutex_);

        // Color code（ANSI VT）
        auto color = "";
        switch (level) {
        case bq::log_level::verbose:
            color = "\x1b[3m";
            break;
        case bq::log_level::debug:
            color = "\x1b[92m";
            break;
        case bq::log_level::info:
            color = "\x1b[94m";
            break;
        case bq::log_level::warning:
            color = "\x1b[1;40;93m";
            break;
        case bq::log_level::error:
            color = "\x1b[1;40;91m";
            break;
        case bq::log_level::fatal:
            color = "\x1b[1;30;101m";
            break;
        default:
            color = "\x1b[37m";
            break;
        }
        auto reset = "\x1b[0m";
        auto prefix = "[Bq]";
        auto newline = "\n";
        const char* msg = text ? text : "";

        const size_t color_len = strlen(color);
        const size_t reset_len = strlen(reset);
        const size_t prefix_len = strlen(prefix);
        const size_t msg_len = strlen(msg);
        constexpr size_t nl_len = 1; // "\n"

        bool to_stderr = (level == bq::log_level::error || level == bq::log_level::fatal);

        constexpr size_t STACK_CAP = 1024;
#if defined(BQ_WIN)
        HANDLE h = ::GetStdHandle(to_stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);

        static bool s_vt_inited = false;
        static bool s_vt_enabled = false;
        DWORD mode = 0;
        const bool has_console = (h && h != INVALID_HANDLE_VALUE && ::GetConsoleMode(h, &mode));

        if (has_console && !s_vt_inited) {
            s_vt_enabled = (::SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0);
            s_vt_inited = true;
        }

        const bool use_color = has_console && s_vt_enabled;

        const size_t total_len = (use_color ? color_len : 0) + prefix_len + msg_len + (use_color ? reset_len : 0) + nl_len;

        if (has_console && total_len <= STACK_CAP) {
            char buf[STACK_CAP];
            char* p = buf;

            if (use_color) {
                memcpy(p, color, color_len);
                p += color_len;
            }
            memcpy(p, prefix, prefix_len);
            p += prefix_len;
            if (msg_len) {
                memcpy(p, msg, msg_len);
                p += msg_len;
            }
            if (use_color) {
                memcpy(p, reset, reset_len);
                p += reset_len;
            }
            memcpy(p, newline, nl_len);
            p += nl_len;

            DWORD written = 0;
            WriteFile(h, buf, static_cast<DWORD>(p - buf), &written, nullptr);
        } else if (has_console) {
            DWORD written = 0;
            BOOL ret;
            if (use_color && color_len) {
                ret = WriteFile(h, color, static_cast<DWORD>(color_len), &written, nullptr);
            }
            ret = WriteFile(h, prefix, static_cast<DWORD>(prefix_len), &written, nullptr);
            if (msg_len) {
                ret = WriteFile(h, msg, static_cast<DWORD>(msg_len), &written, nullptr);
            }
            if (use_color && reset_len) {
                ret = WriteFile(h, reset, static_cast<DWORD>(reset_len), &written, nullptr);
            }
            ret = WriteFile(h, newline, static_cast<DWORD>(nl_len), &written, nullptr);
            (void)ret;
            (void)written;
        }

#if defined(BQ_MSVC)
        OutputDebugStringA(prefix);
        OutputDebugStringA(msg);
        OutputDebugStringA("\n");
#endif

#else
        const int32_t fd = to_stderr ? STDERR_FILENO : STDOUT_FILENO;
        const bool is_tty = (isatty(fd) == 1);

        const bool use_color = is_tty;
        const size_t total_len = (use_color ? color_len : 0) + prefix_len + msg_len + (use_color ? reset_len : 0) + nl_len;
        ssize_t writen = 0;
        if (total_len <= STACK_CAP) {
            char buf[STACK_CAP];
            char* p = buf;

            if (use_color) {
                memcpy(p, color, color_len);
                p += color_len;
            }
            memcpy(p, prefix, prefix_len);
            p += prefix_len;
            if (msg_len) {
                memcpy(p, msg, msg_len);
                p += msg_len;
            }
            if (use_color) {
                memcpy(p, reset, reset_len);
                p += reset_len;
            }
            memcpy(p, newline, nl_len);
            p += nl_len;
            writen += write(fd, buf, static_cast<size_t>(p - buf));
        } else {
            if (use_color && color_len) {
                writen += write(fd, color, color_len);
            }
            writen += write(fd, prefix, prefix_len);
            if (msg_len) {
                writen += write(fd, msg, msg_len);
            }
            if (use_color && reset_len) {
                writen += write(fd, reset, reset_len);
            }
            writen += write(fd, newline, nl_len);
        }
        (void)writen;
#endif

#endif
#endif

    }

    uint32_t util::get_hash(const void* data, size_t size)
    {
        const uint64_t h64 = util::bq_hash_only(data, size);
        return static_cast<uint32_t>(h64 ^ (h64 >> 32));
    }

    uint64_t util::get_hash_64(const void* data, size_t size)
    {
        return util::bq_hash_only(data, size);
    }

    bool util::is_little_endian()
    {
        int32_t test = 1;
        return *reinterpret_cast<char*>(&test) == 1;
    }

    void util::srand(uint32_t seed)
    {
        rand_seed = seed;
    }
    uint32_t util::rand()
    {
        if (rand_seed == 0)
            rand_seed = static_cast<uint32_t>(time(nullptr));
        uint32_t x = rand_seed;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rand_seed = x;
        return x;
    }

    void util::srand64(uint64_t seed) {
        rand_seed_64 = seed;
    }

    uint64_t util::rand64() {
        if (rand_seed_64 == 0)
            rand_seed_64 = util::rand();
        uint64_t x = rand_seed_64;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        rand_seed_64 = x;
        return x;
    }


    // =================================================================================================
    // Fast Implementation (SIMD + Optimized Scalar)
    // =================================================================================================

    //Fallback
    bq_forceinline uint32_t _impl_utf16_to_utf8_scalar_fast(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        (void)dst_character_num;
        const char16_t* src_end = src + src_character_num;
        char* dst_ptr = dst;

        while (src < src_end) {
            uint32_t c = *src++;
            if (c < 0x80) {
                *dst_ptr++ = static_cast<char>(c);
                // Tight loop for ASCII runs
                while (src < src_end && *src < 0x80) {
                    *dst_ptr++ = static_cast<char>(*src++);
                }
            }
            else if (c < 0x800) {
                *dst_ptr++ = static_cast<char>(0xC0 | (c >> 6));
                *dst_ptr++ = static_cast<char>(0x80 | (c & 0x3F));
            }
            else if (c >= 0xD800 && c <= 0xDFFF) {
                if (c >= 0xDC00) { continue; } // Orphan trailing surrogate
                if (src < src_end) {
                    uint32_t c2 = *src;
                    if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
                        src++;
                        c = (((c & 0x3FF) << 10) | (c2 & 0x3FF)) + 0x10000;
                        *dst_ptr++ = static_cast<char>(0xF0 | (c >> 18));
                        *dst_ptr++ = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
                        *dst_ptr++ = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                        *dst_ptr++ = static_cast<char>(0x80 | (c & 0x3F));
                    }
                }
            }
            else {
                *dst_ptr++ = static_cast<char>(0xE0 | (c >> 12));
                *dst_ptr++ = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                *dst_ptr++ = static_cast<char>(0x80 | (c & 0x3F));
            }
        }
        return static_cast<uint32_t>(dst_ptr - dst);
    }

#if defined(BQ_X86)
    // AVX2 Safe Implementation
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_utf16_to_utf8_simd_avx2(const char16_t* BQ_RESTRICT src_ptr, const char16_t* src_end, char* BQ_RESTRICT dst_ptr)
    {
        const char* dst_start = dst_ptr;
        while (src_ptr + 16 <= src_end) {
            const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr));
            __m256i mask = _mm256_set1_epi16(static_cast<int16_t>(0xFF80u));
            if (_mm256_testz_si256(chunk, mask)) {
                __m256i compressed = _mm256_packus_epi16(chunk, chunk); 
                __m256i permuted = _mm256_permute4x64_epi64(compressed, 0xD8);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr), _mm256_castsi256_si128(permuted));
                src_ptr += 16;
                dst_ptr += 16;
            } else {
                 break;
            }
        }
        return static_cast<uint32_t>(dst_ptr - dst_start) + _impl_utf16_to_utf8_scalar_fast(src_ptr, static_cast<uint32_t>(src_end - src_ptr), dst_ptr, 0);
    }

    // SSE Safe Implementation
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_SSE_TARGET uint32_t _impl_utf16_to_utf8_simd_sse(const char16_t* BQ_RESTRICT src_ptr, const char16_t* src_end, char* BQ_RESTRICT dst_ptr)
    {
        const char* dst_start = dst_ptr;
        while (src_ptr + 8 <= src_end) {
            const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
            __m128i mask = _mm_set1_epi16(static_cast<int16_t>(0xFF80u));
            if (_mm_testz_si128(chunk, mask)) {
                const __m128i compressed = _mm_packus_epi16(chunk, chunk);
                _mm_storel_epi64(reinterpret_cast<__m128i*>(dst_ptr), compressed);
                src_ptr += 8;
                dst_ptr += 8;
            } else {
                 break;
            }
        }
        return static_cast<uint32_t>(dst_ptr - dst_start) + _impl_utf16_to_utf8_scalar_fast(src_ptr, static_cast<uint32_t>(src_end - src_ptr), dst_ptr, 0);
    }
#endif

    BQ_SIMD_HW_INLINE uint32_t _impl_utf16_to_utf8_simd(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        const char16_t* src_ptr = src;
        const char16_t* src_end = src + src_character_num;
        char* dst_ptr = dst;
        (void)dst_character_num;
        (void)src_ptr;
        (void)src_end;
        (void)dst_ptr;
#if defined(BQ_X86)
        if (_bq_avx2_supported_) {
            return _impl_utf16_to_utf8_simd_avx2(src_ptr, src_end, dst_ptr);
        } else {
            return _impl_utf16_to_utf8_simd_sse(src_ptr, src_end, dst_ptr);
        }
#elif defined(BQ_ARM_NEON)
        // NEON path
        while (src_ptr + 8 <= src_end) {
            uint16x8_t chunk = vld1q_u16(reinterpret_cast<const uint16_t*>(src_ptr));
            // Check if all < 0x80
            if (bq_vmaxvq_u16(chunk) < 0x80) {
                 uint8x8_t ascii = vmovn_u16(chunk);
                 vst1_u8(reinterpret_cast<uint8_t*>(dst_ptr), ascii);
                 src_ptr += 8;
                 dst_ptr += 8;
            } else {
                // Scalar fallback for this chunk
                 for (int32_t i = 0; i < 8; ++i) {
                     char16_t c = src_ptr[i];
                     if (c < 0x80) {
                         *dst_ptr++ = (char)c;
                     } else {
                         // Simple fallback logic
                         if (c < 0x800) {
                             *dst_ptr++ = static_cast<char>(0xC0 | (c >> 6));
                             *dst_ptr++ = static_cast<char>(0x80 | (c & 0x3F));
                         } else if (c >= 0xD800 && c <= 0xDFFF) {
                             src_ptr += i; 
                             goto scalar_utf16; // break out to scalar
                         } else {
                             *dst_ptr++ = static_cast<char>(0xE0 | (c >> 12));
                             *dst_ptr++ = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                             *dst_ptr++ = static_cast<char>(0x80 | (c & 0x3F));
                         }
                     }
                 }
                 src_ptr += 8;
            }
        }
    scalar_utf16:
        return static_cast<uint32_t>(dst_ptr - dst) + _impl_utf16_to_utf8_scalar_fast(src_ptr, static_cast<uint32_t>(src_end - src_ptr), dst_ptr, 0);
#else
        return _impl_utf16_to_utf8_scalar_fast(src, src_character_num, dst, dst_character_num);
#endif
    }

    //Fallback
    bq_forceinline uint32_t _impl_utf8_to_utf16_scalar_fast(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        (void)dst_character_num;
        const auto* s = reinterpret_cast<const uint8_t*>(src);
        const auto* end = s + src_character_num;
        char16_t* d = dst;

        while (s < end) {
            uint8_t c = *s++;
            if (c < 0x80) {
                *d++ = static_cast<char16_t>(c);
                // Tight loop for ASCII runs
                while (s < end && *s < 0x80) {
                    *d++ = static_cast<char16_t>(*s++);
                }
            }
            else if ((c & 0xE0) == 0xC0) {
                *d++ = static_cast<char16_t>(((c & 0x1F) << 6) | (*s++ & 0x3F));
            }
            else if ((c & 0xF0) == 0xE0) {
                *d++ = static_cast<char16_t>(((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F));
                s += 2;
            }
            else {
                // 4 bytes or invalid
                auto cp = static_cast<uint32_t>(((c & 0x07) << 18) | ((s[0] & 0x3F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F));
                cp -= 0x10000;
                *d++ = static_cast<char16_t>((cp >> 10) + 0xD800);
                *d++ = static_cast<char16_t>((cp & 0x3FF) + 0xDC00);
                s += 3;
            }
        }
        return static_cast<uint32_t>(d - dst);
    }

#if defined(BQ_X86)
    // AVX2 Safe Implementation
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_utf8_to_utf16_simd_avx2(const uint8_t* BQ_RESTRICT src_ptr, const uint8_t* src_end, char16_t* BQ_RESTRICT dst_ptr)
    {
        const char16_t* dst_start = dst_ptr;
        while (src_ptr + 16 <= src_end) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
            if (!_mm_movemask_epi8(chunk)) {
                const __m256i wide = _mm256_cvtepu8_epi16(chunk);
                 _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr), wide);
                 src_ptr += 16;
                 dst_ptr += 16;
            } else {
                 break;
            }
        }
        return static_cast<uint32_t>(dst_ptr - dst_start) + _impl_utf8_to_utf16_scalar_fast(reinterpret_cast<const char*>(src_ptr), static_cast<uint32_t>(src_end - src_ptr), dst_ptr, 0);
    }

    // SSE Safe Implementation
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_SSE_TARGET uint32_t _impl_utf8_to_utf16_simd_sse(const uint8_t* BQ_RESTRICT src_ptr, const uint8_t* src_end, char16_t* BQ_RESTRICT dst_ptr)
    {
        const char16_t* dst_start = dst_ptr;
        while (src_ptr + 16 <= src_end) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
            if (!_mm_movemask_epi8(chunk)) { 
                 __m128i lo = _mm_cvtepu8_epi16(chunk);
                 __m128i hi = _mm_cvtepu8_epi16(_mm_shuffle_epi32(chunk, 0xEE)); // High 64 bits to low
                 // Wait, _mm_cvtepu8_epi16 converts lower 8 bytes.
                 // We need to convert upper 8 bytes too.
                 // Correct logic for SSE:
                 // lo = cvt(chunk[0-7])
                 // hi = cvt(chunk[8-15])
                 // To get chunk[8-15] into low 64 bits: _mm_srli_si128(chunk, 8) or shuffle.
                 _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr), lo);
                 _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr + 8), hi);
                 src_ptr += 16;
                 dst_ptr += 16;
            } else {
                 for(int32_t i=0; i<16; ++i) {
                     if (*src_ptr < 0x80) {
                         *dst_ptr++ = static_cast<char16_t>(*src_ptr++);
                     } else {
                         uint8_t c = *src_ptr;
                         if ((c & 0xE0) == 0xC0) {
                             *dst_ptr++ = static_cast<char16_t>(((c & 0x1F) << 6) | (src_ptr[1] & 0x3F)); src_ptr += 2;
                         } else if ((c & 0xF0) == 0xE0) {
                             *dst_ptr++ = static_cast<char16_t>(((c & 0x0F) << 12) | ((src_ptr[1] & 0x3F) << 6) | (src_ptr[2] & 0x3F)); src_ptr += 3;
                         } else {
                             auto cp = static_cast<uint32_t>(((c & 0x07) << 18) | ((src_ptr[1] & 0x3F) << 12) | ((src_ptr[2] & 0x3F) << 6) | (src_ptr[3] & 0x3F));
                             cp -= 0x10000;
                             *dst_ptr++ = static_cast<char16_t>((cp >> 10) + 0xD800);
                             *dst_ptr++ = static_cast<char16_t>((cp & 0x3FF) + 0xDC00);
                             src_ptr += 4;
                         }
                         break; 
                     }
                 }
            }
        }
        return static_cast<uint32_t>(dst_ptr - dst_start) + _impl_utf8_to_utf16_scalar_fast(reinterpret_cast<const char*>(src_ptr), static_cast<uint32_t>(src_end - src_ptr), dst_ptr, 0);
    }
#endif

    BQ_SIMD_HW_INLINE uint32_t _impl_utf8_to_utf16_simd(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        const auto* src_ptr = reinterpret_cast<const uint8_t*>(src);
        const auto* src_end = src_ptr + src_character_num;
        char16_t* dst_ptr = dst;
        (void)dst_character_num;
        (void)src_ptr;
        (void)src_end;
        (void)dst_ptr;
#if defined(BQ_X86)
        if (_bq_avx2_supported_) {
            return _impl_utf8_to_utf16_simd_avx2(src_ptr, src_end, dst_ptr);
        } else {
            return _impl_utf8_to_utf16_simd_sse(src_ptr, src_end, dst_ptr);
        }
#elif defined(BQ_ARM_NEON)
        while (src_ptr + 16 <= src_end) {
             uint8x16_t chunk = vld1q_u8(src_ptr);
             if (bq_vmaxvq_u8(chunk) < 0x80) {
                 uint16x8_t low = vmovl_u8(vget_low_u8(chunk));
                 uint16x8_t high = vmovl_u8(vget_high_u8(chunk));
                 vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr), low);
                 vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 8), high);
                 src_ptr += 16;
                 dst_ptr += 16;
             } else {
                 // Fallback (same as x86)
                 for(int32_t i=0; i<16; ++i) {
                     if (*src_ptr < 0x80) {
                         *dst_ptr++ = static_cast<char16_t>(*src_ptr++);
                     } else {
                         uint8_t c = *src_ptr;
                         if ((c & 0xE0) == 0xC0) {
                             *dst_ptr++ = static_cast<char16_t>(((c & 0x1F) << 6) | (src_ptr[1] & 0x3F)); src_ptr += 2;
                         } else if ((c & 0xF0) == 0xE0) {
                             *dst_ptr++ = static_cast<char16_t>(((c & 0x0F) << 12) | ((src_ptr[1] & 0x3F) << 6) | (src_ptr[2] & 0x3F)); src_ptr += 3;
                         } else {
                             uint32_t cp = static_cast<uint32_t>(((c & 0x07) << 18) | ((src_ptr[1] & 0x3F) << 12) | ((src_ptr[2] & 0x3F) << 6) | (src_ptr[3] & 0x3F));
                             cp -= 0x10000;
                             *dst_ptr++ = static_cast<char16_t>((cp >> 10) + 0xD800);
                             *dst_ptr++ = static_cast<char16_t>((cp & 0x3FF) + 0xDC00);
                             src_ptr += 4;
                         }
                         break;
                     }
                 }
             }
        }
        return static_cast<uint32_t>(static_cast<uint32_t>(dst_ptr - dst) + _impl_utf8_to_utf16_scalar_fast(reinterpret_cast<const char*>(src_ptr), static_cast<uint32_t>(src_end - src_ptr), dst_ptr, 0));
#else
        return _impl_utf8_to_utf16_scalar_fast(reinterpret_cast<const char*>(src), src_character_num, dst, dst_character_num);
#endif
    }

    // =================================================================================================
    // Optimistic Implementation
    // =================================================================================================
    // Updates src and dst pointers. Stops at first non-ASCII or end.
    static bq_forceinline uint32_t _impl_utf16_to_utf8_ascii_optimistic_sw(const char16_t* BQ_RESTRICT& src, const char16_t* src_end, char* BQ_RESTRICT& dst)
    {
#ifndef NDEBUG
        assert(bq::util::is_little_endian() && "Only Little-Endian is Supported");
#endif
        const auto* start_src = src;
        // Only optimize for Little Endian. Big Endian falls back to scalar loop.
        while (src + 4 <= src_end) {
            uint64_t v;
            memcpy(&v, src, 8);

            // Check for non-ASCII: any high byte of u16 non-zero, or low byte > 0x7F
            // Mask 0xFF80 checks bit 7-15 of each char16.
            if ((v & 0xFF80FF80FF80FF80ULL) != 0) break;

            // Pack 4 chars: 00AA 00BB 00CC 00DD -> AA BB CC DD
            uint32_t out = static_cast<uint32_t>(v & 0xFF) |
                static_cast<uint32_t>((v >> 8) & 0xFF00) |
                static_cast<uint32_t>((v >> 16) & 0xFF0000) |
                static_cast<uint32_t>((v >> 24) & 0xFF000000);

            memcpy(dst, &out, 4);
            src += 4;
            dst += 4;
        }
        // Scalar tail
        while (src < src_end) {
            if (*src >= 0x80) break;
            *dst++ = static_cast<char>(*src++);
        }
        return static_cast<uint32_t>(src - start_src);
    }

#if defined(BQ_X86)
    // AVX2 Optimistic
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_utf16_to_utf8_ascii_optimistic_avx2(const char16_t* BQ_RESTRICT src_ptr, const char16_t* src_end, char* BQ_RESTRICT dst_ptr)
    {
        const char16_t* start_src = src_ptr;
        // 1. Try 64 chars (128 bytes)
        while (src_ptr + 64 <= src_end) {
            const auto v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr));
            const auto v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr + 16));
            const auto v2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr + 32));
            const auto v3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr + 48));

            const auto acc = _mm256_or_si256(_mm256_or_si256(v0, v1), _mm256_or_si256(v2, v3));
            BQ_UNLIKELY_IF (!_mm256_testz_si256(acc, _mm256_set1_epi16(static_cast<int16_t>(0xFF80u))))
            {
                return static_cast<uint32_t>(src_ptr - start_src);
            }

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr),      _mm256_permute4x64_epi64(_mm256_packus_epi16(v0, v1), 0xD8));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr + 32), _mm256_permute4x64_epi64(_mm256_packus_epi16(v2, v3), 0xD8));
            
            src_ptr += 64;
            dst_ptr += 64;
        }

        // 2. Try 32 chars (64 bytes)
        if (src_ptr + 32 <= src_end) {
            const auto v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr));
            const auto v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr + 16));
            
            const auto acc = _mm256_or_si256(v0, v1);
            BQ_UNLIKELY_IF (!_mm256_testz_si256(acc, _mm256_set1_epi16(static_cast<int16_t>(0xFF80u))))
            {
                 return static_cast<uint32_t>(src_ptr - start_src);
            }

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr), _mm256_permute4x64_epi64(_mm256_packus_epi16(v0, v1), 0xD8));
            src_ptr += 32;
            dst_ptr += 32;
        }

        // 3. Try 16 chars (32 bytes) - Single YMM load
        if (src_ptr + 16 <= src_end) {
            const auto v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr));
            BQ_UNLIKELY_IF (!_mm256_testz_si256(v0, _mm256_set1_epi16(static_cast<int16_t>(0xFF80u))))
            {
                 return static_cast<uint32_t>(src_ptr - start_src);
            }
            const auto compressed = _mm256_packus_epi16(v0, v0);
            const auto permuted = _mm256_permute4x64_epi64(compressed, 0xD8);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr), _mm256_castsi256_si128(permuted));
            src_ptr += 16;
            dst_ptr += 16;
        }

        if (src_ptr < src_end) {
            if (src_end - start_src >= 16) {
                const auto* tail_src = src_end - 16;
                auto* tail_dst = dst_ptr - (16 - (src_end - src_ptr));
                const auto v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(tail_src));
                if (!_mm256_testz_si256(v0, _mm256_set1_epi16(static_cast<int16_t>(0xFF80u)))) {
                     return static_cast<uint32_t>(src_ptr - start_src);
                }
                const auto compressed = _mm256_packus_epi16(v0, v0);
                const auto permuted = _mm256_permute4x64_epi64(compressed, 0xD8);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(tail_dst), _mm256_castsi256_si128(permuted));
                return static_cast<uint32_t>(src_end - start_src);
            }
            auto pre_processed = static_cast<uint32_t>(src_ptr - start_src);  //Maybe redundant
            return pre_processed + _impl_utf16_to_utf8_ascii_optimistic_sw(src_ptr, src_end, dst_ptr);
        }

        return static_cast<uint32_t>(src_ptr - start_src);
    }

    // SSE Optimistic
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_SSE_TARGET uint32_t _impl_utf16_to_utf8_ascii_optimistic_sse(const char16_t* BQ_RESTRICT src_ptr, const char16_t* src_end, char* BQ_RESTRICT dst_ptr)
    {
        const char16_t* start_src = src_ptr;

        // 1. Try 32 chars (64 bytes)
        while (src_ptr + 32 <= src_end) {
            const auto v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
            const auto v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr + 8));
            const auto v2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr + 16));
            const auto v3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr + 24));
            
            const auto acc = _mm_or_si128(_mm_or_si128(v0, v1), _mm_or_si128(v2, v3));
            const auto mask = _mm_set1_epi16(static_cast<int16_t>(0xFF80u));
            BQ_UNLIKELY_IF (!_mm_testz_si128(acc, mask)) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr),      _mm_packus_epi16(v0, v1));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr + 16), _mm_packus_epi16(v2, v3));

            src_ptr += 32;
            dst_ptr += 32;
        }

        // 2. Try 16 chars (32 bytes)
        if (src_ptr + 16 <= src_end) {
            const auto v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
            const auto v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr + 8));
            
            const auto mask = _mm_set1_epi16(static_cast<int16_t>(0xFF80u));
            BQ_UNLIKELY_IF (!_mm_testz_si128(_mm_or_si128(v0, v1), mask)) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr), _mm_packus_epi16(v0, v1));
            src_ptr += 16;
            dst_ptr += 16;
        }
        
        // 3. Try 8 chars (16 bytes)
        if (src_ptr + 8 <= src_end) {
            const auto v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
             const auto mask = _mm_set1_epi16(static_cast<int16_t>(0xFF80u));
            BQ_UNLIKELY_IF (!_mm_testz_si128(v0, mask)) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            _mm_storel_epi64(reinterpret_cast<__m128i*>(dst_ptr), _mm_packus_epi16(v0, v0));
            src_ptr += 8;
            dst_ptr += 8;
        }

        if (src_ptr < src_end) {
            if (src_end - start_src >= 8) {
                const auto* tail_src = src_end - 8;
                auto* tail_dst = dst_ptr - (8 - (src_end - src_ptr));
                const auto v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tail_src));
                const auto mask = _mm_set1_epi16(static_cast<int16_t>(0xFF80u));
                BQ_UNLIKELY_IF (!_mm_testz_si128(v0, mask)) {
                    return static_cast<uint32_t>(src_ptr - start_src);
                }
                _mm_storel_epi64(reinterpret_cast<__m128i*>(tail_dst), _mm_packus_epi16(v0, v0));
                return static_cast<uint32_t>(src_end - start_src);
            }
            auto pre_processed = static_cast<uint32_t>(src_ptr - start_src);  //Maybe redundant
            return pre_processed + _impl_utf16_to_utf8_ascii_optimistic_sw(src_ptr, src_end, dst_ptr);
        }

        return static_cast<uint32_t>(src_ptr - start_src);
    }
#elif defined(BQ_ARM_NEON)
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET  uint32_t _impl_utf16_to_utf8_ascii_optimistic_neon(const char16_t* BQ_RESTRICT src_ptr, const char16_t* src_end, char* BQ_RESTRICT dst_ptr) {
        const auto* start_src = src_ptr;

        // 1. Try 32 chars (64 bytes source)
        while (src_ptr + 32 <= src_end) {
            uint16x8_t v0 = vld1q_u16(reinterpret_cast<const uint16_t*>(src_ptr));
            uint16x8_t v1 = vld1q_u16(reinterpret_cast<const uint16_t*>(src_ptr + 8));
            uint16x8_t v2 = vld1q_u16(reinterpret_cast<const uint16_t*>(src_ptr + 16));
            uint16x8_t v3 = vld1q_u16(reinterpret_cast<const uint16_t*>(src_ptr + 24));

            uint16x8_t acc = vorrq_u16(vorrq_u16(v0, v1), vorrq_u16(v2, v3));
            BQ_UNLIKELY_IF(bq_vmaxvq_u16(acc) >= 0x0080) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }

            vst1q_u8(reinterpret_cast<uint8_t*>(dst_ptr), vcombine_u8(vmovn_u16(v0), vmovn_u16(v1)));
            vst1q_u8(reinterpret_cast<uint8_t*>(dst_ptr + 16), vcombine_u8(vmovn_u16(v2), vmovn_u16(v3)));
            src_ptr += 32;
            dst_ptr += 32;
        }

        // 2. Try 16 chars (32 bytes)
        if (src_ptr + 16 <= src_end) {
            uint16x8_t v0 = vld1q_u16(reinterpret_cast<const uint16_t*>(src_ptr));
            uint16x8_t v1 = vld1q_u16(reinterpret_cast<const uint16_t*>(src_ptr + 8));

            uint16x8_t acc = vorrq_u16(v0, v1);
            if (bq_vmaxvq_u16(acc) >= 0x0080) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            vst1q_u8(reinterpret_cast<uint8_t*>(dst_ptr), vcombine_u8(vmovn_u16(v0), vmovn_u16(v1)));
            src_ptr += 16;
            dst_ptr += 16;
        }

        // 3. Try 8 chars (16 bytes)
        if (src_ptr + 8 <= src_end) {
            uint16x8_t v0 = vld1q_u16(reinterpret_cast<const uint16_t*>(src_ptr));
            if (bq_vmaxvq_u16(v0) >= 0x0080) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            vst1_u8(reinterpret_cast<uint8_t*>(dst_ptr), vmovn_u16(v0));
            src_ptr += 8;
            dst_ptr += 8;
        }

        if (src_ptr < src_end) {
            if (src_end - start_src >= 8) {
                const auto* tail_src = src_end - 8;
                auto* tail_dst = dst_ptr - (8 - (src_end - src_ptr));
                uint16x8_t v0 = vld1q_u16(reinterpret_cast<const uint16_t*>(tail_src));
                BQ_UNLIKELY_IF(bq_vmaxvq_u16(v0) >= 0x0080) {
                    return static_cast<uint32_t>(src_ptr - start_src);
                }
                vst1_u8(reinterpret_cast<uint8_t*>(tail_dst), vmovn_u16(v0));
                return static_cast<uint32_t>(src_end - start_src);
            }
            auto pre_processed = static_cast<uint32_t>(src_ptr - start_src);  //Maybe redundant
            return pre_processed + _impl_utf16_to_utf8_ascii_optimistic_sw(src_ptr, src_end, dst_ptr);
        }
        return static_cast<uint32_t>(src_ptr - start_src);
    }
#endif

    BQ_SIMD_HW_INLINE uint32_t _impl_utf16_to_utf8_ascii_optimistic(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        const auto* src_ptr = src;
        const auto* src_end = src + src_character_num;
        char* dst_ptr = dst;
        (void)dst_character_num;
        (void)src_ptr;
        (void)src_end;
        (void)dst_ptr;
#if defined(BQ_X86)
        if (_bq_avx2_supported_) {
            return _impl_utf16_to_utf8_ascii_optimistic_avx2(src_ptr, src_end, dst_ptr);
        } 
        return _impl_utf16_to_utf8_ascii_optimistic_sse(src_ptr, src_end, dst_ptr);
#elif defined(BQ_ARM_NEON)
        return _impl_utf16_to_utf8_ascii_optimistic_neon(src_ptr, src_end, dst_ptr);
#else
        return _impl_utf16_to_utf8_ascii_optimistic_sw(src_ptr, src_end, dst_ptr);
#endif
    }

    // Updates src and dst pointers. Stops at first non-ASCII or end.
    static bq_forceinline uint32_t _impl_utf8_to_utf16_ascii_optimistic_sw(const uint8_t* BQ_RESTRICT& src, const uint8_t* src_end, char16_t* BQ_RESTRICT& dst)
    {
#ifndef NDEBUG
        assert(bq::util::is_little_endian() && "Only Little-Endian is Supported");
#endif
        // Process 8 bytes (chars) at a time -> expands to 16 bytes (8 char16_t)
        const auto* src_start = src;

        while (src + 8 <= src_end) {
            uint64_t v;
            memcpy(&v, src, 8);

            // Check high bit of each byte
            if ((v & 0x8080808080808080ULL) != 0) break;

            // Expand 8 chars to 8 char16_t
            // Input: AA BB CC DD EE FF GG HH
            // We need to insert 0x00 bytes.
            // Split into two 32-bit parts to handle easier
            uint32_t lo = static_cast<uint32_t>(v);
            uint32_t hi = static_cast<uint32_t>(v >> 32);

            // 00000000 00000000 00000000 AABBCCDD
            // Expand u32 to u64 with zeroes
            auto expand = [](uint32_t x) -> uint64_t {
                uint64_t t = x;
                // t = 00000000 00000000 00000000 AABBCCDD
                t = (t | (t << 16)) & 0x0000FFFF0000FFFFULL; // 00000000 AABB0000 0000CCDD
                t = (t | (t << 8)) & 0x00FF00FF00FF00FFULL; // 00AA00BB 00CC00DD
                return t;
                };

            uint64_t out_lo = expand(lo);
            uint64_t out_hi = expand(hi);

            memcpy(dst, &out_lo, 8);
            memcpy(dst + 4, &out_hi, 8);

            src += 8;
            dst += 8;
        }
        // Scalar tail
        while (src < src_end) {
            if (*src >= 0x80) break;
            *dst++ = static_cast<char16_t>(*src++);
        }
        return static_cast<uint32_t>(src - src_start);
    }

#if defined(BQ_X86)
    // AVX2 Optimistic
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_utf8_to_utf16_ascii_optimistic_avx2(const uint8_t* BQ_RESTRICT src_ptr, const uint8_t* src_end, char16_t* BQ_RESTRICT dst_ptr)
    {
        const auto* start_src = src_ptr;
        // 1. Try 64 chars
        while (src_ptr + 64 <= src_end) {
            const auto v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr));
            const auto v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr + 32));
            
            const auto acc = _mm256_or_si256(v0, v1);
            if (_mm256_movemask_epi8(acc)) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr),      _mm256_cvtepu8_epi16(_mm256_castsi256_si128(v0)));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr + 16), _mm256_cvtepu8_epi16(_mm256_extracti128_si256(v0, 1)));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr + 32), _mm256_cvtepu8_epi16(_mm256_castsi256_si128(v1)));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr + 48), _mm256_cvtepu8_epi16(_mm256_extracti128_si256(v1, 1)));
            
            src_ptr += 64;
            dst_ptr += 64;
        }

        // 2. Try 32 chars
        if (src_ptr + 32 <= src_end) {
            const auto chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src_ptr));
            if (_mm256_movemask_epi8(chunk)) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr), _mm256_cvtepu8_epi16(_mm256_castsi256_si128(chunk)));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr + 16), _mm256_cvtepu8_epi16(_mm256_extracti128_si256(chunk, 1)));
            src_ptr += 32;
            dst_ptr += 32;
        }

        // 3. Try 16 chars (SSE/XMM)
        if (src_ptr + 16 <= src_end) {
            const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
            if (_mm_movemask_epi8(chunk)) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst_ptr), _mm256_cvtepu8_epi16(chunk));
            src_ptr += 16;
            dst_ptr += 16;
        }

        if (src_ptr < src_end) {
            if (src_end - start_src >= 16) {
                //Overlapping Load
                const auto* tail_src = src_end - 16;
                auto* tail_dst = dst_ptr - (16 - (src_end - src_ptr));
                const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tail_src));
                if (_mm_movemask_epi8(chunk)) {
                    return static_cast<uint32_t>(src_ptr - start_src);
                }
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(tail_dst), _mm256_cvtepu8_epi16(chunk));
                return static_cast<uint32_t>(src_end - start_src);
            }
            auto pre_processed = static_cast<uint32_t>(src_ptr - start_src);  //Maybe redundant
            return pre_processed + _impl_utf8_to_utf16_ascii_optimistic_sw(src_ptr, src_end, dst_ptr);
        }
        return static_cast<uint32_t>(src_ptr - start_src);
    }

    // SSE Optimistic
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_SSE_TARGET uint32_t _impl_utf8_to_utf16_ascii_optimistic_sse(const uint8_t* BQ_RESTRICT src_ptr, const uint8_t* BQ_RESTRICT src_end, char16_t* BQ_RESTRICT dst_ptr)
    {
        const uint8_t* start_src = src_ptr;

        // 1. Try 32 chars
        while (src_ptr + 32 <= src_end) {
            const auto v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
            const auto v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr + 16));
            
            const auto acc = _mm_or_si128(v0, v1);
            if (_mm_movemask_epi8(acc)) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            
            // v0: 16 bytes -> 32 bytes (16 char16)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr),      _mm_cvtepu8_epi16(v0));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr + 8),  _mm_cvtepu8_epi16(_mm_srli_si128(v0, 8)));
            
            // v1: 16 bytes -> 32 bytes (16 char16)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr + 16), _mm_cvtepu8_epi16(v1));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr + 24), _mm_cvtepu8_epi16(_mm_srli_si128(v1, 8)));

            src_ptr += 32;
            dst_ptr += 32;
        }

        // 2. Try 16 chars
        if (src_ptr + 16 <= src_end) {
            const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src_ptr));
            if (_mm_movemask_epi8(chunk)) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr),      _mm_cvtepu8_epi16(chunk));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst_ptr + 8),  _mm_cvtepu8_epi16(_mm_srli_si128(chunk, 8)));
            src_ptr += 16;
            dst_ptr += 16;
        }

        if (src_ptr < src_end) {
            if (src_end - start_src >= 16) {
                const auto* tail_src = src_end - 16;
                auto* tail_dst = dst_ptr - (16 - (src_end - src_ptr));
                const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tail_src));
                if (_mm_movemask_epi8(chunk)) {
                    return static_cast<uint32_t>(src_ptr - start_src);
                }
                _mm_storeu_si128(reinterpret_cast<__m128i*>(tail_dst),      _mm_cvtepu8_epi16(chunk));
                _mm_storeu_si128(reinterpret_cast<__m128i*>(tail_dst + 8),  _mm_cvtepu8_epi16(_mm_srli_si128(chunk, 8)));
                return static_cast<uint32_t>(src_end - start_src);
            }
            auto pre_processed = static_cast<uint32_t>(src_ptr - start_src);  //Maybe redundant
            return pre_processed + _impl_utf8_to_utf16_ascii_optimistic_sw(src_ptr, src_end, dst_ptr);
        }

        return static_cast<uint32_t>(src_ptr - start_src);
    }
#elif defined(BQ_ARM_NEON)
    BQ_SIMD_HW_INLINE BQ_HW_SIMD_TARGET uint32_t _impl_utf8_to_utf16_ascii_optimistic_neon(const uint8_t* BQ_RESTRICT src_ptr, const uint8_t* BQ_RESTRICT src_end, char16_t* BQ_RESTRICT dst_ptr) {
        const auto* start_src = src_ptr;
        
        // 1. Try 64 chars
        while (src_ptr + 64 <= src_end) {
            uint8x16_t v0 = vld1q_u8(src_ptr);
            uint8x16_t v1 = vld1q_u8(src_ptr + 16);
            uint8x16_t v2 = vld1q_u8(src_ptr + 32);
            uint8x16_t v3 = vld1q_u8(src_ptr + 48);

            uint8x16_t acc = vorrq_u8(vorrq_u8(v0, v1), vorrq_u8(v2, v3));
            if (bq_vmaxvq_u8(acc) >= 0x80) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }

            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr), vmovl_u8(vget_low_u8(v0)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 8), vmovl_u8(vget_high_u8(v0)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 16), vmovl_u8(vget_low_u8(v1)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 24), vmovl_u8(vget_high_u8(v1)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 32), vmovl_u8(vget_low_u8(v2)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 40), vmovl_u8(vget_high_u8(v2)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 48), vmovl_u8(vget_low_u8(v3)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 56), vmovl_u8(vget_high_u8(v3)));

            src_ptr += 64;
            dst_ptr += 64;
        }

        // 2. Try 32 chars
        if (src_ptr + 32 <= src_end) {
            uint8x16_t v0 = vld1q_u8(src_ptr);
            uint8x16_t v1 = vld1q_u8(src_ptr + 16);

            uint8x16_t acc = vorrq_u8(v0, v1);
            if (bq_vmaxvq_u8(acc) >= 0x80) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }

            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr), vmovl_u8(vget_low_u8(v0)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 8), vmovl_u8(vget_high_u8(v0)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 16), vmovl_u8(vget_low_u8(v1)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 24), vmovl_u8(vget_high_u8(v1)));

            src_ptr += 32;
            dst_ptr += 32;
        }

        // 3. Try 16 chars
        if (src_ptr + 16 <= src_end) {
            uint8x16_t v0 = vld1q_u8(src_ptr);
            if (bq_vmaxvq_u8(v0) >= 0x80) {
                return static_cast<uint32_t>(src_ptr - start_src);
            }
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr), vmovl_u8(vget_low_u8(v0)));
            vst1q_u16(reinterpret_cast<uint16_t*>(dst_ptr + 8), vmovl_u8(vget_high_u8(v0)));
            src_ptr += 16;
            dst_ptr += 16;
        }

        if (src_ptr < src_end) {
            if (src_end - start_src >= 16) {
                const auto* tail_src = src_end - 16;
                auto* tail_dst = dst_ptr - (16 - (src_end - src_ptr));
                uint8x16_t v0 = vld1q_u8(tail_src);
                if (bq_vmaxvq_u8(v0) >= 0x80) {
                    return static_cast<uint32_t>(src_ptr - start_src);
                }
                vst1q_u16(reinterpret_cast<uint16_t*>(tail_dst), vmovl_u8(vget_low_u8(v0)));
                vst1q_u16(reinterpret_cast<uint16_t*>(tail_dst + 8), vmovl_u8(vget_high_u8(v0)));
                return static_cast<uint32_t>(src_end - start_src);
            }
            auto pre_processed = static_cast<uint32_t>(src_ptr - start_src);  //Maybe redundant
            return pre_processed + _impl_utf8_to_utf16_ascii_optimistic_sw(src_ptr, src_end, dst_ptr);
        }
        return static_cast<uint32_t>(src_ptr - start_src);
    }
#endif

    BQ_SIMD_HW_INLINE uint32_t _impl_utf8_to_utf16_ascii_optimistic(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        const auto* src_ptr = reinterpret_cast<const uint8_t*>(src);
        const auto* src_end = src_ptr + src_character_num;
        char16_t* dst_ptr = dst;
        (void)dst_character_num;
        (void)src_ptr;
        (void)src_end;
        (void)dst_ptr;
#if defined(BQ_X86)
        if (_bq_avx2_supported_) {
            return _impl_utf8_to_utf16_ascii_optimistic_avx2(src_ptr, src_end, dst_ptr);
        } 
        return _impl_utf8_to_utf16_ascii_optimistic_sse(src_ptr, src_end, dst_ptr);
#elif defined(BQ_ARM_NEON)
        return _impl_utf8_to_utf16_ascii_optimistic_neon(src_ptr, src_end, dst_ptr);
#else
        return _impl_utf8_to_utf16_ascii_optimistic_sw(src_ptr, src_end, dst_ptr);
#endif
    }

    uint32_t util::utf16_to_utf8_fast(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        // Threshold lowered to 8 to catch small strings on NEON/SSE
        if (src_character_num >= 8 && _bq_utf_simd_supported_) {
            // Try optimistic ASCII first
            const uint32_t converted = _impl_utf16_to_utf8_ascii_optimistic(src, src_character_num, dst, dst_character_num);
            if (converted == src_character_num) {
                return converted;
            }
            // Continue from where we failed with safe SIMD
            return converted + _impl_utf16_to_utf8_simd(src + converted, src_character_num - converted, dst + converted, dst_character_num - converted);
        }
        return _impl_utf16_to_utf8_scalar_fast(src, src_character_num, dst, dst_character_num);
    }

    uint32_t util::utf8_to_utf16_fast(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        // Threshold lowered to 16 to catch small strings
        if (src_character_num >= 16 && _bq_utf_simd_supported_) {
            const uint32_t converted = _impl_utf8_to_utf16_ascii_optimistic(src, src_character_num, dst, dst_character_num);
             if (converted == src_character_num) {
                 return converted;
             }
             return converted + _impl_utf8_to_utf16_simd(src + converted, src_character_num - converted, dst + converted, dst_character_num - converted);
        }
        return _impl_utf8_to_utf16_scalar_fast(src, src_character_num, dst, dst_character_num);
    }


    uint32_t util::utf16_to_utf8_ascii_fast(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
#ifndef NDEBUG
        assert(dst_character_num >= src_character_num);
#endif
        return  _impl_utf16_to_utf8_ascii_optimistic(src, src_character_num, dst, dst_character_num);
    }

    uint32_t util::utf8_to_utf16_ascii_fast(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
#ifndef NDEBUG
        assert(dst_character_num >= src_character_num);
#endif
        return _impl_utf8_to_utf16_ascii_optimistic(src, src_character_num, dst, dst_character_num);
    }

    // =================================================================================================
    // Legacy Implementation (State Machine)
    // =================================================================================================

    uint32_t util::utf16_to_utf8(const char16_t* BQ_RESTRICT src_utf16_str, uint32_t src_character_num, char* BQ_RESTRICT dst_utf8_str, uint32_t dst_character_num)
    {
        uint32_t result_len = 0;
        uint32_t codepoint = 0;
        uint32_t surrogate = 0;

        uint32_t wchar_len = src_character_num;

        const char16_t* s = src_utf16_str;
        const char16_t* p = s;

        while (static_cast<uint32_t>(p - s) < wchar_len) {
            char16_t c = *p++;
            if (c == 0) break;
            if (surrogate) {
                if (c >= 0xDC00 && c <= 0xDFFF) {
                    codepoint = static_cast<uint32_t>(0x10000u) + (static_cast<uint32_t>(c) - static_cast<uint32_t>(0xDC00u)) + static_cast<uint32_t>((surrogate - static_cast<uint32_t>(0xD800)) << 10);
                    surrogate = 0;
                }
                else {
                    surrogate = 0;
                    continue;
                }
            }
            else {
                if (c < 0x80) {
                    dst_utf8_str[result_len++] = static_cast<char>(c);

                    while (static_cast<uint32_t>(p - s) < wchar_len && *p && *p < 0x80) {
                        dst_utf8_str[result_len++] = static_cast<char>(*p++);
                    }

                    continue;
                }
                else if (c >= 0xD800 && c <= 0xDBFF) {
                    surrogate = c;
                }
                else if (c >= 0xDC00 && c <= 0xDFFF) {
                    continue;
                }
                else {
                    codepoint = c;
                }
            }

            if (surrogate != 0)
                continue;

            if (codepoint < 0x80) {
                dst_utf8_str[result_len++] = static_cast<char>(codepoint);
            }
            else if (codepoint < 0x0800) {
                dst_utf8_str[result_len++] = static_cast<char>(0xC0 | (codepoint >> 6));
                dst_utf8_str[result_len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
            }
            else if (codepoint < 0x10000) {
                dst_utf8_str[result_len++] = static_cast<char>(0xE0 | (codepoint >> 12));
                dst_utf8_str[result_len++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                dst_utf8_str[result_len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
            }
            else {
                dst_utf8_str[result_len++] = static_cast<char>(0xF0 | (codepoint >> 18));
                dst_utf8_str[result_len++] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                dst_utf8_str[result_len++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                dst_utf8_str[result_len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
            }
        }
        assert(result_len <= dst_character_num && "target buffer of utf16_to_utf8 is not enough!");
        return result_len;
    }

    uint32_t util::utf8_to_utf16(const char* BQ_RESTRICT src_utf8_str, uint32_t src_character_num, char16_t* BQ_RESTRICT dst_utf16_str, uint32_t dst_character_num)
    {
        uint32_t result_len = 0;
        uint32_t mb_size = 0;
        uint32_t mb_remain = 0;

        uint32_t codepoint = 0;

        const char* p = (const char*)src_utf8_str;
        while (static_cast<uint32_t>(p - src_utf8_str) < src_character_num) {
            const auto c = static_cast<char16_t>(static_cast<uint8_t>(*p++));
            if (c == 0) break;
            if (mb_size == 0) {
                if (c < 0x80) {
                    dst_utf16_str[result_len++] = c;
                }
                else if ((c & 0xE0) == 0xC0) {
                    codepoint = c & 0x1F;
                    mb_size = 2;
                }
                else if ((c & 0xF0) == 0xE0) {
                    codepoint = c & 0x0F;
                    mb_size = 3;
                }
                else if ((c & 0xF8) == 0xF0) {
                    codepoint = c & 7;
                    mb_size = 4;
                }
                else if ((c & 0xFC) == 0xF8) {
                    codepoint = c & 3;
                    mb_size = 5;
                }
                else if ((c & 0xFE) == 0xFC) {
                    codepoint = c & 3;
                    mb_size = 6;
                }
                else {
                    codepoint = mb_remain = mb_size = 0;
                }

                if (mb_size > 1)
                    mb_remain = mb_size - 1;

            }
            else {
                if ((c & 0xC0) == 0x80) {
                    codepoint = (codepoint << 6) | (c & 0x3F);

                    if (--mb_remain == 0) {
                        if (codepoint < 0x10000) {
                            dst_utf16_str[result_len++] = static_cast<char16_t>(codepoint % 0x10000);
                        }
                        else if (codepoint < 0x110000) {
                            codepoint -= 0x10000;
                            dst_utf16_str[result_len++] = static_cast<char16_t>((codepoint >> 10) + 0xD800);
                            dst_utf16_str[result_len++] = static_cast<char16_t>((codepoint & 0x3FF) + 0xDC00);
                        }
                        else {
                            codepoint = mb_remain = 0;
                        }
                        mb_size = 0;
                    }
                }
                else {
                    codepoint = mb_remain = mb_size = 0;
                }
            }
        }
        assert(result_len <= dst_character_num && "target buffer of utf8_to_utf16 is not enough!");
        return result_len;
    }

    uint32_t util::utf16_to_utf_mixed(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        auto written_character_num = utf16_to_utf8_ascii_fast(src, src_character_num, dst, dst_character_num);
        BQ_UNLIKELY_IF (written_character_num >= dst_character_num) {
            assert(false && "utf16_to_utf_mixed dst space utf8 not enough");
        }
        dst[written_character_num] = static_cast<char>(0xFF);
        BQ_UNLIKELY_IF(written_character_num < src_character_num) {
            auto left_chars = src_character_num - written_character_num;
            auto left_space = dst_character_num - (written_character_num + 1);
            BQ_UNLIKELY_IF(left_space < left_chars * sizeof(char16_t)) {
                assert(false && "utf16_to_utf_mixed dst space utf16 not enough");
            }
            memcpy(dst + written_character_num + 1, src + written_character_num, left_chars * sizeof(char16_t));
            written_character_num += static_cast<uint32_t>(left_chars * sizeof(char16_t));
        }
        return written_character_num + 1;
    }

    using mixed_aligned_utf16_buffer_type = bq::array<char, bq::aligned_allocator<char, 8>>;
    BQ_TLS_NON_POD(mixed_aligned_utf16_buffer_type, mixed_utf16_buffer_)
    uint32_t util::utf_mixed_to_utf8(const char* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        auto mark_ptr = memchr(static_cast<const void*>(src), 0xFF, static_cast<size_t>(src_character_num));
        BQ_UNLIKELY_IF(!mark_ptr) {
            assert(false && "utf_mixed_to_utf8 invalid src");
        }
        auto utf8_len = static_cast<uint32_t>(static_cast<const char*>(mark_ptr) - src);
        BQ_UNLIKELY_IF(utf8_len > dst_character_num) {
            assert(false && "utf_mixed_to_utf8 dst space utf8 not enough");
        }
        memcpy(dst, src, utf8_len);
        auto left_space = static_cast<uint32_t>(dst_character_num - utf8_len);
        auto left_characters = src_character_num - utf8_len - 1;
        const char* utf16_start = src + utf8_len + 1;
        if ((reinterpret_cast<uintptr_t>(utf16_start) & static_cast<uintptr_t>(0x01)) != 0) {
            mixed_utf16_buffer_.get().clear();
            mixed_utf16_buffer_.get().fill_uninitialized(left_characters);
            memcpy(mixed_utf16_buffer_.get().begin(), src + utf8_len + 1, left_characters);
            utf16_start = static_cast<const char*>(mixed_utf16_buffer_.get().begin());
        }
        utf8_len += utf16_to_utf8_fast(reinterpret_cast<const char16_t*>(utf16_start), static_cast<uint32_t>(left_characters >> 1) /* / sizeof(char16_t) */, dst + utf8_len, left_space);
        if (mixed_utf16_buffer_.get().size() > 256) {
            mixed_utf16_buffer_.get().clear();
            mixed_utf16_buffer_.get().set_capacity(256, true);
        }
        return utf8_len;
    }


#ifdef BQ_UNIT_TEST
    uint32_t util::utf16_to_utf8_fast_sw(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        return _impl_utf16_to_utf8_scalar_fast(src, src_character_num, dst, dst_character_num);
    }
    uint32_t util::utf8_to_utf16_fast_sw(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        return _impl_utf8_to_utf16_scalar_fast(src, src_character_num, dst, dst_character_num);
    }
#if defined(BQ_X86)
    uint32_t util::utf16_to_utf8_fast_sse(const char16_t* BQ_RESTRICT src, uint32_t src_character_num, char* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        (void)dst_character_num;
        const auto* src_ptr = src;
        const auto* src_end = src + src_character_num;
        const uint32_t converted = _impl_utf16_to_utf8_ascii_optimistic_sse(src_ptr, src_end, dst);
        if (converted == src_character_num) {
            return converted;
        }
        src_ptr = src + converted;
        return converted + _impl_utf16_to_utf8_simd_sse(src_ptr, src_end, dst + converted);
    }
    uint32_t util::utf8_to_utf16_fast_sse(const char* BQ_RESTRICT src, uint32_t src_character_num, char16_t* BQ_RESTRICT dst, uint32_t dst_character_num)
    {
        (void)dst_character_num;
        // Threshold lowered to 16 to catch small strings
        const auto* src_ptr = reinterpret_cast<const uint8_t*>(src);
        const auto* src_end = src_ptr + src_character_num;
        const uint32_t converted = _impl_utf8_to_utf16_ascii_optimistic_sse(src_ptr, src_end, dst);
        if (converted == src_character_num) {
            return converted;
        }
        src_ptr = reinterpret_cast<const uint8_t*>(src) + converted;
        return converted + _impl_utf8_to_utf16_simd_sse(src_ptr, src_end, dst + converted);
    }
#endif
#endif
}