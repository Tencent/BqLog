#pragma once
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
/*!
 * Don't call functions defined in this file!!
 *
 * \brief
 *
 * \author pippocao
 * \date 2022.08.03
 */
#include "bq_common/bq_common_public_include.h"
#include "bq_log/misc/bq_log_def.h"

namespace bq {
    namespace api {
        /////////////////////////////////////////////////////////////DYNAMIC LIB APIS BEGIN////////////////////////////////////////////////////

        /// <summary>
        /// get Version
        /// </summary>
        /// <returns></returns>
        BQ_API const char* __api_get_log_version();

        /// <summary>
        /// BqLog is asynchronous by default, logs may not be flushed when crash is occurred.
        /// You can call __api_force_flush manually in your crash handler.
        /// Or just call __api_enable_auto_crash_handler
        /// </summary>
        /// <returns></returns>
        BQ_API void __api_enable_auto_crash_handler();

        /// <summary>
        /// 0 means create failed
        /// </summary>
        /// <param name="log_name_utf8">log name</param>
        /// <param name="config_content_utf8">config content</param>
        /// <param name="category_count">categories count</param>
        /// <param name="category_names_array_utf8">category names in utf8 encoding, it's char* array</param>
        /// <returns>log id, 0 means create failed</returns>
        BQ_API uint64_t __api_create_log(const char* log_name_utf8, const char* config_content_utf8, uint32_t category_count, const char** category_names_array_utf8);

        /// <summary>
        /// 0 means reset failed
        /// </summary>
        /// <param name="log_name_utf8">log name</param>
        /// <param name="config_content_utf8">json config content</param>
        BQ_API bool __api_log_reset_config(const char* log_name_utf8, const char* config_content_utf8);

        /// <summary>
        /// alloc log write memory chunk
        /// </summary>
        /// <param name="lengsh"></param>
        /// <returns>chunk handle, please check result code first</returns>
        BQ_API _api_ring_buffer_chunk_write_handle __api_log_buffer_alloc(uint64_t log_id, uint32_t length);

        /// <summary>
        /// commit write handle after you finished writing log data
        /// </summary>
        /// <param name="write_handle"></param>
        /// <returns></returns>
        BQ_API void __api_log_buffer_commit(uint64_t log_id, bq::_api_ring_buffer_chunk_write_handle write_handle);

        /// <summary>
        /// toggle of all console appenders,
        /// you can disable it to optimize performance in release version
        /// </summary>
        /// <param name="enable"></param>
        /// <returns></returns>
        BQ_API void __api_set_appenders_enable(uint64_t log_id, const char* appender_name, bool enable);

        /// <summary>
        /// get the current logs count
        /// </summary>
        /// <returns></returns>
        BQ_API uint32_t __api_get_logs_count();

        /// <summary>
        /// get log id by index
        /// </summary>
        /// <param name="index"></param>
        /// <returns>id of log at index, 0 if index exceed valid range</returns>
        BQ_API uint64_t __api_get_log_id_by_index(uint32_t index);

        /// <summary>
        /// get log name by id
        /// </summary>
        /// <param name="log_id"></param>
        /// <param name="name_ptr"></param>
        /// <returns>false if log is not found</returns>
        BQ_API bool __api_get_log_name_by_id(uint64_t log_id, bq::_api_string_def* name_ptr);

        /// <summary>
        /// get the count of category items in log
        /// </summary>
        /// <param name="log_id"></param>
        /// <returns></returns>
        BQ_API uint32_t __api_get_log_categories_count(uint64_t log_id);

        /// <summary>
        /// get name of a category by index
        /// </summary>
        /// <param name="log_id"></param>
        /// <param name="category_index"></param>
        /// <param name="category_name_ptr">the pointer of category item name</param>
        /// <returns>false if index exceed valid range or log is not found</returns>
        BQ_API bool __api_get_log_category_name_by_index(uint64_t log_id, uint32_t category_index, bq::_api_string_def* category_name_ptr);

        /// <summary>
        /// get the address of log level bitmap of log, this address is always valid.
        /// </summary>
        /// <param name="log_id"></param>
        /// <returns>nullptr if log is not found</returns>
        BQ_API const uint32_t* __api_get_log_merged_log_level_bitmap_by_log_id(uint64_t log_id);

        /// <summary>
        /// get the address of log print stack trace level bitmap of log, this address is always valid.
        /// </summary>
        /// <param name="log_id"></param>
        /// <returns>nullptr if log is not found</returns>
        BQ_API const uint32_t* __api_get_log_print_stack_level_bitmap_by_log_id(uint64_t log_id);

        /// <summary>
        /// get the address of category masks array, this array address is always valid.
        /// </summary>
        /// <param name="log_id"></param>
        /// <returns>nullptr if log is not found or the categories list is empty</returns>
        BQ_API const uint8_t* __api_get_log_category_masks_array_by_log_id(uint64_t log_id);

        /// <summary>
        /// output log to device console synchronously.
        /// Android : LogCat, iOS:NSLog, Others:Standard Output
        /// </summary>
        /// <param name="level"></param>
        /// <param name="content"></param>
        /// <returns></returns>
        BQ_API void __api_log_device_console(bq::log_level level, const char* content);

