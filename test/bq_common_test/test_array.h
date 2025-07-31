#pragma once
#include "bq_common/bq_common.h"
#include "test_base.h"
#include <random>
#include <stdlib.h>

int32_t test_array_struct1_value = 0;
bq::test::test_result* result_ptr = nullptr;
struct test_array_struct1 {
    test_array_struct1()
    {
        test_array_struct1_value++;
    }
    test_array_struct1(const test_array_struct1& rhs)
    {
        (void)rhs;
        test_array_struct1_value++;
    }
    test_array_struct1(test_array_struct1&& rhs) noexcept
    {
        (void)rhs;
        test_array_struct1_value++;
    }
    ~test_array_struct1()
    {
        --test_array_struct1_value;
    }
    test_array_struct1& operator=(const test_array_struct1& rhs)
    {
        (void)rhs;
        return *this;
    }
    test_array_struct1& operator=(test_array_struct1&& rhs) noexcept
    {
        (void)rhs;
        return *this;
    }
};

struct test_rvalue_struct1 {
    int32_t value = 0;
    test_rvalue_struct1()
    {
        value++;
    }
    test_rvalue_struct1(const test_rvalue_struct1& rhs)
    {
        (void)rhs;
        value++;
    }
    test_rvalue_struct1(test_rvalue_struct1&& rhs) noexcept
    {
        value++;
        rhs.value--;
    }
    test_rvalue_struct1& operator=(const test_rvalue_struct1& rhs)
    {
        (void)rhs;
        value++;
        return *this;
    }
    test_rvalue_struct1& operator=(test_rvalue_struct1&& rhs)
    {
        value++;
        rhs.value--;
        return *this;
    }
};

struct test_array_position_move_struct1 {
    const int32_t value = 0;
    const int32_t* ptr = nullptr;
    test_array_position_move_struct1()
        : ptr(&value)
    {
    }
    test_array_position_move_struct1(const test_array_position_move_struct1& rhs)
        : value(rhs.value)
        , ptr(&value)
    {
    }
    test_array_position_move_struct1(test_array_position_move_struct1&& rhs) noexcept
        : value(rhs.value)
        , ptr(&value)
    {
    }
    test_array_position_move_struct1& operator=(const test_array_position_move_struct1& rhs)
    {
        (void)rhs;
        return *this;
    }
    test_array_position_move_struct1& operator=(test_array_position_move_struct1&& rhs) noexcept
    {
        (void)rhs;
        return *this;
    }
};

namespace bq {
    namespace test {

