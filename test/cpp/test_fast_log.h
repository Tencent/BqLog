#pragma once
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
#include "test_base.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/log_record_reader.h"
#include "bq_log/log/log_manager.h"

namespace bq {
    namespace test {
        class test_fast_log : public test_base {
            static void write_fast(const bq::log& log, int32_t value)
            {
                BQ_LOG_FAST_INFO(log, "fast {}", value);
            }

            static void write_fast_level(const bq::log& log, bq::log_level level, int32_t value)
            {
                if (level == bq::log_level::warning) {
                    BQ_LOG_FAST_WARNING(log, "dynamic {}", value);
                } else {
                    BQ_LOG_FAST_INFO(log, "dynamic {}", value);
                }
            }

            static void write_fast_dynamic(const bq::log& log, const bq::string& format,
                int32_t value)
            {
                BQ_LOG_FAST_INFO(log, format, value);
            }

            class fast_writer_thread : public bq::platform::thread {
                bq::log log_;

            public:
                explicit fast_writer_thread(const bq::log& log)
                    : log_(log)
                {
                }

                void run() override
                {
                    for (int32_t i = 0; i < 100; ++i) {
                        write_fast(log_, 1000 + i);
                        log_.info("normal {}", 1000 + i);
                    }
                }
            };

            class named_writer_thread : public bq::platform::thread {
                bq::log log_;

            public:
                bool hp_without_ext_ = false;
                char thread_tag_[64] = {}; // "[tid-<id> <name>]" as layout prints it
                char thread_id_tag_[32] = {}; // "[tid-<id> "
                explicit named_writer_thread(const bq::log& log)
                    : log_(log)
                {
                    set_thread_name("bq_named_w");
                }

                void run() override
                {
                    const auto& info = bq::log_manager::get_log_by_id(log_.get_id())->get_buffer().get_buffer_info_for_this_thread();
                    for (int32_t i = 0; i < 2000000 && !info.cur_block_; ++i) {
                        log_.info("named warmup {}", i);
                    }
                    hp_without_ext_ = info.cur_block_ != nullptr;
                    const auto& thread_info = bq::get_log_thread_info();
                    snprintf(thread_tag_, sizeof(thread_tag_), "[tid-%" PRIu64 " %.*s]", thread_info.thread_id_,
                        static_cast<int32_t>(thread_info.thread_name_len_), thread_info.thread_name_);
                    snprintf(thread_id_tag_, sizeof(thread_id_tag_), "[tid-%" PRIu64 " ", thread_info.thread_id_);
                    log_.info("named standard {}", 5001);
                    BQ_LOG_FAST_INFO(log_, "named fast {}", 5002);
                }
            };

            static bool decode_has_line(const bq::string& path, const char* text, const char* thread_name)
            {
                bq::tools::log_decoder decoder(path);
                while (decoder.decode() == bq::appender_decode_result::success) {
                    const auto& line = decoder.get_last_decoded_log_entry();
                    if (line.find(text) != bq::string::npos) {
                        return line.find(thread_name) != bq::string::npos;
                    }
                }
                return false;
            }

            static bool text_has_line(const bq::string& data, const char* text, const char* thread_name)
            {
                const size_t pos = data.find(text);
                if (pos == bq::string::npos) {
                    return false;
                }
                size_t line_begin = pos;
                while (line_begin > 0 && data[line_begin - 1] != '\n') {
                    --line_begin;
                }
                return data.substr(line_begin, pos - line_begin).find(thread_name) != bq::string::npos;
            }

