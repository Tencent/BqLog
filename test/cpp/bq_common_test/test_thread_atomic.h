#pragma once
#include "test_base.h"
#include "bq_common/bq_common.h"

namespace bq {
    namespace test {
        template <typename T>
        struct test_atomic_struct {
            bq::platform::atomic<T> i = 0;
        };

        constexpr uint32_t TEST_THREAD_ATOMIC_LOOP_TIMES = 1000000;

        class test_thread_exist : public bq::platform::thread {
        public:
            bq::platform::thread::thread_id thread_id_ = 0;
            bq::platform::atomic<bool> is_started;

        protected:
            virtual void run() override
            {
                thread_id_ = bq::platform::thread::get_current_thread_id();
                is_started.store(true, bq::platform::memory_order::release);
                while (!is_cancelled()) {
                    bq::platform::thread::yield();
                }
            }
        };

        class test_thread_add : public bq::platform::thread {
        public:
            test_atomic_struct<uint32_t>* i_ptr = nullptr;

            test_thread_add(test_atomic_struct<uint32_t>& i)
            {
                i_ptr = &i;
            }

            virtual void run() override
            {
                for (uint32_t i = 0; i < TEST_THREAD_ATOMIC_LOOP_TIMES; ++i) {
                    i_ptr->i.fetch_add(1, platform::memory_order::release);
                }
            }
        };

        class test_thread_cas : public bq::platform::thread {
        public:
            bq::array<uint32_t>& a_;
            bq::platform::atomic<uint32_t>& i_;

            test_thread_cas(test_atomic_struct<uint32_t>& i, bq::array<uint32_t>& a)
                : a_(a), i_(i.i)
            {
            }

            virtual void run() override
            {
                uint32_t expected_value = 0;
                while (true) {
                    if (expected_value >= TEST_THREAD_ATOMIC_LOOP_TIMES) {
                        break;
                    }
                    bool success = i_.compare_exchange_strong(expected_value, expected_value + 1, platform::memory_order::relaxed);
                    if (success) {
                        a_[expected_value]++;
                        expected_value = expected_value + 1;
                    }
                }
            }
        };

        class test_thread_cancel : public bq::platform::thread {
        public:
            bq::platform::atomic<bool> status_is_cancelled = false;
            virtual void run() override
            {
                const auto start_epoch_ms = static_cast<int64_t>(bq::platform::high_performance_epoch_ms());
                while (!is_cancelled()) {
                    bq::platform::thread::yield();
                    if (static_cast<int64_t>(bq::platform::high_performance_epoch_ms()) - start_epoch_ms > 5000) {
                        return;
                    }
                }
                status_is_cancelled = true;
            }
        };

        class test_thread_mutex : public bq::platform::thread {
        public:
            bq::array<uint32_t>* a_ptr = nullptr;
            test_atomic_struct<uint32_t>* i_ptr = nullptr;
            bq::platform::mutex* m_ptr = nullptr;
            uint32_t cas_times_per_loop = 0;
            uint32_t base_value;

            test_thread_mutex(test_atomic_struct<uint32_t>& i, bq::array<uint32_t>& a, uint32_t in_cas_times_per_loop, uint32_t in_base_value, bq::platform::mutex& mutex)
            {
                i_ptr = &i;
                a_ptr = &a;
                base_value = in_base_value;
                cas_times_per_loop = in_cas_times_per_loop;
                m_ptr = &mutex;
            }

            virtual void run() override
            {
                for (uint32_t i = 0; i < TEST_THREAD_ATOMIC_LOOP_TIMES; ++i) {
                    m_ptr->lock();
                    uint32_t value = base_value;
                    i_ptr->i.store_seq_cst(value);
                    for (uint32_t j = 0; j < cas_times_per_loop; ++j) {
                        a_ptr->push_back(i_ptr->i.fetch_add_seq_cst(1));
                        ++value;
                    }
                    m_ptr->unlock();
                }
            }
        };

        class test_thread_spin_lock : public bq::platform::thread {
        public:
            bq::array<uint32_t>* a_ptr = nullptr;
            test_atomic_struct<uint32_t>* i_ptr = nullptr;
            bq::platform::spin_lock* m_ptr = nullptr;
            uint32_t cas_times_per_loop = 0;
            uint32_t base_value;

