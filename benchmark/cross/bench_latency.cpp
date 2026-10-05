// Logging thread cost, measured the same way for every library, with the timing of quill's own hot path benchmark
// (quill/benchmarks/hot_path_latency):
//   - every thread writes batches of MESSAGES_PER_BATCH calls with a 2000-2200 us busy wait between batches
//   - each batch is timed with a serialized hardware counter (isb/mrs cntvct on arm64, lfence/rdtsc on x86),
//     latency per call = batch time / batch size
//   - ITERATIONS batches per thread, percentiles over all batches of all threads
// Unlike quill's benchmark every thread writes a full batch, so the per call resolution and the share of the batch
// fixed cost are the same for any thread count.
// Every library gets a fixed size queue of the same capacity (64 KiB per thread) where it has one, and every consumer
// sleeps 1 ms when idle. Next to the latency: consumer CPU (CPU time of everything but the logging threads while they
// write, % of one core) and peak memory.
//
// usage: bench_latency_<lib> <threads>
// prints: RESULT_LAT|<lib>|<threads>|<mean>|<p50>|<p75>|<p90>|<p95>|<p99>|<p99.9>|<worst>|<consumer cpu %>|<peak MB>
//         (latencies in ns per call)
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

#include "bench_sys.h"

#if defined(__x86_64__) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

#if defined(BENCH_BQLOG_NORMAL) || defined(BENCH_BQLOG_FAST)
#include "bq_log/bq_log.h"
#include <string>
#elif defined(BENCH_QUILL)
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/FileSink.h"
#elif defined(BENCH_FMTLOG)
#define FMTLOG_BLOCK 1
#include "fmtlog.h"
#elif defined(BENCH_SPDLOG)
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#endif
#if defined(_WIN32)
#include <process.h>
#endif

static const size_t MESSAGES_PER_BATCH = 20;
static const size_t ITERATIONS = 5000;
static const int64_t MIN_WAIT_NS = 2000000;
static const int64_t MAX_WAIT_NS = 2200000;
static const size_t FIXED_QUEUE_BYTES = 64 * 1024;

static inline uint64_t serialized_clock()
{
#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
    const uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
#elif defined(__aarch64__)
    uint64_t t;
    __asm__ volatile("isb\n\tmrs %0, cntvct_el0\n\tisb" : "=r"(t) : : "memory");
    return t;
#endif
}

static double ns_per_tick()
{
    const auto w0 = std::chrono::steady_clock::now();
    const uint64_t c0 = serialized_clock();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const uint64_t c1 = serialized_clock();
    const auto w1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(w1 - w0).count() / static_cast<double>(c1 - c0);
}

static void wait_between_batches(std::mt19937& gen)
{
    std::uniform_int_distribution<int64_t> dis(MIN_WAIT_NS, MAX_WAIT_NS);
    const auto end = std::chrono::steady_clock::now() + std::chrono::nanoseconds(dis(gen));
    while (std::chrono::steady_clock::now() < end) {
    }
}

#if defined(BENCH_BQLOG_NORMAL) || defined(BENCH_BQLOG_FAST)
static bq::log* g_log = nullptr;
static void init_logger()
{
    std::string cfg = "log.buffer_size=" + std::to_string(FIXED_QUEUE_BYTES) + "\nlog.buffer_policy_when_full=block\nlog.worker_interval_ms=1\n";
    cfg += R"(
        appenders_config.appender_0.type=text_file
        appenders_config.appender_0.levels=[all]
        appenders_config.appender_0.file_name=output/bqlog_latency
        appenders_config.appender_0.always_create_new_file=true
    )";
    static bq::log log_obj = bq::log::create_log("lat", cfg.c_str());
    g_log = &log_obj;
}
#if defined(BENCH_BQLOG_FAST)
static const char* const LIB_NAME = "bqlog_fast";
#define LOG_CALL(k, i, d) BQ_LOG_FAST_INFO(*g_log, "Logging iteration: {}, message: {}, double: {}", k, i, d)
#else
static const char* const LIB_NAME = "bqlog_normal";
#define LOG_CALL(k, i, d) g_log->info("Logging iteration: {}, message: {}, double: {}", k, i, d)
#endif
static void flush_logger() { bq::log::force_flush_all_logs(); }

#elif defined(BENCH_QUILL)
struct fixed_queue_options : quill::FrontendOptions {
    static constexpr quill::QueueType queue_type = quill::QueueType::BoundedBlocking;
    static constexpr size_t initial_queue_capacity = FIXED_QUEUE_BYTES;
    static constexpr size_t unbounded_queue_max_capacity = FIXED_QUEUE_BYTES;
};
using fixed_frontend = quill::FrontendImpl<fixed_queue_options>;
static quill::LoggerImpl<fixed_queue_options>* g_logger = nullptr;
static const char* const LIB_NAME = "quill";
static void init_logger()
{
    quill::BackendOptions backend_options;
    backend_options.sleep_duration = std::chrono::milliseconds(1);
    quill::Backend::start(backend_options);
    const quill::PatternFormatterOptions pattern { "%(time) [%(thread_id)] %(log_level) %(message)", "%H:%M:%S.%Qns" };
    g_logger = fixed_frontend::create_or_get_logger("lat", fixed_frontend::create_or_get_sink<quill::FileSink>("output/quill_latency.log"), pattern);
}
#define LOG_CALL(k, i, d) LOG_INFO(g_logger, "Logging iteration: {}, message: {}, double: {}", k, i, d)
static void flush_logger() { g_logger->flush_log(); }

