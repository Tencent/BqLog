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
#include <stdio.h>
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
                        // yield();
                    }
                } else {
                    node.lock_counter_.fetch_add_raw(1); // only access in self thread in this case, so no need to use atomic operation
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
                        // yield();
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

            inline bool try_lock()
            {
                if (!value_.get().exchange(true, bq::platform::memory_order::acquire)) {
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                    thread_id_.store(bq::platform::thread::get_current_thread_id(), bq::platform::memory_order::seq_cst);
#endif
                    return true;
                }
#if !defined(NDEBUG) || defined(BQ_UNIT_TEST)
                assert(bq::platform::thread::get_current_thread_id() != thread_id_.load(bq::platform::memory_order::seq_cst) && "spin_lock is not reentrant");
#endif
                return false;
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
        ///

        class spin_lock_rw_crazy {
        private:
            static constexpr uint32_t timeout_ms = 60000U;
            static constexpr int32_t write_lock_mark_value = INT32_MIN;
            struct st_meta {
            private:
                uint64_t misc_;
                bq::platform::atomic<int32_t> writers_wait_counter_;
            public:
                st_meta() : misc_(0), writers_wait_counter_(0){}
                bq::platform::atomic<int32_t>& get_reader_counter()
                {
                    return *static_cast<bq::platform::atomic<int32_t>*>(static_cast<void*>(&misc_));;
                }
                bq::platform::atomic<uint64_t>& get_writer_counter()
                {
                    return *static_cast<bq::platform::atomic<uint64_t>*>(static_cast<void*>(&misc_));
                }
                bq::platform::atomic<int32_t>& get_writers_wait_counter()
                {
                    return writers_wait_counter_;
                }
                bq_forceinline static uint64_t generate_write_counter_value(int32_t reader_counter, uint32_t ticket)
                {
                    uint64_t result;
                    *static_cast<bq::platform::atomic<int32_t>*>(static_cast<void*>(&result)) = reader_counter;
                    *reinterpret_cast<bq::platform::atomic<uint32_t>*>(reinterpret_cast<char*>(&result) + sizeof(int32_t)) = ticket;
                    return result;
                }
            };
            bq::cache_friendly_type<bq::aligned_type<st_meta, sizeof(uint64_t)>> meta_;
            bq::platform::atomic<uint32_t> ticket_seq_;
#if !defined(NDEBUG)
            bq::platform::spin_lock lock_;
            struct debug_record {
                bq::platform::thread::thread_id tid_;
                bool is_write_;
                int32_t phase_;
                bool operator==(const debug_record& other) const
                {
                    return tid_ == other.tid_ && is_write_ == other.is_write_ && phase_ == other.phase_;
                }
            };
            bq::array<debug_record> record_;
#endif
        private:
            void yield();
            void wait();
            uint64_t get_epoch();
            bq_forceinline st_meta& get_meta()
            {
                return meta_.get().get();
            }

        public:
            spin_lock_rw_crazy(): meta_(st_meta()), ticket_seq_(0)
            {
            }

            spin_lock_rw_crazy(const spin_lock_rw_crazy&) = delete;
            spin_lock_rw_crazy(spin_lock_rw_crazy&&) noexcept = delete;
            spin_lock_rw_crazy& operator=(const spin_lock_rw_crazy&) = delete;
            spin_lock_rw_crazy& operator=(spin_lock_rw_crazy&&) noexcept = delete;

#if !defined(NDEBUG)
            void debug_output(int32_t pos)
            {
                printf("record pos:%d, %" PRIu32 ":[\n", pos, get_meta().get_writers_wait_counter().load_seq_cst());
                for (auto item : record_) {
                    printf("\t%" PRIu64 ", %s, %d\n", static_cast<uint64_t>(item.tid_), item.is_write_ ? "true" : "false", item.phase_);
                }
                printf("]n");
                fflush(stdout);
            }
#endif

            inline void read_lock()
            {
#if !defined(NDEBUG)
                uint64_t start_epoch = get_epoch();
                auto id = bq::platform::thread::get_current_thread_id();
                lock_.lock();
                auto reentrant_iter = record_.find_if([id](const debug_record& item) { return item.tid_ == id && !item.is_write_; });
                assert(reentrant_iter == record_.end() && "spin_lock_rw_crazy is not reentrant");
                record_.push_back(debug_record { id, false, 0 });
                lock_.unlock();
                assert(get_meta().get_writers_wait_counter().load_relaxed() >= 0 && "invalid writers_wait_counter_.get()");
#endif
                while (true) {
                    while (get_meta().get_writers_wait_counter().load_relaxed() > 0) {
                        wait();
                    }
                    int32_t previous_counter = get_meta().get_reader_counter().fetch_add_acq_rel(1);
                    if (previous_counter >= 0) {
                        // read lock success.
                        break;
                    }
                    get_meta().get_reader_counter().fetch_sub_relaxed(1);
#if !defined(NDEBUG)
                    if (get_epoch() > start_epoch + timeout_ms) {
                        debug_output(1);
                        assert(false);
                    }
#endif
                    while (true) {
                        yield();
                        int32_t current_counter = get_meta().get_reader_counter().load_acquire();
                        if (current_counter >= 0) {
                            break;
                        }
#if !defined(NDEBUG)
                        if (get_epoch() > start_epoch + timeout_ms) {
                            debug_output(2);
                            assert(false);
                        }
#endif
                    }
                }
#if !defined(NDEBUG)
                lock_.lock();
                auto iter = record_.find(debug_record { id, false, 0 });
                assert(iter != record_.end());
                iter->phase_ = 1;
                lock_.unlock();
#endif
            }

            inline bool try_read_lock()
            {
#if !defined(NDEBUG)
                auto id = bq::platform::thread::get_current_thread_id();
                lock_.lock();
                auto reentrant_iter = record_.find_if([id](const debug_record& item) { return item.tid_ == id && !item.is_write_; });
                assert(reentrant_iter == record_.end() && "spin_lock_rw_crazy is not reentrant");
                record_.push_back(debug_record { id, false, 0 });
                lock_.unlock();
                assert(get_meta().get_writers_wait_counter().load_relaxed() >= 0 && "invalid writers_wait_counter_.get()");
#endif
                while (get_meta().get_writers_wait_counter().load_acquire() > 0) {
                    wait();
                }
                int32_t previous_counter = get_meta().get_reader_counter().fetch_add_acq_rel(1);
                if (previous_counter >= 0) {
                    // read lock success.
#if !defined(NDEBUG)
                    lock_.lock();
                    auto iter = record_.find(debug_record { id, false, 0 });
                    assert(iter != record_.end());
                    iter->phase_ = 1;
                    lock_.unlock();
#endif
                    return true;
                }
                get_meta().get_reader_counter().fetch_sub_relaxed(1);
#if !defined(NDEBUG)
                lock_.lock();
                auto iter = record_.find(debug_record { id, false, 0 });
                assert(iter != record_.end());
                record_.erase(iter);
                lock_.unlock();
#endif
                return false;
            }

            inline void read_unlock()
            {
#if !defined(NDEBUG)
                auto id = bq::platform::thread::get_current_thread_id();
                lock_.lock();
                auto iter = record_.find({ id, false, 1 });
                assert(iter != record_.end());
                iter->phase_ = 2;
                lock_.unlock();
#endif

                int32_t previous_counter = get_meta().get_reader_counter().fetch_sub_release(1);
#if !defined(NDEBUG)
                assert(previous_counter > 0 && "spin_lock_rw_crazy counter error");
                lock_.lock();
                iter = record_.find({ id, false, 2 });
                assert(iter != record_.end());
                record_.erase(iter);
                lock_.unlock();
#else
                (void)previous_counter;
#endif
            }

            inline void write_lock()
            {
#if !defined(NDEBUG)
                auto id = bq::platform::thread::get_current_thread_id();
                lock_.lock();
                auto reentrant_iter = record_.find_if([id](const debug_record& item) { return item.tid_ == id; });
                assert(reentrant_iter == record_.end() && "spin_lock_rw_crazy is not reentrant");
                record_.push_back(debug_record { id, true, 0 });
                lock_.unlock();
                uint64_t start_epoch = get_epoch();
#endif
                get_meta().get_writers_wait_counter().fetch_add_release(1);
                auto my_ticket = ticket_seq_.fetch_add_relaxed(1);
                while (true) {
                    uint64_t expected_counter =  st_meta::generate_write_counter_value(0, my_ticket);
                    uint64_t write_lock_target_value = st_meta::generate_write_counter_value(write_lock_mark_value, my_ticket + 1);
                    if (get_meta().get_writer_counter().compare_exchange_strong(expected_counter
                                            , write_lock_target_value
                                            , bq::platform::memory_order::acq_rel
                                            , bq::platform::memory_order::acquire)) {
                        break;
                    }
                    yield();
#if !defined(NDEBUG)
                    if (get_epoch() > start_epoch + timeout_ms) {
                        debug_output(3);
                        assert(false);
                    }
#endif
                }
                get_meta().get_writers_wait_counter().fetch_sub_relaxed(1);
#if !defined(NDEBUG)
                lock_.lock();
                auto iter = record_.find({ id, true, 0 });
                assert(iter != record_.end());
                iter->phase_ = 1;
                lock_.unlock();
#endif
            }

            inline bool try_write_lock()
            {
#if !defined(NDEBUG)
                auto id = bq::platform::thread::get_current_thread_id();
                lock_.lock();
                auto reentrant_iter = record_.find_if([id](const debug_record& item) { return item.tid_ == id; });
                assert(reentrant_iter == record_.end() && "spin_lock_rw_crazy is not reentrant");
                record_.push_back(debug_record { id, true, 0 });
                lock_.unlock();
#endif
                int32_t expected_counter =  0;
                int32_t write_lock_target_value = write_lock_mark_value;
                if (get_meta().get_reader_counter().compare_exchange_strong(expected_counter
                                                , write_lock_target_value
                                                , bq::platform::memory_order::acq_rel
                                                , bq::platform::memory_order::acquire)) {
#if !defined(NDEBUG)
                    lock_.lock();
                    auto iter = record_.find({ id, true, 0 });
                    assert(iter != record_.end());
                    iter->phase_ = 1;
                    lock_.unlock();
#endif
                    return true;
                }
#if !defined(NDEBUG)
                lock_.lock();
                auto iter = record_.find({ id, true, 0 });
                assert(iter != record_.end());
                record_.erase(iter);
                lock_.unlock();
#endif
                return false;
            }

            inline void write_unlock()
            {
#if !defined(NDEBUG)
                auto id = bq::platform::thread::get_current_thread_id();
                lock_.lock();
                auto iter = record_.find({ id, true, 1 });
                assert(iter != record_.end());
                iter->phase_ = 2;
                lock_.unlock();
                uint64_t start_epoch = get_epoch();
#endif

                while (true) {
                    int32_t expected_counter = write_lock_mark_value;
                    if (get_meta().get_reader_counter().compare_exchange_strong(
                                                expected_counter
                                                , 0
                                                , bq::platform::memory_order::acq_rel
                                                , bq::platform::memory_order::acquire)) {
                        break;
                    }
#if !defined(NDEBUG)
                    if (get_epoch() > start_epoch + timeout_ms) {
                        debug_output(4);
                        assert(false);
                    }
#endif
                }

#if !defined(NDEBUG)
                lock_.lock();
                iter = record_.find({ id, true, 2 });
                assert(iter != record_.end());
                record_.erase(iter);
                lock_.unlock();
#endif
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

        class scoped_try_spin_lock {
        private:
            spin_lock& lock_;
            bool locked_;

        public:
            scoped_try_spin_lock() = delete;

            explicit scoped_try_spin_lock(spin_lock& lock)
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