            // A fresh log (so layout's thread name cache is empty) written by a named thread that reaches HP.
            static void test_hp_thread_info(test_result& result)
            {
                char name[96];
                snprintf(name, sizeof(name), "fast_mode_thread_info_%" PRIu64,
                    bq::platform::high_performance_epoch_ms());
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                const bq::string config = bq::string("appenders_config.test.type=text_file\n")
                    + "appenders_config.test.levels=[all]\n"
                    + "appenders_config.test.file_name=" + file_name + "\n"
                    + "appenders_config.test.base_dir_type=0\n"
                    + "appenders_config.test.enable_rolling_log_file=false\n"
                    + "appenders_config.compressed.type=compressed_file\n"
                    + "appenders_config.compressed.levels=[all]\n"
                    + "appenders_config.compressed.file_name=" + file_name + "_compressed\n"
                    + "appenders_config.compressed.enable_rolling_log_file=false\n"
                    + "log.thread_mode=async\n"
                    + "log.high_perform_mode_freq_threshold_per_second=1\n"
                    + "snapshot.buffer_size=65536\n";
                bq::log log = bq::log::create_log(name, config);
                named_writer_thread writer(log);
                writer.start();
                writer.join();
                result.add_result(writer.hp_without_ext_, "named thread reaches hp");
                log.force_flush();
                const char* tag = writer.thread_tag_;
                // The text layout is shared and caches names by thread id, which the OS may reuse, so only the id is checked here.
                const bq::string data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                result.add_result(text_has_line(data, "named standard 5001", writer.thread_id_tag_), "hp standard record thread id in text");
                result.add_result(text_has_line(data, "named fast 5002", writer.thread_id_tag_), "hp fast record thread id in text");
                const bq::string snapshot = log.take_snapshot("localtime");
                result.add_result(text_has_line(snapshot, "named standard 5001", tag), "hp standard record thread info in snapshot");
                const bq::string compressed_path = TO_ABSOLUTE_PATH(file_name + "_compressed_1.logcompr", 0);
                result.add_result(decode_has_line(compressed_path, "named standard 5001", tag), "hp standard record thread info in compressed file");
                result.add_result(decode_has_line(compressed_path, "named fast 5002", tag), "hp fast record thread info in compressed file");
            }

            static void write_fast_category(const bq::log& log, uint32_t category, int32_t value)
            {
                auto call = bq::make_fast_log_call("category {} {}", category, value);
                static bq::_api_fast_log_site_handle site_a = BQ_FAST_LOG_SITE_INITIALIZER;
                static bq::_api_fast_log_site_handle site_b = BQ_FAST_LOG_SITE_INITIALIZER;
                bq::fast_log_from_call<bq::tuple<uint32_t, int32_t>>(category == 0 ? site_a : site_b, log, category, bq::log_level::info, call);
            }

            // The inline level check reads only the call site's level word: it must follow reset_config and category masks.
            static void test_site_level_word(test_result& result)
            {
                char name[96];
                snprintf(name, sizeof(name), "fast_mode_site_level_%" PRIu64,
                    bq::platform::high_performance_epoch_ms());
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                const bq::string base_config = bq::string("appenders_config.test.type=text_file\n")
                    + "appenders_config.test.levels=[all]\n"
                    + "appenders_config.test.file_name=" + file_name + "\n"
                    + "appenders_config.test.base_dir_type=0\n"
                    + "appenders_config.test.enable_rolling_log_file=false\n"
                    + "log.thread_mode=async\n"
                    + "log.high_perform_mode_freq_threshold_per_second=1\n";
                const char* categories[] = { "ModuleA", "ModuleB" };
                uint64_t log_id = bq::api::__api_create_log(name, (base_config + "log.categories_mask=[ModuleA,ModuleB]\n").c_str(), 2, categories);
                (void)log_id;
                bq::log log = bq::log::get_log_by_name(name);
                result.add_result(log.is_valid(), "site level log create");
                if (!log.is_valid()) {
                    return;
                }
                bq::_api_fast_log_site_handle unregistered = BQ_FAST_LOG_SITE_INITIALIZER;
                result.add_result(*unregistered.level_word == 0, "unregistered site fails the inline level check");
                // flush before each reset_config: the worker filters by the current mask, so records still in the buffer
                // would be judged by the new configuration
                write_fast_category(log, 0, 1);
                write_fast_category(log, 1, 2);
                log.force_flush();
                bq::api::__api_log_reset_config(name, (base_config + "log.categories_mask=[ModuleA]\n").c_str());
                write_fast_category(log, 0, 3);
                write_fast_category(log, 1, 4);
                log.force_flush();
                bq::api::__api_log_reset_config(name, (base_config + "log.categories_mask=[ModuleA,ModuleB]\nappenders_config.test.levels=[error]\n").c_str());
                write_fast_category(log, 0, 5);
                log.force_flush();
                bq::api::__api_log_reset_config(name, (base_config + "log.categories_mask=[ModuleA,ModuleB]\n").c_str());
                write_fast_category(log, 1, 6);
                log.force_flush();
                const bq::string data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                result.add_result(data.find("category 0 1") != bq::string::npos && data.find("category 1 2") != bq::string::npos,
                    "site level word: both categories enabled");
                result.add_result(data.find("category 0 3") != bq::string::npos && data.find("category 1 4") == bq::string::npos,
                    "site level word: category disabled by reset_config");
                result.add_result(data.find("category 0 5") == bq::string::npos, "site level word: level disabled by reset_config");
                result.add_result(data.find("category 1 6") != bq::string::npos, "site level word: re-enabled by reset_config");
            }

