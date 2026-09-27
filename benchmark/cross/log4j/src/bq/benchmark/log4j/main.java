package bq.benchmark.log4j;

import org.apache.logging.log4j.Logger;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.core.async.AsyncLoggerContextSelector;

import static org.apache.logging.log4j.util.Unbox.box;

public class main {

    public static final Logger log_obj = LogManager.getLogger(main.class);

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.out.println("usage: main <thread_count> [mp|np]");
            return;
        }
        int thread_count = Integer.parseInt(args[0]);
        boolean multi_param = args.length < 2 || !args[1].equals("np");

        System.out.println("Is Async:" + AsyncLoggerContextSelector.isSelected());

        Thread[] threads = new Thread[thread_count];
        long start_time = System.currentTimeMillis();
        for (int idx = 0; idx < thread_count; ++idx) {
            final int t = idx;
            threads[idx] = new Thread(() -> {
                for (int i = 0; i < 2000000; ++i) {
                    if (multi_param) {
                        log_obj.info("idx:{}, num:{}, This test, {}, {}",
                            box(t), box(i), box(2.4232f), box(true));
                    } else {
                        log_obj.info("Empty Log, No Param");
                    }
                }
            });
            threads[idx].start();
        }
        for (int idx = 0; idx < thread_count; ++idx) {
            threads[idx].join();
        }

        // stop() drains the Disruptor ring buffer and flushes the appender
        ((org.apache.logging.log4j.core.LoggerContext) LogManager.getContext(false)).stop();
        LogManager.shutdown();

        long flush_time = System.currentTimeMillis();
        System.out.println("RESULT|log4j2|" + (multi_param ? "multi_param" : "no_param")
            + "|" + thread_count + "|" + (flush_time - start_time));
    }
}
