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
#include "bq_log/types/buffer/normal/miso_ring_buffer.h"
#include "bq_log/types/buffer/siso_ring_buffer.h"

namespace bq {
    namespace test {
        static bq::platform::atomic<int32_t> ring_buffer_test_total_write_count_ = 0;
        static bq::platform::atomic<int32_t> ring_buffer_test_total_read_count_ = 0;
        constexpr int32_t total_task = 13;

        class miso_write_task {
        private:
            int32_t id;
            bq::miso_ring_buffer* ring_buffer_ptr;
            int32_t left_write_count;
            bq::platform::atomic<int32_t>& counter_ref;

        public:
            const static int32_t min_chunk_size = 12;
            const static int32_t max_chunk_size = 1024;
            miso_write_task(int32_t id, int32_t left_write_count, bq::miso_ring_buffer* ring_buffer_ptr, bq::platform::atomic<int32_t>& counter)
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
                    ++ring_buffer_test_total_write_count_;
                }
                counter_ref.fetch_add(-1, bq::platform::memory_order::release);
            }
        };

        class siso_write_task {
        private:
            int32_t left_write_count_;
            bq::siso_ring_buffer* ring_buffer_ptr_;
        public:
            siso_write_task(int32_t left_write_count, bq::siso_ring_buffer* ring_buffer_ptr)
            {
                this->left_write_count_ = left_write_count;
                this->ring_buffer_ptr_ = ring_buffer_ptr;
            }

            void operator()()
            {
                while (left_write_count_ > 0) {
                    uint32_t size = (uint32_t)(left_write_count_ % (8 * 1024));
                    size = bq::max_value((uint32_t)1, size);
                    auto handle = ring_buffer_ptr_->alloc_write_chunk(size);
                    if (handle.result != enum_buffer_result_code::success) {
                        bq::platform::thread::yield();
                        continue;
                    }
                    int32_t write_index = 0;
                    for (; (size_t)write_index + sizeof(int32_t) <= (size_t)size; write_index += (int32_t)sizeof(int32_t)) {
                        *((int32_t*)(handle.data_addr + write_index)) = left_write_count_;
                    }
                    assert(write_index <= (int32_t)size);
                    if ((size_t)size > (size_t)write_index) {
                        memcpy(handle.data_addr + write_index, &left_write_count_, (size_t)size - (size_t)write_index);
                    }
                    ring_buffer_ptr_->commit_write_chunk(handle);
                    ++ring_buffer_test_total_write_count_;
                    --left_write_count_;
                }
            }
        };

        class siso_read_task {
        private:
            int32_t left_read_count_;
            bq::siso_ring_buffer* ring_buffer_ptr_;
            test_result* test_result_ptr_;
        public:
            siso_read_task(int32_t left_read_count, bq::siso_ring_buffer* ring_buffer_ptr, test_result& result)
            {
                this->left_read_count_ = left_read_count;
                this->ring_buffer_ptr_ = ring_buffer_ptr;
                this->test_result_ptr_ = &result;
            }

            void operator()()
            {
                while (left_read_count_ > 0) {
                    ring_buffer_ptr_->begin_read();
                    while (true) {
                        auto handle = ring_buffer_ptr_->read();
                        if (handle.result != enum_buffer_result_code::success) {
                            bq::platform::thread::yield();
                            break;
                        }
                        uint32_t expected_size = (uint32_t)(left_read_count_ % (8 * 1024));
                        expected_size = bq::max_value((uint32_t)1, expected_size);
                        test_result_ptr_->add_result(handle.data_size == expected_size, "[siso ring buffer %s] data size error ,expected:%d, read:%d", ring_buffer_ptr_->get_is_memory_mapped() ? "with mmap" : "without mmap", expected_size, (int32_t)handle.data_size);
                        bool data_match = true;
                        int32_t read_index = 0;
                        for (; (size_t)read_index + sizeof(int32_t) <= (size_t)handle.data_size; read_index += (int32_t)sizeof(int32_t)) {
                            if (*((int32_t*)(handle.data_addr + read_index)) != left_read_count_) {
                                data_match = false;
                                break;
                            }
                        }
                        assert(read_index <= (int32_t)handle.data_size);
                        if (data_match && (size_t)handle.data_size > (size_t)read_index) {
                            if (0 != memcmp(handle.data_addr + read_index, &left_read_count_, (size_t)handle.data_size - (size_t)read_index)) {
                                data_match = false;
                            }
                        }
                        test_result_ptr_->add_result(data_match, "[siso ring buffer %s] data mismatch for num:%d", ring_buffer_ptr_->get_is_memory_mapped() ? "with mmap" : "without mmap", left_read_count_);     
                        --left_read_count_;
                        if (left_read_count_ == 0) {
                            bq::platform::thread::sleep(1000);
                            handle = ring_buffer_ptr_->read();
                            test_result_ptr_->add_result(handle.result == enum_buffer_result_code::err_empty_ring_buffer, "[siso ring buffer %s] chunk count mismatch", ring_buffer_ptr_->get_is_memory_mapped() ? "with mmap" : "without mmap");
                        }
                        ++ring_buffer_test_total_read_count_;
                    }
                    ring_buffer_ptr_->end_read();
                }
                test_result_ptr_->add_result(left_read_count_ == 0, "[siso ring buffer %s]total block count mismatch", ring_buffer_ptr_->get_is_memory_mapped() ? "with mmap" : "without mmap");
            }
        };



        class test_ring_buffer : public test_base {
        private:
            void do_miso_test(test_result& result, bool with_mmap)
            {
                ring_buffer_test_total_write_count_.store(0);
                uint64_t serialize_id = with_mmap ? 4323245235 : 0;
                bq::miso_ring_buffer ring_buffer(1000 * 40, serialize_id);
                int32_t chunk_count_per_task = 1024000;
                bq::platform::atomic<int32_t> counter(total_task);
                std::vector<int32_t> task_check_vector;
                for (int32_t i = 0; i < total_task; ++i) {
                    task_check_vector.push_back(0);
                    miso_write_task task(i, chunk_count_per_task, &ring_buffer, counter);
                    std::thread task_thread(task);
                    task_thread.detach();
                }

                int32_t total_chunk = chunk_count_per_task * total_task;
                int32_t readed_chunk = 0;
                int32_t percent = 0;
                auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

                test_output_dynamic_param(bq::log_level::info, "[miso ring buffer] test %s\n", serialize_id != 0 ? "with mmap" : "without mmap");
                test_output_dynamic_param(bq::log_level::info, "[miso ring buffer] test progress:%d%%, time cost:%dms\r", percent, 0);
                ring_buffer.begin_read();
                while (true) {
                    bool write_finished = (counter.load(bq::platform::memory_order::acquire) <= 0);
                    auto handle = ring_buffer.read();
                    bool read_empty = handle.result == bq::enum_buffer_result_code::err_empty_ring_buffer;
                    if (write_finished && read_empty) {
                        break;
                    }
                    if (handle.result != bq::enum_buffer_result_code::success) {
                        ring_buffer.end_read();
                        ring_buffer.begin_read();
                        continue;
                    }
                    int32_t size = (int32_t)handle.data_size;
                    if (size < miso_write_task::min_chunk_size || size > miso_write_task::max_chunk_size) {
                        result.add_result(false, "ring buffer chunk size error");
                        continue;
                    }
                    ++readed_chunk;
                    int32_t new_percent = readed_chunk * 100 / total_chunk;
                    if (new_percent != percent) {
                        percent = new_percent;
                        auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        test_output_dynamic_param(bq::log_level::info, "[miso ring buffer] test progress:%d%%, time cost:%dms              \r", percent, (int32_t)(current_time - start_time));
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
                    result.add_result(task_check_vector[id] + left_count == chunk_count_per_task, "[miso ring buffer]chunk left task check error, real: %d, expected:%d", left_count, chunk_count_per_task - task_check_vector[id]);
                    task_check_vector[id] = chunk_count_per_task - left_count; // error adjust
                    bool content_check = true;
                    for (size_t i = 2; i < size / sizeof(int32_t); ++i) {
                        if (*((int32_t*)handle.data_addr + i) != size) {
                            content_check = false;
                            continue;
                        }
                    }
                    result.add_result(content_check, "[miso ring buffer]content check error");
                }
                ring_buffer.end_read();
                for (size_t i = 0; i < task_check_vector.size(); ++i) {
                    result.add_result(task_check_vector[i] == chunk_count_per_task, "[miso ring buffer]chunk count check error, real:%d , expected:%d", task_check_vector[i], chunk_count_per_task);
                }
                test_output_dynamic_param(bq::log_level::info, "\n[miso ring buffer] test %s finished\n", serialize_id != 0 ? "with mmap" : "without mmap");
                result.add_result(total_chunk == ring_buffer_test_total_write_count_.load(), "%s total write count error, real:%d , expected:%d", serialize_id != 0 ? "with mmap" : "without mmap", ring_buffer_test_total_write_count_.load(), total_chunk);
                result.add_result(total_chunk == readed_chunk, "[miso ring buffer] %s total chunk count check error, read:%d , expected:%d", serialize_id != 0 ? "with mmap" : "without mmap", readed_chunk, total_chunk);
            }

            void do_siso_test(test_result& result, bool with_mmap)
            {
                ring_buffer_test_total_write_count_.store(0);
                ring_buffer_test_total_read_count_.store(0);
                size_t buffer_size = 1024 * 64 + 64; // 1024 blocks and 64 bytes for mmap head, 64 redundant bytes for alignment.
                constexpr size_t task_size = 5;
                bq::file_handle mmap_file_handles[task_size];
                bq::memory_map_handle mmap_handles[task_size];
                uint8_t* buffers[task_size] = {nullptr};
                bq::siso_ring_buffer* ring_buffers[task_size] = { nullptr };

                test_output_dynamic_param(bq::log_level::info, "[siso ring buffer] test %s\n", with_mmap ? "with mmap" : "without mmap");
                test_output_dynamic_param(bq::log_level::info, "[siso ring buffer] test progress:%d%%, time cost:%dms\r", 0, 0);
                int32_t chunk_count_per_task = 1024000;
                for (size_t i = 0; i < task_size; ++i) {
                    if (with_mmap) {
                        char mmap_file_name[128];
                        snprintf(mmap_file_name, sizeof(mmap_file_name), "test_siso_%zu.mmap", i);
                        mmap_file_handles[i] = bq::file_manager::instance().open_file(TO_ABSOLUTE_PATH(mmap_file_name, false), bq::file_open_mode_enum::auto_create | bq::file_open_mode_enum::read_write | bq::file_open_mode_enum::exclusive);
                        if (!mmap_file_handles[i]) {
                            result.add_result(false, "[siso_test] failed to create mmap file");
                            return;
                        }
                        mmap_handles[i] = bq::memory_map::create_memory_map(mmap_file_handles[i], 0, buffer_size);
                        if (!mmap_handles[i].has_been_mapped() || mmap_handles[i].get_mapped_size() != buffer_size) {
                            result.add_result(false, "[siso_test] failed to create mmap");
                            return;
                        }
                        buffers[i] = (uint8_t*)mmap_handles[i].get_mapped_data();
                    } else {
                        buffers[i] = (uint8_t*)malloc(buffer_size);
                    }
                    ring_buffers[i] = new bq::siso_ring_buffer(buffers[i], buffer_size, with_mmap);
                    siso_write_task write_task(chunk_count_per_task, ring_buffers[i]);
                    std::thread write_task_thread(write_task);
                    write_task_thread.detach();

                    siso_read_task read_task(chunk_count_per_task, ring_buffers[i], result);
                    std::thread read_task_thread(read_task);
                    read_task_thread.detach();
                }

                int32_t total_chunk = chunk_count_per_task * (int32_t)task_size;
                int32_t last_read_count = 0;
                auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                while (true) {
                    int32_t current_read_count = ring_buffer_test_total_read_count_.load();
                    int32_t prev_percent = last_read_count * 100 / total_chunk;
                    int32_t new_percent = current_read_count * 100 / total_chunk;
                    if (prev_percent != new_percent) {
                        auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        test_output_dynamic_param(bq::log_level::info, "siso ring buffer test progress:%d%%, time cost:%dms              \r", new_percent, (int32_t)(current_time - start_time));
                        last_read_count = current_read_count;
                    }
                    if (current_read_count == total_chunk) {
                        break;
                    }
                }
                test_output_dynamic_param(bq::log_level::info, "\n[siso ring buffer] test %s finished\n", with_mmap ? "with mmap" : "without mmap");
                
                for (size_t i = 0; i < task_size; ++i) {
                    delete ring_buffers[i];
                    ring_buffers[i] = nullptr;
                    if (with_mmap) {
                        bq::memory_map::release_memory_map(mmap_handles[i]);
                    } else {
                        free(buffers[i]);
                    }
                }
            }
        public:
            virtual test_result test() override
            {
                test_result result;

                result.add_result(1U - 0xFFFFFFFFU == 2, "uint32 overflow test");
                result.add_result(1024U - 1U + 0xFFFFFFFFU == 1022U, "ring buffer left space uint32 overflow test1");
                result.add_result(1024U - 1025U + 1023 == 1022U, "ring buffer left space uint32 overflow test2");
                result.add_result((uint32_t)1U - (uint32_t)0xFFFFFFFFU == 2, "ring buffer left space uint32 overflow test3");

                // test without mmap
                do_siso_test(result, false);
                // test with mmap
                do_siso_test(result, true);

                // test without mmap
                do_miso_test(result, false);
                // test with mmap
                do_miso_test(result, true);
                return result;
            }
        };
    }
}
