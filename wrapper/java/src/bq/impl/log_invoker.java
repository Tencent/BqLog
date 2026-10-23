package bq.impl;
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
import java.nio.ByteBuffer;

/**
 * The native entry points of the library, one per C API function. Only the Java wrapper calls
 * them; everything here is implemented by the JNI layer.
 */
public class log_invoker {
	/** Not instantiable: every entry point here is static. */
	private log_invoker() {
	}
	/**
	 * Version of the library that is loaded.
	 * @return The version string.
	 */
	public static native String __api_get_log_version();
	
	/**
	 * Asks the library to flush its buffers if the process crashes.
	 */
	public static native void __api_enable_auto_crash_handler();
	
	/**
	 * What this platform can offer for crash recovery.
	 * @return One of the recovery_capability values.
	 */
	public static native int __api_get_recovery_capability();

	/**
	 * Capability bits the library was built with.
	 * @return An OR of bq.def.platform_capability values.
	 */
	public static native int __api_get_platform_capabilities();
	
	/**
	 * Creates a log and reads its config.
	 * @param log_name Name of the log.
	 * @param config_content Content of the config file.
	 * @param category_count Number of categories the config defines.
	 * @param category_names_array Their names, in config order.
	 * @return The new log's id, or a negative error code.
	 */
	public static native long __api_create_log(String log_name, String config_content, long category_count, String[] category_names_array);

	/**
	 * Replaces the config of a log that already exists.
	 * @param log_name Name of the log.
	 * @param config_content Content of the new config file.
	 */
	public static native void __api_log_reset_config(String log_name,String config_content);
	
	/**
	 * Reserves room for one entry.
	 * @param log_id The log to write to.
	 * @param log_level Level of the entry.
	 * @param category_index Category of the entry, or a value that means none.
	 * @param format_str_bytes_len Length of the format string in bytes.
	 * @param format_str_data The format string itself.
	 * @param args_data_bytes_len Room the arguments need.
	 * @param finished True when the entry has no arguments.
	 * @return The buffers to pack the format string and the arguments into, or null when the library
	 *     dropped the entry.
	 */
	public static native ByteBuffer[] __api_log_write_begin(long log_id, byte log_level, long category_index, long format_str_bytes_len, String format_str_data, long args_data_bytes_len, boolean finished);
	
	/**
	 * Marks the entry that is being packed as complete.
	 * @param log_id The log the entry belongs to.
	 */
	public static native void __api_log_write_finish(long log_id);
	
	/**
	 * Turns one appender of a log on or off.
	 * @param log_id The log to change.
	 * @param appender_name Name of the appender, as the config lists it.
	 * @param enable True to enable it.
	 */
	public static native void __api_set_appender_enable(long log_id, String appender_name, boolean enable);
	
	/**
	 * How many logs exist at the moment.
	 * @return The number of logs.
	 */
	public static native long __api_get_logs_count();
	
	/**
	 * Id of the log at a position in the log list.
	 * @param index Position in the list.
	 * @return The log id.
	 */
	public static native long __api_get_log_id_by_index(long index);
	
	/**
	 * Name of a log.
	 * @param log_id The log to look up.
	 * @return The log's name.
	 */
	public static native String __api_get_log_name_by_id(long log_id);
	
	/**
	 * How many categories a log has.
	 * @param log_id The log to look up.
	 * @return The number of categories.
	 */
	public static native long __api_get_log_categories_count(long log_id);
	
	/**
	 * Name of one category of a log.
	 * @param log_id The log to look up.
	 * @param category_index Position of the category.
	 * @return The category's name.
	 */
	public static native String __api_get_log_category_name_by_index(long log_id, long category_index);
	
	/**
	 * Which levels are enabled for a log, as a bitmap the Java side reads directly.
	 * @param log_id The log to look up.
	 * @return A buffer holding the bitmap.
	 */
	public static native ByteBuffer __api_get_log_merged_log_level_bitmap_by_log_id(long log_id);
	
