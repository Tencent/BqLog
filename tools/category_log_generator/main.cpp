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

#include <iostream>

static void print_usage(std::ostream& os) {
    os << "Usage:\n"
        << "  BqLog_CategoryLogGenerator ClassName CategoryConfigFile [OutputFolder]\n"
        << "  BqLog_CategoryLogGenerator -h | --help\n\n"
        << "Arguments:\n"
        << "  ClassName            Name of the generated category log class.\n"
        << "  CategoryConfigFile   Path to the category configuration text file (format described below).\n"
        << "  OutputFolder         Optional output directory (default: current directory \"./\").\n\n"
        << "Options:\n"
        << "  -h, --help           Show detailed help and the CategoryConfigFile format and examples.\n\n"
        << "Example:\n"
        << "  BqLog_CategoryLogGenerator DemoCategoryLog ./categories.txt ./out\n\n";
}

int32_t main(int32_t argc, char* argv[])
{
    bq::string help_flag1 = "--help";
    bq::string help_flag2 = "-h";

    // Help
    if (argc == 2 && (help_flag1.equals_ignore_case(argv[1]) || help_flag2.equals_ignore_case(argv[1]))) {
        print_usage(std::cout);
        output_config_file_format(std::cout);
        return 0;
    }

    // Argument count validation
    if (argc != 3 && argc != 4) {
        std::cerr << "Invalid arguments: expected 2 or 3 arguments, got " << (argc - 1) << "." << std::endl;
        print_usage(std::cerr);
        std::cerr << "Tip: run with -h or --help to see the CategoryConfigFile format and examples." << std::endl;
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