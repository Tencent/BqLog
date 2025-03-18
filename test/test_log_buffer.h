#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include <random>
#include <atomic>
#include <cstdio>
#include <vector>
#include <thread>
#include "test_base.h"
#include "bq_log/types/buffer/log_buffer.h"

namespace bq {
    namespace test {
        static bq::platform::atomic<int32_t> log_buffer_test_total_write_count_ = 0;
        constexpr int32_t log_buffer_total_task = 5;

        class log_block_list_test_task : public bq::platform::thread{
            private:
                block_list* from_;
                block_list* to_;
                bq::platform::atomic<int32_t> left_write_count_;

			public:
                log_block_list_test_task(block_list* from, block_list* to, int32_t left_write_count)
                    : from_(from)
                    , to_(to)
                    , left_write_count_(left_write_count)
                {
                }

                int32_t get_left_count() const
                {
                    return left_write_count_.load(bq::platform::memory_order::acquire);
                }

                virtual void run() override
                {
                    while (left_write_count_.load(bq::platform::memory_order::relaxed) > 0) {
                        if (auto block = from_->pop()) {
                            to_->push(block);
                            left_write_count_.fetch_add(-1, bq::platform::memory_order::release);
                        }
                    }
                }
        };

        class log_buffer_write_task {
        private:
            int32_t id;
            bq::log_buffer* log_buffer_ptr;
            int32_t left_write_count;
            bq::platform::atomic<int32_t>& counter_ref;

        public:
            const static int32_t min_chunk_size = 12;
            const static int32_t max_chunk_size = 1024;
            log_buffer_write_task(int32_t id, int32_t left_write_count, bq::log_buffer* ring_buffer_ptr, bq::platform::atomic<int32_t>& counter)
                : counter_ref(counter)
            {
                this->id = id;
                this->left_write_count = left_write_count;
                this->log_buffer_ptr = ring_buffer_ptr;
            }

            void operator()()
            {
                std::random_device sd;
                std::minstd_rand linear_ran(sd());
                std::uniform_int_distribution<int32_t> rand_seq(min_chunk_size, max_chunk_size);
                while (left_write_count > 0) {
                    uint32_t alloc_size = (uint32_t)rand_seq(linear_ran);
                    auto handle = log_buffer_ptr->alloc_write_chunk(alloc_size, bq::platform::high_performance_epoch_ms());
                    if (handle.result == bq::enum_buffer_result_code::err_not_enough_space
                        || handle.result == bq::enum_buffer_result_code::err_buffer_not_inited
                        || handle.result == bq::enum_buffer_result_code::err_wait_and_retry) {
                        bq::platform::thread::yield();
                        continue;
                    }
                    --left_write_count;
                    assert(handle.result == bq::enum_buffer_result_code::success);
                    *(int32_t*)(handle.data_addr) = id;
                    *((int32_t*)(handle.data_addr) + 1) = left_write_count;
                    int32_t count = (int32_t)alloc_size / sizeof(int32_t);
                    int32_t* begin = (int32_t*)(handle.data_addr) + 2;
                    int32_t* end = (int32_t*)(handle.data_addr) + count;
                    std::fill(begin, end, (int32_t)alloc_size);
                    log_buffer_ptr->commit_write_chunk(handle);
                    ++log_buffer_test_total_write_count_;
                }
                counter_ref.fetch_add(-1, bq::platform::memory_order::release);
            }
        };

