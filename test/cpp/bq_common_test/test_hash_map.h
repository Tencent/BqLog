#pragma once
#include <string>
#include "bq_common/bq_common.h"
#include "test_base.h"

namespace bq {
    namespace test {
        class test_hash_map : public test_base {
        private:
            test_result test_result_;
            uint32_t num_ = 0;

        private:
            void add_test_result(bool result, const char* test_info = nullptr)
            {
                if (test_info) {
                    test_result_.add_result(result, test_info);
                } else {
                    test_result_.add_result(result, (std::string("hash map test ") + std::to_string(num_++)).c_str());
                }
            }

        public:
            virtual test_result test() override
            {
                bq::hash_map<uint32_t, uint32_t> test_map;
                test_map[5] = 60;
                test_map[2] = 35;
                add_test_result(test_map.size() == 2);
                test_map[2] = 66;
                add_test_result(test_map.size() == 2);
                add_test_result(test_map[5] == 60);
                add_test_result(test_map[2] == 66);

                auto iter_tmp = test_map.find(5);
                add_test_result((bool)iter_tmp);
                add_test_result(!test_map.find(3));

                // R-reference test
                {
                    bq::hash_map<bq::string, bq::string> str_map;
                    bq::string test1 = "test string1";
                    bq::string test2 = "test string2";
                    str_map.add("test1", test1);
                    str_map["test2"] = test2;
                    add_test_result(test1.size() > 0);
                    add_test_result(test2.size() > 0);
                    str_map.add("test3", bq::move(test1));
                    str_map["test4"] = bq::move(test2);
                    add_test_result(test1.size() == 0);
                    add_test_result(test2.size() == 0);
                }

                {
                    bq::hash_map<float, float> float_map;
                    float_map[5.5] = 20.4f;
                    add_test_result(float_map[5.5] == float_map.begin()->value());
                }

                {
                    bq::hash_map<bq::string, bq::string> str_map;
                    bq::string key_str = "key";
                    bq::string value_str = "value";
                    str_map.add(bq::move(key_str), bq::move(value_str));
                    add_test_result(value_str.is_empty());
                    add_test_result(str_map.erase(str_map.begin()));
                    add_test_result(!str_map.erase(str_map.begin()));
                    uint32_t iter_num = 0;
                    for (auto iter = str_map.begin(); iter != str_map.end(); ++iter) {
                        ++iter_num;
                    }
                    add_test_result(iter_num == 0);
                }

                {
                    bq::hash_map<uint32_t, uint32_t> loop_map;

                    uint32_t sum = 0;
                    for (uint32_t i = 0; i < 10000; ++i) {
                        loop_map[i] = i % 50;
                        sum += loop_map[i];
                    }
                    uint32_t test_sum = 0;
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);
                    for (uint32_t i = 0; i < 10000; ++i) {
                        add_test_result(loop_map.erase(i));
                    }
                    add_test_result(loop_map.size() == 0);

                    int32_t count_erase_test = 0;
                    loop_map.add(5U, 5U);
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        count_erase_test += 1;
                    }
                    add_test_result(count_erase_test == 1);
                    add_test_result(!loop_map.erase(6));
                    add_test_result(loop_map.erase(5));
                    count_erase_test = 0;
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        count_erase_test += 1;
                    }
                    add_test_result(count_erase_test == 0);
                    loop_map.add(6);
                    count_erase_test = 0;
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        count_erase_test += 1;
                    }
                    add_test_result(count_erase_test == 1);

