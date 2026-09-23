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
#include <stdlib.h>
#include <string.h>
#include "native_include_bq_common_platform_macros.h"

namespace bq {
    /*!
     * \class bounded_hash_cache
     * \brief Open-addressing (linear probing) uint64 -> uint32 cache with a hard
     *        entry limit, used by the compressed file appender.
     *
     * Design notes:
     * - Table grows by doubling at 50% load, up to max_size entries.
     * - The slot hash runs the key through the murmur3 fmix64 finalizer and
     *   takes the HIGH bits of the result. Plain low-bit masking or XOR folding
     *   collapses for structured key families (e.g. 4-aligned Windows thread
     *   ids, keys whose entropy lives only in the high 32 bits, or keys built
     *   to cancel a fixed XOR/multiplier). fmix64 avalanches every input bit
     *   into every output bit, which removes those known degenerate families;
     *   like any fixed-seed hash it is not a formal worst-case guarantee, but
     *   the keys here are internal CRC64 format hashes and thread ids.
     * - Once full, a missed insert is admitted once every admission_divisor
     *   attempts and evicts the entry under a clock hand. The sampled
     *   admission keeps the resident set stable under scans and over-capacity
     *   cyclic workloads (always-admit and second-chance variants were
     *   measured to be much worse there). Lower gates (1/32, 1/16) re-admit a
     *   renewed hot set faster but cost measurably more CPU on cyclic and
     *   scan workloads, so the default stays at 1/64.
     * - resize() allocates the new table before releasing the old one, so an
     *   allocation failure never loses the existing cache content.
     */
    template <uint32_t MAX_SIZE>
    class bounded_hash_cache {
    public:
        struct insert_token {
            uint32_t slot_index = UINT32_MAX;
            uint32_t table_revision = 0;

            bool is_valid() const
            {
                return slot_index != UINT32_MAX;
            }
        };

        explicit bounded_hash_cache(uint32_t max_size = MAX_SIZE)
            : max_size_(normalize_max_size(max_size))
        {
        }

        ~bounded_hash_cache()
        {
            free(keys_);
            free(values_);
        }

        bq_forceinline bool find(uint64_t key, uint32_t& value, insert_token& token)
        {
            bool found = false;
            const uint32_t slot_index = find_slot_or_empty(key, found);
            if (!found) {
                token.slot_index = slot_index;
                token.table_revision = table_revision_;
                return false;
            }
            value = values_[slot_index];
            return true;
        }

        bq_forceinline void insert(uint64_t key, uint32_t value)
        {
            bool found = false;
            const uint32_t slot_index = find_slot_or_empty(key, found);
            if (found) {
                values_[slot_index] = value;
            } else {
                insert_new(key, value, insert_token());
            }
        }

        bq_forceinline void insert(uint64_t key, uint32_t value, const insert_token& token)
        {
            insert_new(key, value, token);
        }

        bool set_max_size(uint32_t max_size)
        {
            const uint32_t normalized = normalize_max_size(max_size);
            if (normalized == max_size_) {
                return true;
            }
            if (size_ != 0 && normalized < max_size_) {
                return false;
            }
            max_size_ = normalized;
            return true;
        }

        uint32_t get_max_size() const
        {
            return max_size_;
        }

        void clear()
        {
            free(keys_);
            free(values_);
            keys_ = nullptr;
            values_ = nullptr;
            size_ = 0;
            capacity_ = 0;
            victim_ = 0;
            admission_ = 0;
            ++table_revision_;
        }

#if defined(BQ_UNIT_TEST)
        void fail_allocation_after_for_test(int32_t successful_allocations_before_failure)
        {
            allocation_successes_before_failure_ = successful_allocations_before_failure;
        }

        void clear_allocation_failure_for_test()
        {
            allocation_successes_before_failure_ = -1;
        }
#endif

    private:
        static constexpr uint32_t invalid_value = static_cast<uint32_t>(-1);

        static constexpr uint32_t normalize_max_size(uint32_t max_size)
        {
            return max_size < min_capacity
                ? min_capacity
                : (max_size > MAX_SIZE ? MAX_SIZE : max_size);
        }

        static bq_forceinline uint32_t get_table_hash(uint64_t key)
        {
            // murmur3 fmix64 finalizer.
            key ^= key >> 33;
            key *= UINT64_C(0xff51afd7ed558ccd);
            key ^= key >> 33;
            key *= UINT64_C(0xc4ceb9fe1a85ec53);
            key ^= key >> 33;
            return static_cast<uint32_t>(key >> 32);
        }

