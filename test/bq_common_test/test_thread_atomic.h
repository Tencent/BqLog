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
        constexpr uint32_t MAGIC_NUMBER = 0x4323;

        class test_thread_exist : public bq::platform::thread {
        public:
            bq::platform::thread::thread_id thread_id_ = 0;
            bq::platform::atomic<bool> is_started;
        protected:
            virtual void run() override {
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
            bq::array<uint32_t>* a_ptr = nullptr;
            test_atomic_struct<uint32_t>* i_ptr = nullptr;
            uint32_t cas_times_per_loop = 0;
            uint32_t base_value;

            test_thread_cas(test_atomic_struct<uint32_t>& i, bq::array<uint32_t>& a, uint32_t in_cas_times_per_loop, uint32_t in_base_value)
            {
                i_ptr = &i;
                a_ptr = &a;
                base_value = in_base_value;
                cas_times_per_loop = in_cas_times_per_loop;
            }

            virtual void run() override
            {
                for (uint32_t i = 0; i < TEST_THREAD_ATOMIC_LOOP_TIMES; ++i) {
                    uint32_t value = base_value;
                    auto magic_number_cpy = MAGIC_NUMBER;
                    while (!i_ptr->i.compare_exchange_strong(magic_number_cpy, value + 1, platform::memory_order::release)) {
                        magic_number_cpy = MAGIC_NUMBER;
                    }
                    a_ptr->push_back(value);
                    for (uint32_t j = 1; j < cas_times_per_loop; ++j) {
                        ++value;
                        if (i_ptr->i.compare_exchange_strong(value, value + 1, platform::memory_order::release)) {
                            a_ptr->push_back(value);
                        } else {
                            a_ptr->push_back(MAGIC_NUMBER); // imposible branch
                        }
                    }
                    i_ptr->i.store(MAGIC_NUMBER, platform::memory_order::release);
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

        class test_thread_spin_lock_read : public bq::platform::thread {
        private:
            test_result& result_;
            bq::platform::spin_lock_rw_crazy& spin_lock_;
            uint64_t& counter_;

        public:
            test_thread_spin_lock_read(test_result& result, bq::platform::spin_lock_rw_crazy& lock, uint64_t& counter)
                : result_(result)
                , spin_lock_(lock)
                , counter_(counter)
            {
            }
            virtual void run() override
            {
                for (uint32_t i = 0; i < 1000000; ++i) {
                    {
                        bq::platform::scoped_spin_lock_read_crazy lock(spin_lock_);
                        result_.add_result(counter_ % 10 == 0, "spin_lock_rw_crazy test");
                    }
                    bq::platform::thread::yield();
                }
            }
        };

        class test_thread_spin_lock_write : public bq::platform::thread {
        private:
            test_result& result_;
            bq::platform::spin_lock_rw_crazy& spin_lock_;
            uint64_t& counter_;

        public:
            test_thread_spin_lock_write(test_result& result, bq::platform::spin_lock_rw_crazy& lock, uint64_t& counter)
                : result_(result)
                , spin_lock_(lock)
                , counter_(counter)
            {
                (void)result_;
            }
            virtual void run() override
            {
                for (uint32_t i = 0; i < 1000000; ++i) {
                    {
                        bq::platform::scoped_spin_lock_write_crazy lock(spin_lock_);
                        counter_++;
                        counter_++;
                        counter_++;
                        counter_++;
                        counter_++;
                        ++counter_;
                        ++counter_;
                        ++counter_;
                        ++counter_;
                        ++counter_;
                    }
                    bq::platform::thread::yield();
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
                        return (i_ptr->i.load(platform::memory_order::release) % 2) == 0;
                    });
                    result_ptr_->add_result(i_ptr->i.load(platform::memory_order::acquire) == current_value, "condition variable test %d", current_value);
                    current_value += 2;
                    i_ptr->i.add_fetch(1, platform::memory_order::release);
                    mutex_ptr_->unlock();
                    condition_variable_ptr_->notify_one();
                }
            }
        };

        class test_thread_condition_variable_timeout : public bq::platform::thread {
        private:
            bq::platform::mutex mutex;
            bq::platform::condition_variable condition_variable;

        public:
            test_thread_condition_variable_timeout()
                : mutex(false)
            {
            }

            virtual void run() override
            {
                mutex.lock();
                condition_variable.wait_for(mutex, 5000);
                mutex.unlock();
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
                test_output_dynamic(bq::log_level::info, "atomic add test is finished, now begin the thread alive check test, please wait...                \r");
                {
                    for (int32_t i = 0; i < 1024; ++i) {
                        test_thread_exist thread1;
                        test_thread_exist thread2;
                        test_thread_exist thread3;
                        thread1.start();
                        thread2.start();
                        thread3.start();
                        while (!thread1.is_started.load(bq::platform::memory_order::acquire)) {
                            bq::platform::thread::yield();
                        }
                        result.add_result(bq::platform::thread::is_thread_alive(thread1.thread_id_), "thread alive test failed");
                        while (!thread2.is_started.load(bq::platform::memory_order::acquire)) {
                            bq::platform::thread::yield();
                        }
                        result.add_result(bq::platform::thread::is_thread_alive(thread2.thread_id_), "thread alive test failed");
                        while (!thread3.is_started.load(bq::platform::memory_order::acquire)) {
                            bq::platform::thread::yield();
                        }
                        result.add_result(bq::platform::thread::is_thread_alive(thread3.thread_id_), "thread alive test failed");
                        thread1.cancel();
                        thread2.cancel();
                        thread3.cancel();
                        thread1.join();
                        result.add_result(!bq::platform::thread::is_thread_alive(thread1.thread_id_), "thread alive test failed");
                        thread2.join();
                        result.add_result(!bq::platform::thread::is_thread_alive(thread2.thread_id_), "thread alive test failed");
                        thread3.join();
                        result.add_result(!bq::platform::thread::is_thread_alive(thread3.thread_id_), "thread alive test failed");
                    }
                }
                test_output_dynamic(bq::log_level::info, "thread alive check test is finished, now begin the cas test, please wait...                \r");
                {
                    // CAS test
                    constexpr uint32_t cas_times_per_loop = 5;
                    test_atomic_struct<uint32_t> i_value;
                    i_value.i.store_seq_cst(MAGIC_NUMBER);
                    bq::array<uint32_t> test_array;
                    test_array.set_capacity(TEST_THREAD_ATOMIC_LOOP_TIMES * cas_times_per_loop * 5);
                    test_thread_cas thread1(i_value, test_array, cas_times_per_loop, 0);
                    thread1.set_thread_name("thread1");
                    thread1.start();

                    test_thread_cas thread2(i_value, test_array, cas_times_per_loop, 10);
                    thread2.set_thread_name("thread2");
                    thread2.start();

                    test_thread_cas thread3(i_value, test_array, cas_times_per_loop, 20);
                    thread3.set_thread_name("thread3");
                    thread3.start();

                    test_thread_cas thread4(i_value, test_array, cas_times_per_loop, 30);
                    thread4.set_thread_name("thread4");
                    thread4.start();

                    test_thread_cas thread5(i_value, test_array, cas_times_per_loop, 40);
                    thread5.set_thread_name("thread5");
                    thread5.start();

                    thread1.join();
                    thread2.join();
                    thread3.join();
                    thread4.join();
                    thread5.join();

                    result.add_result(test_array.size() == TEST_THREAD_ATOMIC_LOOP_TIMES * cas_times_per_loop * 5, "atomic CAS test 1, final size:%d", test_array.size());
                    bool check_result = true;
                    uint32_t test_base = 0;
                    for (uint32_t i = 0; i < test_array.size(); ++i) {
                        uint32_t add_value = i % cas_times_per_loop;
                        if (add_value == 0) {
                            test_base = test_array[i];
                        }
                        if (test_array[i] - test_base != add_value) {
                            check_result = false;
                            break;
                        }
                    }

                    result.add_result(check_result, "atomic CAS test2");
                }
                test_output_dynamic(bq::log_level::info, "cas test is finished, now begin the thread cancel test, please wait...                \r");
                {
                    auto start_epoch_ms = static_cast<int64_t>(bq::platform::high_performance_epoch_ms());
                    for (uint32_t i = 0; i < 1000; ++i) {
                        bq::array<test_thread_cancel*> test_array;
                        test_array.fill_uninitialized(16);
                        for (size_t j = 0; j< test_array.size(); ++j) {
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
                    uint32_t test_base = 0;
                    for (uint32_t i = 0; i < test_array.size(); ++i) {
                        uint32_t add_value = i % cas_times_per_loop;
                        if (add_value == 0) {
                            test_base = test_array[i];
                        }
                        if (test_array[i] - test_base != add_value) {
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
                    uint32_t test_base = 0;
                    for (uint32_t i = 0; i < test_array.size(); ++i) {
                        uint32_t add_value = i % cas_times_per_loop;
                        if (add_value == 0) {
                            test_base = test_array[i];
                        }
                        if (test_array[i] - test_base != add_value) {
                            check_result = false;
                            break;
                        }
                    }
                    result.add_result(check_result, "atomic spin_lock test2");
                }

                test_output_dynamic(bq::log_level::info, "spin lock test is finished, now begin the spin_lock_rw_crazy test, please wait...                \r");
                {
                    uint64_t counter = 0;
                    bq::platform::spin_lock_rw_crazy test_lock;
                    test_thread_spin_lock_read thread1(result, test_lock, counter);
                    test_thread_spin_lock_read thread2(result, test_lock, counter);
                    test_thread_spin_lock_read thread3(result, test_lock, counter);
                    test_thread_spin_lock_read thread4(result, test_lock, counter);
                    test_thread_spin_lock_read thread5(result, test_lock, counter);

                    test_thread_spin_lock_write thread_write1(result, test_lock, counter);
                    test_thread_spin_lock_write thread_write2(result, test_lock, counter);
                    test_thread_spin_lock_write thread_write3(result, test_lock, counter);
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
                            return (i_value.i.load(platform::memory_order::release) % 2) == 1;
                        });
                        result.add_result(i_value.i.load(platform::memory_order::acquire) == current_value, "condition variable test %d", current_value);
                        current_value += 2;
                        i_value.i.add_fetch(1, platform::memory_order::release);
                        test_mutex.unlock();
                        test_cond.notify_one();
                    }
                    thread1.join();
                }
                {
                    test_thread_condition_variable_timeout thread1;
                    thread1.start();
                    auto start_time = bq::platform::high_performance_epoch_ms();
                    thread1.join();
                    auto end_time = bq::platform::high_performance_epoch_ms();
                    auto diff = end_time - start_time;
                    result.add_result(diff > 4000 && diff < 6000, "condition variable timeout test");
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
