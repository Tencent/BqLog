/* Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0. */
// M6 helper: the T04 worker minus the test-only storage report API, so it
// builds against a 2.6.0 checkout (see BQ_LEGACY_SRC in CMakeLists.txt).
#include "bq_log/bq_log.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

static void write_text_file(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    file << content;
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: worker <worker-id> <log-name> <records>\n");
        return 2;
    }
    const int worker_id = std::atoi(argv[1]);
    const int records = std::atoi(argv[3]);
    if (worker_id < 0 || records <= 0) {
        return 2;
    }
    const std::string id = std::to_string(worker_id);
    const std::string config = "log.thread_mode=async\n"
                               "log.recovery=true\n"
                               "log.buffer_size=4096\n"
                               "log.high_perform_mode_freq_threshold_per_second=1\n"
                               "appenders_config.out.type=text_file\n"
                               "appenders_config.out.levels=[all]\n"
                               "appenders_config.out.file_name=output/worker_"
        + id + "\n"
               "appenders_config.out.base_dir_type=0\n"
               "appenders_config.out.enable_rolling_log_file=false\n";
    bq::log log = bq::log::create_log(argv[2], config.c_str());
    if (!log.is_valid()) {
        write_text_file("ready_" + id + ".json", "{\"log_valid\": false}");
        return 4;
    }
    for (int i = 0; i < 2000; ++i) {
        log.info("warmup worker={} seq={}", worker_id, i);
    }
    log.force_flush();
    write_text_file("ready_" + id + ".json", "{\"log_valid\": true}");
    const std::string large(128 * 1024, 'x');
    for (int i = 0; i < records; ++i) {
        log.info("normal worker={} seq={}", worker_id, i);
        BQ_LOG_FAST_INFO(log, "fast worker={} seq={}", worker_id, i);
        if (i % 32 == 0) {
            log.info("oversize worker={} seq={} {}", worker_id, i, large.c_str());
        }
    }
    log.force_flush();
    write_text_file("done_" + id, "done\n");
    return 0;
}
