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
#ifdef BQ_POSIX
#include <sys/stat.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"

namespace bq {
    namespace platform {
        using platform_file_handle = int32_t;
        constexpr platform_file_handle invalid_platform_file_handle = -1;
        bq_forceinline bool is_platform_handle_valid(const platform_file_handle& file_handle)
        {
            return file_handle >= 0;
        }

        // File exclusive works well across different processes,
        // but mutual exclusion within the same process is not explicitly documented to function reliably across different system platforms.
        // To eliminate platform compatibility risks, we decided to implement it ourselves.
        BQ_PACK_BEGIN
        struct alignas(4) file_node_info {
            decltype(bq::declval<struct stat>().st_ino) ino;
            uint64_t hash_code() const;
            bool operator==(const file_node_info& rhs) const
            {
                return ino == rhs.ino;
            }
        } BQ_PACK_END
    }
}
#endif
