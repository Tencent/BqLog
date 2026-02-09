import bq.*;
import static bq.utils.param.no_boxing;;
/**
 * @author pippocao
 *
 *	Please copy dynamic native library to your classpath before you run this demo.
 *  Or set the Native Library Location to the directory of the dynamic libraries for the current platform under `(ProjectRoot)/dist`. 
 *  Otherwise, you may encounter an `UnsatisfiedLinkError`.
 */
public class demo_main {

	public static void main(String[] args) {
		// TODO Auto-generated method stub
		String log_config =  """
				appenders_config.appender_0.type=console
				appenders_config.appender_0.time_zone=localtime
				appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]
				appenders_config.appender_0.file_name=CCLog/normal
				appenders_config.appender_0.base_dir_type=0
				appenders_config.appender_0.max_file_size=10000000
				appenders_config.appender_0.expire_time_days=10
				appenders_config.appender_0.capacity_limit=200000000
			
				appenders_config.appender_1.type=text_file
				appenders_config.appender_1.time_zone=localtime
				appenders_config.appender_1.levels=[verbose,debug,info,warning,error,fatal]
				appenders_config.appender_1.file_name=CCLog/normal
				appenders_config.appender_1.base_dir_type=0
				appenders_config.appender_1.max_file_size=1000000000
				appenders_config.appender_1.expire_time_days=10
				appenders_config.appender_1.capacity_limit=10000000000
			
				appenders_config.appender_3409.type=compressed_file
				appenders_config.appender_3409.time_zone=localtime
				appenders_config.appender_3409.levels=[verbose,debug,info,warning,error,fatal]
				appenders_config.appender_3409.file_name=CCLog/normal
				appenders_config.appender_3409.base_dir_type=0
				appenders_config.appender_3409.max_file_size=1000000000
				appenders_config.appender_3409.expire_time_days=10
				appenders_config.appender_3409.capacity_limit=8000000000

			    log.buffer_size=65535
			    log.reliable_level=normal
			""";
		

		String demo_log_config =  """
				appenders_config.appender_0.type=console
				appenders_config.appender_0.time_zone=localtime
				appenders_config.appender_0.levels=[verbose,debug,info,warning,error,fatal]
				appenders_config.appender_0.file_name=CCLog/normal
				appenders_config.appender_0.base_dir_type=0
				appenders_config.appender_0.max_file_size=10000000
				appenders_config.appender_0.expire_time_days=10
				appenders_config.appender_0.capacity_limit=200000000
			
				appenders_config.appender_1.type=text_file
				appenders_config.appender_1.time_zone=localtime
				appenders_config.appender_1.levels=[verbose,debug,info,warning,error,fatal]
				appenders_config.appender_1.file_name=CCLog/demo
				appenders_config.appender_1.base_dir_type=0
				appenders_config.appender_1.max_file_size=1000000000
				appenders_config.appender_1.expire_time_days=10
				appenders_config.appender_1.capacity_limit=10000000000
			
				appenders_config.appender_3409.type=compressed_file
				appenders_config.appender_3409.time_zone=localtime
				appenders_config.appender_3409.levels=[verbose,debug,info,warning,error,fatal]
				appenders_config.appender_3409.file_name=CCLog/demo
				appenders_config.appender_3409.base_dir_type=0
				appenders_config.appender_3409.max_file_size=1000000000
				appenders_config.appender_3409.expire_time_days=10
				appenders_config.appender_3409.capacity_limit=8000000000

			    log.buffer_size=65535
			    log.reliable_level=normal
			""";
		
		log base_log = log.create_log("MyLog", log_config);
		base_log.info("测试日志{}, {}", bq.utils.param.no_boxing(false), 5.3245f);
		
		demo_category_log my_demo_log = demo_category_log.create_log("Demo_Log", demo_log_config);
		my_demo_log.info("测试日志{}, {}", bq.utils.param.no_boxing(false), 5.3245f);
		my_demo_log.info(my_demo_log.cat.node_2.node_5, "Demo Log测试日志{}, {}", no_boxing(false), 5.3245f);
		
		
		log.force_flush_all_logs();
	}

}
