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
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using bq.def;
using bq.impl;

namespace bq
{
    public class @log
    {
        private static log_category_base default_category_ = new log_category_base();


        private ulong log_id_ = 0;
        private string name_ = "";
        private unsafe uint* merged_log_level_bitmap_ = null;
        private unsafe byte* categories_mask_array_ = null;
        private unsafe uint* print_stack_level_bitmap_ = null;
        protected List<string> categories_name_array_ = new List<string>();

        protected static log get_log_by_id(ulong log_id)
        {
            log log = new log();
            unsafe
            {
                _api_string_def name_struct = new _api_string_def();
                if (log_invoker.__api_get_log_name_by_id(log_id, &name_struct))
                {
                    log.name_ = new string(name_struct.str, 0, (int)name_struct.len, System.Text.Encoding.UTF8);
                }
                else
                {
                    return log;
                }

                log.merged_log_level_bitmap_ = log_invoker.__api_get_log_merged_log_level_bitmap_by_log_id(log_id);
                log.categories_mask_array_ = log_invoker.__api_get_log_category_masks_array_by_log_id(log_id);
                log.print_stack_level_bitmap_ = log_invoker.__api_get_log_print_stack_level_bitmap_by_log_id(log_id);
            }

            uint category_count = log_invoker.__api_get_log_categories_count(log_id);
            log.categories_name_array_.Capacity = (int)category_count;
            unsafe
            {
                for (uint i = 0; i < category_count; ++i)
                {
                    _api_string_def category_item_name = new _api_string_def();
                    if (log_invoker.__api_get_log_category_name_by_index(log_id, i, &category_item_name))
                    {
                        string category_name = new string(category_item_name.str, 0, (int)category_item_name.len, System.Text.Encoding.UTF8);
                        log.categories_name_array_.Add(category_name);
                    }
                }
            }
            log.log_id_ = log_id;
            return log;
        }
        private bool is_enable_for(log_category_base category, log_level level)
        {
            unsafe
            {
                if ((*merged_log_level_bitmap_ & (1 << (int)level)) == 0 || categories_mask_array_[log_category_base.get_index(category)] == 0)
                {
                    return false;
                }
            }
            return true;
        }

        private unsafe static int get_str_len(sbyte* str)
        {
            int len = 0;
            if (null == str)
            {
                return len;
            }
            while (str[len] != 0)
            {
                ++len;
            }
            return len;
        }

        /// <summary>
        /// Get bqLog lib version
        /// </summary>
        /// <returns></returns>
        public static string get_version()
        {
            unsafe
            {
                sbyte* version_str = log_invoker.__api_get_log_version();
                if (null == version_str)
                {
                    return "";
                }
                int len = get_str_len(version_str);
                return new string(version_str, 0, len, System.Text.Encoding.UTF8);
            }
        }

        /// <summary>
        /// If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk. 
        /// If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer in the event of a crash. However, 
        /// this functionality does not guarantee success.
        /// </summary>
        public static void enable_auto_crash_handle()
        {
            log_invoker.__api_enable_auto_crash_handler();
        }

        /// <summary>
        /// If bqLog is stored in a relative path, the base dir is determined by the value of base_dir_type.
        /// This will return the absolute paths corresponding to both scenarios.
        /// </summary>
        /// <param name="base_dir_type"></param>
        /// <returns></returns>
        public static string get_file_base_dir(int base_dir_type)
        {
            unsafe
            {
                sbyte* path_str = log_invoker.__api_get_file_base_dir(base_dir_type);
                int len = get_str_len(path_str);
                return new string(path_str, 0, len, System.Text.Encoding.UTF8);
            }
        }

        /// <summary>
        /// Reset the base dir
        /// </summary>
        /// <param name="base_dir_type"></param>
        /// <param name="dir"></param>
        public void reset_base_dir(int base_dir_type, string dir)
        {
            unsafe
            {
                byte* dir_utf8 = utf8_encoder.alloc_utf8_fixed_str(dir);
                bq.impl.log_invoker.__api_reset_base_dir(base_dir_type, dir_utf8);
                utf8_encoder.release_utf8_fixed_str(dir_utf8);
            }
        }

