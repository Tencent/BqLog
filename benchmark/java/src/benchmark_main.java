import java.io.IOException;
import java.util.*;

/**
 * @author pippocao
 *
 *	Please copy dynamic native library to your classpath before you run this benchmark.
 *  Or set the Native Library Location to the directory of the dynamic libraries for the current platform under `(ProjectRoot)/dist`. 
 *  Otherwise, you may encounter an `UnsatisfiedLinkError`.
 */
public class benchmark_main {
	
	static abstract class benchmark_thread implements Runnable
	{
		protected int idx;
		public benchmark_thread(int idx)
		{
			this.idx = idx;
		}
	}
	
	private static void test_compress_multi_param(int thread_count) throws Exception
	{
		System.out.println("============================================================");
		System.out.println("=========Begin Compressed File Log Test 1, 4 params=========");
		bq.log log_obj = bq.log.get_log_by_name("compress");
		Thread[] threads = new Thread[thread_count];

		long start_time = System.currentTimeMillis();
		System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
		for (int idx = 0; idx < thread_count; ++idx)
		{
			Runnable r = new benchmark_thread(idx) {
				@Override
				public void run()
				{
					for (int i = 0; i < 2000000; ++i)
					{
						log_obj.info("idx:{}, num:{}, This test, {}, {}", bq.utils.param.no_boxing(idx)
							, bq.utils.param.no_boxing(i)
							, bq.utils.param.no_boxing(2.4232f)
							, bq.utils.param.no_boxing(true));
					}
				}
			};
			threads[idx] = new Thread(r);
			threads[idx].start();
		}
		for (int idx = 0; idx < thread_count; ++idx)
		{
			threads[idx].join();
		}
		bq.log.force_flush_all_logs();
		long flush_time = System.currentTimeMillis();
		System.out.println("\"Time Cost:" + (flush_time - start_time));
		System.out.println("============================================================");
		System.out.println("");
	}

	private static void test_text_multi_param(int thread_count) throws Exception
	{
		System.out.println("============================================================");
		System.out.println("============Begin Text File Log Test 2, 4 params============");
		bq.log log_obj = bq.log.get_log_by_name("text");
		Thread[] threads = new Thread[thread_count];

		long start_time = System.currentTimeMillis();
		System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
		for (int idx = 0; idx < thread_count; ++idx)
		{
			Runnable r = new benchmark_thread(idx) {
				@Override
				public void run()
				{
					for (int i = 0; i < 2000000; ++i)
					{
						log_obj.info("idx:{}, num:{}, This test, {}, {}", bq.utils.param.no_boxing(idx)
							, bq.utils.param.no_boxing(i)
							, bq.utils.param.no_boxing(2.4232f)
							, bq.utils.param.no_boxing(true));
					}
				}
			};
			threads[idx] = new Thread(r);
			threads[idx].start();
		}
		for (int idx = 0; idx < thread_count; ++idx)
		{
			threads[idx].join();
		}
		bq.log.force_flush_all_logs();
		long flush_time = System.currentTimeMillis();
		System.out.println("\"Time Cost:" + (flush_time - start_time));
		System.out.println("============================================================");
		System.out.println("");
	}

	private static void test_compress_no_param(int thread_count) throws Exception
	{
		System.out.println("============================================================");
		System.out.println("=========Begin Compressed File Log Test 3, no param=========");
		bq.log log_obj = bq.log.get_log_by_name("compress");
		Thread[] threads = new Thread[thread_count];

		long start_time = System.currentTimeMillis();
		System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
		for (int idx = 0; idx < thread_count; ++idx)
		{
			Runnable r = new benchmark_thread(idx) {
				@Override
				public void run()
				{
					for (int i = 0; i < 2000000; ++i)
					{
						log_obj.info("Empty Log, No Param");
					}
				}
			};
			threads[idx] = new Thread(r);
			threads[idx].start();
		}
		for (int idx = 0; idx < thread_count; ++idx)
		{
			threads[idx].join();
		}
		bq.log.force_flush_all_logs();
		long flush_time = System.currentTimeMillis();
		System.out.println("\"Time Cost:" + (flush_time - start_time));
		System.out.println("============================================================");
		System.out.println("");
	}

	private static void test_text_no_param(int thread_count) throws Exception
	{
		System.out.println("============================================================");
		System.out.println("============Begin Text File Log Test 4, no param============");
		bq.log log_obj = bq.log.get_log_by_name("text");
		Thread[] threads = new Thread[thread_count];

		long start_time = System.currentTimeMillis();
		System.out.println("Now Begin, each thread will write 2000000 log entries, please wait the result...");
		for (int idx = 0; idx < thread_count; ++idx)
		{
			Runnable r = new benchmark_thread(idx) {
				@Override
				public void run()
				{
					for (int i = 0; i < 2000000; ++i)
					{
						log_obj.info("Empty Log, No Param");
					}
				}
			};
			threads[idx] = new Thread(r);
			threads[idx].start();
		}
		for (int idx = 0; idx < thread_count; ++idx)
		{
			threads[idx].join();
		}
		bq.log.force_flush_all_logs();
		long flush_time = System.currentTimeMillis();
		System.out.println("\"Time Cost:" + (flush_time - start_time));
		System.out.println("============================================================");
		System.out.println("");
	}
	

	public static void main(String[] args) throws Exception {
		// TODO Auto-generated method stub
		bq.log compressed_log =  bq.log.create_log("compress", """
				appenders_config.appender_3.type=compressed_file
				appenders_config.appender_3.levels=[all]
				appenders_config.appender_3.file_name= benchmark_output/compress_
				appenders_config.appender_3.capacity_limit= 1
			""");

		bq.log text_log =  bq.log.create_log("text", """
				appenders_config.appender_3.type=text_file
				appenders_config.appender_3.levels=[all]
				appenders_config.appender_3.file_name= benchmark_output/text_
				appenders_config.appender_3.capacity_limit= 1
			""");
		

		System.out.println("Please input the number of threads which will write log simultaneously:");
		int thread_count = 0;
		Scanner scanner = new Scanner(System.in);
		try {
			thread_count = scanner.nextInt();
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			return;
		}finally {
			scanner.close();
		}

		compressed_log.verbose("use this log to trigger capacity_limit make sure old log files is deleted");
		text_log.verbose("use this log to trigger capacity_limit make sure old log files is deleted");
		bq.log.force_flush_all_logs();

		test_compress_multi_param(thread_count);
		test_text_multi_param(thread_count);
		test_compress_no_param(thread_count);
		test_text_no_param(thread_count);
	}

}
