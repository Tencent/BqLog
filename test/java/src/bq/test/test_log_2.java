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
import java.util.concurrent.atomic.AtomicInteger;


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
		log_inst_sync = bq.log.create_log("sync_log", "appenders_config.FileAppender.type=compressed_file\n"
                + "						appenders_config.FileAppender.time_zone=localtime\n"
                + "						appenders_config.FileAppender.max_file_size=100000000\n"
                + "						appenders_config.FileAppender.file_name=Output/sync_log\n"
                + "						appenders_config.FileAppender.levels=[info, info, error,info]\n"
                + "					\n"
                + "						log.thread_mode=sync");
		log_inst_async = bq.log.create_log("async_log", "appenders_config.FileAppender.type=compressed_file\n"
                + "						appenders_config.FileAppender.time_zone=localtime\n"
                + "						appenders_config.FileAppender.max_file_size=100000000\n"
                + "						appenders_config.FileAppender.file_name=Output/async_log\n"
                + "						appenders_config.FileAppender.levels=[error,info]\n"
                + "					\n");
		test_result result = new test_result();
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
		bq.log log_inst_console = bq.log.create_log("console_log", "appenders_config.Appender1.type=console\n"
				+ "						appenders_config.Appender1.time_zone=localtime\n"
				+ "						appenders_config.Appender1.levels=[all]\n"
				+ "					    log.thread_mode=sync\n");
		bq.log.set_console_buffer_enable(false);
		bq.log.register_console_callback((long log_id, int category_idx, bq.def.log_level log_level, String content) -> {
			if(log_id != 0)
			{
				result.add_result(log_id == log_inst_console.get_id(), "console callback test 1");
				result.add_result(log_level == bq.def.log_level.debug, "console callback test 2");
				result.add_result(content.endsWith("ConsoleTest"), "console callback test 3");
			}
		});
		log_inst_console.debug("ConsoleTest");
		bq.log.register_console_callback(null);
		log_inst_async.force_flush();
		result.add_result(true, "");
		return result;
	}
}
