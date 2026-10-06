// quill benchmark, configured per quill's official benchmark with busy-spin
// backend (sleep_duration = 0ns) for maximum performance.
// block:  a fixed 64 KiB queue per thread that blocks the logging thread when full, the same as BqLog's default
// expand: quill's default queue, which grows when full (compared with BqLog's log.buffer_policy_when_full=expand)
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
#include <cstring>
#include "bench_sys.h"

static const int ITERATIONS = 2000000;

struct fixed_queue_options : quill::FrontendOptions {
    static constexpr quill::QueueType queue_type = quill::QueueType::BoundedBlocking;
    static constexpr size_t initial_queue_capacity = 64 * 1024;
    static constexpr size_t unbounded_queue_max_capacity = 64 * 1024;
#if defined(_WIN32)
    // quill retries a full queue after sleep_for_ns(800), which is ::Sleep(1 ms) on Windows: every blocked call
    // would sleep a whole millisecond. Retry without sleeping, as the 800 ns sleep effectively is elsewhere.
    static constexpr uint32_t blocking_queue_retry_interval_ns = 0;
#endif
};
using fixed_frontend = quill::FrontendImpl<fixed_queue_options>;

template <typename FRONTEND>
static void run_test(const char* lib_name, const char* logger_name, const char* file, int thread_count, bool multi_param)
{
    auto file_sink = FRONTEND::template create_or_get_sink<quill::FileSink>(file);
    auto* logger = FRONTEND::create_or_get_logger(
        logger_name, std::move(file_sink),
        quill::PatternFormatterOptions{
            "%(time) [%(thread_id)] %(log_level) %(message)",
            "%H:%M:%S.%Qns"});

    const double cpu_start = bench_process_cpu_ms();
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
    FRONTEND::remove_logger(logger);
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "RESULT|" << lib_name << "|" << (multi_param ? "multi_param" : "no_param")
              << "|" << thread_count << "|" << ms << "|" << static_cast<long long>(bench_process_cpu_ms() - cpu_start) << "|" << bench_peak_mb() << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    quill::BackendOptions backend_options;
    backend_options.sleep_duration = std::chrono::nanoseconds{0}; // busy spin
    quill::Backend::start(backend_options);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // let backend init

    if (argc > 2 && strcmp(argv[2], "expand") == 0) {
        run_test<quill::Frontend>("quill_expand", "bench_mp", "output/quill_mp.log", thread_count, true);
        run_test<quill::Frontend>("quill_expand", "bench_np", "output/quill_np.log", thread_count, false);
    } else {
        run_test<fixed_frontend>("quill", "bench_mp", "output/quill_mp.log", thread_count, true);
        run_test<fixed_frontend>("quill", "bench_np", "output/quill_np.log", thread_count, false);
    }
    return 0;
}
