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

#include "native_src_bq_common_bq_common.h"

namespace bq {
    namespace encryption {
        /// <summary>
        /// Where a fill_random_bytes() result came from; callers report software_mix instead of hiding it.
        /// </summary>
        enum class entropy_source : int32_t {
            platform_csprng = 0,
            software_mix = 1,
            injected = 2,
        };

        /// <summary>
        /// Process-wide fallback for platforms without a system CSPRNG (consoles). Null removes it.
        /// </summary>
        typedef bool (*secure_random_fn)(void* buf, size_t size);
        void set_secure_random_source(secure_random_fn fn);

        /// <summary>
        /// True when key material comes from the platform CSPRNG or an injected source.
        /// </summary>
        bool has_secure_random_source();

        /// <summary>
        /// Fills [buf, buf + size) from the platform CSPRNG, else the injected source, else a ChaCha20
        /// keystream keyed by clocks, thread id, addresses and a counter, which is not cryptographic.
        /// </summary>
        entropy_source fill_random_bytes(void* buf, size_t size);

        /// <summary>
        /// Zeroes [buf, buf + size) in a way the optimizer may not drop.
        /// </summary>
        void secure_zero(void* buf, size_t size);
    }
}
