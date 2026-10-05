#include "bq_log/bq_log.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include "bench_sys.h"

static const int ITERATIONS = 2000000;
static bool g_expand = false; // log.buffer_policy_when_full=expand, compared with quill's default unbounded queue

static bq::log create(const char* name, const char* type, const char* file, bool enc)
{
    std::string cfg = "log.high_perform_mode_freq_threshold_per_second=1\n";
    if (g_expand) {
        cfg += "log.buffer_policy_when_full=expand\n";
    }
    cfg += "appenders_config.appender_0.type=";
    cfg += type;
    cfg += "\nappenders_config.appender_0.levels=[all]\n";
    cfg += "appenders_config.appender_0.file_name=";
    cfg += file;
    cfg += "\nappenders_config.appender_0.always_create_new_file=true\n";
    if (enc) {
        cfg += "appenders_config.appender_0.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+FonyOHuS2uC6IZc16bfd6qQk4ykBOt3nTfBFcNr8ZWvvcf4H0hFkrpMtQ0AJO057GhVTQCCfnvfStSq2Yra+O5VGpI5Q6NLrUuVERimjNgwtxbXt3P8Nw87jEIJiY/8m2FUXhZEPwoA7t+2/953cNE1itJskJtojwaUlMN0dXBJxs4NP8MfBPPZQ5vNV8xgEf1SCQzQBAJsofy1kPHHqJNBXUBsNA44SP5H95JOz+r0oaNkYxT88Zk4tbk5N3hk5aXyZVp49OqhrXCPf5owDa4Lqk4UzVTk9EimxvtSuiUTzr7IJhHYy7jsGnSgq6dH0xlUfxKeX pippocao@PIPPOCAO-PC6\n";
    }
    return bq::log::create_log(name, cfg.c_str());
}

// wall time from the first call until everything is on disk, and the CPU time all threads spent on it
static void report(const char* lib_name, int thread_count, bool multi_param, std::chrono::steady_clock::time_point start, double cpu_start)
{
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "RESULT|" << lib_name << (g_expand ? "_expand" : "") << "|" << (multi_param ? "multi_param" : "no_param")
              << "|" << thread_count << "|" << ms << "|" << static_cast<long long>(bench_process_cpu_ms() - cpu_start) << "|" << bench_peak_mb() << std::endl;
}

// normal mode: log.info
static void run_test(bq::log& log_obj, const char* lib_name, int thread_count, bool multi_param)
{
    const double cpu_start = bench_process_cpu_ms();
    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([t, &log_obj, multi_param]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                if (multi_param) {
                    log_obj.info("idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                } else {
                    log_obj.info("Empty Log, No Param");
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    bq::log::force_flush_all_logs();
    report(lib_name, thread_count, multi_param, start, cpu_start);
}

// C++ only fast mode: BQ_LOG_FAST_* macros. A call site binds the log object of its first call,
// so every log gets its own instantiation (and with it its own call sites).
template <int LOG_TAG>
static void run_fast_test(bq::log& log_obj, const char* lib_name, int thread_count, bool multi_param)
{
    const double cpu_start = bench_process_cpu_ms();
    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([t, &log_obj, multi_param]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                if (multi_param) {
                    BQ_LOG_FAST_INFO(log_obj, "idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                } else {
                    BQ_LOG_FAST_INFO(log_obj, "Empty Log, No Param");
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    bq::log::force_flush_all_logs();
    report(lib_name, thread_count, multi_param, start, cpu_start);
}

// usage: bench_bqlog <threads> [block|expand] [only this lib, e.g. bqlog_fast_text]
int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);
    g_expand = argc > 2 && strcmp(argv[2], "expand") == 0;
    const char* only = argc > 3 ? argv[3] : nullptr;
    auto want = [only](const char* lib_name) { return !only || strcmp(only, lib_name) == 0; };

    bq::log log_text = create("bench_text", "text_file", "output/bqlog_text", false);
    bq::log log_compress = create("bench_compress", "compressed_file", "output/bqlog_compress", false);
    bq::log log_compress_enc = create("bench_compress_enc", "compressed_file", "output/bqlog_compress_enc", true);

    bq::log log_fast_text = create("bench_fast_text", "text_file", "output/bqlog_fast_text", false);
    bq::log log_fast_compress = create("bench_fast_compress", "compressed_file", "output/bqlog_fast_compress", false);
    bq::log log_fast_compress_enc = create("bench_fast_compress_enc", "compressed_file", "output/bqlog_fast_compress_enc", true);
    bq::log::force_flush_all_logs();

    for (int multi = 1; multi >= 0; --multi) {
        if (want("bqlog_compress")) run_test(log_compress, "bqlog_compress", thread_count, multi != 0);
        if (want("bqlog_compress_enc")) run_test(log_compress_enc, "bqlog_compress_enc", thread_count, multi != 0);
        if (want("bqlog_text")) run_test(log_text, "bqlog_text", thread_count, multi != 0);
        if (want("bqlog_fast_compress")) run_fast_test<0>(log_fast_compress, "bqlog_fast_compress", thread_count, multi != 0);
        if (want("bqlog_fast_compress_enc")) run_fast_test<1>(log_fast_compress_enc, "bqlog_fast_compress_enc", thread_count, multi != 0);
        if (want("bqlog_fast_text")) run_fast_test<2>(log_fast_text, "bqlog_fast_text", thread_count, multi != 0);
    }
    return 0;
}