        bq_forceinline uint32_t find_slot_or_empty(uint64_t key, bool& found) const
        {
            if (!values_) {
                found = false;
                return invalid_value;
            }
            uint32_t slot_index = get_table_hash(key) & (capacity_ - 1);
            while (values_[slot_index] != invalid_value) {
                if (keys_[slot_index] == key) {
                    found = true;
                    return slot_index;
                }
                slot_index = (slot_index + 1) & (capacity_ - 1);
            }
            found = false;
            return slot_index;
        }

        void* allocate_bytes(size_t size)
        {
#if defined(BQ_UNIT_TEST)
            if (allocation_successes_before_failure_ >= 0) {
                if (allocation_successes_before_failure_ == 0) {
                    allocation_successes_before_failure_ = -1;
                    return nullptr;
                }
                --allocation_successes_before_failure_;
            }
#endif
            return malloc(size);
        }

        bool resize(uint32_t new_capacity)
        {
            uint64_t* new_keys = static_cast<uint64_t*>(allocate_bytes(sizeof(uint64_t) * new_capacity));
            if (!new_keys) {
                return false;
            }
            uint32_t* new_values = static_cast<uint32_t*>(allocate_bytes(sizeof(uint32_t) * new_capacity));
            if (!new_values) {
                free(new_keys);
                free(new_values);
                return false;
            }
            memset(new_values, 0xFF, sizeof(uint32_t) * new_capacity);
            for (uint32_t i = 0; i < capacity_; ++i) {
                if (values_[i] != invalid_value) {
                    uint32_t slot_index = get_table_hash(keys_[i]) & (new_capacity - 1);
                    while (new_values[slot_index] != invalid_value) {
                        slot_index = (slot_index + 1) & (new_capacity - 1);
                    }
                    new_keys[slot_index] = keys_[i];
                    new_values[slot_index] = values_[i];
                }
            }
            free(keys_);
            free(values_);
            keys_ = new_keys;
            values_ = new_values;
            capacity_ = new_capacity;
            victim_ = 0;
            ++table_revision_;
            return true;
        }

        void erase(uint32_t slot_index)
        {
            const uint32_t mask = capacity_ - 1;
            uint32_t empty = slot_index;
            uint32_t current = (slot_index + 1) & mask;
            while (values_[current] != invalid_value) {
                const uint32_t home = get_table_hash(keys_[current]) & mask;
                if (((current - home) & mask) > ((empty - home) & mask)) {
                    keys_[empty] = keys_[current];
                    values_[empty] = values_[current];
                    empty = current;
                }
                current = (current + 1) & mask;
            }
            values_[empty] = invalid_value;
            --size_;
            ++table_revision_;
        }

        bq_forceinline void insert_new(uint64_t key, uint32_t value, const insert_token& token)
        {
            if (!values_) {
                if (!resize(min_capacity)) {
                    return;
                }
            } else if (size_ < max_size_ && size_ >= capacity_ / 2) {
                if (!resize(capacity_ * 2)) {
                    return;
                }
            } else if (size_ >= max_size_) {
                if ((++admission_ & (admission_divisor - 1)) != 0) {
                    return;
                }
                while (values_[victim_] == invalid_value) {
                    victim_ = (victim_ + 1) & (capacity_ - 1);
                }
                const uint32_t old_victim = victim_;
                victim_ = (victim_ + 1) & (capacity_ - 1);
                erase(old_victim);
            }

            uint32_t slot_index = invalid_value;
            if (token.table_revision == table_revision_
                && token.slot_index < capacity_
                && values_[token.slot_index] == invalid_value) {
                slot_index = token.slot_index;
            } else {
                bool found = false;
                slot_index = find_slot_or_empty(key, found);
                if (found) {
                    values_[slot_index] = value;
                    return;
                }
            }
            keys_[slot_index] = key;
            values_[slot_index] = value;
            ++size_;
            ++table_revision_;
        }

        static constexpr uint32_t min_capacity = 8;
        // A missed insert is admitted once every admission_divisor attempts when full.
        static constexpr uint32_t admission_divisor = 64;

        static_assert(MAX_SIZE >= min_capacity, "MAX_SIZE is too small");

        bounded_hash_cache(const bounded_hash_cache&) = delete;
        bounded_hash_cache& operator=(const bounded_hash_cache&) = delete;

        uint64_t* keys_ = nullptr;
        uint32_t* values_ = nullptr;
        uint32_t size_ = 0;
        uint32_t capacity_ = 0;
        uint32_t victim_ = 0;
        uint32_t admission_ = 0;
        uint32_t max_size_ = MAX_SIZE;
        uint32_t table_revision_ = 0;

#if defined(BQ_UNIT_TEST)
        int32_t allocation_successes_before_failure_ = -1;
#endif
    };
}
