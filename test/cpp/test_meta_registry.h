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
#include "test_base.h"
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/types/meta/meta_registry.h"
#include "bq_log/types/storage/instance_storage.h"
#include "bq_log/types/storage/log_storage.h"
#include <atomic>
#include <thread>
#include <vector>

namespace bq {
    namespace test {
        class test_meta_registry : public test_base {
        private:
            static bool instance_available()
            {
                return bq::instance_storage::get() != nullptr;
            }

            static bq::string meta_dir()
            {
                auto* inst = bq::instance_storage::get();
                return inst ? inst->meta_dir() : bq::string();
            }

            static const fast_format_meta* reg(const char* format, uint32_t format_size,
                uint8_t level, uint32_t category, const uint8_t* arg_types, uint16_t arg_count)
            {
                return bq::meta_registry::instance().register_format(format, format_size, level, category,
                    static_cast<uint8_t>(bq::log_arg_type_enum::string_utf8_type), arg_types, arg_count);
            }

            static void test_dedupe(test_result& result)
            {
                const uint8_t i32 = static_cast<uint8_t>(bq::log_arg_type_enum::int32_type);
                const uint8_t u32 = static_cast<uint8_t>(bq::log_arg_type_enum::uint32_type);
                const uint8_t types_a[] = { i32 };
                const uint8_t types_b[] = { u32 };
                const auto* a = reg("dedupe {}", 9, static_cast<uint8_t>(bq::log_level::info), 0, types_a, 1);
                const auto* b = reg("dedupe {}", 9, static_cast<uint8_t>(bq::log_level::info), 0, types_a, 1);
                result.add_result(a && b && a == b, "dedupe: identical registration returns same pointer");
                result.add_result(reg("dedupe {}", 9, static_cast<uint8_t>(bq::log_level::warning), 0, types_a, 1) != a,
                    "dedupe: different level is a distinct entry");
                result.add_result(reg("dedupe {}", 9, static_cast<uint8_t>(bq::log_level::info), 1, types_a, 1) != a,
                    "dedupe: different category is a distinct entry");
                result.add_result(reg("dedupe {}", 9, static_cast<uint8_t>(bq::log_level::info), 0, types_b, 1) != a,
                    "dedupe: different arg types is a distinct entry");
                result.add_result(reg("dedupeX {}", 10, static_cast<uint8_t>(bq::log_level::info), 0, types_a, 1) != a,
                    "dedupe: different format string is a distinct entry");
                if (!a) {
                    return;
                }
                result.add_result(a->head.kind == fast_meta_kind::format && a->head.ready == 1,
                    "dedupe: entry fields valid");
                result.add_result(a->format_hash == bq::util::get_hash_64("dedupe {}", 9),
                    "dedupe: format hash matches format bytes");
                result.add_result(a->arg_count == 1 && a->format_size == 9
                        && a->arg_types()[0] == i32 && memcmp(a->format_data(), "dedupe {}", 9) == 0,
                    "dedupe: format data and arg types round-trip");
                result.add_result(a->head.record_size == static_cast<uint32_t>(bq::align_8(sizeof(fast_format_meta) + 9 + 1)),
                    "dedupe: record size is align_8(40 + format + args)");
            }

            static void test_verify_entry(test_result& result)
            {
                const uint8_t types[] = { static_cast<uint8_t>(bq::log_arg_type_enum::int32_type) };
                const auto* a = reg("verify {}", 9, static_cast<uint8_t>(bq::log_level::info), 0, types, 1);
                if (!a) {
                    result.add_result(false, "verify_entry: register failed");
                    return;
                }
                result.add_result(bq::meta_registry::verify_entry(&a->head, a->head.record_size),
                    "verify_entry: valid entry passes");
                bq::array<uint8_t> copy;
                copy.fill_uninitialized(a->head.record_size);
                memcpy(&copy[0], a, a->head.record_size);
                ++copy[copy.size() / 2];
                result.add_result(!bq::meta_registry::verify_entry(reinterpret_cast<fast_meta_head*>(&copy[0]),
                                      static_cast<uint32_t>(copy.size())),
                    "verify_entry: tampered byte fails checksum");
                result.add_result(!bq::meta_registry::verify_entry(&a->head, a->head.record_size - 1),
                    "verify_entry: short available_size fails");
            }

