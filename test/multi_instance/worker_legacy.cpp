/* Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0. */
// legacy_coexistence helper: the multi-instance worker minus the test-only storage report API, so
// it builds against a 2.6.0 checkout (see BQ_LEGACY_SRC in CMakeLists.txt).
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
        std::fprintf(stderr, "usage: worker <worker-id> <log-name> <records> [--pace-ms N]\n");
        return 2;
    }
    const int worker_id = std::atoi(argv[1]);
    const int records = std::atoi(argv[3]);
    if (worker_id < 0 || records <= 0) {
        return 2;
    }
    // legacy_coexistence needs this worker busy when the new runtime joins; a bigger count only costs file descriptors.
    int pace_ms = 0;
    if (argc >= 6 && std::strcmp(argv[4], "--pace-ms") == 0) {
        pace_ms = std::atoi(argv[5]);
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
        if (pace_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(pace_ms));
        }
    }
    log.force_flush();
    write_text_file("done_" + id, "done\n");
    return 0;
}