            static void test_recovery(test_result& result)
            {
                char name[96];
                snprintf(name, sizeof(name), "fast_mode_recovery_%" PRIu64,
                    bq::platform::high_performance_epoch_ms());
                bq::log_buffer_config config;
                config.log_name = name;
                config.log_categories_name = { "" };
                config.need_recovery = true;
                config.policy = bq::log_memory_policy::auto_expand_when_full;
                const uint16_t old_version = [&]() {
                    bq::log_buffer buffer(config);
                    uint8_t arg_type = static_cast<uint8_t>(bq::log_arg_type_enum::int32_type);
                    const bq::fast_format_meta* first = nullptr;
                    const bq::fast_format_meta* last = nullptr;
                    for (uint32_t i = 0; i < 1600; ++i) {
                        const auto* meta = buffer.register_fast_log_format("recover {}", 10,
                            static_cast<uint8_t>(bq::log_level::info), 0,
                            static_cast<uint8_t>(bq::log_arg_type_enum::string_utf8_type),
                            &arg_type, 1);
                        if (i == 0) {
                            first = meta;
                        }
                        last = meta;
                    }
                    bq::array<char> large_format;
                    large_format.fill_uninitialized(100000);
                    memset(&large_format[0], 'z', large_format.size());
                    const auto* large = buffer.register_fast_log_format(&large_format[0],
                        static_cast<uint32_t>(large_format.size()),
                        static_cast<uint8_t>(bq::log_level::info), 0,
                        static_cast<uint8_t>(bq::log_arg_type_enum::string_utf8_type),
                        &arg_type, 1);
                    result.add_result(first && last, "fast recovery metadata register");
                    result.add_result(large != nullptr, "fast recovery oversize metadata register");
                    auto& tls = bq::log_tls_info__get_direct().get_buffer_info(&buffer);
                    tls.fast_mode_ = true;
                    tls.cur_block_ = buffer.alloc_new_hp_block();
                    const bq::fast_format_meta* formats[] = { first, last, large };
                    for (int32_t i = 0; i < 3; ++i) {
                        auto seq = bq::tools::make_size_seq<false>(int32_t(73 + i));
                        auto handle = buffer.alloc_write_chunk(
                            static_cast<uint32_t>(sizeof(bq::log_head_fast_def) + seq.get_total()),
                            bq::platform::high_performance_epoch_ms());
                        result.add_result(handle.result == bq::enum_buffer_result_code::success,
                            "fast recovery write");
                        if (formats[i] && handle.result == bq::enum_buffer_result_code::success) {
                            auto& head = *reinterpret_cast<bq::log_head_fast_def*>(handle.data_addr);
                            head.timestamp_epoch = bq::log_head_base_def::set_fast(
                                bq::platform::high_performance_epoch_ms());
                            head.format_meta_addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(formats[i]));
                            bq::impl::_do_log_args_fill<false>(handle.data_addr + sizeof(head),
                                seq, int32_t(73 + i));
                        }
                        buffer.commit_write_chunk(handle);
                    }
                    auto damaged = buffer.alloc_write_chunk(
                        sizeof(bq::log_head_fast_def), bq::platform::high_performance_epoch_ms());
                    if (damaged.result == bq::enum_buffer_result_code::success) {
                        auto& head = *reinterpret_cast<bq::log_head_fast_def*>(damaged.data_addr);
                        head.timestamp_epoch = bq::log_head_base_def::set_fast(
                            bq::platform::high_performance_epoch_ms());
                        head.format_meta_addr = 1;
                    }
                    buffer.commit_write_chunk(damaged);
                    return buffer.get_version();
                }();
                char first_name[32];
                char second_name[32];
                char large_name[40];
                snprintf(first_name, sizeof(first_name), "%" PRIu16 "_0.mmap", old_version);
                snprintf(second_name, sizeof(second_name), "%" PRIu16 "_1.mmap", old_version);
                snprintf(large_name, sizeof(large_name), "%" PRIu16 "_0.large.mmap", old_version);
                const bq::string folder = TO_ABSOLUTE_PATH(
                    bq::string("bqlog_mmap/mmap_") + name + "/fast_meta", 0);
                const bq::string first_path = bq::file_manager::combine_path(folder, first_name);
                const bq::string second_path = bq::file_manager::combine_path(folder, second_name);
                const bq::string large_path = bq::file_manager::combine_path(folder, large_name);
                result.add_result(bq::file_manager::is_file(first_path), "fast recovery first segment persisted");
                result.add_result(bq::file_manager::is_file(second_path), "fast recovery multiple segments persisted");
                result.add_result(bq::file_manager::is_file(large_path), "fast recovery oversize metadata persisted");
                {
                    bq::log_buffer buffer(config);
                    const auto& thread_info = bq::get_log_thread_info();
                    bool found_first = false;
                    bool found_second = false;
                    bool found_large = false;
                    bool found_recovery_error = false;
                    bq::array<uint8_t, bq::aligned_allocator<uint8_t, 8>> converted;
                    for (int32_t i = 0; i < 64; ++i) {
                        auto chunk = buffer.read_chunk();
                        bq::scoped_log_buffer_handle<bq::log_buffer> guard(buffer, chunk);
                        if (chunk.result != bq::enum_buffer_result_code::success) {
                            continue;
                        }
                        bq::log_entry_handle entry(chunk.data_addr, chunk.data_size);
                        if (bq::log_record_reader::read(buffer, chunk.data_addr, chunk.data_size,
                                converted, entry)) {
                            const bool valid = entry.validate()
                                && entry.get_log_head().log_thread_id == thread_info.thread_id_
                                && entry.get_ext_head().thread_name_len_ == thread_info.thread_name_len_
                                && entry.get_log_head().level == static_cast<uint8_t>(bq::log_level::info);
                            found_first |= valid && entry.get_log_args_data()[4] == 73;
                            found_second |= valid && entry.get_log_args_data()[4] == 74;
                            found_large |= valid && entry.get_log_head().log_format_data_len == 100000
                                && entry.get_log_args_data()[4] == 75;
                        } else if (bq::log_record_reader::make_recovery_error(buffer,
                                       chunk.data_addr, chunk.data_size, converted, entry)) {
                            found_recovery_error = entry.validate()
                                && entry.get_level() == bq::log_level::warning
                                && bq::string(entry.get_format_string_data(),
                                       entry.get_log_head().log_format_data_len)
                                        .find("fast log recovery failed")
                                    != bq::string::npos;
                        }
                    }
                    result.add_result(found_first && found_second,
                        "fast recovery reads metadata from multiple segments");
                    result.add_result(found_large, "fast recovery reads oversize metadata");
                    result.add_result(found_recovery_error,
                        "fast recovery corrupt metadata warning record");
                }
                result.add_result(!bq::file_manager::is_file(first_path)
                        && !bq::file_manager::is_file(second_path)
                        && !bq::file_manager::is_file(large_path),
                    "fast recovery multiple segments retired");
            }

