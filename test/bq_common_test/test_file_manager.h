#pragma once
#include <string.h>
#include "test_base.h"
#include "bq_common/bq_common.h"

namespace bq {
    namespace test {
        const int32_t base_dir_type = 1;

        class test_file_manager : public test_base {
        public:
            virtual test_result test() override
            {
                test_result result;
                bq::file_manager& file_manager = bq::file_manager::instance();
#ifdef BQ_WIN
                result.add_result(bq::file_manager::is_dir("C:"), "root dir test1");
                result.add_result(bq::file_manager::is_dir("C:\\"), "root dir test2");
                result.add_result(bq::file_manager::is_dir("C:/"), "root dir test3");
#else
                result.add_result(bq::file_manager::is_dir("/usr"), "root dir test");
                result.add_result(bq::file_manager::is_dir("/usr/"), "root dir test2");
#endif
                file_manager.remove_file_or_dir(TO_ABSOLUTE_PATH("cc", base_dir_type));
                result.add_result(file_manager.create_directory(TO_ABSOLUTE_PATH("cc/bb/aa/dd", base_dir_type)), "create_directory_in_base_path cc/bb/aa/dd");
                result.add_result(file_manager.create_directory(TO_ABSOLUTE_PATH("cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/", base_dir_type)), "super long path: create_directory_in_base_path cc/bb/aa/dd/*");
                result.add_result(file_manager.create_directory(TO_ABSOLUTE_PATH("cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/测试中文%##@aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/cc/bb/aa/dd/", base_dir_type)), "super long path and Chinese characters: create_directory_in_base_path cc/bb/aa/dd/*");
                result.add_result(file_manager.is_dir(TO_ABSOLUTE_PATH("cc/bb/aa/dd", base_dir_type)), "check create_directory_in_base_path cc/bb/aa/dd");
                result.add_result(file_manager.create_directory(TO_ABSOLUTE_PATH("cc/bb/aa/ee", base_dir_type)), "create_directory_in_base_path cc/bb/aa/ee");
                bq::array<bq::string> sub_dirs = file_manager.get_sub_dirs_and_files_name(TO_ABSOLUTE_PATH("cc/bb/aa/", base_dir_type));
                result.add_result(sub_dirs.size() == 2, "get_sub_dirs_and_files_name test 0");
                result.add_result((sub_dirs[0] == "dd" && sub_dirs[1] == "ee")
                        || (sub_dirs[1] == "dd" && sub_dirs[0] == "ee"),
                    "get_sub_dirs_and_files_name test 1");

                {
                    auto handle1 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/ff/real/v.txt", base_dir_type), file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    result.add_result(handle1.is_valid(), "open_or_create_file test 0", base_dir_type);
                    char data1[] = "This is test data 1\r\n";
                    char data2[] = "This is test data 2\r\n";
                    char data3[] = "This is test data 3\r\n";
                    result.add_result(sizeof(data1) == file_manager.write_file(handle1, data1, sizeof(data1)), "write test0");
                    auto handle2 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/ff/real/v.txt", base_dir_type), file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    result.add_result(handle2.is_valid(), "open_or_create_file test 1");
                    result.add_result(sizeof(data2) == file_manager.write_file(handle2, data2, sizeof(data2)), "write test1");
                    result.add_result(sizeof(data3) == file_manager.write_file(handle1, data3, sizeof(data3)), "write test2");
                    file_manager.flush_all_opened_files();
                    result.add_result(file_manager.get_file_size(handle1) == bq::max_value(sizeof(data1) + sizeof(data3), sizeof(data2)), "file size test 0");
                    result.add_result(file_manager.get_file_size(handle2) == bq::max_value(sizeof(data1) + sizeof(data3), sizeof(data2)), "file size test 1");
                    result.add_result(file_manager.close_file(handle1), "close file test 0");
                    result.add_result(file_manager.close_file(handle2), "close file test 1");

                    auto handle3 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/ff/real/v.txt", base_dir_type), file_open_mode_enum::read_write);
                    result.add_result(handle3.is_valid(), "open_or_create_file test 2");
                    result.add_result(file_manager.get_file_size(handle3) == bq::max_value(sizeof(data1) + sizeof(data3), sizeof(data2)), "file size test 2");
                    result.add_result(sizeof(data3) == file_manager.write_file(handle3, data3, sizeof(data3)), "write test3");
                    result.add_result(file_manager.close_file(handle3), "close file test 2");
                }

                {
                    file_manager.remove_file_or_dir(TO_ABSOLUTE_PATH("cc/bb/write_test.txt", base_dir_type));
                    auto handle3 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/write_test.txt", base_dir_type), file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    decltype(handle3) handle4;
                    decltype(handle3) handle5 = handle3;
                    handle4 = handle5;
                    const bq::string write_content = "This is \r\n my skdfjslkdfjwoi3jr23irjisjfsj速度快放假啊是l";
                    file_manager.write_file(handle3, write_content.c_str(), write_content.size());
                    file_manager.seek(handle4, file_manager::seek_option::begin, 0);
                    bq::string read_content = file_manager.read_text(handle5);
                    result.add_result(write_content == read_content, "read text test");
                }

                {
                    const char test_data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
                    file_manager.remove_file_or_dir(TO_ABSOLUTE_PATH("cc/bb/write_test.bin", base_dir_type));
                    auto handle3 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/write_test.bin", base_dir_type), file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    file_manager.write_file(handle3, test_data, sizeof(test_data));

                    char read_buffer[128];
                    size_t read_len = file_manager.read_file(handle3, read_buffer, 128, file_manager::seek_option::begin, 0);
                    result.add_result(sizeof(test_data) == read_len, "read write test 0");
                    result.add_result(memcmp(test_data, read_buffer, read_len) == 0, "read_write_test 1");
                    result.add_result(file_manager.write_file(handle3, test_data, sizeof(test_data) - 4, file_manager::seek_option::begin, 4), "read write test 2");
                    read_len = file_manager.read_file(handle3, read_buffer, 128, file_manager::seek_option::begin, 0);
                    result.add_result(sizeof(test_data) == read_len, "read write test 3");
                    result.add_result(memcmp(test_data, read_buffer, read_len) != 0, "read_write_test 4");
                    result.add_result(file_manager.write_file(handle3, test_data + 4, 4, file_manager::seek_option::begin, 0), "read write test 5");
                    result.add_result(file_manager.write_file(handle3, test_data + 4, 4, file_manager::seek_option::begin, 4), "read write test 5");
                    read_len = file_manager.read_file(handle3, read_buffer, 128, file_manager::seek_option::begin, 0);
                    result.add_result(sizeof(test_data) == read_len, "read write test 6");
                    result.add_result(memcmp(test_data + 4, read_buffer, 4) == 0, "read_write_test 7");
                    result.add_result(memcmp(test_data + 4, read_buffer + 4, 4) == 0, "read_write_test 8");

                    file_manager.close_file(handle3);
                }

                {
                    auto handle1 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/ff/truncate/v.txt", base_dir_type), file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    result.add_result(handle1.is_valid(), "truncate test 1");
                    char data1[] = "This is test data 1\r\n";
                    char data2[] = "This is test data 2\r\n";
                    char data3[] = "This is test data 3\r\n";
                    result.add_result(file_manager.write_file(handle1, data1, sizeof(data1)), "truncate test 2");
                    result.add_result(file_manager.write_file(handle1, data2, sizeof(data2)), "truncate test 3");
                    result.add_result(file_manager.write_file(handle1, data3, sizeof(data3)), "truncate test 4");
                    result.add_result(file_manager.get_file_size(handle1) == (sizeof(data1) + sizeof(data2) + sizeof(data3)), "truncate test 5");
                    result.add_result(file_manager.truncate_file(handle1, sizeof(data1)), "truncate test 6");
                    result.add_result(file_manager.get_file_size(handle1) == sizeof(data1), "truncate test 7");
                    result.add_result(file_manager.truncate_file(handle1, 1), "truncate test 6.1");
                    result.add_result(file_manager.get_file_size(handle1) == 1, "truncate test 7.1");
                    result.add_result(file_manager.truncate_file(handle1, 0), "truncate test 6.2");
                    result.add_result(file_manager.get_file_size(handle1) == 0, "truncate test 7.2");
                    result.add_result(file_manager.write_file(handle1, data1, sizeof(data1)), "write after truncate test");
                    file_manager.close_file(handle1);
                    result.add_result(!handle1.is_valid(), "truncate test 8");
                    handle1 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/ff/truncate/v.txt", base_dir_type), file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    result.add_result(file_manager.get_file_size(handle1) == sizeof(data1), "truncate test 9");
                }

                {
                    // last modified time
                    auto last_modified_time0 = bq::file_manager::get_file_last_modified_epoch_ms(TO_ABSOLUTE_PATH("cc/bb/32323/not_exist.32", base_dir_type));
                    result.add_result(last_modified_time0 == 0, "last_modified_time test 0");
                    auto current_epoch = bq::platform::high_performance_epoch_ms();
                    auto last_modified_time1 = bq::file_manager::get_file_last_modified_epoch_ms(TO_ABSOLUTE_PATH("cc/bb/ff/real/v.txt", base_dir_type));
                    result.add_result(last_modified_time1 <= current_epoch && (current_epoch - last_modified_time1) < 60000, "last_modified_time test 1， last_modified_time1：%" PRIu64 ", current_epoch:%" PRIu64 "", last_modified_time1, current_epoch);
                }

                {
                    // exclusive test
                    auto group_handle_A_1 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/aa/dd/exclusive_A.txt", true), file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    auto group_handle_A_2 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/../cc/bb/aa/dd/exclusive_A.txt", true), file_open_mode_enum::exclusive | file_open_mode_enum::read_write);
                    result.add_result(group_handle_A_1 && !group_handle_A_2, "exclusive test 1");
                    group_handle_A_1.invalid();
                    group_handle_A_2 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/../cc/bb/aa/dd/exclusive_A.txt", true), file_open_mode_enum::exclusive | file_open_mode_enum::read_write);
                    result.add_result(group_handle_A_2, "exclusive test 2");
                    auto group_handle_A_3 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/aa/dd/exclusive_A.txt", true), file_open_mode_enum::read);
                    auto group_handle_A_4 = file_manager.open_file(TO_ABSOLUTE_PATH("cc/bb/aa/dd/exclusive_A.txt", true), file_open_mode_enum::write);
                    result.add_result(group_handle_A_3 && !group_handle_A_4, "exclusive test 3");
                }

