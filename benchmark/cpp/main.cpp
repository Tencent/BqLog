#if defined(WIN32)
#include <windows.h>
#endif
#include "bq_log/bq_log.h"
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

static bq::string ascii_charset;
static bq::u16string ascii_charset_u16;
static bq::u16string chinese_charset_u16;
static bq::u16string mixed_charset_u16;
static bq::array<bq::tuple<size_t, size_t>> positions;
static constexpr size_t character_pool_size = 1024 * 1024 * 8;
static constexpr size_t logs_count = 2000000;

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

static void prepare_datas()
{
    ascii_charset.fill_uninitialized(character_pool_size);
    ascii_charset_u16.fill_uninitialized(character_pool_size);
    chinese_charset_u16.fill_uninitialized(character_pool_size);
    mixed_charset_u16.fill_uninitialized(character_pool_size);
    for (size_t i = 0; i < character_pool_size; ++i) {
        ascii_charset[i] = (char)((i % 95) + 32);
        ascii_charset_u16[i] = (char16_t)((i % 95) + 32);
        chinese_charset_u16[i] = (char16_t)(0x4E00 + (i % 20902));
        if ((i % 200) < 100) {
            mixed_charset_u16[i] = (char16_t)((i % 95) + 32);
        } else {
            mixed_charset_u16[i] = (char16_t)(0x4E00 + (i % 20902));
        }
    }
    for (size_t i = 0; i < logs_count; ++i) {
        size_t start = (i * 9973) % (character_pool_size - 1);
        size_t max_size = (character_pool_size - 1 - start);
        size_t data_size = i % 1024;
        data_size = bq::min_value(data_size, max_size);
        positions.push_back(bq::make_tuple(start, data_size));
    }
}

void test_compress_ascii_utf8(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Test Compressed File Log ASCII UTF8=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("test_ascii_u8");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (size_t i = 0; i < logs_count; ++i) {
                log_obj.info(benchmark_string_view<char>(ascii_charset.begin() + bq::get<0>(positions[i]), bq::get<1>(positions[i])));
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
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
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (size_t i = 0; i < logs_count; ++i) {
                log_obj.info(benchmark_string_view<char16_t>(ascii_charset_u16.begin() + bq::get<0>(positions[i]), bq::get<1>(positions[i])));
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
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
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (size_t i = 0; i < logs_count; ++i) {
                log_obj.info(benchmark_string_view<char16_t>(chinese_charset_u16.begin() + bq::get<0>(positions[i]), bq::get<1>(positions[i])));
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
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
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (size_t i = 0; i < logs_count; ++i) {
                log_obj.info(benchmark_string_view<char16_t>(mixed_charset_u16.begin() + bq::get<0>(positions[i]), bq::get<1>(positions[i])));
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_multi_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Compressed File Log Test 1, 4 params=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i) {
                log_obj.info("idx:{}, num:{}, This test, {}, {}", idx, i, 2.4232f, true);
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_enc_multi_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Encrypted Compressed File Log Test 1, 4 params=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress_enc");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i) {
                log_obj.info("idx:{}, num:{}, This test, {}, {}", idx, i, 2.4232f, true);
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_text_multi_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "============Begin Text File Log Test 2, 4 params============" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("text");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i) {
                log_obj.info("idx:{}, num:{}, This test, {}, {}", idx, i, 2.4232f, true);
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_no_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Compressed File Log Test 3, no param=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i) {
                log_obj.info("Empty Log, No Param");
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_compress_enc_no_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Encrypted Compressed File Log Test 3, no param=========" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("compress_enc");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i) {
                log_obj.info("Empty Log, No Param");
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
    }
    bq::log::force_flush_all_logs();
    uint64_t flush_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Time Cost:" << (uint64_t)(flush_time - start_time) << std::endl;
    std::cout << "============================================================" << std::endl
              << std::endl;
}

void test_text_no_param(int32_t thread_count)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "============Begin Text File Log Test 4, no param============" << std::endl;
    bq::log log_obj = bq::log::get_log_by_name("text");
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i) {
                log_obj.info("Empty Log, No Param");
            }
        });
        threads[idx] = st;
    }
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        threads[idx]->join();
        delete threads[idx];
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

    test_compress_multi_param(thread_count);
    test_compress_enc_multi_param(thread_count);
    test_text_multi_param(thread_count);
    test_compress_no_param(thread_count);
    test_compress_enc_no_param(thread_count);
    test_text_no_param(thread_count);
    return 0;
}
