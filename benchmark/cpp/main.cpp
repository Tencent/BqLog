#if defined(WIN32)
#include <windows.h>
#endif
#include "bq_log/bq_log.h"
#include "multi_format_templates.h"
#include "write_thread_cost.h"
#include <cstdint>
#include <cstdlib>
#include <stdio.h>
#include <thread>
#include <chrono>
#include <string>
#include <iostream>
#include <vector>

static bq::log compressed_log = bq::log::create_log("compress", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/compress
		appenders_config.appender_3.capacity_limit=1
    )");
static bq::log compressed_enc_log = bq::log::create_log("compress_enc", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/compress_enc
		appenders_config.appender_3.capacity_limit=1
        appenders_config.appender_3.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+FonyOHuS2uC6IZc16bfd6qQk4ykBOt3nTfBFcNr8ZWvvcf4H0hFkrpMtQ0AJO057GhVTQCCfnvfStSq2Yra+O5VGpI5Q6NLrUuVERimjNgwtxbXt3P8Nw87jEIJiY/8m2FUXhZEPwoA7t+2/953cNE1itJskJtojwaUlMN0dXBJxs4NP8MfBPPZQ5vNV8xgEf1SCQzQBAJsofy1kPHHqJNBXUBsNA44SP5H95JOz+r0oaNkYxT88Zk4tbk5N3hk5aXyZVp49OqhrXCPf5owDa4Lqk4UzVTk9EimxvtSuiUTzr7IJhHYy7jsGnSgq6dH0xlUfxKeX pippocao@PIPPOCAO-PC6
	)");
static bq::log text_log = bq::log::create_log("text", R"(
        appenders_config.appender_3.type=text_file
        appenders_config.appender_3.levels=[all]
        appenders_config.appender_3.file_name= benchmark_output/text
        appenders_config.appender_3.capacity_limit=1
    )");
static bq::log compressed_log_ascii_utf8 = bq::log::create_log("test_ascii_u8", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_ascii_u8
		appenders_config.appender_3.capacity_limit=1
	)");
static bq::log compressed_log_ascii_utf16 = bq::log::create_log("test_ascii_u16", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_ascii_u16
		appenders_config.appender_3.capacity_limit=1
    )");
static bq::log compressed_log_chinese_utf16 = bq::log::create_log("test_chinese_u16", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_chinese_u16
		appenders_config.appender_3.capacity_limit=1
    )");
static bq::log compressed_log_mixed_utf16 = bq::log::create_log("test_mixed_u16", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_mixed_u16
		appenders_config.appender_3.capacity_limit=1
	)");
static bq::log compressed_log_cstr_utf16 = bq::log::create_log("test_cstr_u16", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_cstr_u16
		appenders_config.appender_3.capacity_limit=1
	)");
static bq::log compressed_log_cstr_utf32 = bq::log::create_log("test_cstr_u32", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_cstr_u32
		appenders_config.appender_3.capacity_limit=1
	)");
static bq::log compressed_log_multi_format = bq::log::create_log("test_multi_format", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_multi_format
		appenders_config.appender_3.capacity_limit=1
	)");
static bq::log write_thread_cost_log = bq::log::create_log("write_thread_cost", R"(
        appenders_config.appender_3.type=text_file
        appenders_config.appender_3.levels=[all]
        appenders_config.appender_3.file_name= benchmark_output/write_thread_cost
        appenders_config.appender_3.capacity_limit=1
    )");

static bq::string ascii_charset;
static bq::u16string ascii_charset_u16;
static bq::u16string chinese_charset_u16;
static bq::u16string mixed_charset_u16;
static bq::array<bq::tuple<size_t, size_t>> positions;
static constexpr size_t character_pool_size = 1024 * 1024 * 8;
static constexpr size_t logs_count = 2000000;

// Terminated strings laid end to end, so their start alignment varies with the lengths before them.
static constexpr size_t cstr_count = 50000;
static constexpr size_t cstr_max_length = 128;
static std::vector<char16_t> cstr_pool_u16;
static std::vector<char32_t> cstr_pool_u32;
static std::vector<size_t> cstr_starts;

