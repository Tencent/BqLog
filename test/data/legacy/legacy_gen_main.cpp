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

// Writes one log file with the runtime of a specific historical tag, so that the current
// standalone decoder has something real to be held against. It only uses the public API that
// every tag in scope shares: __api_create_log for the category list, do_log for per-category
// records, and either a clean shutdown or a SIGKILL to leave pending state behind.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <chrono>
#include <thread>

#include "bq_log/bq_log.h"

namespace {
    // do_log and get_log_by_id are protected: they are what the generated category classes call,
    // not part of the flat API. Deriving is the documented way in, and it is all this generator
    // needs - no generated code, no codegen step.
    struct sample_log : public bq::log {
        sample_log() = default;
        explicit sample_log(const bq::log& rhs)
            : bq::log(rhs)
        {
        }
        using bq::log::do_log;
        using bq::log::get_log_by_id;
    };
    struct options {
        bq::string base_dir;
        bq::string phase;
        bq::string pub_key;
        bq::string thread_mode;
        int32_t entries = 0;
        bool crash = false;
        bool recovery = true;
        // Non-zero: log continuously and get killed after this many microseconds, which is what
        // leaves the appender's write cache dirty as opposed to the log buffer.
        int32_t crash_delay_us = 0;
        // Pauses between record sets in the continuous loop, so a crash sample holds hundreds of
        // records rather than megabytes of them.
        int32_t throttle_us = 0;
    };

    bq::string build_config(const options& opt)
    {
        bq::string conf;
        conf += "log.thread_mode=";
        conf += opt.thread_mode;
        conf += "\n";
        conf += "log.recovery=";
        conf += opt.recovery ? "true" : "false";
        conf += "\n";
        conf += "appenders_config.legacy.type=compressed_file\n";
        conf += "appenders_config.legacy.levels=[all]\n";
        conf += "appenders_config.legacy.file_name=";
        conf += opt.base_dir;
        conf += "/legacy_sample\n";
        conf += "appenders_config.legacy.base_dir_type=0\n";
        conf += "appenders_config.legacy.enable_rolling_log_file=false\n";
        conf += "appenders_config.legacy.max_file_size=0\n";
        if (!opt.pub_key.is_empty()) {
            conf += "appenders_config.legacy.pub_key=";
            conf += opt.pub_key;
            conf += "\n";
        }
        return conf;
    }

    // Two templates (UTF-8 and UTF-16), four categories with a Chinese one, and records whose
    // text carries the index so a decoded file can be diffed against a golden output.
    void write_records(const sample_log& log_obj, int32_t from, int32_t to, int32_t throttle_us = 0)
    {
        for (int32_t i = from; i < to; ++i) {
            if (throttle_us > 0) {
                usleep(static_cast<useconds_t>(throttle_us));
            }
            log_obj.do_log(0, bq::log_level::info, "plain record {} value {}", i, static_cast<int32_t>(i * 17));
            log_obj.do_log(1, bq::log_level::warning, u"中文模板 第{}条 \u8bb0\u5f55", i);
            log_obj.do_log(2, bq::log_level::error, "类别记录 {} ratio {}", i, static_cast<double>(i) + 0.5);
            log_obj.do_log(3, bq::log_level::debug, "cat B {} ptr {}", i, static_cast<const void*>(&log_obj));
        }
    }
}

int main(int argc, char** argv)
{
    options opt;
    if (argc < 8) {
        printf("usage: %s <base_dir> <phase> <pub_key|-> <thread_mode> <entries> <crash 0|1> <recovery 0|1> [crash_delay_us]\n", argv[0]);
        return 1;
    }
    opt.base_dir = argv[1];
    opt.phase = argv[2];
    opt.pub_key = (strcmp(argv[3], "-") == 0) ? bq::string() : bq::string(argv[3]);
    opt.thread_mode = argv[4];
    opt.entries = atoi(argv[5]);
    opt.crash = atoi(argv[6]) != 0;
    opt.recovery = atoi(argv[7]) != 0;
    if (argc > 8) {
        opt.crash_delay_us = atoi(argv[8]);
    }
    if (argc > 9) {
        opt.throttle_us = atoi(argv[9]);
    }

    const char* category_names[] = { "Default", "CategoryA", "中文类别", "CategoryB" };
    const bq::string conf = build_config(opt);
    const uint64_t log_id = bq::api::__api_create_log("legacy_sample", conf.c_str(), 4, category_names);
    const sample_log log_obj = sample_log(sample_log::get_log_by_id(log_id));
    if (!log_obj.is_valid()) {
        printf("create_log failed\n");
        return 2;
    }

    // The resume phase replays whatever the killed process left behind, then continues the same
    // index sequence so the decoded output of both runs is one continuous record list.
    const int32_t start = (opt.phase == "resume") ? opt.entries : 0;
    const int32_t end = start + opt.entries;

    if (opt.crash && opt.crash_delay_us > 0) {
        // Killed mid-stream rather than after the last record, so whatever the appender had not
        // flushed yet is still in its cache when the process dies.
        std::thread([](int32_t delay_us) {
            std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
            raise(SIGKILL);
        }, opt.crash_delay_us)
            .detach();
        write_records(log_obj, start, 0x7FFFFFFF, opt.throttle_us);
        return 3;
    }

    write_records(log_obj, start, end);

    if (opt.crash) {
        // Same effect as kill -9 from outside: no flush, no destructor, nothing ordered.
        fflush(stdout);
        raise(SIGKILL);
        return 3;
    }

    bq::log::force_flush_all_logs();
    return 0;
}
