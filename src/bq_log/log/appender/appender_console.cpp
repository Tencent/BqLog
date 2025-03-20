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
#include <stdlib.h>
#include "bq_common/bq_common.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/appender/appender_console.h"
#include "bq_log/types/buffer/siso_ring_buffer.h"

namespace bq {
    bq::appender_console::console_static_misc appender_console::console_misc_;

    void appender_console::console_callbacks::register_callback(bq::type_func_ptr_console_callback callback)
    {
        bq::platform::scoped_mutex lock(mutex_);
        callbacks_[callback] = true;
    }

    void appender_console::console_callbacks::erase_callback(bq::type_func_ptr_console_callback callback)
    {
        bq::platform::scoped_mutex lock(mutex_);
        auto iter = callbacks_.find(callback);
        if (iter != callbacks_.end()) {
            callbacks_.erase(iter);
        }
    }

    void appender_console::console_callbacks::call(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
    {
        if (callbacks_.is_empty()) {
            return;
        }
        bq::platform::scoped_mutex lock(mutex_);
        for (auto iter = callbacks_.begin(); iter != callbacks_.end(); ++iter) {
            bq::type_func_ptr_console_callback callback = iter->key();
            callback(log_id, category_idx, log_level, content, length);
        }
    }

    appender_console::console_buffer::console_buffer()
        : buffer_(nullptr)
    {
    }

    appender_console::console_buffer::~console_buffer()
    {
        auto real_buffer = buffer_.exchange_seq_cst(nullptr);
        if (real_buffer) {
            delete real_buffer;
        }
    }

    void appender_console::console_buffer::insert(uint64_t epoch_ms, uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
    {
        if (!is_enable()) {
            return;
        }
        size_t write_length = sizeof(log_id) + sizeof(category_idx) + sizeof(log_level) + sizeof(int32_t) + (size_t)length + 1;

        auto buffer = buffer_.load_relaxed();
        if (!buffer) {
            buffer = buffer_.load_acquire();
            if (!buffer) {
                log_buffer_config config;
                config.use_mmap = false;
                config.policy = log_memory_policy::auto_expand_when_full;
                config.high_frequency_threshold_per_second = UINT64_MAX;
                buffer = new bq::log_buffer(config);
                log_buffer* expected = nullptr;
                if (!buffer_.compare_exchange_strong(expected, buffer, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                    delete buffer;
                    buffer = expected;
                }
            }
        }
        auto handle = buffer->alloc_write_chunk((uint32_t)write_length, epoch_ms);
        bq::scoped_log_buffer_handle scoped_handle(*buffer, handle);
        if (handle.result == enum_buffer_result_code::success) {
            uint8_t* data = handle.data_addr;
            *(uint64_t*)data = log_id;
            data += sizeof(uint64_t);
            *(uint32_t*)data = category_idx;
            data += sizeof(uint32_t);
            *(int32_t*)data = log_level;
            data += sizeof(int32_t);
            *(int32_t*)data = length;
            data += sizeof(int32_t);
            memcpy(data, content, (size_t)length);
            data[length] = 0;
        } else {
            util::log_device_console(log_level::error, "failed to insert data entry to console fetch buffer, ring_buffer error code:%d", (int32_t)handle.result);
        }
    }

    bool appender_console::console_buffer::fetch_and_remove(bq::type_func_ptr_console_buffer_fetch_callback callback, const void* pass_through_param)
    {
        if (!is_enable()) {
            return false;
        }

        auto buffer = buffer_.load_relaxed();
        if (!buffer) {
            return false;
        }
        auto read_handle = buffer->read_chunk();
        scoped_log_buffer_handle scoped_read_handle(*buffer, read_handle);
        if (read_handle.result == enum_buffer_result_code::success) {
            uint8_t* data = read_handle.data_addr;
            uint64_t log_id = *(uint64_t*)data;
            data += sizeof(uint64_t);
            uint32_t category_idx = *(uint32_t*)data;
            data += sizeof(uint32_t);
            int32_t log_level = *(int32_t*)data;
            data += sizeof(int32_t);
            int32_t length = *(int32_t*)data;
            data += sizeof(int32_t);
            char* content = (char*)data;
            callback(const_cast<void*>(pass_through_param), log_id, category_idx, log_level, content, length);
        } else if (read_handle.result == enum_buffer_result_code::err_empty_log_buffer) {
            // do nothing
        } else {
            bq::util::log_device_console(bq::log_level::error, "failed to fetch data entry from console buffer, ring_buffer error code:%d", (int32_t)read_handle.result);
        }
        return read_handle.result == enum_buffer_result_code::success;
    }

    void appender_console::console_buffer::set_enable(bool enable)
    {
        enable_ = enable;
    }

    appender_console::appender_console()
    {
    }

    void appender_console::register_console_callback(bq::type_func_ptr_console_callback callback)
    {
        console_misc_.callback().register_callback(callback);
    }

    void appender_console::unregister_console_callback(bq::type_func_ptr_console_callback callback)
    {
        console_misc_.callback().erase_callback(callback);
    }

    void appender_console::set_console_buffer_enable(bool enable)
    {
        console_misc_.buffer().set_enable(enable);
    }

    bool appender_console::fetch_and_remove_from_console_buffer(bq::type_func_ptr_console_buffer_fetch_callback callback, const void* pass_through_param)
    {
        return console_misc_.buffer().fetch_and_remove(callback, pass_through_param);
    }

    bool appender_console::init_impl(const bq::property_value& config_obj)
    {
        (void)config_obj;
        log_name_prefix_ += "[";
        log_name_prefix_ += parent_log_->get_name();
        log_name_prefix_ += "]\t";
        log_entry_cache_ = log_name_prefix_;
        return true;
    }

    void appender_console::log_impl(const log_entry_handle& handle)
    {
        auto layout_result = layout_ptr_->do_layout(handle, is_gmt_time_, &parent_log_->get_categories_name());
        if (layout_result != layout::enum_layout_result::finished) {
            bq::util::log_device_console(log_level::error, "console layout error, result:%d, format str:%s", (int32_t)layout_result, handle.get_format_string_data());
            return;
        }
        log_entry_cache_.erase(log_entry_cache_.begin() + log_name_prefix_.size(), (log_entry_cache_.size() - log_name_prefix_.size()));
        const char* text_log_data = layout_ptr_->get_formated_str();
        uint32_t log_text_len = layout_ptr_->get_formated_str_len();
        log_entry_cache_.insert_batch(log_entry_cache_.end(), text_log_data, log_text_len);
        auto level = handle.get_level();

#if !BQ_UNIT_TEST
        util::log_device_console_plain_text(level, log_entry_cache_.c_str());
#endif
        console_misc_.callback().call(parent_log_->id(), handle.get_category_idx(), (int32_t)level, log_entry_cache_.c_str(), (int32_t)log_entry_cache_.size());
        console_misc_.buffer().insert(handle.get_log_head().timestamp_epoch, parent_log_->id(), handle.get_category_idx(), (int32_t)level, log_entry_cache_.c_str(), (int32_t)log_entry_cache_.size());
    }

}
