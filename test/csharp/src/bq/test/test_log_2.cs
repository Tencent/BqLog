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
            log_inst_sync = bq.log.get_log_by_name("sync_log");
            log_inst_async = bq.log.get_log_by_name("async_log");
            test_result result = new test_result();

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
                    Console.WriteLine("New Thread Start:" + tr.ManagedThreadId); 
                    Interlocked.Increment(ref live_thread);
                    Interlocked.Decrement(ref left_thread);
                }
                else
                {
                    Thread.Sleep(1);
                }
            }

            log_inst_async.force_flush();
            result.add_result(true, "");
            test_manager.register_default_console_callback();
            return result;
        }
    }
}