template <typename CHAR>
struct benchmark_string_view {
public:
    using value_type = CHAR;
    using size_type = size_t;

private:
    const CHAR* data_;
    size_t size_;

public:
    benchmark_string_view(const CHAR* data, size_t size)
        : data_(data)
        , size_(size)
    {
    }
    const CHAR* data() const
    {
        return data_;
    }
    size_t size() const
    {
        return size_;
    }
};

static size_t get_template_pool_size()
{
    const char* env = getenv("BENCH_POOL_SIZE");
    if (env && env[0]) {
        long long v = atoll(env);
        if (v > 0 && (unsigned long long)v <= (unsigned long long)logs_count) {
            return (size_t)v;
        }
    }
    return 50000;
}

static void prepare_datas()
{
    ascii_charset.fill_uninitialized(character_pool_size);
    ascii_charset_u16.fill_uninitialized(character_pool_size);
    chinese_charset_u16.fill_uninitialized(character_pool_size);
    mixed_charset_u16.fill_uninitialized(character_pool_size);
    for (size_t character_index = 0; character_index < character_pool_size; ++character_index) {
        ascii_charset[character_index] = (char)((character_index % 95) + 32);
        ascii_charset_u16[character_index] = (char16_t)((character_index % 95) + 32);
        chinese_charset_u16[character_index] = (char16_t)(0x4E00 + (character_index % 20902));
        if ((character_index % 200) < 100) {
            mixed_charset_u16[character_index] = (char16_t)((character_index % 95) + 32);
        } else {
            mixed_charset_u16[character_index] = (char16_t)(0x4E00 + (character_index % 20902));
        }
    }
    for (size_t log_index = 0; log_index < logs_count; ++log_index) {
        size_t start = (log_index * 9973) % (character_pool_size - 1);
        size_t max_size = (character_pool_size - 1 - start);
        size_t data_size = log_index % 1024;
        data_size = bq::min_value(data_size, max_size);
        positions.push_back(bq::make_tuple(start, data_size));
    }
    for (size_t string_index = 0; string_index < cstr_count; ++string_index) {
        const size_t length = (string_index * 37) % cstr_max_length;
        cstr_starts.push_back(cstr_pool_u32.size());
        for (size_t character_index = 0; character_index < length; ++character_index) {
            const char32_t c = (char32_t)(0x4E00 + ((string_index + character_index) % 20902));
            cstr_pool_u16.push_back((char16_t)c);
            cstr_pool_u32.push_back(c);
        }
        cstr_pool_u16.push_back(0);
        cstr_pool_u32.push_back(0);
    }
}

