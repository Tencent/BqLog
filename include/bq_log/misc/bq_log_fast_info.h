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
            // the element type fixed_format picked for the literal; it fixes the encoding
            using char_type = typename decltype(F)::storage_type;
            static constexpr size_t char_count = decltype(F)::length;
            // the literal as do_log() wants it on the fallback path
            using literal_type = char_type[char_count];
            // the log_arg_type_enum the format is registered with; same mapping as the macro path
            static constexpr bq::log_arg_type_enum arg_type =
                bq::tools::_is_bq_log_format_type<literal_type>::arg_type;
            // type_copy writes a 4-byte length and then the normalized bytes: one byte per code
            // unit for UTF-8, and never more than the literal's own byte size for the wider forms
            // (a 4-byte code point becomes at most two 2-byte units), so this always has room
            static constexpr size_t buffer_size = sizeof(char_type) * char_count + sizeof(uint32_t);
            static_assert(buffer_size < UINT32_MAX, "fast_*: format string literal is too long");
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

        // v = meta_addr | (1u << level) once registered, 0 while unregistered. The meta arena entry
        // is 64-byte aligned, so the low 6 bits are free for the enabled-level bit; publishing the
        // address and the bit in ONE store keeps the hot path free of any acquire (a relaxed read
        // of v is a plain load everywhere) and removes the two-field publication order problem
        // entirely: there is no "half registered" state to observe. Read-read coherence on this
        // single atomic object then guarantees the value used to write the record head is the same
        // non-zero value the branch tested.
        struct alignas(8) fast_info_site {
            uint64_t v;
        };

        template <fixed_format F, uint8_t LEVEL, uint32_t CAT, uint8_t... WIRE>
        struct BQ_FAST_INFO_HIDDEN fast_info_site_holder {
            static fast_info_site site;
        };
        template <fixed_format F, uint8_t LEVEL, uint32_t CAT, uint8_t... WIRE>
        fast_info_site fast_info_site_holder<F, LEVEL, CAT, WIRE...>::site = { 0 };

        bq_forceinline uint64_t load_site_v(const fast_info_site& site)
        {
            return BQ_PACK_ACCESS_BY_TYPE(site.v, bq::platform::atomic<uint64_t>).load_relaxed();
        }

        // release so a thread that later observes a non-zero v is ordered after the registration's
        // writes (the store only ever publishes the final value once, keeping it simple)
        bq_forceinline void store_site_v(fast_info_site& site, uint64_t v)
        {
            BQ_PACK_ACCESS_BY_TYPE(site.v, bq::platform::atomic<uint64_t>).store_release(v);
        }

        template <fixed_format F, bq::log_level LEVEL, uint32_t CAT, typename... Args>
        BQ_FAST_INFO_HIDDEN bq_noinline bool fast_slow(fast_info_site& site, const bq::log& l, typename bq::fast_slow_arg<Args>::type... args);

        template <fixed_format F, bq::log_level LEVEL, uint32_t CAT, typename... Args>
        bq_forceinline bool fast_impl(const bq::log& l, const Args&... args)
        {
            auto& site = fast_info_site_holder<F, static_cast<uint8_t>(LEVEL), CAT, fast_wire_of<Args>()...>::site;
            const uint64_t v = load_site_v(site);
            const uint32_t word = log_access::level_words(l)[CAT];
            // no arguments: the record is just the head, so nothing to size or fill
            if constexpr (sizeof...(Args) == 0) {
                // uint8_t(word & v): the low byte of the AND holds (word's level bits) & (v's level
                // bits). word's bits 6 and 7 are always 0, so an address bit above bit 5 can never
                // make the test true; masking v down to uint8_t instead costs an extra insn on x86.
                BQ_LIKELY_IF(static_cast<uint8_t>(word & v) != 0)
                {
                    const int32_t r = bq::fast_inline::write(log_access::buffer_id(l),
                        bq::fast_inline::fast_meta_by_site_v { &site.v }, 0, [](uint8_t*) { });
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
                BQ_LIKELY_IF(static_cast<uint8_t>(word & v) != 0 && size_seq.get_total() <= UINT32_MAX - sizeof(bq::log_head_fast_def))
                {
                    const int32_t r = bq::fast_inline::write(log_access::buffer_id(l),
                        bq::fast_inline::fast_meta_by_site_v { &site.v },
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
        BQ_FAST_INFO_HIDDEN bq_noinline bool fast_slow(fast_info_site& site, const bq::log& l, typename bq::fast_slow_arg<Args>::type... args)
        {
            // an invalid log has no bitmaps; never touch them
            if (log_access::log_id(l) == 0) {
                return false;
            }
            if (!l.is_enabled_for(CAT, LEVEL)) {
                return false;
            }
            using traits = fast_format_traits<F>;
            const typename traits::char_type (&format)[traits::char_count] = fixed_format_chars<F>::data.value;
            if (log_access::buffer_id(l) == 0 || l.is_stack_trace_enabled_for(LEVEL)) {
                return l.do_log(CAT, LEVEL, format, args...);
            }
            if (load_site_v(site) == 0) {
                bq::fast_inline::site_init_guard guard(&site);
                if (load_site_v(site) == 0) {
                    // normalized the way init_fast_log_site does it: type_copy writes a 4-byte
                    // length and then the UTF-8/UTF-16 bytes the registry stores
                    alignas(8) uint8_t format_bytes[traits::buffer_size];
                    const size_t format_size = bq::tools::_serialize_str_helper_by_type<typename traits::literal_type>::get_storage_data_size(format);
                    bq::tools::_serialize_str_helper_by_type<typename traits::literal_type>::type_copy(
                        format, format_bytes, format_size + sizeof(uint32_t));
                    using wire = fast_wire_types<fast_wire_of<Args>()...>;
                    const uint64_t meta = bq::api::__api_fast_meta_register(static_cast<uint8_t>(LEVEL), CAT,
                        static_cast<uint8_t>(traits::arg_type),
                        reinterpret_cast<const char*>(format_bytes + sizeof(uint32_t)),
                        static_cast<uint32_t>(format_size), wire::count ? wire::values : nullptr, wire::count);
                    if (meta == 0) {
                        return l.do_log(CAT, LEVEL, format, args...);
                    }
                    // one store publishes both the arena address and the enabled-level bit
                    store_site_v(site, meta | (UINT64_C(1) << static_cast<uint32_t>(LEVEL)));
                }
            }
            const uint64_t meta_addr = load_site_v(site) & ~bq::fast_meta_addr_mask;
            // same shape as the macro slow path, but the buffer is found by log id
            if constexpr (sizeof...(Args) == 0) {
                if (!bq::api::__api_fast_log_write_no_args_by_id(log_access::log_id(l), meta_addr)) {
                    return l.do_log(CAT, LEVEL, format);
                }
            } else {
                auto size_seq = bq::tools::make_size_seq<false>(args...);
                if (size_seq.get_total() > UINT32_MAX - sizeof(bq::log_head_fast_def)) {
                    return l.do_log(CAT, LEVEL, format, args...);
                }
                auto handle = bq::api::__api_fast_log_write_begin_by_id(log_access::log_id(l), meta_addr,
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

#undef BQ_FAST_INFO_HIDDEN

#endif
