// fmtlog benchmark. FMTLOG_BLOCK=1 prevents silent log dropping (fmtlog's
// default behavior drops entries when the queue is full).
#define FMTLOG_BLOCK 1
#include "fmtlog.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

static const int ITERATIONS = 2000000;

int main(int argc, char* argv[])
{
    if (argc < 2) return 1;
    int thread_count = std::atoi(argv[1]);
    const char* which = (argc >= 3) ? argv[2] : "both";

    // fmtlog's setLogFile races with its polling thread; run each test in a
    // separate process invocation (argv[2] = mp | np) to keep results reliable.

    if (strcmp(which, "np") != 0) {
        fmtlog::setLogFile("output/fmtlog_mp.log", false);
        fmtlog::setHeaderPattern("{YmdHMSf} {l}[{t}] ");
        fmtlog::startPollingThread(1);
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([t]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    FMTLOG(fmtlog::INF, "idx:{}, num:{}, This test, {}, {}", t, i, 2.4232f, true);
                }
            });
        }
        for (auto& th : threads) th.join();
        // fmtlog::poll is NOT thread-safe with the polling thread: stop it first,
        // then drain synchronously. A single poll can leave the tail unwritten
        // (it only processes entries older than the rdtsc taken at poll entry).
        fmtlog::stopPollingThread();
        for (int drain = 0; drain < 10; ++drain) {
            fmtlog::poll(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|fmtlog|multi_param|" << thread_count << "|" << ms << std::endl;
    }

    if (strcmp(which, "mp") != 0) {
        fmtlog::setLogFile("output/fmtlog_np.log", false);
        fmtlog::setHeaderPattern("{YmdHMSf} {l}[{t}] ");
        fmtlog::startPollingThread(1);
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    FMTLOG(fmtlog::INF, "Empty Log, No Param");
                }
            });
        }
        for (auto& th : threads) th.join();
        fmtlog::stopPollingThread();
        for (int drain = 0; drain < 10; ++drain) {
            fmtlog::poll(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "RESULT|fmtlog|no_param|" << thread_count << "|" << ms << std::endl;
    }

    _exit(0); // fmtlog has cleanup issues, use _exit
}
