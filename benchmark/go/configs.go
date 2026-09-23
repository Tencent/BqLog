// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package main

// These configurations match benchmark/cpp/main.cpp; checked by Test_cpp_parity.
var log_configs = []struct{ name, config string }{
	{"compress", `
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/compress
		appenders_config.appender_3.capacity_limit=1
    `},
	{"compress_enc", `
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/compress_enc
		appenders_config.appender_3.capacity_limit=1
        appenders_config.appender_3.pub_key=ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+FonyOHuS2uC6IZc16bfd6qQk4ykBOt3nTfBFcNr8ZWvvcf4H0hFkrpMtQ0AJO057GhVTQCCfnvfStSq2Yra+O5VGpI5Q6NLrUuVERimjNgwtxbXt3P8Nw87jEIJiY/8m2FUXhZEPwoA7t+2/953cNE1itJskJtojwaUlMN0dXBJxs4NP8MfBPPZQ5vNV8xgEf1SCQzQBAJsofy1kPHHqJNBXUBsNA44SP5H95JOz+r0oaNkYxT88Zk4tbk5N3hk5aXyZVp49OqhrXCPf5owDa4Lqk4UzVTk9EimxvtSuiUTzr7IJhHYy7jsGnSgq6dH0xlUfxKeX pippocao@PIPPOCAO-PC6
	`},
	{"text", `
        appenders_config.appender_3.type=text_file
        appenders_config.appender_3.levels=[all]
        appenders_config.appender_3.file_name= benchmark_output/text
        appenders_config.appender_3.capacity_limit=1
    `},
	{"test_ascii_u8", `
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_ascii_u8
		appenders_config.appender_3.capacity_limit=1
	`},
	{"test_ascii_u16", `
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_ascii_u16
		appenders_config.appender_3.capacity_limit=1
    `},
	{"test_chinese_u16", `
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_chinese_u16
		appenders_config.appender_3.capacity_limit=1
    `},
	{"test_mixed_u16", `
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_mixed_u16
		appenders_config.appender_3.capacity_limit=1
	`},
	{"test_multi_format", `
		appenders_config.appender_3.type=compressed_file
		appenders_config.appender_3.levels=[all]
		appenders_config.appender_3.file_name= benchmark_output/test_multi_format
		appenders_config.appender_3.capacity_limit=1
	`},
}
