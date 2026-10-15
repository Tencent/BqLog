#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "test_base.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_file_compressed.h"
#include "bq_log/log/log_imp.h"

namespace bq {
    namespace test {
        class test_compressed_cache : public test_base {
        private:
            struct file_case_result {
                bool success = false;
                uint64_t file_size = 0;
                uint64_t decoded_count = 0;
                uint32_t file_count = 0;
            };

            static uint64_t splitmix64(uint64_t value)
            {
                value += UINT64_C(0x9E3779B97F4A7C15);
                value = (value ^ (value >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
                value = (value ^ (value >> 27)) * UINT64_C(0x94D049BB133111EB);
                return value ^ (value >> 31);
            }

            static std::vector<std::string> make_diverse_formats(uint32_t count)
            {
                static const char* subsystems[] = {
                    "network", "render", "physics", "audio",
                    "script", "storage", "input", "scene"
                };
                static const char* actions[] = {
                    "recv", "submit", "solve", "mix",
                    "dispatch", "write", "sample", "update"
                };

                std::vector<std::string> formats;
                formats.reserve(count);
                char buffer[256];
                for (uint32_t i = 0; i < count; ++i) {
                    const uint64_t a = splitmix64(static_cast<uint64_t>(i) * 3);
                    const uint64_t b = splitmix64(static_cast<uint64_t>(i) * 3 + 1);
                    const uint64_t c = splitmix64(static_cast<uint64_t>(i) * 3 + 2);
                    const uint32_t padding_size = static_cast<uint32_t>((c >> 9) & 15);
                    char padding[20];
                    for (uint32_t p = 0; p < padding_size; ++p) {
                        padding[p] = static_cast<char>('a' + ((a >> (p & 7)) + p + i) % 26);
                    }
                    padding[padding_size] = '\0';
                    snprintf(
                        buffer,
                        sizeof(buffer),
                        "%s_%08X %s_%08X value={} code={} tail_%08X_%s",
                        subsystems[(a >> 4) & 7],
                        static_cast<unsigned>(a),
                        actions[(b >> 7) & 7],
                        static_cast<unsigned>(b >> 32),
                        static_cast<unsigned>(c),
                        padding);
                    formats.emplace_back(buffer);
                }
                return formats;
            }

            static bq::string make_parent_config()
            {
                return R"(
                    log.thread_mode=sync
                    log.recovery=false
                    appenders_config.console.type=console
                    appenders_config.console.levels=[all]
                    appenders_config.console.enable=false
                )";
            }

            static bq::string make_appender_config(
                const bq::string& file_name,
                const bq::string& extra_config)
            {
                return bq::string(R"(
                    type=compressed_file
                    levels=[all]
                    file_name=)")
                    + file_name
                    + R"(
                    base_dir_type=0
                    always_create_new_file=true
                    enable_rolling_log_file=false
                    )"
                    + extra_config;
            }

            static void test_config_values(test_result& result)
            {
                bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("compressed_cache_test", 0));
                log_imp parent_log;
                bq::array<bq::string> categories;
                categories.push_back("");
                const auto parent_config = bq::property_value::create_from_string(make_parent_config());
                bool success = parent_log.init("compressed_cache_config_parent", parent_config, categories);

                {
                    appender_file_compressed appender;
                    const auto config = bq::property_value::create_from_string(
                        make_appender_config("compressed_cache_test/default", ""));
                    success &= appender.init("default", config, &parent_log);
                    result.add_result(
                        success
                            && appender.get_format_template_cache_max_entries_for_test()
                                == appender_file_compressed::DEFAULT_FORMAT_TEMPLATE_CACHE_MAX_ENTRIES
                            && appender.get_thread_info_cache_max_entries_for_test()
                                == appender_file_compressed::DEFAULT_THREAD_INFO_CACHE_MAX_ENTRIES,
                        "compressed cache default config values");
                }

