package bq.test;

import java.util.ArrayList;

import bq.log;
import bq.def.log_level;

public class test_manager {
	private static ArrayList<test_base> test_list = new ArrayList<test_base>();
	private static String log_console_output;
	private static bq.log.console_callback_delegate default_callback = new bq.log.console_callback_delegate() {
		@Override
		public void callback(long log_id, int category_idx, log_level level_value, String content) {
			// TODO Auto-generated method stub
			if(log_id != 0) {
				log_console_output = content;
			}else {
				System.out.println(content);
			}
		}
	};
	
	public static void add_test(test_base test_obj) {
		test_list.add(test_obj);
	}
	
	public static void register_default_console_callback() {
        log.register_console_callback(default_callback);
	}
	
	public static boolean test() {
		register_default_console_callback();
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
