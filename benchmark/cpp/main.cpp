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
#include <algorithm>
#include <random>

const bq::string u8_characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const bq::u16string u16_characters = u"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
bq::array<bq::string> u8_multi_characters = { "a", "b", "c", "d" };
bq::array<bq::u16string> u16_multi_characters = { u"e", u"f", u"g", u"h" };

bq::array<bq::string> pre_generated_u8_strings;
bq::array<bq::u16string> pre_generated_u16_strings;

static size_t random_value = 0;

static bq::string generate_utf8_string() {
    std::random_device rd;  
    std::mt19937 generator(rd());  
    //std::uniform_int_distribution<size_t> distribution_char(0, u8_characters.size() - 1);
    //std::uniform_int_distribution<size_t> distribution_word(0, u8_multi_characters.size() - 1);

    bq::string random_string;
    size_t length = (++random_value) % 256; //distribution_char(generator) % 256;
    random_string.set_capacity(length * 3);

    for (size_t i = 0; i < length; ++i) {
        random_string.push_back(u8_characters[(++random_value) % u8_characters.size()]);
        random_string += u8_multi_characters[(++random_value) % u8_multi_characters.size()];
    }
    random_string = bq::string("{}") + random_string + "{}{}{}";
    return random_string;
}

static bq::u16string generate_utf16_string() {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> distribution_char(0, u8_characters.size() - 1);
    std::uniform_int_distribution<size_t> distribution_word(0, u16_multi_characters.size() - 1);

    bq::u16string random_string;
    size_t length = distribution_char(generator) % 256;
    random_string.set_capacity(length * 3);

    for (size_t i = 0; i < length; ++i) {
        random_string.push_back(u8_characters[distribution_char(generator)]);
        random_string += u16_multi_characters[distribution_word(generator)];
    }
    random_string += u"{}{}{}{}";
    return random_string;
}

template<typename STR_TYPE>
void test_multi_param(int32_t thread_count, bq::log& log_obj, const bq::array<STR_TYPE>& format_templates)
{
    std::cout << "============================================================" << std::endl;
    std::cout << "=========Begin Log Test 4 params=========, log_name:" << log_obj.get_name().c_str() << std::endl;
    std::vector<std::thread*> threads;
    threads.resize(thread_count);
    uint64_t start_time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    std::cout << "Now Begin, each thread will write 2000000 log entries, please wait the result..." << std::endl;
    for (int32_t idx = 0; idx < thread_count; ++idx) {
        std::thread* st = new std::thread([&format_templates, idx, &log_obj]() {
            for (int i = 0; i < 2000000; ++i) {
                log_obj.info(format_templates[i], i, idx, 2.4232f, true);
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
    bq::log compressed_log_u8 = bq::log::create_log("compress_u8", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/compress_u8
		appenders_config.appender_3.always_create_new_file=true
		appenders_config.appender_3.enable_rolling_log_file=false
		appenders_config.appender_4.type=text_file
		appenders_config.appender_4.levels=[all]
		appenders_config.appender_4.file_name= benchmark_output/text_u8
		appenders_config.appender_4.always_create_new_file=true
		appenders_config.appender_4.enable_rolling_log_file=false
    )");
    bq::log compressed_log_u16 = bq::log::create_log("compress_u16", R"(
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/compress_u16
		appenders_config.appender_3.always_create_new_file=true
		appenders_config.appender_3.enable_rolling_log_file=false
    )");
    bq::log text_log_u8 = bq::log::create_log("text_u8", R"(
		appenders_config.appender_3.type=text_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/text_u8
		appenders_config.appender_3.always_create_new_file=true
		appenders_config.appender_3.enable_rolling_log_file=false
    )");
    bq::log text_log_u16 = bq::log::create_log("text_u16", R"(
		appenders_config.appender_3.type=text_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/text_u16
		appenders_config.appender_3.always_create_new_file=true
		appenders_config.appender_3.enable_rolling_log_file=false
    )");
    std::cout << "Generating u8 string..." << std::endl;
    for (int32_t i = 0; i < 2000000; ++i) {
        pre_generated_u8_strings.push_back(generate_utf8_string());
    }    
    std::cout << "Generating u16 string..." << std::endl;
    for (int32_t i = 0; i < 2000000; ++i) {
        pre_generated_u16_strings.push_back(generate_utf16_string());
    }
    int32_t thread_count = 1;
    test_multi_param(thread_count, compressed_log_u8, pre_generated_u8_strings);
    //test_multi_param(thread_count, compressed_log_u16, pre_generated_u16_strings);
    //test_multi_param(thread_count, text_log_u8, pre_generated_u8_strings);
    //test_multi_param(thread_count, text_log_u16, pre_generated_u16_strings);

    return 0;
}
