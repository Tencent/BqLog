package bq.test;

public class test_log_1 extends test_base{
	public test_log_1(String name) {
		super(name);
	}
	
	@Override
	public test_result test() {
		test_result result = new test_result();
		bq.log log_inst = bq.log.create_log("basic_log", "appenders_config.ConsoleAppender.type=console\n"
				+ "						appenders_config.ConsoleAppender.time_zone=localtime\n"
				+ "						appenders_config.ConsoleAppender.levels=[error,fatal]\n"
				+ "					\n"
				+ "						log.thread_mode=sync");
		
		
		String empty_str = null;
        String full_str = "123";
        
        log_inst.debug("AAAA");
        result.add_result(test_manager.get_console_output() == null, "log level test");
        
        log_inst.fatal("测试字符串");
        result.check_log_output_end_with("测试字符串", "basic test");
        log_inst.fatal("测试字符串{},{}", empty_str, full_str);
        result.check_log_output_end_with("测试字符串null,123", "basic param test 1");
        
        
        return result;
	}
	

}
