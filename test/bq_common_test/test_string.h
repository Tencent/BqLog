#pragma once
#include "test_base.h"
#include "bq_common/bq_common.h"
#include <string>
#if BQ_CPP_17
#include <string_view>
#endif

namespace bq {
    namespace test {

        class test_string : public test_base {
        public:
            virtual test_result test() override
            {
                test_result result;

#define TEST_STR1 "THIS_IS_TEST_STR_1"
#define TEST_STR1_LEN strlen(TEST_STR1)

#define TEST_STR2 "THIS_IS_TEST_STR_2"
#define TEST_STR2_LEN strlen(TEST_STR2)

#define TEST_STR1STR2 "THIS_IS_TEST_STR_1THIS_IS_TEST_STR_2"

#define TEST_STR3 "THIS"
#define TEST_STR3_LEN strlen(TEST_STR3)
                {
                    bq::string test_str1 = TEST_STR1;
                    result.add_result(test_str1.size() == TEST_STR1_LEN, "string length test0");
                    result.add_result(test_str1 == TEST_STR1, "string equal test0");
                    result.add_result(test_str1 != TEST_STR2, "string not equal test0");

                    bq::string test_str2 = TEST_STR2;
                    test_str1 += test_str2;
                    result.add_result(test_str1.size() == TEST_STR1_LEN + TEST_STR2_LEN, "string length test1");
                    result.add_result(test_str1 == TEST_STR1STR2, "string equal test1");
                    const char* test1test2_char_ptr = TEST_STR1STR2;
                    result.add_result(test_str1 == test1test2_char_ptr, "string equal test2");

                    result.add_result(test_str1.find("@@") == bq::string::npos, "string find test 0");
                    result.add_result(test_str1.find(TEST_STR3) == 0, "string find test 1");
                    result.add_result(test_str1.find(TEST_STR3, 1) == TEST_STR1_LEN, "string find test 2");
                    result.add_result(test_str1.find("VVVVVV") == bq::string::npos, "string find test 3");
                    result.add_result(test_str1.find("VVVVVV", 1) == bq::string::npos, "string find test 4");

                    test_str1 = "";
                    test_str2 = "1";
                    bq::string test_str3;
                    const char* empty_c_str1 = test_str1.c_str();
                    const char* empty_c_str3 = test_str3.c_str();
                    test_str2.erase(test_str2.begin());
                    result.add_result(test_str1 == test_str2, "string \\0 equal test");
                    result.add_result(empty_c_str1 == empty_c_str3, "empty string \\0 equal test");

                    bq::array<bq::string> split = bq::string(TEST_STR1).split("_");
                    result.add_result(split.size() == 5, "string split test 0");
                    result.add_result(split[0] == "THIS", "string split test 1");
                    result.add_result(split[1] == "IS", "string split test 2");
                    result.add_result(split[2] == "TEST", "string split test 3");
                    result.add_result(split[3] == "STR", "string split test 4");
                    result.add_result(split[4] == "1", "string split test 5");
                    bq::array<bq::string> split2 = bq::string("///////").split("/");
                    bq::array<bq::string> split3 = bq::string("a///////bb").split("/");
                    result.add_result(split2.size() == 0, "string split test 6");
                    result.add_result(split3.size() == 2, "string split test 7");
                    result.add_result(split3[0] == "a", "string split test 8");
                    result.add_result(split3[1] == "bb", "string split test 9");

                    bq::string replace_str = bq::string(TEST_STR1).replace("_", "####");
                    result.add_result(replace_str == "THIS####IS####TEST####STR####1", "string split test 4");

                    result.add_result(replace_str.equals_ignore_case("ThiS####IS####TEST####Str####1"), "string compare ignore case");

                    bq::string find_last_test = TEST_STR1STR2;
                    result.add_result(find_last_test.find_last("STR") == find_last_test.find("STR_2"), "string find_last test0");
                    result.add_result(find_last_test.find_last("####") == bq::string::npos, "string find_last test1");

                    bq::string trim_test1 = " \t   test \t  fix  \t";
                    result.add_result(trim_test1.trim() == "test \t  fix", "string trim test0");
                    bq::string trim_test2 = " \t    \t   \t";
                    result.add_result(trim_test2.trim() == "", "string trim test1");
                }

                {
                    bq::string test_insert1;
                    bq::string test_insert2 = "insert2";
                    test_insert1.insert_batch(test_insert1.end(), test_insert2.begin(), test_insert2.size());
                    result.add_result(test_insert1 == test_insert2, "string insert batch test 0");
                    result.add_result(strlen(test_insert1.c_str()) == test_insert1.size(), "string insert batch test1");
                    test_insert1.insert(test_insert1.begin(), 'a');
                    result.add_result(test_insert1 == "ainsert2", "string insert test 0");
                    test_insert1.erase(test_insert1.begin());
                    result.add_result(test_insert1 == test_insert2, "string erase test 0");
                    test_insert1.erase_replace(test_insert1.begin());
                    result.add_result(test_insert1 == "2nsert", "string erase replace test");
                    test_insert1.pop_back();
                    result.add_result(test_insert1 == "2nser", "string pop back test");
                }

                {
                    bq::string test_str = "YYYAAAABBB";
                    result.add_result(test_str.begin_with("YYY"), "string begin_with test 0");
                    result.add_result(!test_str.begin_with("YYYYYYYYYYYYYYYYY"), "string begin_with test 1");
                    result.add_result(!test_str.begin_with("YYYY"), "string begin_with test 2");
                    result.add_result(test_str.end_with("BBB"), "string end_with test 0");
                    result.add_result(!test_str.end_with("BBBBBBBBBBBBBBB"), "string end_with test 1");
                    result.add_result(!test_str.end_with("BBBB"), "string end_with test 2");
                }

#undef TEST_STR1
#undef TEST_STR1_LEN
#define TEST_STR1 u"THIS_IS_TEST_STR_1"
#define TEST_STR1_LEN bq::___string_len(TEST_STR1)

#undef TEST_STR2
#undef TEST_STR2_LEN
#define TEST_STR2 u"THIS_IS_TEST_STR_2"
#define TEST_STR2_LEN bq::___string_len(TEST_STR2)

#undef TEST_STR1STR2
#define TEST_STR1STR2 u"THIS_IS_TEST_STR_1THIS_IS_TEST_STR_2"

#undef TEST_STR3
#undef TEST_STR3_LEN
#define TEST_STR3 u"THIS"
#define TEST_STR3_LEN bq::___string_len(TEST_STR3)
                {
                    bq::u16string test_str1 = TEST_STR1;
                    result.add_result(test_str1.size() == TEST_STR1_LEN, "string length test0");
                    result.add_result(test_str1 == TEST_STR1, "string equal test0");
                    result.add_result(test_str1 != TEST_STR2, "string not equal test0");

                    bq::u16string test_str2 = TEST_STR2;
                    test_str1 += test_str2;
                    result.add_result(test_str1.size() == TEST_STR1_LEN + TEST_STR2_LEN, "string length test1");
                    result.add_result(test_str1 == TEST_STR1STR2, "string equal test1");
                    const char16_t* test1test2_char_ptr = TEST_STR1STR2;
                    result.add_result(test_str1 == test1test2_char_ptr, "string equal test2");

                    result.add_result(test_str1.find(u"@@") == bq::string::npos, "string find test 0");
                    result.add_result(test_str1.find(TEST_STR3) == 0, "string find test 1");
                    result.add_result(test_str1.find(TEST_STR3, 1) == TEST_STR1_LEN, "string find test 2");
                    result.add_result(test_str1.find(u"VVVVVV") == bq::string::npos, "string find test 3");
                    result.add_result(test_str1.find(u"VVVVVV", 1) == bq::string::npos, "string find test 4");

                    test_str1 = u"";
                    test_str2 = u"1";
                    bq::u16string test_str3;
                    const char16_t* empty_c_str1 = test_str1.c_str();
                    const char16_t* empty_c_str3 = test_str3.c_str();
                    test_str2.erase(test_str2.begin());
                    result.add_result(test_str1 == test_str2, "string \\0 equal test");
                    result.add_result(empty_c_str1 == empty_c_str3, "empty string \\0 equal test");

                    bq::array<bq::u16string> split = bq::u16string(TEST_STR1).split(u"_");
                    result.add_result(split.size() == 5, "string split test 0");
                    result.add_result(split[0] == u"THIS", "string split test 1");
                    result.add_result(split[1] == u"IS", "string split test 2");
                    result.add_result(split[2] == u"TEST", "string split test 3");
                    result.add_result(split[3] == u"STR", "string split test 4");
                    result.add_result(split[4] == u"1", "string split test 5");
                    bq::array<bq::u16string> split2 = bq::u16string(u"///////").split(u"/");
                    bq::array<bq::u16string> split3 = bq::u16string(u"a///////bb").split(u"/");
                    result.add_result(split2.size() == 0, "string split test 6");
                    result.add_result(split3.size() == 2, "string split test 7");
                    result.add_result(split3[0] == u"a", "string split test 8");
                    result.add_result(split3[1] == u"bb", "string split test 9");

                    bq::u16string replace_str = bq::u16string(TEST_STR1).replace(u"_", u"####");
                    result.add_result(replace_str == u"THIS####IS####TEST####STR####1", "string split test 4");

                    result.add_result(replace_str.equals_ignore_case(u"ThiS####IS####TEST####Str####1"), "string compare ignore case");

                    bq::u16string find_last_test = TEST_STR1STR2;
                    result.add_result(find_last_test.find_last(u"STR") == find_last_test.find(u"STR_2"), "string find_last test0");
                    result.add_result(find_last_test.find_last(u"####") == bq::string::npos, "string find_last test1");

                    bq::u16string trim_test1 = u" \t   test \t  fix  \t";
                    result.add_result(trim_test1.trim() == u"test \t  fix", "string trim test0");
                    bq::u16string trim_test2 = u" \t    \t   \t";
                    result.add_result(trim_test2.trim() == u"", "string trim test1");
                }

                {
                    bq::u16string test_insert1;
                    bq::u16string test_insert2 = u"insert2";
                    test_insert1.insert_batch(test_insert1.end(), test_insert2.begin(), test_insert2.size());
                    result.add_result(test_insert1 == test_insert2, "string insert batch test 0");
                    result.add_result(bq::___string_len(test_insert1.c_str()) == test_insert1.size(), "string insert batch test1");
                    test_insert1.insert(test_insert1.begin(), 'a');
                    result.add_result(test_insert1 == u"ainsert2", "string insert test 0");
                    test_insert1.erase(test_insert1.begin());
                    result.add_result(test_insert1 == test_insert2, "string erase test 0");
                    test_insert1.erase_replace(test_insert1.begin());
                    result.add_result(test_insert1 == u"2nsert", "string erase replace test");
                    test_insert1.pop_back();
                    result.add_result(test_insert1 == u"2nser", "string pop back test");
                }

                {
                    bq::u16string test_str = u"YYYAAAABBB";
                    result.add_result(test_str.begin_with(u"YYY"), "string begin_with test 0");
                    result.add_result(!test_str.begin_with(u"YYYYYYYYYYYYYYYYY"), "string begin_with test 1");
                    result.add_result(!test_str.begin_with(u"YYYY"), "string begin_with test 2");
                    result.add_result(test_str.end_with(u"BBB"), "string end_with test 0");
                    result.add_result(!test_str.end_with(u"BBBBBBBBBBBBBBB"), "string end_with test 1");
                    result.add_result(!test_str.end_with(u"BBBB"), "string end_with test 2");
                }
                
                
                //test std::string and std::string_view
                {
                    bq::string bq_str1 = "This is Bq Str 1 utf16";
                    std::string std_str1 = bq_str1;
                    std::string_view std_str_view1 = bq_str1;
                    size_t str1_len = bq_str1.size();
                    result.add_result(str1_len == std_str1.size() && str1_len == std_str_view1.size(), "bq::string, std::string, std::string_view cast test: utf8 size");
                    
                    result.add_result(memcmp(&bq_str1[0], &std_str1[0], str1_len) == 0, "bq::string, std::string cast test: utf8 equal");
                    result.add_result(memcmp(&bq_str1[0], &std_str_view1[0], str1_len) == 0, "bq::string, std::string_view cast test: utf8 equal");
                    
                    bq::string bq_str1_from_std_str = std_str1;
                    bq::string bq_str2_from_std_str_view = std_str_view1;
                    
                    result.add_result(bq_str1 == bq_str1_from_std_str, "std::string, bq::string cast test: utf8 equal");
                    result.add_result(bq_str1 == bq_str2_from_std_str_view, "std::string_view, bq::string cast test: utf8 equal");
                }
                {
                    bq::u16string bq_str1 = u"This is Bq Str 1 utf16";
                    std::u16string std_str1 = bq_str1;
                    std::u16string_view std_str_view1 = bq_str1;
                    size_t str1_len = bq_str1.size();
                    result.add_result(str1_len == std_str1.size() && str1_len == std_str_view1.size(), "bq::string, std::string, std::string_view cast test: utf16 size");
                    
                    result.add_result(memcmp(&bq_str1[0], &std_str1[0], str1_len * sizeof(char16_t)) == 0, "bq::string, std::string cast test: utf16 equal");
                    result.add_result(memcmp(&bq_str1[0], &std_str_view1[0], str1_len * sizeof(char16_t)) == 0, "bq::string, std::string_view cast test: utf16 equal");
                    
                    bq::u16string bq_str1_from_std_str = std_str1;
                    bq::u16string bq_str2_from_std_str_view = std_str_view1;
                    
                    result.add_result(bq_str1 == bq_str1_from_std_str, "std::string, bq::string cast test: utf16 equal");
                    result.add_result(bq_str1 == bq_str2_from_std_str_view, "std::string_view, bq::string cast test: utf16 equal");
                }
                {
                    bq::u32string bq_str1 = U"This is Bq Str 1 utf32";
                    std::u32string std_str1 = bq_str1;
                    std::u32string_view std_str_view1 = bq_str1;
                    size_t str1_len = bq_str1.size();
                    result.add_result(str1_len == std_str1.size() && str1_len == std_str_view1.size(), "bq::string, std::string, std::string_view cast test: utf32 size");
                    
                    result.add_result(memcmp(&bq_str1[0], &std_str1[0], str1_len * sizeof(char32_t)) == 0, "bq::string, std::string cast test: utf32 equal");
                    result.add_result(memcmp(&bq_str1[0], &std_str_view1[0], str1_len * sizeof(char32_t)) == 0, "bq::string, std::string_view cast test: utf32 equal");
                    
                    bq::u32string bq_str1_from_std_str = std_str1;
                    bq::u32string bq_str2_from_std_str_view = std_str_view1;
                    
                    result.add_result(bq_str1 == bq_str1_from_std_str, "std::string, bq::string cast test: utf32 equal");
                    result.add_result(bq_str1 == bq_str2_from_std_str_view, "std::string_view, bq::string cast test: utf32 equal");
                }
                return result;
            }
        };
    }
}
