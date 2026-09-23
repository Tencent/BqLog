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
	"strings"

	bq "github.com/Tencent/BqLog/wrapper/go/src/bq"
)

func test_log_basic() *test_result {
	result := &test_result{}
	log_inst_sync := bq.Create_log("sync_log", "appenders_config.ConsoleAppender.type=console\n"+
		"appenders_config.ConsoleAppender.time_zone=localtime\n"+
		"appenders_config.ConsoleAppender.levels=[info, info, error,info]\n"+
		"\n"+
		"log.thread_mode=sync\n", nil)

	full_str := "123"

	log_inst_sync.Debug("AAAA")
	_, ok := get_console_output()
	result.add_result(!ok, "log level test")

	log_inst_sync.Info("测试字符串")
	result.check_log_output_end_with("测试字符串", "basic test")
	log_inst_sync.Info("测试字符串{},{}", nil, full_str)
	result.check_log_output_end_with("测试字符串null,123", "basic param test 1")

	standard_output := "Float value result: 62.1564"
	log_inst_sync.Info("Float value result: {}", 62.15645)
	output, _ := get_console_output()
	result.add_result(strings.Contains(output, standard_output), "Float format test")

	standard_output = "这些是结果，abc, abcde, -32, FALSE, TRUE, null, 3, 3823823, -32354, 测试字符串完整的， 结果完成了"
	log_inst_sync.Info("这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了",
		"abc", "abcde", int32(-32), false, true, nil,
		int8(3), int32(3823823), int16(-32354), "测试字符串完整的")
	result.check_log_output_end_with(standard_output, "basic param test 2")

	format_prefix := "a"
	appender := strings.Repeat("a", 1024)
	for len(format_prefix) <= 1024*1024+1024+4 {
		log_inst_sync.Info(format_prefix+"这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了",
			"abc", "abcde", int32(-32), false, true, nil,
			int8(3), int32(3823823), int16(-32354), "测试字符串完整的")
		result.check_log_output_end_with(format_prefix+standard_output, "basic param test 2")
		format_prefix += appender
	}

	log_inst_async := bq.Create_log("async_log", "appenders_config.ConsoleAppender.type=console\n"+
		"appenders_config.ConsoleAppender.time_zone=localtime\n"+
		"appenders_config.ConsoleAppender.levels=[error,info]\n"+
		"\n", nil)
	format_prefix = "a"
	for len(format_prefix) <= 1024*1024+1024+4 {
		log_inst_async.Info(format_prefix+"这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了",
			"abc", "abcde", int32(-32), false, true, nil,
			int8(3), int32(3823823), int16(-32354), "测试字符串完整的")
		log_inst_async.Force_flush()
		real_output, _ := get_console_output()
		result.add_result(strings.HasSuffix(real_output, format_prefix+standard_output),
			"basic param test\n standard_end:"+format_prefix+standard_output+"\n real_output:"+real_output)
		format_prefix += appender
	}

	return result
}