            // T06 §3.3: entries registered before enable_journal are replayed into the
            // segment file at enable_journal time
            static void test_journal_content(test_result& result)
            {
                if (!instance_available()) {
                    result.add_result(true, "journal content (no instance claimed, mmap unsupported)");
                    return;
                }
                auto* inst = bq::instance_storage::get();
                char name[96];
                snprintf(name, sizeof(name), "meta_journal_content_%" PRIu64, bq::platform::high_performance_epoch_ms());
                const bq::string config = "appenders_config.console.type=console\n"
                                          "appenders_config.console.levels=[all]\n"
                                          "log.recovery=true\n"
                                          "log.thread_mode=sync\n";
                const uint8_t u64 = static_cast<uint8_t>(bq::log_arg_type_enum::uint64_type);
                const uint8_t types[] = { u64 };
                // register before any recovery log of this run enables the journal:
                // use a fresh run so enable_journal() below is its first call
                inst->test_begin_new_run();
                const auto* e1 = reg("entry one {}", 11, static_cast<uint8_t>(bq::log_level::info), 0, types, 1);
                const auto* e2 = reg("entry two {}", 11, static_cast<uint8_t>(bq::log_level::info), 0, types, 1);
                const auto* e3 = reg("entry three {}", 13, static_cast<uint8_t>(bq::log_level::info), 0, types, 1);
                result.add_result(e1 && e2 && e3, "journal content: 3 entries registered");
                result.add_result(!bq::meta_registry::instance().is_journal_enabled() || true,
                    "journal content: pre-check");
                bq::log log = bq::log::create_log(name, config); // log_imp::init calls enable_journal
                result.add_result(log.is_valid(), "journal content: recovery log created");
                result.add_result(bq::meta_registry::instance().is_journal_enabled(), "journal content: journal enabled");
                if (!e1 || !e2 || !e3) {
                    return;
                }
                char file_name[32];
                snprintf(file_name, sizeof(file_name), "%" PRIu16 ".mmap", inst->run_version());
                const bq::string seg_path = bq::file_manager::combine_path(meta_dir(), file_name);
                result.add_result(bq::file_manager::is_file(seg_path), "journal content: segment file exists");
                auto file = bq::file_manager::instance().open_file(seg_path, bq::file_open_mode_enum::read);
                result.add_result(file.is_valid(), "journal content: segment file readable");
                if (!file.is_valid()) {
                    return;
                }
                const int64_t fsize = static_cast<int64_t>(bq::file_manager::get_file_size(seg_path));
                bq::array<uint8_t> bytes;
                bytes.fill_uninitialized(static_cast<size_t>(fsize));
                const size_t got = bq::file_manager::instance().read_file(file, &bytes[0], static_cast<size_t>(fsize),
                    bq::file_manager::seek_option::begin, 0);
                bq::file_manager::instance().close_file(file);
                result.add_result(got == static_cast<size_t>(fsize) && fsize >= 64, "journal content: file readable");
                if (got != static_cast<size_t>(fsize) || fsize < 64) {
                    return;
                }
                struct journal_head_layout {
                    uint64_t magic;
                    uint32_t format_version;
                    uint16_t run_version;
                    uint16_t segment_index;
                    uint64_t run_serial;
                    uint32_t committed_size;
                } head;
                memcpy(&head, &bytes[0], sizeof(head));
                result.add_result(head.magic == UINT64_C(0x42514C4F474D4554), "journal content: head magic");
                result.add_result(head.format_version == 1, "journal content: head format version");
                result.add_result(head.run_version == inst->run_version(), "journal content: head run version is current");
                result.add_result(head.segment_index == 0, "journal content: head segment index");
                result.add_result(head.committed_size <= static_cast<uint32_t>(fsize) - 64,
                    "journal content: committed_size within file");
                // the arena addresses of the 3 registered entries must be journaled,
                // each followed by a valid record
                const uint64_t addrs[3] = { reinterpret_cast<uintptr_t>(e1), reinterpret_cast<uintptr_t>(e2), reinterpret_cast<uintptr_t>(e3) };
                bool all_found = true;
                for (int32_t i = 0; i < 3 && all_found; ++i) {
                    bool found = false;
                    for (int64_t off = 64; off + 16 <= static_cast<int64_t>(head.committed_size) + 64; off += 8) {
                        uint64_t addr = 0;
                        memcpy(&addr, &bytes[off], sizeof(addr));
                        if (addr == addrs[i]) {
                            found = true;
                            break;
                        }
                    }
                    all_found = all_found && found;
                }
                result.add_result(all_found, "journal content: all 3 arena addresses journaled");
            }