        class test_array : public test_base {
        public:
            virtual test_result test() override
            {
                test_result result;
                result_ptr = &result;
                {

                    bq::array<int32_t> i_array_empty = {};
                    bq::array<int32_t> i_array_single = { 32 };
                    bq::array<int32_t> i_array_two = { 32, 543 };
                    bq::array<int32_t> i_array = { 32, 543, 43 };
                    bq::array<bq::string> s_array = { "abc", "cde" };

                    result.add_result(i_array_empty.size() == 0, "initializer list test i_array_empty 0");

                    result.add_result(i_array_single.size() == 1, "initializer list test i_array_single 0");
                    result.add_result(i_array_single[0] == 32, "initializer list test i i_array_single");

                    result.add_result(i_array_two.size() == 2, "initializer list test i_array_two 0");
                    result.add_result(i_array_two[0] == 32, "initializer list test i_array_two 1");
                    result.add_result(i_array_two[1] == 543, "initializer list test i_array_two 2");

                    result.add_result(i_array.size() == 3, "initializer list test i_array 0");
                    result.add_result(i_array[0] == 32, "initializer list test i_array 1");
                    result.add_result(i_array[1] == 543, "initializer list test i_array 2");
                    result.add_result(i_array[2] == 43, "initializer list test i_array 3");

                    result.add_result(s_array.size() == 2, "initializer list test s_array 0");
                    result.add_result(s_array[0] == "abc", "initializer list test s_array 1");
                    result.add_result(s_array[1] == "cde", "initializer list test s_array 2");
                }

                {
                    bq::array<int32_t> i_array_1;
                    bq::array<int32_t> i_array_2;
                    i_array_1.push_back(20);
                    i_array_2.push_back(15);
                    i_array_2 = i_array_1;
                    result.add_result(i_array_1.size() == 1, "i_array_1 size test");
                    result.add_result(i_array_1[0] == 20, "i_array_1 value test");
                    result.add_result(i_array_2.size() == 1, "i_array_2 size test");
                    result.add_result(i_array_2[0] == 20, "i_array_2 value test");
                }

                {
                    bq::array<test_array_struct1> construct_test_array_1;
                    bq::array<test_array_struct1> construct_test_array_2;
                    for (int32_t i = 0; i < 20; ++i) {
                        construct_test_array_1.push_back(test_array_struct1());
                        construct_test_array_2.emplace_back(test_array_struct1());
                    }
                    construct_test_array_1.insert_batch(construct_test_array_1.begin() + 10, construct_test_array_2.begin() + 5, 15);
                    construct_test_array_2.erase(construct_test_array_2.begin() + 5, 5);
                    construct_test_array_2.erase(construct_test_array_2.end() - 10);
                    result.add_result(construct_test_array_1.size() == 35, "construct_test_array_1 size test");
                    result.add_result(construct_test_array_2.size() == 14, "construct_test_array_2 size test");
                    result.add_result((size_t)test_array_struct1_value == (construct_test_array_1.size() + construct_test_array_2.size()), "construct test");
                }

                {
                    test_array_struct1_value = 0;
                    bq::array<test_array_struct1> construct_test_array_1;
                    bq::array<test_array_struct1> construct_test_array_2;
                    for (int32_t i = 0; i < 20; ++i) {
                        construct_test_array_1.insert(construct_test_array_1.begin(), test_array_struct1());
                        construct_test_array_2.insert(construct_test_array_2.end(), test_array_struct1());
                    }
                    for (int32_t i = 0; i < 10; ++i) {
                        construct_test_array_1.erase(construct_test_array_1.begin());
                        construct_test_array_2.erase_replace(construct_test_array_2.begin());
                    }
                    result.add_result(construct_test_array_1.size() == 10, "construct_test_array_1 size test");
                    result.add_result(construct_test_array_2.size() == 10, "construct_test_array_2 size test");
                    result.add_result((size_t)test_array_struct1_value == (construct_test_array_1.size() + construct_test_array_2.size()), "construct test");
                }

                {
                    bq::array<test_rvalue_struct1> rvalue_test_array;
                    rvalue_test_array.push_back(test_rvalue_struct1());
                    rvalue_test_array.emplace_back(test_rvalue_struct1());
                    test_rvalue_struct1 rvalue_test_obj;
                    rvalue_test_array.push_back(rvalue_test_obj);
                    rvalue_test_array.emplace_back(rvalue_test_obj);
                    result.add_result(rvalue_test_array[0].value == 1
                            && rvalue_test_array[1].value == 1
                            && rvalue_test_array[2].value == 1
                            && rvalue_test_array[3].value == 1,
                        "rvalue_test_array construct test");
                    result.add_result(rvalue_test_obj.value == 1, "rvalue_test_array test");


                    bq::array<test_rvalue_struct1> rvalue_src_array;
                    constexpr size_t src_array_size = 1024;
                    for (size_t i = 0; i < src_array_size; ++i) {
                        rvalue_src_array.push_back(test_rvalue_struct1());
                    }
                    for (size_t push_size = 0; push_size < src_array_size; push_size += 13) {
                        bq::array<test_rvalue_struct1> rvalue_target_array;
                        std::random_device sd;
                        std::minstd_rand linear_ran(sd());
                        for (size_t i = 0; i < 1024; ++i) {
                            std::uniform_int_distribution<size_t> rand_seq(0, rvalue_target_array.size());
                            size_t insert_pos = rand_seq(linear_ran); 
                            rvalue_target_array.insert_batch(rvalue_target_array.begin() + insert_pos, rvalue_src_array.begin(), push_size);
                        }
                        size_t total_value = 0;
                        for (const auto& item : rvalue_target_array) {
                            if (item.value == 1) {
                                total_value++;
                            }
                        }
                        result.add_result(total_value == rvalue_target_array.size(), "rvalue_test_array back insert test for size:%zu", push_size);
                    }
                }

                {
                    bq::array<test_array_position_move_struct1> move_position_test_array;
                    std::mt19937 random_seed((uint32_t)bq::platform::high_performance_epoch_ms());
                    decltype(move_position_test_array)::size_type max_size = 1024 * 16;
                    decltype(max_size) random_size = (decltype(max_size))(random_seed()) % max_size;
                    for (decltype(move_position_test_array)::size_type i = 0; i < random_size; ++i) {
                        move_position_test_array.push_back(test_array_position_move_struct1());
                    }
                    for (auto it = move_position_test_array.begin(); it != move_position_test_array.end(); ++it) {
                        result.add_result(it->ptr == &(it->value), "array move non trivial constructor test phase 1, index:%d", (it - move_position_test_array.begin()));
                    }
                    decltype(move_position_test_array)::size_type remove_index = 0;
                    while (remove_index < move_position_test_array.size()) {
                        decltype(move_position_test_array)::size_type remove_count = remove_index % 10;
                        if (remove_count == 1 && (remove_index % 2 == 0)) {
                            move_position_test_array.erase_replace(move_position_test_array.begin() + remove_index);
                        } else {
                            move_position_test_array.erase(move_position_test_array.begin() + remove_index, remove_count);
                        }
                        remove_index += (random_seed() % 10);
                    }
                    for (auto it = move_position_test_array.begin(); it != move_position_test_array.end(); ++it) {
                        result.add_result(it->ptr == &(it->value), "array move non trivial constructor test phase 2, index:%d", (it - move_position_test_array.begin()));
                    }
                    random_size = (decltype(max_size))(random_seed()) % max_size;
                    for (decltype(move_position_test_array)::size_type i = 0; i < random_size; ++i) {
                        move_position_test_array.push_back(test_array_position_move_struct1());
                    }
                    for (auto it = move_position_test_array.begin(); it != move_position_test_array.end(); ++it) {
                        result.add_result(it->ptr == &(it->value), "array move non trivial constructor test phase 3, index:%d", (it - move_position_test_array.begin()));
                    }
                }

                {
                    bq::array<int32_t> i_array_1;
                    for (int32_t i = 0; i < 20; ++i) {
                        i_array_1.insert(i_array_1.begin(), i);
                    }
                    bq::array<int32_t> i_array_2;
                    for (int32_t i = 0; i < 20; ++i) {
                        i_array_2.insert(i_array_2.end(), i);
                    }
                    for (int32_t i = 0; i < 20; ++i) {
                        result.add_result(i_array_1[i] == 19 - i, "insert test dec order");
                        result.add_result(i_array_2[i] == i, "insert test inc order");
                    }
                    bq::array<int32_t> i_array3 = i_array_2;
                    result.add_result(i_array_2 == i_array3, "i_array_equal_test");
                    i_array_2.insert_batch(i_array_2.begin() + 10, i_array_1.begin(), 10);
                    result.add_result(i_array_2 != i_array3, "i_array_not_equal_test");
                    result.add_result(i_array_2.size() == 30, "insert batch test 0");
                    for (int32_t i = 0; i < 10; ++i) {
                        result.add_result(i_array_2[i] == i, "insert batch test 1");
                    }
                    for (int32_t i = 10; i < 20; ++i) {
                        result.add_result(i_array_2[i] == 19 - (i - 10), "insert batch test 2");
                    }
                    for (int32_t i = 20; i < 30; ++i) {
                        result.add_result(i_array_2[i] == i - 10, "insert batch test 3");
                    }
                }

                {
                    bq::array<int32_t> i_array_1;
                    for (int32_t i = 0; i < 20; ++i) {
                        i_array_1.insert(i_array_1.begin(), i);
                    }
                    bq::array<int32_t> i_array_2 = bq::move(i_array_1);
                    result.add_result(i_array_2.size() == 20, "move test 0");
                    for (int32_t i = 0; i < 20; ++i) {
                        result.add_result(i_array_2[i] == 19 - i, "move test 1");
                    }
                    result.add_result(i_array_1.is_empty(), "move test 2");
                }

                {
                    bq::array<int32_t> i_array_1;
                    for (int32_t i = 0; i < 20; ++i) {
                        i_array_1.insert(i_array_1.begin(), i);
                    }
                    i_array_1.clear();
                    result.add_result(i_array_1.size() == 0, "clear test 0");
                    result.add_result(i_array_1.capacity() > 0, "clear test 1");
                    i_array_1.reset();
                    result.add_result(i_array_1.capacity() == 0, "reset test");
                }

                {
                    bq::array<int32_t> i_array_1;
                    for (int32_t i = 0; i < 20; ++i) {
                        i_array_1.insert(i_array_1.begin(), i);
                    }
                    i_array_1.shrink();
                    result.add_result(i_array_1.size() == 20, "shrink test0");
                    result.add_result(i_array_1.capacity() == i_array_1.size(), "shrink test1");
                }

                {
                    bq::array<bq::array<int32_t>> nested_array = { bq::array<int32_t>({ 32, 56 }), bq::array<int32_t>({ 23, 45 }) };
                    result.add_result(nested_array.size() == 2, "nested test 0");
                    result.add_result(nested_array[0].size() == 2, "nested test 1");
                    result.add_result(nested_array[1].size() == 2, "nested test 2");
                    nested_array[1].push_back(34);
                    result.add_result(nested_array[0][0] == 32, "nested test 3");
                    result.add_result(nested_array[0][1] == 56, "nested test 4");
                    result.add_result(nested_array[1][0] == 23, "nested test 5");
                    result.add_result(nested_array[1][1] == 45, "nested test 6");
                    result.add_result(nested_array[1][2] == 34, "nested test 7");
                    auto& child_array_1 = nested_array[1];
                    auto& child_array_2 = *(nested_array.begin() + 1);
                    result.add_result(child_array_1 == child_array_2, "nested test 8");
                }
                {
                    bq::array<int32_t> test_pop_back_array = { 354, 32, 22 };
                    test_pop_back_array.pop_back();
                    result.add_result(test_pop_back_array.size() == 2, "pop back test 0");
                    result.add_result(test_pop_back_array[0] == 354, "pop back test 1");
                    result.add_result(test_pop_back_array[1] == 32, "pop back test 2");
                }

                {
                    bq::array<bq::string> find_test_array;
                    for (int32_t i = 0; i < 10000; ++i) {
                        char tmp[10];
                        snprintf(tmp, sizeof(tmp), "%d", i);
                        find_test_array.push_back(tmp);
                    }
                    for (int32_t i = 0; i < 10000; ++i) {
                        char tmp[10];
                        snprintf(tmp, sizeof(tmp), "%d", i);
                        auto iter = find_test_array.find(tmp);
                        result.add_result(iter == (find_test_array.begin() + i), "array find test %d", i);
                        iter = find_test_array.find(tmp, true);
                        result.add_result(iter == (find_test_array.begin() + i), "array reverse find test %d", i);
                    }
                    for (int32_t i = 10000; i < 20000; ++i) {
                        char tmp[10];
                        snprintf(tmp, sizeof(tmp), "%d", i);
                        auto iter = find_test_array.find(tmp);
                        result.add_result(iter == find_test_array.end(), "array find none exist test %d", i);
                    }
                    auto find_if1 = find_test_array.find_if([](const bq::string& str) { return str == "1234"; });
                    result.add_result(find_if1 == find_test_array.begin() + 1234, "array find_if test 1");
                    auto find_if2 = find_test_array.find_if([](const bq::string& str) { return str.size() == 3; }, true);
                    result.add_result(find_if2 == find_test_array.begin() + 999, "array find_if test 2");
                    auto find_if3 = find_test_array.find_if([](const bq::string& str) { return str.size() == 5; });
                    result.add_result(find_if3 == find_test_array.end(), "array find_if test 3");
                }

                {
                    bq::array<uint8_t> insert_test_array;
                    uint8_t tmp_test_array[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
                    insert_test_array.insert_batch(insert_test_array.end(), tmp_test_array, sizeof(tmp_test_array));
                    for (decltype(insert_test_array)::size_type i = 0; i < insert_test_array.size(); ++i) {
                        result.add_result(insert_test_array[i] == ((i % sizeof(tmp_test_array)) + 1), "array insert test %d", i);
                    }
                    insert_test_array.insert_batch(insert_test_array.end(), tmp_test_array, sizeof(tmp_test_array));
                    for (decltype(insert_test_array)::size_type i = 0; i < insert_test_array.size(); ++i) {
                        result.add_result(insert_test_array[i] == ((i % sizeof(tmp_test_array)) + 1), "array insert test %d", i);
                    }
                }

                {
                    bq::array<uint8_t> insert_test_array;
                    insert_test_array.fill_uninitialized(500);
                    result.add_result(insert_test_array.size() == 500, "fill_uninitialized test 1");
                }

                {
                    for (int32_t i = 0; i < 100000; ++i) {
                        bq::array<char, bq::aligned_allocator<char, 64>> alignement_test_array;
                        std::mt19937 random_seed((uint32_t)bq::platform::high_performance_epoch_ms());
                        size_t random_size = (size_t)(random_seed()) % 10000 + 1;
                        alignement_test_array.fill_uninitialized(random_size);
                        result.add_result(((uintptr_t)(&alignement_test_array[0])) % 64 == 0, "Array Alignment 64 test %d", i);
                    }
                    for (int32_t i = 0; i < 100000; ++i) {
                        bq::array<char, bq::aligned_allocator<char, 32>> alignement_test_array;
                        std::mt19937 random_seed((uint32_t)bq::platform::high_performance_epoch_ms());
                        size_t random_size = (size_t)(random_seed()) % 10000 + 1;
                        alignement_test_array.fill_uninitialized(random_size);
                        result.add_result(((uintptr_t)(&alignement_test_array[0])) % 32 == 0, "Array Alignment 32 test %d", i);
                    }
                    for (int32_t i = 0; i < 100000; ++i) {
                        bq::array<char, bq::aligned_allocator<char, 16>> alignement_test_array;
                        std::mt19937 random_seed((uint32_t)bq::platform::high_performance_epoch_ms());
                        size_t random_size = (size_t)(random_seed()) % 10000 + 1;
                        alignement_test_array.fill_uninitialized(random_size);
                        result.add_result(((uintptr_t)(&alignement_test_array[0])) % 16 == 0, "Array Alignment 16 test %d", i);
                    }
                }

                return result;
            }
        };
    }
}
