/* Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0. */
// Multi-instance regression worker. One process owns one log and writes
// normal / fast / oversize records in a deterministic order; run.py drives
// several workers and asserts isolation, recovery and instance assignment.
#include "bq_log/bq_log.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

static void write_text_file(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    file << content;
}

static void self_kill()
{
#if defined(_WIN32)
    TerminateProcess(GetCurrentProcess(), 9);
#else
    kill(getpid(), SIGKILL);
#endif
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: worker <worker-id> <log-name> <records> [options]\n");
        return 2;
    }
    const int worker_id = std::atoi(argv[1]);
    const std::string log_name = argv[2];
    const int records = std::atoi(argv[3]);
    if (worker_id < 0 || records <= 0) {
        return 2;
    }
    bool recovery = true;
    bool two_logs = false;
    bool report = false;
    bool child_lock_test = false;
    long long die_after = -1;
    long long sleep_seconds = -1;
    const char* child_lock_path = nullptr;
    const char* hold_until = nullptr;
    for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value_of = [&](const char* prefix) -> const char* {
            if (arg.compare(0, strlen(prefix), prefix) != 0) {
                return nullptr;
            }
            if (arg.size() == strlen(prefix) && i + 1 < argc) {
                return argv[++i];
            }
            return arg.c_str() + strlen(prefix);
        };
        if (arg == "--recovery" && i + 1 < argc) {
            recovery = std::string(argv[++i]) == "on";
        } else if (arg == "--die-after") {
            die_after = std::atoll(argv[++i]);
        } else if (arg == "--two-logs") {
            two_logs = true;
        } else if (arg == "--report") {
            report = true;
        } else if (arg == "--child-lock-test") {
            child_lock_test = true;
            if (i + 1 < argc) {
                child_lock_path = argv[++i];
            }
        } else if (arg == "--sleep") {
            sleep_seconds = std::atoll(argv[++i]);
        } else if (arg == "--hold-until") {
            hold_until = argv[++i];
        } else {
            std::fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 2;
        }
    }

    if (sleep_seconds >= 0) {
        std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
        return 0;
    }

    const std::string id = std::to_string(worker_id);
    std::string config = "log.thread_mode=async\n";
    config += recovery ? "log.recovery=true\n" : "log.recovery=false\n";
    config += "log.buffer_size=4096\n"
              "log.high_perform_mode_freq_threshold_per_second=1\n"
              "appenders_config.out.type=text_file\n"
              "appenders_config.out.levels=[all]\n"
              "appenders_config.out.file_name=output/worker_"
        + id + "\n"
               "appenders_config.out.base_dir_type=0\n"
               "appenders_config.out.enable_rolling_log_file=false\n";
    bq::log log = bq::log::create_log(log_name.c_str(), config.c_str());
    bq::log log_b = log;
    if (two_logs) {
        // the second log must not share the appender output file
        std::string config_b = config;
        config_b.replace(config_b.find("output/worker_"), strlen("output/worker_"),
            "output/workerb_");
        log_b = bq::log::create_log((log_name + "_b").c_str(), config_b.c_str());
    }

    // child-lock-test: hold the instance (log created above), spawn a sleeping
    // child, then exit without ever flushing. The child must not inherit the
    // lock handle: the next worker has to claim instance 0 again.
    if (child_lock_test) {
        if (!log.is_valid() || !child_lock_path) {
            return 4;
        }
        log.info("lock holder alive");
        log.force_flush();
        std::string self_path;
#if defined(_WIN32)
        char module_path[MAX_PATH] = { 0 };
        GetModuleFileNameA(nullptr, module_path, MAX_PATH);
        self_path = module_path;
#else
        char module_path[4096];
        ssize_t len = readlink("/proc/self/exe", module_path, sizeof(module_path) - 1);
        if (len <= 0) {
            return 4;
        }
        module_path[len] = '\0';
        self_path = module_path;
#endif
        std::string cmd = "\"" + self_path + "\" 0 " + log_name + " 1 --sleep 10";
#if defined(_WIN32)
        STARTUPINFOA startup_info = { };
        startup_info.cb = sizeof(startup_info);
        PROCESS_INFORMATION process_info = { };
        if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW, nullptr, nullptr, &startup_info, &process_info)) {
            return 4;
        }
        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
#else
        int rc = std::system(cmd.c_str());
        (void)rc;
#endif
        return 0; // normal exit, child still alive holding nothing
    }

    if (!log.is_valid()) {
        write_text_file("ready_" + id + ".json", "{\"log_valid\": false}");
        return 4;
    }

    uint32_t instance = UINT32_MAX;
    bool lp_mapped = false;
    bool hp_mapped = false;
    if (report) {
        bq::api::__api_test_get_storage_report(log.get_id(), &instance, &lp_mapped, &hp_mapped);
    }

    // warmup: 2000 records push this thread into the hp buffer, so hp_mapped
    // is meaningful in the ready report
    for (int i = 0; i < 2000; ++i) {
        log.info("warmup worker={} seq={}", worker_id, i);
    }
    log.force_flush();

    if (report) {
        bq::api::__api_test_get_storage_report(log.get_id(), &instance, &lp_mapped, &hp_mapped);
        write_text_file("ready_" + id + ".json",
            "{\"log_valid\": true, \"instance\": " + std::to_string(instance)
                + ", \"lp_mapped\": " + (lp_mapped ? "true" : "false")
                + ", \"hp_mapped\": " + (hp_mapped ? "true" : "false") + "}");
    } else {
        write_text_file("ready_" + id + ".json", "{\"log_valid\": true}");
    }

    if (hold_until) {
        // M1/M5 barrier: run.py creates the sentinel before spawning everyone
        // and deletes it once all workers are ready; instances stay claimed
        // while the file exists
        for (int i = 0; i < 600; ++i) {
            std::FILE* f = std::fopen(hold_until, "rb");
            if (!f) {
                break;
            }
            std::fclose(f);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    const std::string large(128 * 1024, 'x');
    const int committed = die_after >= 0 ? static_cast<int>(die_after < records ? die_after : records) : records;
    for (int i = 0; i < committed; ++i) {
        if (two_logs) {
            // alternate between the two logs, same format strings
            if (i % 2 == 0) {
                log.info("normal worker={} seq={}", worker_id, i);
                BQ_LOG_FAST_INFO(log, "fast worker={} seq={}", worker_id, i);
            } else {
                log_b.info("normal_b worker={} seq={}", worker_id, i);
                BQ_LOG_FAST_INFO(log_b, "fast_b worker={} seq={}", worker_id, i);
            }
        } else {
            log.info("normal worker={} seq={}", worker_id, i);
            BQ_LOG_FAST_INFO(log, "fast worker={} seq={}", worker_id, i);
            if (i % 32 == 0) {
                log.info("oversize worker={} seq={} {}", worker_id, i, large.c_str());
            }
        }
        if (die_after >= 0 && i + 1 == committed) {
            // records up to committed_seq stay in the ring buffer / appender
            // cache: the successor must recover them, so no force_flush here
            write_text_file("committed_" + id + ".json",
                "{\"committed_seq\": " + std::to_string(committed) + "}");
            self_kill();
        }
    }
    log.force_flush();
    if (two_logs) {
        log_b.force_flush();
    }
    write_text_file("done_" + id, "done\n");
    return 0;
}