                if (bq::memory_map::is_platform_support()) {
                    // memory map test
                    constexpr size_t memory_map_file_size = 64 * 1024;
                    auto mmf_handle_src = file_manager.open_file(TO_ABSOLUTE_PATH("cc/mm_map_src.mmp", true), file_open_mode_enum::auto_create | file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
                    bq::file_manager::instance().truncate_file(mmf_handle_src, memory_map_file_size);
                    auto mmp_handle_src = memory_map::create_memory_map(mmf_handle_src, 0, memory_map_file_size);
                    result.add_result(mmp_handle_src.has_been_mapped(), "create memory map file");
                    result.add_result(mmp_handle_src.get_error_code() == 0, "create memory map file error code");
                    for (size_t i = 0; i < memory_map_file_size; ++i) {
                        ((uint8_t*)mmp_handle_src.get_mapped_data())[i] = (uint8_t)(i % 255);
                    }
                    memory_map::flush_memory_map(mmp_handle_src);
                    memory_map::release_memory_map(mmp_handle_src);
                    file_manager.close_file(mmf_handle_src);

                    auto mmf_handle_tar = file_manager.open_file(TO_ABSOLUTE_PATH("cc/mm_map_src.mmp", true), file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
                    auto mmp_handle_tar = memory_map::create_memory_map(mmf_handle_tar, 0, memory_map_file_size);
                    result.add_result(mmp_handle_tar.has_been_mapped(), "open memory map file");
                    result.add_result(mmp_handle_tar.get_error_code() == 0, "open memory map file error code");

                    bool check_result = true;
                    for (size_t i = 0; i < memory_map_file_size; ++i) {
                        if (((uint8_t*)mmp_handle_tar.get_mapped_data())[i] != (uint8_t)(i % 255)) {
                            check_result = false;
                        }
                    }
                    result.add_result(check_result, "memory map check result");
                    memory_map::flush_memory_map(mmp_handle_tar);
                    memory_map::release_memory_map(mmp_handle_tar);
                    file_manager.close_file(mmf_handle_tar);
                }

                result.add_result(file_manager.remove_file_or_dir(TO_ABSOLUTE_PATH("cc/../cc/bb/aa/dd/", base_dir_type)), "check remove directory 0");
                result.add_result(!file_manager.is_dir(TO_ABSOLUTE_PATH("cc/bb/aa/dd", base_dir_type)), "check remove directory 1");
                result.add_result(file_manager.remove_file_or_dir(TO_ABSOLUTE_PATH("cc", base_dir_type)), "check remove directory 2");
                result.add_result(!file_manager.is_dir(TO_ABSOLUTE_PATH("cc/bb/aa/", base_dir_type)), "check remove directory 3");
                result.add_result(!file_manager.is_dir(TO_ABSOLUTE_PATH("cc", base_dir_type)), "check remove directory 4");
                return result;
            }
        };
    }
}
