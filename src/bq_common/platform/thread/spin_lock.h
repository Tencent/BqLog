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

        // Similar to Linux MCS lock, but it is a simpler version.
        class spin_lock {
        private:
            struct lock_stack_node {
                bq::platform::atomic<bool> locked_ = true;
                bq::platform::atomic<lock_stack_node*> next_ = nullptr;
            };
        private:
            bq::platform::atomic<lock_stack_node*> tail_;
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
            bq::platform::atomic<bq::platform::thread::thread_id> thread_id_;
#endif
        private:
            void yield();
            bq_forceinline static lock_stack_node& get_lock_stack_node()
            {
                thread_local lock_stack_node stack_node_;
                return stack_node_;
            }
        public:
            spin_lock()
                : tail_(nullptr)
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
                lock_stack_node& node = get_lock_stack_node();
#if !defined(NDEBUG)
                assert((bq::platform::thread::get_current_thread_id() != thread_id_.load_seq_cst()) && "spin_lock is not reentrant");
                assert((node.next_.load_relaxed() == nullptr) && "spin_lock unit status error, next_ should be null");
                assert((node.locked_.load_relaxed()) && "spin_lock unit status error, it is locked");
#endif

                lock_stack_node* prev_tail = tail_.exchange_acquire(&node);
                if (prev_tail) {
#if !defined(NDEBUG)
                    assert(prev_tail->next_.load_relaxed() == nullptr && "spin_lock unit status error, next_ should be null");
#endif
                    prev_tail->next_.store_release(&node);
                    // The acquire and release memory orders provide memory synchronization semantics
                    // for the business logic protected by this lock, ensuring thread safety and data consistency.
                    while (node.locked_.load_acquire()) {
                        //yield();
                    }
                }
                else {
                    node.locked_.store_relaxed(false);
                }
#if !defined(NDEBUG)
                thread_id_.store_seq_cst(bq::platform::thread::get_current_thread_id());
#endif
            }

            inline void unlock()
            {
#if !defined(NDEBUG)
                assert((bq::platform::thread::get_current_thread_id() == thread_id_.load_seq_cst()) && "spin_lock thread id error");
                thread_id_.store_seq_cst(0);
#endif
                lock_stack_node& node = get_lock_stack_node();
                lock_stack_node* next = node.next_.load_acquire();
                if (next == nullptr) { 
                    lock_stack_node* expected = &node;
                    if (tail_.compare_exchange_strong(expected, nullptr, bq::platform::memory_order::relaxed, bq::platform::memory_order::relaxed)) {
                        node.locked_.store_relaxed(true);
                        node.next_.store_relaxed(nullptr);
                        return;
                    }
                    while ((next = node.next_.load_acquire()) == nullptr) {
                        //yield();
                    }
                }
                // The acquire and release memory orders provide memory synchronization semantics 
                // for the business logic protected by this lock, ensuring thread safety and data consistency.
                next->locked_.store_release(false);

                node.next_.store_relaxed(nullptr);
                node.locked_.store_relaxed(true);
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
                    counter_type previous_counter = counter_.get().fetch_add_acquire(1);
                    if (previous_counter >= 0) {
                        // read lock success.
                        break;
                    }
                    while (true) {
                        yield();
                        counter_type current_counter = counter_.get().load_relaxed();
                        if (current_counter >= 0) {
                            break;
                        }
                    }
                }
            }

            inline void read_unlock()
            {
                counter_type previous_counter = counter_.get().fetch_add_release(-1);
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
                write_lock_thread_id_.store_seq_cst(0);
#endif
                counter_.get().store_release(0);
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