            static bool decode_has_both(const bq::string& path)
            {
                bq::tools::log_decoder decoder(path);
                bool has_fast = false;
                bool has_standard = false;
                for (int32_t i = 0; i < 3000; ++i) {
                    const auto state = decoder.decode();
                    if (state == bq::appender_decode_result::eof) {
                        break;
                    }
                    if (state != bq::appender_decode_result::success) {
                        return false;
                    }
                    const auto& text = decoder.get_last_decoded_log_entry();
                    has_fast |= text.find("fast 2001") != bq::string::npos;
                    has_standard |= text.find("normal 999") != bq::string::npos;
                }
                return has_fast && has_standard;
            }

        public:
            // Library state seen through the exported layout and the pushed thread state must match the library's own view.
            static void test_inline_support(test_result& result, const bq::log& log)
            {
                bq::_api_fast_log_layout layout;
                result.add_result(!bq::api::__api_fast_log_get_layout(&layout, BQ_FAST_LOG_LAYOUT_VERSION + 1), "layout rejects unknown version");
                result.add_result(bq::api::__api_fast_log_get_layout(&layout, BQ_FAST_LOG_LAYOUT_VERSION), "layout accepts current version");
                result.add_result(layout.struct_size == sizeof(layout) && layout.layout_version == BQ_FAST_LOG_LAYOUT_VERSION, "layout header");

                // a thread that entered HP through the normal API but never used the fast path
                class normal_first_thread : public bq::platform::thread {
                public:
                    bq::log log_;
                    bool thread_info_after_slow_ = false;
                    bool block_after_slow_ = false;
                    explicit normal_first_thread(const bq::log& l)
                        : log_(l)
                    {
                    }
                    void run() override
                    {
                        for (int32_t i = 0; i < 2000000; ++i) {
                            log_.info("hp by normal {}", i);
                            if (bq::log_manager::get_log_by_id(log_.get_id())->get_buffer().get_buffer_info_for_this_thread().cur_block_) {
                                break;
                            }
                        }
                        write_fast(log_, 3000);
                        const auto& info = bq::log_manager::get_log_by_id(log_.get_id())->get_buffer().get_buffer_info_for_this_thread();
                        thread_info_after_slow_ = info.cur_block_
                            && info.cur_block_->get_misc_data<bq::log_buffer::block_misc_data>().thread_info_.thread_id_ == bq::platform::thread::get_current_thread_id();
                        block_after_slow_ = info.cur_block_ != nullptr && info.fast_mode_;
                    }
                };
                normal_first_thread t(log);
                t.start();
                t.join();
                result.add_result(t.thread_info_after_slow_ && t.block_after_slow_, "slow path keeps a block owned by a thread already in HP");

                // the pushed thread state describes the block the library sees as current
                write_fast(log, 3001);
                auto& buffer = bq::log_manager::get_log_by_id(log.get_id())->get_buffer();
                const auto& info = buffer.get_buffer_info_for_this_thread();
                const auto& state = bq::fast_inline::get_thread_slot().state;
                auto expect_state_matches = [&](const char* when) {
                    bq::block_node_head* block = info.cur_block_;
                    result.add_result(block && ~state.buffer_key == buffer.get_id(), "thread state: buffer key %s", when);
                    if (!block) {
                        return;
                    }
                    result.add_result(state.need_reallocate == &block->get_misc_data<bq::log_buffer::block_misc_data>().need_reallocate_, "thread state: need_reallocate %s", when);
                    result.add_result(state.siso_units == block->get_buffer().get_buffer_addr()
                            && state.unit_count == block->get_buffer().get_total_blocks_count()
                            && state.half_unit_count == state.unit_count / 2,
                        "thread state: siso fields %s", when);
                };
                expect_state_matches("after first write");
                const auto& block_thread = info.cur_block_->get_misc_data<bq::log_buffer::block_misc_data>().thread_info_;
                result.add_result(block_thread.thread_id_ == bq::get_log_thread_info().thread_id_
                        && block_thread.thread_name_len_ == bq::get_log_thread_info().thread_name_len_
                        && memcmp(block_thread.thread_name_, bq::get_log_thread_info().thread_name_, block_thread.thread_name_len_) == 0,
                    "hp block records its owner thread");
                result.add_result(state.unit_count > 0 && (state.unit_count & (state.unit_count - 1)) == 0, "thread state: unit count is a power of two");
                result.add_result(layout.chunk_data_offset == 8 && layout.block_size_log2 == 3, "layout: chunk format");

                // the library republishes on every block change: compaction, then an oversize record (block dropped and re-allocated)
                bq::block_node_head* before_compaction = info.cur_block_;
                before_compaction->get_misc_data<bq::log_buffer::block_misc_data>().need_reallocate_ = true;
                result.add_result(*state.need_reallocate, "thread state: reallocate flag read through the state");
                write_fast(log, 3010);
                result.add_result(info.cur_block_ != before_compaction, "thread state: compaction switched block");
                expect_state_matches("after compaction");
                bq::array<char> big_text;
                big_text.fill_uninitialized(200000);
                memset(&big_text[0], 'y', big_text.size());
                const bq::string big_arg(&big_text[0], big_text.size());
                // the block is dropped and a new one allocated (possibly at the same recycled address): its seq moves on
                const uint32_t seq_before_oversize = info.cur_block_->get_misc_data<bq::log_buffer::block_misc_data>().context_.seq_;
                BQ_LOG_FAST_INFO(log, "state oversize {}", big_arg);
                result.add_result(info.cur_block_ && info.cur_block_->get_misc_data<bq::log_buffer::block_misc_data>().context_.seq_ != seq_before_oversize,
                    "thread state: oversize record replaced block");
                expect_state_matches("after oversize");
                write_fast(log, 3011);
                expect_state_matches("after write following oversize");

                // thread exit: the library clears the bound state when the thread's log_tls_info dies
                class bound_state_thread : public bq::platform::thread {
                public:
                    bq::log log_;
                    bq::_api_fast_log_thread_state* heap_state_ = nullptr; // outlives the thread so the clearing is observable
                    bool bound_ = false;
                    explicit bound_state_thread(const bq::log& l)
                        : log_(l)
                    {
                    }
                    void run() override
                    {
                        write_fast(log_, 3002);
                        write_fast(log_, 3003);
                        bound_ = bq::fast_inline::get_thread_slot().state.buffer_key != 0;
                        bq::api::__api_fast_log_bind_thread_state(heap_state_);
                        bound_ = bound_ && heap_state_->buffer_key != 0;
                    }
                };
                bq::_api_fast_log_thread_state* heap_state = new bq::_api_fast_log_thread_state();
                bound_state_thread* bt = new bound_state_thread(log);
                bt->heap_state_ = heap_state;
                bt->start();
                bt->join();
                result.add_result(bt->bound_, "inline path binds the thread state");
                result.add_result(heap_state->buffer_key == 0 && heap_state->need_reallocate && !*heap_state->need_reallocate,
                    "thread exit clears the bound state");
                delete bt;
                delete heap_state;

                // two logs alternating on one thread keep their own per-log state (one call site per log: a site is bound to its first log)
                bq::log log2 = bq::log::create_log("fast_mode_second_log", "appenders_config.c.type=text_file\nappenders_config.c.levels=[all]\nappenders_config.c.file_name=fast_mode_output/second_log\nlog.high_perform_mode_freq_threshold_per_second=1\n");
                for (int32_t i = 0; i < 100; ++i) {
                    write_fast(log, 4000 + i);
                    BQ_LOG_FAST_INFO(log2, "second {}", 4000 + i);
                }
                auto& buffer2 = bq::log_manager::get_log_by_id(log2.get_id())->get_buffer();
                result.add_result(&buffer2.get_buffer_info_for_this_thread() != &buffer.get_buffer_info_for_this_thread(), "two logs keep separate tls state");
                result.add_result(buffer2.get_buffer_info_for_this_thread().cur_block_ != nullptr
                        && buffer2.get_buffer_info_for_this_thread().cur_block_ != buffer.get_buffer_info_for_this_thread().cur_block_,
                    "second log uses its own hp block");

                // low space notification on a real site, and on a site whose log no longer resolves
                bq::_api_fast_log_site_handle site = {};
                result.add_result(bq::init_fast_log_site<bq::tuple<int32_t>>(site, log, 0, bq::log_level::info, "notify {}"), "site init");
                result.add_result(site.log_id == log.get_id() && site.buffer_id == buffer.get_id(), "site carries log id and buffer id");
                bq::api::__api_fast_log_notify_low_space(&site);
                bq::_api_fast_log_site_handle dead_site = site;
                dead_site.log_id = 0;
                bq::api::__api_fast_log_notify_low_space(&dead_site);
            }

