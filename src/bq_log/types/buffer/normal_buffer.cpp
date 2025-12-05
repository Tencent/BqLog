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
/*!
 * \class bq::normal_buffer
 *
 * buffer for common use scenarios. It can backed by memory-mapped file or heap memory.
 *
 * \author pippocao
 * \date 2025/07/26
 */

#include "bq_log/types/buffer/normal_buffer.h"

namespace bq {
    normal_buffer::normal_buffer(size_t size, const bq::string& mmap_file_abs_path/* = ""*/, bool auto_create/* = true */ )
        : buffer_data_(nullptr)
        , buffer_size_(0)
        , delete_mmap_when_destruct_(true)
    {
        if (mmap_file_abs_path.is_empty()) {
            mmap_result_ = create_memory_map_result::failed;
        }
        else {
            mmap_result_ = create_memory_map(size, mmap_file_abs_path, auto_create);
        }
        if (mmap_result_ == create_memory_map_result::failed) {
            buffer_data_ = bq::platform::aligned_alloc(BQ_CACHE_LINE_SIZE, size);
            buffer_size_ = size;
        }
        else {
            buffer_data_ = memory_map_handle_.get_mapped_data();
            buffer_size_ = memory_map_handle_.get_mapped_size();
        }
    }

    normal_buffer::~normal_buffer()
    {
        if (memory_map_handle_.has_been_mapped()) {
            bq::memory_map::release_memory_map(memory_map_handle_);
            bq::string mmap_file_path = memory_map_file_.abs_file_path();
            bq::file_manager::instance().close_file(memory_map_file_);
            if (delete_mmap_when_destruct_) {
                bq::file_manager::remove_file_or_dir(mmap_file_path);
            }
        }
        else {
            bq::platform::aligned_free(buffer_data_);
        }
    }

    void normal_buffer::resize(size_t new_size)
    {
        if (new_size == size()) {
            return;
        }
        bool is_from_mmap = is_memory_mapped();
        void* back_updata = nullptr;
        auto prev_size = size();
        // Why do we have to back up first?
        // Reason 1: If it was from mmap before, and this time the new mmap application failed, reverting to ordinary heap memory, it is too late to back up the previous data.
        // Reason 2: We do not want the reliability of mmap backup data to depend on operating system behavior.
        if (is_from_mmap) {
            if (size() > 0) {
                back_updata = bq::platform::aligned_alloc(BQ_CACHE_LINE_SIZE, prev_size);
                memcpy(back_updata, data(), size());
            }
        }
        else {
            back_updata = data();
        }
        if(is_from_mmap){
            do {
                bq::memory_map::release_memory_map(memory_map_handle_);
                new_size = bq::memory_map::get_min_size_of_memory_map_file(0, new_size);
                if (!bq::file_manager::instance().truncate_file(memory_map_file_, new_size)) {
                    bq::util::log_device_console(log_level::warning, "normal buffer truncate memory map file \"%s\" failed, use memory instead.", memory_map_file_.abs_file_path().c_str());
                    bq::string mmap_file_path = memory_map_file_.abs_file_path();
                    bq::file_manager::instance().close_file(memory_map_file_);
                    bq::file_manager::remove_file_or_dir(mmap_file_path);
                    mmap_result_ = create_memory_map_result::failed;
                    break;
                }
                memory_map_handle_ = bq::memory_map::create_memory_map(memory_map_file_, 0, new_size);
                if (!memory_map_handle_.has_been_mapped()) {
                    bq::util::log_device_console(log_level::warning, "ring buffer create memory map failed from file \"%s\" failed, use memory instead.", memory_map_file_.abs_file_path().c_str());
                    bq::string mmap_file_path = memory_map_file_.abs_file_path();
                    bq::file_manager::instance().close_file(memory_map_file_);
                    bq::file_manager::remove_file_or_dir(mmap_file_path);
                    mmap_result_ = create_memory_map_result::failed;
                }
                buffer_data_ = memory_map_handle_.get_mapped_data();
                buffer_size_ = memory_map_handle_.get_mapped_size();
            }while(false);
        }
        if (!is_memory_mapped()) {
            buffer_data_ = bq::platform::aligned_alloc(BQ_CACHE_LINE_SIZE, new_size);
            buffer_size_ = new_size;
        }
        if (back_updata != nullptr) {
            memcpy(buffer_data_, back_updata, bq::min_value(prev_size, buffer_size_));
            bq::platform::aligned_free(back_updata);
        }
    }

    create_memory_map_result normal_buffer::create_memory_map(size_t  size, const bq::string& mmap_file_abs_path, bool auto_create)
    {
        if (!bq::memory_map::is_platform_support() || mmap_file_abs_path.is_empty()) {
            return create_memory_map_result::failed;
        }
        if (!auto_create) {
            if (!bq::file_manager::is_file(mmap_file_abs_path)) {
                return create_memory_map_result::failed;
            }
        }
        auto create_opt = auto_create ? (file_open_mode_enum::auto_create | file_open_mode_enum::read_write | file_open_mode_enum::exclusive) : (file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
        memory_map_file_ = bq::file_manager::instance().open_file(mmap_file_abs_path, create_opt);
        if (!memory_map_file_.is_valid()) {
            bq::util::log_device_console(bq::log_level::warning, "failed to open mmap file %s, use memory instead of mmap file, error code:%d", mmap_file_abs_path.c_str(), bq::file_manager::get_and_clear_last_file_error());
            return create_memory_map_result::failed;
        }
        size_t file_size = bq::memory_map::get_min_size_of_memory_map_file(0, size);
        create_memory_map_result result = create_memory_map_result::failed;
        size_t current_file_size = bq::file_manager::instance().get_file_size(memory_map_file_);
        if (current_file_size != file_size) {
            if (!bq::file_manager::instance().truncate_file(memory_map_file_, file_size)) {
                bq::util::log_device_console(log_level::warning, "normal buffer truncate memory map file \"%s\" failed, use memory instead.", memory_map_file_.abs_file_path().c_str());
                bq::file_manager::instance().close_file(memory_map_file_);
                bq::file_manager::remove_file_or_dir(mmap_file_abs_path);
                return create_memory_map_result::failed;
            }
            result = create_memory_map_result::new_created;
        } else {
            result = create_memory_map_result::use_existed;
        }

        memory_map_handle_ = bq::memory_map::create_memory_map(memory_map_file_, 0, file_size);
        if (!memory_map_handle_.has_been_mapped()) {
            bq::util::log_device_console(log_level::warning, "ring buffer create memory map failed from file \"%s\" failed, use memory instead.", memory_map_file_.abs_file_path().c_str());
            bq::file_manager::instance().close_file(memory_map_file_);
            bq::file_manager::remove_file_or_dir(mmap_file_abs_path);
            return create_memory_map_result::failed;
        }

        if (((uintptr_t)memory_map_handle_.get_mapped_data() & (BQ_CACHE_LINE_SIZE - 1)) != 0) {
            bq::util::log_device_console(log_level::warning, "normal buffer memory map file \"%s\" memory address is not aligned, use memory instead.", memory_map_file_.abs_file_path().c_str());
            bq::memory_map::release_memory_map(memory_map_handle_);
            bq::file_manager::instance().close_file(memory_map_file_);
            bq::file_manager::remove_file_or_dir(mmap_file_abs_path);
            return create_memory_map_result::failed;
        }
        buffer_data_ = memory_map_handle_.get_mapped_data();
        return result;
    }
}
