package bq.impl;
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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

public class log_invoker {
	public static native String __api_get_log_version();
	
	public static native void __api_enable_auto_crash_handler();
	
	public static native long __api_create_log(String log_name, String config_content, long category_count, String[] category_names_array);

	public static native void __api_log_reset_config(String log_name,String config_content);
	
	public static native java.nio.ByteBuffer __api_get_log_ring_buffer(long log_id);
	
	public static native long __api_log_buffer_alloc(long log_id, long length, short level, long category_index, String fmt_string, long string_utf16_byte_len);
	
	public static native void __api_log_arg_push_utf16_string(long log_id, long offset, String str, long string_utf16_byte_len);
	
	public static native void __api_log_buffer_commit(long log_id, long handle);
	
	public static native void __api_set_appenders_enable(long log_id, String appender_name, boolean enable);
	
	public static native long __api_get_logs_count();
	
	public static native long __api_get_log_id_by_index(long index);
	
	public static native String __api_get_log_name_by_id(long log_id);
	
	public static native long __api_get_log_categories_count(long log_id);
	
	public static native String __api_get_log_category_name_by_index(long log_id, long category_index);
	
	public static native ByteBuffer __api_get_log_merged_log_level_bitmap_by_log_id(long log_id);
	
	public static native ByteBuffer __api_get_log_category_masks_array_by_log_id(long log_id);
	
	public static native ByteBuffer __api_get_log_print_stack_level_bitmap_by_log_id(long log_id);
	
	public static native void __api_log_device_console(int/*bq.log.typedef.log_level*/ level, String content);
	
	public static native void __api_force_flush(long log_id);
	
	public static native String __api_get_file_base_dir(boolean is_in_sandbox);

	public static native long __api_log_decoder_create(String log_file_path);

	public static native int __api_log_decoder_decode(long handle, bq.def.string_holder out_decoded_text);

	public static native void __api_log_decoder_destroy(long handle);
	
	public static native boolean __api_log_decode(String in_file_path, String out_file_path);
	
	public static native void __api_enable_snapshot(long log_id, long snapshot_buffer_size);
	
	public static native String __api_take_snapshot_string(long log_id, boolean use_gmt_time);

	public static native void __api_set_console_callback(boolean enable);
	
	public static native void __api_set_console_buffer_enable(boolean enable);
	
	public static native boolean __api_fetch_and_remove_console_buffer(Object callback);
}
