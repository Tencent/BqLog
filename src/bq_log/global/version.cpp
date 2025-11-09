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
#include "bq_log/global/version.h"
namespace bq {
    const char* const BQ_LOG_VERSION = "2.0.1";

    const char* get_bq_log_version()
    {
        return BQ_LOG_VERSION;
    }
}