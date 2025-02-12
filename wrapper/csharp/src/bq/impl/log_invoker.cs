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
using System;
using System.Runtime.InteropServices;
using bq.def;
using static bq.tools.log_decoder;

namespace bq.impl
{
    public class log_invoker
    {
#if UNITY_IOS && !UNITY_EDITOR
        private const string LIB_NAME = "__Internal";
#else
        private const string LIB_NAME = bq.lib_def.lib_log_name;
#endif
        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern sbyte* __api_get_log_version();

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern void __api_enable_auto_crash_handler();

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern ulong __api_create_log(byte*  log_name_utf8, byte*  config_content_utf8, uint category_count, byte** category_names_array_utf8);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern bool __api_log_reset_config(byte* log_name_utf8, byte* config_content_utf8);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern _log_api_log_buffer_write_handle __api_log_buffer_alloc(ulong log_id, uint length);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern void __api_log_buffer_commit(ulong log_id, _log_api_log_buffer_write_handle write_handle_ptr);
 
        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern void __api_set_appenders_enable(ulong log_id, byte* appender_name, bool enable);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static extern uint __api_get_logs_count();

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static extern ulong __api_get_log_id_by_index(uint index);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern bool __api_get_log_name_by_id(ulong log_id, _api_string_def* name_ptr);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static extern uint __api_get_log_categories_count(ulong log_id);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern bool __api_get_log_category_name_by_index(ulong log_id, uint category_index, _api_string_def* category_name_ptr);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern uint* __api_get_log_merged_log_level_bitmap_by_log_id(ulong log_id);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern byte* __api_get_log_category_masks_array_by_log_id(ulong log_id);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern uint* __api_get_log_print_stack_level_bitmap_by_log_id(ulong log_id);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern byte* __api_log_device_console(log_level level, byte* content);
        
        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern void __api_force_flush(ulong log_id);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern sbyte* __api_get_file_base_dir(bool is_in_sandbox);
        
        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern bq.tools.log_decoder.appender_decode_result __api_log_decoder_create(byte* log_file_path_utf8, uint* out_handle);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern bq.tools.log_decoder.appender_decode_result __api_log_decoder_decode(uint handle, bq.def._api_string_def* out_decoded_log_text);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern void __api_log_decoder_destroy(uint handle);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern bool __api_log_decode(char* in_file, char* out_file);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern void __api_register_console_callbacks(type_func_ptr_console_callback on_console_callback);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern void __api_unregister_console_callbacks(type_func_ptr_console_callback on_console_callback);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern void __api_set_console_buffer_enable(bool enable);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public static unsafe extern bool __api_fetch_and_remove_console_buffer(type_func_ptr_console_buffer_fetch_callback on_console_callback, IntPtr pass_through_param);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern void __api_take_snapshot_string(ulong log_id, bool use_gmt_time, bq.def._api_string_def* out_snapshot_string);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern void __api_release_snapshot_string(ulong log_id, bq.def._api_string_def* snapshot_string);

        [DllImport(LIB_NAME, CharSet = CharSet.Unicode)]
        public unsafe static extern void __api_uninit();
    }
}