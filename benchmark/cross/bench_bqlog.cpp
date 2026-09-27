#include "bq_log/bq_log.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <string>
#include <cstdlib>

static const int ITERATIONS = 2000000;

static bq::log create(const char* name, const char* type, const char* file, bool enc)
{
    std::string cfg = "log.high_perform_mode_freq_threshold_per_second=1\n";
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

static void run_test(bq::log& log_obj, const char* lib_name, int thread_count, bool multi_param)
{
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
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "RESULT|" << lib_name << "|" << (multi_param ? "multi_param" : "no_param")
              << "|" << thread_count << "|" << ms << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    bq::log log_text = create("bench_text", "text_file", "output/bqlog_text", false);
    bq::log log_compress = create("bench_compress", "compressed_file", "output/bqlog_compress", false);
    bq::log log_compress_enc = create("bench_compress_enc", "compressed_file", "output/bqlog_compress_enc", true);
    bq::log::force_flush_all_logs();

    run_test(log_compress, "bqlog_compress", thread_count, true);
    run_test(log_compress_enc, "bqlog_compress_enc", thread_count, true);
    run_test(log_text, "bqlog_text", thread_count, true);
    run_test(log_compress, "bqlog_compress", thread_count, false);
    run_test(log_compress_enc, "bqlog_compress_enc", thread_count, false);
    run_test(log_text, "bqlog_text", thread_count, false);
    return 0;
}
