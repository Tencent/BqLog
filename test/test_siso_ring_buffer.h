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
#include <thread>
#include "test_base.h"
#include "bq_log/types/buffer/siso_ring_buffer.h"

namespace bq {
    namespace test {
        static bq::platform::atomic<int32_t> siso_ring_buffer_test_total_write_count_ = 0;
        static bq::platform::atomic<int32_t> siso_ring_buffer_test_total_read_count_ = 0;
        static bq::platform::atomic<int32_t> siso_ring_buffer_test_alive_write_thread_count = 0;
        static bq::platform::atomic<int32_t> siso_ring_buffer_test_alive_read_thread_count = 0;

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
                    size = bq::min_value(size, (uint32_t)(ring_buffer_ptr_->get_block_size() * ring_buffer_ptr_->get_total_blocks_count()) >> 1);
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
                    ++siso_ring_buffer_test_total_write_count_;
                    --left_write_count_;
                }
                --siso_ring_buffer_test_alive_write_thread_count;
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
                    auto handle = ring_buffer_ptr_->read_chunk();
                    bq::scoped_log_buffer_handle<siso_ring_buffer> scoped_handle(*ring_buffer_ptr_, handle);
                    if (handle.result == enum_buffer_result_code::err_empty_log_buffer) {
                        bq::platform::thread::yield();
                        continue;
                    } else if (handle.result != enum_buffer_result_code::success) {
                        test_result_ptr_->add_result(false, "[siso ring buffer %s] error read return code:%d", ring_buffer_ptr_->get_is_memory_recovery() ? "with mmap" : "without mmap", (int32_t)handle.result);
                        break;
                    }
                    uint32_t expected_size = (uint32_t)(left_read_count_ % (8 * 1024));
                    expected_size = bq::max_value((uint32_t)1, expected_size);
                    expected_size = bq::min_value(expected_size, (uint32_t)(ring_buffer_ptr_->get_block_size() * ring_buffer_ptr_->get_total_blocks_count()) >> 1);
                    test_result_ptr_->add_result(handle.data_size == expected_size, "[siso ring buffer %s] data size error ,expected:%d, read:%d", ring_buffer_ptr_->get_is_memory_recovery() ? "with mmap" : "without mmap", expected_size, (int32_t)handle.data_size);
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
                    test_result_ptr_->add_result(data_match, "[siso ring buffer %s] data mismatch for num:%d", ring_buffer_ptr_->get_is_memory_recovery() ? "with mmap" : "without mmap", left_read_count_);
                    --left_read_count_;
                    ++siso_ring_buffer_test_total_read_count_;
                }
                auto new_handle = ring_buffer_ptr_->read_chunk();
                bq::scoped_log_buffer_handle<siso_ring_buffer> scoped_new_handle(*ring_buffer_ptr_, new_handle);
                test_result_ptr_->add_result(new_handle.result == enum_buffer_result_code::err_empty_log_buffer, "[siso ring buffer %s] chunk count mismatch, overflow 1", ring_buffer_ptr_->get_is_memory_recovery() ? "with mmap" : "without mmap");
                test_result_ptr_->add_result(left_read_count_ == 0, "[siso ring buffer %s]total block count mismatch", ring_buffer_ptr_->get_is_memory_recovery() ? "with mmap" : "without mmap");
                --siso_ring_buffer_test_alive_read_thread_count;
            }
        };

        class test_siso_ring_buffer : public test_base {
        private:
            void do_siso_test(test_result& result, bool with_mmap)
            {
                siso_ring_buffer_test_total_write_count_.store_seq_cst(0);
                siso_ring_buffer_test_total_read_count_.store_seq_cst(0);
                constexpr size_t task_size = 6;
                siso_ring_buffer_test_alive_write_thread_count.store_seq_cst((int32_t)task_size);
                siso_ring_buffer_test_alive_read_thread_count.store_seq_cst((int32_t)task_size);
                bq::file_handle mmap_file_handles[task_size];
                bq::memory_map_handle mmap_handles[task_size];
                uint8_t* buffers[task_size] = { nullptr };
                bq::siso_ring_buffer* ring_buffers[task_size] = { nullptr };

                test_output_dynamic_param(bq::log_level::info, "[siso ring buffer] test %s\n", with_mmap ? "with mmap" : "without mmap");
                test_output_dynamic_param(bq::log_level::info, "[siso ring buffer] test progress:%d%%, time cost:%dms\r", 0, 0);
                int32_t chunk_count_per_task = 1024000;
                for (size_t i = 0; i < task_size; ++i) {
                    size_t buffer_size = (size_t)(1024 * (4 << i) + 64 + 64); // 1024 blocks and 64 bytes for mmap head, 64 redundant bytes for alignment.
                    if (with_mmap) {
                        char mmap_file_name[128];
                        snprintf(mmap_file_name, sizeof(mmap_file_name), "test_siso_%" PRIu64 ".mmap", static_cast<uint64_t>(i));
                        mmap_file_handles[i] = bq::file_manager::instance().open_file(TO_ABSOLUTE_PATH(mmap_file_name, 1), bq::file_open_mode_enum::auto_create | bq::file_open_mode_enum::read_write | bq::file_open_mode_enum::exclusive);
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
                        buffers[i] = (uint8_t*)bq::platform::aligned_alloc(CACHE_LINE_SIZE, buffer_size);
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
                    bool all_finished = siso_ring_buffer_test_alive_read_thread_count.load() == 0
                        && siso_ring_buffer_test_alive_write_thread_count.load() == 0;

                    int32_t current_read_count = siso_ring_buffer_test_total_read_count_.load();
                    int32_t prev_percent = last_read_count * 100 / total_chunk;
                    int32_t new_percent = current_read_count * 100 / total_chunk;
                    if (prev_percent != new_percent) {
                        auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        test_output_dynamic_param(bq::log_level::info, "siso ring buffer test progress:%d%%, time cost:%dms              \r", new_percent, (int32_t)(current_time - start_time));
                        last_read_count = current_read_count;
                    }
                    if (all_finished) {
                        result.add_result(current_read_count == total_chunk, "[siso_test] read write mismatch, read_count :%d, total_chunk:%d", current_read_count, total_chunk);
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
                        bq::platform::aligned_free(buffers[i]);
                    }
                }
            }

            void do_traverse_test(test_result& result)
            {
                auto buffer_size = siso_ring_buffer::calculate_min_size_of_memory(64 * 1024);
                uint8_t* buffer_data1 = (uint8_t*)bq::platform::aligned_alloc(CACHE_LINE_SIZE, buffer_size);
                uint8_t* buffer_data2 = (uint8_t*)bq::platform::aligned_alloc(CACHE_LINE_SIZE, buffer_size);
                bq::siso_ring_buffer ring_buffer1(buffer_data1, buffer_size, false);
                bq::siso_ring_buffer ring_buffer2(buffer_data2, buffer_size, false);
                bq::array<uint32_t> data_src;
                constexpr uint32_t data_src_size = 4096;
                data_src.fill_uninitialized(4096 / sizeof(uint32_t));
                for (size_t i = 0; i < data_src.size(); ++i) {
                    data_src[i] = bq::util::rand();
                }
                while (true) {
                    uint32_t size = bq::util::rand() % data_src_size;
                    auto handle1 = ring_buffer1.alloc_write_chunk(size);
                    bq::scoped_log_buffer_handle<siso_ring_buffer> scoped1(ring_buffer1, handle1);
                    auto handle2 = ring_buffer2.alloc_write_chunk(size);
                    bq::scoped_log_buffer_handle<siso_ring_buffer> scoped2(ring_buffer2, handle2);
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
                    bq::scoped_log_buffer_handle<siso_ring_buffer> scoped(ring_buffer1_ref, handle);
                    result_ref.add_result(handle.result == enum_buffer_result_code::success, "miso traverse test, read data");
                    result_ref.add_result(handle.data_size == size, "miso traverse test, read data size");
                    result_ref.add_result(memcmp(handle.data_addr, data, size) == 0, "miso traverse test, read data content");
                    return;
                },
                    &user_data);
                auto handle1 = ring_buffer1.read_chunk();
                result.add_result(handle1.result == enum_buffer_result_code::err_empty_log_buffer, "miso traverse test, final");
                bq::platform::aligned_free(buffer_data1);
                bq::platform::aligned_free(buffer_data2);
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
                if (bq::memory_map::is_platform_support()) {
                    do_siso_test(result, true);
                }

                do_traverse_test(result);
                return result;
            }
        };
    }
}
