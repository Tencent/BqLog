#if defined(WIN32)
#include <windows.h>
#endif
#include "demo_category_log.h"
#include <stdio.h>
#include <thread>
#include <chrono>
#include <string>

int main()
{
#if defined(WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    bq::string config = R"(
						appenders_config.appender_0.type=console
						appenders_config.appender_0.time_zone=default local time
						appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]
						appenders_config.appender_0.file_name=CCLog/normal
						appenders_config.appender_0.base_dir_type=0
						appenders_config.appender_0.max_file_size=10000000
						appenders_config.appender_0.expire_time_days=10
						appenders_config.appender_0.capacity_limit=200000000
					
						appenders_config.appender_1.type=text_file
						appenders_config.appender_1.time_zone=default local time
						appenders_config.appender_1.levels=[verbose,debug,info,warning,error,fatal]
						appenders_config.appender_1.file_name=CCLog/normal
						appenders_config.appender_1.base_dir_type=0
						appenders_config.appender_1.max_file_size=1000000000
						appenders_config.appender_1.expire_time_days=10
						appenders_config.appender_1.capacity_limit=10000000000
						appenders_config.appender_1.always_create_new_file=true
					
						appenders_config.appender_3409.type=compressed_file
						appenders_config.appender_3409.time_zone=default local time
						appenders_config.appender_3409.levels=[verbose,debug,info,warning,error,fatal]
						appenders_config.appender_3409.file_name=CCLog/normal
						appenders_config.appender_3409.base_dir_type=0
						appenders_config.appender_3409.max_file_size=1000000000
						appenders_config.appender_3409.expire_time_days=10
						appenders_config.appender_3409.capacity_limit=8000000000

            log.buffer_size=65535
            log.recovery=true
        )";

    bq::demo_category_log my_category_log = bq::demo_category_log::create_log("AAA", config); // create a my_log object with config.
    my_category_log.info("content"); // this is for empty category
    my_category_log.info(my_category_log.cat.node_4.node_7, "content");
    bq::demo_category_log::force_flush_all_logs();
    return 0;
}
