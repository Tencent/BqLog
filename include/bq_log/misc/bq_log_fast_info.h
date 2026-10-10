/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
// C++20 only: fast_info<"...">(args...), the macro-free fast log interface. The format string is
// a non-type template parameter, so a site is shared by (format, level, category, wire types) and
// the level and buffer come from the log passed to each call: any log works.
#if defined(BQ_CPP_20)
#include "bq_log/misc/bq_log_fast_inline.h"

namespace bq {
    namespace fast_info_detail {
        // defined here (not in class log) so log can friend it: it reads the handle's private fields
        struct log_access {
            static bq_forceinline const uint32_t* level_words(const bq::log& l) { return l.category_level_words_; }
            static bq_forceinline uint64_t buffer_id(const bq::log& l) { return l.buffer_id_; }
            static bq_forceinline uint64_t log_id(const bq::log& l) { return l.log_id_; }
        };

        template <fixed_format F>
        struct fast_format_traits {
            // the literal's trailing '\0' is not part of the format, same as the macro path
            static constexpr uint32_t size = static_cast<uint32_t>(sizeof(F.chars) - 1);
        };

        template <uint8_t... WIRE>
        struct fast_wire_types {
            static constexpr uint16_t count = static_cast<uint16_t>(sizeof...(WIRE));
            static constexpr uint8_t values[sizeof...(WIRE) + 1] = { WIRE..., 0 };
        };

        template <typename T>
        constexpr uint8_t fast_wire_of()
        {
            return static_cast<uint8_t>(bq::tools::_get_log_param_type_enum<bq::decay_t<T>>());
        }

        // meta address 0 until registered; mask 0 until registered, then 1u << level, published last
        struct alignas(16) fast_info_site {
            uint64_t meta_addr;
            uint32_t mask;
            uint32_t reserved;
        };

        template <fixed_format F, uint8_t LEVEL, uint32_t CAT, uint8_t... WIRE>
        struct fast_info_site_holder {
            static fast_info_site site;
        };
        template <fixed_format F, uint8_t LEVEL, uint32_t CAT, uint8_t... WIRE>
        fast_info_site fast_info_site_holder<F, LEVEL, CAT, WIRE...>::site = { 0, 0, 0 };

        // The publication field: written with release in the slow path. The hot path reads it as
        // a plain 32-bit load. That is race-free in practice: the field is naturally aligned, no
        // target tears a 32-bit access, and a stale 0 only sends the call to the slow path, which
        // is correct. Reading it with an atomic load instead costs one extra instruction in the
        // fast path for no behavioral gain.
        bq_forceinline uint32_t load_mask(const fast_info_site& site)
        {
            return site.mask;
        }

        bq_forceinline uint32_t load_mask_acquire(const fast_info_site& site)
        {
            return BQ_PACK_ACCESS_BY_TYPE(site.mask, bq::platform::atomic<uint32_t>).load_acquire();
        }

        bq_forceinline void store_mask_release(fast_info_site& site, uint32_t mask)
        {
            BQ_PACK_ACCESS_BY_TYPE(site.mask, bq::platform::atomic<uint32_t>).store_release(mask);
        }

        // reset_config writes level words from another thread, so the hot path reads the word
        // through the atomic type. load_raw is the codebase's barrier-free accessor: it is a
        // naturally aligned 32-bit load that no target tears, which is all this needs (a stale
        // value only delays a level change, it cannot produce a wrong record). A load_relaxed
        // costs one more instruction here, and on x86/ARM64 both compile to the same plain mov.
        bq_forceinline uint32_t load_level_word_relaxed(const uint32_t& word)
        {
            return BQ_PACK_ACCESS_BY_TYPE(word, bq::platform::atomic<uint32_t>).load_raw();
        }

        template <fixed_format F, bq::log_level LEVEL, uint32_t CAT, typename... Args>
        bq_noinline bool fast_slow(fast_info_site& site, const bq::log& l, typename bq::fast_slow_arg<Args>::type... args);