            static void test_level_bitmaps(test_result& result)
            {
                char name[96];
                snprintf(name, sizeof(name), "fast_mode_levels_%" PRIu64,
                    bq::platform::high_performance_epoch_ms());
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                const bq::string base_config = bq::string("appenders_config.test.type=text_file\n")
                    + "appenders_config.test.levels=[info,warning,error]\n"
                    + "appenders_config.test.file_name=" + file_name + "\n"
                    + "appenders_config.test.base_dir_type=0\n"
                    + "appenders_config.test.enable_rolling_log_file=false\n"
                    + "log.thread_mode=sync\n";
                bq::log log = bq::log::create_log(name, base_config + "log.print_stack_levels=[error]\n");
                result.add_result(log.is_valid(), "level bitmap log create");
                if (!log.is_valid()) {
                    return;
                }
                const uint32_t* merged = bq::api::__api_get_log_merged_log_level_bitmap_by_log_id(log.get_id());
                const uint32_t* stack = bq::api::__api_get_log_print_stack_level_bitmap_by_log_id(log.get_id());
                const uint32_t* words = bq::api::__api_get_log_category_level_words_by_log_id(log.get_id());
                result.add_result(words != nullptr && words[0] == (*merged & ~*stack), "category level word is merged without stack levels");
                result.add_result(log.is_enabled_without_stack_trace_for(0, bq::log_level::info)
                        && !log.is_enabled_without_stack_trace_for(0, bq::log_level::error)
                        && !log.is_enabled_without_stack_trace_for(0, bq::log_level::debug),
                    "inline enable uses no stack bitmap");
                log.reset_config(base_config + "log.print_stack_levels=[warning]\n");
                result.add_result(words[0] == (*merged & ~*stack)
                        && log.is_enabled_without_stack_trace_for(0, bq::log_level::error)
                        && !log.is_enabled_without_stack_trace_for(0, bq::log_level::warning),
                    "no stack bitmap follows reset_config");
                result.add_result(!log.debug("level bitmap disabled"), "standard disabled level rejected");
                log.info("level bitmap plain {}", 1);
                log.warning("level bitmap stack {}", 2);
                BQ_LOG_FAST_WARNING(log, "level bitmap fast stack {}", 3);
                log.force_flush();
                const bq::string data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                const size_t plain_pos = data.find("level bitmap plain 1");
                const size_t stack_pos = data.find("level bitmap stack 2");
                const size_t fast_pos = data.find("level bitmap fast stack 3");
                result.add_result(plain_pos != bq::string::npos && stack_pos != bq::string::npos && fast_pos != bq::string::npos,
                    "level bitmap records written");
                result.add_result(stack_pos != bq::string::npos && fast_pos != bq::string::npos
                        && fast_pos - stack_pos > bq::string("level bitmap stack 2\n").size(),
                    "standard stack level still prints stack trace");
            }

