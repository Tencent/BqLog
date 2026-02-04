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
    using System.Threading;
    using bq;
    using bq.def;

    public class test_log_2 : test_base
    {
        private bq.log log_inst_sync = null;
        private bq.log log_inst_async = null;
        private int live_thread = 0;
        private int left_thread = 100;
        private string appender = "";

        public test_log_2(string name) : base(name)
        {
            for (int i = 0; i < 32; ++i)
            {
                appender += "a";
            }
        }

        public override test_result test()
        {
            log_inst_sync = bq.log.create_log("sync_log", "appenders_config.FileAppender.type=compressed_file\n"
                    + "						appenders_config.FileAppender.time_zone=localtime\n"
                    + "						appenders_config.FileAppender.max_file_size=100000000\n"
                    + "						appenders_config.FileAppender.file_name=Output/sync_log\n"
                    + "						appenders_config.FileAppender.levels=[info, info, error,info]\n"
                    + "					\n"
                    + "						log.thread_mode=sync");
            log_inst_async = bq.log.create_log("async_log", "appenders_config.FileAppender.type=compressed_file\n"
                    + "						appenders_config.FileAppender.time_zone=localtime\n"
                    + "						appenders_config.FileAppender.max_file_size=100000000\n"
                    + "						appenders_config.FileAppender.file_name=Output/async_log\n"
                    + "						appenders_config.FileAppender.levels=[error,info]\n"
                    + "					\n");

            test_result result = new test_result();

            Console.WriteLine("Pressure Sync log Test Begin");
            // Sync Test
            while (Volatile.Read(ref left_thread) > 0 || Volatile.Read(ref live_thread) > 0)
            {
                if (Volatile.Read(ref left_thread) > 0 && Volatile.Read(ref live_thread) < 5)
                {
                    Thread tr = new Thread(() =>
                    {
                        string log_content = "";
                        for (int i = 0; i < 128; ++i)
                        {
                            log_content += appender;
                            log_inst_sync.info(log_content);
                        }
                        Interlocked.Decrement(ref live_thread);
                    });
                    tr.IsBackground = false; // setDaemon(false)
                    tr.Start();
                    Interlocked.Increment(ref live_thread);
                    Interlocked.Decrement(ref left_thread);
                }
                else
                {
                    Thread.Sleep(1);
                }
            }
            Console.WriteLine("Sync Test Finished");

            Console.WriteLine("Pressure ASync log Test Begin");
            // Async Test
            Interlocked.Exchange(ref left_thread, 128);

            while (Volatile.Read(ref left_thread) > 0 || Volatile.Read(ref live_thread) > 0)
            {
                if (Volatile.Read(ref left_thread) > 0 && Volatile.Read(ref live_thread) < 5)
                {
                    Thread tr = new Thread(() =>
                    {
                        string log_content = "";
                        for (int i = 0; i < 2048; ++i)
                        {
                            log_content += appender;
                            log_inst_async.info(log_content);
                        }
                        Interlocked.Decrement(ref live_thread);
                    });
                    tr.IsBackground = false;
                    tr.Start();
                    Interlocked.Increment(ref live_thread);
                    Interlocked.Decrement(ref left_thread);
                }
                else
                {
                    Thread.Sleep(1);
                }
            }
            Console.WriteLine("ASync Test Finished");


            bq.log log_inst_console = bq.log.create_log("console_log", "appenders_config.Appender1.type=console\n"
                    + "						appenders_config.Appender1.time_zone=localtime\n"
                    + "						appenders_config.Appender1.levels=[all]\n"
                    + "					    log.thread_mode=sync\n");
            bq.log.set_console_buffer_enable(false);
#pragma warning disable CS0618 // ���ͻ��Ա�ѹ�ʱ
            bq.log.register_console_callback((ulong log_id, int category_idx, bq.def.log_level log_level, string content) => {
                if(log_id != 0)
                {
                    result.add_result(log_id == log_inst_console.get_id(), "console callback test 1");
                    result.add_result(log_level == bq.def.log_level.debug, "console callback test 2");
                    result.add_result(content.EndsWith("ConsoleTest"), "console callback test 3");
                }
            });
#pragma warning restore CS0618 // ���ͻ��Ա�ѹ�ʱ
            log_inst_console.debug("ConsoleTest");

            log_inst_async.force_flush();
            result.add_result(true, "");
            return result;
        }
    }
}