        /// <summary>
        /// Create a log object
        /// </summary>
        /// <param name="name">
        ///     If the log name is an empty string, bqLog will automatically assign you a unique log name. 
        ///     If the log name already exists, it will return the previously existing log object and overwrite the previous configuration with the new config.
        /// </param>
        /// <param name="config">
        ///     Log config string
        /// </param>
        /// <returns>
        ///     A log object, if create failed, the is_valid() method of it will return false
        /// </returns>
        public static log create_log(string name, string config)
        {
            if (string.IsNullOrEmpty(config))
            {
                return new log();
            }
            ulong log_id = 0;
            unsafe
            {
                byte* utf8_name_bytes = utf8_encoder.alloc_utf8_fixed_str(name);
                byte* utf8_config_bytes = utf8_encoder.alloc_utf8_fixed_str(config);
                log_id = log_invoker.__api_create_log(utf8_name_bytes, utf8_config_bytes, (uint)0, null);
                utf8_encoder.release_utf8_fixed_str(utf8_name_bytes);
                utf8_encoder.release_utf8_fixed_str(utf8_config_bytes);
            }
            log result = get_log_by_id(log_id);
            return result;
        }

        /// <summary>
        /// Get a log object by it's name
        /// </summary>
        /// <param name="log_name">
        ///     Name of the log you want to find
        /// </param>
        /// <returns>
        ///     A log object, if the log object with specific name was not found, the is_valid() method of it will return false
        /// </returns>
        public static log get_log_by_name(string log_name)
        {
            if(string.IsNullOrEmpty(log_name))
            {
                return new log();
            }
            uint log_count = log_invoker.__api_get_logs_count();
            for (uint i = 0; i < log_count; ++i)
            {
                var id = log_invoker.__api_get_log_id_by_index(i);
                unsafe
                {
                    _api_string_def log_name_tmp = new _api_string_def();
                    if (log_invoker.__api_get_log_name_by_id(id, &log_name_tmp))
                    {
                        string log_name_str_tmp = new string(log_name_tmp.str, 0, (int)log_name_tmp.len, System.Text.Encoding.UTF8);
                        if (log_name.Equals(log_name_str_tmp))
                        {
                            return get_log_by_id(id);
                        }
                    }
                }
            }
            return new log();
        }

        /// <summary>
        /// Synchronously flush the buffer of all log objects
        /// to ensure that all data in the buffer is processed after the call.
        /// </summary>
        public static void force_flush_all_logs()
        {
            log_invoker.__api_force_flush(0);
        }

        public delegate void type_console_callback(ulong log_id, int category_idx, bq.def.log_level log_level, string content);
        private static type_console_callback console_callback_ = null;
        /// <summary>
        /// Register a callback that will be invoked whenever a console log message is output. 
        /// This can be used for an external system to monitor console log output.
        /// </summary>
        /// <param name="callback"></param>
        public static void register_console_callback(type_console_callback callback)
        {
            console_callback_ = callback;
            unsafe
            {
                if (console_callback_ != null)
                {
                    log_invoker.__api_register_console_callbacks(new type_func_ptr_console_callback(_native_console_callback_wrapper));
                }
                else
                {
                    log_invoker.__api_unregister_console_callbacks(new type_func_ptr_console_callback(_native_console_callback_wrapper));
                }
            }
        }

        /// <summary>
        /// Unregister console callback.
        /// </summary>
        /// <param name="callback"></param>
        public static void unregister_console_callback(type_console_callback callback)
        {
            if(console_callback_ == callback)
            {
                console_callback_ = null;
                unsafe
                {
                    log_invoker.__api_unregister_console_callbacks(new type_func_ptr_console_callback(_native_console_callback_wrapper));
                }
            }
        }

#if ENABLE_IL2CPP
        [AOT.MonoPInvokeCallback(typeof(type_console_callback))]
#endif
        private unsafe static void _native_console_callback_wrapper(ulong log_id, int category_idx, int log_level, sbyte* content, int length)
        {
            if (console_callback_ != null)
            {
                string value = new string(content, 0, length, System.Text.Encoding.UTF8);
                console_callback_(log_id, category_idx, (bq.def.log_level)log_level, value);
            }
        }

