package bq.test;
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
import bq.log;

public class test_main {
    public static void main(String[] args) {
        System.out.println("Running Java Wrapper Tests...");
        try {
            // Verify library load
            String version = log.get_version();
            System.out.println("BqLog Version: " + version);
            if (version == null || version.isEmpty()) {
                throw new RuntimeException("Failed to get version");
            }
            
            test_manager.add_test(new test_log_1("Test Log Basic"));
            test_manager.add_test(new test_log_2("Test Log MultiThread"));
            boolean result = test_manager.test();
            if(result) {
            	bq.log.console(bq.def.log_level.info, "--------------------------------");
            	bq.log.console(bq.def.log_level.info, "CONGRATULATION!!! ALL TEST CASES IS PASSED");
            	bq.log.console(bq.def.log_level.info, "--------------------------------");
            }else {
            	bq.log.console(bq.def.log_level.error, "--------------------------------");
            	bq.log.console(bq.def.log_level.error, "SORRY!!! TEST CASES FAILED");
            	bq.log.console(bq.def.log_level.error, "--------------------------------");
            	System.exit(-1);
            }
        } catch (Throwable t) {
            t.printStackTrace();
            bq.log.console(bq.def.log_level.error, "--------------------------------");
        	bq.log.console(bq.def.log_level.error, "SORRY!!! TEST CASES FAILED");
        	bq.log.console(bq.def.log_level.error, "--------------------------------");
        	System.exit(-1);
        }
    }

    
}