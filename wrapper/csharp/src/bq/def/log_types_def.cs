/*
 * Copyright (C) 2024 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System;

namespace bq.def
{
    public enum enum_buffer_result_code
    {
        success = 0,
		err_empty_ring_buffer,                           //no valid data to read in ring buffer;
		err_not_enough_space,                            //not enough space in ring buffer to alloc
		err_alloc_failed_by_race_condition,              //alloc failed caused by multi-thread race condition, you can try again later.
		err_alloc_size_invalid,                          //invalid alloc size, too big or 0.
        err_buffer_not_inited,                           //buffer not initialized
        result_code_count
    };

    public enum log_level
    {
        verbose,
        debug,
        info,
        warning,
        error,
        fatal,

        log_level_max = 32,        //上限
    }

    internal enum log_arg_type_enum
    {
        unsupported_type,
        null_type,
        pointer_type,
        bool_type,
        char_type,
        char16_type,
        char32_type,
        int8_type,
        uint8_type,
        int16_type,
        uint16_type,
        int32_type,
        uint32_type,
        int64_type,
        uint64_type,
        float_type,
        double_type,
        string_utf8_type,
        string_utf16_type
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public unsafe struct _log_api_ring_buffer_write_handle
    {
        public byte* data_ptr;
        public enum_buffer_result_code result_code;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal unsafe struct _log_api_ring_buffer_read_handle
    {
        public byte* data_ptr;
        public enum_buffer_result_code result_code;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal unsafe struct _log_api_console_log_data_head
    {
        public ulong log_id;
        public uint category_idx;
        //public uint category_hash;
        public int level;
        public int log_len;
        public byte log_str;
    };

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal unsafe struct _log_head_def
    {
        public ulong timestamp_epoch;
        public uint category_idx;
        public byte level;
        public byte log_format_str_type; //log_arg_type_enum.string_utf8_type or log_arg_type_enum.string_utf16_type
        public uint log_args_offset;
        public uint ext_info_offset;
        public ushort dummy;
    }



    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public unsafe struct _api_string_def
    {
        public sbyte* str;
        public uint len;
    }

    public class log_console_item
    {
        public ulong log_id;
        public uint category_idx;
        //public uint category_hash;
        public log_level level;
        public string log_str;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public unsafe delegate void type_func_ptr_console_callback(ulong log_id, int category_idx, int log_level,sbyte* content, int length);
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public unsafe delegate void type_func_ptr_console_buffer_fetch_callback(IntPtr pass_through_param, ulong log_id, int category_idx, int log_level, sbyte* content, int length);
    
}