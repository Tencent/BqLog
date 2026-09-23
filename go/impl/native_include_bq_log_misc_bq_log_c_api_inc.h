/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
// Shared declarations, included only by bq_log_c_api.h. No include guard.
// BQ_LOG_API_TYPE selects scoped C++ types or prefixed C ABI types.

/// <summary>
/// get Version
/// </summary>
/// <returns></returns>
BQ_API_DEF(const char*, __api_get_log_version, (void), ());

/// <summary>
/// BqLog is asynchronous by default, logs may not be flushed when crash is occurred.
/// You can call __api_force_flush manually in your crash handler.
/// Or just call __api_enable_auto_crash_handler
/// </summary>
/// <returns></returns>
BQ_API_DEF(void, __api_enable_auto_crash_handler, (void), ());

/// <summary>
/// 0 means create failed
/// </summary>
/// <param name="log_name_utf8">log name</param>
/// <param name="config_content_utf8">config content</param>
/// <param name="category_count">categories count</param>
/// <param name="category_names_array_utf8">category names in utf8 encoding, it's char* array</param>
/// <returns>log id, 0 means create failed</returns>
BQ_API_DEF(uint64_t, __api_create_log, (const char* log_name_utf8, const char* config_content_utf8, uint32_t category_count, const char* const* category_names_array_utf8), (log_name_utf8, config_content_utf8, category_count, category_names_array_utf8));

/// <summary>
/// 0 means reset failed
/// </summary>
/// <param name="log_name_utf8">log name</param>
/// <param name="config_content_utf8">json config content</param>
BQ_API_DEF(bool, __api_log_reset_config, (const char* log_name_utf8, const char* config_content_utf8), (log_name_utf8, config_content_utf8));

/// <summary>
/// Initiates a log write operation and allocates the necessary buffer space.
/// </summary>
/// <remarks>
/// If manual population is required, the starting address for argument data is calculated as:
/// <code>handle.format_data_addr + align4(format_str_bytes_len)</code>
/// </remarks>
/// <param name="log_id">The unique identifier for the log entry.</param>
/// <param name="log_level">The severity level of the log.</param>
/// <param name="category_index">The index of the log category.</param>
/// <param name="format_string_type">The character encoding type of the format string (e.g., log_arg_type_enum::string_utf16_type).</param>
/// <param name="format_str_bytes_len">The length of the format string in bytes.</param>
/// <param name="format_str_data">
/// Pointer to the format string data.
/// If NULL, indicates a zero-copy scenario where the caller must manually write data to the allocated buffer.
/// </param>
/// <param name="args_data_bytes_len">The length of the variable arguments data in bytes.</param>
/// <returns>A handle to the allocated log buffer chunk.</returns>
BQ_API_DEF(BQ_LOG_API_TYPE(_api_log_write_handle, bq_api_log_write_handle), __api_log_write_begin, (uint64_t log_id, uint8_t log_level, uint32_t category_index, uint8_t format_string_type, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_data_bytes_len), (log_id, log_level, category_index, format_string_type, format_str_bytes_len, format_str_data, args_data_bytes_len));

/// <summary>
/// Finalizes the log write operation and commits the data to the ring buffer.
/// </summary>
/// <param name="log_id">The unique identifier for the log entry.</param>
/// <param name="write_handle">The handle returned by <see cref="__api_log_write_begin"/>.</param>
BQ_API_DEF(void, __api_log_write_finish, (uint64_t log_id, BQ_LOG_API_TYPE(_api_log_write_handle, bq_api_log_write_handle) write_handle), (log_id, write_handle));

/// <summary>
/// toggle of all console appenders,
/// you can disable it to optimize performance in release version
/// </summary>
/// <param name="enable"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_set_appender_enable, (uint64_t log_id, const char* appender_name, bool enable), (log_id, appender_name, enable));

/// <summary>
/// get the current logs count
/// </summary>
/// <returns></returns>
BQ_API_DEF(uint32_t, __api_get_logs_count, (void), ());

/// <summary>
/// get log id by index
/// </summary>
/// <param name="index"></param>
/// <returns>id of log at index, 0 if index exceed valid range</returns>
BQ_API_DEF(uint64_t, __api_get_log_id_by_index, (uint32_t index), (index));

/// <summary>
/// get log name by id
/// </summary>
/// <param name="log_id"></param>
/// <param name="name_ptr"></param>
/// <returns>false if log is not found</returns>
BQ_API_DEF(bool, __api_get_log_name_by_id, (uint64_t log_id, BQ_LOG_API_TYPE(_api_string_def, bq_api_string_def)* name_ptr), (log_id, name_ptr));

/// <summary>
/// get the count of category items in log
/// </summary>
/// <param name="log_id"></param>
/// <returns></returns>
BQ_API_DEF(uint32_t, __api_get_log_categories_count, (uint64_t log_id), (log_id));

