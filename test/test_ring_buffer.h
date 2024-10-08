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
#include "bq_log/types/ring_buffer.h"

class write_task {
private:
    int32_t id;
    bq::ring_buffer* ring_buffer_ptr;
    int32_t left_write_count;
    bq::platform::atomic<int32_t>& counter_ref;

public:
    const static int32_t min_chunk_size = 12;
    const static int32_t max_chunk_size = 1024;
    write_task(int32_t id, int32_t left_write_count, bq::ring_buffer* ring_buffer_ptr, bq::platform::atomic<int32_t>& counter)
        : counter_ref(counter)
    {
        this->id = id;
        this->left_write_count = left_write_count;
        this->ring_buffer_ptr = ring_buffer_ptr;
    }

    void operator()()
    {
        std::random_device sd;
        std::minstd_rand linear_ran(sd());
        std::uniform_int_distribution<int32_t> rand_seq(min_chunk_size, max_chunk_size);
        while (left_write_count > 0) {
            uint32_t alloc_size = (uint32_t)rand_seq(linear_ran);
            auto handle = ring_buffer_ptr->alloc_write_chunk(alloc_size);
            if (handle.result == bq::enum_buffer_result_code::err_not_enough_space
                || handle.result == bq::enum_buffer_result_code::err_alloc_failed_by_race_condition
                || handle.result == bq::enum_buffer_result_code::err_buffer_not_inited) {
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
            ring_buffer_ptr->commit_write_chunk(handle);
        }
        counter_ref.fetch_add(-1, bq::platform::memory_order::release);
    }
};

namespace bq {
    namespace test {

        class test_ring_buffer : public test_base {
        public:
            virtual test_result test() override
            {
                test_result result;

                result.add_result(1U - 0xFFFFFFFFU == 2, "uint32 overflow test");
                result.add_result(1024U - 1U + 0xFFFFFFFFU == 1022U, "ring buffer left space uint32 overflow test1");
                result.add_result(1024U - 1025U + 1023 == 1022U, "ring buffer left space uint32 overflow test2");
                result.add_result((uint32_t)1U - (uint32_t)0xFFFFFFFFU == 2, "ring buffer left space uint32 overflow test3");

                bq::ring_buffer ring_buffer(1000 * 40);
                int32_t chunk_count_per_task = 1024000;
                int32_t total_task = 13;
                bq::platform::atomic<int32_t> counter(total_task);
                std::vector<int32_t> task_check_vector;
                for (int32_t i = 0; i < total_task; ++i) {
                    task_check_vector.push_back(0);
                    write_task task(i, chunk_count_per_task, &ring_buffer, counter);
                    std::thread task_thread(task);
                    task_thread.detach();
                }

                int32_t total_chunk = chunk_count_per_task * total_task;
                int32_t readed_chunk = 0;
                int32_t percent = 0;
                auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                test_output_dynamic_param(bq::log_level::info, "ring buffer test progress:%d%%, time cost:%dms\r", percent, 0);
                bool read_empty = false;
                ring_buffer.begin_read();
                while (counter.load(bq::platform::memory_order::acquire) > 0 || !read_empty) {
                    auto handle = ring_buffer.read();
                    read_empty = handle.result == bq::enum_buffer_result_code::err_empty_ring_buffer;
                    if (handle.result != bq::enum_buffer_result_code::success) {
                        ring_buffer.end_read();
                        ring_buffer.begin_read();
                        continue;
                    }
                    int32_t size = (int32_t)handle.data_size;
                    if (size < write_task::min_chunk_size || size > write_task::max_chunk_size) {
                        result.add_result(false, "ring buffer chunk size error");
                        continue;
                    }
                    ++readed_chunk;
                    int32_t new_percent = readed_chunk * 100 / total_chunk;
                    if (new_percent != percent) {
                        percent = new_percent;
                        auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        test_output_dynamic_param(bq::log_level::info, "ring buffer test progress:%d%%, time cost:%dms              \r", percent, (int32_t)(current_time - start_time));
                    }

                    int32_t id = *(int32_t*)handle.data_addr;
                    int32_t left_count = *((int32_t*)handle.data_addr + 1);
                    if (id < 0 || id >= total_task) {
                        std::string error_msg = "invalid task id:";
                        error_msg += std::to_string(id);
                        result.add_result(false, error_msg.c_str());
                        continue;
                    }

                    task_check_vector[id]++;
                    if (task_check_vector[id] + left_count != chunk_count_per_task) {
                        result.add_result(false, "chunk left task check error, real: %d, expected:%d", left_count, chunk_count_per_task - task_check_vector[id]);
                        task_check_vector[id] = chunk_count_per_task - left_count;
                    }
                    bool content_check = true;
                    for (size_t i = 2; i < size / sizeof(int32_t); ++i) {
                        if (*((int32_t*)handle.data_addr + i) != size) {
                            content_check = false;
                            continue;
                        }
                    }
                    result.add_result(content_check, "content check error");
                }
                ring_buffer.end_read();
                for (size_t i = 0; i < task_check_vector.size(); ++i) {
                    result.add_result(task_check_vector[i] == chunk_count_per_task, "chunk count check error, real:%d , expected:%d", task_check_vector[i], chunk_count_per_task);
                }
                result.add_result(total_chunk == readed_chunk, "total chunk count check error, read:%d , expected:%d", readed_chunk, total_chunk);
                return result;
            }
        };
    }
}
