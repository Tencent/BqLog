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
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include "bq_log/bq_log.h"

namespace bq_godot {

    // Zero-GC encoder: writes format string (UTF-32 -> UTF-8 directly into ring buffer)
    // and Variant arguments (UTF-32 strings zero-transcoding, POD/math types stack-only)
    // straight into BqLog's ring buffer via __api_log_write_begin / __api_log_write_finish.
    bool write_godot_log_entry(
        uint64_t log_id,
        bq::log_level level,
        uint32_t category_index,
        const godot::String& format_str,
        const godot::Variant* const* args,
        int64_t arg_count);

}