/// <summary>
/// get name of a category by index
/// </summary>
/// <param name="log_id"></param>
/// <param name="category_index"></param>
/// <param name="category_name_ptr">the pointer of category item name</param>
/// <returns>false if index exceed valid range or log is not found</returns>
BQ_API_DEF(bool, __api_get_log_category_name_by_index, (uint64_t log_id, uint32_t category_index, BQ_LOG_API_TYPE(_api_string_def, bq_api_string_def)* category_name_ptr), (log_id, category_index, category_name_ptr));

/// <summary>
/// get the address of log level bitmap of log, this address is always valid.
/// </summary>
/// <param name="log_id"></param>
/// <returns>nullptr if log is not found</returns>
BQ_API_DEF(const uint32_t*, __api_get_log_merged_log_level_bitmap_by_log_id, (uint64_t log_id), (log_id));

/// <summary>
/// get the address of log print stack trace level bitmap of log, this address is always valid.
/// </summary>
/// <param name="log_id"></param>
/// <returns>nullptr if log is not found</returns>
BQ_API_DEF(const uint32_t*, __api_get_log_print_stack_level_bitmap_by_log_id, (uint64_t log_id), (log_id));

/// <summary>
/// get the address of category masks array, this array address is always valid.
/// </summary>
/// <param name="log_id"></param>
/// <returns>nullptr if log is not found or the categories list is empty</returns>
BQ_API_DEF(const uint8_t*, __api_get_log_category_masks_array_by_log_id, (uint64_t log_id), (log_id));

/// <summary>
/// output log to device console synchronously.
/// Android : LogCat, iOS:NSLog, Others:Standard Output
/// </summary>
/// <param name="level"></param>
/// <param name="content"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_log_device_console, (BQ_LOG_API_TYPE(log_level, bq_log_level) level, const char* content), (level, content));

/// <summary>
/// Bq Log is a asynchronous system.
/// you can call this function manually in some case for example in crash handler
/// to ensure all the logs can be save to log file.
/// Important: This function is signal-safety
/// </summary>
/// <param name="log_id">the id of log object you want to flush, or pass 0 to flush all the log objects</param>
/// <returns></returns>
BQ_API_DEF(void, __api_force_flush, (uint64_t log_id), (log_id));

/// <summary>
/// get file base dir
/// android, iOS, harmonyOS storage path is distinguished by "base_dir_type"(internal storage or external storage)
/// </summary>
/// <param name="base_dir_type"></param>
/// <returns></returns>
BQ_API_DEF(const char*, __api_get_file_base_dir, (int32_t base_dir_type), (base_dir_type));

/// <summary>
/// create a decoder to decode binary log file
/// </summary>
/// <param name="log_file_path"></param>
/// <param name="priv_key"></param>
/// <param name="out_handle">will be used in __api_log_decoder_decode and __api_log_decoder_destroy</param>
/// <returns></returns>
BQ_API_DEF(BQ_LOG_API_TYPE(appender_decode_result, bq_appender_decode_result), __api_log_decoder_create, (const char* log_file_path, const char* priv_key, uint32_t* out_handle), (log_file_path, priv_key, out_handle));

/// <summary>
/// decode binary log file
/// </summary>
/// <param name="handle"></param>
/// <param name="out_decoded_log_text">if result is success, this is the decoded log text, and it's always valid before next time you call decode_single_item or destroy_decoder</param>
/// <returns>if result is not success, you should stop invoke this function, and call __api_log_decoder_destroy to release memory</returns>
BQ_API_DEF(BQ_LOG_API_TYPE(appender_decode_result, bq_appender_decode_result), __api_log_decoder_decode, (uint32_t handle, BQ_LOG_API_TYPE(_api_string_def, bq_api_string_def)* out_decoded_log_text), (handle, out_decoded_log_text));

/// <summary>
/// destroy decoder to release memory
/// </summary>
/// <param name="handle"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_log_decoder_destroy, (uint32_t handle), (handle));

/// <summary>
/// Directly decode a log file to a text file.
/// </summary>
/// <param name="in_file_path"></param>
/// <param name="out_file_path"></param>
/// <param name="priv_key"></param>
/// <returns></returns>
BQ_API_DEF(bool, __api_log_decode, (const char* in_file_path, const char* out_file_path, const char* priv_key), (in_file_path, out_file_path, priv_key));

/// <summary>
/// Register a callback which will be invoked when each log entry was written to Console
/// </summary>
/// <param name="on_console_callback"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_register_console_callbacks, (BQ_LOG_API_TYPE(type_func_ptr_console_callback, bq_console_callback) on_console_callback), (on_console_callback));

