#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include "test_base.h"
#include "bq_common/bq_common.h"

struct test_struct {
    char b;
    bq::aligned_type<uint64_t, 64> l;
};

struct test_struct2 {
    int32_t i;
    const char* c;
};

struct test_struct3 {
    int32_t i;
    int64_t l;
};

struct test_pod_struct1 {
    int32_t i;
    int32_t* p;
};

struct test_pod_struct2 {
    int32_t i;
    int32_t* p;
    test_pod_struct2()
    {
        i = 0;
        p = (int32_t*)malloc(sizeof(int32_t));
    }

    ~test_pod_struct2()
    {
        free(p);
    }
};

static int32_t unique_ptr_counter = 0;
struct test_unique_ptr {
    test_unique_ptr()
    {
        ++unique_ptr_counter;
    }
    ~test_unique_ptr()
    {
        --unique_ptr_counter;
    }
};

namespace bq {
    namespace test {

        class test_basic_type : public test_base {
        public:
            virtual test_result test() override
            {
                test_result result;
                bq::aligned_type<uint32_t, 64> type_uint32_t = static_cast<uint32_t>(50);
                result.add_result((size_t)&type_uint32_t.get() % 64 == 0, "bq::aligned_type<uint32_t, 64> on stack");

                bq::aligned_type<char, 64> type_char = 'a';
                result.add_result((size_t)&type_char.get() % 64 == 0, "bq::aligned_type<char, 64> on stack");

                for (int32_t i = 0; i < 1024; ++i) {
                    bq::aligned_type<char, 64>* p_char = new bq::aligned_type<char, 64>();
                    result.add_result((size_t)&p_char->get() % 64 == 0, "bq::aligned_type<char, 64> on heap");
                    delete p_char;

                    bq::aligned_type<std::string, 64>* p_str = new bq::aligned_type<std::string, 64>();
                    result.add_result((size_t)&p_str->get() % 64 == 0, "bq::aligned_type<std::string, 64> on heap");
                    delete p_str;

                    test_struct* ts = new test_struct();
                    result.add_result((size_t)&ts->l.get() % 64 == 0, "bq::aligned_type<uint64_t, 64> in struct and on heap");
                    delete ts;
                }
                {
                    typedef bq::aligned_type<bq::string, 32> test_align_string_type;
                    constexpr size_t test_align_string_type_size = sizeof(test_align_string_type) + 128;

#ifdef BQ_WIN
                    constexpr size_t offset1 = 1;
                    constexpr size_t offset2 = 5;
                    constexpr size_t offset3 = 17;
#else
                    constexpr size_t offset1 = 8;
                    constexpr size_t offset2 = 16;
                    constexpr size_t offset3 = 32;
#endif

                    char tmp1_cache[test_align_string_type_size];
                    test_align_string_type* tmp1_ptr = (test_align_string_type*)tmp1_cache;
                    char tmp2_cache[test_align_string_type_size];
                    test_align_string_type* tmp2_ptr = (test_align_string_type*)(tmp2_cache + offset1);
                    char tmp3_cache[test_align_string_type_size];
                    test_align_string_type* tmp3_ptr = (test_align_string_type*)(tmp3_cache + offset2);
                    char tmp4_cache[test_align_string_type_size];
                    test_align_string_type* tmp4_ptr = (test_align_string_type*)(tmp4_cache + offset3);

                    const bq::string str_template = "this is template";
                    bq::string str_rhs = str_template;

                    new (tmp1_ptr) test_align_string_type();
                    new (tmp2_ptr) test_align_string_type(str_template);
                    new (tmp3_ptr) test_align_string_type();
                    *tmp3_ptr = *tmp2_ptr;
                    new (tmp4_ptr) test_align_string_type(bq::move(str_rhs));

                    result.add_result((size_t)&tmp1_ptr->get() % 16 == 0, "bq::aligned_type placement new test alignment 1");
                    result.add_result((size_t)&tmp2_ptr->get() % 16 == 0, "bq::aligned_type placement new test alignment 2");
                    result.add_result((size_t)&tmp3_ptr->get() % 16 == 0, "bq::aligned_type placement new test alignment 3");
                    result.add_result((size_t)&tmp4_ptr->get() % 16 == 0, "bq::aligned_type placement new test alignment 4");

                    result.add_result(str_rhs.is_empty(), "bq::aligned_type right reference constructor test 1");

                    result.add_result(tmp1_ptr->get().is_empty(), "bq::aligned_type constructor test 1");
                    result.add_result(tmp2_ptr->get() == str_template, "bq::aligned_type constructor test 2");
                    result.add_result(tmp3_ptr->get() == str_template, "bq::aligned_type constructor test 3");
                    result.add_result(tmp4_ptr->get() == str_template, "bq::aligned_type constructor test 4");

                    *tmp3_ptr = bq::move(*tmp4_ptr);
                    result.add_result(tmp3_ptr->get() == str_template, "bq::aligned_type right reference operator= test 1");
                    result.add_result(tmp4_ptr->get().is_empty(), "bq::aligned_type right reference operator= test 2");

                    bq::object_destructor<test_align_string_type>::destruct(tmp1_ptr);
                    bq::object_destructor<test_align_string_type>::destruct(tmp2_ptr);
                    bq::object_destructor<test_align_string_type>::destruct(tmp3_ptr);
                    bq::object_destructor<test_align_string_type>::destruct(tmp4_ptr);
                }

                result.add_result(bq::is_pod<char*>::value, "bq::is_pod<char*>::value");
                result.add_result(!bq::is_pod<std::string>::value, "bq::is_pod<std::string>::value");

                char char_array[] = { '2', '0', 'f' };
                result.add_result(bq::is_pod<decltype(char_array)>::value, "bool pod");

                result.add_result(bq::is_pod<test_pod_struct1>::value, "test_pod_struct1 pod");
                result.add_result(!bq::is_pod<test_pod_struct2>::value, "test_pod_struct2 pod");

                bq::unique_ptr<int32_t> up1(new int32_t(22));
                bq::unique_ptr<int32_t> up2(std::move(up1));
                result.add_result(!up1, "test_unique_ptr 1");
                result.add_result(*up2 == 22, "test_unique_ptr 2");

                {
                    bq::unique_ptr<test_unique_ptr> up_test_1;
                    bq::unique_ptr<test_unique_ptr> up_test_2(new test_unique_ptr());
                    result.add_result(!up_test_1, "test_unique_ptr 2");
                    result.add_result(1 == unique_ptr_counter, "test_unique_ptr 1");
                    bq::unique_ptr<test_unique_ptr> up_test_3(bq::move(up_test_2));
                    result.add_result(!up_test_2, "test_unique_ptr 2");
                    result.add_result(1 == unique_ptr_counter, "test_unique_ptr 3");
                }
                bq::unique_ptr<int32_t> swap1 = new int32_t(11);
                bq::unique_ptr<int32_t> swap2 = new int32_t(22);
                swap1.swap(swap2);
                result.add_result(*swap1 == 22, "test_unique_ptr 4");
                result.add_result(*swap2 == 11, "test_unique_ptr 5");

                {
                    std::atomic<int32_t> test_value(0);
                    struct shared_ptr_test_struct {
                        shared_ptr_test_struct(std::atomic<int32_t>& value)
                            : value_(value)
                        {
                            value_.fetch_add(1, std::memory_order_relaxed);
                        }
                        ~shared_ptr_test_struct()
                        {
                            value_.fetch_sub(1, std::memory_order_relaxed);
                        }
                        std::atomic<int32_t>& value_;
                    };
                    std::vector<bq::shared_ptr<shared_ptr_test_struct>> vec;
                    for (uint32_t i = 0; i < 1024 * 128; ++i) {
                        vec.push_back(bq::make_shared<shared_ptr_test_struct>(test_value));
                    }

                    std::vector<std::thread> test_threads;
                    for (uint32_t idx = 0; idx < 8; ++idx) {
                        test_threads.push_back(std::thread([&vec]() {
                            std::vector<bq::shared_ptr<shared_ptr_test_struct>> local_vec;
                            for (uint32_t loop = 0; loop < 16; ++loop) {
                                for (size_t i = 0; i < vec.size(); ++i) {
                                    bq::shared_ptr<shared_ptr_test_struct> sp = vec[i];
                                    bq::shared_ptr<shared_ptr_test_struct> sp2 = std::move(sp);
                                    local_vec.push_back(sp2);
                                }
                            }
                        }));
                    }
                    for (auto& t : test_threads) {
                        t.join();
                    }
                    vec.clear();
                    result.add_result(test_value.load(std::memory_order_relaxed) == 0, "shared_ptr test");
                }

                {
                    bq::tuple<int32_t, bool, bq::string> tuple_empty;
                    bq::tuple<int32_t, bool, bq::string> tuple1 = { 32, false };
                    bq::tuple<int32_t, bool, bq::string> tuple2 = { 32, true, "bq" };
                    result.add_result(bq::get<0>(tuple1) == 32, "tuple_test 1");
                    result.add_result(bq::get<1>(tuple1) == false, "tuple_test 2");
                    result.add_result(bq::get<2>(tuple1).is_empty(), "tuple_test 3");
                    result.add_result(bq::get<2>(tuple2) == "bq", "tuple_test 4");

                    struct empty_class_local {
                    };
                    bq::tuple<int32_t, int32_t, empty_class_local> ebco_tuple1;
                    bq::tuple<int32_t, int32_t> ebco_tuple2;
                    result.add_result(sizeof(ebco_tuple1) == sizeof(ebco_tuple2), "tuple ebco test");

                    result.add_result(bq::is_same<
                                          bq::tuple_element_t<0, bq::tuple<int32_t, bool, bq::string>>, bq::tuple_element_t<0, bq::tuple<int32_t, char>>>::value,
                        "tuple type test 1");
                    result.add_result(!bq::is_same<
                                          bq::tuple_element_t<1, bq::tuple<int32_t, bool, bq::string>>, bq::tuple_element_t<1, bq::tuple<int32_t, char>>>::value,
                        "tuple type test 2");
                }
                return result;
            }
        };
    }
}
