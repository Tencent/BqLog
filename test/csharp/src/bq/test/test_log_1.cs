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
namespace bq.test
{
    using System;
    using bq;
    using bq.def;

    public class test_log_1 : test_base
    {
        public test_log_1(string name) : base(name)
        {
        }

        public override test_result test()
        {
            test_result result = new test_result();
            bq.log log_inst_sync = bq.log.create_log("sync_log", "appenders_config.ConsoleAppender.type=console\n"
                    + "						appenders_config.ConsoleAppender.time_zone=localtime\n"
                    + "						appenders_config.ConsoleAppender.levels=[info, info, error,info]\n"
                    + "					\n"
                    + "						log.thread_mode=sync");


            string empty_str = null;
            string full_str = "123";

            log_inst_sync.debug("AAAA");
            result.add_result(test_manager.get_console_output() == null, "log level test");

            log_inst_sync.info("测试字符串");
            result.check_log_output_end_with("测试字符串", "basic test");
            log_inst_sync.info("测试字符串{},{}", empty_str, full_str);
            result.check_log_output_end_with("测试字符串null,123", "basic param test 1");

            string standard_output = "Float value result: 62.1564";
            log_inst_sync.info("Float value result: {}", 62.15645f);
            result.add_result(test_manager.get_console_output().Contains(standard_output), "Float format test");

            standard_output = "这些是结果，abc, abcde, -32, FALSE, TRUE, 3, 3823823, -32354, 测试字符串完整的， 结果完成了";
            log_inst_sync.info("这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了",
                "abc", "abcde", -32, false, true, (sbyte)3, 3823823, (short)-32354, "测试字符串完整的");
            result.check_log_output_end_with(standard_output, "basic param test 2");

            string format_prefix = "a";
            string appender = "";
            for (int i = 0; i < 1024; ++i)
            {
                appender += "a";
            }
            Console.WriteLine("Sync log Test Begin");
            while (format_prefix.Length <= 1024 * 1024 + 1024 + 4)
            {
                log_inst_sync.info(format_prefix + "这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了",
                    "abc", "abcde", -32, false, true, (sbyte)3, 3823823, (short)-32354, "测试字符串完整的");
                result.check_log_output_end_with(format_prefix + standard_output, "basic param test 2");
                format_prefix += appender;
            }

            bq.log log_inst_async = bq.log.create_log("async_log", "appenders_config.ConsoleAppender.type=console\n"
                    + "						appenders_config.ConsoleAppender.time_zone=localtime\n"
                    + "						appenders_config.ConsoleAppender.levels=[error,info]\n"
                    + "					\n");
            Console.WriteLine("ASync log Test Begin");
            format_prefix = "a";
            while (format_prefix.Length <= 1024 * 1024 + 1024 + 4)
            {
                log_inst_async.info(format_prefix + "这些是结果，{}, {}, {}, {}, {}, {}, {}, {}, {}， 结果完成了",
                    "abc", "abcde", -32, false, true, (sbyte)3, 3823823, (short)-32354, "测试字符串完整的");
                log_inst_async.force_flush();
                result.check_log_output_end_with(format_prefix + standard_output, "basic param test\n standard_end:" + format_prefix + standard_output + "\n real_output:" + test_manager.get_console_output());
                format_prefix += appender;
            }

            return result;
        }
    }
}
