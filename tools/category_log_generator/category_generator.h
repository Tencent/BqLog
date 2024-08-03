#pragma once
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
#include "bq_common/bq_common.h"
#include "common_header.h"

namespace bq {

    class category_generator {
    private:
        static bool parse_config_file(const bq::string& file_path, category_node& category_root);

    public:
        static bool general_log_class(const bq::string& class_name, const bq::string& config_file, const bq::string& output_path);
    };
}
