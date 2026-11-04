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

#include "bq_common/bq_common.h"

namespace bq {
    namespace encryption {
        /// <summary>
        /// Where a fill_random_bytes() result came from. The caller is expected to surface the
        /// degraded case instead of silently accepting it: weakly seeded key material is not a
        /// failure that shows up in tests, it shows up as a break.
        /// </summary>
        enum class entropy_source : int32_t {
            platform_csprng = 0,
            software_mix = 1,
            injected = 2,
        };

        /// <summary>
        /// Supplies the process-wide secure random source. Console platforms have no CSPRNG of
        /// their own and their SDK's is under NDA, so the application injects one. Without it key
        /// material comes from the software mix below: encryption still happens, and the appender
        /// says once at creation that its keys are weaker than they should be. Passing null removes
        /// the injected source.
        /// </summary>
        typedef bool (*secure_random_fn)(void* buf, size_t size);
        void set_secure_random_source(secure_random_fn fn);

        /// <summary>
        /// True when key material would come from a real CSPRNG - injected or from the platform.
        /// </summary>
        bool has_secure_random_source();

        /// <summary>
        /// Fills [buf, buf + size) with key material.
        /// Uses the injected source first, then the operating system CSPRNG where one exists.
        /// Console SDKs expose none, so there the bytes come from mixing clocks, thread id, ASLR
        /// addresses and a call counter into a ChaCha20 keystream - enough to defeat "seed with
        /// epoch milliseconds and predict the whole keystream", not enough to be called
        /// cryptographic randomness. That last result is reported so callers can say so.
        /// </summary>
        entropy_source fill_random_bytes(void* buf, size_t size);

        /// <summary>
        /// Zeroes [buf, buf + size) in a way the optimizer may not drop. A plain memset on a buffer
        /// that is never read again is dead store elimination material, and "we wipe the key" would
        /// then be a comment rather than code.
        /// </summary>
        void secure_zero(void* buf, size_t size);
    }
}