        /// <summary>
        /// Enable or disable the console appender buffer. 
        /// Since our wrapper may run in both C# and Java virtual machines, and we do not want to directly invoke callbacks from a native thread, 
        /// we can enable this option. This way, all console outputs will be saved in the buffer until we fetch them.
		/// </summary>
		/// <param name="enable"></param>
        public static void set_console_buffer_enable(bool enable)
        {
            log_invoker.__api_set_console_buffer_enable(enable);
        }

        /// <summary>
        /// Fetch and remove a log entry from the console appender buffer in a thread-safe manner. 
        /// If the console appender buffer is not empty, the on_console_callback function will be invoked for this log entry. 
        /// Please ensure not to output synchronized BQ logs within the callback function.
        /// IMPORTANT: If you are using this code in an IL2CPP environment, please make sure that the on_console_callback is marked as static unsafe and is decorated with the [AOT.MonoPInvokeCallback(typeof(type_console_callback))] attribute.
        /// </summary>
        /// <param name="on_console_callback">A callback function to be invoked for the fetched log entry if the console appender buffer is not empty</param>
        /// <returns>True if the console appender buffer is not empty and a log entry is fetched; otherwise False is returned.</returns>
        public static unsafe bool fetch_and_remove_console_buffer(type_console_callback on_console_callback)
        {
            IntPtr delegate_ptr = Marshal.GetFunctionPointerForDelegate(on_console_callback);
            return log_invoker.__api_fetch_and_remove_console_buffer(new type_func_ptr_console_buffer_fetch_callback(_native_console_buffer_fetch_callback_wrapper), delegate_ptr);
        }
#if ENABLE_IL2CPP
        [AOT.MonoPInvokeCallback(typeof(type_console_callback))]
#endif
        private unsafe static void _native_console_buffer_fetch_callback_wrapper(IntPtr pass_through_param, ulong log_id, int category_idx, int log_level, sbyte* content, int length)
        {
            string value = new string(content, 0, length, System.Text.Encoding.UTF8);
            type_console_callback recover_callback = (type_console_callback)Marshal.GetDelegateForFunctionPointer(pass_through_param, typeof(type_console_callback));
            recover_callback(log_id, category_idx, (bq.def.log_level)log_level, value);
        }

        /// <summary>
        /// Output to console with log_level.
        /// Important: This is not log entry, and can not be caught by console callback which was registered by register_console_callback or fetch_and_remove_console_buffer
        /// </summary>
        /// <param name="level"></param>
        /// <param name="content"></param>
        public unsafe static void console(log_level level, string content)
        {
            byte* utf8_bytes = utf8_encoder.alloc_utf8_fixed_str(content);
            log_invoker.__api_log_device_console(level, utf8_bytes);
            utf8_encoder.release_utf8_fixed_str(utf8_bytes);
        }

        /// <summary>
        /// Checks whether stack trace information is enabled for the specified log level.
        /// </summary>
        /// <param name="level">the log level to check for stack trace information</param>
        /// <returns>true if stack trace information is enabled for the specified log level, false otherwise</returns>
        internal static unsafe bool is_stack_trace_enable_for(log log, log_level level)
        {
            return (*log.print_stack_level_bitmap_ & (1 << (int)level)) != 0;
        }

        protected log()
        {
        }

        /// <summary>
        /// copy constructor
        /// </summary>
        /// <param name="rhs"></param>
        protected log(log rhs)
        {
            unsafe
            {
                log_id_ = rhs.log_id_;
                name_ = rhs.name_;
                merged_log_level_bitmap_ = rhs.merged_log_level_bitmap_;
                categories_mask_array_ = rhs.categories_mask_array_;
                print_stack_level_bitmap_ = rhs.print_stack_level_bitmap_;
            }

            uint category_count = log_invoker.__api_get_log_categories_count(log_id_);
            categories_name_array_ = new List<string>();
            for (int i = 0; i < (int)category_count; ++i)
            {
                categories_name_array_.Add(rhs.categories_name_array_[i]);
            }
        }