            test_thread_spin_lock(test_atomic_struct<uint32_t>& i, bq::array<uint32_t>& a, uint32_t in_cas_times_per_loop, uint32_t in_base_value, bq::platform::spin_lock& mutex)
            {
                i_ptr = &i;
                a_ptr = &a;
                base_value = in_base_value;
                cas_times_per_loop = in_cas_times_per_loop;
                m_ptr = &mutex;
            }

            virtual void run() override
            {
                auto tid = bq::platform::thread::get_current_thread_id();
                for (uint32_t i = 0; i < TEST_THREAD_ATOMIC_LOOP_TIMES; ++i) {
                    if (i % 50000 == 0) {
                        test_output_dynamic_param(bq::log_level::info, "[spin_lock] thread %" PRIu64 " before lock, iteration %" PRIu32 "\n", tid, i);
                    }
                    m_ptr->lock();
                    if (i % 50000 == 0) {
                        test_output_dynamic_param(bq::log_level::info, "[spin_lock] thread %" PRIu64 " got lock, iteration %" PRIu32 "\n", tid, i);
                    }
                    uint32_t value = base_value;
                    i_ptr->i.store_seq_cst(value);
                    for (uint32_t j = 0; j < cas_times_per_loop; ++j) {
                        a_ptr->push_back(i_ptr->i.fetch_add_seq_cst(1));
                        ++value;
                    }
                    m_ptr->unlock();
                    if (i % 50000 == 0) {
                        test_output_dynamic_param(bq::log_level::info, "[spin_lock] thread %" PRIu64 " released lock, iteration %" PRIu32 "\n", tid, i);
                    }
                }
                test_output_dynamic_param(bq::log_level::info, "[spin_lock] thread %" PRIu64 " finished all iterations\n", tid);
            }
        };

        class test_thread_spin_lock_read : public bq::platform::thread {

        private:
            test_result& result_;
            bq::platform::spin_lock_rw_crazy& spin_lock_;
            uint64_t& counter_modify_by_write_;
            bq::platform::atomic<uint64_t>& counter_modify_by_read_;

        public:
            test_thread_spin_lock_read(test_result& result, bq::platform::spin_lock_rw_crazy& lock, uint64_t& counter_modify_by_write, bq::platform::atomic<uint64_t>& counter_modify_by_read)
                : result_(result)
                , spin_lock_(lock)
                , counter_modify_by_write_(counter_modify_by_write)
                , counter_modify_by_read_(counter_modify_by_read)
            {
            }
            virtual void run() override
            {
                uint32_t error_count = 0;
                for (uint32_t i = 0; i < 10000000; ++i) {
                    {
                        bq::platform::scoped_spin_lock_read_crazy lock(spin_lock_);
                        error_count += (counter_modify_by_write_ % 10 == 0) ? 0U : 1U;
                        counter_modify_by_read_.fetch_add_relaxed(1);
                        counter_modify_by_read_.fetch_add_relaxed(1);
                        counter_modify_by_read_.fetch_add_relaxed(1);
                        counter_modify_by_read_.fetch_add_relaxed(1);
                        counter_modify_by_read_.fetch_add_relaxed(1);
                        counter_modify_by_read_.add_fetch_relaxed(1);
                        counter_modify_by_read_.add_fetch_relaxed(1);
                        counter_modify_by_read_.add_fetch_relaxed(1);
                        counter_modify_by_read_.add_fetch_relaxed(1);
                        counter_modify_by_read_.add_fetch_relaxed(1);
                    }
                    bq::platform::thread::yield();
                }
                result_.add_result(error_count == 0, "spin_lock_rw_crazy test(read lock thread)");
            }
        };

        class test_thread_spin_lock_write : public bq::platform::thread {
        private:
            test_result& result_;
            bq::platform::spin_lock_rw_crazy& spin_lock_;
            uint64_t& counter_modify_by_write_;
            bq::platform::atomic<uint64_t>& counter_modify_by_read_;