template <typename CHAR>
static void test_compress_c_string(int32_t thread_count, const char* log_name, const char* title, const std::vector<CHAR>& pool)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Test Compressed File Log " << title << "=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name(log_name);
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([&log_obj, &pool]() {
            for (size_t log_index = 0; log_index < logs_count; ++log_index) {
                const CHAR* text = pool.data() + cstr_starts[log_index % cstr_count];
                log_obj.info("{}", text);
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_ascii_utf8(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Test Compressed File Log ASCII UTF8=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("test_ascii_u8");
    const size_t template_pool_ = get_template_pool_size();
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([&log_obj, template_pool_]() {
            for (size_t log_index = 0; log_index < logs_count; ++log_index) {
                log_obj.info(benchmark_string_view<char>(ascii_charset.begin() + bq::get<0>(positions[log_index % template_pool_]), bq::get<1>(positions[log_index % template_pool_])));
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_ascii_utf16(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Test Compressed File Log ASCII UTF16=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("test_ascii_u16");
    const size_t template_pool_ = get_template_pool_size();
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([&log_obj, template_pool_]() {
            for (size_t log_index = 0; log_index < logs_count; ++log_index) {
                log_obj.info(benchmark_string_view<char16_t>(ascii_charset_u16.begin() + bq::get<0>(positions[log_index % template_pool_]), bq::get<1>(positions[log_index % template_pool_])));
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_chinese_utf16(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Test Compressed File Log CHINESE UTF16=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("test_chinese_u16");
    const size_t template_pool_ = get_template_pool_size();
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([&log_obj, template_pool_]() {
            for (size_t log_index = 0; log_index < logs_count; ++log_index) {
                log_obj.info(benchmark_string_view<char16_t>(chinese_charset_u16.begin() + bq::get<0>(positions[log_index % template_pool_]), bq::get<1>(positions[log_index % template_pool_])));
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_mixed_utf16(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Test Compressed File Log MIXED(ASCII + CHINESE) UTF16=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("test_mixed_u16");
    const size_t template_pool_ = get_template_pool_size();
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([&log_obj, template_pool_]() {
            for (size_t log_index = 0; log_index < logs_count; ++log_index) {
                log_obj.info(benchmark_string_view<char16_t>(mixed_charset_u16.begin() + bq::get<0>(positions[log_index % template_pool_]), bq::get<1>(positions[log_index % template_pool_])));
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_multi_param(int32_t thread_count, write_thread_cost::write_mode mode)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Compressed File Log Test 1, 4 params=========" << (mode == write_thread_cost::write_mode::fast ? " (BQ_LOG_FAST_INFO)" : " (log.info)") << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress");
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([thread_index, &log_obj, mode]() {
            if (mode == write_thread_cost::write_mode::fast) {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    BQ_LOG_FAST_INFO(log_obj, "idx:{}, num:{}, This test, {}, {}", thread_index, log_index, 2.4232f, true);
                }
            } else {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    log_obj.info("idx:{}, num:{}, This test, {}, {}", thread_index, log_index, 2.4232f, true);
                }
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_enc_multi_param(int32_t thread_count, write_thread_cost::write_mode mode)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Encrypted Compressed File Log Test 1, 4 params=========" << (mode == write_thread_cost::write_mode::fast ? " (BQ_LOG_FAST_INFO)" : " (log.info)") << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress_enc");
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([thread_index, &log_obj, mode]() {
            if (mode == write_thread_cost::write_mode::fast) {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    BQ_LOG_FAST_INFO(log_obj, "idx:{}, num:{}, This test, {}, {}", thread_index, log_index, 2.4232f, true);
                }
            } else {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    log_obj.info("idx:{}, num:{}, This test, {}, {}", thread_index, log_index, 2.4232f, true);
                }
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_text_multi_param(int32_t thread_count, write_thread_cost::write_mode mode)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "============Begin Text File Log Test 2, 4 params============" << (mode == write_thread_cost::write_mode::fast ? " (BQ_LOG_FAST_INFO)" : " (log.info)") << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("text");
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([thread_index, &log_obj, mode]() {
            if (mode == write_thread_cost::write_mode::fast) {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    BQ_LOG_FAST_INFO(log_obj, "idx:{}, num:{}, This test, {}, {}", thread_index, log_index, 2.4232f, true);
                }
            } else {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    log_obj.info("idx:{}, num:{}, This test, {}, {}", thread_index, log_index, 2.4232f, true);
                }
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_no_param(int32_t thread_count, write_thread_cost::write_mode mode)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Compressed File Log Test 3, no param=========" << (mode == write_thread_cost::write_mode::fast ? " (BQ_LOG_FAST_INFO)" : " (log.info)") << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress");
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([thread_index, &log_obj, mode]() {
            if (mode == write_thread_cost::write_mode::fast) {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    BQ_LOG_FAST_INFO(log_obj, "Empty Log, No Param");
                }
            } else {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    log_obj.info("Empty Log, No Param");
                }
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_enc_no_param(int32_t thread_count, write_thread_cost::write_mode mode)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Encrypted Compressed File Log Test 3, no param=========" << (mode == write_thread_cost::write_mode::fast ? " (BQ_LOG_FAST_INFO)" : " (log.info)") << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress_enc");
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([thread_index, &log_obj, mode]() {
            if (mode == write_thread_cost::write_mode::fast) {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    BQ_LOG_FAST_INFO(log_obj, "Empty Log, No Param");
                }
            } else {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    log_obj.info("Empty Log, No Param");
                }
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_text_no_param(int32_t thread_count, write_thread_cost::write_mode mode)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "============Begin Text File Log Test 4, no param============" << (mode == write_thread_cost::write_mode::fast ? " (BQ_LOG_FAST_INFO)" : " (log.info)") << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("text");
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([thread_index, &log_obj, mode]() {
            if (mode == write_thread_cost::write_mode::fast) {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    BQ_LOG_FAST_INFO(log_obj, "Empty Log, No Param");
                }
            } else {
                for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                    log_obj.info("Empty Log, No Param");
                }
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_multi_format_single(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "======Begin Compressed Multi-Format Test: SINGLE template======" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("test_multi_format");
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries (1 template), please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([&log_obj]() {
            for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                bench_log_multi_format(log_obj, 0, log_index); // always the same template
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_multi_format_roundrobin(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "===Begin Compressed Multi-Format Test: ROUND-ROBIN (no locality)===" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("test_multi_format");
    // warm up: make sure all templates are already registered so we measure steady state
    for (int32_t format_index = 0; format_index < BENCH_MULTI_FORMAT_COUNT; ++format_index) {
        bench_log_multi_format(log_obj, format_index, 0);
    }
    bq::log::force_flush_all_logs();
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries (" << BENCH_MULTI_FORMAT_COUNT << " templates, round-robin), please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([&log_obj]() {
            for (int32_t log_index = 0; log_index < 2000000; ++log_index) {
                bench_log_multi_format(log_obj, log_index % BENCH_MULTI_FORMAT_COUNT, log_index);
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_multi_format_hotwindow(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "===Begin Compressed Multi-Format Test: HOT-WINDOW (locality)===" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("test_multi_format");
    // warm up: register all templates so the lookup structure is fully populated
    for (int32_t format_index = 0; format_index < BENCH_MULTI_FORMAT_COUNT; ++format_index) {
        bench_log_multi_format(log_obj, format_index, 0);
    }
    bq::log::force_flush_all_logs();
    constexpr int32_t hot_window_size = 8;
    constexpr int32_t burst_size = 64;
    std::vector<std::thread> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries (" << BENCH_MULTI_FORMAT_COUNT << " templates, hot-window W=" << hot_window_size << "), please wait the result..." << std::endl;
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index] = std::thread([&log_obj, hot_window_size, burst_size]() {
            int32_t window_base_index = 0;
            int32_t log_index = 0;
            while (log_index < 2000000) {
                for (int32_t burst_index = 0; burst_index < burst_size && log_index < 2000000; ++burst_index, ++log_index) {
                    bench_log_multi_format(log_obj, window_base_index + (log_index % hot_window_size), log_index);
                }
                window_base_index += hot_window_size;
                if (window_base_index > BENCH_MULTI_FORMAT_COUNT - hot_window_size) {
                    window_base_index = 0;
                }
            }
        });
    }
    for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads[thread_index].join();
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

int main()
{
#if defined(WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    std::cout << "Please input the number of threads which will write log simultaneously:" << std::endl;
    int32_t thread_count;
    std::cin >> thread_count;

    bq::log::force_flush_all_logs();
    prepare_datas();
    test_compress_ascii_utf8(thread_count);
    test_compress_ascii_utf16(thread_count);
    test_compress_chinese_utf16(thread_count);
    test_compress_mixed_utf16(thread_count);
    test_compress_c_string(thread_count, "test_cstr_u16", "C-STRING UTF16", cstr_pool_u16);
    test_compress_c_string(thread_count, "test_cstr_u32", "C-STRING UTF32", cstr_pool_u32);

    // Fixed format tests run twice: once through log.info() and once through BQ_LOG_FAST_INFO().
    const write_thread_cost::write_mode modes[] = { write_thread_cost::write_mode::normal, write_thread_cost::write_mode::fast };
    for (const auto mode : modes) {
        test_compress_multi_param(thread_count, mode);
        test_compress_enc_multi_param(thread_count, mode);
        test_text_multi_param(thread_count, mode);
        test_compress_no_param(thread_count, mode);
        test_compress_enc_no_param(thread_count, mode);
        test_text_no_param(thread_count, mode);
    }

    // Multi-format tests: same total log volume as the single-format tests above,
    // but spread across BENCH_MULTI_FORMAT_COUNT distinct format templates so the
    // compressed appender's format-template lookup is exercised with a large
    // template set. Single-format (test_compress_multi_param above) vs these gives
    // the comparison.
    test_compress_multi_format_single(thread_count);
    test_compress_multi_format_roundrobin(thread_count);
    test_compress_multi_format_hotwindow(thread_count);

    write_thread_cost::test(thread_count);
    return 0;
}
