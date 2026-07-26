/* Copyright (C) 2025 Tencent.
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

#include "bq_common/bq_common_public_include.h"
#if defined(BQ_SWITCH_NN)
namespace bq {
    namespace platform {
        // Opaque pointer to a per-open-file control block (nn::fs::FileHandle
        // plus the current IO offset, since nn::fs IO is positional and keeps
        // no file cursor). Defined in nx_misc.cpp so that platform headers
        // stay free of nn:: includes.
        struct nx_file_control_block;
        using platform_file_handle = nx_file_control_block*;
        constexpr platform_file_handle invalid_platform_file_handle = nullptr;
        bq_forceinline bool is_platform_handle_valid(const platform_file_handle& file_handle)
        {
            return file_handle != invalid_platform_file_handle;
        }

        // File exclusive works well across different processes,
        // but mutual exclusion within the same process is not explicitly documented to function reliably across different system platforms.
        // To eliminate platform compatibility risks, we decided to implement it ourselves.
        // (On Switch/Horizon titles are single-process, so only the in-process
        // cache below matters; nn::fs exposes no dev/ino pair, so the key is
        // the hash of the lexically-normalized path instead.)
        struct file_node_info {
            uint64_t path_hash;
            uint64_t hash_code() const;
            bool operator==(const file_node_info& rhs) const
            {
                return path_hash == rhs.path_hash;
            }
        };
    }
}
#endif
