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
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicLong;
import bq.def.log_level;

public class test_manager {
	private static ArrayList<test_base> test_list = new ArrayList<test_base>();
	private static AtomicLong fetch_count = new AtomicLong(0);
	private static String log_console_output;
	
	public static void add_test(test_base test_obj) {
		test_list.add(test_obj);
	}
	
	public static boolean test() {
		bq.log.set_console_buffer_enable(true);
        Thread tr = new Thread(new Runnable() {
            @Override
            public void run() {
                long last_fetch_count = 0;
                while(true) {
                    long new_fetch_count = fetch_count.get();
                    while(true) {
                        boolean fetch_result = bq.log.fetch_and_remove_console_buffer((long log_id, int category_idx, log_level level, String content) -> {
                            log_console_output = content;
                        });
                        if(!fetch_result) {
                            break;
                        }
                    }
                    if(new_fetch_count != last_fetch_count) {
                        ++new_fetch_count;
                        fetch_count.set(new_fetch_count);
                    }
                    last_fetch_count = new_fetch_count;
                    try {
                        Thread.sleep(1);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
        });
        tr.setDaemon(true);
        tr.start();
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
        long prev = fetch_count.get();
        fetch_count.set(prev + 1);
        while(fetch_count.get() != prev + 2) {
            Thread.yield();
        }
		return log_console_output;
	}
}
