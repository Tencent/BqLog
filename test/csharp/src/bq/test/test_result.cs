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
    using System.Collections.Generic;
    using System;

    public class test_result
    {
        private List<string> errors = new List<string>();
        private int pass_count = 0;

        public void add_result(bool result, string error_msg)
        {
            if (result)
            {
                pass_count++;
            }
            else
            {
                errors.Add(error_msg);
            }
        }

        public void check_log_output_end_with(string standard_log, string error_msg)
        {
            string output = test_manager.get_console_output();
            bool result = false;
            if (output != null && output.EndsWith(standard_log, StringComparison.Ordinal))
            {
                result = true;
            }
            add_result(result, error_msg + " [Expected ends with: " + standard_log + ", Actual: " + output + "]");
        }

        public bool is_all_pass()
        {
            return errors.Count == 0;
        }

        public void output(string test_name)
        {
            if (is_all_pass())
            {
                Console.WriteLine("[PASS] " + test_name);
            }
            else
            {
                Console.WriteLine("[FAIL] " + test_name);
                foreach (var err in errors)
                {
                    Console.WriteLine("    - " + err);
                }
            }
        }
    }
}
