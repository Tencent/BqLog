#pragma once
/*
 * Copyright (C) 2025 Tencent.
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
#include <chrono>
#include <thread>
#include "test_base.h"
#include "bq_log/types/buffer/miso_ring_buffer.h"

namespace bq {
    namespace test {
        static bq::platform::atomic<int32_t> miso_ring_buffer_test_total_write_count_ = 0;
        constexpr int32_t miso_total_task = 13;

        class miso_write_task {
        private:
            int32_t id_;
            bq::miso_ring_buffer* ring_buffer_ptr_;
            int32_t left_write_count_;
            bq::platform::atomic<bool>& mark_ref_;

        public:
            const static int32_t min_chunk_size = 12;
            const static int32_t max_chunk_size = 1024;
            miso_write_task(int32_t id, int32_t left_write_count, bq::miso_ring_buffer* ring_buffer_ptr, bq::platform::atomic<bool>& mark_ref)
                : mark_ref_(mark_ref)
            {
                this->id_ = id;
                this->left_write_count_ = left_write_count;
                this->ring_buffer_ptr_ = ring_buffer_ptr;
            }

            void operator()()
            {
                uint32_t sleep_strategy = 0;
                std::random_device sd;
                std::minstd_rand linear_ran(sd());
                std::uniform_int_distribution<int32_t> rand_seq(min_chunk_size, max_chunk_size);
                while (left_write_count_ > 0) {
                    uint32_t alloc_size = (uint32_t)rand_seq(linear_ran);
                    auto handle = ring_buffer_ptr_->alloc_write_chunk(alloc_size);
                    if (handle.result == bq::enum_buffer_result_code::err_not_enough_space
                        || handle.result == bq::enum_buffer_result_code::err_buffer_not_inited) {
                        if ((++sleep_strategy) % 1024 != 0) {
                            bq::platform::thread::yield();
                        }else {
                            std::this_thread::sleep_for(std::chrono::microseconds(10));
                        }
                        continue;
                    }
                    --left_write_count_;
                    assert(handle.result == bq::enum_buffer_result_code::success);
                    *(int32_t*)(handle.data_addr) = id_;
                    *((int32_t*)(handle.data_addr) + 1) = left_write_count_;
                    int32_t count = (int32_t)alloc_size / static_cast<int32_t>(sizeof(int32_t));
                    int32_t* begin = (int32_t*)(handle.data_addr) + 2;
                    int32_t* end = (int32_t*)(handle.data_addr) + count;
                    std::fill(begin, end, (int32_t)alloc_size);
                    ring_buffer_ptr_->commit_write_chunk(handle);
                    ++miso_ring_buffer_test_total_write_count_;
                }
                mark_ref_.store_release(true);
            }
        };

        class test_miso_ring_buffer : public test_base {
        private:
            void do_miso_test(test_result& result, bool with_mmap)
            {
                uint32_t sleep_strategy = 0;
                miso_ring_buffer_test_total_write_count_.store_seq_cst(0);
                log_buffer_config config;
                config.log_name = "test_miso_log_buffer";
                config.log_categories_name = { "_default", "category1" };
                config.need_recovery = with_mmap;
                bq::miso_ring_buffer ring_buffer(config);
#ifdef BQ_UNITE_TEST_LOW_PERFORMANCE_MODE
                constexpr int32_t chunk_count_per_task = 1024000 / 8;
#else
                constexpr int32_t chunk_count_per_task = 1024000;
#endif
                bq::platform::atomic<bool> write_finish_marks_array[miso_total_task];
                bq::array<int32_t> task_check_vector;
                for (int32_t i = 0; i < miso_total_task; ++i) {
                    task_check_vector.push_back(0);
                    write_finish_marks_array[i].store_seq_cst(false);
                    miso_write_task task(i, chunk_count_per_task, &ring_buffer, write_finish_marks_array[i]);
                    std::thread task_thread(task);
                    task_thread.detach();
                }

                int32_t total_chunk = chunk_count_per_task * miso_total_task;
                int32_t readed_chunk = 0;
                int32_t percent = 0;
                auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

                test_output_dynamic_param(bq::log_level::info, "[miso ring buffer] test %s\n", with_mmap ? "with mmap" : "without mmap");
                test_output_dynamic_param(bq::log_level::info, "[miso ring buffer] test progress:%d%%, time cost:%dms\r", percent, 0);
                while (true) {
                    bool write_finished = true;
                    for (auto i = 0; i < miso_total_task; ++i) {
                        if (write_finish_marks_array[i].load_acquire() == false) {
                            write_finished = false;
                            break;
                        }
                    }
                    auto handle = ring_buffer.read_chunk();
                    bq::scoped_log_buffer_handle<miso_ring_buffer> read_handle(ring_buffer, handle);
                    bool read_empty = handle.result == bq::enum_buffer_result_code::err_empty_log_buffer;
                    if (write_finished && read_empty) {
                        break;
                    }
                    if (handle.result != bq::enum_buffer_result_code::success) {
                        if ((++sleep_strategy) % 256 != 0) {
                            bq::platform::thread::yield();
                        }else {
                            std::this_thread::sleep_for(std::chrono::microseconds(10));
                        }
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
                    if (id < 0 || id >= miso_total_task) {
                        std::string error_msg = "invalid task id:";
                        error_msg += std::to_string(id);
                        result.add_result(false, error_msg.c_str());
                        continue;
                    }

                    task_check_vector[id]++;
                    result.add_result(task_check_vector[id] + left_count == chunk_count_per_task, "[miso ring buffer]chunk left task check error, real: %d, expected:%d", left_count, chunk_count_per_task - task_check_vector[id]);
                    task_check_vector[id] = chunk_count_per_task - left_count; // error adjust
                    bool content_check = true;
                    for (size_t i = 2; i < static_cast<size_t>(size) / sizeof(int32_t); ++i) {
                        if (*(reinterpret_cast<int32_t*>(handle.data_addr) + i) != size) {
                            content_check = false;
                            continue;
                        }
                    }
                    result.add_result(content_check, "[miso ring buffer]content check error");
                }
                for (int i : task_check_vector) {
                    result.add_result(i == chunk_count_per_task, "[miso ring buffer]chunk count check error, real:%d , expected:%d", i, chunk_count_per_task);
                }
                test_output_dynamic_param(bq::log_level::info, "\n[miso ring buffer] test %s finished\n", with_mmap ? "with mmap" : "without mmap");
                result.add_result(total_chunk == miso_ring_buffer_test_total_write_count_.load(), "%s total write count error, real:%d , expected:%d", with_mmap ? "with mmap" : "without mmap", miso_ring_buffer_test_total_write_count_.load(), total_chunk);
                result.add_result(total_chunk == readed_chunk, "[miso ring buffer] %s total chunk count check error, read:%d , expected:%d", with_mmap ? "with mmap" : "without mmap", readed_chunk, total_chunk);
            }

            void do_traverse_test(test_result& result)
            {
                log_buffer_config config;
                config.log_name = "test_miso_log_buffer";
                config.log_categories_name = { "_default", "category1" };
                config.need_recovery = false;
                bq::miso_ring_buffer ring_buffer1(config);
                bq::miso_ring_buffer ring_buffer2(config);
                bq::array<uint32_t> data_src;
                constexpr uint32_t data_src_size = 4096;
                data_src.fill_uninitialized(4096 / sizeof(uint32_t));
                for (size_t i = 0; i < data_src.size(); ++i) {
                    data_src[i] = bq::util::rand();
                }
                while (true) {

                    uint32_t size = bq::util::rand() % data_src_size;
                    auto handle1 = ring_buffer1.alloc_write_chunk(size);
                    bq::scoped_log_buffer_handle<miso_ring_buffer> scoped1(ring_buffer1, handle1);
                    auto handle2 = ring_buffer2.alloc_write_chunk(size);
                    bq::scoped_log_buffer_handle<miso_ring_buffer> scoped2(ring_buffer2, handle2);
                    result.add_result(handle1.result == handle2.result, "miso traverse test, prepare data");
                    if (handle1.result != enum_buffer_result_code::success) {
                        break;
                    }
                    size_t start_pos = (size == data_src_size) ? 0 : bq::util::rand() % (uint32_t)(4096 - size);
                    memcpy(handle1.data_addr, (uint8_t*)(uint32_t*)data_src.begin() + start_pos, size);
                    memcpy(handle2.data_addr, (uint8_t*)(uint32_t*)data_src.begin() + start_pos, size);
                }
                auto user_data = bq::make_tuple(&ring_buffer1, &result);
                using user_data_type = decltype(user_data);
                ring_buffer2.data_traverse([](uint8_t* data, uint32_t size, void* user_data_input) {
                    // do nothing
                    user_data_type& user_data_ref = *(user_data_type*)user_data_input;
                    auto& ring_buffer1_ref = *bq::get<0>(user_data_ref);
                    auto& result_ref = *bq::get<1>(user_data_ref);
                    auto handle = ring_buffer1_ref.read_chunk();
                    bq::scoped_log_buffer_handle<miso_ring_buffer> scoped(ring_buffer1_ref, handle);
                    result_ref.add_result(handle.result == enum_buffer_result_code::success, "miso traverse test, read data");
                    result_ref.add_result(handle.data_size == size, "miso traverse test, read data size");
                    result_ref.add_result(memcmp(handle.data_addr, data, size) == 0, "miso traverse test, read data content");
                    return;
                },
                    &user_data);

                auto handle1 = ring_buffer1.read_chunk();
                result.add_result(handle1.result == enum_buffer_result_code::err_empty_log_buffer, "miso traverse test, final");
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
                do_miso_test(result, false);
                // test with mmap
                do_miso_test(result, true);
                do_traverse_test(result);
                return result;
            }
        };
    }
}