        /// <summary>
        /// Bq Log is a asynchronous system.
        /// you can call this function manually in some case for example in crash handler
        /// to ensure all the logs can be save to log file.
        /// Important: This function is signal-safety
        /// </summary>
        /// <param name="log_id">the id of log object you want to flush, or pass 0 to flush all the log objects</param>
        /// <returns></returns>
        BQ_API void __api_force_flush(uint64_t log_id);

        /// <summary>
        /// get file base dir
        /// android storage path is distinguished by "is_in_sandbox"(internal storage or external storage)
        /// </summary>
        /// <param name="is_in_sandbox"></param>
        /// <returns></returns>
        BQ_API const char* __api_get_file_base_dir(bool is_in_sandbox);

        /// <summary>
        /// create a decoder to decode binary log file
        /// </summary>
        /// <param name="log_file_path"></param>
        /// <param name="out_handle">will be used in __api_log_decoder_decode and __api_log_decoder_destroy</param>
        /// <returns></returns>
        BQ_API bq::appender_decode_result __api_log_decoder_create(const char* log_file_path, uint32_t* out_handle);

        /// <summary>
        /// decode binary log file
        /// </summary>
        /// <param name="handle"></param>
        /// <param name="out_decoded_log_text">if result is success, this is the decoded log text, and it's always valid before next time you call decode_single_item or destroy_decoder</param>
        /// <returns>if result is not success, you should stop invoke this function, and call __api_log_decoder_destroy to release memory</returns>
        BQ_API bq::appender_decode_result __api_log_decoder_decode(uint32_t handle, bq::_api_string_def* out_decoded_log_text);

        /// <summary>
        /// destroy decoder to release memory
        /// </summary>
        /// <param name="handle"></param>
        /// <returns></returns>
        BQ_API void __api_log_decoder_destroy(uint32_t handle);

        /// <summary>
        /// parsing interface for log raw
        /// </summary>
        /// <param name="handle"></param>
        /// <returns></returns>
        BQ_API bool __api_log_decode(const char* in_file_path, const char* out_file_path);

        /// <summary>
        /// Register a callback which will be invoked when each log entry was written to Console
        /// </summary>
        /// <param name="on_console_callback"></param>
        /// <returns></returns>
        BQ_API void __api_register_console_callbacks(bq::type_func_ptr_console_callback on_console_callback);

        /// <summary>
        /// Unregister a console callback
        /// </summary>
        /// <param name="on_console_callback"></param>
        /// <returns></returns>
		BQ_API void __api_unregister_console_callbacks(bq::type_func_ptr_console_callback on_console_callback);

		/// <summary>
		/// set console appender buffer enable or not
		/// </summary>
		/// <param name="enable"></param>
		/// <returns></returns>
		BQ_API void __api_set_console_buffer_enable(bool enable);

		/// <summary>
		/// Fetch and remove a log entry from the console appender buffer in a thread-safe manner. 
        /// If the console appender buffer is not empty, the on_console_callback function will be invoked for this log entry. 
        /// Please ensure not to output synchronized BQ logs within the callback function.
        /// </summary>
		/// <param name="on_console_callback"></param>
        /// <param name="pass_through_param">path through parameter that will pass to on_console_callback</param>
		/// <returns>True if the console appender buffer is not empty, otherwise False is returned.</returns>
		BQ_API bool __api_fetch_and_remove_console_buffer(bq::type_func_ptr_console_buffer_fetch_callback on_console_callback, const void* pass_through_param);

        /// <summary>
        /// The snapshot feature is disabled by default, as it consumes some performance overhead.
        /// Note: Once enabled, calling the function again will only modify the buffer size and cannot disable the feature.
        /// </summary>
        /// <param name="log_id"></param>
        /// <param name="snapshot_buffer_size"></param>
        /// <returns></returns>
        BQ_API void __api_enable_snapshot(uint64_t log_id, uint32_t snapshot_buffer_size);

        /// <summary>
        /// Note: if snapshot is not enabled, this API will return empty snapshot string.
        /// </summary>
        /// <param name="log_id"></param>
        /// <param name="use_gmt_time"></param>
        /// <param name="out_snapshot_string"></param>
        /// <returns></returns>
        BQ_API void __api_take_snapshot_string(uint64_t log_id, bool use_gmt_time, bq::_api_string_def* out_snapshot_string);

        /// <summary>
        /// The functions __api_take_snapshot_string and __api_release_snapshot_string must be called in pairs.
        /// The snapshot string is guaranteed to be valid only before __api_release_snapshot_string is called.
        /// If they are not called in pairs, a crash or deadlock may occur.
        /// and the snapshot string is only valid til
        /// </summary>
        /// <param name="log_id"></param>
        /// <param name="snapshot_string"></param>
        /// <returns></returns>
        BQ_API void __api_release_snapshot_string(uint64_t log_id, bq::_api_string_def* snapshot_string);

        /// <summary>
        /// Get stack trace of current thread.
        /// The result is safe only in current thread until next call to this api.
        /// </summary>
        /// <param name="out_name_ptr"></param>
        /// <param name="skip_frame_count">the count of stack frames you want to skip</param>
        /// <returns></returns>
        BQ_API void __api_get_stack_trace(bq::_api_string_def* out_name_ptr, uint32_t skip_frame_count);
        BQ_API void __api_get_stack_trace_utf16(bq::_api_u16string_def* out_name_ptr, uint32_t skip_frame_count);
        /////////////////////////////////////////////////////////////DYNAMIC LIB APIS END///////////////////////////////////////////////////////
    }
}
