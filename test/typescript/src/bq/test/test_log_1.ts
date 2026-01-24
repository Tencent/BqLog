/*
 * Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
import { test_base } from "./test_base.ts";
import { test_result } from "./test_result.ts";
import { bq } from "./bq_lib.ts";
import { test_manager } from "./test_manager.ts";

export class test_log_1 extends test_base {
    constructor(name: string) {
        super(name);
    }

    public async test(): Promise<test_result> {
        const result = new test_result();
        const log_inst_sync = bq.log.create_log("sync_log", `appenders_config.ConsoleAppender.type=console
                        appenders_config.ConsoleAppender.time_zone=localtime
                        appenders_config.ConsoleAppender.levels=[info, info, error,info]
                    
                        log.thread_mode=sync`);

        const empty_str = null;
        const full_str = "123";

        log_inst_sync.debug("AAAA");
        result.add_result(test_manager.get_console_output() == "", "log level test");

        log_inst_sync.info("测试字符串");
        result.check_log_output_end_with("测试字符串", "basic test");
        log_inst_sync.info("测试字符串{},{}", empty_str, full_str);
        result.check_log_output_end_with("测试字符串null,123", "basic param test 1");

        const standard_output_1 = "Float value result: 62.1564";
        log_inst_sync.info("Float value result: {}", 62.15645);
        result.add_result(test_manager.get_console_output().includes(standard_output_1), "Float format test");

        const standard_output = "这些是结果，abc, abcde, -32, FALSE, TRUE, null, 3, 3823823, -32354, 测试字符串完整的， 结果完成了";
        
        log_inst_sync.info("这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了", 
            "abc", "abcde", -32, false, true, null, 3, 3823823, -32354, "测试字符串完整的");
        result.check_log_output_end_with(standard_output, "basic param test 2");

        let format_prefix = "a";
        let appender = "";
        for(let i = 0; i < 1024; ++i) {
            appender += "a";
        }
        while(format_prefix.length <= 1024 * 1024 + 1024 + 4) {
            log_inst_sync.info(format_prefix + "这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了", 
                "abc", "abcde", -32, false, true, null, 3, 3823823, -32354, "测试字符串完整的");
            result.check_log_output_end_with(format_prefix + standard_output, "basic param test 2");
            format_prefix += appender;
        }

        const log_inst_async = bq.log.create_log("async_log", `appenders_config.ConsoleAppender.type=console
                        appenders_config.ConsoleAppender.time_zone=localtime
                        appenders_config.ConsoleAppender.levels=[error,info]
                    
                        `);
        format_prefix = "a";
        while(format_prefix.length <= 1024 * 1024 + 1024 + 4) {
            log_inst_async.info(format_prefix + "这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了", 
                "abc", "abcde", -32, false, true, null, 3, 3823823, -32354, "测试字符串完整的");
            log_inst_async.force_flush();
            result.check_log_output_end_with(format_prefix + standard_output, "basic param test");
            format_prefix += appender;
        }

        return result;
    }
}
