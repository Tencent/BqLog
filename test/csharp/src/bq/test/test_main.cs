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

    class test_main
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Running C# Wrapper Tests...");
            try
            {
                // Ensure native lib loading if needed, though usually automatic via P/Invoke if DLL is in path.
                // In Java "System.loadLibrary" was called in static block of bq.log.
                // In C#, it depends on OS loader.
                
                string version = bq.log.get_version();
                Console.WriteLine("BqLog Version: " + version);
                if (string.IsNullOrEmpty(version))
                {
                    throw new Exception("Failed to get version");
                }

                test_manager.add_test(new test_log_1("Test Log Basic"));
                test_manager.add_test(new test_log_2("Test Log MultiThread"));
                
                bool result = test_manager.test();
                if (result)
                {
                    bq.log.console(bq.def.log_level.info, "--------------------------------");
                    bq.log.console(bq.def.log_level.info, "CONGRATULATION!!! ALL TEST CASES IS PASSED");
                    bq.log.console(bq.def.log_level.info, "--------------------------------");
                }
                else
                {
                    bq.log.console(bq.def.log_level.error, "--------------------------------");
                    bq.log.console(bq.def.log_level.error, "SORRY!!! TEST CASES FAILED");
                    bq.log.console(bq.def.log_level.error, "--------------------------------");
                    Environment.Exit(-1);
                }
            }
            catch (Exception t)
            {
                Console.WriteLine(t.ToString());
                bq.log.console(bq.def.log_level.error, "--------------------------------");
                bq.log.console(bq.def.log_level.error, "SORRY!!! TEST CASES FAILED");
                bq.log.console(bq.def.log_level.error, "--------------------------------");
                Environment.Exit(-1);
            }
        }
    }
}
