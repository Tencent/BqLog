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
#include "bq_log/types/buffer/memory_pool.h"

namespace bq {
    namespace test {
        static bq::platform::atomic<int32_t> linked_list_test_number_generator_ = 0;
        static bq::platform::atomic<int32_t> log_buffer_test_total_write_count_ = 0;
        constexpr int32_t log_buffer_total_task = 5;
        constexpr int32_t linked_list_test_number_per_thread = 2000000;


        class miso_linked_list_test_insert_task {
        private:
            bq::miso_linked_list<int32_t>& linked_list_;
            test_result& result_;
        public:
            miso_linked_list_test_insert_task(bq::miso_linked_list<int32_t>& linked_list, test_result& result)
                : linked_list_(linked_list)
                , result_(result)
            {
                (void)result_;
            }
            void operator()()
            {
                for (uint32_t i = 0; i < linked_list_test_number_per_thread; ++i) {
                    int32_t number = linked_list_test_number_generator_.fetch_add_release(1);
                    auto iter = linked_list_.insert(number);
                    (void)iter;
                }
                while (linked_list_test_number_generator_.load_acquire() < linked_list_test_number_per_thread * log_buffer_total_task) {
                    bq::platform::thread::yield();
                }
                linked_list_test_number_generator_.fetch_add_release(1);
            }
        };

        class miso_linked_list_test_remove_task {
        private:
            bq::miso_linked_list<int32_t>& linked_list_;
            test_result& result_;

        public:
            miso_linked_list_test_remove_task(bq::miso_linked_list<int32_t>& linked_list, test_result& result)
                : linked_list_(linked_list)
                , result_(result)
            {
            }

            void operator()()
            {
                int32_t max_value = linked_list_test_number_per_thread * log_buffer_total_task;
                bq::array<bool> mark_array;
                mark_array.fill_uninitialized((size_t)max_value);
                memset(mark_array.begin(), 0, (size_t)max_value);
                int32_t total_read_number = 0;
                while (true) {
                    bool all_insert_done = (linked_list_test_number_generator_.load_acquire() == max_value + log_buffer_total_task);
                    auto iter = linked_list_.first();
                    uint32_t idx = 0;
                    while (iter) {
                        if (all_insert_done || ((++idx) % 2 == 0)) {
                            mark_array[iter.value()] = true;
                            iter = linked_list_.remove(iter);
                            ++total_read_number;
                            continue;
                        }
                        iter = linked_list_.next(iter);
                    }
                    if (all_insert_done) {
                        break;
                    }
                }
                result_.add_result(total_read_number == max_value, "linked list remove test total number failed");
                for (size_t i = 0; i < mark_array.size(); ++i) {
                    result_.add_result(mark_array[i], "linked list remove test failed, %zu", i);
                }
            }
        };

        class memory_pool_test_obj_aligned : public bq::memory_pool_obj_base<memory_pool_test_obj_aligned, true> {
        public :
            int32_t id_;
            memory_pool_test_obj_aligned(int32_t id)
                : id_(id)
            {
            }
        };
        class memory_pool_test_obj_not_aligned : public bq::memory_pool_obj_base<memory_pool_test_obj_not_aligned, false> {
        public:
            int32_t id_;
            memory_pool_test_obj_not_aligned(int32_t id)
                : id_(id)
            {
            }
        };

        template<bool is_aligned>
        class memory_pool_test_task {
            using obj_type = bq::condition_type_t<is_aligned, memory_pool_test_obj_aligned, memory_pool_test_obj_not_aligned>;
            bq::memory_pool<obj_type>& from_pool_;
            bq::memory_pool<obj_type>& to_pool_;
            int32_t left_loop_count_;
            int32_t op_;
        public:
            memory_pool_test_task(bq::memory_pool<obj_type>& from_pool, bq::memory_pool<obj_type>& to_pool, int32_t left_loop_count, int32_t op)
                : from_pool_(from_pool)
                , to_pool_(to_pool)
                , left_loop_count_(left_loop_count)
                , op_(op)
            {
            }

