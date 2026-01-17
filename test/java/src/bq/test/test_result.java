package bq.test;
import java.util.concurrent.atomic.AtomicLong;
import java.util.ArrayList;

public class test_result {
	private AtomicLong success_count = new AtomicLong(0);
	private AtomicLong total_count = new AtomicLong(0);
	private Object mutex = new Object();
	private ArrayList<String> failed_infos = new ArrayList<String>();
	
	public void add_result(boolean success, String format, Object... args) {
		total_count.getAndAdd(1);
		if(success) {
			success_count.getAndAdd(1);
		}else {
			synchronized(mutex){
				if(failed_infos.size() < 128) {
					failed_infos.add(String.format(format, args));
				}else if(failed_infos.size() == 128) {
					failed_infos.add("... Too many test case errors. A maximum of 128 can be displayed, and the rest are omitted. ....");
				}
			}
		}
	}
	
	public void check_log_output_end_with(String end_with_str, String format, Object... args)
	{
		if(test_manager.get_console_output() == null) {
			add_result(false, format, args);
		}else {
			add_result(test_manager.get_console_output().endsWith(end_with_str), format, args);
		}
	}

    public void output(String type_name) {
    	bq.def.log_level level = (is_all_pass()) ? bq.def.log_level.info : bq.def.log_level.error;
        String summary = String.format("test case %s result: %d/%d", type_name, success_count.getAcquire(), total_count.getAcquire());
        bq.log.console(level, summary);
        for (int i = 0; i < failed_infos.size(); ++i) {
        	bq.log.console(level, "\t" + failed_infos.get(i));
        }
        bq.log.console(bq.def.log_level.info, " ");
    }

    public boolean is_all_pass() { 
    	return success_count.getAcquire() == total_count.getAcquire(); 
    }
}
