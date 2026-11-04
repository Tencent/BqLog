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
#include "bq_common/encryption/random_bytes.h"
#include "bq_common/encryption/chacha20.h"
#include "bq_common/platform/atomic/atomic.h"
#include "bq_common/platform/thread/thread.h"

namespace bq {
    namespace encryption {
        namespace {
            // Installed by the application on platforms that have no CSPRNG of their own. Read on the
            // segment-creation path, which is not hot, but still has to be race free against a late
            // injection.
            bq::platform::atomic_trivially_constructible<secure_random_fn> s_secure_random_fn;

            // MurmurHash3 finalizer. Only used to spread each weak signal over all 64 bits before
            // they are packed into a ChaCha20 key; it adds no entropy of its own.
            bq_forceinline uint64_t mix64(uint64_t x)
            {
                x ^= x >> 33;
                x *= 0xff51afd7ed558ccdULL;
                x ^= x >> 33;
                x *= 0xc4ceb9fe1a85ec53ULL;
                x ^= x >> 33;
                return x;
            }

            bq_forceinline void store_le64(uint8_t* dst, uint64_t value)
            {
                for (int32_t i = 0; i < 8; ++i) {
                    dst[i] = static_cast<uint8_t>(value >> (i * 8));
                }
            }
        }

        void set_secure_random_source(secure_random_fn fn)
        {
            s_secure_random_fn.store(fn, bq::platform::memory_order::release);
        }

        bool has_secure_random_source()
        {
            uint8_t probe[1];
            return fill_random_bytes(probe, sizeof(probe)) != entropy_source::software_mix;
        }

        entropy_source fill_random_bytes(void* buf, size_t size)
        {
            if (!buf || size == 0) {
                return entropy_source::platform_csprng;
            }
            const secure_random_fn injected_fn = s_secure_random_fn.load(bq::platform::memory_order::acquire);
            if (injected_fn && injected_fn(buf, size)) {
                return entropy_source::injected;
            }
            if (bq::platform::secure_random_bytes(buf, size)) {
                return entropy_source::platform_csprng;
            }

            static bq::platform::atomic_trivially_constructible<uint64_t> call_counter;
            const uint64_t counter = call_counter.fetch_add_raw(1) + 1;

            const uint64_t signals[6] = {
                bq::platform::system_epoch_ms(),
                bq::platform::high_performance_epoch_ms(),
                static_cast<uint64_t>(bq::platform::thread::get_current_thread_id()),
                counter,
                static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&counter)),
                static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&fill_random_bytes)),
            };

            uint8_t key[chacha20::key_size];
            // Fold every weak signal into one 64-bit digest, then expand it into four key lanes.
            // chained so that no single signal alone determines the key.
            uint64_t digest = mix64(counter ^ signals[0]);
            for (size_t i = 1; i < sizeof(signals) / sizeof(signals[0]); ++i) {
                digest = mix64(digest ^ signals[i]);
            }
            // The step is named so that both factors already have the accumulator's type. A ULL
            // literal would widen the product to unsigned long long where uint64_t is unsigned
            // long, and -Wsign-conversion reports that widening even though no value changes.
            const uint64_t lane_step = 0x9E3779B97F4A7C15ULL;
            for (int32_t lane = 0; lane < 4; ++lane) {
                store_le64(key + lane * 8, mix64(digest + static_cast<uint64_t>(lane) * lane_step));
            }

            uint8_t nonce[chacha20::nonce_size];
            const uint64_t nonce_lo = mix64(digest ^ 0x243F6A8885A308D3ULL);
            const uint64_t nonce_hi = mix64(nonce_lo);
            store_le64(nonce, nonce_lo);
            for (size_t i = 8; i < chacha20::nonce_size; ++i) {
                nonce[i] = static_cast<uint8_t>(nonce_hi >> ((i - 8) * 8));
            }

            memset(buf, 0, size);
            chacha20::keystream_xor(static_cast<uint8_t*>(buf), size, key, nonce, 0);
            return entropy_source::software_mix;
        }

        void secure_zero(void* buf, size_t size)
        {
            // volatile is the portable way to say "this write has an effect the compiler cannot see".
            // The loop is not hot: it runs once per segment, or once per key teardown.
            volatile uint8_t* p = static_cast<volatile uint8_t*>(buf);
            while (size-- > 0) {
                *p++ = 0U;
            }
        }
    }
}