        /// <summary>
        /// Modify the log configuration, but some fields, such as buffer_size, cannot be modified.
        /// </summary>
        /// <param name="name"></param>
        /// <param name="config"></param>
        /// <returns></returns>
        public bool reset_config(string name, string config)
        {
            if (config == null || config.Length == 0)
            {
                return false;
            }
            unsafe
            {

                byte* utf8_name_bytes = utf8_encoder.alloc_utf8_fixed_str(name);
                byte* utf8_config_bytes = utf8_encoder.alloc_utf8_fixed_str(config);
                bool result = log_invoker.__api_log_reset_config(utf8_name_bytes, utf8_config_bytes);
                utf8_encoder.release_utf8_fixed_str(utf8_name_bytes);
                utf8_encoder.release_utf8_fixed_str(utf8_config_bytes);
                return result;
            }
        }

        /// <summary>
        /// Temporarily disable or enable a specific Appender.
        /// </summary>
        /// <param name="appender_name"></param>
        /// <param name="enable"></param>
        public void set_appenders_enable(string appender_name, bool enable)
        {
            unsafe
            {
                byte* in_str = utf8_encoder.alloc_utf8_fixed_str(appender_name);
                log_invoker.__api_set_appenders_enable(log_id_, in_str, enable);
                utf8_encoder.release_utf8_fixed_str(in_str);
            }
        }

        /// <summary>
        /// Synchronously flush the buffer of this log object
        /// to ensure that all data in the buffer is processed after the call.
        /// </summary>
        public void force_flush()
        {
            log_invoker.__api_force_flush(log_id_);
        }

        /// <summary>
        /// Get id of this log object
        /// </summary>
        /// <returns></returns>
        public ulong get_id()
        {
            return log_id_;
        }

        /// <summary>
        /// Whether a log object is valid
        /// </summary>
        /// <returns></returns>
        public bool is_valid()
        {
            return get_id() != 0;
        }

        /// <summary>
        /// Get the name of a log
        /// </summary>
        /// <returns></returns>
        public string get_name()
        {
            return name_;
        }

        /// <summary>
        /// Works only when snapshot is configured.
        /// It will decode the snapshot buffer to text.
        /// </summary>
        /// <param name="time_zone_config">Use this to specify the time display of log text. such as : "localtime", "gmt", "Z", "UTC", "UTC+8", "UTC-11", "utc+11:30"</param>
        /// <returns>The decoded snapshot buffer</returns>
        public string take_snapshot(string time_zone_config)
        {
            unsafe
            {
                _api_string_def snapshot_def = new _api_string_def();
                byte* utf8_time_zone_config_bytes = utf8_encoder.alloc_utf8_fixed_str(time_zone_config);
                bq.impl.log_invoker.__api_take_snapshot_string(log_id_, utf8_time_zone_config_bytes, &snapshot_def);
                utf8_encoder.release_utf8_fixed_str(utf8_time_zone_config_bytes);
                string result = new string(snapshot_def.str, 0, (int)snapshot_def.len, System.Text.Encoding.UTF8);
                bq.impl.log_invoker.__api_release_snapshot_string(log_id_, &snapshot_def);
                return result;
            }
        }

        public override bool Equals(object obj)
        {
            if (ReferenceEquals(obj, null))
            {
                return false;
            }

            var other = obj as log;
            if (ReferenceEquals(other, null))
            {
                return false;
            }
            return log_id_ == other.get_id();
        }

        public override int GetHashCode()
        {
            return log_id_.GetHashCode();
        }

        ///Core log functions, there are 6 log levels:
        ///verbose, debug, info, warning, error, fatal
        #region log methods for param count 0
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content)
        {
            return do_log(default_category_, log_level.verbose, log_format_content);
        }
        public unsafe bool debug(string log_format_content)
        {
            return do_log(default_category_, log_level.debug, log_format_content);
        }
        public unsafe bool info(string log_format_content)
        {
            return do_log(default_category_, log_level.info, log_format_content);
        }
        public unsafe bool warning(string log_format_content)
        {
            return do_log(default_category_, log_level.warning, log_format_content);
        }
        public unsafe bool error(string log_format_content)
        {
            return do_log(default_category_, log_level.error, log_format_content);
        }
        public unsafe bool fatal(string log_format_content)
        {
            return do_log(default_category_, log_level.fatal, log_format_content);
        }
        #endregion