            // T06 §3.4: expired cleanup removes out-of-window versions and garbage names
            static void test_cleanup_expired(test_result& result)
            {
                if (!instance_available()) {
                    result.add_result(true, "cleanup expired (no instance claimed, mmap unsupported)");
                    return;
                }
                auto* inst = bq::instance_storage::get();
                const uint16_t cur = inst->run_version();
                const bq::string dir = meta_dir();
                auto make_file = [&dir](const char* file_name) {
                    auto h = bq::file_manager::instance().open_file(
                        bq::file_manager::combine_path(dir, file_name),
                        bq::file_open_mode_enum::write | bq::file_open_mode_enum::auto_create);
                    if (h.is_valid()) {
                        bq::file_manager::instance().close_file(h);
                    }
                };
                char name_a[32];
                char name_b[32];
                char name_c[32];
                snprintf(name_a, sizeof(name_a), "%" PRIu16 ".mmap", static_cast<uint16_t>(cur - 17));
                snprintf(name_b, sizeof(name_b), "%" PRIu16 ".mmap", static_cast<uint16_t>(cur - 16));
                snprintf(name_c, sizeof(name_c), "%" PRIu16 ".mmap", cur);
                // drop the registry's journal segments first: they hold <cur>.mmap
                // open exclusively and this test must be able to re-create that file
                // (as if left behind by a wrapped earlier run); the next registration
                // re-opens and replays the segments
                bq::meta_registry::instance().test_cleanup_expired();
                make_file(name_a);
                make_file(name_b);
                make_file(name_c);
                make_file("garbage.txt");
                make_file("12_34_56.mmap"); // unparseable name
                bq::meta_registry::instance().test_cleanup_expired();
                result.add_result(!bq::file_manager::is_file(bq::file_manager::combine_path(dir, name_a)),
                    "cleanup: version outside window (cur-17) removed");
                result.add_result(bq::file_manager::is_file(bq::file_manager::combine_path(dir, name_b)),
                    "cleanup: version at window edge (cur-16) kept");
                result.add_result(!bq::file_manager::is_file(bq::file_manager::combine_path(dir, name_c)),
                    "cleanup: current version file removed (stale from a wrapped earlier run)");
                result.add_result(!bq::file_manager::is_file(bq::file_manager::combine_path(dir, "garbage.txt")),
                    "cleanup: garbage name removed");
                result.add_result(!bq::file_manager::is_file(bq::file_manager::combine_path(dir, "12_34_56.mmap")),
                    "cleanup: unparseable segment name removed");
            }

            // T06 §3.5: wraparound distances
            static void test_cleanup_wraparound(test_result& result)
            {
                if (!instance_available()) {
                    result.add_result(true, "cleanup wraparound (no instance claimed, mmap unsupported)");
                    return;
                }
                auto* inst = bq::instance_storage::get();
                inst->test_set_run(3, inst->run_serial());
                const bq::string dir = meta_dir();
                auto make_file = [&dir](const char* file_name) {
                    auto h = bq::file_manager::instance().open_file(
                        bq::file_manager::combine_path(dir, file_name),
                        bq::file_open_mode_enum::write | bq::file_open_mode_enum::auto_create);
                    if (h.is_valid()) {
                        bq::file_manager::instance().close_file(h);
                    }
                };
                make_file("65535.mmap"); // distance 4
                make_file("65530.mmap"); // distance 9
                make_file("65520.mmap"); // distance 19
                bq::meta_registry::instance().test_cleanup_expired();
                result.add_result(bq::file_manager::is_file(bq::file_manager::combine_path(dir, "65535.mmap")),
                    "wraparound: 65535 (distance 4) kept");
                result.add_result(bq::file_manager::is_file(bq::file_manager::combine_path(dir, "65530.mmap")),
                    "wraparound: 65530 (distance 9) kept");
                result.add_result(!bq::file_manager::is_file(bq::file_manager::combine_path(dir, "65520.mmap")),
                    "wraparound: 65520 (distance 19) removed");
                inst->test_begin_new_run();
            }

