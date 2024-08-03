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
#include "bq_log/log/log_manager.h"
#include "bq_log/log/appender/appender_base.h"
#include "bq_log/log/log_imp.h"

namespace bq {
    appender_base::appender_base()
    {
        clear();
    }

    appender_base::~appender_base()
    {
        clear();
    }

    const bq::string& appender_base::get_config_name_by_type(appender_base::appender_type type)
    {
        const static bq::string TYPE_CONFIG_NAME[(int32_t)appender_base::appender_type::type_count] = { "console", "text_file", "raw_file", "compressed_file" };
        return TYPE_CONFIG_NAME[(int32_t)type];
    }

    void appender_base::set_enable(bool enable)
    {
        appenders_enable = enable;
    }

    bool appender_base::get_enable()
    {
        return appenders_enable;
    }

    void appender_base::clear()
    {
        is_gmt_time_ = false;
        log_level_bitmap_.clear();
        parent_log_ = nullptr;
        layout_ptr_ = nullptr;
        name_.clear();
        categories_mask_array_.clear();
        categories_mask_config_.clear();
    }

    bool appender_base::init(const bq::string& name, const bq::property_value& config_obj, const log_imp* parent_log)
    {
        clear();
        name_ = name;
        const auto& levels_array = config_obj["levels"];
        if (!levels_array.is_array()) {
            util::log_device_console(bq::log_level::info, "bq log info: no levels config was found in appender type %s, use default level \"all\"", ((string)config_obj["type"]).c_str());
        } else {
            for (typename property_value::array_type::size_type i = 0; i < levels_array.array_size(); ++i) {
                const auto& level_obj = levels_array[i];
                if (!level_obj.is_string()) {
                    util::log_device_console(bq::log_level::warning, "bq log info: invalid level config was found in appender type %s, use default level \"all\"", ((string)config_obj["type"]).c_str());
                    continue;
                }
                log_level_bitmap_.add_level(((string)level_obj).trim());
            }
        }
        is_gmt_time_ = false;
        if (config_obj["time_zone"].is_string()) {
            bq::string time_zone_str = ((string)config_obj["time_zone"]).trim();
            if (time_zone_str.equals_ignore_case("gmt")) {
                is_gmt_time_ = true;
            }
        }

        if (config_obj["enable"].is_bool()) {
            appenders_enable = (bool)config_obj["enable"];
        }

        // config verification is already done in log_imp::add_appender
        bq::string type_str = ((string)config_obj["type"]).trim();
        for (int32_t i = 0; i < (int32_t)appender_base::appender_type::type_count; ++i) {
            if (type_str.equals_ignore_case(appender_base::get_config_name_by_type((appender_base::appender_type)i))) {
                type_ = (appender_base::appender_type)i;
                break;
            }
        }
        parent_log_ = parent_log;

        // init categories mask
        {
            categories_mask_config_.clear();
            const auto& categories_mask_config = config_obj["categories_mask"];
            if (categories_mask_config.is_array()) {
                for (typename property_value::array_type::size_type i = 0; i < categories_mask_config.array_size(); ++i) {
                    if (categories_mask_config[i].is_string()) {
                        bq::string mask = (string)categories_mask_config[i];
                        categories_mask_config_.push_back(bq::move(mask));
                    }
                }
            }
        }

        categories_mask_array_.clear();
        auto& categories_name_array_ = parent_log->get_categories_name();
        for (size_t i = 0; i < categories_name_array_.size(); ++i) {
            const auto& category_name = categories_name_array_[i];
            uint8_t mask = 0;
            if (categories_mask_config_.size() == 0) {
                mask = 1;
            } else {
                for (const bq::string& mask_config : categories_mask_config_) {
                    if (mask_config == "*" || category_name.begin_with(mask_config)) {
                        mask = 1;
                        break;
                    }
                }
            }
            categories_mask_array_.push_back(mask);
        }
        if (parent_log_->get_thread_mode() == log_thread_mode::async) {
            layout_ptr_ = &log_manager::instance().get_public_layout();
        } else {
            layout_ptr_ = const_cast<layout*>(&(parent_log_->get_layout()));
        }
        return init_impl(config_obj);
    }

    void appender_base::log(const log_entry_handle& handle)
    {
        auto category_idx = handle.get_log_head().category_idx;
        if (categories_mask_array_.size() <= category_idx || categories_mask_array_[category_idx] == 0) {
            return;
        }

        if (log_level_bitmap_.have_level(handle.get_level())) {
            if (appenders_enable)
                log_impl(handle);
        }
    }
}