            test_result test() override
            {
                test_result result;
                char name[96];
                snprintf(name, sizeof(name), "fast_mode_text_%" PRIu64,
                    bq::platform::high_performance_epoch_ms());
                bq::string file_name = bq::string("fast_mode_output/") + name;
                bq::string config = bq::string("appenders_config.test.type=text_file\n")
                    + "appenders_config.test.levels=[all]\n"
                    + "appenders_config.test.file_name=" + file_name + "\n"
                    + "appenders_config.test.base_dir_type=0\n"
                    + "appenders_config.test.enable_rolling_log_file=false\n"
                    + "appenders_config.raw.type=raw_file\n"
                    + "appenders_config.compressed.type=compressed_file\n"
                    + "appenders_config.compressed.levels=[all]\n"
                    + "appenders_config.compressed.file_name=" + file_name + "_compressed\n"
                    + "appenders_config.compressed.enable_rolling_log_file=false\n"
                    + "log.thread_mode=async\n"
                    + "log.buffer_size=65536\n"
                    + "log.buffer_policy_when_full=expand\n"
                    + "log.high_perform_mode_freq_threshold_per_second=1000000\n"
                    + "snapshot.buffer_size=65536\n";
                bq::log log = bq::log::create_log(name, config);
                result.add_result(log.is_valid(), "fast log create");
                if (!log.is_valid()) {
                    return result;
                }
                for (int32_t i = 0; i < 1000; ++i) {
                    write_fast(log, i);
                    log.info("normal {}", i);
                }
                auto& buffer = bq::log_manager::get_log_by_id(log.get_id())->get_buffer();
                bq::platform::thread::sleep(1100);
                log.info("normal after idle {}", int32_t(42));
                result.add_result(buffer.get_buffer_info_for_this_thread().cur_block_ != nullptr
                        && buffer.get_buffer_info_for_this_thread().fast_mode_,
                    "fast hp retained across idle standard write");
                write_fast_dynamic(log, bq::string("dynamic first {}"), 1);
                write_fast_dynamic(log, bq::string("dynamic second {}"), 2);
                auto* old_block = buffer.get_buffer_info_for_this_thread().cur_block_;
                old_block->get_misc_data<bq::log_buffer::block_misc_data>().need_reallocate_ = true;
                write_fast(log, 2000);
                result.add_result(buffer.get_buffer_info_for_this_thread().cur_block_ != old_block,
                    "fast record compaction switched block");
                fast_writer_thread writer(log);
                writer.start();
                writer.join();
                write_fast(log, 2001);
                int32_t evaluation_count = 0;
                BQ_LOG_FAST_INFO(log, "single evaluation {}", ++evaluation_count);
                result.add_result(evaluation_count == 1, "fast macro evaluates arguments once");
                BQ_LOG_FAST_INFO(log, "fast without arguments");
                BQ_LOG_FAST_INFO(log, u"utf16 {}", int32_t(7));
                BQ_LOG_FAST_INFO(log, U"utf32 {}", int32_t(8));
                const char unterminated_format[] = { 'a', 'r', 'r', 'a', 'y', ' ', '{', '}' };
                BQ_LOG_FAST_INFO(log, unterminated_format, int32_t(9));
                const char terminated_format[] = "array copy {}";
                BQ_LOG_FAST_INFO(log, terminated_format, int32_t(10));
                bq::array<char> large_text;
                large_text.fill_uninitialized(100000);
                memset(&large_text[0], 'x', large_text.size());
                const bq::string large_arg(&large_text[0], large_text.size());
                BQ_LOG_FAST_INFO(log, "oversize {}", large_arg);
                result.add_result(buffer.get_buffer_info_for_this_thread().cur_block_ != nullptr,
                    "fast hp restored after oversize record");
                log.info("normal after oversize {}", int32_t(2002));
                write_fast(log, 2003);
                log.info("normal oversize {}", large_arg);
                write_fast(log, 2004);
                write_fast_level(log, bq::log_level::info, 1);
                write_fast_level(log, bq::log_level::warning, 2);
                log.force_flush();
                const bq::string path = TO_ABSOLUTE_PATH(file_name + "_1.log", 0);
                const bq::string data = bq::file_manager::read_all_text(path);
                result.add_result(data.find("fast 0") != bq::string::npos, "fast text first");
                result.add_result(data.find("fast 999") != bq::string::npos, "fast text last");
                result.add_result(data.find("normal 0") != bq::string::npos, "standard text first");
                result.add_result(data.find("normal 999") != bq::string::npos, "standard text last");
                result.add_result(data.find("normal after idle 42") != bq::string::npos,
                    "standard text after idle fast mode");
                result.add_result(data.find("dynamic first 1") != bq::string::npos
                        && data.find("dynamic first 2") != bq::string::npos,
                    "fast dynamic format uses first normalized value");
                result.add_result(data.find("fast 2000") != bq::string::npos, "fast text after compaction");
                result.add_result(data.find("fast 1099") != bq::string::npos, "fast text from exited thread");
                result.add_result(data.find("fast 2001") != bq::string::npos, "fast text after thread exit");
                result.add_result(data.find("single evaluation 1") != bq::string::npos,
                    "fast argument evaluated once in text");
                result.add_result(data.find("fast without arguments") != bq::string::npos,
                    "fast text without arguments");
                result.add_result(data.find("utf16 7") != bq::string::npos, "fast utf16 format");
                result.add_result(data.find("utf32 8") != bq::string::npos, "fast utf32 format");
                result.add_result(data.find("array 9") != bq::string::npos, "fast format from a char array without terminator");
                result.add_result(data.find("array copy 10") != bq::string::npos, "fast format from a terminated char array");
                result.add_result(data.find("oversize xxx") != bq::string::npos, "fast oversize text");
                result.add_result(data.find("normal after oversize 2002") != bq::string::npos,
                    "standard text after fast oversize");
                result.add_result(data.find("fast 2003") != bq::string::npos, "fast text after oversize");
                result.add_result(data.find("normal oversize xxx") != bq::string::npos,
                    "standard oversize after fast records");
                result.add_result(data.find("fast 2004") != bq::string::npos,
                    "fast record after standard oversize");
                result.add_result(data.find("[W]\tdynamic 2") != bq::string::npos,
                    "fast site level rebinding");
                const bq::string snapshot = log.take_snapshot("localtime");
                result.add_result(snapshot.find("fast 2003") != bq::string::npos,
                    "fast record snapshot");
                result.add_result(log.is_valid() && !bq::file_manager::is_file(TO_ABSOLUTE_PATH(file_name + "_raw_1.lograw", 0)),
                    "removed raw_file appender is ignored");
                result.add_result(decode_has_both(TO_ABSOLUTE_PATH(file_name + "_compressed_1.logcompr", 0)),
                    "fast and standard compressed file");
                test_inline_support(result, log);
                test_level_bitmaps(result);
                test_site_level_word(result);
                test_hp_thread_info(result);
                test_recovery(result);
                return result;
            }
        };
    }
}
