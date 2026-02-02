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
    using System.Collections.Generic;
    using bq;
    using bq.def;

    public class test_manager
    {
        private static List<test_base> test_list = new List<test_base>();
        private static string log_console_output;

        // Delegate implementation matching bq.log.type_console_callback
        private static void default_callback(ulong log_id, int category_idx, log_level level_value, string content)
        {
            if (log_id != 0)
            {
                log_console_output = content;
            }
            else
            {
                Console.WriteLine(content);
            }
        }

        public static void add_test(test_base test_obj)
        {
            test_list.Add(test_obj);
        }

        public static void register_default_console_callback()
        {
            bq.log.register_console_callback(default_callback);
        }

        public static bool test()
        {
            register_default_console_callback();
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
            return log_console_output;
        }
    }
}
