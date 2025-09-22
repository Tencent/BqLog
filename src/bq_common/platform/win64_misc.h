#pragma once
/*
 * Copyright (C) 2024 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#ifdef BQ_WIN
#include <WinSock2.h>
#include <windows.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"

namespace bq {
    namespace platform {
        using platform_file_handle = HANDLE;
        const platform_file_handle invalid_platform_file_handle = INVALID_HANDLE_VALUE;
        bq_forceinline bool is_platform_handle_valid(const platform_file_handle& file_handle)
        {
            return file_handle != invalid_platform_file_handle;
        }
    }
}
#endif