/// <summary>
/// Unregister a console callback
/// </summary>
/// <param name="on_console_callback"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_unregister_console_callbacks, (BQ_LOG_API_TYPE(type_func_ptr_console_callback, bq_console_callback) on_console_callback), (on_console_callback));

/// <summary>
/// set console appender buffer enable or not
/// </summary>
/// <param name="enable"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_set_console_buffer_enable, (bool enable), (enable));

/// <summary>
/// reset the base dir in runtime
/// </summary>
/// <param name="base_dir_type"></param>
/// <param name="dir"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_reset_base_dir, (int32_t base_dir_type, const char* dir), (base_dir_type, dir));

/// <summary>
/// Fetch and remove a log entry from the console appender buffer in a thread-safe manner.
/// If the console appender buffer is not empty, the on_console_callback function will be invoked for this log entry.
/// Please ensure not to output synchronized BQ logs within the callback function.
/// </summary>
/// <param name="on_console_callback"></param>
/// <param name="pass_through_param">path through parameter that will pass to on_console_callback</param>
/// <returns>True if the console appender buffer is not empty, otherwise False is returned.</returns>
BQ_API_DEF(bool, __api_fetch_and_remove_console_buffer, (BQ_LOG_API_TYPE(type_func_ptr_console_buffer_fetch_callback, bq_console_buffer_fetch_callback) on_console_callback, const void* pass_through_param), (on_console_callback, pass_through_param));

/// <summary>
/// Note: if snapshot is not enabled, this API will return empty snapshot string.
/// </summary>
/// <param name="log_id"></param>
/// <param name="time_zone_config_utf8"></param>
/// <param name="out_snapshot_string"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_take_snapshot_string, (uint64_t log_id, const char* time_zone_config_utf8, BQ_LOG_API_TYPE(_api_string_def, bq_api_string_def)* out_snapshot_string), (log_id, time_zone_config_utf8, out_snapshot_string));

/// <summary>
/// The functions __api_take_snapshot_string and __api_release_snapshot_string must be called in pairs.
/// The snapshot string is guaranteed to be valid only before __api_release_snapshot_string is called.
/// If they are not called in pairs, a crash or deadlock may occur.
/// and the snapshot string is only valid til
/// </summary>
/// <param name="log_id"></param>
/// <param name="snapshot_string"></param>
/// <returns></returns>
BQ_API_DEF(void, __api_release_snapshot_string, (uint64_t log_id, BQ_LOG_API_TYPE(_api_string_def, bq_api_string_def)* snapshot_string), (log_id, snapshot_string));

/// <summary>
/// Get stack trace of current thread.
/// The result is safe only in current thread until next call to this api.
/// </summary>
/// <param name="out_name_ptr"></param>
/// <param name="skip_frame_count">the count of stack frames you want to skip</param>
/// <returns></returns>
BQ_API_DEF(void, __api_get_stack_trace, (BQ_LOG_API_TYPE(_api_string_def, bq_api_string_def)* out_name_ptr, uint32_t skip_frame_count), (out_name_ptr, skip_frame_count));
BQ_API_DEF(void, __api_get_stack_trace_utf16, (BQ_LOG_API_TYPE(_api_u16string_def, bq_api_u16string_def)* out_name_ptr, uint32_t skip_frame_count), (out_name_ptr, skip_frame_count));

// GO_SUPPORT=ON exports the fused Go write entries. Small argument types are
// packed into args_types, one byte per slot; string values carry byte lengths.
// Values and string pointers are passed separately so cgo can pin each string.
#if defined(BQ_GO)
BQ_API_DEF(uint32_t, __api_go_log_write, (uint64_t log_id, uint8_t level, uint32_t category_index, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_data_bytes_len, const void* args_data), (log_id, level, category_index, format_str_bytes_len, format_str_data, args_data_bytes_len, args_data));
BQ_API_DEF(uint32_t, __api_go_log_write_1, (uint64_t log_id, uint8_t level, uint32_t category_index, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_types, uint64_t value0, const char* string0), (log_id, level, category_index, format_str_bytes_len, format_str_data, args_types, value0, string0));
BQ_API_DEF(uint32_t, __api_go_log_write_2, (uint64_t log_id, uint8_t level, uint32_t category_index, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_types, uint64_t value0, const char* string0, uint64_t value1, const char* string1), (log_id, level, category_index, format_str_bytes_len, format_str_data, args_types, value0, string0, value1, string1));
BQ_API_DEF(uint32_t, __api_go_log_write_4, (uint64_t log_id, uint8_t level, uint32_t category_index, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_count, uint32_t args_types, uint64_t value0, const char* string0, uint64_t value1, const char* string1, uint64_t value2, const char* string2, uint64_t value3, const char* string3), (log_id, level, category_index, format_str_bytes_len, format_str_data, args_count, args_types, value0, string0, value1, string1, value2, string2, value3, string3));
#endif