            void operator()()
            {
                while (left_loop_count_> 0) {
                    switch (op_) {
                    case 0:
                        if (auto obj = from_pool_.pop()) {
                            to_pool_.push(obj);
                            --left_loop_count_;
                        } else {
                            bq::platform::thread::yield();
                        }
                        break;
                    case 1:
                        if (auto obj = from_pool_.evict([](const obj_type* candidiate_obj, void* user_data) {
                                (void)candidiate_obj;
                                (void)user_data;
                                return true;
                            }, nullptr)){
                            to_pool_.push(obj);
                            --left_loop_count_;
                        } else {
                            bq::platform::thread::yield();
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        };



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
            protected:
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
            void do_linked_list_test(test_result& result)
            {
                test_output_dynamic(bq::log_level::info, "[memory pool] miso linked list test begin\n");
                bq::miso_linked_list<int32_t> linked_list;
                std::vector<std::thread> insert_tasks;
                for (int32_t i = 0; i < log_buffer_total_task; ++i) {
                    insert_tasks.emplace_back(miso_linked_list_test_insert_task(linked_list, result));
                }
                std::thread remove_task(miso_linked_list_test_remove_task(linked_list, result));
                remove_task.join();
                for (auto& task : insert_tasks) {
                    task.join();
                }
                test_output_dynamic(bq::log_level::info, "[memory pool] miso linked list test end\n");
            }

            void do_memory_pool_test(test_result& result)
            {
                {
                    constexpr int32_t LOOP_COUNT = 1000000;
                    constexpr int32_t OBJ_COUNT = 1024;
                    constexpr int32_t THREAD_COUNT = 4;
                    bq::memory_pool<memory_pool_test_obj_aligned> pool_aligned_form, pool_aligned_to;
                    bq::memory_pool<memory_pool_test_obj_not_aligned> pool_not_aligned_form, pool_not_aligned_to;

                    for (int32_t i = 0; i < OBJ_COUNT; ++i) {
                        pool_aligned_form.push(bq::util::aligned_new<memory_pool_test_obj_aligned>(CACHE_LINE_SIZE, i));
                        pool_aligned_to.push(util::aligned_new<memory_pool_test_obj_aligned>(CACHE_LINE_SIZE, i));
                        pool_not_aligned_form.push(new memory_pool_test_obj_not_aligned(i));
                        pool_not_aligned_to.push(new memory_pool_test_obj_not_aligned(i));
                    }

                    // aligned test
                    test_output_dynamic(bq::log_level::info, "[memory pool] Cache Line Aligned test begin\n");
                    uint64_t start_time = bq::platform::high_performance_epoch_ms();
                    std::vector<std::thread> task_aligned; 
                    for (int32_t i = 0; i < THREAD_COUNT; ++i) {
                        task_aligned.emplace_back(memory_pool_test_task<true>(pool_aligned_form, pool_aligned_to, LOOP_COUNT, i % 2));
                        task_aligned.emplace_back(memory_pool_test_task<true>(pool_aligned_to, pool_aligned_form, LOOP_COUNT, i % 2));
                    }
                    for (auto& task : task_aligned) {
                        task.join();
                    }
                    test_output_dynamic_param(bq::log_level::info, "[memory pool] Cache Line Aligned test end, time cost:%" PRIu64 "ms\n\n", bq::platform::high_performance_epoch_ms() - start_time);
                    
                    bq::array<int32_t> marks;
                    marks.fill_uninitialized(OBJ_COUNT);
                    memset((int32_t*)marks.begin(), 0, sizeof(decltype(marks)::value_type) * marks.size());
                    while (auto obj = pool_aligned_form.pop()) {
                        marks[obj->id_]++;
                        bq::util::aligned_delete(obj);
                    }
                    while (auto obj = pool_aligned_to.pop()) {
                        marks[obj->id_]++;
                        bq::util::aligned_delete(obj);
                    }
                    for (int32_t i = 0; i < OBJ_COUNT; ++i) {
                        result.add_result(marks[i] == 2, "memory pool aligned test %d", i);
                    }

                    
                    // not aligned test
                    test_output_dynamic(bq::log_level::info, "[memory pool] Cache Line Not Aligned test begin\n");
                    start_time = bq::platform::high_performance_epoch_ms();
                    std::vector<std::thread> task_not_aligned;
                    for (int32_t i = 0; i < THREAD_COUNT; ++i) {
                        task_not_aligned.emplace_back(memory_pool_test_task<false>(pool_not_aligned_form, pool_not_aligned_to, LOOP_COUNT, i % 2));
                        task_not_aligned.emplace_back(memory_pool_test_task<false>(pool_not_aligned_to, pool_not_aligned_form, LOOP_COUNT, i % 2));
                    }
                    for (auto& task : task_not_aligned) {
                        task.join();
                    }
                    test_output_dynamic_param(bq::log_level::info, "[memory pool] Cache Line Not Aligned test end, time cost:%" PRIu64 "ms\n\n", bq::platform::high_performance_epoch_ms() - start_time);

                    memset((int32_t*)marks.begin(), 0, sizeof(decltype(marks)::value_type) * marks.size());
                    while (auto obj = pool_not_aligned_form.pop()) {
                        marks[obj->id_]++;
                        delete obj;
                    }
                    while (auto obj = pool_not_aligned_to.pop()) {
                        marks[obj->id_]++;
                        delete obj;
                    }
                    for (int32_t i = 0; i < OBJ_COUNT; ++i) {
                        result.add_result(marks[i] == 2, "memory pool not aligned test %d", i);
                    }
                }
            }


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
                new ((void*)aligned_addr, bq::enum_new_dummy::dummy) block_list(BLOCK_COUNT, (uint8_t*)buffer_addr, size - (ptrdiff_t)(buffer_addr - base_addr), config.need_recovery);
                block_list& list_from = *(block_list*)aligned_addr;
                aligned_addr += sizeof(block_list);
                new ((void*)aligned_addr, bq::enum_new_dummy::dummy) block_list(BLOCK_COUNT, (uint8_t*)buffer_addr, size - (ptrdiff_t)(buffer_addr - base_addr), config.need_recovery);
                block_list& list_to = *(block_list*)aligned_addr;

                if (config.need_recovery) {
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
                    new ((void*)block_head_addr, bq::enum_new_dummy::dummy) block_node_head(block_head_addr + block_node_head::get_buffer_data_offset(), config.default_buffer_size - (size_t)block_node_head::get_buffer_data_offset(), config.need_recovery);
                    block_node_head* block = (block_node_head*)block_head_addr;
                    list_from.push(block);
                }
                constexpr int32_t LOOP_COUNT = 1000000;
                log_block_list_test_task task1(&list_from, &list_to, LOOP_COUNT);
                log_block_list_test_task task2(&list_to, &list_from, LOOP_COUNT);
                task1.start();
                task2.start();
                
                int32_t percent = 0;
                test_output_dynamic_param(bq::log_level::info, "[block list] recovery:%s, test progress:%d%%, time cost:%dms\r", config.need_recovery ? "Y" : "-",  percent, 0);
                auto start_time = bq::platform::high_performance_epoch_ms();
                while (task1.get_left_count() + task2.get_left_count() > 0) {
                    int32_t current_left_count = 2 * LOOP_COUNT - task1.get_left_count() - task2.get_left_count();
                    int32_t new_percent = current_left_count * 100 / (2 * LOOP_COUNT);
                    if (new_percent != percent) {
                        percent = new_percent;
                        auto current_time = bq::platform::high_performance_epoch_ms();
                        test_output_dynamic_param(bq::log_level::info, "[block list] recovery:%s, test progress:%d%%, time cost:%dms              \r", config.need_recovery ? "Y" : "-", percent, (int32_t)(current_time - start_time));
                    }
                    bq::platform::thread::yield();
                }
                task1.join();
                task2.join();
                percent = 100;
                auto current_time = bq::platform::high_performance_epoch_ms();
                test_output_dynamic_param(bq::log_level::info, "[block list] recovery:%s, test progress:%d%%, time cost:%dms              \r", config.need_recovery ? "Y" : "-", percent, (int32_t)(current_time - start_time));
                test_output_dynamic_param(bq::log_level::info, "\n[block list] recovery:%s, test finished, time cost:%dms\n", config.need_recovery ? "Y" : "-", (int32_t)(current_time - start_time));
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
                bq::log_buffer test_buffer(config);
                int32_t chunk_count_per_task = 1024000;
                bq::platform::atomic<int32_t> counter(log_buffer_total_task);
                std::vector<int32_t> task_check_vector;
                std::vector<std::thread> task_thread_vector; 
                for (int32_t i = 0; i < log_buffer_total_task; ++i) {
                    task_check_vector.push_back(0);
                    log_buffer_write_task task(i, chunk_count_per_task, &test_buffer, counter);
                    task_thread_vector.emplace_back(task);
                }

                int32_t total_chunk = chunk_count_per_task * log_buffer_total_task;
                int32_t readed_chunk = 0;
                int32_t percent = 0;
                auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

                test_output_dynamic_param(bq::log_level::info, "================\n[log buffer] recovery:%s, auto expand:%s, high performance mode:%s\n"
                    , config.need_recovery ? "Y" : "-"
                    , config.policy == log_memory_policy::auto_expand_when_full ? "Y" : "-"
                    , config.high_frequency_threshold_per_second < UINT64_MAX ? "Y" : "-");
                test_output_dynamic_param(bq::log_level::info, "[log buffer] test progress:%d%%, time cost:%dms\r", percent, 0);

                int32_t read_empty_time = 0;
                while (true) {
                    bool write_finished = (counter.load(bq::platform::memory_order::acquire) <= 0);
                    auto handle = test_buffer.read_chunk();
                    bq::scoped_log_buffer_handle<log_buffer> read_handle(test_buffer, handle);
                    bool read_empty = handle.result == bq::enum_buffer_result_code::err_empty_log_buffer;
                    if (write_finished && read_empty) {
                        // without double check, this may fails on ARM chips, need review.
                        if (++read_empty_time >= 2) {
                            break;
                        }
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
                test_output_dynamic_param(bq::log_level::info, "\n[log buffer] test finished, time cost:%dms\n", (int32_t)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() - start_time));
                result.add_result(total_chunk == log_buffer_test_total_write_count_.load(), "total write count error, real:%d , expected:%d", log_buffer_test_total_write_count_.load(), total_chunk);
                result.add_result(total_chunk == readed_chunk, "[log buffer] total chunk count check error, read:%d , expected:%d", readed_chunk, total_chunk);
            
                for (auto& task : task_thread_vector) {
                    task.join();
                }
                // If double read is not performed,
                // there is no guarantee that all memory cleanup will be completed by the next read,
                // as memory cleanup, for the sake of performance, does not guarantee timeliness.
                {
                    auto final_handle = test_buffer.read_chunk();
                    bq::scoped_log_buffer_handle<log_buffer> scoped_final_handle(test_buffer, final_handle);
                    result.add_result(final_handle.result == bq::enum_buffer_result_code::err_empty_log_buffer, "final read test");
                }
                {
                    auto final_handle = test_buffer.read_chunk();
                    bq::scoped_log_buffer_handle<log_buffer> scoped_final_handle(test_buffer, final_handle);
                    result.add_result(final_handle.result == bq::enum_buffer_result_code::err_empty_log_buffer, "final read test");
                }
#if !defined(BQ_WIN) || !defined(BQ_GCC)   //MinGW with GCC has bug on thread_local, so the log buffer recycle may not work properly.
                result.add_result(test_buffer.get_groups_count() == 0, "group recycle test， expected left group:0, but: %" PRIu32 "", test_buffer.get_groups_count());
#endif
                bq::platform::thread::sleep(group_list::GROUP_NODE_GC_LIFE_TIME_MS * 2);
                test_buffer.garbage_collect();
                result.add_result(test_buffer.get_garbage_count() == 0, "group garbage collect test");
            }


            void do_recovery_test(test_result& result)
            {
                if (!bq::memory_map::is_platform_support()) {
                    return;
                }
                constexpr uint32_t WRITE_VERSION_COUNT = 5;
                constexpr uint32_t MESSAGE_PER_VERSION = 100000;
                test_output_dynamic(bq::log_level::info, "=======================\n[log buffer] do recovery test begin...\n");
                
                //Single Thread Test
                for (uint32_t version = 0; version < WRITE_VERSION_COUNT; ++version) {
                     log_buffer_config config;
                     config.log_name = "log_buffer_recovery_test";
                     config.log_categories_name = { "_default" };
                     config.need_recovery = true;
                     config.policy = log_memory_policy::auto_expand_when_full;
                     config.high_frequency_threshold_per_second = 1000;
                     bq::log_buffer test_recovery_buffer(config);
                     std::random_device sd;
                     std::minstd_rand linear_ran(sd());
                     std::uniform_int_distribution<int32_t> rand_seq(0, 1024);
                     for (uint32_t i = 0; i < MESSAGE_PER_VERSION; ++i) {
                         uint32_t alloc_size = (uint32_t)rand_seq(linear_ran);
                         auto handle = test_recovery_buffer.alloc_write_chunk(alloc_size, bq::platform::high_performance_epoch_ms());
                         result.add_result(handle.result == enum_buffer_result_code::success, "recovery test write alloc, size:%" PRIu32 "", alloc_size);
                         bq::scoped_log_buffer_handle<log_buffer> scoped_handle(test_recovery_buffer, handle);
                         for (size_t pos = 0; pos + sizeof(uint64_t) <= alloc_size; pos += sizeof(uint64_t)) {
                             uint32_t* ptr = reinterpret_cast<uint32_t*>(handle.data_addr + pos);
                             ptr[0] = version;
                             ptr[1] = i;
                         }
                     }
                 }

                 {
                     log_buffer_config config;
                     config.log_name = "log_buffer_recovery_test";
                     config.log_categories_name = { "_default" };
                     config.need_recovery = true;
                     config.policy = log_memory_policy::auto_expand_when_full;
                     config.high_frequency_threshold_per_second = 1000;
                     bq::log_buffer test_recovery_buffer(config);
                     for (uint32_t version = 0; version < WRITE_VERSION_COUNT; ++version) {
                         for (uint32_t i = 0; i < MESSAGE_PER_VERSION; ++i) {
                             auto handle = test_recovery_buffer.read_chunk();
                             result.add_result(handle.result == enum_buffer_result_code::success, "recovery test read chunk, version:%" PRIu32 "index:%" PRIu32 "", version , i);
                             bq::scoped_log_buffer_handle<log_buffer> scoped_handle(test_recovery_buffer, handle);
                             bool valid = true;
                             for (size_t pos = 0; pos + sizeof(uint64_t) <= handle.data_size; pos += sizeof(uint64_t)) {
                                 uint32_t* ptr = reinterpret_cast<uint32_t*>(handle.data_addr + pos);
                                 if (ptr[0] != version || ptr[1] != i) {
                                     valid = false;
                                     break;
                                 }
                             }
                             result.add_result(valid, "recovery test read content, version:%" PRIu32 "index:%" PRIu32 "", version, i);
                         }
                     }
                     auto handle = test_recovery_buffer.read_chunk();
                     result.add_result(handle.result == enum_buffer_result_code::err_empty_log_buffer, "recovery test read content final for single thread");
                 }

                // Multi Thread Test
                constexpr uint32_t THREAD_COUNT = 5;
                uint32_t message_verify_group[WRITE_VERSION_COUNT][THREAD_COUNT];
                static_assert(sizeof(message_verify_group) == sizeof(uint32_t) * WRITE_VERSION_COUNT * THREAD_COUNT, "message_verify_group size error");
                memset(message_verify_group, 0, sizeof(message_verify_group));
                uint32_t total_message_count = WRITE_VERSION_COUNT * THREAD_COUNT * MESSAGE_PER_VERSION;
                uint32_t read_message_count = 0;
                for (uint32_t version = 0; version < WRITE_VERSION_COUNT; ++version) {
                    log_buffer_config config;
                    config.log_name = "log_buffer_recovery_test";
                    config.log_categories_name = { "_default" };
                    config.need_recovery = true;
                    config.policy = log_memory_policy::auto_expand_when_full;
                    config.high_frequency_threshold_per_second = 1000;
                    bq::log_buffer test_recovery_buffer(config);
                    std::vector<std::thread> task_thread_vector;
                    uint32_t msg_count_cpy = MESSAGE_PER_VERSION;
                    for (uint32_t thread_idx = 0; thread_idx < THREAD_COUNT; ++thread_idx) {
                        task_thread_vector.emplace_back([&test_recovery_buffer, &result, version, thread_idx, msg_count_cpy]() {
                            std::random_device sd;
                            std::minstd_rand linear_ran(sd());
                            std::uniform_int_distribution<int32_t> rand_seq((int32_t)(3 * sizeof(uint32_t)), 1024);
                            for (uint32_t i = 0; i < msg_count_cpy; ++i) {
                                uint32_t alloc_size = (uint32_t)rand_seq(linear_ran);
                                auto handle = test_recovery_buffer.alloc_write_chunk(alloc_size, bq::platform::high_performance_epoch_ms());
                                result.add_result(handle.result == enum_buffer_result_code::success, "recovery test write alloc, size:%" PRIu32 "", alloc_size);
                                bq::scoped_log_buffer_handle<log_buffer> scoped_handle(test_recovery_buffer, handle);
                                for (size_t pos = 0; pos + 3 * sizeof(uint32_t) <= alloc_size; pos += 3 * sizeof(uint32_t)) {
                                    uint32_t* ptr = reinterpret_cast<uint32_t*>(handle.data_addr + pos);
                                    ptr[0] = version;
                                    ptr[1] = thread_idx;
                                    ptr[2] = i;
                                }
                            }
                        });
                    }

                    std::random_device sd;
                    std::minstd_rand linear_ran(sd());
                    std::uniform_int_distribution<uint32_t> rand_seq(0, MESSAGE_PER_VERSION);
                    uint32_t total_read_count = (uint32_t)rand_seq(linear_ran);
                    for (uint32_t i = 0; i < total_read_count; ++i) {
                        auto handle = test_recovery_buffer.read_chunk();
                        bq::scoped_log_buffer_handle<log_buffer> scoped_handle(test_recovery_buffer, handle);
                        if (handle.result == enum_buffer_result_code::err_empty_log_buffer) {
                            --i;
                            continue;
                        }
                        result.add_result(handle.result == enum_buffer_result_code::success, "recovery multi thread test read chunk, version:%" PRIu32 "index:%" PRIu32 "", version, i);
                        if (handle.result == enum_buffer_result_code::success) {
                            ++read_message_count;
                            uint32_t read_version = ((uint32_t*)(handle.data_addr))[0];
                            uint32_t read_thread_idx = ((uint32_t*)(handle.data_addr))[1];
                            uint32_t read_message_idx = ((uint32_t*)(handle.data_addr))[2];
                            bool valid = true;
                            for (size_t pos = 0; pos + 3 * sizeof(uint32_t) <= handle.data_size; pos += 3 * sizeof(uint32_t)) {
                                uint32_t* ptr = reinterpret_cast<uint32_t*>(handle.data_addr + pos);
                                if (ptr[0] != read_version || ptr[1] != read_thread_idx || ptr[2] != read_message_idx) {
                                    valid = false;
                                    break;
                                }
                            }
                            result.add_result(valid, "recovery multi thread test read content check, version:%" PRIu32 "thread idx:%" PRIu32 "index:%" PRIu32 "", read_version, read_thread_idx, read_message_idx);
                            if (valid) {
                                result.add_result(message_verify_group[read_version][read_thread_idx] == read_message_idx, "recovery multi thread test read content seq check, version:%" PRIu32 "thread idx:%" PRIu32 "index:%" PRIu32 "", read_version, read_thread_idx, read_message_idx);
                                ++message_verify_group[read_version][read_thread_idx];
                            }
                        }
                    }
                    for (auto& task : task_thread_vector) {
                        task.join();
                    }
                }

                {
                    log_buffer_config config;
                    config.log_name = "log_buffer_recovery_test";
                    config.log_categories_name = { "_default" };
                    config.need_recovery = true;
                    config.policy = log_memory_policy::auto_expand_when_full;
                    config.high_frequency_threshold_per_second = 1000;
                    bq::log_buffer test_recovery_buffer(config);
                    bool first_message = true;
                    for (; read_message_count < total_message_count; ) {
                        auto handle = test_recovery_buffer.read_chunk();
                        bq::scoped_log_buffer_handle<log_buffer> scoped_handle(test_recovery_buffer, handle);
                        if (first_message) {
                            if (handle.result == enum_buffer_result_code::err_empty_log_buffer) {
                                continue;
                            }
                        }
                        ++read_message_count;
                        first_message = false;
                        result.add_result(handle.result == enum_buffer_result_code::success, "recovery multi thread test read chunk for left messages idx:%" PRIu32 "", read_message_count);
                        if (handle.result == enum_buffer_result_code::success) {
                            uint32_t read_version = ((uint32_t*)(handle.data_addr))[0];
                            uint32_t read_thread_idx = ((uint32_t*)(handle.data_addr))[1];
                            uint32_t read_message_idx = ((uint32_t*)(handle.data_addr))[2];
                            bool valid = true;
                            for (size_t pos = 0; pos + 3 * sizeof(uint32_t) <= handle.data_size; pos += 3 * sizeof(uint32_t)) {
                                uint32_t* ptr = reinterpret_cast<uint32_t*>(handle.data_addr + pos);
                                if (ptr[0] != read_version || ptr[1] != read_thread_idx || ptr[2] != read_message_idx) {
                                    valid = false;
                                    break;
                                }
                            }
                            result.add_result(valid, "recovery multi thread test read content check, version:%" PRIu32 "thread idx:%" PRIu32 "index:%" PRIu32 "", read_version, read_thread_idx, read_message_idx);
                            if (valid) {
                                result.add_result(message_verify_group[read_version][read_thread_idx] == read_message_idx, "recovery multi thread test read content seq check, version:%" PRIu32 "thread idx:%" PRIu32 "index:%" PRIu32 "", read_version, read_thread_idx, read_message_idx);
                                ++message_verify_group[read_version][read_thread_idx];
                            }
                        }
                    }


                    auto handle = test_recovery_buffer.read_chunk();
                    result.add_result(handle.result == enum_buffer_result_code::err_empty_log_buffer, "recovery multi thread test read content final for single thread");
                }
                
                test_output_dynamic(bq::log_level::info, "[log buffer] do recovery test end...\n");
            }
        public:
            virtual test_result test() override
            {
                test_result result;
                do_linked_list_test(result);
                do_memory_pool_test(result);

                log_buffer_config config;
                config.log_name = "log_buffer_test";
                config.log_categories_name = { "_default" };
                config.need_recovery = false;
                config.policy = log_memory_policy::auto_expand_when_full;
                config.high_frequency_threshold_per_second = 1000;

                do_block_list_test(result, config);
                config.need_recovery = true;
                do_block_list_test(result, config);

                do_basic_test(result, config);
                config.policy = log_memory_policy::block_when_full;
                do_basic_test(result, config);
                config.need_recovery = false;
                do_basic_test(result, config);
                config.policy = log_memory_policy::auto_expand_when_full;
                do_basic_test(result, config);

                config.high_frequency_threshold_per_second = UINT64_MAX;
                do_basic_test(result, config);
                config.policy = log_memory_policy::block_when_full;
                do_basic_test(result, config);
                config.need_recovery = true;
                do_basic_test(result, config);
                config.policy = log_memory_policy::auto_expand_when_full;
                do_basic_test(result, config);

                do_recovery_test(result);
                return result;
            }
        };
    }
}
