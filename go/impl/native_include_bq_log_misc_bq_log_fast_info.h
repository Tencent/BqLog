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
#include "native_include_bq_log_misc_bq_log_fast_inline.h"

namespace bq {
    BQ_MODULE_EXPORT_BEGIN
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
            static constexpr bq::log_arg_type_enum arg_type = bq::tools::_is_bq_log_format_type<literal_type>::arg_type;
            static_assert(char_count <= UINT32_MAX, "fast_*: format string literal is too long");
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
        // address and the bit in ONE store removes the two-field publication order problem
        // entirely: there is no "half registered" state to observe. Read-read coherence on this
        // single atomic object then guarantees the value used to write the record head is the same
        // non-zero value the branch tested.
        struct alignas(8) fast_info_site {
            uint64_t v;
        };

        constexpr uint64_t fast_site_refused = UINT64_C(1) << 63;

        struct fast_info_site_desc {
            uint32_t category;
            // the literal's length as an array format has it: N - 1 when NUL terminated, else N
            uint32_t format_units;
            uint8_t level;
        };

        template <typename CHAR, size_t N>
        struct fast_info_site_desc_storage {
            fast_info_site_desc head;
            CHAR format[N];
        };

        // `value` first: a site's address is still the handle the inline path reads
        struct alignas(8) fast_info_site_cell {
            fast_info_site value;
            const fast_info_site_desc* desc;
        };

        template <typename CHAR>
        struct fast_format_view {
            using value_type = CHAR;
            using size_type = size_t;
            const CHAR* chars;
            size_t units;
            const value_type* data() const { return chars; }
            size_type size() const { return units; }
        };

        bq_forceinline const fast_info_site_desc& site_desc(const fast_info_site& site)
        {
            return *reinterpret_cast<const fast_info_site_cell*>(&site)->desc;
        }

        template <typename CHAR>
        bq_forceinline fast_format_view<CHAR> desc_format(const fast_info_site_desc& desc)
        {
            return fast_format_view<CHAR> { reinterpret_cast<const CHAR*>(reinterpret_cast<const uint8_t*>(&desc) + sizeof(fast_info_site_desc)),
                desc.format_units };
        }

        template <fixed_format F, uint8_t LEVEL, uint32_t CAT, uint8_t... WIRE>
        struct BQ_FAST_INFO_HIDDEN fast_info_site_holder {
            using char_type = typename decltype(F)::storage_type;
            static constexpr size_t char_count = decltype(F)::length;
            using desc_type = fast_info_site_desc_storage<char_type, char_count>;
            // desc_format() finds the literal right after the head
            static_assert(offsetof(desc_type, format) == sizeof(fast_info_site_desc), "fast_*: the literal must follow the descriptor head");
            static_assert(offsetof(fast_info_site_cell, value) == 0, "fast_*: the inline path reads the site at offset 0");
            static_assert(bq::tools::_is_bq_log_format_type<char_type[1]>::arg_type == fast_format_traits<F>::arg_type
                    && bq::tools::_is_bq_log_format_type<fast_format_view<char_type>>::arg_type == fast_format_traits<F>::arg_type
                    && bq::tools::_get_log_param_type_enum<fast_format_view<char_type>>() == bq::tools::_get_log_param_type_enum<typename fast_format_traits<F>::literal_type>(),
                "fast_*: the shared slow path would not log this literal the way the array does");

            static constexpr desc_type make_desc()
            {
                desc_type d {};
                d.head.category = CAT;
                d.head.format_units = static_cast<uint32_t>(F.at(char_count - 1) == 0 ? char_count - 1 : char_count);
                d.head.level = LEVEL;
                for (size_t i = 0; i < char_count; ++i) {
                    d.format[i] = F.at(i);
                }
                return d;
            }
            static constexpr desc_type desc = make_desc();
            // constant initialized, no dynamic initializer
            static fast_info_site_cell site;
        };
        template <fixed_format F, uint8_t LEVEL, uint32_t CAT, uint8_t... WIRE>
        fast_info_site_cell fast_info_site_holder<F, LEVEL, CAT, WIRE...>::site = { { 0 }, &fast_info_site_holder<F, LEVEL, CAT, WIRE...>::desc.head };

