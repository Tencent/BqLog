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
#if defined(BQ_IOS)
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"

namespace bq {
    namespace platform {
        void ios_vprintf(const char* __restrict format, va_list args);
        void ios_print(const char* __restrict content);
        // get the read only programe home path: /var/mobile/Containers/Data/Application/XXXXXX/
        bq::string get_programe_home_path();
    }
}
#endif