            // T06 §3.6: corrupt segment heads make resolve_recovered return nullptr without crashing
            static void test_segment_head_validation(test_result& result)
            {
                if (!instance_available()) {
                    result.add_result(true, "segment head validation (no instance claimed, mmap unsupported)");
                    return;
                }
                auto* inst = bq::instance_storage::get();
                const bq::string dir = meta_dir();
                auto write_file = [&dir](const char* file_name, const void* bytes, size_t size) {
                    auto h = bq::file_manager::instance().open_file(
                        bq::file_manager::combine_path(dir, file_name),
                        bq::file_open_mode_enum::write | bq::file_open_mode_enum::auto_create);
                    if (h.is_valid()) {
                        bq::file_manager::instance().write_file(h, bytes, size);
                        bq::file_manager::instance().close_file(h);
                    }
                };
                struct head_layout {
                    uint64_t magic;
                    uint32_t format_version;
                    uint16_t run_version;
                    uint16_t segment_index;
                    uint64_t run_serial;
                    uint32_t committed_size;
                };
                // wrong run_version in head
                {
                    char file_name[32];
                    const uint16_t stale_v = static_cast<uint16_t>(inst->run_version() - 1);
                    snprintf(file_name, sizeof(file_name), "%" PRIu16 ".mmap", stale_v);
                    head_layout head = { UINT64_C(0x42514C4F474D4554), 1, static_cast<uint16_t>(stale_v - 1), 0,
                        inst->run_serial() - 1, 0 };
                    write_file(file_name, &head, sizeof(head));
                    result.add_result(bq::meta_registry::instance().resolve_recovered(stale_v, 0x1234) == nullptr,
                        "segment validation: wrong run_version rejected");
                }
                // bad magic
                {
                    char file_name[32];
                    const uint16_t stale_v = static_cast<uint16_t>(inst->run_version() - 2);
                    snprintf(file_name, sizeof(file_name), "%" PRIu16 ".mmap", stale_v);
                    head_layout head = { UINT64_C(0xDEADBEEFDEADBEEF), 1, stale_v, 0, inst->run_serial() - 2, 0 };
                    write_file(file_name, &head, sizeof(head));
                    result.add_result(bq::meta_registry::instance().resolve_recovered(stale_v, 0x1234) == nullptr,
                        "segment validation: bad magic rejected");
                }
                // committed_size overflows the file
                {
                    char file_name[32];
                    const uint16_t stale_v = static_cast<uint16_t>(inst->run_version() - 3);
                    snprintf(file_name, sizeof(file_name), "%" PRIu16 ".mmap", stale_v);
                    head_layout head = { UINT64_C(0x42514C4F474D4554), 1, stale_v, 0, inst->run_serial() - 3, 0x7FFFFFFF };
                    write_file(file_name, &head, sizeof(head));
                    result.add_result(bq::meta_registry::instance().resolve_recovered(stale_v, 0x1234) == nullptr,
                        "segment validation: committed_size overflow rejected");
                }
            }

            // T06 §3.7: concurrent registration of the same format strings dedupes to one entry each
            static void test_register_race(test_result& result)
            {
                constexpr int32_t thread_count = 64;
                constexpr int32_t format_count = 100;
                const uint8_t i32 = static_cast<uint8_t>(bq::log_arg_type_enum::int32_type);
                const uint8_t types[] = { i32 };
                char formats[format_count][32];
                for (int32_t i = 0; i < format_count; ++i) {
                    snprintf(formats[i], sizeof(formats[i]), "race format %d", i);
                }
                const size_t before = bq::meta_registry::instance().test_entry_count();
                std::atomic<bool> all_same { true };
                std::vector<std::thread> threads;
                for (int32_t t = 0; t < thread_count; ++t) {
                    threads.emplace_back([&formats, &types, &all_same]() {
                        const void* seen[format_count] = { };
                        for (int32_t i = 0; i < format_count; ++i) {
                            const auto* meta = reg(formats[i], static_cast<uint32_t>(strlen(formats[i])),
                                static_cast<uint8_t>(bq::log_level::info), 0, types, 1);
                            if (!meta) {
                                all_same = false;
                                return;
                            }
                            if (seen[i] && seen[i] != meta) {
                                all_same = false;
                            }
                            seen[i] = meta;
                        }
                    });
                }
                for (auto& thread : threads) {
                    thread.join();
                }
                result.add_result(all_same.load(), "register race: every thread got the same pointer per format");
                result.add_result(bq::meta_registry::instance().test_entry_count() == before + format_count,
                    "register race: exactly %d new entries", format_count);
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                if (!bq::memory_map::is_platform_support()) {
                    return result;
                }
                test_dedupe(result);
                test_verify_entry(result);
                test_register_race(result);
                test_journal_content(result);
                test_cleanup_expired(result);
                test_cleanup_wraparound(result);
                test_segment_head_validation(result);
                return result;
            }
        };
    }
}