        public:
            test_thread_spin_lock_write(test_result& result, bq::platform::spin_lock_rw_crazy& lock, uint64_t& counter_modify_by_write, bq::platform::atomic<uint64_t>& counter_modify_by_read)
                : result_(result)
                , spin_lock_(lock)
                , counter_modify_by_write_(counter_modify_by_write)
                , counter_modify_by_read_(counter_modify_by_read)
            {
                (void)result_;
            }
            virtual void run() override
            {
                uint32_t error_count = 0;
                for (uint32_t i = 0; i < 10000000; ++i) {
                    {
                        bq::platform::scoped_spin_lock_write_crazy lock(spin_lock_);
                        error_count += (counter_modify_by_read_.load_relaxed() % 10 == 0) ? 0U : 1U;
                        counter_modify_by_write_++;
                        counter_modify_by_write_++;
                        counter_modify_by_write_++;
                        counter_modify_by_write_++;
                        counter_modify_by_write_++;
                        ++counter_modify_by_write_;
                        ++counter_modify_by_write_;
                        ++counter_modify_by_write_;
                        ++counter_modify_by_write_;
                        ++counter_modify_by_write_;
                    }
                    bq::platform::thread::yield();
                    result_.add_result(error_count == 0, "spin_lock_rw_crazy test(write lock thread)");
                }
            }
        };

        class test_thread_name : public bq::platform::thread {
        private:
            test_result* result_ptr_;
            bq::string set_name_;

        public:
            test_thread_name(const bq::string& set_name, test_result& result)
            {
                result_ptr_ = &result;
                set_name_ = set_name;
            }

            virtual void run() override
            {
                result_ptr_->add_result(get_thread_name() == set_name_, "thread name test 1");
                result_ptr_->add_result(bq::platform::thread::get_current_thread_name() == set_name_, "thread name test 2");
            }
        };

        class test_thread_restart : public bq::platform::thread {
        public:
            bq::platform::atomic<int32_t> run_count_ = 0;
            bq::platform::thread::thread_id last_tid_ = 0;
            bq::platform::thread::thread_id last_obj_tid_ = 0;
            bq::string last_name_;
            virtual void run() override {
                run_count_.fetch_add_release(1);
                last_tid_ = bq::platform::thread::get_current_thread_id();
                last_obj_tid_ = get_thread_id();
                last_name_ = bq::platform::thread::get_current_thread_name();
            }
        };

        class test_thread_detach : public bq::platform::thread {
        public:
            bq::platform::atomic<bool>* flag_ptr = nullptr;
            virtual void run() override {
                bq::platform::thread::sleep(100);
                if (flag_ptr) flag_ptr->store_seq_cst(true);
            }
        };

        class test_thread_condition_variable : public bq::platform::thread {
        private:
            test_result* result_ptr_;
            test_atomic_struct<uint32_t>* i_ptr = nullptr;
            bq::platform::mutex* mutex_ptr_;
            bq::platform::condition_variable* condition_variable_ptr_;

        public:
            test_thread_condition_variable(test_result& result, test_atomic_struct<uint32_t>& i, bq::platform::mutex& mutex, bq::platform::condition_variable& condition_var)
            {
                result_ptr_ = &result;
                i_ptr = &i;
                mutex_ptr_ = &mutex;
                condition_variable_ptr_ = &condition_var;
            }

            virtual void run() override
            {
                uint32_t current_value = 0;
                while (i_ptr->i.load(platform::memory_order::acquire) < 10) {
                    mutex_ptr_->lock();
                    condition_variable_ptr_->wait(*mutex_ptr_, [&]() {
                        return (i_ptr->i.load(platform::memory_order::acquire) % 2) == 0;
                    });
                    result_ptr_->add_result(i_ptr->i.load(platform::memory_order::acquire) == current_value, "condition variable test %d", current_value);
                    current_value += 2;
                    i_ptr->i.add_fetch(1, platform::memory_order::release);
                    mutex_ptr_->unlock();
                    condition_variable_ptr_->notify_one();
                }
            }
        };

        class test_thread_condition_variable_waitfor : public bq::platform::thread {
        private:
            bq::platform::mutex mutex;
            bq::platform::condition_variable condition_variable;
            test_result* result_ptr_;
            bool predict;