        template <fixed_format F, bq::log_level LEVEL, uint32_t CAT, typename... Args>
        bq_forceinline bool fast_impl(const bq::log& l, const Args&... args)
        {
            auto& site = fast_info_site_holder<F, static_cast<uint8_t>(LEVEL), CAT, fast_wire_of<Args>()...>::site;
            const uint32_t mask = load_mask(site);
            const uint32_t word = load_level_word_relaxed(log_access::level_words(l)[CAT]);
            // no arguments: the record is just the head, so nothing to size or fill
            if constexpr (sizeof...(Args) == 0) {
                BQ_LIKELY_IF((word & mask) != 0)
                {
                    const int32_t r = bq::fast_inline::write(log_access::buffer_id(l),
                        bq::fast_inline::fast_meta_by_ptr { &site.meta_addr }, 0, [](uint8_t*) { });
                    BQ_UNLIKELY_IF(r > 0)
                    {
                        bq::api::__api_fast_log_notify_low_space_by_id(log_access::log_id(l));
                    }
                    BQ_LIKELY_IF(r >= 0)
                    {
                        return true;
                    }
                }
                return fast_slow<F, LEVEL, CAT>(site, l);
            } else {
                const auto size_seq = bq::tools::make_size_seq<false>(args...);
                BQ_LIKELY_IF((word & mask) != 0 && size_seq.get_total() <= UINT32_MAX - sizeof(bq::log_head_fast_def))
                {
                    const int32_t r = bq::fast_inline::write(log_access::buffer_id(l),
                        bq::fast_inline::fast_meta_by_ptr { &site.meta_addr },
                        static_cast<uint32_t>(size_seq.get_total()),
                        [&](uint8_t* p) { bq::impl::_do_log_args_fill<false>(p, size_seq, args...); });
                    BQ_UNLIKELY_IF(r > 0)
                    {
                        bq::api::__api_fast_log_notify_low_space_by_id(log_access::log_id(l));
                    }
                    BQ_LIKELY_IF(r >= 0)
                    {
                        return true;
                    }
                }
                return fast_slow<F, LEVEL, CAT, Args...>(site, l, typename bq::fast_slow_arg<Args>::type(args)...);
            }
        }

        template <fixed_format F, bq::log_level LEVEL, uint32_t CAT, typename... Args>
        bq_noinline bool fast_slow(fast_info_site& site, const bq::log& l, typename bq::fast_slow_arg<Args>::type... args)
        {
            // an invalid log has no bitmaps; never touch them
            if (log_access::log_id(l) == 0) {
                return false;
            }
            if (!l.is_enabled_for(CAT, LEVEL)) {
                return false;
            }
            const char (&format)[sizeof(F.chars)] = F.chars;
            if (log_access::buffer_id(l) == 0 || l.is_stack_trace_enabled_for(LEVEL)) {
                return l.do_log(CAT, LEVEL, format, args...);
            }
            if (load_mask_acquire(site) == 0) {
                bq::fast_inline::site_init_guard guard(&site);
                if (site.mask == 0) {
                    using wire = fast_wire_types<fast_wire_of<Args>()...>;
                    const uint64_t meta = bq::api::__api_fast_meta_register(static_cast<uint8_t>(LEVEL), CAT,
                        static_cast<uint8_t>(bq::log_arg_type_enum::string_utf8_type),
                        F.chars, fast_format_traits<F>::size, wire::count ? wire::values : nullptr, wire::count);
                    if (meta == 0) {
                        return l.do_log(CAT, LEVEL, format, args...);
                    }
                    site.meta_addr = meta;
                    store_mask_release(site, 1U << static_cast<uint32_t>(LEVEL));
                }
            }
            // same shape as the macro slow path, but the buffer is found by log id
            if constexpr (sizeof...(Args) == 0) {
                if (!bq::api::__api_fast_log_write_no_args_by_id(log_access::log_id(l), site.meta_addr)) {
                    return l.do_log(CAT, LEVEL, format);
                }
            } else {
                auto size_seq = bq::tools::make_size_seq<false>(args...);
                if (size_seq.get_total() > UINT32_MAX - sizeof(bq::log_head_fast_def)) {
                    return l.do_log(CAT, LEVEL, format, args...);
                }
                auto handle = bq::api::__api_fast_log_write_begin_by_id(log_access::log_id(l), site.meta_addr,
                    static_cast<uint32_t>(size_seq.get_total()));
                if (handle.result != bq::enum_buffer_result_code::success) {
                    return l.do_log(CAT, LEVEL, format, args...);
                }
                bq::impl::_do_log_args_fill<false>(handle.args_addr, size_seq, args...);
                bq::api::__api_fast_log_write_finish_by_id(log_access::log_id(l), handle.args_addr);
            }
            bq::fast_inline::refresh_thread_slot();
            return true;
        }
    }
}

#endif
