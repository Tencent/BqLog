/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
//
// Write thread cost: how long the calling thread spends inside one log call.
//
// Same shape as the common "hot path latency" benchmarks of other C++ loggers: each thread logs a batch of
// MESSAGES_PER_ITERATION entries, times the whole batch, then waits 2.0 ~ 2.2 ms before the next batch, so the
// consumer can keep up and what is measured is the producer side only. The batch time divided by the batch size is
// one sample; the percentiles of all samples are reported.
//
// Deliberately no CPU pinning, frequency locking or warm-up: the numbers should reflect an ordinary application
// on an ordinary machine. Results vary between runs and machines; compare runs taken on the same machine.
#include "bq_log/bq_log.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

namespace write_thread_cost {
    static constexpr size_t ITERATIONS = 10000;
    static constexpr size_t MESSAGES_PER_ITERATION = 20;
    static constexpr int64_t MIN_WAIT_US = 2000;
    static constexpr int64_t MAX_WAIT_US = 2200;

    enum class write_mode {
        normal, // log.info(...)
        fast // BQ_LOG_FAST_INFO(...)
    };

    // Busy waits rather than sleeping: a sleep would let the core clock down and bill the wake-up to the next batch.
    static void wait_between_batches(std::minstd_rand& random)
    {
        std::uniform_int_distribution<int64_t> wait_us(MIN_WAIT_US, MAX_WAIT_US);
        const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(wait_us(random));
        while (std::chrono::steady_clock::now() < end) {
        }
    }

    static void write_batch(const bq::log& log_obj, write_mode mode, uint64_t iteration, double value)
    {
        if (mode == write_mode::normal) {
            for (uint64_t i = 0; i < MESSAGES_PER_ITERATION; ++i) {
                log_obj.info("Logging iteration: {}, message: {}, double: {}", iteration, i, value);
            }
        } else {
            for (uint64_t i = 0; i < MESSAGES_PER_ITERATION; ++i) {
                BQ_LOG_FAST_INFO(log_obj, "Logging iteration: {}, message: {}, double: {}", iteration, i, value);
            }
        }
    }

    static void run(const bq::log& log_obj, write_mode mode, int32_t thread_count)
    {
        std::vector<std::vector<double>> per_thread_samples(static_cast<size_t>(thread_count));
        std::atomic<int32_t> ready_count(0);
        std::vector<std::thread> threads;
        threads.reserve(static_cast<size_t>(thread_count));
        for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
            threads.emplace_back([&, thread_index]() {
                std::vector<double>& samples = per_thread_samples[static_cast<size_t>(thread_index)];
                samples.reserve(ITERATIONS);
                std::minstd_rand random(static_cast<uint32_t>(thread_index + 1));
                ready_count.fetch_add(1);
                while (ready_count.load() < thread_count) {
                }
                for (uint64_t iteration = 0; iteration < ITERATIONS; ++iteration) {
                    const double value = static_cast<double>(iteration) * 1.1;
                    const auto begin = std::chrono::steady_clock::now();
                    write_batch(log_obj, mode, iteration, value);
                    const auto end = std::chrono::steady_clock::now();
                    samples.push_back(static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()) / static_cast<double>(MESSAGES_PER_ITERATION));
                    wait_between_batches(random);
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        bq::log::force_flush_all_logs();

        std::vector<double> samples;
        for (const auto& thread_samples : per_thread_samples) {
            samples.insert(samples.end(), thread_samples.begin(), thread_samples.end());
        }
        std::sort(samples.begin(), samples.end());
        auto percentile = [&samples](double p) {
            size_t rank = static_cast<size_t>(p / 100.0 * static_cast<double>(samples.size()));
            return samples[std::min(rank, samples.size() - 1)];
        };
        std::cout << std::fixed << std::setprecision(1);
        std::cout << (mode == write_mode::normal ? "log.info()         " : "BQ_LOG_FAST_INFO() ")
                  << "| threads " << thread_count << " | ns per log | 50th " << percentile(50) << " | 75th " << percentile(75)
                  << " | 90th " << percentile(90) << " | 95th " << percentile(95) << " | 99th " << percentile(99)
                  << " | 99.9th " << percentile(99.9) << " | worst " << samples.back() << std::endl;
    }

    static void test(int32_t thread_count)
    {
        std::cout << "============================================================" << std::endl;
        std::cout << "=========Begin Write Thread Cost Test (text file)=========" << std::endl;
        std::cout << "Each thread logs " << ITERATIONS << " batches of " << MESSAGES_PER_ITERATION
                  << " entries, waiting " << MIN_WAIT_US << " ~ " << MAX_WAIT_US << " us between batches, please wait the result..." << std::endl;
        bq::log log_obj = bq::log::get_log_by_name("write_thread_cost");
        // the same write once, so both modes start from a registered call site and an allocated buffer
        write_batch(log_obj, write_mode::normal, 0, 0.0);
        write_batch(log_obj, write_mode::fast, 0, 0.0);
        bq::log::force_flush_all_logs();
        run(log_obj, write_mode::normal, 1);
        run(log_obj, write_mode::fast, 1);
        if (thread_count > 1) {
            run(log_obj, write_mode::normal, thread_count);
            run(log_obj, write_mode::fast, thread_count);
        }
        std::cout << "============================================================" << std::endl
                  << std::endl;
    }
}
