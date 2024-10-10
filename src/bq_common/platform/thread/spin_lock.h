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
/*!
 * \file spin_lock.h
 * TTAS high performance spin lock
 * Important: It's not reentrant!
 *
 * \author pippocao
 * \date 2023/08/21
 *
 *
 */
#include <stdint.h>
#include "bq_common/bq_common.h"

namespace bq {
    namespace platform {
        class spin_lock {
        private:
            bq::cache_friendly_type<bq::platform::atomic<bool>> value_;
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
            bq::platform::atomic<bq::platform::thread::thread_id> thread_id_;
#endif
        private:
            void yield();

        public:
            spin_lock()
                : value_(false)
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                , thread_id_(0)
#endif
            {
            }

            spin_lock(const spin_lock&) = delete;
            spin_lock(spin_lock&&) noexcept = delete;
            spin_lock& operator=(const spin_lock&) = delete;
            spin_lock& operator=(spin_lock&&) noexcept = delete;

            inline void lock()
            {
                while (true) {
                    if (!value_.get().exchange(true, bq::platform::memory_order::acquire)) {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                        thread_id_.store(bq::platform::thread::get_current_thread_id(), bq::platform::memory_order::seq_cst);
#endif
                        break;
                    }
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                    assert(bq::platform::thread::get_current_thread_id() != thread_id_.load(bq::platform::memory_order::seq_cst) && "spin_lock is not reentrant");
#endif
                    while (value_.get().load(bq::platform::memory_order::relaxed)) {
                        yield();
                    }
                }
            }

            inline void unlock()
            {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                thread_id_.store(0, bq::platform::memory_order::seq_cst);
#endif
                value_.get().store(false, bq::platform::memory_order::release);
            }
        };

        /// <summary>
        /// This is an crazy optimized read-write spin lock,
        /// betting that you won't have more than INT32_MAX(32 bit) or INT64_MAX(64 bit) threads waiting to acquire the read lock.
        /// This version is designed for extreme performance of read locks when there is no write lock contention.
        /// warning: the write lock is not re-entrant.
        /// by pippocao
        /// 2024/7/8
        /// </summary>
        class spin_lock_rw_crazy {
        private:
            typedef bq::condition_type_t<sizeof(void*) == 4, int32_t, int64_t> counter_type;
            static constexpr counter_type write_lock_mark_value = bq::condition_value<sizeof(void*) == 4, counter_type, (counter_type)INT32_MIN, (counter_type)INT64_MIN>::value;
            bq::cache_friendly_type<bq::platform::atomic<counter_type>> counter_;
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
            bq::platform::atomic<bq::platform::thread::thread_id> write_lock_thread_id_;
#endif
        private:
            void yield();

        public:
            spin_lock_rw_crazy()
                : counter_(0)
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                , write_lock_thread_id_(0)
#endif
            {
            }

            spin_lock_rw_crazy(const spin_lock_rw_crazy&) = delete;
            spin_lock_rw_crazy(spin_lock_rw_crazy&&) noexcept = delete;
            spin_lock_rw_crazy& operator=(const spin_lock_rw_crazy&) = delete;
            spin_lock_rw_crazy& operator=(spin_lock_rw_crazy&&) noexcept = delete;

            inline void read_lock()
            {
                while (true) {
                    counter_type previous_counter = counter_.get().fetch_add(1, bq::platform::memory_order::acquire);
                    if (previous_counter >= 0) {
                        // read lock success.
                        break;
                    }
                    while (true) {
                        yield();
                        counter_type current_counter = counter_.get().load(bq::platform::memory_order::relaxed);
                        if (current_counter >= 0) {
                            break;
                        }
                    }
                }
            }

            inline void read_unlock()
            {
                counter_type previous_counter = counter_.get().fetch_add(-1, bq::platform::memory_order::release);
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                assert(previous_counter > 0 && "spin_lock_rw_crazy counter error");
#else
                (void)previous_counter;
#endif
            }

            inline void write_lock()
            {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                assert(bq::platform::thread::get_current_thread_id() != write_lock_thread_id_.load(bq::platform::memory_order::seq_cst) && "spin_lock is not reentrant");
                write_lock_thread_id_.store(bq::platform::thread::get_current_thread_id(), bq::platform::memory_order::seq_cst);
#endif
                while (true) {
                    counter_type expected_counter = 0;
                    if (counter_.get().compare_exchange_strong(expected_counter, write_lock_mark_value, bq::platform::memory_order::acquire, bq::platform::memory_order::relaxed)) {
                        break;
                    }
                    yield();
                }
            }

            inline void write_unlock()
            {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                write_lock_thread_id_.store(0, bq::platform::memory_order::seq_cst);
#endif
                counter_.get().store(0, bq::platform::memory_order::release);
            }
        };

        class scoped_spin_lock {
        private:
            spin_lock& lock_;

        public:
            scoped_spin_lock() = delete;

            scoped_spin_lock(spin_lock& lock)
                : lock_(lock)
            {
                lock_.lock();
            }

            ~scoped_spin_lock()
            {
                lock_.unlock();
            }
        };

        class scoped_spin_lock_read_crazy {
        private:
            spin_lock_rw_crazy& lock_;

        public:
            scoped_spin_lock_read_crazy() = delete;

            scoped_spin_lock_read_crazy(spin_lock_rw_crazy& lock)
                : lock_(lock)
            {
                lock_.read_lock();
            }

            ~scoped_spin_lock_read_crazy()
            {
                lock_.read_unlock();
            }
        };

        class scoped_spin_lock_write_crazy {
        private:
            spin_lock_rw_crazy& lock_;

        public:
            scoped_spin_lock_write_crazy() = delete;

            scoped_spin_lock_write_crazy(spin_lock_rw_crazy& lock)
                : lock_(lock)
            {
                lock_.write_lock();
            }

            ~scoped_spin_lock_write_crazy()
            {
                lock_.write_unlock();
            }
        };
    }
}