        class test_log_buffer : public test_base {
        private:
            void do_block_list_test(test_result& result, log_buffer_config config)
            {
                config.default_buffer_size = bq::roundup_pow_of_two(config.default_buffer_size);
                constexpr size_t BLOCK_COUNT = 16;
                size_t size = sizeof(block_list) * 2 + config.default_buffer_size * BLOCK_COUNT + CACHE_LINE_SIZE;
                bq::array<uint8_t> buffer;
                buffer.fill_uninitialized(size);
                uintptr_t base_addr = (uintptr_t)(uint8_t*)buffer.begin();
                uintptr_t aligned_addr = (base_addr + (uintptr_t)(CACHE_LINE_SIZE - 1)) & ~((uintptr_t)CACHE_LINE_SIZE - 1);
                uintptr_t buffer_addr = aligned_addr + 2 * sizeof(block_list);
                new ((void*)aligned_addr, bq::enum_new_dummy::dummy) block_list(BLOCK_COUNT, (uint8_t*)buffer_addr, size - (ptrdiff_t)(buffer_addr - base_addr), config.use_mmap);
                block_list& list_from = *(block_list*)aligned_addr;
                aligned_addr += sizeof(block_list);
                new ((void*)aligned_addr, bq::enum_new_dummy::dummy) block_list(BLOCK_COUNT, (uint8_t*)buffer_addr, size - (ptrdiff_t)(buffer_addr - base_addr), config.use_mmap);
                block_list& list_to = *(block_list*)aligned_addr;

                if (config.use_mmap) {
                    while (true) {
                        if (!list_from.pop()) {
                            break;
                        }
                    }
                    while (true) {
                        if (!list_to.pop()) {
                            break;
                        }
                    }
                }

                for (uint16_t i = 0; i < BLOCK_COUNT; ++i) {
                    uint8_t* block_head_addr = (uint8_t*)(buffer_addr + i * config.default_buffer_size);
                    new ((void*)block_head_addr, bq::enum_new_dummy::dummy) block_node_head(block_head_addr + block_node_head::get_buffer_data_offset(), config.default_buffer_size - (size_t)block_node_head::get_buffer_data_offset(), config.use_mmap);
                    block_node_head* block = (block_node_head*)block_head_addr;
                    list_from.push(block);
                }
                constexpr int32_t LOOP_COUNT = 1000000;
                log_block_list_test_task task1(&list_from, &list_to, LOOP_COUNT);
                log_block_list_test_task task2(&list_to, &list_from, LOOP_COUNT);
                task1.start();
                task2.start();
                
                int32_t percent = 0;
                test_output_dynamic_param(bq::log_level::info, "[block list] mmap:%s, test progress:%d%%, time cost:%dms\r", config.use_mmap ? "Y" : "-",  percent, 0);
                auto start_time = bq::platform::high_performance_epoch_ms();
                while (task1.get_left_count() + task2.get_left_count() > 0) {
                    int32_t current_left_count = 2 * LOOP_COUNT - task1.get_left_count() - task2.get_left_count();
                    int32_t new_percent = current_left_count * 100 / (2 * LOOP_COUNT);
                    if (new_percent != percent) {
                        percent = new_percent;
                        auto current_time = bq::platform::high_performance_epoch_ms();
                        test_output_dynamic_param(bq::log_level::info, "[block list] mmap:%s, test progress:%d%%, time cost:%dms              \r", config.use_mmap ? "Y" : "-", percent, (int32_t)(current_time - start_time));
                    }
                    bq::platform::thread::yield();
                }
                task1.join();
                task2.join();
                percent = 100;
                auto current_time = bq::platform::high_performance_epoch_ms();
                test_output_dynamic_param(bq::log_level::info, "[block list] mmap:%s, test progress:%d%%, time cost:%dms              \r", config.use_mmap ? "Y" : "-", percent, (int32_t)(current_time - start_time));
                test_output_dynamic_param(bq::log_level::info, "\n[block list] mmap:%s, test finished\n", config.use_mmap ? "Y" : "-");
                result.add_result(list_to.pop() == nullptr, "block list test 1");
                size_t num_from = 0;
                auto from_node = list_from.first();
                while (from_node) {
                    ++num_from;
                    from_node = list_from.next(from_node);
                }
                result.add_result(num_from == BLOCK_COUNT, "block list test 2");

                
                for (uint16_t i = 0; i < BLOCK_COUNT; ++i) {
                    uint8_t* block_head_addr = (uint8_t*)(buffer_addr + i * config.default_buffer_size);
                    block_node_head* block = (block_node_head*)block_head_addr;
                    block->~block_node_head();
                }
                (&list_from)->~block_list();
                (&list_to)->~block_list();
            }

