package bq.test;
import java.io.IOException;
import java.lang.Thread.State;
import java.util.concurrent.atomic.AtomicInteger;

import bq.log;
import bq.def.log_level;

public class test_log_2 extends test_base{
	private bq.log log_inst_sync = null;
	private bq.log log_inst_async = null;
	private AtomicInteger live_thread = new AtomicInteger(0);
	private AtomicInteger left_thread = new AtomicInteger(100);
	private String appender = "";
	public test_log_2(String name) {
		super(name);
        for(int i = 0; i < 32; ++i) {
        	appender += "a";
        }
	}

	@Override
	public test_result test() {
		log_inst_sync = bq.log.get_log_by_name("sync_log");
		log_inst_async = bq.log.get_log_by_name("async_log");
		test_result result = new test_result();
		log.register_console_callback(null);
		while(left_thread.getAcquire() > 0 || live_thread.getAcquire() > 0) {
			if(left_thread.getAcquire() > 0 && live_thread.getAcquire() < 5) {
				Runnable task = new Runnable() {
					@Override
					public void run() {
						String log_content = "";
						for(int i = 0; i < 128; ++i) {
							log_content += appender;
							log_inst_sync.info(log_content);
						}
						live_thread.decrementAndGet();
					}
				};
				Thread tr = new Thread(task);
				tr.setDaemon(false);
				tr.start();
				live_thread.incrementAndGet();
				left_thread.decrementAndGet();
			}else {
				try {
					Thread.sleep(1);
				} catch (InterruptedException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				};
			}
		}

		System.out.println("Sync Test Finished");
		left_thread.setRelease(32);
		
		while(left_thread.getAcquire() > 0 || live_thread.getAcquire() > 0) {
			if(left_thread.getAcquire() > 0 && live_thread.getAcquire() < 5) {
				Runnable task = new Runnable() {
					@Override
					public void run() {
						String log_content = "";
						for(int i = 0; i < 2048; ++i) {
							log_content += appender;
							log_inst_async.info(log_content);
						}
						live_thread.decrementAndGet();
					}
				};
				Thread tr = new Thread(task);
				tr.setDaemon(false);
				tr.start();
				System.out.println("New Thread Start:" + tr.getName());
				live_thread.incrementAndGet();
				left_thread.decrementAndGet();
			}else {
				try {
					Thread.sleep(1);
				} catch (InterruptedException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				};
			}
		}
		log_inst_async.force_flush();
		result.add_result(true, "");
		test_manager.register_default_console_callback();
		return result;
	}

}
