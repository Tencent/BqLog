/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdio.h>
#include <iostream>
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_file_compressed.h"
#include "bq_log/log/appender/appender_file_raw.h"
#include "common_header.h"

static void output_usage()
{
    std::cout << "decoder format version:" << std::endl;
    std::cout << "raw file format version:" << bq::appender_file_compressed::format_version << std::endl;
    std::cout << "compressed file format version:" << bq::appender_file_raw::format_version << std::endl;
    std::cout << std::endl;
    std::cout << "usage : log_decoder input_log_file_path [output_log_file_path]" << std::endl;
    std::cout << "and the decoded log text will output to standard output if [output_log_file_path] is not set or is not valid" << std::endl;
}

static bq::file_manager& file_manager_ref = bq::file_manager::instance();
static bq::array<char> cache;
static constexpr size_t cache_size = 64 * 1024;

static void flush_cache(bq::file_handle& output_handle)
{
    if (output_handle) {
        file_manager_ref.write_file(output_handle, cache.begin(), cache.size());
    } else {
        cache.push_back('\0');
        fputs(cache.begin(), stdout);
        fflush(stdout);
    }
    cache.clear();
}

int32_t main(int32_t argc, char* argv[])
{
    if (argc != 2 && argc != 3) {
        output_usage();
        return -1;
    }
    bq::file_handle output_handle;
    uint32_t handle = 0;
    auto result = bq::api::__api_log_decoder_create(argv[1], &handle);
    if (argc == 3) {
        output_handle = bq::file_manager::instance().open_file(argv[2], bq::file_open_mode_enum::read_write | bq::file_open_mode_enum::auto_create);
        if (output_handle) {
            bq::file_manager::instance().seek(output_handle, bq::file_manager::seek_option::end, 0);
        }
    }
    if (result != bq::appender_decode_result::success) {
        std::cerr << "create decoder failed, result : " << result << std::endl;
        return -1 * (int32_t)result;
    }

    cache.set_capacity(cache_size);
    while (true) {
        bq::_api_string_def log_text;
        result = bq::api::__api_log_decoder_decode(handle, &log_text);
        if (result == bq::appender_decode_result::eof) {
            break;
        } else if (result != bq::appender_decode_result::success) {
            std::cerr << "decode failed, result : " << result << std::endl;
            break;
        } else {
            if (cache.size() + log_text.len + 2 + 1 >= cache_size) {
                flush_cache(output_handle);
            }
            cache.insert_batch(cache.end(), log_text.str, log_text.len);
            cache.push_back('\n');
        }
    }
    flush_cache(output_handle);
    if (output_handle) {
        bq::file_manager::instance().flush_file(output_handle);
    }
    return (int32_t)result;
}