        // one per (character type, argument types), shared by every format
        template <typename CHAR, typename... Args>
        BQ_FAST_INFO_HIDDEN bq_noinline bool fast_slow(fast_info_site& site, const bq::log& l, typename bq::fast_slow_arg<Args>::type... args);

        template <fixed_format F, bq::log_level LEVEL, uint32_t CAT, typename... Args>
        bq_forceinline bool fast_impl(const bq::log& l, const Args&... args)
        {
            auto& site = fast_info_site_holder<F, static_cast<uint8_t>(LEVEL), CAT, fast_wire_of<Args>()...>::site.value;
            // A registers the format and publishes the site, B writes a record through it, C reads
            // that record in the appender. With a relaxed read B has no edge from A, so C would
            // dereference an entry A may not have published to it; the acquire here orders B after
            // A, and the ring buffer's release/acquire passes that edge on to C.
            const uint64_t v = BQ_PACK_ACCESS_BY_TYPE(site.v, bq::platform::atomic<uint64_t>).load_acquire();
            const uint32_t word = log_access::level_words(l)[CAT];
            // no arguments: the record is just the head, so nothing to size or fill
            if constexpr (sizeof...(Args) == 0) {
                // uint8_t(word & v): the low byte of the AND holds (word's level bits) & (v's level
                // bits). word's bits 6 and 7 are always 0, so an address bit above bit 5 can never
                // make the test true; masking v down to uint8_t instead costs an extra insn on x86.
                BQ_LIKELY_IF(static_cast<uint8_t>(word & v) != 0)
                {
                    const int32_t r = bq::fast_inline::write(log_access::buffer_id(l),
                        bq::fast_inline::fast_meta_by_site_v { &site.v }, 0, [](uint8_t*) {});
                    BQ_UNLIKELY_IF(r > 0)
                    {
                        bq::api::__api_fast_log_notify_low_space_by_id(log_access::log_id(l));
                    }
                    BQ_LIKELY_IF(r >= 0)
                    {
                        return true;
                    }
                }
                return fast_slow<typename decltype(F)::storage_type>(site, l);
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
                return fast_slow<typename decltype(F)::storage_type, Args...>(site, l, typename bq::fast_slow_arg<Args>::type(args)...);
            }
        }

        // First use of a site: register its literal and publish v, returning what was published.
        // UTF-32 is converted to UTF-16, the wide literal is bounded by 4 bytes per code point.
        template <typename CHAR>
        BQ_FAST_INFO_HIDDEN bq_noinline uint64_t fast_register(fast_info_site& site, const uint8_t* wire, uint16_t wire_count)
        {
            const fast_info_site_desc& desc = site_desc(site);
            bq::fast_inline::site_init_guard guard(&site);
            const uint64_t published = BQ_PACK_ACCESS_BY_TYPE(site.v, bq::platform::atomic<uint64_t>).load_acquire();
            if (published != 0) {
                return published;
            }
            const fast_format_view<CHAR> format = desc_format<CHAR>(desc);
            uint8_t converted_buffer[128];
            bq::array<uint8_t> converted_heap;
            const char* format_bytes;
            size_t format_size;
            if constexpr (sizeof(CHAR) == 4) {
                format_size = bq::tools::_serialize_str_helper_by_encode<CHAR>::get_c_style_string_storage_size(format.chars, format.units);
                uint8_t* converted = converted_buffer;
                if (format_size > sizeof(converted_buffer)) {
                    converted_heap.fill_uninitialized(format_size);
                    converted = &converted_heap[0];
                }
                bq::tools::_serialize_str_helper_by_encode<CHAR>::c_style_string_copy(converted, format.chars, format_size);
                format_bytes = reinterpret_cast<const char*>(converted);
            } else {
                format_size = static_cast<size_t>(format.units) * sizeof(CHAR);
                format_bytes = reinterpret_cast<const char*>(format.chars);
            }
            const uint64_t meta = bq::api::__api_fast_meta_register(desc.level, desc.category,
                static_cast<uint8_t>(bq::tools::_is_bq_log_format_type<CHAR[1]>::arg_type),
                format_bytes, static_cast<uint32_t>(format_size), wire_count ? wire : nullptr, wire_count);
            // one store publishes both the arena address and the enabled-level bit
            const uint64_t site_value = meta == 0 ? fast_site_refused : (meta | (UINT64_C(1) << static_cast<uint32_t>(desc.level)));
            BQ_PACK_ACCESS_BY_TYPE(site.v, bq::platform::atomic<uint64_t>).store_release(site_value);
            return site_value;
        }

