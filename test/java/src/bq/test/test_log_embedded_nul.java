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
import bq.def.log_level;
import bq.tools.log_decoder;
import java.io.File;

// Text that bq hands back to Java used to be cut at the first '\0', and characters outside the BMP
// came back garbled. Every path that returns log text has to return all of it.
public class test_log_embedded_nul extends test_base {
	private static final String TEXT = "before\0after \u4E2D\u6587 \uD83D\uDE00";

	public test_log_embedded_nul(String name) {
		super(name);
	}

	@Override
	public test_result test() {
		test_result result = new test_result();
		bq.log log_inst = bq.log.create_log("embedded_nul_log", "log.thread_mode=sync\n"
				+ "log.recovery=false\n"
				+ "appenders_config.ConsoleAppender.type=console\n"
				+ "appenders_config.ConsoleAppender.levels=[all]\n"
				+ "appenders_config.FileAppender.type=compressed_file\n"
				+ "appenders_config.FileAppender.file_name=Output/embedded_nul\n"
				+ "appenders_config.FileAppender.base_dir_type=0\n"
				+ "appenders_config.FileAppender.levels=[all]\n"
				+ "snapshot.buffer_size=65536\n"
				+ "snapshot.levels=[all]\n");
		result.add_result(log_inst.is_valid(), "the embedded NUL test log is valid");
		final long log_id = log_inst.get_id();

		bq.log.set_console_buffer_enable(false);
		final String[] callback_text = { null };
		bq.log.register_console_callback((long id, int category_idx, log_level level, String content) -> {
			if (id == log_id) {
				callback_text[0] = content;
			}
		});
		log_inst.info(TEXT);
		bq.log.register_console_callback(null);
		result.add_result(callback_text[0] != null && callback_text[0].endsWith(TEXT), "console callback keeps the text after NUL");

		bq.log.set_console_buffer_enable(true);
		log_inst.info(TEXT);
		String fetched = test_manager.get_console_output();
		result.add_result(fetched != null && fetched.endsWith(TEXT), "console buffer keeps the text after NUL");

		String snapshot = log_inst.take_snapshot("gmt");
		result.add_result(snapshot.endsWith(TEXT + "\n"), "snapshot keeps the text after NUL");

		log_inst.force_flush();
		String input_path = test_log_decoder.find_compressed_file("Output", "embedded_nul");
		result.add_result(input_path != null, "the compressed appender wrote a log file");
		if (input_path != null) {
			String last_entry = null;
			try (log_decoder decoder = new log_decoder(new File(input_path).getAbsolutePath())) {
				while (decoder.decode() == log_decoder.appender_decode_result.success) {
					last_entry = decoder.get_last_decoded_log_entry();
				}
			}
			result.add_result(last_entry != null && last_entry.contains(TEXT), "decoder keeps the text after NUL");
		}
		return result;
	}
}
