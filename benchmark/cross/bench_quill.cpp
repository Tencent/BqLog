// quill benchmark, configured per quill's official benchmark with busy-spin
// backend (sleep_duration = 0ns) for maximum performance.
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/FileSink.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>

static const int ITERATIONS = 2000000;

static void run_test(const char* logger_name, const char* file, int thread_count, bool multi_param)
{
    auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(file);
    quill::Logger* logger = quill::Frontend::create_or_get_logger(
        logger_name, std::move(file_sink),
        quill::PatternFormatterOptions{
            "%(time) [%(thread_id)] %(log_level) %(message)",
            "%H:%M:%S.%Qns"});

    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([logger, t, multi_param]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                if (multi_param) {
                    LOG_INFO(logger, "idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                } else {
                    LOG_INFO(logger, "Empty Log, No Param");
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    logger->flush_log();
    quill::Frontend::remove_logger(logger);
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "RESULT|quill|" << (multi_param ? "multi_param" : "no_param")
              << "|" << thread_count << "|" << ms << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    quill::BackendOptions backend_options;
    backend_options.sleep_duration = std::chrono::nanoseconds{0}; // busy spin
    quill::Backend::start(backend_options);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // let backend init

    run_test("bench_mp", "output/quill_mp.log", thread_count, true);
    run_test("bench_np", "output/quill_np.log", thread_count, false);
    return 0;
}
