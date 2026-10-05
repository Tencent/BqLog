// glog benchmark. NOTE: glog is synchronous by design -- the standard library
// has no async mode. LOG(INFO) formats and writes under a mutex in the caller
// thread. Also glog has no {fmt}-style formatting; it uses stream operator<<.
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include "bench_sys.h"

static const int ITERATIONS = 2000000;

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);

    google::InitGoogleLogging("benchmark");
    FLAGS_log_dir = "output/";
    FLAGS_logtostderr = false;
    FLAGS_alsologtostderr = false;

    // multi_param
    {
        const double cpu_start = bench_process_cpu_ms();
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([t]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    LOG(INFO) << "idx:" << t << ", num:" << i << ", This test, " << 2.4232f << ", " << true;
                }
            });
        }
        for (auto& th : threads) th.join();
        google::FlushLogFiles(google::GLOG_INFO);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|glog|multi_param|" << thread_count << "|" << ms << "|" << static_cast<long long>(bench_process_cpu_ms() - cpu_start) << "|" << bench_peak_mb() << std::endl;
    }

    // no_param
    {
        const double cpu_start = bench_process_cpu_ms();
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    LOG(INFO) << "Empty Log, No Param";
                }
            });
        }
        for (auto& th : threads) th.join();
        google::FlushLogFiles(google::GLOG_INFO);
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|glog|no_param|" << thread_count << "|" << ms << "|" << static_cast<long long>(bench_process_cpu_ms() - cpu_start) << "|" << bench_peak_mb() << std::endl;
    }

    google::ShutdownGoogleLogging();
    return 0;
}
