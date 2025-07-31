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
/*!
 * \class bq::normal_buffer
 *
 * buffer for common use scenarios, for example where data chunks are larger than
 * default buffer size of miso_buffer or siso_buffer.
 *
 * \author pippocao
 * \date 2025/07/26
 */
 
 #include "bq_log/types/buffer/normal_buffer.h"

 namespace bq {
    normal_buffer::normal_buffer(uint32_t size, bool need_recovery, const bq::string& mmap_file_abs_path)
         : required_size_(size)
         , need_recovery_(need_recovery)
         , mmap_file_abs_path_(mmap_file_abs_path)
    {
        init_buffer_data();
    }

    normal_buffer::~normal_buffer()
    {
        bq::object_destructor<bq::siso_ring_buffer>::destruct(&get_buffer());
        if (!uninit_memory_map()) {
            bq::platform::aligned_free(buffer_data_);
        }
    }

    uint32_t normal_buffer::calculate_size_of_memory(uint32_t required_size)
    {
        return bq::siso_ring_buffer::calculate_min_size_of_memory(required_size) + static_cast<uint32_t>(HEAD_SIZE);
    }

    void normal_buffer::init_buffer_data()
    {
        auto mmap_result = create_memory_map();
        switch (mmap_result) {
        case bq::create_memory_map_result::failed:
            buffer_data_ = (uint8_t*)bq::platform::aligned_alloc(CACHE_LINE_SIZE, calculate_size_of_memory(required_size_));
            bq::object_constructor<bq::siso_ring_buffer>::construct(reinterpret_cast<bq::siso_ring_buffer*>(siso_buffer_obj_), buffer_data_ + HEAD_SIZE, calculate_size_of_memory(required_size_) - static_cast<uint32_t>(HEAD_SIZE), false);
            init_with_memory();
            break;
        case bq::create_memory_map_result::use_existed:
            bq::object_constructor<bq::siso_ring_buffer>::construct(reinterpret_cast<bq::siso_ring_buffer*>(siso_buffer_obj_), buffer_data_ + HEAD_SIZE, calculate_size_of_memory(required_size_) - static_cast<uint32_t>(HEAD_SIZE), true);
            if (!try_recover_from_exist_memory_map()) {
                init_with_memory_map();
            }
            break;
        case bq::create_memory_map_result::new_created:
            bq::object_constructor<bq::siso_ring_buffer>::construct(reinterpret_cast<bq::siso_ring_buffer*>(siso_buffer_obj_), buffer_data_ + HEAD_SIZE, calculate_size_of_memory(required_size_) - static_cast<uint32_t>(HEAD_SIZE), true);
            init_with_memory_map();
            break;
        default:
            break;
        }
    }

    create_memory_map_result normal_buffer::create_memory_map()
    {
        if (!bq::memory_map::is_platform_support() || !need_recovery_) {
            return create_memory_map_result::failed;
        }
        memory_map_file_ = bq::file_manager::instance().open_file(mmap_file_abs_path_, file_open_mode_enum::auto_create | file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
        if (!memory_map_file_.is_valid()) {
            bq::util::log_device_console(bq::log_level::warning, "failed to open mmap file %s, use memory instead of mmap file, error code:%d", mmap_file_abs_path_.c_str(), bq::file_manager::get_and_clear_last_file_error());
            return create_memory_map_result::failed;
        }
        auto map_size = calculate_size_of_memory(required_size_);
        size_t file_size = bq::memory_map::get_min_size_of_memory_map_file(0, map_size);
        create_memory_map_result result = create_memory_map_result::failed;
        size_t current_file_size = bq::file_manager::instance().get_file_size(memory_map_file_);
        if (current_file_size != file_size) {
            if (!bq::file_manager::instance().truncate_file(memory_map_file_, file_size)) {
                bq::util::log_device_console(log_level::warning, "normal buffer truncate memory map file \"%s\" failed, use memory instead.", memory_map_file_.abs_file_path().c_str());
                bq::file_manager::instance().close_file(memory_map_file_);
                bq::file_manager::remove_file_or_dir(mmap_file_abs_path_);
                return create_memory_map_result::failed;
            }
            result = create_memory_map_result::new_created;
        } else {
            result = create_memory_map_result::use_existed;
        }

        memory_map_handle_ = bq::memory_map::create_memory_map(memory_map_file_, 0, map_size);
        if (!memory_map_handle_.has_been_mapped()) {
            bq::util::log_device_console(log_level::warning, "ring buffer create memory map failed from file \"%s\" failed, use memory instead.", memory_map_file_.abs_file_path().c_str());
            bq::file_manager::instance().close_file(memory_map_file_);
            return create_memory_map_result::failed;
        }

        if (((uintptr_t)memory_map_handle_.get_mapped_data() & (CACHE_LINE_SIZE - 1)) != 0) {
            bq::util::log_device_console(log_level::warning, "normal buffer memory map file \"%s\" memory address is not aligned, use memory instead.", memory_map_file_.abs_file_path().c_str());
            bq::memory_map::release_memory_map(memory_map_handle_);
            bq::file_manager::instance().close_file(memory_map_file_); 
            return create_memory_map_result::failed;
        }
        buffer_data_ = (uint8_t*)memory_map_handle_.get_mapped_data();
        return result;
    }

    bool normal_buffer::try_recover_from_exist_memory_map()
    {
        if (get_buffer().get_memory_map_buffer_state() != bq::memory_map_buffer_state::recover_from_memory_map) {
            return false;
        }
        return true;
    }

    void normal_buffer::init_with_memory_map()
    {
        memset(buffer_data_, 0, HEAD_SIZE);
    }

    void normal_buffer::init_with_memory()
    {
        memset(buffer_data_, 0, HEAD_SIZE);
    }

    bool normal_buffer::uninit_memory_map()
    {
        if (memory_map_handle_.has_been_mapped()) {
            bq::memory_map::release_memory_map(memory_map_handle_);
            return true;
        }
        return false;
    }
 }

