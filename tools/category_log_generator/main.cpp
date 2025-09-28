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
#include "bq_common/bq_common.h"
#include "category_generator.h"

int32_t main(int32_t argc, char* argv[])
{
    bq::string help_flag1 = "--help";
    bq::string help_flag2 = "-h";

    if (argc == 2 && (help_flag1.equals_ignore_case(argv[1]) || help_flag2.equals_ignore_case(argv[1]))) {
        std::cout << "usage : BqLog_CategoryLogGenerator ClassName CategoryConfigFile [OutputFolder]" << std::endl;
        std::cout << "or : BqLog_CategoryLogGenerator -h(or --help) " << std::endl;
        output_config_file_format(std::cout);
        return 0;
    }
    if (argc != 3 && argc != 4) {
        std::cerr << "usage : BqLog_CategoryLogGenerator ClassName CategoryConfigFile [OutputFolder]" << std::endl;
        std::cerr << "or : BqLog_CategoryLogGenerator -h(or --help) " << std::endl;
        return -1;
    }

    bq::string class_name = argv[1];
    bq::string config_file = argv[2];
    bq::string output_dir = "./";
    if (argc == 4) {
        output_dir = argv[3];
    }
    if (!bq::category_generator::general_log_class(class_name, config_file, output_dir)) {
        std::cerr << "Generate category log file failed!" << std::endl;
        return -1;
    }
    std::cout << "Generate category log file success!" << std::endl;
    return 0;
}