                {
                    appender_file_compressed appender;
                    const auto config = bq::property_value::create_from_string(
                        make_appender_config(
                            "compressed_cache_test/custom",
                            R"(
                                format_template_cache_max_entries=4096
                                thread_info_cache_max_entries=128
                            )"));
                    const bool init_ok = appender.init("custom", config, &parent_log);
                    result.add_result(
                        init_ok
                            && appender.get_format_template_cache_max_entries_for_test() == 4096
                            && appender.get_thread_info_cache_max_entries_for_test() == 128,
                        "compressed cache custom config values");
                    const auto changed_config = bq::property_value::create_from_string(
                        make_appender_config(
                            "compressed_cache_test/custom",
                            R"(
                                format_template_cache_max_entries=8192
                                thread_info_cache_max_entries=128
                            )"));
                    result.add_result(
                        init_ok
                            && appender.appender_base::reset(changed_config)
                            && appender.get_format_template_cache_max_entries_for_test() == 8192
                            && appender.get_thread_info_cache_max_entries_for_test() == 128,
                        "compressed cache capacity reset config");
                }

                {
                    appender_file_compressed appender;
                    const auto config = bq::property_value::create_from_string(
                        make_appender_config(
                            "compressed_cache_test/clamped",
                            R"(
                                format_template_cache_max_entries=1
                                thread_info_cache_max_entries=2
                            )"));
                    const bool init_ok = appender.init("clamped", config, &parent_log);
                    result.add_result(
                        init_ok
                            && appender.get_format_template_cache_max_entries_for_test() == 8
                            && appender.get_thread_info_cache_max_entries_for_test() == 8,
                        "compressed cache config minimum clamp");
                }

                {
                    appender_file_compressed appender;
                    const auto config = bq::property_value::create_from_string(
                        make_appender_config(
                            "compressed_cache_test/invalid",
                            R"(
                                format_template_cache_max_entries=invalid
                                thread_info_cache_max_entries=invalid
                            )"));
                    const bool init_ok = appender.init("invalid", config, &parent_log);
                    result.add_result(
                        init_ok
                            && appender.get_format_template_cache_max_entries_for_test()
                                == appender_file_compressed::DEFAULT_FORMAT_TEMPLATE_CACHE_MAX_ENTRIES
                            && appender.get_thread_info_cache_max_entries_for_test()
                                == appender_file_compressed::DEFAULT_THREAD_INFO_CACHE_MAX_ENTRIES,
                        "compressed cache invalid config fallback");
                }

                {
                    appender_file_compressed appender;
                    const auto config = bq::property_value::create_from_string(
                        make_appender_config(
                            "compressed_cache_test/max_clamped",
                            R"(
                                format_template_cache_max_entries=20000000
                                thread_info_cache_max_entries=2000000
                            )"));
                    const bool init_ok = appender.init("max_clamped", config, &parent_log);
                    result.add_result(
                        init_ok
                            && appender.get_format_template_cache_max_entries_for_test()
                                == 16 * 1024 * 1024
                            && appender.get_thread_info_cache_max_entries_for_test()
                                == 1024 * 1024,
                        "compressed cache config maximum clamp");
                }
            }

            static bq::array<bq::string> get_compressed_files(
                const bq::string& directory,
                const bq::string& prefix)
            {
                bq::array<bq::string> result;
                const bq::array<bq::string> names = bq::file_manager::get_sub_dirs_and_files_name(directory);
                for (const auto& name : names) {
                    if (!name.begin_with(prefix) || !name.end_with(".logcompr")) {
                        continue;
                    }
                    const bq::string full_path = bq::file_manager::combine_path(directory, name);
                    if (bq::file_manager::is_file(full_path)) {
                        result.push_back(full_path);
                    }
                }
                return result;
            }

            static file_case_result decode_files(
                const bq::string& directory,
                const bq::string& prefix)
            {
                file_case_result result;
                const auto files = get_compressed_files(directory, prefix);
                result.file_count = static_cast<uint32_t>(files.size());
                result.success = !files.is_empty();
                for (const auto& file_path : files) {
                    result.file_size += bq::file_manager::get_file_size(file_path);
                    bq::tools::log_decoder decoder(file_path);
                    while (true) {
                        const auto decode_result = decoder.decode();
                        if (decode_result == bq::appender_decode_result::eof) {
                            break;
                        }
                        if (decode_result != bq::appender_decode_result::success) {
                            result.success = false;
                            break;
                        }
                        ++result.decoded_count;
                    }
                }
                return result;
            }

            static void close_file_appender(bq::log& log_obj)
            {
                log_obj.force_flush();
                log_obj.reset_config(R"(
                    log.thread_mode=sync
                    appenders_config.console_only.type=console
                    appenders_config.console_only.levels=[all]
                    appenders_config.console_only.enable=false
                )");
            }

            static void write_format_passes(
                bq::log& log_obj,
                const std::vector<std::string>& formats,
                uint32_t first_pass,
                uint32_t pass_count)
            {
                for (uint32_t pass = first_pass; pass < first_pass + pass_count; ++pass) {
                    for (uint32_t i = 0; i < static_cast<uint32_t>(formats.size()); ++i) {
                        log_obj.info(
                            formats[i],
                            static_cast<uint64_t>(pass) * formats.size() + i,
                            i);
                    }
                }
            }

            static file_case_result run_reopen_case(
                const bq::string& case_name,
                uint32_t format_cache_max_entries)
            {
                const bq::string directory = TO_ABSOLUTE_PATH("compressed_cache_test", 0);
                const bq::string file_prefix = case_name;
                const bq::string file_name = bq::file_manager::combine_path("compressed_cache_test", file_prefix);
                const auto formats = make_diverse_formats(512);

                char config_buffer[2048];
                snprintf(
                    config_buffer,
                    sizeof(config_buffer),
                    "log.thread_mode=sync\n"
                    "log.recovery=false\n"
                    "appenders_config.file.type=compressed_file\n"
                    "appenders_config.file.levels=[all]\n"
                    "appenders_config.file.file_name=%s\n"
                    "appenders_config.file.base_dir_type=0\n"
                    "appenders_config.file.always_create_new_file=false\n"
                    "appenders_config.file.enable_rolling_log_file=false\n"
                    "appenders_config.file.format_template_cache_max_entries=%" PRIu32 "\n"
                    "appenders_config.file.thread_info_cache_max_entries=8\n",
                    file_name.c_str(),
                    format_cache_max_entries);

                {
                    bq::log first_log = bq::log::create_log(case_name + "_first", config_buffer);
                    write_format_passes(first_log, formats, 0, 2);
                    close_file_appender(first_log);
                }
                {
                    bq::log second_log = bq::log::create_log(case_name + "_second", config_buffer);
                    write_format_passes(second_log, formats, 2, 2);
                    close_file_appender(second_log);
                }

                return decode_files(directory, file_prefix);
            }

            static void test_overflow_and_reopen(test_result& result)
            {
                bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("compressed_cache_test", 0));
                const auto bounded = run_reopen_case("bounded_reopen", 8);
                const auto roomy = run_reopen_case("roomy_reopen", 1024);
                const uint64_t expected_count = 512 * 4;

                result.add_result(
                    bounded.success
                        && bounded.file_count == 1
                        && bounded.decoded_count == expected_count,
                    "compressed bounded cache overflow and reopen decode");
                result.add_result(
                    roomy.success
                        && roomy.file_count == 1
                        && roomy.decoded_count == expected_count,
                    "compressed roomy cache reopen decode");
                result.add_result(
                    bounded.file_size > roomy.file_size,
                    "compressed bounded cache overflow increases file size");
            }

            static void test_rolling_decode(test_result& result)
            {
                bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("compressed_cache_test", 0));
                const bq::string directory = TO_ABSOLUTE_PATH("compressed_cache_test", 0);
                const bq::string file_name = "compressed_cache_test/rolling";
                const auto formats = make_diverse_formats(256);
                bq::log log_obj = bq::log::create_log("compressed_cache_rolling", R"(
                    log.thread_mode=sync
                    log.recovery=false
                    appenders_config.file.type=compressed_file
                    appenders_config.file.levels=[all]
                    appenders_config.file.file_name=compressed_cache_test/rolling
                    appenders_config.file.base_dir_type=0
                    appenders_config.file.always_create_new_file=true
                    appenders_config.file.enable_rolling_log_file=false
                    appenders_config.file.max_file_size=16384
                    appenders_config.file.format_template_cache_max_entries=32
                    appenders_config.file.thread_info_cache_max_entries=8
                )");
                write_format_passes(log_obj, formats, 0, 8);
                close_file_appender(log_obj);

                const auto decoded = decode_files(directory, "rolling");
                result.add_result(
                    decoded.success
                        && decoded.file_count > 1
                        && decoded.decoded_count == 256 * 8,
                    "compressed bounded cache rolling decode");
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                test_config_values(result);
                test_overflow_and_reopen(result);
                test_rolling_decode(result);
                return result;
            }
        };
    }
}
