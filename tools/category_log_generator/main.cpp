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
#if defined(WIN32)
#include "Windows.h"
#include <shellapi.h> // for CommandLineToArgvW
#endif // WIN

static void print_usage(std::ostream& os)
{
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

#if defined(WIN32)
// Convert wide string to UTF-8
static bq::string wchar_to_utf8(const wchar_t* wstr)
{
    if (!wstr || !*wstr) {
        return bq::string();
    }
    int32_t utf8_len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (utf8_len <= 0) {
        return bq::string();
    }
    bq::string result;
    result.fill_uninitialized(static_cast<size_t>(utf8_len - 1)); // -1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.begin(), utf8_len, NULL, NULL);
    return result;
}
#endif

int32_t main(int32_t argc, char* argv[])
{
#if defined(WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Get UTF-16 encoded command line arguments
    int32_t wargc;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);

    // Convert to UTF-8 strings
    bq::array<bq::string> utf8_args;
    bq::array<char*> utf8_argv_ptrs;
    if (wargv) {
        utf8_args.set_capacity(wargc);
        utf8_argv_ptrs.set_capacity(wargc);
        for (int32_t i = 0; i < wargc; ++i) {
            utf8_args.push_back(wchar_to_utf8(wargv[i]));
        }
        for (int32_t i = 0; i < wargc; ++i) {
            utf8_argv_ptrs.push_back(utf8_args[i].begin());
        }
        argc = wargc;
        argv = utf8_argv_ptrs.begin();
        LocalFree(wargv);
    }
#endif
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