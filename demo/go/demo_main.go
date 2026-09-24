/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
package main

import (
	bq "github.com/Tencent/BqLog/wrapper/go/src/bq"
)

func main() {
	log_config := `
appenders_config.appender_0.type=console
appenders_config.appender_0.time_zone=localtime
appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]
appenders_config.appender_0.file_name=CCLog/normal
appenders_config.appender_0.base_dir_type=0
appenders_config.appender_0.max_file_size=10000000
appenders_config.appender_0.expire_time_days=10
appenders_config.appender_0.capacity_limit=200000000

appenders_config.appender_1.type=text_file
appenders_config.appender_1.time_zone=localtime
appenders_config.appender_1.levels=[verbose,debug,info,warning,error,fatal]
appenders_config.appender_1.file_name=CCLog/normal
appenders_config.appender_1.base_dir_type=0
appenders_config.appender_1.max_file_size=1000000000
appenders_config.appender_1.expire_time_days=10
appenders_config.appender_1.capacity_limit=10000000000

appenders_config.appender_3409.type=compressed_file
appenders_config.appender_3409.time_zone=localtime
appenders_config.appender_3409.levels=[verbose,debug,info,warning,error,fatal]
appenders_config.appender_3409.file_name=CCLog/normal
appenders_config.appender_3409.base_dir_type=0
appenders_config.appender_3409.max_file_size=1000000000
appenders_config.appender_3409.expire_time_days=10
appenders_config.appender_3409.capacity_limit=8000000000

log.buffer_size=65535
log.reliable_level=normal
`

	demo_log_config := `
appenders_config.appender_0.type=console
appenders_config.appender_0.time_zone=localtime
appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]
appenders_config.appender_0.file_name=CCLog/normal
appenders_config.appender_0.base_dir_type=0
appenders_config.appender_0.max_file_size=10000000
appenders_config.appender_0.expire_time_days=10
appenders_config.appender_0.capacity_limit=200000000

appenders_config.appender_1.type=text_file
appenders_config.appender_1.time_zone=localtime
appenders_config.appender_1.levels=[verbose,debug,info,warning,error,fatal]
appenders_config.appender_1.file_name=CCLog/demo
appenders_config.appender_1.base_dir_type=0
appenders_config.appender_1.max_file_size=1000000000
appenders_config.appender_1.expire_time_days=10
appenders_config.appender_1.capacity_limit=10000000000

appenders_config.appender_3409.type=compressed_file
appenders_config.appender_3409.time_zone=localtime
appenders_config.appender_3409.levels=[verbose,debug,info,warning,error,fatal]
appenders_config.appender_3409.file_name=CCLog/demo
appenders_config.appender_3409.base_dir_type=0
appenders_config.appender_3409.max_file_size=1000000000
appenders_config.appender_3409.expire_time_days=10
appenders_config.appender_3409.capacity_limit=8000000000

log.buffer_size=65535
log.reliable_level=normal
`

	base_log := bq.Create_log("MyLog", log_config, nil)
	base_log.Info("测试日志{}, {}", false, 5.3245)

	my_demo_log := Create_demo_category_log("Demo_Log", demo_log_config)
	my_demo_log.Info("测试日志{}, {}", false, 5.3245)
	my_demo_log.Info_c(my_demo_log.Cat.Node_2.Node_5, "Demo Log测试日志{}, {}", false, 5.3245)

	base_log.Force_flush()
	my_demo_log.Force_flush()
}
