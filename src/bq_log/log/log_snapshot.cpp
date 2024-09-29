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
#include "bq_log/log/log_snapshot.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/utils/log_utils.h"

namespace bq {

    log_snapshot::log_snapshot(class log_imp* parent_log, const bq::property_value& snapshot_config)
        : buffer_size_(0)
        , snapshot_buffer_(nullptr)
        , snapshot_text_ { "", "" }
        , snapshot_text_index_(0)
        , snapshot_text_continuous_(false)
        , parent_log_(parent_log)
    {
        reset_config(snapshot_config);
    }

    log_snapshot::~log_snapshot()
    {
        bq::platform::scoped_spin_lock scoped_lock(lock_);
        if (snapshot_buffer_) {
            delete snapshot_buffer_;
        }
    }

    void log_snapshot::reset_config(const bq::property_value& snapshot_config)
    {
        bq::platform::scoped_spin_lock scoped_lock(lock_);
        uint32_t buffer_size = 0;
        if (snapshot_config["buffer_size"].is_integral()) {
            buffer_size = (uint32_t)(int64_t)snapshot_config["buffer_size"];
        }
        if (buffer_size != 0) {
            if (snapshot_buffer_ && ((uint32_t)(snapshot_buffer_->get_block_size() * snapshot_buffer_->get_total_blocks_count()) != buffer_size)) {
                //create a new snapshot_buffer_ and backup log data.
                ring_buffer* new_buffer = new ring_buffer(buffer_size);
                new_buffer->set_thread_check_enable(false);
                snapshot_buffer_->begin_read();
                while (true) {
                    auto backup_read_handle = snapshot_buffer_->read();
                    if (backup_read_handle.result != enum_buffer_result_code::success) {
                        break;
                    }
                    bool write_success = false;
                    while (!write_success) {
                        auto write_handle = new_buffer->alloc_write_chunk(backup_read_handle.data_size);
                        if (write_handle.result == enum_buffer_result_code::success) {
                            // chunk is too big for new buffer, discard and renew new buffer;
                            memcpy(write_handle.data_addr, backup_read_handle.data_addr, (size_t)backup_read_handle.data_size);
                            write_success = true;
                        }
                        else if (write_handle.result == enum_buffer_result_code::err_alloc_size_invalid) {
                            // chunk is too big for new buffer, discard and renew new buffer;
                            delete new_buffer;
                            new_buffer = new ring_buffer(buffer_size);
                            new_buffer->set_thread_check_enable(false);
                            write_success = true;
                        } else {
                            new_buffer->begin_read();
                            new_buffer->read();
                            new_buffer->end_read();
                        }
                    }

                }
                snapshot_buffer_->end_read();
                delete snapshot_buffer_;
                snapshot_buffer_ = new_buffer;
            } else {
                snapshot_buffer_ = new ring_buffer(buffer_size);
                snapshot_buffer_->set_thread_check_enable(false);
            }
        } else {
            if (snapshot_buffer_) {
                delete snapshot_buffer_;
                snapshot_buffer_ = nullptr;
            }
            snapshot_text_[0].clear();
            snapshot_text_[1].clear();
            snapshot_text_index_ = 0;
            snapshot_text_continuous_ = false;
        }

        const auto& levels_array = snapshot_config["levels"];
        if (!levels_array.is_array()) {
            log_level_bitmap_.add_level("all");
        } else {
            log_level_bitmap_ = bq::log_utils::get_log_level_bitmap_by_config(levels_array);
        }
        
        categories_mask_array_ = bq::log_utils::get_categories_mask_by_config(parent_log_->get_categories_name(), snapshot_config["categories_mask"]);
    }

    void log_snapshot::write_data(const bq::log_entry_handle& log_entry)
    {
        if (log_level_bitmap_.have_level(log_entry.get_level()) && categories_mask_array_[log_entry.get_category_idx()]) {
            bq::platform::scoped_spin_lock scoped_lock(lock_);
            volatile ring_buffer* volatile_ptr = (volatile ring_buffer*)(snapshot_buffer_);
            ring_buffer* buffer = const_cast<ring_buffer*>(volatile_ptr);
            if (buffer) {
                while (true) {
                    bq::ring_buffer_write_handle snapshot_write_handle = buffer->alloc_write_chunk(log_entry.data_size());
                    if (snapshot_write_handle.result == enum_buffer_result_code::success) {
                        memcpy(snapshot_write_handle.data_addr, log_entry.data(), log_entry.data_size());
                        buffer->commit_write_chunk(snapshot_write_handle);
                        break;
                    } else if (snapshot_write_handle.result == enum_buffer_result_code::err_not_enough_space) {
                        buffer->begin_read();
                        buffer->read();
                        buffer->end_read();
                        snapshot_text_continuous_ = false;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    const bq::string& log_snapshot::take_snapshot_string(bool use_gmt_time)
    {
        lock_.lock();
        snapshot_text_index_ = (snapshot_text_index_ + 1) & 0x01;
        bq::string& text = snapshot_text_[snapshot_text_index_];
        text.clear();
        if (!snapshot_buffer_) {
            bq::util::log_device_console_plain_text(log_level::warning, "calling take_snapshot without enable snapshot");
            return snapshot_text_[snapshot_text_index_];
        }
        snapshot_buffer_->begin_read();
        while (true) {
            auto snapshot_read_handle = snapshot_buffer_->read();
            if (snapshot_read_handle.result != enum_buffer_result_code::success) {
                break;
            }
            bq::log_entry_handle item(snapshot_read_handle.data_addr, snapshot_read_handle.data_size);
            snapshot_layout_.do_layout(item, use_gmt_time, &parent_log_->get_categories_name());
            text.insert_batch(text.end(), snapshot_layout_.get_formated_str(), snapshot_layout_.get_formated_str_len());
            text.insert(text.end(), '\n');
        }
        snapshot_buffer_->end_read();

        if (snapshot_text_continuous_) {
            if (text.size() < buffer_size_) {
                uint32_t left_size = buffer_size_ - (uint32_t)text.size();
                bq::string& last_text = snapshot_text_[(snapshot_text_index_ + 1) & 0x01];
                left_size = bq::min_value(left_size, (uint32_t)last_text.size());
                text.insert_batch(text.begin(), last_text.end() - (size_t)left_size, left_size);
            }
        }
        snapshot_text_continuous_ = true;
        return text;
    }

    void log_snapshot::release_snapshot_string()
    {
        lock_.unlock();
    }

}