        #region log methods for param count 1
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1);
        }
        #endregion

        #region log methods for param count 2
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2);
        }
        #endregion

        #region log methods for param count 3
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3);
        }
        #endregion

        #region log methods for param count 4
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4);
        }
        #endregion

        #region log methods for param count 5
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4, ref param_wrapper p5)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size + p5.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            params_addr += p4.aligned_size;
            p5.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5);
        }
        #endregion

        #region log methods for param count 6
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4, ref param_wrapper p5, ref param_wrapper p6)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size + p5.aligned_size + p6.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            params_addr += p4.aligned_size;
            p5.write(params_addr);
            params_addr += p5.aligned_size;
            p6.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6);
        }
        #endregion

        #region log methods for param count 7
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4, ref param_wrapper p5, ref param_wrapper p6, ref param_wrapper p7)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size + p5.aligned_size + p6.aligned_size + p7.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            params_addr += p4.aligned_size;
            p5.write(params_addr);
            params_addr += p5.aligned_size;
            p6.write(params_addr);
            params_addr += p6.aligned_size;
            p7.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7);
        }
        #endregion

        #region log methods for param count 8
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4, ref param_wrapper p5, ref param_wrapper p6, ref param_wrapper p7, ref param_wrapper p8)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size + p5.aligned_size + p6.aligned_size + p7.aligned_size + p8.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            params_addr += p4.aligned_size;
            p5.write(params_addr);
            params_addr += p5.aligned_size;
            p6.write(params_addr);
            params_addr += p6.aligned_size;
            p7.write(params_addr);
            params_addr += p7.aligned_size;
            p8.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8);
        }
        #endregion

        #region log methods for param count 9
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4, ref param_wrapper p5, ref param_wrapper p6, ref param_wrapper p7, ref param_wrapper p8, ref param_wrapper p9)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size + p5.aligned_size + p6.aligned_size + p7.aligned_size + p8.aligned_size + p9.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            params_addr += p4.aligned_size;
            p5.write(params_addr);
            params_addr += p5.aligned_size;
            p6.write(params_addr);
            params_addr += p6.aligned_size;
            p7.write(params_addr);
            params_addr += p7.aligned_size;
            p8.write(params_addr);
            params_addr += p8.aligned_size;
            p9.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9);
        }
        #endregion

        #region log methods for param count 10
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4, ref param_wrapper p5, ref param_wrapper p6, ref param_wrapper p7, ref param_wrapper p8, ref param_wrapper p9, ref param_wrapper p10)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size + p5.aligned_size + p6.aligned_size + p7.aligned_size + p8.aligned_size + p9.aligned_size + p10.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            params_addr += p4.aligned_size;
            p5.write(params_addr);
            params_addr += p5.aligned_size;
            p6.write(params_addr);
            params_addr += p6.aligned_size;
            p7.write(params_addr);
            params_addr += p7.aligned_size;
            p8.write(params_addr);
            params_addr += p8.aligned_size;
            p9.write(params_addr);
            params_addr += p9.aligned_size;
            p10.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10);
        }
        #endregion

        #region log methods for param count 11
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4, ref param_wrapper p5, ref param_wrapper p6, ref param_wrapper p7, ref param_wrapper p8, ref param_wrapper p9, ref param_wrapper p10, ref param_wrapper p11)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size + p5.aligned_size + p6.aligned_size + p7.aligned_size + p8.aligned_size + p9.aligned_size + p10.aligned_size + p11.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            params_addr += p4.aligned_size;
            p5.write(params_addr);
            params_addr += p5.aligned_size;
            p6.write(params_addr);
            params_addr += p6.aligned_size;
            p7.write(params_addr);
            params_addr += p7.aligned_size;
            p8.write(params_addr);
            params_addr += p8.aligned_size;
            p9.write(params_addr);
            params_addr += p9.aligned_size;
            p10.write(params_addr);
            params_addr += p10.aligned_size;
            p11.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11);
        }
        #endregion

        #region log methods for param count 12
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, ref param_wrapper p1, ref param_wrapper p2, ref param_wrapper p3, ref param_wrapper p4, ref param_wrapper p5, ref param_wrapper p6, ref param_wrapper p7, ref param_wrapper p8, ref param_wrapper p9, ref param_wrapper p10, ref param_wrapper p11, ref param_wrapper p12)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0 + p1.aligned_size + p2.aligned_size + p3.aligned_size + p4.aligned_size + p5.aligned_size + p6.aligned_size + p7.aligned_size + p8.aligned_size + p9.aligned_size + p10.aligned_size + p11.aligned_size + p12.aligned_size;
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            p1.write(params_addr);
            params_addr += p1.aligned_size;
            p2.write(params_addr);
            params_addr += p2.aligned_size;
            p3.write(params_addr);
            params_addr += p3.aligned_size;
            p4.write(params_addr);
            params_addr += p4.aligned_size;
            p5.write(params_addr);
            params_addr += p5.aligned_size;
            p6.write(params_addr);
            params_addr += p6.aligned_size;
            p7.write(params_addr);
            params_addr += p7.aligned_size;
            p8.write(params_addr);
            params_addr += p8.aligned_size;
            p9.write(params_addr);
            params_addr += p9.aligned_size;
            p10.write(params_addr);
            params_addr += p10.aligned_size;
            p11.write(params_addr);
            params_addr += p11.aligned_size;
            p12.write(params_addr);
            context.end_copy();
            return true;
        }

        public unsafe bool verbose(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11, param_wrapper p12)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11, ref p12);
        }
        public unsafe bool debug(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11, param_wrapper p12)
        {
            return do_log(default_category_, log_level.debug, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11, ref p12);
        }
        public unsafe bool info(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11, param_wrapper p12)
        {
            return do_log(default_category_, log_level.info, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11, ref p12);
        }
        public unsafe bool warning(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11, param_wrapper p12)
        {
            return do_log(default_category_, log_level.warning, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11, ref p12);
        }
        public unsafe bool error(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11, param_wrapper p12)
        {
            return do_log(default_category_, log_level.error, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11, ref p12);
        }
        public unsafe bool fatal(string log_format_content, param_wrapper p1, param_wrapper p2, param_wrapper p3, param_wrapper p4, param_wrapper p5, param_wrapper p6, param_wrapper p7, param_wrapper p8, param_wrapper p9, param_wrapper p10, param_wrapper p11, param_wrapper p12)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, ref p1, ref p2, ref p3, ref p4, ref p5, ref p6, ref p7, ref p8, ref p9, ref p10, ref p11, ref p12);
        }
        #endregion

        #region log methods for more params. but call this method will cause GC(Heap) alloc
        protected unsafe bool do_log(log_category_base category, log_level level, string log_format_content, params param_wrapper[] ps)
        {
            if (!is_enable_for(category, level))
            {
                return false;
            }
            uint param_size = 0;
            for (int i = 0; i < ps.Length; ++i)
            {
                param_size += ps[i].aligned_size;
            }
            log_context context = new log_context(this, category, level, log_format_content, param_size);
            if (!context.begin_copy())
            {
                return false;
            }
            var params_addr = context.get_params_data_addr();
            for (int i = 0; i < ps.Length; ++i)
            {
                ps[i].write(params_addr);
                params_addr += ps[i].aligned_size;
            }
            context.end_copy();
            return true;
        }
        
        public unsafe bool verbose(string log_format_content, params param_wrapper[] args)
        {
            return do_log(default_category_, log_level.verbose, log_format_content, args);
        }
        public unsafe bool debug(string log_format_content, params param_wrapper[] args)
        {
            return do_log(default_category_, log_level.debug, log_format_content, args);
        }
        public unsafe bool info(string log_format_content, params param_wrapper[] args)
        {
            return do_log(default_category_, log_level.info, log_format_content, args);
        }
        public unsafe bool warning(string log_format_content, params param_wrapper[] args)
        {
            return do_log(default_category_, log_level.warning, log_format_content, args);
        }
        public unsafe bool error(string log_format_content, params param_wrapper[] args)
        {
            return do_log(default_category_, log_level.error, log_format_content, args);
        }
        public unsafe bool fatal(string log_format_content, params param_wrapper[] args)
        {
            return do_log(default_category_, log_level.fatal, log_format_content, args);
        }
        #endregion
       
    }
}