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
#pragma once

#include <stdint.h>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include "bq_log_godot.h"

namespace bq {

    class bq_category_log : public bq_log_base {
        GDCLASS(bq_category_log, bq_log_base)

    protected:
        static void _bind_methods();

    public:
        bq_category_log() = default;

        void init_with_log_id(int64_t log_id);
        int64_t get_categories_count() const { return static_cast<int64_t>(categories_count_); }
        godot::PackedStringArray get_categories_name_array() const { return categories_name_array_; }

        static int64_t call_api_create_category_log(
            const godot::String& name,
            const godot::String& config,
            int64_t categories_count,
            const godot::PackedStringArray& categories);
    };

}
