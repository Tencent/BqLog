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
    using System.Threading;
    using System.Collections.Generic;
    using bq;
    using bq.def;
    using System.Threading.Tasks;

    public class test_manager
    {
        private static List<test_base> test_list = new List<test_base>();
        private static long fetch_count = 0;
        private static string log_console_output = null;

        public static void add_test(test_base test_obj)
        {
            test_list.Add(test_obj);
        }

        public static bool test()
        {
            bq.log.set_console_buffer_enable(true);
            Task.Run(() => {
                long last_fetch_count = Interlocked.Read(ref fetch_count);
                while (true)
                {
                    long new_fetch_count = Interlocked.Read(ref fetch_count);
                    while (true)
                    {
                        bool fetch_result = bq.log.fetch_and_remove_console_buffer((log_id, category_idx, log_level, content) =>
                        {
                            log_console_output = content;
                        });
                        if (!fetch_result)
                        {
                            break;
                        }
                    }
                    if(new_fetch_count != last_fetch_count)
                    {
                        ++new_fetch_count;
                        Interlocked.Exchange(ref fetch_count, new_fetch_count);
                    }
                    last_fetch_count = new_fetch_count;
                    Thread.Sleep(1);
                }
            });
            bool success = true;
            foreach (var test_obj in test_list)
            {
                test_result result = test_obj.test();
                result.output(test_obj.get_name());
                success &= result.is_all_pass();
            }
            return success;
        }

        public static string get_console_output()
        {
            long prev = Interlocked.Read(ref fetch_count);
            Interlocked.Exchange(ref fetch_count, prev + 1);
            while (Interlocked.Read(ref fetch_count) != prev + 2)
            {
                Thread.Yield();
            }
            return log_console_output;
        }
    }
}
