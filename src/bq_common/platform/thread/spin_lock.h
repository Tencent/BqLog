#pragma once
/*
 * Copyright (C) 2025 Tencent.
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
#include "bq_common/bq_common_public_include.h"
#include "bq_common/platform/atomic/atomic.h"
#include "bq_common/platform/thread/thread.h"
#include "bq_common/utils/utility_types.h"
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

        public:
            mcs_spin_lock()
                : tail_(nullptr)
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
                (void)old_value; // avoid unused warning in release mode
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
                        // bq::platform::thread::cpu_relax();
                    }
                } else {
                    node.lock_counter_.fetch_add_raw(1); // only access in self thread in this case, so no need to use atomic operation
                }
            }

            inline void unlock(mcs_spin_lock::lock_node& node)
            {
                auto this_thread_id = bq::platform::thread::get_current_thread_id();
                auto old_value = node.owner_.exchange_relaxed(this_thread_id);
                (void)old_value; // avoid unused warning in release mode
                assert((old_value == this_thread_id) && "a mcs_spin_lock::lock_node only can be used by one thread, and lock() must be called before unlock()");
                auto old_counter = node.lock_counter_.fetch_sub_raw(1);
                if (old_counter > 1) {
                    return; // reentrant
                }
                assert(old_counter >= 1 && "mcs_spin_lock unlock time more than lock time");

                lock_node* next = node.next_.load_acquire();
                if (next == nullptr) {
                    lock_node* expected = &node;
                    if (tail_.compare_exchange_strong(expected, nullptr, bq::platform::memory_order::seq_cst, bq::platform::memory_order::acquire)) {
                        node.lock_counter_.store_raw(0);
                        node.next_.store_raw(nullptr);
                        return;
                    }
                    while ((next = node.next_.load_acquire()) == nullptr) {
                        // bq::platform::thread::cpu_relax();
                    }
                }
                // The acquire and release memory orders provide memory synchronization semantics
                // for the business logic protected by this lock, ensuring thread safety and data consistency.
                next->lock_counter_.store_release(1);
                node.next_.store_raw(nullptr);
            }
        };

        /// <summary>
        /// Base class of spin_lock, but it can be used as global variable and don't need to worry about
        /// the "Static Initialization Order Fiasco" problem, because it is zero initialized.
        /// </summary>
        class spin_lock_zero_init {
        protected:
            bq::platform::atomic_trivially_constructible<bool> value_; 
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
            bq::platform::atomic_trivially_constructible<bq::platform::thread::thread_id> thread_id_; 
#endif
        protected:
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
            bq::platform::atomic_trivially_constructible<bq::platform::thread::thread_id>& thread_id()
            {
                return thread_id_;
            }
#endif
        public:
            inline void lock()
            {
                while (true) {
                    if (!value_.exchange(true, bq::platform::memory_order::acquire)) {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                        thread_id().store(bq::platform::thread::get_current_thread_id(), bq::platform::memory_order::seq_cst);
#endif
                        break;
                    }
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                    assert(bq::platform::thread::get_current_thread_id() != thread_id().load(bq::platform::memory_order::seq_cst) && "spin_lock is not reentrant");
#endif
                    while (value_.load(bq::platform::memory_order::relaxed)) {
                        bq::platform::thread::cpu_relax();
                    }
                }
            }

            inline bool try_lock()
            {
                if (!value_.exchange(true, bq::platform::memory_order::acquire)) {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                    thread_id().store(bq::platform::thread::get_current_thread_id(), bq::platform::memory_order::seq_cst);
#endif
                    return true;
                }
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                assert(bq::platform::thread::get_current_thread_id() != thread_id().load(bq::platform::memory_order::seq_cst) && "spin_lock is not reentrant");
#endif
                return false;
            }

            inline void unlock()
            {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                thread_id().store(0, bq::platform::memory_order::seq_cst);
#endif
                value_.store(false, bq::platform::memory_order::release);
            }
        };
        static_assert(bq::is_trivially_constructible<spin_lock_zero_init>::value, "spin_lock_zero_init must be trivially constructible");
        static_assert(bq::is_trivially_destructible<spin_lock_zero_init>::value, "spin_lock_zero_init must be trivially destructible");

        class spin_lock : public spin_lock_zero_init {
        public:
            spin_lock()
            {
                value_.store_seq_cst(false);
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                thread_id_.store_seq_cst(0);
#endif
            }
            spin_lock(const spin_lock&) = delete;
            spin_lock(spin_lock&&) noexcept = delete;
            spin_lock& operator=(const spin_lock&) = delete;
        };

        /// <summary>
        /// This is an crazy optimized read-write spin lock,
        /// betting that you won't have more than INT32_MAX(32 bit) or INT64_MAX(64 bit) threads waiting to acquire the read lock.
        /// This version is designed for extreme performance of read locks when there is no write lock contention.
        /// warning: the write lock is not re-entrant.
        /// warning: starvation may happen, be aware!
        /// by pippocao
        /// 2024/7/8
        /// </summary>
        ///

        BQ_PACK_BEGIN
        class alignas(BQ_CACHE_LINE_SIZE) spin_lock_rw_crazy {
        public:
            typedef bq::condition_type_t<sizeof(void*) == 4, int32_t, int64_t> counter_type;
            static constexpr counter_type write_lock_mark_value = bq::condition_value<sizeof(void*) == 4, counter_type, (counter_type)INT32_MIN, (counter_type)INT64_MIN>::value;
            bq::platform::atomic<counter_type> counter_;
            char padding_[BQ_CACHE_LINE_SIZE - sizeof(counter_)];

        public:
            spin_lock_rw_crazy()
                : counter_(0)
            {
                (void)padding_;
            }

            spin_lock_rw_crazy(const spin_lock_rw_crazy&) = delete;
            spin_lock_rw_crazy(spin_lock_rw_crazy&&) noexcept = delete;
            spin_lock_rw_crazy& operator=(const spin_lock_rw_crazy&) = delete;
            spin_lock_rw_crazy& operator=(spin_lock_rw_crazy&&) noexcept = delete;

            inline void read_lock()
            {
                while (true) {
                    counter_type previous_counter = counter_.fetch_add_acq_rel(1);
                    if (previous_counter >= 0) {
                        // read lock success.
                        break;
                    }
                    counter_.fetch_sub_relaxed(1);
                    while (true) {
                        bq::platform::thread::cpu_relax();
                        counter_type current_counter = counter_.load_acquire();
                        if (current_counter >= 0) {
                            break;
                        }
                    }
                }
            }

            inline bool try_read_lock()
            {
                counter_type previous_counter = counter_.fetch_add_acq_rel(1);
                if (previous_counter >= 0) {
                    // read lock success.
                    return true;
                }
                counter_.fetch_sub_relaxed(1);
                return false;
            }

            inline void read_unlock()
            {
                counter_type previous_counter = counter_.fetch_sub_release(1);
                (void)previous_counter;
            }

            inline void write_lock()
            {
                while (true) {
                    counter_type expected_counter = 0;
                    if (counter_.compare_exchange_strong(expected_counter, write_lock_mark_value, bq::platform::memory_order::acq_rel, bq::platform::memory_order::acquire)) {
                        break;
                    }
                    bq::platform::thread::cpu_relax();
                }
            }

            inline bool try_write_lock()
            {
                counter_type expected_counter = 0;
                if (counter_.compare_exchange_strong(expected_counter, write_lock_mark_value, bq::platform::memory_order::acq_rel, bq::platform::memory_order::acquire)) {
                    return true;
                }
                return false;
            }

            inline void write_unlock()
            {
                while (true) {
                    counter_type expected_counter = write_lock_mark_value;
                    if (counter_.compare_exchange_strong(expected_counter, 0, bq::platform::memory_order::acq_rel, bq::platform::memory_order::acquire)) {
                        break;
                    }
                }
            }
        }
        BQ_PACK_END

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
            spin_lock_zero_init& lock_;

        public:
            scoped_spin_lock() = delete;

            explicit scoped_spin_lock(spin_lock_zero_init& lock)
                : lock_(lock)
            {
                lock_.lock();
            }

            ~scoped_spin_lock()
            {
                lock_.unlock();
            }
        };

        class scoped_try_spin_lock {
        private:
            spin_lock_zero_init& lock_;
            bool locked_;

        public:
            scoped_try_spin_lock() = delete;

            explicit scoped_try_spin_lock(spin_lock_zero_init& lock)
                : lock_(lock)
            {
                locked_ = lock_.try_lock();
            }

            bool owns_lock() const
            {
                return locked_;
            }

            ~scoped_try_spin_lock()
            {
                if (locked_) {
                    lock_.unlock();
                }
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

        class scoped_try_spin_lock_read_crazy {
        private:
            spin_lock_rw_crazy& lock_;
            bool locked_;

        public:
            scoped_try_spin_lock_read_crazy() = delete;

            explicit scoped_try_spin_lock_read_crazy(spin_lock_rw_crazy& lock)
                : lock_(lock)
            {
                locked_ = lock_.try_read_lock();
            }

            bool owns_lock() const
            {
                return locked_;
            }

            ~scoped_try_spin_lock_read_crazy()
            {
                if (locked_) {
                    lock_.read_unlock();
                }
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

        class scoped_try_spin_lock_write_crazy {
        private:
            spin_lock_rw_crazy& lock_;
            bool locked_;

        public:
            scoped_try_spin_lock_write_crazy() = delete;

            explicit scoped_try_spin_lock_write_crazy(spin_lock_rw_crazy& lock)
                : lock_(lock)
            {
                locked_ = lock_.try_write_lock();
            }

            bool owns_lock() const
            {
                return locked_;
            }

            ~scoped_try_spin_lock_write_crazy()
            {
                if (locked_) {
                    lock_.write_unlock();
                }
            }
        };
    }
}
