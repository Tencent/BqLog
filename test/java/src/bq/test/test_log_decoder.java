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
import bq.tools.log_decoder;
import java.io.File;
import java.io.FilenameFilter;

public class test_log_decoder extends test_base {
	private static String find_compressed_file(String folder_path, String file_prefix)
	{
		File folder = new File(folder_path);
		File[] entries = folder.listFiles(new FilenameFilter() {
			@Override
			public boolean accept(File dir, String file_name) {
				return file_name.startsWith(file_prefix) && file_name.endsWith(".logcompr");
			}
		});
		File newest = null;
		if (entries != null) {
			for (File file : entries) {
				if (newest == null || file.lastModified() > newest.lastModified()) {
					newest = file;
				}
			}
		}
		// The appender may nest the file under a directory named after it.
		File[] sub_dirs = folder.listFiles(File::isDirectory);
		if (sub_dirs != null) {
			for (File sub_dir : sub_dirs) {
				String nested = find_compressed_file(sub_dir.getPath(), file_prefix);
				if (nested != null) {
					File nested_file = new File(nested);
					if (newest == null || nested_file.lastModified() > newest.lastModified()) {
						newest = nested_file;
					}
				}
			}
		}
		return newest == null ? null : newest.getPath();
	}

	public test_log_decoder(String name) {
		super(name);
	}

	@Override
	public test_result test() {
		test_result result = new test_result();

		bq.log log_inst = bq.log.create_log("decoder_log", "log.thread_mode=sync\n"
				+ "log.recovery=false\n"
				+ "appenders_config.FileAppender.type=compressed_file\n"
				+ "appenders_config.FileAppender.file_name=Output/decoder_in\n"
				+ "appenders_config.FileAppender.base_dir_type=0\n"
				+ "appenders_config.FileAppender.levels=[info]\n");
		result.add_result(log_inst.is_valid(), "the decoder test log is valid");
		log_inst.info("decoder test marker {}", 1);
		log_inst.force_flush();

		String input_path = find_compressed_file("Output", "decoder_in");
		result.add_result(input_path != null, "the compressed appender wrote a log file");
		if (input_path == null) {
			return result;
		}
		File input_file = new File(input_path);
		long input_size = input_file.length();

		File output_file = new File("Output/decoder_out.txt");
		if (output_file.exists()) {
			output_file.delete();
		}

		boolean decode_ok = log_decoder.decode_file(input_path, output_file.getPath());
		result.add_result(decode_ok, "decode_file reports success");
		result.add_result(output_file.exists() && output_file.length() > 0, "decode_file wrote the output file");
		// decode_file used to hand the input path in twice, which overwrote the log file.
		result.add_result(input_file.exists() && input_file.length() == input_size,
				"decode_file left the input file untouched");

		return result;
	}
}