                    loop_map.clear();
                    test_sum = 0;
                    for (uint32_t i = 0; i < 10000; ++i) {
                        loop_map[i] = i % 50;
                    }
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);
                    for (uint32_t i = 0; i < 10000; ++i) {
                        add_test_result(loop_map.erase(i));
                    }
                    add_test_result(loop_map.size() == 0);

                    loop_map.reset();
                    test_sum = 0;
                    for (uint32_t i = 0; i < 10000; ++i) {
                        loop_map[i] = i % 50;
                    }
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);
                    for (uint32_t i = 0; i < 10000; ++i) {
                        add_test_result(loop_map.erase(i));
                    }
                    add_test_result(loop_map.size() == 0);

                    loop_map.clear();
                    test_sum = 0;
                    for (uint32_t i = 0; i < 10000; ++i) {
                        loop_map[i] = i % 50;
                    }
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);

                    loop_map.clear();
                    test_sum = 0;
                    for (uint32_t i = 0; i < 10000; ++i) {
                        loop_map[i] = i % 50;
                    }
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);

                    loop_map.reset();
                    test_sum = 0;
                    for (uint32_t i = 0; i < 10000; ++i) {
                        loop_map[i] = i % 50;
                    }
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);

                    loop_map.clear();
                    test_sum = 0;
                    for (uint32_t i = 0; i < 10000; ++i) {
                        loop_map[i] = i % 50;
                    }
                    for (auto iter = loop_map.begin(); iter != loop_map.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);

                    test_sum = 0;
                    bq::hash_map<uint32_t, uint32_t> loop_map_cpy = loop_map;
                    for (auto iter = loop_map_cpy.begin(); iter != loop_map_cpy.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);

                    loop_map_cpy.clear();
                    test_sum = 0;
                    loop_map_cpy = loop_map;
                    for (auto iter = loop_map_cpy.begin(); iter != loop_map_cpy.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);

                    loop_map_cpy.reset();
                    test_sum = 0;
                    loop_map_cpy = loop_map;
                    for (auto iter = loop_map_cpy.begin(); iter != loop_map_cpy.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);

                    test_sum = 0;
                    bq::hash_map<uint32_t, uint32_t> loop_map_move = bq::move(loop_map);
                    for (auto iter = loop_map_move.begin(); iter != loop_map_move.end(); ++iter) {
                        test_sum += iter->value();
                    }
                    add_test_result(test_sum == sum);

                    add_test_result(loop_map.size() == 0);
                }

                {
                    bq::hash_map<bq::string, bq::string> str_map;
                    constexpr typename decltype(str_map)::size_type loop_count = 100000;
                    constexpr typename decltype(str_map)::size_type count_per_round = 50000;
                    for (typename decltype(str_map)::size_type i = 0; i < loop_count; ++i) {
                        char format_key[16];
                        char format_value[16];
                        snprintf(format_key, 16, "test_%d", i % count_per_round);
                        snprintf(format_value, 16, "test_%d", i);
                        str_map[format_key] = format_value;
                    }
                    add_test_result(str_map.size() == count_per_round);
                    static uint8_t check_array[count_per_round];
                    memset(check_array, 0, sizeof(uint8_t) * count_per_round);
                    for (auto iter = str_map.begin(); iter != str_map.end(); ++iter) {
                        const auto& key = iter->key();
                        const auto& value = iter->value();
                        int32_t key_i = atoi(key.c_str() + sizeof("test"));
                        int32_t value_i = atoi(value.c_str() + sizeof("test"));
                        add_test_result(key_i >= 0 && (size_t)key_i < count_per_round);
                        add_test_result((size_t)value_i >= loop_count - count_per_round && (size_t)value_i < loop_count);
                        ++check_array[key_i];
                    }
                    for (size_t i = 0; i < count_per_round; ++i) {
                        add_test_result(check_array[i] == 1);
                    }
                }

                {
                    bq::hash_map<int32_t, int32_t> erase_test_map;
                    int32_t result_total = 0;
                    bq::hash_map<int32_t, int32_t>::size_type result_count = 0;
                    for (int32_t i = 0; i < 1000; ++i) {
                        result_total += i;
                        ++result_count;
                        erase_test_map[i] = i;
                    }
                    for (int32_t i = 999; i >= 0; i -= 2) {
                        result_total -= i;
                        --result_count;
                        add_test_result(erase_test_map.erase(i), "erase_test");
                    }
                    int32_t result_test = 0;
                    for (auto iter = erase_test_map.begin(); iter != erase_test_map.end(); ++iter) {
                        result_test += iter->value();
                    }
                    add_test_result(erase_test_map.size() == result_count, "erase_test_size");
                    add_test_result(result_test == result_total, "erase_test_size add");
                }

                {
                    bq::hash_map<bq::string, int32_t> erase_test_map;
                    int32_t result_total = 0;
                    bq::hash_map<int32_t, int32_t>::size_type result_count = 0;
                    for (int32_t i = 0; i < 1000; ++i) {
                        char tmp_str[128];
                        snprintf(tmp_str, sizeof(tmp_str), "key:%d", i);
                        result_total += i;
                        ++result_count;
                        erase_test_map[tmp_str] = i;
                    }
                    for (int32_t i = 999; i >= 0; i -= 2) {
                        char tmp_str[128];
                        snprintf(tmp_str, sizeof(tmp_str), "key:%d", i);
                        result_total -= erase_test_map[tmp_str];
                        --result_count;
                        add_test_result(erase_test_map.erase(tmp_str), "erase_test");
                    }
                    int32_t result_test = 0;
                    for (auto iter = erase_test_map.begin(); iter != erase_test_map.end(); ++iter) {
                        result_test += iter->value();
                    }
                    add_test_result(erase_test_map.size() == result_count, "erase_test_size string");
                    add_test_result(result_test == result_total, "erase_test_size add string");
                }

                return test_result_;
            }
        };
    }
}