#elif defined(BENCH_FMTLOG)
static const char* const LIB_NAME = "fmtlog";
static void init_logger()
{
    fmtlog::setLogFile("output/fmtlog_latency.log", false);
    fmtlog::setHeaderPattern("{YmdHMSf} {l}[{t}] ");
    fmtlog::startPollingThread(1000000); // 1 ms
}
#define LOG_CALL(k, i, d) FMTLOG(fmtlog::INF, "Logging iteration: {}, message: {}, double: {}", k, i, d)
// poll is not thread safe with the polling thread, which drains by itself
static void flush_logger() { std::this_thread::sleep_for(std::chrono::milliseconds(500)); }

#elif defined(BENCH_SPDLOG)
static std::shared_ptr<spdlog::logger> g_logger;
static const char* const LIB_NAME = "spdlog_async";
static void init_logger()
{
    spdlog::init_thread_pool(8192, 1);
    g_logger = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>("lat", "output/spdlog_latency.log", true);
    g_logger->set_pattern("%Y-%m-%d %H:%M:%S.%f [%t] [%l] %v");
}
#define LOG_CALL(k, i, d) g_logger->info("Logging iteration: {}, message: {}, double: {}", k, i, d)
static void flush_logger() { g_logger->flush(); }
#endif

// log_func is a lambda, so every library's call site is inlined into the timed loop as in user code
template <typename LOG_FUNC>
static void producer(const LOG_FUNC& log_func, size_t thread_num, std::atomic<size_t>& started, size_t thread_count,
    double tick_ns, std::vector<uint64_t>& latencies, double& cpu_ms)
{
    // same warm up for every library: let it create its per thread state, then let the consumer drain
    for (uint64_t w = 0; w < 1000; ++w) {
        log_func(w, w, 0.0);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    started.fetch_add(1);
    while (started.load() != thread_count) {
    }
    std::mt19937 gen(static_cast<std::mt19937::result_type>(0x51ee50a7u + thread_num));
    latencies.reserve(ITERATIONS);
    const double cpu_start = bench_thread_cpu_ms();
    for (size_t iteration = 0; iteration < ITERATIONS; ++iteration) {
        const double d = static_cast<double>(iteration) + (0.1 * static_cast<double>(iteration));
        const uint64_t start = serialized_clock();
        for (size_t i = 0; i < MESSAGES_PER_BATCH; ++i) {
            log_func(iteration, i, d);
        }
        const uint64_t end = serialized_clock();
        latencies.push_back(static_cast<uint64_t>(static_cast<double>(end - start) / static_cast<double>(MESSAGES_PER_BATCH) * tick_ns));
        wait_between_batches(gen);
    }
    cpu_ms = bench_thread_cpu_ms() - cpu_start;
}

template <typename LOG_FUNC>
static void run(const LOG_FUNC& log_func, size_t thread_count)
{
    const double tick_ns = ns_per_tick();
    std::vector<std::vector<uint64_t>> latencies(thread_count);
    std::vector<double> producer_cpu(thread_count, 0.0);
    std::atomic<size_t> started { 0 };
    std::vector<std::thread> threads;
    for (size_t t = 0; t < thread_count; ++t) {
        threads.emplace_back([&, t]() {
            producer(log_func, t, started, thread_count, tick_ns, latencies[t], producer_cpu[t]);
        });
    }
    while (started.load() != thread_count) {
        std::this_thread::yield();
    }
    const double process_cpu_start = bench_process_cpu_ms();
    const auto wall_start = std::chrono::steady_clock::now();
    for (auto& th : threads) {
        th.join();
    }
    const double wall_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wall_start).count();
    double consumer_cpu_ms = bench_process_cpu_ms() - process_cpu_start;
    for (double ms : producer_cpu) {
        consumer_cpu_ms -= ms;
    }
    flush_logger();
    double current_mb, peak_mb;
    bench_memory_mb(current_mb, peak_mb);

    std::vector<uint64_t> all;
    for (auto& v : latencies) {
        all.insert(all.end(), v.begin(), v.end());
    }
    std::sort(all.begin(), all.end());
    double sum = 0;
    for (uint64_t v : all) {
        sum += static_cast<double>(v);
    }
    // nearest rank, as quill's benchmark
    const auto pct = [&](double p) {
        const size_t rank = static_cast<size_t>(std::ceil(static_cast<double>(all.size()) * p));
        return all[std::min(all.size() - 1, rank == 0 ? size_t { 0 } : rank - 1)];
    };
    printf("RESULT_LAT|%s|%zu|%.2f|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%.1f|%.1f\n", LIB_NAME, thread_count, sum / static_cast<double>(all.size()), (unsigned long long)pct(0.5), (unsigned long long)pct(0.75),
        (unsigned long long)pct(0.9), (unsigned long long)pct(0.95), (unsigned long long)pct(0.99),
        (unsigned long long)pct(0.999), (unsigned long long)all.back(), consumer_cpu_ms * 100.0 / wall_ms, peak_mb);
    fflush(stdout);
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        return 1;
    }
    const size_t thread_count = static_cast<size_t>(std::atoi(argv[1]));
    init_logger();
    run([](uint64_t k, uint64_t i, double d) { LOG_CALL(k, i, d); }, thread_count);
#if defined(BENCH_FMTLOG)
    _exit(0); // fmtlog has cleanup issues at exit
#elif defined(BENCH_SPDLOG)
    spdlog::shutdown();
#endif
    return 0;
}
