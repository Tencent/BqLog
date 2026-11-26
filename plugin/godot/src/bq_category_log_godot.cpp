/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_category_log_godot.h"

#include <vector>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include "bq_log/bq_log.h"

void BqCategoryLog::_bind_methods()
{
    godot::ClassDB::bind_method(godot::D_METHOD("init_with_log_id", "log_id"), &BqCategoryLog::init_with_log_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_categories_count"), &BqCategoryLog::get_categories_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_categories_name_array"), &BqCategoryLog::get_categories_name_array);
    godot::ClassDB::bind_static_method(
        "BqCategoryLog",
        godot::D_METHOD("call_api_create_category_log", "name", "config", "categories_count", "categories"),
        &BqCategoryLog::call_api_create_category_log);
}

void BqCategoryLog::init_with_log_id(int64_t log_id)
{
    init_by_id(log_id > 0 ? static_cast<uint64_t>(log_id) : 0u);
}

int64_t BqCategoryLog::call_api_create_category_log(
    const godot::String& name,
    const godot::String& config,
    int64_t categories_count,
    const godot::PackedStringArray& categories)
{
    if (config.is_empty() || categories_count <= 0 || categories.size() < categories_count) {
        return 0;
    }

    const godot::CharString name_utf8 = name.utf8();
    const godot::CharString config_utf8 = config.utf8();

    std::vector<godot::CharString> utf8_storage;
    std::vector<const char*> ptrs;
    utf8_storage.reserve(static_cast<size_t>(categories_count));
    ptrs.reserve(static_cast<size_t>(categories_count));

    for (int64_t i = 0; i < categories_count; ++i) {
        utf8_storage.push_back(categories[static_cast<int32_t>(i)].utf8());
        ptrs.push_back(utf8_storage.back().get_data());
    }

    const uint64_t log_id = bq::api::__api_create_log(
        name_utf8.get_data(),
        config_utf8.get_data(),
        static_cast<uint32_t>(categories_count),
        ptrs.data());
    return static_cast<int64_t>(log_id);
}
