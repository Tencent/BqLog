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
#include <inttypes.h>
#include "bq_common/utils/utility_types.h"
#include "bq_common/bq_common.h"

namespace bq {
    namespace platform {
        /*
         * The MCS spin lock performed exceptionally well on my test devices under
         * multi-core, high-concurrency scenarios, significantly outperforming regular
         * spin_lock. However, for some unknown reason, its performance was extremely
         * poor in the GitHub Actions test environment. Since it’s difficult to run a
         * profiler on their platform and I’m unclear about their hardware and virtual
         * machine setup, I’ve temporarily abandoned the MCS spin lock.
         */

        // Similar to Linux MCS lock, but it is a simpler version.
        // MCS Lock is much faster than normal spin lock which is implemented by single atomic variable
        // when there are many threads contending for the lock.
        class mcs_spin_lock {
        public:
            struct lock_node {
                bq::platform::atomic<int32_t> lock_counter_ = 0;
                bq::platform::atomic<lock_node*> next_ = nullptr;
                bq::platform::atomic<bq::platform::thread::thread_id> owner_ = 0;
            };
        private:
            bq::platform::atomic<lock_node*> tail_;
        private:
            void yield();
        public:
            mcs_spin_lock() : tail_(nullptr)
            {
                assert(false && "mcs_spin_lock is unavailable now, please use spin_lock instead");
            }

            mcs_spin_lock(const mcs_spin_lock&) = delete;
            mcs_spin_lock(mcs_spin_lock&&) noexcept = delete;
            mcs_spin_lock& operator=(const mcs_spin_lock&) = delete;
            mcs_spin_lock& operator=(mcs_spin_lock&&) noexcept = delete;

            inline void lock(mcs_spin_lock::lock_node& node)
            {
                auto this_thread_id = bq::platform::thread::get_current_thread_id();
                auto old_value = node.owner_.exchange_relaxed(this_thread_id);
                assert((old_value == 0 || old_value == this_thread_id) && "a mcs_spin_lock::lock_node only can be used by one thread");
                if (node.lock_counter_.load_raw() > 0) {
                    node.lock_counter_.fetch_add_raw(1); // reentrant
                    return;
                }
                lock_node* prev_tail = tail_.exchange_seq_cst(&node);
                if (prev_tail) {
#if !defined(NDEBUG)
                    assert(prev_tail->next_.load_relaxed() == nullptr && "spin_lock init status error, next_ should be null");
#endif
                    prev_tail->next_.store_release(&node);
                    // The acquire and release memory orders provide memory synchronization semantics
                    // for the business logic protected by this lock, ensuring thread safety and data consistency.
                    while (node.lock_counter_.load_acquire() == 0) {
                        //yield();
                    }
                }
                else {
                    node.lock_counter_.fetch_add_raw(1); //only access in self thread in this case, so no need to use atomic operation
                }
            }

            inline void unlock(mcs_spin_lock::lock_node& node)
            {
                auto this_thread_id = bq::platform::thread::get_current_thread_id();
                auto old_value = node.owner_.exchange_relaxed(this_thread_id);
                assert((old_value == this_thread_id) && "a mcs_spin_lock::lock_node only can be used by one thread, and lock() must be called before unlock()");
                auto old_counter = node.lock_counter_.fetch_sub_raw(1);
                if (old_counter > 1) {
                    return; // reentrant
                }
                assert(old_counter >=1 && "mcs_spin_lock unlock time more than lock time");

                lock_node* next = node.next_.load_acquire();
                if (next == nullptr) {
                    lock_node* expected = &node;
                    if (tail_.compare_exchange_strong(expected, nullptr, bq::platform::memory_order::seq_cst, bq::platform::memory_order::acquire)) {
                        node.lock_counter_.store_raw(0);
                        node.next_.store_raw(nullptr);
                        return;
                    }
                    while ((next = node.next_.load_acquire()) == nullptr) {
                        //yield();
                    }
                }
                // The acquire and release memory orders provide memory synchronization semantics
                // for the business logic protected by this lock, ensuring thread safety and data consistency.
                next->lock_counter_.store_release(1);
                node.next_.store_raw(nullptr);
            }
        };

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
            uint64_t get_epoch();
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
                uint64_t start_epoch = get_epoch();
                while (true) {
                    counter_type previous_counter = counter_.get().fetch_add_acq_rel(1);
                    if (previous_counter >= 0) {
                        // read lock success.
                        break;
                    }
                    auto new_value = counter_.get().fetch_sub_relaxed(1);
                    if (get_epoch() > start_epoch + 5000) {
                        printf("Value 1 : %" PRId64 "\n", (int64_t)new_value);
                        fflush(stdout);
                       assert(false);
                    }
                    while (true) {
                        yield();
                        counter_type current_counter = counter_.get().load_acquire();
                        if (current_counter >= 0) {
                            break;
                        }
                        if (get_epoch() > start_epoch + 5000) {
                            printf("Value 2 : %" PRId64 "\n", (int64_t)new_value);
                            fflush(stdout);
                            assert(false);
                        }
                    }
                }
            }

            inline void read_unlock()
            {
                counter_type previous_counter = counter_.get().fetch_sub_acq_rel(1);
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                assert(previous_counter > 0 && "spin_lock_rw_crazy counter error");
#else
                (void)previous_counter;
#endif
            }

            inline void write_lock()
            {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                assert(bq::platform::thread::get_current_thread_id() != write_lock_thread_id_.load_seq_cst() && "spin_lock is not reentrant");
                write_lock_thread_id_.store_seq_cst(bq::platform::thread::get_current_thread_id());
#endif
                uint64_t start_epoch = get_epoch();
                while (true) {
                    counter_type expected_counter = 0;
                    if (counter_.get().compare_exchange_strong(expected_counter, write_lock_mark_value, bq::platform::memory_order::acq_rel, bq::platform::memory_order::relaxed)) {
                        break;
                    }
                    yield();
                    if (get_epoch() > start_epoch + 5000) {
                        printf("Value 3 : %" PRId64 "\n", (int64_t)expected_counter);
                        fflush(stdout);
                        assert(false);
                    }
                }
            }

            inline void write_unlock()
            {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                write_lock_thread_id_.store_seq_cst(0);
#endif
                uint64_t start_epoch = get_epoch();
                while (true) {
                    counter_type expected_counter = write_lock_mark_value;
                    if (counter_.get().compare_exchange_strong(expected_counter, 0, bq::platform::memory_order::release, bq::platform::memory_order::relaxed)) {
                        break;
                    }
                    if (get_epoch() > start_epoch + 5000) {
                        printf("Value 4 : %" PRId64 "\n", (int64_t)expected_counter);
                        fflush(stdout);
                        assert(false);
                    }
                }
            }
        };

        class scoped_mcs_spin_lock {
        private:
            mcs_spin_lock& lock_;
            mcs_spin_lock::lock_node node_;
        public:
            scoped_mcs_spin_lock() = delete;

            explicit scoped_mcs_spin_lock(mcs_spin_lock& lock)
                : lock_(lock)
            {
                lock_.lock(node_);
            }

            ~scoped_mcs_spin_lock()
            {
                lock_.unlock(node_);
            }
        };

        class scoped_spin_lock {
        private:
            spin_lock& lock_;

        public:
            scoped_spin_lock() = delete;

            explicit scoped_spin_lock(spin_lock& lock)
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

            explicit scoped_spin_lock_read_crazy(spin_lock_rw_crazy& lock)
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

            explicit scoped_spin_lock_write_crazy(spin_lock_rw_crazy& lock)
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