	/**
	 * Per-category level masks of a log, as a buffer the Java side reads directly.
	 * @param log_id The log to look up.
	 * @return A buffer holding one mask per category.
	 */
	public static native ByteBuffer __api_get_log_category_masks_array_by_log_id(long log_id);
	
	/**
	 * Which levels print a call stack, as a buffer the Java side reads directly.
	 * @param log_id The log to look up.
	 * @return A buffer holding the bitmap.
	 */
	public static native ByteBuffer __api_get_log_print_stack_level_bitmap_by_log_id(long log_id);

	/**
	 * The level words of a log's categories: one int per category, with bit (1 &lt;&lt; level) set when
	 * that level is enabled without a call stack. Only a library that is new enough fills this in.
	 * @param log_id The log to look up.
	 * @return A buffer holding the words, or null on an older library.
	 */
	public static native ByteBuffer __api_get_log_category_level_words_by_log_id(long log_id);
	
	/**
	 * Writes straight to the platform console (logcat on Android, stderr elsewhere).
	 * @param level Level to log at, as a log_level ordinal.
	 * @param content Text to write.
	 */
	public static native void __api_log_device_console(int/*bq.log.typedef.log_level*/ level, String content);
	
	/**
	 * Flushes a log's buffers now.
	 * @param log_id The log to flush.
	 */
	public static native void __api_force_flush(long log_id);
	
	/**
	 * The directory the library resolved for a base_dir_type.
	 * @param base_dir_type Which base directory to ask for.
	 * @return The directory path.
	 */
	public static native String __api_get_file_base_dir(int base_dir_type);

	/**
	 * Creates a decoder for a log file.
	 * @param log_file_path Path of the log file.
	 * @param priv_key Key the file was encrypted with, or an empty string.
	 * @return A decoder handle, or a negative error code.
	 */
	public static native long __api_log_decoder_create(String log_file_path, String priv_key);

	/**
	 * Decodes the next entry of a file.
	 * @param handle Handle from __api_log_decoder_create.
	 * @param out_decoded_text Receives the text of the entry.
	 * @return A decoder result code.
	 */
	public static native int __api_log_decoder_decode(long handle, bq.def.string_holder out_decoded_text);

	/**
	 * Releases a decoder.
	 * @param handle Handle from __api_log_decoder_create.
	 */
	public static native void __api_log_decoder_destroy(long handle);
	
	/**
	 * Decodes a whole file into a text file.
	 * @param in_file_path Path of the log file.
	 * @param out_file_path Path of the text file to write.
	 * @param priv_key Key the file was encrypted with, or an empty string.
	 * @return True when the file was decoded.
	 */
	public static native boolean __api_log_decode(String in_file_path, String out_file_path, String priv_key);
	
	/**
	 * Builds the snapshot text of a log.
	 * @param log_id The log to snapshot.
	 * @param time_zone_config Time zone to print the timestamps in.
	 * @return The snapshot text.
	 */
	public static native String __api_take_snapshot_string(long log_id, String time_zone_config);

	/**
	 * Routes console output to the Java callback, or stops doing that.
	 * @param enable True to send console output to Java.
	 */
	public static native void __api_set_console_callback(boolean enable);
	
	/**
	 * Keeps console output in a buffer for the Java side to fetch.
	 * @param enable True to buffer console output.
	 */
	public static native void __api_set_console_buffer_enable(boolean enable);
	
	/**
	 * Overrides the base directory the library uses for a base_dir_type.
	 * @param base_dir_type Which base directory to change.
	 * @param dir The new directory.
	 */
	public static native void __api_reset_base_dir(int base_dir_type, String dir);
	
	/**
	 * Hands the buffered console output to the callback and clears the buffer.
	 * @param callback The callback to deliver the output to.
	 * @return True when there was anything to deliver.
	 */
	public static native boolean __api_fetch_and_remove_console_buffer(Object callback);
	
	/**
	 * Tells the native side the JVM is going away, so it stops calling back into Java.
	 */
	public static native void __api_mark_jvm_destroyed();
}
