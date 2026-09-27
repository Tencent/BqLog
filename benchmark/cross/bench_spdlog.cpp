// spdlog ASYNC benchmark (replaces the previous synchronous basic_logger_mt version).
// async_logger with a dedicated thread-pool worker; overflow policy defaults to
// "block" so no log entries are dropped (comparable to fmtlog FMTLOG_BLOCK=1).
// Timing ends after spdlog::shutdown(), which drains the queue and flushes the
// sink to disk -- the same "written to disk" semantics as the other benchmarks.
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>

static const int ITERATIONS = 2000000;
static const size_t QUEUE_SIZE = 8192; // spdlog default async queue slots
static const size_t BACKEND_THREADS = 1;

static void run_test(const char* name, const char* file, int thread_count, bool multi_param)
{
    spdlog::init_thread_pool(QUEUE_SIZE, BACKEND_THREADS);
    auto logger = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>(name, file, true);
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%f [%t] [%l] %v");

    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&logger, t, multi_param]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                if (multi_param) {
                    logger->info("idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                } else {
                    logger->info("Empty Log, No Param");
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    spdlog::shutdown(); // drains queue, flushes sink to disk
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "RESULT|spdlog_async|" << (multi_param ? "multi_param" : "no_param")
              << "|" << thread_count << "|" << ms << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    run_test("bench_mp", "output/spdlog_mp.log", thread_count, true);
    run_test("bench_np", "output/spdlog_np.log", thread_count, false);
    return 0;
}
