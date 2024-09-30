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
#include "bq_log/utils/log_utils.h"

namespace bq {


    bool log_utils::get_categories_mask_by_config(const bq::array<bq::string> categories_name, const bq::property_value& categories_mask_config, bq::array_inline<uint8_t>& out_categories_mask)
    {
        assert(categories_name.size() == out_categories_mask.size() && "categories mask size must be equal to categories name size");
        bq::array<bq::string> tmp;
        {
            if (categories_mask_config.is_array()) {
                for (typename property_value::array_type::size_type i = 0; i < categories_mask_config.array_size(); ++i) {
                    if (categories_mask_config[i].is_string()) {
                        bq::string mask = (string)categories_mask_config[i];
                        tmp.push_back(bq::move(mask));
                    }
                }
            }
        }
        // init categories
        {
            tmp.set_capacity(categories_name.size());
            for (size_t i = 0; i < categories_name.size(); ++i) {
                const auto& category_name = categories_name[i];
                uint8_t mask = 0;
                if (tmp.is_empty()) {
                    mask = 1;
                } else {
                    for (const bq::string& mask_config : tmp) {
                        if (category_name == mask_config
                            || (category_name.size() > mask_config.size() && category_name.begin_with(mask_config) && category_name[mask_config.size()] == '.')
                        ) {
                            mask = 1;
                            break;
                        }
                        if (i == 0 && mask_config == "*default") // default category mask
                        {
                            mask = 1;
                            break;
                        }
                    }
                }
                out_categories_mask[i] = mask;
            }
        }
        return true;
    }

    bool log_utils::get_log_level_bitmap_by_config(const bq::property_value& log_level_bitmap_config, bq::log_level_bitmap& out_level_bitmap)
    {
        out_level_bitmap.clear();
        if (!log_level_bitmap_config.is_array()) {
            return false;
        }
        for (typename property_value::array_type::size_type i = 0; i < log_level_bitmap_config.array_size(); ++i) {
            const auto& level_obj = log_level_bitmap_config[i];
            if (!level_obj.is_string()) {
                util::log_device_console(bq::log_level::warning, "bq log info: invalid [log_level] item: %s", level_obj.serialize().c_str());
                continue;
            }
            out_level_bitmap.add_level(((bq::string)level_obj).trim());
        }
        return true;
    }

}