            void do_basic_test(test_result& result, log_buffer_config config)
            {
                log_buffer_test_total_write_count_.store_seq_cst(0);
                bq::log_buffer ring_buffer(config);
                int32_t chunk_count_per_task = 1024000;
                bq::platform::atomic<int32_t> counter(log_buffer_total_task);
                std::vector<int32_t> task_check_vector;
                std::vector<std::thread> task_thread_vector; 
                for (int32_t i = 0; i < log_buffer_total_task; ++i) {
                    task_check_vector.push_back(0);
                    log_buffer_write_task task(i, chunk_count_per_task, &ring_buffer, counter);
                    task_thread_vector.emplace_back(task);
                }

                int32_t total_chunk = chunk_count_per_task * log_buffer_total_task;
                int32_t readed_chunk = 0;
                int32_t percent = 0;
                auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

                test_output_dynamic_param(bq::log_level::info, "================\n[log buffer] mmap:%s, auto expand:%s, high performance mode:%s\n"
                    , config.use_mmap ? "Y" : "-"
                    , config.policy == log_memory_policy::auto_expand_when_full ? "Y" : "-"
                    , config.high_frequency_threshold_per_second < UINT64_MAX ? "Y" : "-");
                test_output_dynamic_param(bq::log_level::info, "[log buffer] test progress:%d%%, time cost:%dms\r", percent, 0);
                while (true) {
                    bool write_finished = (counter.load(bq::platform::memory_order::acquire) <= 0);
                    auto handle = ring_buffer.read_chunk();
                    bq::scoped_log_buffer_handle<log_buffer> read_handle(ring_buffer, handle);
                    bool read_empty = handle.result == bq::enum_buffer_result_code::err_empty_log_buffer;
                    if (write_finished && read_empty) {
                        break;
                    }
                    if (handle.result != bq::enum_buffer_result_code::success) {
                        continue;
                    }
                    int32_t size = (int32_t)handle.data_size;
                    if (size < log_buffer_write_task::min_chunk_size || size > log_buffer_write_task::max_chunk_size) {
                        result.add_result(false, "ring buffer chunk size error");
                        continue;
                    }
                    ++readed_chunk;
                    int32_t new_percent = readed_chunk * 100 / total_chunk;
                    if (new_percent != percent) {
                        percent = new_percent;
                        auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        test_output_dynamic_param(bq::log_level::info, "[log buffer] test progress:%d%%, time cost:%dms              \r", percent, (int32_t)(current_time - start_time));
                    }

                    int32_t id = *(int32_t*)handle.data_addr;
                    int32_t left_count = *((int32_t*)handle.data_addr + 1);
                    if (id < 0 || id >= log_buffer_total_task) {
                        std::string error_msg = "invalid task id:";
                        error_msg += std::to_string(id);
                        result.add_result(false, error_msg.c_str());
                        continue;
                    }

                    task_check_vector[id]++;
                    result.add_result(task_check_vector[id] + left_count == chunk_count_per_task, "[log buffer]chunk left task check error, real: %d, expected:%d", left_count, chunk_count_per_task - task_check_vector[id]);
                    task_check_vector[id] = chunk_count_per_task - left_count; // error adjust
                    bool content_check = true;
                    for (size_t i = 2; i < size / sizeof(int32_t); ++i) {
                        if (*((int32_t*)handle.data_addr + i) != size) {
                            content_check = false;
                            continue;
                        }
                    }
                    result.add_result(content_check, "[log buffer]content check error");
                }
                test_output_dynamic_param(bq::log_level::info, "[log buffer] test progress:%d%%, time cost:%dms              \r", 100, (int32_t)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() - start_time));
                for (size_t i = 0; i < task_check_vector.size(); ++i) {
                    result.add_result(task_check_vector[i] == chunk_count_per_task, "[log buffer]chunk count check error, real:%d , expected:%d", task_check_vector[i], chunk_count_per_task);
                }
                test_output_dynamic(bq::log_level::info, "\n[log buffer] test finished\n");
                result.add_result(total_chunk == log_buffer_test_total_write_count_.load(), "total write count error, real:%d , expected:%d", log_buffer_test_total_write_count_.load(), total_chunk);
                result.add_result(total_chunk == readed_chunk, "[log buffer] total chunk count check error, read:%d , expected:%d", readed_chunk, total_chunk);
            
                for (auto& task : task_thread_vector) {
                    task.join();
                }
                auto final_handle = ring_buffer.read_chunk();
                bq::scoped_log_buffer_handle<log_buffer> scoped_final_handle(ring_buffer, final_handle);
                result.add_result(final_handle.result == bq::enum_buffer_result_code::err_empty_log_buffer, "final read test");
                result.add_result(ring_buffer.get_groups_count() == 0, "group recycle test");
            }
        public:
            virtual test_result test() override
            {
                test_result result;

                log_buffer_config config;
                config.log_name = "log_buffer_test";
                config.log_categories_name = { "_default"};
                config.use_mmap = false;
                config.policy = log_memory_policy::auto_expand_when_full;
                config.high_frequency_threshold_per_second = 1000;

                do_block_list_test(result, config);
                config.use_mmap = true;
                do_block_list_test(result, config);

                do_basic_test(result, config);
                config.policy = log_memory_policy::block_when_full;
                do_basic_test(result, config);
                config.use_mmap = false;
                do_basic_test(result, config);
                config.policy = log_memory_policy::auto_expand_when_full;
                do_basic_test(result, config);

                config.high_frequency_threshold_per_second = UINT64_MAX;
                do_basic_test(result, config);
                config.policy = log_memory_policy::block_when_full;
                do_basic_test(result, config);
                config.use_mmap = true;
                do_basic_test(result, config);
                config.policy = log_memory_policy::auto_expand_when_full;
                do_basic_test(result, config);

                return result;
            }
        };
    }
}