        public:
            test_thread_condition_variable_waitfor(test_result& result, bool in_predict)
                : mutex(false)
                , predict(in_predict)
            {
                result_ptr_ = &result;
            }

            bq::platform::condition_variable& get_cond_var() { return condition_variable; }

            virtual void run() override
            {
                auto start_time = bq::platform::high_performance_epoch_ms();
                mutex.lock();
                bool result = condition_variable.wait_for(mutex, 5000, [&]() {
                    return predict;
                });
                mutex.unlock();
                result_ptr_->add_result(result == predict, "wait for time out test, %d", predict);
                auto end_time = bq::platform::high_performance_epoch_ms();
                if (!predict) {
                    auto diff = end_time - start_time;
                    result_ptr_->add_result(diff > 4000 && diff < 6000, "wait for time out test, timeout:%d", diff);
                }
            }
        };

        class test_thread_atomic : public test_base {
        public:
            virtual test_result test() override
            {
                test_output_dynamic(bq::log_level::info, "doing multi thread atomic add test, this may take some time, please wait....                \r");
                test_result result;
                {
                    // atomic test
                    test_atomic_struct<uint32_t> i_value;
                    test_thread_add thread1(i_value);
                    thread1.set_thread_name("thread1");
                    thread1.start();

                    test_thread_add thread2(i_value);
                    thread2.set_thread_name("thread2");
                    thread2.start();

                    test_thread_add thread3(i_value);
                    thread3.set_thread_name("thread3");
                    thread3.start();

                    test_thread_add thread4(i_value);
                    thread4.set_thread_name("thread4");
                    thread4.start();

                    test_thread_add thread5(i_value);
                    thread5.set_thread_name("thread5");
                    thread5.start();

                    thread1.join();
                    thread2.join();
                    thread3.join();
                    thread4.join();
                    thread5.join();

                    auto i_result = i_value.i.load(platform::memory_order::acquire);
                    result.add_result(i_result == TEST_THREAD_ATOMIC_LOOP_TIMES * 5, "atomic add test 1, final value:%d", i_result);
                }
                test_output_dynamic(bq::log_level::info, "atomic add test is finished, now begin the cas test, please wait...                \r");
                {
                    // CAS test
                    constexpr uint32_t task_number = 5;
                    test_atomic_struct<uint32_t> i_value;
                    i_value.i.store_seq_cst(0);
                    bq::array<uint32_t> test_array;
                    test_array.fill_uninitialized(TEST_THREAD_ATOMIC_LOOP_TIMES);
                    memset((uint32_t*)test_array.begin(), 0, sizeof(uint32_t) * test_array.size());
                    bq::array<bq::unique_ptr<test_thread_cas>> threads_array;
                    for (uint32_t i = 0; i < task_number; ++i) {
                        threads_array.emplace_back(bq::make_unique<test_thread_cas>(i_value, test_array));
                        threads_array[i]->start();
                    }

                    for (uint32_t i = 0; i < task_number; ++i) {
                        threads_array[i]->join();
                    }
                    bool check_result = true;
                    for (uint32_t i = 0; i < test_array.size(); ++i) {
                        if (test_array[i] != 1) {
                            check_result = false;
                        }
                    }
                    result.add_result(i_value.i.load_seq_cst() == TEST_THREAD_ATOMIC_LOOP_TIMES, "atomic CAS test1");
                    result.add_result(check_result, "atomic CAS test2");
                }
                test_output_dynamic(bq::log_level::info, "cas test is finished, now begin the thread cancel test, please wait...                \r");
                {
                    auto start_epoch_ms = static_cast<int64_t>(bq::platform::high_performance_epoch_ms());
                    for (uint32_t i = 0; i < 1000; ++i) {
                        bq::array<test_thread_cancel*> test_array;
                        test_array.fill_uninitialized(16);
                        for (size_t j = 0; j < test_array.size(); ++j) {
                            test_array[j] = new test_thread_cancel();
                            test_array[j]->start();
                            test_array[j]->cancel();
                        }
                        for (size_t j = 0; j < test_array.size(); ++j) {
                            test_array[j]->join();
                            result.add_result(test_array[j]->status_is_cancelled.load_seq_cst(), "thread cancel test");
                            delete test_array[j];
                        }
                        if (static_cast<int64_t>(bq::platform::high_performance_epoch_ms()) - start_epoch_ms > 15000) {
                            break;
                        }
                    }
                }
                test_output_dynamic(bq::log_level::info, "thread cancel test is finished, now begin the mutex test, please wait...                \r");
                {
                    // mutex test
                    constexpr uint32_t cas_times_per_loop = 5;
                    test_atomic_struct<uint32_t> i_value;
                    bq::array<uint32_t> test_array;
                    test_array.set_capacity(TEST_THREAD_ATOMIC_LOOP_TIMES * cas_times_per_loop * 5);
                    bq::platform::mutex test_mutex(false);
                    test_thread_mutex thread1(i_value, test_array, cas_times_per_loop, 0, test_mutex);
                    thread1.set_thread_name("thread1");
                    thread1.start();

                    test_thread_mutex thread2(i_value, test_array, cas_times_per_loop, 10, test_mutex);
                    thread2.set_thread_name("thread2");
                    thread2.start();

                    test_thread_mutex thread3(i_value, test_array, cas_times_per_loop, 20, test_mutex);
                    thread3.set_thread_name("thread3");
                    thread3.start();

                    test_thread_mutex thread4(i_value, test_array, cas_times_per_loop, 30, test_mutex);
                    thread4.set_thread_name("thread4");
                    thread4.start();

                    test_thread_mutex thread5(i_value, test_array, cas_times_per_loop, 40, test_mutex);
                    thread5.set_thread_name("thread5");
                    thread5.start();

                    thread1.join();
                    thread2.join();
                    thread3.join();
                    thread4.join();
                    thread5.join();

                    result.add_result(test_array.size() == TEST_THREAD_ATOMIC_LOOP_TIMES * cas_times_per_loop * 5, "atomic mutex test 1, final size:%d", test_array.size());
                    bool check_result = true;
                    uint32_t test_base_value = 0;
                    for (uint32_t i = 0; i < test_array.size(); ++i) {
                        uint32_t add_value = i % cas_times_per_loop;
                        if (add_value == 0) {
                            test_base_value = test_array[i];
                        }
                        if (test_array[i] - test_base_value != add_value) {
                            check_result = false;
                            break;
                        }
                    }
                    result.add_result(check_result, "atomic mutex test2");
                }
                test_output_dynamic(bq::log_level::info, "mutex test is finished, now begin the spin_lock test, please wait...                \r");
                {
                    // spin_lock test
                    constexpr uint32_t cas_times_per_loop = 5;
                    test_atomic_struct<uint32_t> i_value;
                    bq::array<uint32_t> test_array;
                    test_array.set_capacity(TEST_THREAD_ATOMIC_LOOP_TIMES * cas_times_per_loop * 5);
                    bq::platform::spin_lock test_lock;
                    test_thread_spin_lock thread1(i_value, test_array, cas_times_per_loop, 0, test_lock);
                    thread1.set_thread_name("thread1");
                    thread1.start();

                    test_thread_spin_lock thread2(i_value, test_array, cas_times_per_loop, 10, test_lock);
                    thread2.set_thread_name("thread2");
                    thread2.start();

                    test_thread_spin_lock thread3(i_value, test_array, cas_times_per_loop, 20, test_lock);
                    thread3.set_thread_name("thread3");
                    thread3.start();

                    test_thread_spin_lock thread4(i_value, test_array, cas_times_per_loop, 30, test_lock);
                    thread4.set_thread_name("thread4");
                    thread4.start();

                    test_thread_spin_lock thread5(i_value, test_array, cas_times_per_loop, 40, test_lock);
                    thread5.set_thread_name("thread5");
                    thread5.start();

                    thread1.join();
                    thread2.join();
                    thread3.join();
                    thread4.join();
                    thread5.join();

                    result.add_result(test_array.size() == TEST_THREAD_ATOMIC_LOOP_TIMES * cas_times_per_loop * 5, "atomic mutex test 1, final size:%d", test_array.size());
                    bool check_result = true;
                    uint32_t test_base_value = 0;
                    for (uint32_t i = 0; i < test_array.size(); ++i) {
                        uint32_t add_value = i % cas_times_per_loop;
                        if (add_value == 0) {
                            test_base_value = test_array[i];
                        }
                        if (test_array[i] - test_base_value != add_value) {
                            check_result = false;
                            break;
                        }
                    }
                    result.add_result(check_result, "atomic spin_lock test2");
                }

                test_output_dynamic(bq::log_level::info, "spin lock test is finished, now begin the spin_lock_rw_crazy test, please wait...                \r");
                {
                    bq::platform::atomic<uint64_t> counter_modify_by_read = 0;
                    uint64_t counter_modify_by_write = 0;
                    bq::platform::spin_lock_rw_crazy test_lock;
                    test_thread_spin_lock_read thread1(result, test_lock, counter_modify_by_write, counter_modify_by_read);
                    test_thread_spin_lock_read thread2(result, test_lock, counter_modify_by_write, counter_modify_by_read);
                    test_thread_spin_lock_read thread3(result, test_lock, counter_modify_by_write, counter_modify_by_read);
                    test_thread_spin_lock_read thread4(result, test_lock, counter_modify_by_write, counter_modify_by_read);
                    test_thread_spin_lock_read thread5(result, test_lock, counter_modify_by_write, counter_modify_by_read);

                    test_thread_spin_lock_write thread_write1(result, test_lock, counter_modify_by_write, counter_modify_by_read);
                    test_thread_spin_lock_write thread_write2(result, test_lock, counter_modify_by_write, counter_modify_by_read);
                    test_thread_spin_lock_write thread_write3(result, test_lock, counter_modify_by_write, counter_modify_by_read);
                    thread1.start();
                    thread2.start();
                    thread3.start();
                    thread4.start();
                    thread5.start();
                    thread_write1.start();
                    thread_write2.start();
                    thread_write3.start();

                    thread1.join();
                    thread2.join();
                    thread3.join();
                    thread4.join();
                    thread5.join();
                    thread_write1.join();
                    thread_write2.join();
                    thread_write3.join();
                }

                test_output_dynamic(bq::log_level::info, "spin_lock_rw_crazy test is finished, now begin the condition variable test, please wait...                \r");
                {
                    test_atomic_struct<uint32_t> i_value;
                    bq::platform::mutex test_mutex(false);
                    bq::platform::condition_variable test_cond;
                    test_thread_condition_variable thread1(result, i_value, test_mutex, test_cond);
                    thread1.start();

                    uint32_t current_value = 1;
                    while (i_value.i.load(platform::memory_order::acquire) < 10) {
                        test_mutex.lock();
                        test_cond.wait(test_mutex, [&]() {
                            return (i_value.i.load(platform::memory_order::acquire) % 2) == 1;
                        });
                        result.add_result(i_value.i.load(platform::memory_order::acquire) == current_value, "condition variable test %d", current_value);
                        current_value += 2;
                        i_value.i.add_fetch(1, platform::memory_order::release);
                        test_mutex.unlock();
                        test_cond.notify_one();
                    }
                    thread1.join();
                }

#ifndef BQ_IN_GITHUB_ACTIONS
                //Condition variable timeouts are significantly prolonged and inaccurate due to severe time dilation 
                //in the non-hardware-accelerated virtual machine environment
                {
                    constexpr uint64_t precision_ms = 500;
                    constexpr uint64_t sleep_time_ms = 5000;
                    constexpr uint64_t min_sleep_time_ms = sleep_time_ms - precision_ms;
                    constexpr uint64_t max_sleep_time_ms = sleep_time_ms + precision_ms;
                    for (int32_t i = 0; i < 5; ++i) {
                        auto start_time = bq::platform::high_performance_epoch_ms();
                        bq::platform::mutex condition_timeout_mutex_;
                        bq::platform::condition_variable condition_timeout_variable_;
                        condition_timeout_mutex_.lock();
                        condition_timeout_variable_.wait_for(condition_timeout_mutex_, sleep_time_ms);
                        condition_timeout_mutex_.unlock();
                        auto end_time = bq::platform::high_performance_epoch_ms();
                        auto diff = end_time - start_time;
                        result.add_result(diff > min_sleep_time_ms && diff < max_sleep_time_ms, "condition variable timeout test, real time elapsed:%" PRIu64 "ms, expect (%" PRIu64 ", %" PRIu64 ")ms", diff, max_sleep_time_ms, min_sleep_time_ms);
                    }
                    for (int32_t i = 0; i < 5; ++i) {
                        auto start_time = bq::platform::high_performance_epoch_ms();
                        bq::platform::thread::sleep(sleep_time_ms);
                        auto end_time = bq::platform::high_performance_epoch_ms();
                        auto diff = end_time - start_time;
                        result.add_result(diff > min_sleep_time_ms && diff < max_sleep_time_ms, "sleep test, real time elapsed:%" PRIu64 "ms, expect (%" PRIu64 ", %" PRIu64 ")ms", diff, max_sleep_time_ms, min_sleep_time_ms);
                    }
                }

                {
                    test_thread_condition_variable_waitfor thread_true(result, true);
                    test_thread_condition_variable_waitfor thread_false(result, false);
                    thread_true.start();
                    thread_false.start();
                    for (int32_t i = 0; i < 100; ++i) {
                        bq::platform::thread::sleep(60);
                        thread_true.get_cond_var().notify_all();
                        thread_false.get_cond_var().notify_all();
                    }
                    if (thread_true.get_status() == platform::enum_thread_status::running) {
                        thread_true.join();
                    }
                    if (thread_false.get_status() == platform::enum_thread_status::running) {
                        thread_false.join();
                    }
                }
#endif

                {
                    // Restart Test
                    test_thread_restart t_restart;
                    t_restart.set_thread_name("Run1");
                    t_restart.start();
                    t_restart.join();
                    result.add_result(t_restart.run_count_.load() == 1, "thread restart test 1");
                    result.add_result(t_restart.last_name_ == "Run1", "thread name test Run1");
                    auto tid1 = t_restart.last_tid_;
                    result.add_result(tid1 == t_restart.last_obj_tid_, "thread id member check 1");

                    t_restart.set_thread_name("Run2");
                    t_restart.start(); // Restart!
                    t_restart.join();
                    result.add_result(t_restart.run_count_.load() == 2, "thread restart test 2");
                    result.add_result(t_restart.last_name_ == "Run2", "thread name test Run2");
                    auto tid2 = t_restart.last_tid_;
                    result.add_result(tid2 == t_restart.last_obj_tid_, "thread id member check 2");
                    
                    // On POSIX, thread ID (pthread_t) can be reused immediately after join.
                    // So we cannot strictly assert tid1 != tid2.
                    // We only care that thread_id_ member is updated to the current running thread's ID (verified above).
                    if (tid1 == tid2) {
                        bq::util::log_device_console(bq::log_level::info, "Thread ID was reused by OS: %" PRIu64, (uint64_t)tid1);
                    }
                }

                {
                    // Detach Test
                    // We only verify that detach() sets the status to detached.
                    // We do NOT call start() again because it is designed to crash (assert).
                    bq::platform::atomic<bool> flag = false;
                    test_thread_detach t_detach;
                    t_detach.flag_ptr = &flag;
                    t_detach.start();
                    t_detach.detach();
                    
                    result.add_result(t_detach.get_status() == bq::platform::enum_thread_status::detached, "thread status detached test");

                    while (!flag.load()) {
                        bq::platform::thread::sleep(50);
                    }
                    bq::platform::thread::sleep(100);
                }

                test_output_dynamic(bq::log_level::info, "                                                                                                          \r");

#ifndef BQ_WIN
                {
                    // thread name test
                    bq::string thread_name = "test_thread_1";
                    test_thread_name test_thread(thread_name, result);
                    test_thread.set_thread_name(thread_name);
                    test_thread.start();
                    test_thread.join();
                }
#endif // !BQ_WIN

                return result;
            }
        };
    }
}
