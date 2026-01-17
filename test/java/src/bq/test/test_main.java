package bq.test;

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