package bq.test;

import java.util.ArrayList;

import bq.log;

public class test_manager {
	private static ArrayList<test_base> test_list = new ArrayList<test_base>();
	private static String log_console_output;
	
	public static void add_test(test_base test_obj) {
		test_list.add(test_obj);
	}
	
	public static boolean test() {
        log.register_console_callback(new bq.log.console_callback_delegate() {		
			@Override
			public void callback(long log_id, int category_idx, int log_level, String content) {
				// TODO Auto-generated method stub
				log_console_output = content;
			}
		});
        
		boolean success = true;
		for(int i = 0; i < test_list.size(); ++i) {
			test_base test_obj = test_list.get(i);
			test_result result = test_obj.test();
			result.output(test_obj.get_name());
			success &= result.is_all_pass();
		}
		return success;
	}
	
	public static String get_console_output() {
		return log_console_output;
	}
}
