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
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System;

namespace bq.def
{
    public enum enum_buffer_result_code
    {
        success = 0,
        err_empty_log_buffer,       // no valid data to read in log buffer;
        err_not_enough_space,       // not enough space in log buffer to alloc
        err_wait_and_retry,         // need wait and try again
        err_data_not_contiguous,    // data is not contiguous, this error code is only used for internal statistics within the log_buffer and will not be exposed externally.
        err_alloc_size_invalid,     // invalid alloc size, too big or 0.
        err_buffer_not_inited,      // buffer not initialized
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
    internal unsafe struct _api_log_buffer_chunk_read_handle
    {
        public byte* data_ptr;
        public enum_buffer_result_code result_code;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public unsafe struct _api_log_buffer_chunk_write_handle
    {
        public byte* data_ptr;
        public enum_buffer_result_code result_code;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal unsafe struct _log_entry_head_def
    {
        public ulong timestamp_epoch;
        public uint ext_info_offset;
        public uint category_idx;
        public ulong format_hash;
        public byte log_format_str_type;
        public byte level;
        public ushort padding;
        public uint log_format_data_len;
    }


    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public unsafe struct _api_string_def
    {
        public sbyte* str;
        public uint len;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public unsafe delegate void type_func_ptr_console_callback(ulong log_id, int category_idx, int log_level,sbyte* content, int length);
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public unsafe delegate void type_func_ptr_console_buffer_fetch_callback(IntPtr pass_through_param, ulong log_id, int category_idx, int log_level, sbyte* content, int length);
    
}