        // what fast_slow_prepare returns instead of the meta address when do_log has to do the work
        constexpr uint64_t fast_slow_fallback = 1;

        template <typename CHAR>
        BQ_FAST_INFO_HIDDEN bq_noinline uint64_t fast_slow_prepare(fast_info_site& site, const bq::log& l, const fast_info_site_desc& desc, const uint8_t* wire,
            uint16_t wire_count)
        {
            if (log_access::buffer_id(l) == 0 || l.is_stack_trace_enabled_for(static_cast<bq::log_level>(desc.level))) {
                return fast_slow_fallback;
            }
            uint64_t site_value = BQ_PACK_ACCESS_BY_TYPE(site.v, bq::platform::atomic<uint64_t>).load_acquire();
            if (site_value == 0) {
                site_value = fast_register<CHAR>(site, wire, wire_count);
            }
            if (site_value == fast_site_refused) {
                return fast_slow_fallback;
            }
            return site_value & ~bq::fast_meta_addr_mask();
        }

        template <typename CHAR, typename... Args>
        BQ_FAST_INFO_HIDDEN bq_noinline bool fast_slow(fast_info_site& site, const bq::log& l, typename bq::fast_slow_arg<Args>::type... args)
        {
            const fast_info_site_desc& desc = site_desc(site);
            const bq::log_level level = static_cast<bq::log_level>(desc.level);
            // an invalid log has no bitmaps; a disabled level stops here, not in the out-of-line prepare
            if (log_access::log_id(l) == 0 || !l.is_enabled_for(desc.category, level)) {
                return false;
            }
            using wire = fast_wire_types<fast_wire_of<Args>()...>;
            const uint64_t meta_addr = fast_slow_prepare<CHAR>(site, l, desc, wire::values, wire::count);
            if (meta_addr != fast_slow_fallback) {
                if constexpr (sizeof...(Args) == 0) {
                    if (bq::api::__api_fast_log_write_no_args_by_id(log_access::log_id(l), meta_addr)) {
                        bq::fast_inline::refresh_thread_slot();
                        return true;
                    }
                } else {
                    auto size_seq = bq::tools::make_size_seq<false>(args...);
                    if (size_seq.get_total() <= UINT32_MAX - sizeof(bq::log_head_fast_def)) {
                        auto handle = bq::api::__api_fast_log_write_begin_by_id(log_access::log_id(l), meta_addr,
                            static_cast<uint32_t>(size_seq.get_total()));
                        if (handle.result == bq::enum_buffer_result_code::success) {
                            bq::impl::_do_log_args_fill<false>(handle.args_addr, size_seq, args...);
                            bq::api::__api_fast_log_write_finish_by_id(log_access::log_id(l), handle.args_addr);
                            bq::fast_inline::refresh_thread_slot();
                            return true;
                        }
                    }
                }
            }
            return l.do_log(desc.category, level, desc_format<CHAR>(desc), args...);
        }
    }
    BQ_MODULE_EXPORT_END
}

#undef BQ_FAST_INFO_HIDDEN

#endif
